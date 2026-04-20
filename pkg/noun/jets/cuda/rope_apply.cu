/*
 * rope_apply.cu — deterministic fp32 rotary positional embedding kernel.
 *
 * Elementwise: one thread per output element, no reduction, no atomics.
 * Bit-exact across any IEEE-754 GPU because each output is a fixed
 * two-fmaf computation over a single (x, rotate_half(x), cos, sin)
 * quadruple.
 */

#include "rope_apply.h"

#include <cuda_runtime.h>

__global__ void
rope_apply_kernel(const float* __restrict__ x,
                  const float* __restrict__ cos_tbl,
                  const float* __restrict__ sin_tbl,
                  float*       __restrict__ y,
                  size_t S,
                  size_t H,
                  size_t Dh,
                  size_t half)
{
  size_t j = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
  size_t h = (size_t)blockIdx.y * blockDim.y + threadIdx.y;
  size_t p = (size_t)blockIdx.z;
  if ( j >= Dh || h >= H || p >= S ) return;

  size_t base = p * H * Dh + h * Dh;
  float xj = x[base + j];

  /* rotate_half: j < half → -x[j + half]; else +x[j - half] */
  float xr;
  if ( j < half ) xr = -x[base + j + half];
  else            xr =  x[base + j - half];

  size_t cs_idx = p * Dh + j;
  float cp = cos_tbl[cs_idx];
  float sp = sin_tbl[cs_idx];

  /* Explicit mul+add (two IEEE roundings per term), then add the two
   * products.  Matches Hoon's  (add (mul xj cp) (mul rot sp))  byte-exact. */
  y[base + j] = (xj * cp) + (xr * sp);
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

extern "C" rope_apply_status
rope_apply_fp32(const float* x,
                const float* cos_tbl,
                const float* sin_tbl,
                float*       y,
                size_t       S,
                size_t       H,
                size_t       Dh)
{
  if ( !x || !cos_tbl || !sin_tbl || !y ||
       S == 0 || H == 0 || Dh == 0 || (Dh & 1) ) {
    return ROPE_INVALID_ARG;
  }
  if ( !_init_once() ) return ROPE_NO_CUDA;

  size_t half = Dh / 2;
  size_t x_bytes  = S * H * Dh * sizeof(float);
  size_t cs_bytes = S * Dh * sizeof(float);

  float *d_x = NULL, *d_c = NULL, *d_s = NULL, *d_y = NULL;
  if ( cudaMalloc((void**)&d_x, x_bytes)  != cudaSuccess ||
       cudaMalloc((void**)&d_c, cs_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_s, cs_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_y, x_bytes)  != cudaSuccess ) {
    if ( d_x ) cudaFree(d_x);
    if ( d_c ) cudaFree(d_c);
    if ( d_s ) cudaFree(d_s);
    if ( d_y ) cudaFree(d_y);
    return ROPE_ALLOC_FAIL;
  }
  cudaMemcpy(d_x, x,       x_bytes,  cudaMemcpyHostToDevice);
  cudaMemcpy(d_c, cos_tbl, cs_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_s, sin_tbl, cs_bytes, cudaMemcpyHostToDevice);

  dim3 block(64, 4);
  dim3 grid((unsigned)((Dh + 63) / 64), (unsigned)((H + 3) / 4), (unsigned)S);
  rope_apply_kernel<<<grid, block>>>(d_x, d_c, d_s, d_y, S, H, Dh, half);

  rope_apply_status st = ROPE_OK;
  if ( cudaGetLastError() != cudaSuccess ||
       cudaDeviceSynchronize() != cudaSuccess ||
       cudaMemcpy(y, d_y, x_bytes, cudaMemcpyDeviceToHost) != cudaSuccess ) {
    st = ROPE_LAUNCH_FAIL;
  }
  cudaFree(d_x); cudaFree(d_c); cudaFree(d_s); cudaFree(d_y);
  return st;
}
