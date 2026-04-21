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
#include "expf_hoon.cuh"

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

/*
 * Fused RMSNorm + MLX2 matmul kernel.
 *
 * Equivalent to running rms_norm_kernel(x, gamma, eps) -> x1 followed by
 * mlx2_matmul_kernel(x1, w, scales, biases) -> y, but without ever
 * materializing x1 in global memory.
 *
 * Determinism: the rms reduction is single-threaded (thread 0 of the
 * block, sequential sum across D), identical to rms_norm_kernel.  The
 * normalization uses `x[d] / rms` (division, not multiply-by-reciprocal)
 * and applies gamma in the same order as the un-fused rms_norm.  The
 * matmul loop reads the normalized value inline but computes it as
 * ((x[d] / rms) * gamma[d]) * w_fp — same three-op sequence, same
 * IEEE roundings — so the output matches the un-fused pipeline byte
 * for byte.
 *
 * Grid:  (ceil(out / blockDim.x), S)  — one block per (row, out-tile)
 * Block: (BLOCK_OUT,)                  — 128 is a reasonable default
 * Shared memory: one float (broadcast rms for this row).
 */
__global__ void
rmsnorm_mlx2_matmul_kernel(const float*    __restrict__ x,
                           const float*    __restrict__ gamma,
                           float                         eps,
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
  size_t s = blockIdx.y;
  if ( s >= S ) return;
  size_t o = (size_t)blockIdx.x * blockDim.x + threadIdx.x;

  __shared__ float s_rms;

  const float* x_row = x + s * in_features;

  if ( threadIdx.x == 0 ) {
    /* Byte-exact with rms_norm_kernel: sequential mul+add sumsq, then
     * sqrt(mean_sq + eps).  No fmaf, no warp reductions. */
    float sumsq = 0.0f;
    for ( size_t d = 0; d < in_features; d++ ) {
      sumsq = sumsq + x_row[d] * x_row[d];
    }
    float mean_sq = sumsq / (float)in_features;
    s_rms = sqrtf(mean_sq + eps);
  }
  __syncthreads();

  if ( o >= out_features ) return;

  float rms = s_rms;
  size_t gpr_offset = o * groups_per_row;
  size_t w_row      = o * packed_cols;

  float acc = 0.0f;
  for ( size_t wc = 0; wc < packed_cols; wc++ ) {
    uint32_t word = w[w_row + wc];
    size_t i_base = wc * 16;
    #pragma unroll
    for ( int k = 0; k < 16; k++ ) {
      size_t i    = i_base + (size_t)k;
      size_t grp  = i / group_size;
      float scale = scales[gpr_offset + grp];
      float bias  = biases[gpr_offset + grp];
      uint32_t q  = (word >> (k * 2)) & 0x3u;
      float w_fp  = (scale * (float)q) + bias;
      /* Fused rmsnorm: (x[i]/rms) * gamma[i].  Same op order and ULP
       * rounding as rms_norm_kernel's normalize step. */
      float x_normed = (x_row[i] / rms) * gamma[i];
      acc = acc + x_normed * w_fp;
    }
  }
  y[s * out_features + o] = acc;
}

/* Host-side driver for testing.  Allocates VRAM, uploads inputs, launches
 * the fused kernel, downloads output. */
