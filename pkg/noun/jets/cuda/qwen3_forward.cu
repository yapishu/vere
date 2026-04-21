/*
 * qwen3_forward.cu — multi-block Qwen3 forward.
 *
 * Runs all n_blocks transformer blocks back-to-back on the GPU.  The
 * activation x stays resident in VRAM between blocks via a ping-pong
 * buffer pair; scratch for intermediates (q, k, v, attn, gate, up,
 * hid, mlp, …) is allocated once and reused across every block.
 *
 * Semantically identical to 28 sequential qw3_block_fp32 calls — same
 * kernels, same byte-exact math — but with one HtoD (input embedding),
 * one DtoH (final x), and one cudaDeviceSynchronize.  The per-block
 * qw3_block_fp32 path is kept for the per-op jet fallback and for
 * smaller forward streams (e.g. per-tick block-streaming).
 */

#include "qwen3_block.h"

#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <math.h>
#include <string.h>

extern __global__ void
mlx2_matmul_kernel(const float*, const uint32_t*, const float*, const float*,
                   float*, size_t S, size_t in_features, size_t out_features,
                   size_t group_size, size_t packed_cols, size_t groups_per_row);

extern __global__ void
narrow_fp32_to_fp16_kernel(const float*, __half*, size_t n);

extern __global__ void
rms_norm_kernel(const float*, const float*, float eps, float*, size_t S, size_t D);

extern __global__ void
rope_apply_kernel(const float*, const float*, const float*, float*,
                  size_t S, size_t H, size_t Dh, size_t half);

extern __global__ void
silu_mul_kernel(const float*, const float*, float*, size_t N);

extern __global__ void
gate_up_silu_mlx2_kernel(const float*, const uint32_t*, const float*, const float*,
                         const uint32_t*, const float*, const float*,
                         float*, size_t S, size_t in_features, size_t out_features,
                         size_t group_size, size_t packed_cols, size_t groups_per_row);

extern __global__ void
gqa_attention_kernel(const float*, const float*, const float*, float*,
                     size_t S, size_t H, size_t KH, size_t Dh,
                     float inv_sqrt_dh, size_t group);

/* Elementwise add already defined in qwen3_block.cu; we reuse it. */
extern __global__ void
qw3_add_kernel(const float*, const float*, float*, size_t N);

static int
_qw3_fwd_init_once(void)
{
  static int done = 0;
  if ( done ) return 1;
  int dev = 0;
  if ( cudaGetDeviceCount(&dev) != cudaSuccess || dev == 0 ) return 0;
  if ( cudaSetDevice(0) != cudaSuccess ) return 0;
  done = 1;
  return 1;
}

#define ALLOC(p, n) do { \
    if ( cudaMallocAsync((void**)&(p), (n), 0) != cudaSuccess ) goto fail; \
  } while (0)

#define LAUNCH_CHECK() do { \
    if ( cudaGetLastError() != cudaSuccess ) goto fail; \
  } while (0)

