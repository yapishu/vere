/*
 * silu_mul.cu — fused fp32 SiLU(a) * b kernel.
 *
 *   y[i] = (a[i] / (1 + expf(-a[i]))) * b[i]
 *
 * Elementwise, no reduction — one thread per output.  All ops are
 * IEEE-754 deterministic under our compile flags (no --ftz, no
 * fast-math, expf is IEEE-compliant).
 */

#include "silu_mul.h"
#include "expf_hoon.cuh"

#include <cuda_runtime.h>

__global__ void
silu_mul_kernel(const float* __restrict__ a,
                const float* __restrict__ b,
                float*       __restrict__ y,
                size_t N)
{
  size_t i = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
  if ( i >= N ) return;
  float av = a[i];
  /* sigmoid(x) = 1 / (1 + exp(-x)).  Use expf_hoon for byte-exact
   * match against Hoon's rs:math exp. */
  float sig = 1.0f / (1.0f + expf_hoon(-av));
  y[i] = (av * sig) * b[i];
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

extern "C" silu_mul_status
silu_mul_fp32(const float* a, const float* b, float* y, size_t N)
{
  if ( !a || !b || !y || N == 0 ) return SILU_INVALID_ARG;
  if ( !_init_once() ) return SILU_NO_CUDA;

  size_t bytes = N * sizeof(float);
  float *d_a = NULL, *d_b = NULL, *d_y = NULL;
  if ( cudaMallocAsync((void**)&d_a, bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_b, bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_y, bytes, 0) != cudaSuccess ) {
    if ( d_a ) cudaFreeAsync(d_a, 0);
    if ( d_b ) cudaFreeAsync(d_b, 0);
    if ( d_y ) cudaFreeAsync(d_y, 0);
    return SILU_ALLOC_FAIL;
  }
  cudaMemcpy(d_a, a, bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_b, b, bytes, cudaMemcpyHostToDevice);

  size_t threads = 256;
  size_t blocks = (N + threads - 1) / threads;
  silu_mul_kernel<<<(unsigned)blocks, (unsigned)threads>>>(d_a, d_b, d_y, N);

  silu_mul_status st = SILU_OK;
  if ( cudaGetLastError() != cudaSuccess ||
       cudaDeviceSynchronize() != cudaSuccess ||
       cudaMemcpy(y, d_y, bytes, cudaMemcpyDeviceToHost) != cudaSuccess ) {
    st = SILU_LAUNCH_FAIL;
  }
  cudaFreeAsync(d_a, 0); cudaFreeAsync(d_b, 0); cudaFreeAsync(d_y, 0);
  return st;
}