extern "C" mlx2_matmul_status
rmsnorm_mlx2_matmul_fresh(const float*    x,
                          const float*    gamma,
                          float           eps,
                          const uint32_t* w_packed,
                          const float*    scales,
                          const float*    biases,
                          float*          y,
                          size_t          S,
                          size_t          in_features,
                          size_t          out_features,
                          size_t          group_size)
{
  if ( !x || !gamma || !w_packed || !scales || !biases || !y ||
       S == 0 || in_features == 0 || out_features == 0 || group_size == 0 ||
       (in_features % group_size) != 0 || (in_features % 16) != 0 ) {
    return MLX2_MATMUL_INVALID_ARG;
  }
  size_t packed_cols    = in_features / 16;
  size_t groups_per_row = in_features / group_size;
  size_t w_bytes  = out_features * packed_cols    * sizeof(uint32_t);
  size_t sb_bytes = out_features * groups_per_row * sizeof(float);
  size_t g_bytes  = in_features * sizeof(float);
  size_t x_bytes  = S * in_features  * sizeof(float);
  size_t y_bytes  = S * out_features * sizeof(float);

  float    *d_x = NULL, *d_gamma = NULL, *d_s = NULL, *d_b = NULL, *d_y = NULL;
  uint32_t *d_w = NULL;
  if ( cudaMalloc((void**)&d_x,     x_bytes)  != cudaSuccess ||
       cudaMalloc((void**)&d_gamma, g_bytes)  != cudaSuccess ||
       cudaMalloc((void**)&d_w,     w_bytes)  != cudaSuccess ||
       cudaMalloc((void**)&d_s,     sb_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_b,     sb_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_y,     y_bytes)  != cudaSuccess ) {
    if (d_x) cudaFree(d_x); if (d_gamma) cudaFree(d_gamma);
    if (d_w) cudaFree(d_w); if (d_s) cudaFree(d_s);
    if (d_b) cudaFree(d_b); if (d_y) cudaFree(d_y);
    return MLX2_MATMUL_ALLOC_FAIL;
  }
  cudaMemcpy(d_x,     x,        x_bytes,  cudaMemcpyHostToDevice);
  cudaMemcpy(d_gamma, gamma,    g_bytes,  cudaMemcpyHostToDevice);
  cudaMemcpy(d_w,     w_packed, w_bytes,  cudaMemcpyHostToDevice);
  cudaMemcpy(d_s,     scales,   sb_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_b,     biases,   sb_bytes, cudaMemcpyHostToDevice);

  dim3 block(128);
  dim3 grid((unsigned)((out_features + 127) / 128), (unsigned)S);
  rmsnorm_mlx2_matmul_kernel<<<grid, block>>>(
    d_x, d_gamma, eps, d_w, d_s, d_b, d_y,
    S, in_features, out_features, group_size,
    packed_cols, groups_per_row);

  mlx2_matmul_status st = MLX2_MATMUL_OK;
  if ( cudaGetLastError() != cudaSuccess ||
       cudaDeviceSynchronize() != cudaSuccess ||
       cudaMemcpy(y, d_y, y_bytes, cudaMemcpyDeviceToHost) != cudaSuccess ) {
    st = MLX2_MATMUL_LAUNCH_FAIL;
  }
  cudaFree(d_x); cudaFree(d_gamma); cudaFree(d_w);
  cudaFree(d_s); cudaFree(d_b); cudaFree(d_y);
  return st;
}

/*
 * Fused gate+up matmul + SiLU*mul.
 *
 * Replaces the three-kernel sequence:
 *   mlx2_matmul(x, gate_w)  -> gate [S, D_ff]
 *   mlx2_matmul(x, up_w)    -> up   [S, D_ff]
 *   silu_mul(gate, up)      -> hid  [S, D_ff]
 *
 * with one kernel that, per output (s, o_ff), computes both dot
 * products and immediately folds silu*up.  Intermediate gate and up
 * never touch global memory.
 *
 * Determinism: each dot product uses the same sequential mul+add order
 * as mlx2_matmul_kernel (byte-exact).  SiLU uses expf_hoon with the
 * same op sequence as silu_mul_kernel (byte-exact).  The final
 * multiply by `up_val` is the same final multiplication silu_mul_kernel
 * does.  So the output of this fused kernel equals the three-kernel
 * pipeline byte-for-byte.
 */
__global__ void
gate_up_silu_mlx2_kernel(const float*    __restrict__ x,
                         const uint32_t* __restrict__ gate_w,
                         const float*    __restrict__ gate_s,
                         const float*    __restrict__ gate_b,
                         const uint32_t* __restrict__ up_w,
                         const float*    __restrict__ up_s,
                         const float*    __restrict__ up_b,
                         float*          __restrict__ hid,
                         size_t S,
                         size_t in_features,
                         size_t out_features,
                         size_t group_size,
                         size_t packed_cols,
                         size_t groups_per_row);

