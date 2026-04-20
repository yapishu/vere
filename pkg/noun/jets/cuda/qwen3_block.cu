/*
 * qwen3_block.cu — one-shot per-block Qwen3 forward.
 *
 * Chains all the existing __global__ kernels (rms_norm, mlx2_matmul,
 * rope_apply, gqa_attention, silu_mul) with a tiny elementwise add,
 * keeping every intermediate tensor in VRAM.  Single HtoD (the input
 * x + norm gammas + cos/sin) and single DtoH (the final x).
 *
 * Each sub-step is byte-exact against the per-op jet path; the only
 * difference is that intermediates no longer round-trip to host.
 */

#include "qwen3_block.h"
#include "expf_hoon.cuh"

#include <cuda_runtime.h>
#include <math.h>
#include <string.h>

/* Forward-declare the externally-linked kernels from the per-op files. */
extern __global__ void
sgemm_det_kernel(const float*, const float*, float*, size_t M, size_t K, size_t N);

extern __global__ void
mlx2_matmul_kernel(const float*, const uint32_t*, const float*, const float*,
                   float*, size_t S, size_t in_features, size_t out_features,
                   size_t group_size, size_t packed_cols, size_t groups_per_row);

extern __global__ void
rms_norm_kernel(const float*, const float*, float eps, float*, size_t S, size_t D);

extern __global__ void
rope_apply_kernel(const float*, const float*, const float*, float*,
                  size_t S, size_t H, size_t Dh, size_t half);

extern __global__ void
silu_mul_kernel(const float*, const float*, float*, size_t N);

extern __global__ void
gqa_attention_kernel(const float*, const float*, const float*, float*,
                     size_t S, size_t H, size_t KH, size_t Dh,
                     float inv_sqrt_dh, size_t group);

/* Simple elementwise add for residual connections.  Matches the
 * jetted +add-rays path; one IEEE add per element. */
__global__ void
qw3_add_kernel(const float* __restrict__ a,
               const float* __restrict__ b,
               float*       __restrict__ y,
               size_t N)
{
  size_t i = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
  if ( i < N ) y[i] = a[i] + b[i];
}

static int
_qw3_init_once(void)
{
  static int done = 0;
  if ( done ) return 1;
  int dev = 0;
  if ( cudaGetDeviceCount(&dev) != cudaSuccess || dev == 0 ) return 0;
  if ( cudaSetDevice(0) != cudaSuccess ) return 0;
  done = 1;
  return 1;
}

/* Allocation + error helpers to keep the main path readable. */
#define ALLOC(p, n) do { \
    if ( cudaMallocAsync((void**)&(p), (n), 0) != cudaSuccess ) goto fail; \
  } while (0)

#define LAUNCH_CHECK() do { \
    if ( cudaGetLastError() != cudaSuccess ) goto fail; \
  } while (0)

