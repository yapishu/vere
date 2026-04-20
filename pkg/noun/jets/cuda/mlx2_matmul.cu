/*
 * mlx2_matmul.cu — fused MLX 2-bit dequant + fp32 matmul kernel.
 *
 * Per output (s, o):
 *   acc = 0
 *   for i in 0..in_features-1:
 *     q     = (w[o, i/16] >> ((i%16)*2)) & 0x3
 *     w_fp  = fmaf(scales[o, i/group], (float)q, biases[o, i/group])
 *     acc   = fmaf(x[s, i], w_fp, acc)
 *   y[s, o] = acc
 *
 * Determinism: same guarantees as sgemm_det.cu — sequential fmaf over i,
 * one thread per output element, no atomics or warp reductions.
 *
 * Note: identical result to (cpu-dequant → cpu sgemm_ref) because
 *   fmaf(a, b, fmaf(c, d, 0))  ==  fmaf(c, d, a*b) under single-round fma
 *   ... and (scale * q + bias) rounded once per element, then used as the
 * b operand of the outer fmaf, is exactly what the CPU reference does.
 */

#include "mlx2_matmul.h"

#include <cuda_runtime.h>
#include <stdio.h>

#define CUDA_TRY(call) do {                          \
    if ( (call) != cudaSuccess ) return MLX2_MATMUL_LAUNCH_FAIL; \
  } while (0)

__global__ void
mlx2_matmul_kernel(const float*    __restrict__ x,
                   const uint32_t* __restrict__ w,
                   const float*    __restrict__ scales,
                   const float*    __restrict__ biases,
                   float*          __restrict__ y,
                   size_t S,
                   size_t in_features,
                   size_t out_features,
                   size_t group_size,
                   size_t packed_cols,
                   size_t groups_per_row)
{
  size_t s = (size_t)blockIdx.y * blockDim.y + threadIdx.y;
  size_t o = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
  if ( s >= S || o >= out_features ) return;

  size_t gpr_offset = o * groups_per_row;
  size_t w_row      = o * packed_cols;
  const float* x_row = x + s * in_features;

  float acc = 0.0f;

  for ( size_t wc = 0; wc < packed_cols; wc++ ) {
    uint32_t word = w[w_row + wc];
    size_t i_base = wc * 16;
    /* unroll by 16 with fixed k order */
    #pragma unroll
    for ( int k = 0; k < 16; k++ ) {
      size_t i = i_base + (size_t)k;
      size_t grp = i / group_size;
      float scale = scales[gpr_offset + grp];
      float bias  = biases[gpr_offset + grp];
      uint32_t q  = (word >> (k * 2)) & 0x3u;
      /* Explicit mul+add (two IEEE roundings) — byte-exact against
       * Hoon's softfloat (add (mul scale qf) bias) / (add (mul xi w) acc). */
      float w_fp  = (scale * (float)q) + bias;
      acc = acc + x_row[i] * w_fp;
    }
  }
  y[s * out_features + o] = acc;
}

static int
_mlx2_init_once(void)
{
  static int done = 0;
  if ( done ) return 1;
  int dev = 0;
  if ( cudaGetDeviceCount(&dev) != cudaSuccess || dev == 0 ) return 0;
  if ( cudaSetDevice(0) != cudaSuccess ) return 0;
  done = 1;
  return 1;
}