/* Host-side driver for testing. */
extern "C" mlx2_matmul_status
gate_up_silu_mlx2_fresh(const float*    x,
                        const uint32_t* gate_w, const float* gate_s, const float* gate_b,
                        const uint32_t* up_w,   const float* up_s,   const float* up_b,
                        float*          hid,
                        size_t S, size_t in_features, size_t out_features,
                        size_t group_size);

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
  if ( cudaMallocAsync((void**)&d_x, x_bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_y, y_bytes, 0) != cudaSuccess ) {
    if ( d_x ) cudaFreeAsync(d_x, 0);
    if ( d_y ) cudaFreeAsync(d_y, 0);
    return MLX2_MATMUL_ALLOC_FAIL;
  }
  if ( cudaMemcpy(d_x, x_host, x_bytes, cudaMemcpyHostToDevice) != cudaSuccess ) {
    cudaFreeAsync(d_x, 0); cudaFreeAsync(d_y, 0);
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
    cudaFreeAsync(d_x, 0); cudaFreeAsync(d_y, 0);
    return MLX2_MATMUL_LAUNCH_FAIL;
  }

  cudaFreeAsync(d_x, 0);
  cudaFreeAsync(d_y, 0);
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
  if ( cudaMallocAsync((void**)&d_w, w_bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_s, s_bytes, 0) != cudaSuccess ||
       cudaMallocAsync((void**)&d_b, s_bytes, 0) != cudaSuccess ) {
    if ( d_w ) cudaFreeAsync(d_w, 0);
    if ( d_s ) cudaFreeAsync(d_s, 0);
    if ( d_b ) cudaFreeAsync(d_b, 0);
    return MLX2_MATMUL_ALLOC_FAIL;
  }
  cudaMemcpy(d_w, w_packed, w_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_s, scales,   s_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_b, biases,   s_bytes, cudaMemcpyHostToDevice);

  mlx2_matmul_status st = _mlx2_launch(
    x, (uintptr_t)d_w, (uintptr_t)d_s, (uintptr_t)d_b, y,
    S, in_f, out_f, group);

  cudaFreeAsync(d_w, 0);
  cudaFreeAsync(d_s, 0);
  cudaFreeAsync(d_b, 0);
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

/*
 * Fused gate_matmul + up_matmul + silu_mul kernel.
 *
 * Each thread computes one (s, o_ff) element of hid by doing TWO dot
 * products (against gate_w and up_w) in its inner loop, then applies
 * silu*mul inline — all without writing gate or up to global memory.
 *
 * Determinism:
 *   - The two dot-product loops use the same sequential mul+add order
 *     as mlx2_matmul_kernel (byte-exact against Hoon softfloat).
 *   - silu uses `1 / (1 + expf_hoon(-gate_val))` and the final combine
 *     is `(gate_val * sig) * up_val` — the exact sequence in
 *     silu_mul_kernel.
 *   - No fma, no warp reductions, no atomics.
 */
__global__ void
gate_up_silu_mlx2_kernel(const float*    __restrict__ x,
                         const uint32_t* __restrict__ gate_w,
                         const float*    __restrict__ gate_s,
                         const float*    __restrict__ gate_b,
                         const uint32_t* __restrict__ up_w,
                         const float*    __restrict__ up_s,
                         const float*    __restrict__ up_b,
                         float*          __restrict__ hid,
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

  size_t gpr_off = o * groups_per_row;
  size_t w_row   = o * packed_cols;
  const float* x_row = x + s * in_features;

  float gate_acc = 0.0f;
  float up_acc   = 0.0f;

  for ( size_t wc = 0; wc < packed_cols; wc++ ) {
    uint32_t g_word = gate_w[w_row + wc];
    uint32_t u_word = up_w  [w_row + wc];
    size_t i_base = wc * 16;
    #pragma unroll
    for ( int k = 0; k < 16; k++ ) {
      size_t i   = i_base + (size_t)k;
      size_t grp = i / group_size;
      float g_scale = gate_s[gpr_off + grp];
      float g_bias  = gate_b[gpr_off + grp];
      float u_scale = up_s  [gpr_off + grp];
      float u_bias  = up_b  [gpr_off + grp];
      uint32_t g_q = (g_word >> (k * 2)) & 0x3u;
      uint32_t u_q = (u_word >> (k * 2)) & 0x3u;
      float g_w_fp = (g_scale * (float)g_q) + g_bias;
      float u_w_fp = (u_scale * (float)u_q) + u_bias;
      float xi = x_row[i];
      gate_acc = gate_acc + xi * g_w_fp;
      up_acc   = up_acc   + xi * u_w_fp;
    }
  }

  /* SiLU*mul — identical to silu_mul_kernel's per-element op sequence. */
  float sig = 1.0f / (1.0f + expf_hoon(-gate_acc));
  hid[s * out_features + o] = (gate_acc * sig) * up_acc;
}

