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
#include <cuda_fp16.h>
#include <math.h>

/* Narrow an fp32 row to fp16.  Used by prefill/decode to write K, V
 * into the half-precision KV cache.  Per-element, no reductions —
 * deterministic across hosts by the IEEE round-to-nearest-even rule
 * that `__float2half` implements. */
__global__ void
narrow_fp32_to_fp16_kernel(const float* __restrict__ src,
                           __half*      __restrict__ dst,
                           size_t n)
{
  size_t i = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
  if ( i < n ) dst[i] = __float2half(src[i]);
}

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

/* Decode-mode attention: exactly one query (position N-1) against N
 * cached K/V positions.  Same byte-exact math as the prefill kernel —
 * just no causal-mask padding since there's only one query.
 *
 *   Grid: (H)  — one block per query head
 *   Block: 128 threads
 *   Shared: 2 * N * sizeof(float)
 *
 *   q: [1, H, Dh]
 *   k: [N, KH, Dh]
 *   v: [N, KH, Dh]
 *   y: [1, H, Dh]
 *
 * Online-softmax flash-style kernel.  One warp (32 threads) per query
 * head; each thread owns Dh/32 dims of the accumulator.  For each j:
 *   - threads cooperatively compute s_j = q·k[j] (partial dots + warp
 *     shuffle reduction; order within the warp is fixed by the shuffle
 *     tree, so the result is deterministic across runs on one host).
 *   - all threads update running softmax stats (m, l) and rescale their
 *     dims of acc.
 * Eliminates the single-threaded O(N·Dh) score loop and the O(N)
 * shared-memory scratch arrays the previous implementation used. */
__global__ void
gqa_attention_decode_kernel(const float*  __restrict__ q,
                            const __half* __restrict__ k,
                            const __half* __restrict__ v,
                            float*        __restrict__ y,
                            size_t N,
                            size_t H,
                            size_t KH,
                            size_t Dh,
                            float  inv_sqrt_dh,
                            size_t group)
{
  size_t h = blockIdx.x;
  if ( h >= H ) return;
  size_t kvh = h / group;

  const int   lane = (int)threadIdx.x;
  const int   WARP = 32;
  const int   dims_per_thread = (int)((Dh + WARP - 1) / WARP);

  /* Max 8 dims per thread — covers Dh up to 256 (qwen3: 128 → 4 dims).
   * Static sizing keeps these in registers rather than local memory. */
  float q_local[8];
  float acc[8];
  #pragma unroll
  for ( int i = 0; i < 8; i++ ) acc[i] = 0.0f;

  const float* q_ptr = q + h * Dh;
  #pragma unroll
  for ( int i = 0; i < 8; i++ ) {
    int d = lane + i * WARP;
    q_local[i] = ( (size_t)d < Dh ) ? q_ptr[d] : 0.0f;
  }

  float m = -INFINITY;
  float l = 0.0f;

  for ( size_t j = 0; j < N; j++ ) {
    const __half* k_ptr = k + j * KH * Dh + kvh * Dh;
    const __half* v_ptr = v + j * KH * Dh + kvh * Dh;

    /* Partial dot across warp; reduce via shuffle tree (fixed order).
     * Widen each K element to fp32 on load for math precision. */
    float s = 0.0f;
    #pragma unroll
    for ( int i = 0; i < 8; i++ ) {
      int d = lane + i * WARP;
      if ( (size_t)d < Dh && i < dims_per_thread ) {
        s = s + q_local[i] * __half2float(k_ptr[d]);
      }
    }
    #pragma unroll
    for ( int off = 16; off > 0; off /= 2 ) {
      s = s + __shfl_xor_sync(0xFFFFFFFF, s, off);
    }
    s = s * inv_sqrt_dh;

    /* Online softmax: fold s into running (m, l, acc). */
    float m_new = fmaxf(m, s);
    float factor = ( m == -INFINITY ) ? 0.0f : expf_hoon(m - m_new);
    float p      = expf_hoon(s - m_new);
    l = l * factor + p;

    #pragma unroll
    for ( int i = 0; i < 8; i++ ) {
      int d = lane + i * WARP;
      if ( (size_t)d < Dh && i < dims_per_thread ) {
        acc[i] = acc[i] * factor + p * __half2float(v_ptr[d]);
      }
    }
    m = m_new;
  }

  float inv_l = 1.0f / l;
  #pragma unroll
  for ( int i = 0; i < 8; i++ ) {
    int d = lane + i * WARP;
    if ( (size_t)d < Dh && i < dims_per_thread ) {
      y[h * Dh + d] = acc[i] * inv_l;
    }
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
  if ( cudaMallocAsync((void**)&d_q, q_bytes, 0)  != cudaSuccess ||
       cudaMallocAsync((void**)&d_k, kv_bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_v, kv_bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_y, y_bytes, 0)  != cudaSuccess ) {
    if ( d_q ) cudaFreeAsync(d_q, 0);
    if ( d_k ) cudaFreeAsync(d_k, 0);
    if ( d_v ) cudaFreeAsync(d_v, 0);
    if ( d_y ) cudaFreeAsync(d_y, 0);
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
  cudaFreeAsync(d_q, 0); cudaFreeAsync(d_k, 0); cudaFreeAsync(d_v, 0); cudaFreeAsync(d_y, 0);
  return st;
}
