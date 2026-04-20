/*
 * gqa_attention.cu — fused causal GQA attention kernel.
 *
 * Grid: (H, S)   one block per (query_head, query_position)
 * Block: 128 threads, parallelized over Dh for the output write
 *
 * Thread 0 of each block computes scores[0..p] sequentially (Q·K^T
 * scaled, then softmax with max-subtract), writes to __shared__.
 * All threads then cooperatively compute the Dh-dimensional output
 * slice for (h, p), each thread doing a single sequential fmaf over
 * j=0..S-1 multiplied by probs[j].
 *
 * Determinism: score computation, max, sum all run on a single thread
 * in fixed order.  Output uses fmaf sequential over j.  No warp
 * reductions, no atomics.
 *
 * Shared-memory cap: 2*S*4 bytes per block (scores + probs).  For
 * S <= 6000 this fits in 48KB shared on sm_75+.  Longer contexts
 * would require a chunked / flash-attention variant; out of scope.
 */

#include "gqa_attention.h"
#include "expf_hoon.cuh"

#include <cuda_runtime.h>
#include <math.h>

__global__ void
gqa_attention_kernel(const float* __restrict__ q,
                     const float* __restrict__ k,
                     const float* __restrict__ v,
                     float*       __restrict__ y,
                     size_t S,
                     size_t H,
                     size_t KH,
                     size_t Dh,
                     float  inv_sqrt_dh,
                     size_t group)
{
  size_t h = blockIdx.x;
  size_t p = blockIdx.y;
  if ( h >= H || p >= S ) return;
  size_t kvh = h / group;

  extern __shared__ float sm[];
  float* probs = sm;               /* [S] */
  float* scores = sm + S;          /* [S] scratch */

  /* Phase 1: thread 0 computes scaled scores for j=0..p, then softmax.
   * Explicit mul+add (not fmaf), and expf_hoon (not CUDA expf) so that
   * every result matches Hoon's rs:math byte-for-byte. */
  if ( threadIdx.x == 0 ) {
    const float* q_ptr = q + p * H * Dh + h * Dh;
    float mx = -INFINITY;
    for ( size_t j = 0; j <= p; j++ ) {
      const float* k_ptr = k + j * KH * Dh + kvh * Dh;
      float s = 0.0f;
      for ( size_t e = 0; e < Dh; e++ ) {
        s = s + q_ptr[e] * k_ptr[e];
      }
      s = s * inv_sqrt_dh;
      scores[j] = s;
      if ( s > mx ) mx = s;
    }
    float sum = 0.0f;
    for ( size_t j = 0; j <= p; j++ ) {
      float e = expf_hoon(scores[j] - mx);
      scores[j] = e;
      sum = sum + e;
    }
    for ( size_t j = 0; j <= p; j++ ) {
      probs[j] = scores[j] / sum;
    }
    /* Zero probs past p — strictly masked, included for correctness
     * of the parallel output loop below (which iterates 0..S-1). */
    for ( size_t j = p + 1; j < S; j++ ) {
      probs[j] = 0.0f;
    }
  }
  __syncthreads();

  /* Phase 2: parallel output.  One thread per Dh index.  Each thread
   * does its own sequential fmaf reduction over j=0..S-1 — still
   * deterministic since each thread operates on its own d only. */
  for ( size_t d = threadIdx.x; d < Dh; d += blockDim.x ) {
    float out = 0.0f;
    for ( size_t j = 0; j < S; j++ ) {
      float vj = v[j * KH * Dh + kvh * Dh + d];
      out = out + probs[j] * vj;
    }
    y[p * H * Dh + h * Dh + d] = out;
  }
}

static int
_init_once(void)
{
  static int done = 0;
  if ( done ) return 1;
  int dev = 0;
  if ( cudaGetDeviceCount(&dev) != cudaSuccess || dev == 0 ) return 0;
  if ( cudaSetDevice(0) != cudaSuccess ) return 0;
  done = 1;
  return 1;
}

extern "C" gqa_attention_status
gqa_attention_fp32(const float* q,
                   const float* k,
                   const float* v,
                   float*       y,
                   size_t       S,
                   size_t       H,
                   size_t       KH,
                   size_t       Dh)
{
  if ( !q || !k || !v || !y || S == 0 || H == 0 || KH == 0 || Dh == 0 ) {
    return GQA_INVALID_ARG;
  }
  if ( H % KH != 0 ) return GQA_INVALID_ARG;
  if ( !_init_once() ) return GQA_NO_CUDA;

  size_t group = H / KH;
  size_t q_bytes = S * H * Dh * sizeof(float);
  size_t kv_bytes = S * KH * Dh * sizeof(float);
  size_t y_bytes = q_bytes;

  float *d_q = NULL, *d_k = NULL, *d_v = NULL, *d_y = NULL;
  if ( cudaMalloc((void**)&d_q, q_bytes)  != cudaSuccess ||
       cudaMalloc((void**)&d_k, kv_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_v, kv_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_y, y_bytes)  != cudaSuccess ) {
    if ( d_q ) cudaFree(d_q);
    if ( d_k ) cudaFree(d_k);
    if ( d_v ) cudaFree(d_v);
    if ( d_y ) cudaFree(d_y);
    return GQA_ALLOC_FAIL;
  }
  cudaMemcpy(d_q, q, q_bytes,  cudaMemcpyHostToDevice);
  cudaMemcpy(d_k, k, kv_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_v, v, kv_bytes, cudaMemcpyHostToDevice);

  float inv_sqrt_dh = 1.0f / sqrtf((float)Dh);
  dim3 grid((unsigned)H, (unsigned)S);
  size_t shared_bytes = 2 * S * sizeof(float);
  gqa_attention_kernel<<<grid, 128, shared_bytes>>>(
    d_q, d_k, d_v, d_y, S, H, KH, Dh, inv_sqrt_dh, group);

  gqa_attention_status st = GQA_OK;
  if ( cudaGetLastError() != cudaSuccess ||
       cudaDeviceSynchronize() != cudaSuccess ||
       cudaMemcpy(y, d_y, y_bytes, cudaMemcpyDeviceToHost) != cudaSuccess ) {
    st = GQA_LAUNCH_FAIL;
  }
  cudaFree(d_q); cudaFree(d_k); cudaFree(d_v); cudaFree(d_y);
  return st;
}