static mlx2_matmul_status
_mlx2_launch(const float* x_host,
             uintptr_t    w_dptr,
             uintptr_t    s_dptr,
             uintptr_t    b_dptr,
             float*       y_host,
             size_t S, size_t in_f, size_t out_f, size_t group)
{
  size_t packed_cols    = in_f / 16;
  size_t groups_per_row = in_f / group;

  size_t x_bytes = S * in_f * sizeof(float);
  size_t y_bytes = S * out_f * sizeof(float);

  float *d_x = NULL, *d_y = NULL;
  if ( cudaMalloc((void**)&d_x, x_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_y, y_bytes) != cudaSuccess ) {
    if ( d_x ) cudaFree(d_x);
    if ( d_y ) cudaFree(d_y);
    return MLX2_MATMUL_ALLOC_FAIL;
  }
  if ( cudaMemcpy(d_x, x_host, x_bytes, cudaMemcpyHostToDevice) != cudaSuccess ) {
    cudaFree(d_x); cudaFree(d_y);
    return MLX2_MATMUL_LAUNCH_FAIL;
  }

  dim3 block(16, 16);
  dim3 grid((unsigned)((out_f + 15) / 16), (unsigned)((S + 15) / 16));
  mlx2_matmul_kernel<<<grid, block>>>(
    d_x,
    (const uint32_t*)(void*)w_dptr,
    (const float*)(void*)s_dptr,
    (const float*)(void*)b_dptr,
    d_y,
    S, in_f, out_f, group,
    packed_cols, groups_per_row);

  if ( cudaGetLastError() != cudaSuccess ||
       cudaDeviceSynchronize() != cudaSuccess ||
       cudaMemcpy(y_host, d_y, y_bytes, cudaMemcpyDeviceToHost) != cudaSuccess ) {
    cudaFree(d_x); cudaFree(d_y);
    return MLX2_MATMUL_LAUNCH_FAIL;
  }

  cudaFree(d_x);
  cudaFree(d_y);
  return MLX2_MATMUL_OK;
}

extern "C" mlx2_matmul_status
mlx2_matmul_fresh(const float*    x,
                  const uint32_t* w_packed,
                  const float*    scales,
                  const float*    biases,
                  float*          y,
                  size_t S, size_t in_f, size_t out_f, size_t group)
{
  if ( !x || !w_packed || !scales || !biases || !y ||
       S == 0 || in_f == 0 || out_f == 0 || group == 0 ||
       (in_f % 16) != 0 || (in_f % group) != 0 ) {
    return MLX2_MATMUL_INVALID_ARG;
  }
  if ( !_mlx2_init_once() ) return MLX2_MATMUL_NO_CUDA;

  size_t packed_cols    = in_f / 16;
  size_t groups_per_row = in_f / group;
  size_t w_bytes = out_f * packed_cols * sizeof(uint32_t);
  size_t s_bytes = out_f * groups_per_row * sizeof(float);

  uint32_t *d_w = NULL;
  float    *d_s = NULL, *d_b = NULL;
  if ( cudaMalloc((void**)&d_w, w_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_s, s_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_b, s_bytes) != cudaSuccess ) {
    if ( d_w ) cudaFree(d_w);
    if ( d_s ) cudaFree(d_s);
    if ( d_b ) cudaFree(d_b);
    return MLX2_MATMUL_ALLOC_FAIL;
  }
  cudaMemcpy(d_w, w_packed, w_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_s, scales,   s_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_b, biases,   s_bytes, cudaMemcpyHostToDevice);

  mlx2_matmul_status st = _mlx2_launch(
    x, (uintptr_t)d_w, (uintptr_t)d_s, (uintptr_t)d_b, y,
    S, in_f, out_f, group);

  cudaFree(d_w);
  cudaFree(d_s);
  cudaFree(d_b);
  return st;
}

extern "C" mlx2_matmul_status
mlx2_matmul_cached(const float* x,
                   uintptr_t    w_packed_dptr,
                   uintptr_t    scales_dptr,
                   uintptr_t    biases_dptr,
                   float*       y,
                   size_t S, size_t in_f, size_t out_f, size_t group)
{
  if ( !x || !y || w_packed_dptr == 0 || scales_dptr == 0 || biases_dptr == 0 ||
       S == 0 || in_f == 0 || out_f == 0 || group == 0 ||
       (in_f % 16) != 0 || (in_f % group) != 0 ) {
    return MLX2_MATMUL_INVALID_ARG;
  }
  if ( !_mlx2_init_once() ) return MLX2_MATMUL_NO_CUDA;

  return _mlx2_launch(x, w_packed_dptr, scales_dptr, biases_dptr, y,
                      S, in_f, out_f, group);
}
