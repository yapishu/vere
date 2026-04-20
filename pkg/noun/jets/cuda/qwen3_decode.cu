/*
 * qwen3_decode.cu — single-token Qwen3 decode step with KV cache.
 *
 * One new token at `position` (seq_len = position+1), using cached K/V
 * from the previous step for each layer.  The per-layer flow:
 *
 *   x₁ = rmsnorm(x, input_ln)
 *   q,k_new,v_new = mlx2_matmul of [1, D]  →  [1, H*Dh] / [1, KV_D]
 *   q,k_new = per-head rmsnorm
 *   q,k_new = rope at single row cos/sin[position]
 *   curr_k = prev_k(N-1 positions) ‖ k_new       (in a freshly-alloc'd
 *   curr_v = prev_v(N-1 positions) ‖ v_new        [N, KV_D] VRAM tensor
 *                                                 already owned by caller)
 *   attn = decode-attention(q[1,H,Dh], curr_k[N,KH,Dh], curr_v[N,KH,Dh])
 *   x = x + o_proj(attn)
 *   x₂ = rmsnorm(x, post_ln)
 *   mlp = down(silu(gate(x₂)) * up(x₂))
 *   x = x + mlp
 *
 * Output: new activation at position N-1, shape [1, D].
 *
 * Same kernels + same byte-exact math as prefill, so results match
 * bit-for-bit with a full re-forward over the extended sequence.
 */

#include "qwen3_block.h"

#include <cuda_runtime.h>
#include <math.h>

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
qw3_add_kernel(const float*, const float*, float*, size_t N);

extern __global__ void
gqa_attention_decode_kernel(const float*, const float*, const float*, float*,
                            size_t N, size_t H, size_t KH, size_t Dh,
                            float inv_sqrt_dh, size_t group);