/* Host-side driver for testing. */
extern "C" mlx2_matmul_status
gate_up_silu_mlx2_fresh(const float*    x,
                        const uint32_t* gate_w, const float* gate_s, const float* gate_b,
                        const uint32_t* up_w,   const float* up_s,   const float* up_b,
                        float*          hid,
                        size_t S, size_t in_features, size_t out_features,
                        size_t group_size)
{
  if ( !x || !gate_w || !gate_s || !gate_b || !up_w || !up_s || !up_b || !hid ||
       S == 0 || in_features == 0 || out_features == 0 || group_size == 0 ||
       (in_features % group_size) != 0 || (in_features % 16) != 0 ) {
    return MLX2_MATMUL_INVALID_ARG;
  }
  size_t packed_cols    = in_features / 16;
  size_t groups_per_row = in_features / group_size;
  size_t w_bytes  = out_features * packed_cols    * sizeof(uint32_t);
  size_t sb_bytes = out_features * groups_per_row * sizeof(float);
  size_t x_bytes  = S * in_features   * sizeof(float);
  size_t h_bytes  = S * out_features  * sizeof(float);

  float    *d_x = NULL;
  uint32_t *d_gw = NULL, *d_uw = NULL;
  float    *d_gs = NULL, *d_gb = NULL, *d_us = NULL, *d_ub = NULL, *d_h = NULL;
  if ( cudaMalloc((void**)&d_x,  x_bytes)  != cudaSuccess ||
       cudaMalloc((void**)&d_gw, w_bytes)  != cudaSuccess ||
       cudaMalloc((void**)&d_gs, sb_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_gb, sb_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_uw, w_bytes)  != cudaSuccess ||
       cudaMalloc((void**)&d_us, sb_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_ub, sb_bytes) != cudaSuccess ||
       cudaMalloc((void**)&d_h,  h_bytes)  != cudaSuccess ) {
    if (d_x)  cudaFree(d_x);  if (d_gw) cudaFree(d_gw);
    if (d_gs) cudaFree(d_gs); if (d_gb) cudaFree(d_gb);
    if (d_uw) cudaFree(d_uw); if (d_us) cudaFree(d_us);
    if (d_ub) cudaFree(d_ub); if (d_h)  cudaFree(d_h);
    return MLX2_MATMUL_ALLOC_FAIL;
  }
  cudaMemcpy(d_x,  x,      x_bytes,  cudaMemcpyHostToDevice);
  cudaMemcpy(d_gw, gate_w, w_bytes,  cudaMemcpyHostToDevice);
  cudaMemcpy(d_gs, gate_s, sb_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_gb, gate_b, sb_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_uw, up_w,   w_bytes,  cudaMemcpyHostToDevice);
  cudaMemcpy(d_us, up_s,   sb_bytes, cudaMemcpyHostToDevice);
  cudaMemcpy(d_ub, up_b,   sb_bytes, cudaMemcpyHostToDevice);

  dim3 block(16, 16);
  dim3 grid((unsigned)((out_features + 15) / 16), (unsigned)((S + 15) / 16));
  gate_up_silu_mlx2_kernel<<<grid, block>>>(
    d_x, d_gw, d_gs, d_gb, d_uw, d_us, d_ub, d_h,
    S, in_features, out_features, group_size, packed_cols, groups_per_row);

  mlx2_matmul_status st = MLX2_MATMUL_OK;
  if ( cudaGetLastError() != cudaSuccess ||
       cudaDeviceSynchronize() != cudaSuccess ||
       cudaMemcpy(hid, d_h, h_bytes, cudaMemcpyDeviceToHost) != cudaSuccess ) {
    st = MLX2_MATMUL_LAUNCH_FAIL;
  }
  cudaFree(d_x);  cudaFree(d_gw); cudaFree(d_gs); cudaFree(d_gb);
  cudaFree(d_uw); cudaFree(d_us); cudaFree(d_ub); cudaFree(d_h);
  return st;
}