extern "C" qw3_block_status
qw3_forward_fp32(const float* x_host,
                 float*       y_host,
                 const qw3_block_dptrs* blocks,
                 size_t       n_blocks,
                 uintptr_t    cos_dptr,
                 uintptr_t    sin_dptr,
                 size_t S, size_t D, size_t D_ff,
                 size_t H, size_t KH, size_t Dh,
                 size_t group_size,
                 float  rms_eps,
                 const uintptr_t* out_k_dptrs,
                 const uintptr_t* out_v_dptrs)
{
  if ( !x_host || !y_host || !blocks || n_blocks == 0 ||
       cos_dptr == 0 || sin_dptr == 0 ||
       S == 0 || D == 0 || D_ff == 0 || H == 0 || KH == 0 || Dh == 0 ||
       group_size == 0 || (H % KH) != 0 || H * Dh != D ) {
    return QW3_INVALID_ARG;
  }
  if ( !_qw3_fwd_init_once() ) return QW3_NO_CUDA;

  size_t packed_cols_D     = D    / 16;
  size_t packed_cols_D_ff  = D_ff / 16;
  size_t groups_per_row_D  = D    / group_size;
  size_t KV_D              = KH * Dh;

  size_t x_bytes   = S * D    * sizeof(float);
  size_t kv_bytes  = S * KV_D * sizeof(float);
  size_t ff_bytes  = S * D_ff * sizeof(float);
  size_t gqa_shared = 2 * S * sizeof(float);

  dim3 mm_block(16, 16);
  dim3 mm_grid_q ((unsigned)((D    + 15) / 16), (unsigned)((S + 15) / 16));
  dim3 mm_grid_kv((unsigned)((KV_D + 15) / 16), (unsigned)((S + 15) / 16));
  dim3 mm_grid_ff((unsigned)((D_ff + 15) / 16), (unsigned)((S + 15) / 16));
  dim3 rope_block(64, 4);
  dim3 rope_grid_q((unsigned)((Dh + 63) / 64), (unsigned)((H  + 3) / 4), (unsigned)S);
  dim3 rope_grid_k((unsigned)((Dh + 63) / 64), (unsigned)((KH + 3) / 4), (unsigned)S);
  dim3 gqa_grid((unsigned)H, (unsigned)S);
  size_t half        = Dh / 2;
  size_t group       = H / KH;
  float  inv_sqrt_dh = 1.0f / sqrtf((float)Dh);

  const float *d_cos = (const float*)(void*)cos_dptr;
  const float *d_sin = (const float*)(void*)sin_dptr;

  /* Ping-pong x buffers + one-shot scratch. */
  float *d_x_a = NULL, *d_x_b = NULL;
  float *d_x1  = NULL;
  float *d_q   = NULL, *d_q2  = NULL;
  float *d_k   = NULL, *d_k2  = NULL;
  float *d_v   = NULL;
  float *d_attn = NULL;
  float *d_o    = NULL;
  float *d_xr1  = NULL;
  float *d_x2   = NULL;
  float *d_gate = NULL;
  float *d_up   = NULL;
  float *d_hid  = NULL;
  float *d_mlp  = NULL;
  /* Hoisted above any ALLOC macro (C++ forbids goto crossing inits). */
  float* d_x_in  = NULL;
  float* d_x_out = NULL;

  ALLOC(d_x_a,  x_bytes);
  ALLOC(d_x_b,  x_bytes);
  ALLOC(d_x1,   x_bytes);
  ALLOC(d_q,    x_bytes);
  ALLOC(d_q2,   x_bytes);
  ALLOC(d_k,    kv_bytes);
  ALLOC(d_k2,   kv_bytes);
  ALLOC(d_v,    kv_bytes);
  ALLOC(d_attn, x_bytes);
  ALLOC(d_o,    x_bytes);
  ALLOC(d_xr1,  x_bytes);
  ALLOC(d_x2,   x_bytes);
  ALLOC(d_gate, ff_bytes);
  ALLOC(d_up,   ff_bytes);
  ALLOC(d_hid,  ff_bytes);
  ALLOC(d_mlp,  x_bytes);

  /* Single HtoD of the input embedding. */
  if ( cudaMemcpyAsync(d_x_a, x_host, x_bytes, cudaMemcpyHostToDevice, 0)
       != cudaSuccess ) goto fail;

  /* Ping-pong: block i reads d_x_in and writes d_x_out. */
  d_x_in  = d_x_a;
  d_x_out = d_x_b;

  for ( size_t i = 0; i < n_blocks; i++ ) {
    const qw3_block_dptrs* bw = &blocks[i];

    rms_norm_kernel<<<(unsigned)S, 256>>>(
      d_x_in, (const float*)(void*)bw->input_ln, rms_eps, d_x1, S, D);
    LAUNCH_CHECK();

    mlx2_matmul_kernel<<<mm_grid_q, mm_block>>>(
      d_x1, (const uint32_t*)(void*)bw->qw, (const float*)(void*)bw->qs,
      (const float*)(void*)bw->qb, d_q, S, D, D, group_size,
      packed_cols_D, groups_per_row_D);
    LAUNCH_CHECK();
    mlx2_matmul_kernel<<<mm_grid_kv, mm_block>>>(
      d_x1, (const uint32_t*)(void*)bw->kw, (const float*)(void*)bw->ks,
      (const float*)(void*)bw->kb, d_k, S, D, KV_D, group_size,
      packed_cols_D, (size_t)(D / group_size));
    LAUNCH_CHECK();
    mlx2_matmul_kernel<<<mm_grid_kv, mm_block>>>(
      d_x1, (const uint32_t*)(void*)bw->vw, (const float*)(void*)bw->vs,
      (const float*)(void*)bw->vb, d_v, S, D, KV_D, group_size,
      packed_cols_D, (size_t)(D / group_size));
    LAUNCH_CHECK();

    rms_norm_kernel<<<(unsigned)(S * H),  256>>>(
      d_q, (const float*)(void*)bw->q_norm, rms_eps, d_q, S * H,  Dh);
    LAUNCH_CHECK();
    rms_norm_kernel<<<(unsigned)(S * KH), 256>>>(
      d_k, (const float*)(void*)bw->k_norm, rms_eps, d_k, S * KH, Dh);
    LAUNCH_CHECK();

    rope_apply_kernel<<<rope_grid_q, rope_block>>>(d_q, d_cos, d_sin, d_q2, S, H,  Dh, half);
    LAUNCH_CHECK();
    rope_apply_kernel<<<rope_grid_k, rope_block>>>(d_k, d_cos, d_sin, d_k2, S, KH, Dh, half);
    LAUNCH_CHECK();

    /* Optionally emit K (post-RoPE) and V as fp16 to caller-provided
     * VRAM dptrs so decode steps can reuse this prefill's KV.  Cache is
     * fp16 to halve decode-side attention bandwidth; the narrow is
     * per-element and deterministic. */
    if ( out_k_dptrs && out_k_dptrs[i] ) {
      size_t n = S * KV_D;
      size_t t = 256, g = (n + t - 1) / t;
      narrow_fp32_to_fp16_kernel<<<(unsigned)g, (unsigned)t>>>(
        d_k2, (__half*)(void*)out_k_dptrs[i], n);
      LAUNCH_CHECK();
    }
    if ( out_v_dptrs && out_v_dptrs[i] ) {
      size_t n = S * KV_D;
      size_t t = 256, g = (n + t - 1) / t;
      narrow_fp32_to_fp16_kernel<<<(unsigned)g, (unsigned)t>>>(
        d_v, (__half*)(void*)out_v_dptrs[i], n);
      LAUNCH_CHECK();
    }

    gqa_attention_kernel<<<gqa_grid, 128, gqa_shared>>>(
      d_q2, d_k2, d_v, d_attn, S, H, KH, Dh, inv_sqrt_dh, group);
    LAUNCH_CHECK();

    mlx2_matmul_kernel<<<mm_grid_q, mm_block>>>(
      d_attn, (const uint32_t*)(void*)bw->ow, (const float*)(void*)bw->os,
      (const float*)(void*)bw->ob, d_o, S, D, D, group_size,
      packed_cols_D, groups_per_row_D);
    LAUNCH_CHECK();

    {
      size_t N = S * D;
      size_t t = 256, g = (N + t - 1) / t;
      qw3_add_kernel<<<(unsigned)g, (unsigned)t>>>(d_x_in, d_o, d_xr1, N);
      LAUNCH_CHECK();
    }

    rms_norm_kernel<<<(unsigned)S, 256>>>(
      d_xr1, (const float*)(void*)bw->post_ln, rms_eps, d_x2, S, D);
    LAUNCH_CHECK();

    /* Fused gate + up + silu*mul: replaces 2 matmul launches + 1
     * silu_mul launch with one kernel that writes hid directly.
     * Byte-exact: see fused_gate_up_silu_test. */
    gate_up_silu_mlx2_kernel<<<mm_grid_ff, mm_block>>>(
      d_x2,
      (const uint32_t*)(void*)bw->gate_w, (const float*)(void*)bw->gate_s,
      (const float*)(void*)bw->gate_b,
      (const uint32_t*)(void*)bw->up_w,   (const float*)(void*)bw->up_s,
      (const float*)(void*)bw->up_b,
      d_hid, S, D, D_ff, group_size,
      packed_cols_D, (size_t)(D / group_size));
    LAUNCH_CHECK();
    (void)d_gate; (void)d_up;  /* retained for the scratch alloc; unused in fused path */

    mlx2_matmul_kernel<<<mm_grid_q, mm_block>>>(
      d_hid, (const uint32_t*)(void*)bw->down_w, (const float*)(void*)bw->down_s,
      (const float*)(void*)bw->down_b, d_mlp, S, D_ff, D, group_size,
      packed_cols_D_ff, (size_t)(D_ff / group_size));
    LAUNCH_CHECK();

    {
      size_t N = S * D;
      size_t t = 256, g = (N + t - 1) / t;
      qw3_add_kernel<<<(unsigned)g, (unsigned)t>>>(d_xr1, d_mlp, d_x_out, N);
      LAUNCH_CHECK();
    }

    /* ping-pong */
    float* tmp = d_x_in;
    d_x_in  = d_x_out;
    d_x_out = tmp;
  }

  /* After n_blocks iterations, the final output lives in d_x_in (we
   * swapped at the end of each iteration — last output was written
   * to d_x_out, then we renamed d_x_in ← d_x_out). */
  if ( cudaDeviceSynchronize() != cudaSuccess ) goto fail;
  if ( cudaMemcpy(y_host, d_x_in, x_bytes, cudaMemcpyDeviceToHost)
       != cudaSuccess ) goto fail;

  cudaFreeAsync(d_x_a, 0);  cudaFreeAsync(d_x_b, 0);
  cudaFreeAsync(d_x1,  0);
  cudaFreeAsync(d_q,   0);  cudaFreeAsync(d_q2,  0);
  cudaFreeAsync(d_k,   0);  cudaFreeAsync(d_k2,  0);
  cudaFreeAsync(d_v,   0);
  cudaFreeAsync(d_attn,0);  cudaFreeAsync(d_o,   0);
  cudaFreeAsync(d_xr1, 0);  cudaFreeAsync(d_x2,  0);
  cudaFreeAsync(d_gate,0);  cudaFreeAsync(d_up,  0);
  cudaFreeAsync(d_hid, 0);  cudaFreeAsync(d_mlp, 0);
  return QW3_OK;

fail:
  if ( d_x_a ) cudaFreeAsync(d_x_a, 0);
  if ( d_x_b ) cudaFreeAsync(d_x_b, 0);
  if ( d_x1 )  cudaFreeAsync(d_x1,  0);
  if ( d_q )   cudaFreeAsync(d_q,   0);
  if ( d_q2 )  cudaFreeAsync(d_q2,  0);
  if ( d_k )   cudaFreeAsync(d_k,   0);
  if ( d_k2 )  cudaFreeAsync(d_k2,  0);
  if ( d_v )   cudaFreeAsync(d_v,   0);
  if ( d_attn )cudaFreeAsync(d_attn,0);
  if ( d_o )   cudaFreeAsync(d_o,   0);
  if ( d_xr1 ) cudaFreeAsync(d_xr1, 0);
  if ( d_x2 )  cudaFreeAsync(d_x2,  0);
  if ( d_gate )cudaFreeAsync(d_gate,0);
  if ( d_up )  cudaFreeAsync(d_up,  0);
  if ( d_hid ) cudaFreeAsync(d_hid, 0);
  if ( d_mlp ) cudaFreeAsync(d_mlp, 0);
  return QW3_LAUNCH_FAIL;
}