extern "C" qw3_block_status
qw3_block_fp32(const float* x_host,
               float*       y_host,
               uintptr_t q_w, uintptr_t q_s, uintptr_t q_b,
               uintptr_t k_w, uintptr_t k_s, uintptr_t k_b,
               uintptr_t v_w, uintptr_t v_s, uintptr_t v_b,
               uintptr_t o_w, uintptr_t o_s, uintptr_t o_b,
               uintptr_t gate_w, uintptr_t gate_s, uintptr_t gate_b,
               uintptr_t up_w,   uintptr_t up_s,   uintptr_t up_b,
               uintptr_t down_w, uintptr_t down_s, uintptr_t down_b,
               uintptr_t input_ln_dptr,
               uintptr_t post_ln_dptr,
               uintptr_t q_norm_dptr,
               uintptr_t k_norm_dptr,
               uintptr_t cos_dptr,
               uintptr_t sin_dptr,
               size_t S, size_t D, size_t D_ff,
               size_t H, size_t KH, size_t Dh,
               size_t group_size,
               float  rms_eps)
{
  if ( !x_host || !y_host ||
       input_ln_dptr == 0 || post_ln_dptr == 0 ||
       q_norm_dptr == 0 || k_norm_dptr == 0 ||
       cos_dptr == 0 || sin_dptr == 0 ||
       q_w == 0 || k_w == 0 || v_w == 0 || o_w == 0 ||
       gate_w == 0 || up_w == 0 || down_w == 0 ||
       S == 0 || D == 0 || D_ff == 0 || H == 0 || KH == 0 || Dh == 0 ||
       group_size == 0 || (H % KH) != 0 ||
       H * Dh != D ) {
    return QW3_INVALID_ARG;
  }
  if ( !_qw3_init_once() ) return QW3_NO_CUDA;

  /* Derived sizes — all byte counts. */
  size_t packed_cols_D     = D     / 16;
  size_t packed_cols_D_ff  = D_ff  / 16;
  size_t groups_per_row_D    = D     / group_size;
  size_t groups_per_row_D_ff = D_ff  / group_size;
  size_t KV_D = KH * Dh;

  size_t x_bytes     = S * D * sizeof(float);
  size_t kv_bytes    = S * KV_D * sizeof(float);
  size_t ff_bytes    = S * D_ff * sizeof(float);
  size_t gqa_shared  = 2 * S * sizeof(float);

  /* Hoist all non-pointer scalars/dim3s up front so the C++ compiler
   * (nvcc front-end) doesn't complain about goto-crossing-initialization
   * in the ALLOC macros below. */
  dim3 mm_block(16, 16);
  dim3 mm_grid_q ((unsigned)((D    + 15) / 16), (unsigned)((S + 15) / 16));
  dim3 mm_grid_kv((unsigned)((KV_D + 15) / 16), (unsigned)((S + 15) / 16));
  dim3 mm_grid_ff((unsigned)((D_ff + 15) / 16), (unsigned)((S + 15) / 16));
  dim3 rope_block(64, 4);
  dim3 rope_grid_q((unsigned)((Dh + 63) / 64), (unsigned)((H  + 3) / 4), (unsigned)S);
  dim3 rope_grid_k((unsigned)((Dh + 63) / 64), (unsigned)((KH + 3) / 4), (unsigned)S);
  dim3 gqa_grid((unsigned)H, (unsigned)S);
  size_t half = Dh / 2;
  size_t group = H / KH;
  float inv_sqrt_dh = 1.0f / sqrtf((float)Dh);

  /* VRAM scratch buffers.  Each intermediate gets its own so we can
   * treat each kernel as having disjoint input/output (avoids race
   * hazards for rope / attention / matmul in-place writes). */
  float *d_x_in  = NULL, *d_x1 = NULL;
  float *d_q    = NULL, *d_q2 = NULL;
  float *d_k    = NULL, *d_k2 = NULL;
  float *d_v    = NULL;
  float *d_attn = NULL;
  float *d_o    = NULL;
  float *d_xr1  = NULL;
  float *d_x2   = NULL;
  float *d_gate = NULL;
  float *d_up   = NULL;
  float *d_hid  = NULL;
  float *d_mlp  = NULL;
  float *d_y    = NULL;
  /* Alias caller-provided VRAM pointers (already cached). */
  const float *d_input_ln = (const float*)(void*)input_ln_dptr;
  const float *d_post_ln  = (const float*)(void*)post_ln_dptr;
  const float *d_qn       = (const float*)(void*)q_norm_dptr;
  const float *d_kn       = (const float*)(void*)k_norm_dptr;
  const float *d_cos      = (const float*)(void*)cos_dptr;
  const float *d_sin      = (const float*)(void*)sin_dptr;

  ALLOC(d_x_in,  x_bytes);
  ALLOC(d_x1,    x_bytes);
  ALLOC(d_q,     x_bytes);        /* q_flat: S*H*Dh = S*D */
  ALLOC(d_q2,    x_bytes);
  ALLOC(d_k,     kv_bytes);
  ALLOC(d_k2,    kv_bytes);
  ALLOC(d_v,     kv_bytes);
  ALLOC(d_attn,  x_bytes);        /* [S, H*Dh] = [S, D] */
  ALLOC(d_o,     x_bytes);
  ALLOC(d_xr1,   x_bytes);
  ALLOC(d_x2,    x_bytes);
  ALLOC(d_gate,  ff_bytes);
  ALLOC(d_up,    ff_bytes);
  ALLOC(d_hid,   ff_bytes);
  ALLOC(d_mlp,   x_bytes);
  ALLOC(d_y,     x_bytes);

  /* Only the activation x is still uploaded per-call. */
  cudaMemcpy(d_x_in, x_host, x_bytes, cudaMemcpyHostToDevice);

  /* ---- input_ln ---- */
  rms_norm_kernel<<<(unsigned)S, 256>>>(d_x_in, d_input_ln, rms_eps, d_x1, S, D);
  LAUNCH_CHECK();

  /* ---- q / k / v projections (mlx2_matmul) ---- */
  mlx2_matmul_kernel<<<mm_grid_q, mm_block>>>(
    d_x1, (const uint32_t*)(void*)q_w, (const float*)(void*)q_s,
    (const float*)(void*)q_b, d_q, S, D, D, group_size,
    packed_cols_D, groups_per_row_D);
  LAUNCH_CHECK();

  mlx2_matmul_kernel<<<mm_grid_kv, mm_block>>>(
    d_x1, (const uint32_t*)(void*)k_w, (const float*)(void*)k_s,
    (const float*)(void*)k_b, d_k, S, D, KV_D, group_size,
    packed_cols_D, (size_t)(D / group_size));
  LAUNCH_CHECK();
  mlx2_matmul_kernel<<<mm_grid_kv, mm_block>>>(
    d_x1, (const uint32_t*)(void*)v_w, (const float*)(void*)v_s,
    (const float*)(void*)v_b, d_v, S, D, KV_D, group_size,
    packed_cols_D, (size_t)(D / group_size));
  LAUNCH_CHECK();

  /* ---- per-head rmsnorm on q, k ----
   * View [S, H, Dh] / [S, KH, Dh] as [S*H, Dh] / [S*KH, Dh] for kernel. */
  rms_norm_kernel<<<(unsigned)(S * H),  256>>>(d_q, d_qn, rms_eps, d_q, S * H,  Dh);
  LAUNCH_CHECK();
  rms_norm_kernel<<<(unsigned)(S * KH), 256>>>(d_k, d_kn, rms_eps, d_k, S * KH, Dh);
  LAUNCH_CHECK();

  /* ---- rope (not in-place safe; write to _2) ---- */
  rope_apply_kernel<<<rope_grid_q, rope_block>>>(d_q, d_cos, d_sin, d_q2, S, H, Dh, half);
  LAUNCH_CHECK();
  rope_apply_kernel<<<rope_grid_k, rope_block>>>(d_k, d_cos, d_sin, d_k2, S, KH, Dh, half);
  LAUNCH_CHECK();

  /* ---- gqa-attention: reads q/k/v, writes attn (separate buffer). ---- */
  gqa_attention_kernel<<<gqa_grid, 128, gqa_shared>>>(
    d_q2, d_k2, d_v, d_attn, S, H, KH, Dh, inv_sqrt_dh, group);
  LAUNCH_CHECK();

  /* ---- o projection ---- */
  mlx2_matmul_kernel<<<mm_grid_q, mm_block>>>(
    d_attn, (const uint32_t*)(void*)o_w, (const float*)(void*)o_s,
    (const float*)(void*)o_b, d_o, S, D, D, group_size,
    packed_cols_D, groups_per_row_D);
  LAUNCH_CHECK();

  /* ---- residual: xr1 = x_in + o ---- */
  {
    size_t N = S * D;
    size_t t = 256, g = (N + t - 1) / t;
    qw3_add_kernel<<<(unsigned)g, (unsigned)t>>>(d_x_in, d_o, d_xr1, N);
    LAUNCH_CHECK();
  }

  /* ---- post_ln ---- */
  rms_norm_kernel<<<(unsigned)S, 256>>>(d_xr1, d_post_ln, rms_eps, d_x2, S, D);
  LAUNCH_CHECK();

  /* ---- gate_proj and up_proj (both produce [S, D_ff]) ---- */
  mlx2_matmul_kernel<<<mm_grid_ff, mm_block>>>(
    d_x2, (const uint32_t*)(void*)gate_w, (const float*)(void*)gate_s,
    (const float*)(void*)gate_b, d_gate, S, D, D_ff, group_size,
    packed_cols_D, (size_t)(D / group_size));
  LAUNCH_CHECK();
  mlx2_matmul_kernel<<<mm_grid_ff, mm_block>>>(
    d_x2, (const uint32_t*)(void*)up_w, (const float*)(void*)up_s,
    (const float*)(void*)up_b, d_up, S, D, D_ff, group_size,
    packed_cols_D, (size_t)(D / group_size));
  LAUNCH_CHECK();

  /* ---- silu-mul: hid = silu(gate) * up ---- */
  {
    size_t N = S * D_ff;
    size_t t = 256, g = (N + t - 1) / t;
    silu_mul_kernel<<<(unsigned)g, (unsigned)t>>>(d_gate, d_up, d_hid, N);
    LAUNCH_CHECK();
  }

  /* ---- down_proj: [S, D_ff] -> [S, D] ---- */
  mlx2_matmul_kernel<<<mm_grid_q, mm_block>>>(
    d_hid, (const uint32_t*)(void*)down_w, (const float*)(void*)down_s,
    (const float*)(void*)down_b, d_mlp, S, D_ff, D, group_size,
    packed_cols_D_ff, (size_t)(D_ff / group_size));
  LAUNCH_CHECK();

  /* ---- residual: y = xr1 + mlp ---- */
  {
    size_t N = S * D;
    size_t t = 256, g = (N + t - 1) / t;
    qw3_add_kernel<<<(unsigned)g, (unsigned)t>>>(d_xr1, d_mlp, d_y, N);
    LAUNCH_CHECK();
  }

  /* Sync + download final x. */
  if ( cudaDeviceSynchronize() != cudaSuccess ) goto fail;
  if ( cudaMemcpy(y_host, d_y, x_bytes, cudaMemcpyDeviceToHost) != cudaSuccess )
    goto fail;

  /* Free scratch (weights remain in the VRAM cache, not freed here). */
  cudaFreeAsync(d_x_in, 0);  cudaFreeAsync(d_x1, 0);
  cudaFreeAsync(d_q, 0);     cudaFreeAsync(d_q2, 0);
  cudaFreeAsync(d_k, 0);     cudaFreeAsync(d_k2, 0);
  cudaFreeAsync(d_v, 0);
  cudaFreeAsync(d_attn, 0);  cudaFreeAsync(d_o, 0);
  cudaFreeAsync(d_xr1, 0);   cudaFreeAsync(d_x2, 0);
  cudaFreeAsync(d_gate, 0);  cudaFreeAsync(d_up, 0);
  cudaFreeAsync(d_hid, 0);   cudaFreeAsync(d_mlp, 0);
  cudaFreeAsync(d_y, 0);
  /* d_input_ln / d_post_ln / d_qn / d_kn / d_cos / d_sin are
   * vram_cache-owned; we do not free them. */
  return QW3_OK;

fail:
  /* Best-effort cleanup.  Async frees on NULL are safe. */
  if ( d_x_in ) cudaFreeAsync(d_x_in, 0);
  if ( d_x1 )   cudaFreeAsync(d_x1, 0);
  if ( d_q )    cudaFreeAsync(d_q, 0);
  if ( d_q2 )   cudaFreeAsync(d_q2, 0);
  if ( d_k )    cudaFreeAsync(d_k, 0);
  if ( d_k2 )   cudaFreeAsync(d_k2, 0);
  if ( d_v )    cudaFreeAsync(d_v, 0);
  if ( d_attn ) cudaFreeAsync(d_attn, 0);
  if ( d_o )    cudaFreeAsync(d_o, 0);
  if ( d_xr1 )  cudaFreeAsync(d_xr1, 0);
  if ( d_x2 )   cudaFreeAsync(d_x2, 0);
  if ( d_gate ) cudaFreeAsync(d_gate, 0);
  if ( d_up )   cudaFreeAsync(d_up, 0);
  if ( d_hid )  cudaFreeAsync(d_hid, 0);
  if ( d_mlp )  cudaFreeAsync(d_mlp, 0);
  if ( d_y )    cudaFreeAsync(d_y, 0);
  return QW3_LAUNCH_FAIL;
}
