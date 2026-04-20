/*
 * sgemm_det.cu — deterministic fp32 matmul kernel.
 *
 * Strategy for determinism: one thread per output element, accumulation loop
 * runs left-to-right over the K axis in a fixed order.  No atomicAdd, no
 * warp shuffle reductions, no tensor cores.  Operates on row-major data.
 *
 *     tid = (row, col)
 *     acc = 0.0f
 *     for k in 0..K-1:  acc = fmaf(A[row,k], B[k,col], acc)
 *     C[row, col] = acc
 *
 * fmaf is IEEE-754 fused multiply-add — deterministic on all supported GPUs.
 * Reading A by stride K and B by stride N is naive; coalescing is only
 * one-sided, so this is ~ memory-bound.  We're not competing with cuBLAS on
 * throughput here; we're buying determinism plus saving the dequant/copy
 * round-trip that currently dominates on-ship inference.  A tiled variant
 * comes later once the API/caching story is settled.
 */

#include "sgemm_det.h"

#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>

#define CUDA_CHECK(call, on_fail) do {                                    \
    cudaError_t _err = (call);                                            \
    if (_err != cudaSuccess) {                                            \
      fprintf(stderr, "[sgemm_det] %s:%d %s -> %s\n",                     \
              __FILE__, __LINE__, #call, cudaGetErrorString(_err));       \
      on_fail;                                                            \
    }                                                                     \
  } while (0)

static int g_initialized = 0;

/*
 * Determinism-safe kernel: one thread per (row, col) output; loop over K
 * sequentially with fmaf.
 */
__global__ void
sgemm_det_kernel(const float* __restrict__ a,
                 const float* __restrict__ b,
                 float*       __restrict__ c,
                 size_t M,
                 size_t K,
                 size_t N)
{
  size_t row = (size_t)blockIdx.y * blockDim.y + threadIdx.y;
  size_t col = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
  if ( row >= M || col >= N ) return;

  float acc = 0.0f;
  /* Strictly sequential accumulation.  Explicit mul then add (two
   * IEEE roundings) rather than fmaf's one rounding — this matches
   * Hoon's softfloat `(add (mul a b) c)` byte-exactly. */
  for ( size_t k = 0; k < K; k++ ) {
    acc = acc + a[row * K + k] * b[k * N + col];
  }
  c[row * N + col] = acc;
}

extern "C" sgemm_det_status
sgemm_det_init(void)
{
  if ( g_initialized ) return SGEMM_DET_OK;
  int dev_count = 0;
  cudaError_t err = cudaGetDeviceCount(&dev_count);
  if ( err != cudaSuccess || dev_count == 0 ) {
    return SGEMM_DET_NO_CUDA;
  }
  CUDA_CHECK(cudaSetDevice(0), return SGEMM_DET_NO_CUDA);
  g_initialized = 1;
  return SGEMM_DET_OK;
}

extern "C" void
sgemm_det_shutdown(void)
{
  if ( !g_initialized ) return;
  cudaDeviceReset();
  g_initialized = 0;
}

static sgemm_det_status
_launch(const float* a_host, float* d_a, size_t a_bytes,
        const float* d_b,
        float*       c_host, float* d_c, size_t c_bytes,
        size_t M, size_t K, size_t N)
{
  if ( a_host ) {
    cudaError_t err = cudaMemcpy(d_a, a_host, a_bytes, cudaMemcpyHostToDevice);
    if ( err != cudaSuccess ) return SGEMM_DET_LAUNCH_FAIL;
  }
  dim3 block(16, 16);
  dim3 grid((unsigned)((N + 15) / 16), (unsigned)((M + 15) / 16));
  sgemm_det_kernel<<<grid, block>>>(d_a, d_b, d_c, M, K, N);
  if ( cudaGetLastError() != cudaSuccess ) return SGEMM_DET_LAUNCH_FAIL;
  if ( cudaDeviceSynchronize() != cudaSuccess ) return SGEMM_DET_LAUNCH_FAIL;
  if ( cudaMemcpy(c_host, d_c, c_bytes, cudaMemcpyDeviceToHost) != cudaSuccess )
    return SGEMM_DET_LAUNCH_FAIL;
  return SGEMM_DET_OK;
}

extern "C" sgemm_det_status
sgemm_det_row_major(const float* a,
                    const float* b,
                    float*       c,
                    size_t       M,
                    size_t       K,
                    size_t       N)
{
  if ( !a || !b || !c || M == 0 || K == 0 || N == 0 ) {
    return SGEMM_DET_INVALID_ARG;
  }
  sgemm_det_status s = sgemm_det_init();
  if ( s != SGEMM_DET_OK ) return s;

  float *d_a = NULL, *d_b = NULL, *d_c = NULL;
  size_t a_bytes = M * K * sizeof(float);
  size_t b_bytes = K * N * sizeof(float);
  size_t c_bytes = M * N * sizeof(float);

  if ( cudaMallocAsync((void**)&d_a, a_bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_b, b_bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_c, c_bytes, 0) != cudaSuccess ) {
    if ( d_a ) cudaFreeAsync(d_a, 0);
    if ( d_b ) cudaFreeAsync(d_b, 0);
    if ( d_c ) cudaFreeAsync(d_c, 0);
    return SGEMM_DET_ALLOC_FAIL;
  }
  if ( cudaMemcpy(d_b, b, b_bytes, cudaMemcpyHostToDevice) != cudaSuccess ) {
    cudaFreeAsync(d_a, 0); cudaFreeAsync(d_b, 0); cudaFreeAsync(d_c, 0);
    return SGEMM_DET_LAUNCH_FAIL;
  }
  sgemm_det_status st = _launch(a, d_a, a_bytes, d_b, c, d_c, c_bytes, M, K, N);
  cudaFreeAsync(d_a, 0); cudaFreeAsync(d_b, 0); cudaFreeAsync(d_c, 0);
  return st;
}

extern "C" sgemm_det_status
sgemm_det_row_major_cached_b(const float* a,
                             uintptr_t    b_dptr,
                             float*       c,
                             size_t       M,
                             size_t       K,
                             size_t       N)
{
  if ( !a || !c || b_dptr == 0 || M == 0 || K == 0 || N == 0 ) {
    return SGEMM_DET_INVALID_ARG;
  }
  sgemm_det_status s = sgemm_det_init();
  if ( s != SGEMM_DET_OK ) return s;

  float *d_a = NULL, *d_c = NULL;
  float *d_b = (float*)(void*)b_dptr;
  size_t a_bytes = M * K * sizeof(float);
  size_t c_bytes = M * N * sizeof(float);

  if ( cudaMallocAsync((void**)&d_a, a_bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_c, c_bytes, 0) != cudaSuccess ) {
    if ( d_a ) cudaFreeAsync(d_a, 0);
    if ( d_c ) cudaFreeAsync(d_c, 0);
    return SGEMM_DET_ALLOC_FAIL;
  }
  sgemm_det_status st = _launch(a, d_a, a_bytes, d_b, c, d_c, c_bytes, M, K, N);
  cudaFreeAsync(d_a, 0); cudaFreeAsync(d_c, 0);
  return st;
}
