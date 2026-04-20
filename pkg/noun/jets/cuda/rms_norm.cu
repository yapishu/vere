/*
 * rms_norm.cu — deterministic fp32 RMSNorm kernel.
 *
 * Strategy for determinism: one block per row.  Thread 0 computes the
 * sum-of-squares sequentially with fmaf (fixed reduction order), stores
 * rms in __shared__.  All threads then read rms and normalize their
 * assigned output slice.  No atomics, no warp reductions.
 *
 * Cost per row: O(D) for the reduction (single-threaded) + O(D/T) for
 * the parallel normalize.  For typical transformer shapes (D <= 8192)
 * this is memory-bound and tens of microseconds.  We're not competing
 * with cuBLAS-style tree-reductions — we're buying determinism plus
 * removing a multi-thousand-element Hoon per-row loop.
 */

#include "rms_norm.h"

#include <cuda_runtime.h>
#include <math.h>

#define BLOCK_THREADS 256

__global__ void
rms_norm_kernel(const float* __restrict__ x,
                const float* __restrict__ gamma,
                float        eps,
                float*       __restrict__ y,
                size_t S,
                size_t D)
{
  size_t s = blockIdx.x;
  if ( s >= S ) return;
  const float* x_row = x + s * D;
  float*       y_row = y + s * D;

  __shared__ float s_rms;

  if ( threadIdx.x == 0 ) {
    float sumsq = 0.0f;
    /* Explicit mul+add (two IEEE roundings) to match Hoon's softfloat
     * (add sumsq (mul v v)) byte-for-byte — not fused. */
    for ( size_t d = 0; d < D; d++ ) {
      sumsq = sumsq + x_row[d] * x_row[d];
    }
    float mean_sq = sumsq / (float)D;
    s_rms = sqrtf(mean_sq + eps);
  }
  __syncthreads();

  float rms = s_rms;
  for ( size_t d = threadIdx.x; d < D; d += BLOCK_THREADS ) {
    /* x/rms * gamma — one rounding via fmaf-free mul-div chain would
     * differ from CPU reference; use explicit div then mul to match
     * the per-element CPU form. */
    float v = x_row[d] / rms;
    y_row[d] = v * gamma[d];
  }
}

static int
_rms_norm_init_once(void)
{
  static int done = 0;
  if ( done ) return 1;
  int dev = 0;
  if ( cudaGetDeviceCount(&dev) != cudaSuccess || dev == 0 ) return 0;
  if ( cudaSetDevice(0) != cudaSuccess ) return 0;
  done = 1;
  return 1;
}

extern "C" rms_norm_status
rms_norm_fp32(const float* x,
              const float* gamma,
              float        eps,
              float*       y,
              size_t       S,
              size_t       D)
{
  if ( !x || !gamma || !y || S == 0 || D == 0 ) return RMSN_INVALID_ARG;
  if ( !_rms_norm_init_once() ) return RMSN_NO_CUDA;

  float *d_x = NULL, *d_g = NULL, *d_y = NULL;
  size_t x_bytes = S * D * sizeof(float);
  size_t g_bytes = D * sizeof(float);

  if ( cudaMalloc((void**)&d_x, x_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_g, g_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_y, x_bytes) != cudaSuccess ) {
    if ( d_x ) cudaFree(d_x);
    if ( d_g ) cudaFree(d_g);
    if ( d_y ) cudaFree(d_y);
    return RMSN_ALLOC_FAIL;
  }
  cudaMemcpy(d_x, x,     x_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_g, gamma, g_bytes, cudaMemcpyHostToDevice);

  rms_norm_kernel<<<(unsigned)S, BLOCK_THREADS>>>(d_x, d_g, eps, d_y, S, D);
  if ( cudaGetLastError() != cudaSuccess ||
       cudaDeviceSynchronize() != cudaSuccess ||
       cudaMemcpy(y, d_y, x_bytes, cudaMemcpyDeviceToHost) != cudaSuccess ) {
    cudaFree(d_x); cudaFree(d_g); cudaFree(d_y);
    return RMSN_LAUNCH_FAIL;
  }
  cudaFree(d_x); cudaFree(d_g); cudaFree(d_y);
  return RMSN_OK;
}