static int
_qw3_dec_init_once(void)
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
qw3_decode_fp32(const float* x_host,
                float*       y_host,
                const qw3_block_dptrs* blocks,
                size_t       n_blocks,
                uintptr_t    cos_dptr,
                uintptr_t    sin_dptr,
                size_t       position,
                const uintptr_t* kv_k_prev_dptrs,
                const uintptr_t* kv_v_prev_dptrs,
                const uintptr_t* kv_k_curr_dptrs,
                const uintptr_t* kv_v_curr_dptrs,
                size_t D, size_t D_ff,
                size_t H, size_t KH, size_t Dh,
                size_t group_size,
                float  rms_eps)
{
  if ( !x_host || !y_host || !blocks || n_blocks == 0 ||
       cos_dptr == 0 || sin_dptr == 0 ||
       !kv_k_prev_dptrs || !kv_v_prev_dptrs ||
       !kv_k_curr_dptrs || !kv_v_curr_dptrs ||
       D == 0 || D_ff == 0 || H == 0 || KH == 0 || Dh == 0 ||
       group_size == 0 || (H % KH) != 0 || H * Dh != D ) {
    return QW3_INVALID_ARG;
  }
  if ( !_qw3_dec_init_once() ) return QW3_NO_CUDA;

  size_t S = 1;  /* decode is always one new token */
  size_t N = position + 1;  /* total sequence length including new token */
  size_t KV_D = KH * Dh;

  size_t packed_cols_D    = D    / 16;
  size_t packed_cols_D_ff = D_ff / 16;
  size_t groups_per_row_D = D    / group_size;

  size_t x_bytes    = S * D      * sizeof(float);
  size_t kv_row_bts = S * KV_D   * sizeof(float);  /* bytes for 1 position of K/V */
  size_t ff_bytes   = S * D_ff   * sizeof(float);
  size_t attn_shared = 2 * N * sizeof(float);

  dim3 mm_block(16, 16);
  dim3 mm_grid_q ((unsigned)((D    + 15) / 16), 1u);
  dim3 mm_grid_kv((unsigned)((KV_D + 15) / 16), 1u);
  dim3 mm_grid_ff((unsigned)((D_ff + 15) / 16), 1u);
  dim3 rope_block(64, 4);
  dim3 rope_grid_q((unsigned)((Dh + 63) / 64), (unsigned)((H  + 3) / 4), 1u);
  dim3 rope_grid_k((unsigned)((Dh + 63) / 64), (unsigned)((KH + 3) / 4), 1u);
  dim3 gqa_grid((unsigned)H, 1u);
  size_t half        = Dh / 2;
  size_t group       = H / KH;
  float  inv_sqrt_dh = 1.0f / sqrtf((float)Dh);

  /* Slice cos/sin to just the new position's row — [1, Dh]. */
  const float* d_cos_pos =
    (const float*)(void*)(cos_dptr + position * Dh * sizeof(float));
  const float* d_sin_pos =
    (const float*)(void*)(sin_dptr + position * Dh * sizeof(float));

  /* Scratch: all shapes have S=1.  Q / K_new / V_new / attn and
   * intermediate activations, plus FFN workspace. */
  float *d_x_in  = NULL, *d_x1 = NULL;
  float *d_q = NULL, *d_q2 = NULL;
  float *d_k_new = NULL, *d_k2_new = NULL;
  float *d_v_new = NULL;
  float *d_attn = NULL;
  float *d_o = NULL;
  float *d_xr1 = NULL;
  float *d_x2 = NULL;
  float *d_gate = NULL, *d_up = NULL, *d_hid = NULL;
  float *d_mlp = NULL, *d_y = NULL;

  ALLOC(d_x_in,   x_bytes);
  ALLOC(d_x1,     x_bytes);
  ALLOC(d_q,      x_bytes);
  ALLOC(d_q2,     x_bytes);
  ALLOC(d_k_new,  kv_row_bts);
  ALLOC(d_k2_new, kv_row_bts);
  ALLOC(d_v_new,  kv_row_bts);
  ALLOC(d_attn,   x_bytes);
  ALLOC(d_o,      x_bytes);
  ALLOC(d_xr1,    x_bytes);
  ALLOC(d_x2,     x_bytes);
  ALLOC(d_gate,   ff_bytes);
  ALLOC(d_up,     ff_bytes);
  ALLOC(d_hid,    ff_bytes);
  ALLOC(d_mlp,    x_bytes);
  ALLOC(d_y,      x_bytes);

  if ( cudaMemcpyAsync(d_x_in, x_host, x_bytes, cudaMemcpyHostToDevice, 0)
       != cudaSuccess ) goto fail;

  for ( size_t i = 0; i < n_blocks; i++ ) {
    const qw3_block_dptrs* bw = &blocks[i];
    uintptr_t prev_k = kv_k_prev_dptrs[i];
    uintptr_t prev_v = kv_v_prev_dptrs[i];
    uintptr_t curr_k = kv_k_curr_dptrs[i];
    uintptr_t curr_v = kv_v_curr_dptrs[i];
    if ( curr_k == 0 || curr_v == 0 ) goto fail;

    /* Copy the previous (N-1) positions' K/V into the freshly-allocated
     * curr buffers.  If prev_k/prev_v is 0 we'd be at position 0 with no
     * prior — but that case is a prefill, handled separately. */
    if ( position > 0 ) {
      if ( prev_k == 0 || prev_v == 0 ) goto fail;
      size_t prev_bytes = position * KV_D * sizeof(float);
      if ( cudaMemcpyAsync((void*)curr_k, (const void*)prev_k, prev_bytes,
                           cudaMemcpyDeviceToDevice, 0) != cudaSuccess )
        goto fail;
      if ( cudaMemcpyAsync((void*)curr_v, (const void*)prev_v, prev_bytes,
                           cudaMemcpyDeviceToDevice, 0) != cudaSuccess )
        goto fail;
    }

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
      (const float*)(void*)bw->kb, d_k_new, S, D, KV_D, group_size,
      packed_cols_D, (size_t)(D / group_size));
    LAUNCH_CHECK();
    mlx2_matmul_kernel<<<mm_grid_kv, mm_block>>>(
      d_x1, (const uint32_t*)(void*)bw->vw, (const float*)(void*)bw->vs,
      (const float*)(void*)bw->vb, d_v_new, S, D, KV_D, group_size,
      packed_cols_D, (size_t)(D / group_size));
    LAUNCH_CHECK();

    rms_norm_kernel<<<(unsigned)(S * H),  256>>>(
      d_q, (const float*)(void*)bw->q_norm, rms_eps, d_q, S * H,  Dh);
    LAUNCH_CHECK();
    rms_norm_kernel<<<(unsigned)(S * KH), 256>>>(
      d_k_new, (const float*)(void*)bw->k_norm, rms_eps, d_k_new, S * KH, Dh);
    LAUNCH_CHECK();

    rope_apply_kernel<<<rope_grid_q, rope_block>>>(
      d_q, d_cos_pos, d_sin_pos, d_q2, S, H, Dh, half);
    LAUNCH_CHECK();
    rope_apply_kernel<<<rope_grid_k, rope_block>>>(
      d_k_new, d_cos_pos, d_sin_pos, d_k2_new, S, KH, Dh, half);
    LAUNCH_CHECK();

    /* Append new K/V (post-rope for K, plain for V) to the curr cache
     * at slot `position`. */
    {
      void* dst_k = (void*)(curr_k + position * KV_D * sizeof(float));
      if ( cudaMemcpyAsync(dst_k, d_k2_new, kv_row_bts,
                           cudaMemcpyDeviceToDevice, 0) != cudaSuccess )
        goto fail;
      void* dst_v = (void*)(curr_v + position * KV_D * sizeof(float));
      if ( cudaMemcpyAsync(dst_v, d_v_new, kv_row_bts,
                           cudaMemcpyDeviceToDevice, 0) != cudaSuccess )
        goto fail;
    }

    /* Attention: 1 query × N cached K/V. */
    gqa_attention_decode_kernel<<<gqa_grid, 128, attn_shared>>>(
      d_q2, (const float*)(void*)curr_k, (const float*)(void*)curr_v,
      d_attn, N, H, KH, Dh, inv_sqrt_dh, group);
    LAUNCH_CHECK();

    mlx2_matmul_kernel<<<mm_grid_q, mm_block>>>(
      d_attn, (const uint32_t*)(void*)bw->ow, (const float*)(void*)bw->os,
      (const float*)(void*)bw->ob, d_o, S, D, D, group_size,
      packed_cols_D, groups_per_row_D);
    LAUNCH_CHECK();

    {
      size_t Nelem = S * D;
      size_t t = 256, g = (Nelem + t - 1) / t;
      qw3_add_kernel<<<(unsigned)g, (unsigned)t>>>(d_x_in, d_o, d_xr1, Nelem);
      LAUNCH_CHECK();
    }

    rms_norm_kernel<<<(unsigned)S, 256>>>(
      d_xr1, (const float*)(void*)bw->post_ln, rms_eps, d_x2, S, D);
    LAUNCH_CHECK();

    mlx2_matmul_kernel<<<mm_grid_ff, mm_block>>>(
      d_x2, (const uint32_t*)(void*)bw->gate_w, (const float*)(void*)bw->gate_s,
      (const float*)(void*)bw->gate_b, d_gate, S, D, D_ff, group_size,
      packed_cols_D, (size_t)(D / group_size));
    LAUNCH_CHECK();
    mlx2_matmul_kernel<<<mm_grid_ff, mm_block>>>(
      d_x2, (const uint32_t*)(void*)bw->up_w, (const float*)(void*)bw->up_s,
      (const float*)(void*)bw->up_b, d_up, S, D, D_ff, group_size,
      packed_cols_D, (size_t)(D / group_size));
    LAUNCH_CHECK();

    {
      size_t Nelem = S * D_ff;
      size_t t = 256, g = (Nelem + t - 1) / t;
      silu_mul_kernel<<<(unsigned)g, (unsigned)t>>>(d_gate, d_up, d_hid, Nelem);
      LAUNCH_CHECK();
    }

    mlx2_matmul_kernel<<<mm_grid_q, mm_block>>>(
      d_hid, (const uint32_t*)(void*)bw->down_w, (const float*)(void*)bw->down_s,
      (const float*)(void*)bw->down_b, d_mlp, S, D_ff, D, group_size,
      packed_cols_D_ff, (size_t)(D_ff / group_size));
    LAUNCH_CHECK();

    {
      size_t Nelem = S * D;
      size_t t = 256, g = (Nelem + t - 1) / t;
      qw3_add_kernel<<<(unsigned)g, (unsigned)t>>>(d_xr1, d_mlp, d_y, Nelem);
      LAUNCH_CHECK();
    }

    /* d_y becomes input for next layer.  Swap pointers (d_x_in ← d_y
     * conceptually).  We have two buffers to ping-pong: reuse d_x_in
     * as the next layer's x_in by memcpy'ing d_y → d_x_in. */
    if ( cudaMemcpyAsync(d_x_in, d_y, x_bytes,
                         cudaMemcpyDeviceToDevice, 0) != cudaSuccess )
      goto fail;
  }

  if ( cudaDeviceSynchronize() != cudaSuccess ) goto fail;
  if ( cudaMemcpy(y_host, d_x_in, x_bytes, cudaMemcpyDeviceToHost)
       != cudaSuccess ) goto fail;

  cudaFreeAsync(d_x_in, 0);  cudaFreeAsync(d_x1, 0);
  cudaFreeAsync(d_q, 0);     cudaFreeAsync(d_q2, 0);
  cudaFreeAsync(d_k_new, 0); cudaFreeAsync(d_k2_new, 0);
  cudaFreeAsync(d_v_new, 0);
  cudaFreeAsync(d_attn, 0);  cudaFreeAsync(d_o, 0);
  cudaFreeAsync(d_xr1, 0);   cudaFreeAsync(d_x2, 0);
  cudaFreeAsync(d_gate, 0);  cudaFreeAsync(d_up, 0);
  cudaFreeAsync(d_hid, 0);   cudaFreeAsync(d_mlp, 0);
  cudaFreeAsync(d_y, 0);
  return QW3_OK;

fail:
  if ( d_x_in )   cudaFreeAsync(d_x_in, 0);
  if ( d_x1 )     cudaFreeAsync(d_x1, 0);
  if ( d_q )      cudaFreeAsync(d_q, 0);
  if ( d_q2 )     cudaFreeAsync(d_q2, 0);
  if ( d_k_new )  cudaFreeAsync(d_k_new, 0);
  if ( d_k2_new ) cudaFreeAsync(d_k2_new, 0);
  if ( d_v_new )  cudaFreeAsync(d_v_new, 0);
  if ( d_attn )   cudaFreeAsync(d_attn, 0);
  if ( d_o )      cudaFreeAsync(d_o, 0);
  if ( d_xr1 )    cudaFreeAsync(d_xr1, 0);
  if ( d_x2 )     cudaFreeAsync(d_x2, 0);
  if ( d_gate )   cudaFreeAsync(d_gate, 0);
  if ( d_up )     cudaFreeAsync(d_up, 0);
  if ( d_hid )    cudaFreeAsync(d_hid, 0);
  if ( d_mlp )    cudaFreeAsync(d_mlp, 0);
  if ( d_y )      cudaFreeAsync(d_y, 0);
  return QW3_LAUNCH_FAIL;
}
