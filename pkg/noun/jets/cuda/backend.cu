/*
 * backend.cu — glue between backend.h (plain C API) and the CUDA kernels.
 */

#include "backend.h"
#include "sgemm_det.h"
#include "mlx2_matmul.h"
#include "rms_norm.h"
#include "rope_apply.h"
#include "silu_mul.h"
#include "gqa_attention.h"
#include "qwen3_block.h"
#include "vram_cache.h"

#include <cuda_runtime.h>
#include <stdio.h>

static int g_backend_init = 0;
static int g_backend_ok   = 0;

extern "C" backend_status
backend_init(void)
{
  if ( g_backend_init ) {
    return g_backend_ok ? BACKEND_OK : BACKEND_NO_CUDA;
  }
  g_backend_init = 1;

  int dev_count = 0;
  cudaError_t err = cudaGetDeviceCount(&dev_count);
  if ( err != cudaSuccess || dev_count == 0 ) {
    return BACKEND_NO_CUDA;
  }

  /* Note: we skip cudaGetDeviceProperties to avoid the _v2 symbol versioning
   * issue when linking against musl.  CC ≥ 2.0 is required for IEEE-754
   * fp32 compliance; any CUDA-capable device from 2010+ satisfies this, and
   * a non-compliant device would fail the kernel launch anyway, triggering
   * the CPU fallback. */

  if ( cudaSetDevice(0) != cudaSuccess ) {
    return BACKEND_NO_CUDA;
  }
  if ( vram_cache_init(0) != VRAM_CACHE_OK ) {
    return BACKEND_NO_CUDA;
  }
  if ( sgemm_det_init() != SGEMM_DET_OK ) {
    return BACKEND_NO_CUDA;
  }

  g_backend_ok = 1;
  return BACKEND_OK;
}

extern "C" int
backend_available(void)
{
  if ( !g_backend_init ) backend_init();
  return g_backend_ok;
}

extern "C" backend_status
backend_mmul_fp32(const void* a_bytes,
                  const void* b_bytes,
                  void*       c_bytes,
                  size_t      M,
                  size_t      K,
                  size_t      N,
                  uint32_t    b_hash)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;

  if ( b_hash != 0 ) {
    uintptr_t b_dptr = 0;
    vram_cache_status s =
      vram_cache_get_or_upload(b_bytes, K * N * sizeof(float), b_hash, &b_dptr);
    if ( s != VRAM_CACHE_OK ) return BACKEND_ALLOC_FAIL;
    sgemm_det_status r = sgemm_det_row_major_cached_b(
      (const float*)a_bytes, b_dptr, (float*)c_bytes, M, K, N);
    return r == SGEMM_DET_OK ? BACKEND_OK : BACKEND_LAUNCH_FAIL;
  }
  sgemm_det_status r = sgemm_det_row_major(
    (const float*)a_bytes, (const float*)b_bytes, (float*)c_bytes, M, K, N);
  return r == SGEMM_DET_OK ? BACKEND_OK : BACKEND_LAUNCH_FAIL;
}

extern "C" backend_status
backend_mmul_mlx2(const void* x_bytes,
                  const void* w_packed_bytes,
                  const void* scales_bytes,
                  const void* biases_bytes,
                  void*       y_bytes,
                  size_t      S,
                  size_t      in_features,
                  size_t      out_features,
                  size_t      group_size,
                  uint32_t    w_hash,
                  uint32_t    s_hash,
                  uint32_t    b_hash)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;

  size_t packed_cols    = in_features / 16;
  size_t groups_per_row = in_features / group_size;
  size_t w_bytes = out_features * packed_cols * sizeof(uint32_t);
  size_t sb_bytes = out_features * groups_per_row * sizeof(float);

  uintptr_t dW = 0, dS = 0, dB = 0;
  if ( w_hash && vram_cache_get_or_upload(w_packed_bytes, w_bytes, w_hash, &dW) != VRAM_CACHE_OK )
    return BACKEND_ALLOC_FAIL;
  if ( s_hash && vram_cache_get_or_upload(scales_bytes,   sb_bytes, s_hash, &dS) != VRAM_CACHE_OK )
    return BACKEND_ALLOC_FAIL;
  if ( b_hash && vram_cache_get_or_upload(biases_bytes,   sb_bytes, b_hash, &dB) != VRAM_CACHE_OK )
    return BACKEND_ALLOC_FAIL;

  if ( dW && dS && dB ) {
    mlx2_matmul_status r = mlx2_matmul_cached(
      (const float*)x_bytes, dW, dS, dB, (float*)y_bytes,
      S, in_features, out_features, group_size);
    return r == MLX2_MATMUL_OK ? BACKEND_OK : BACKEND_LAUNCH_FAIL;
  }
  /* Any buffer not cached (hash=0) → fresh path.  Small-input use only. */
  mlx2_matmul_status r = mlx2_matmul_fresh(
    (const float*)x_bytes,
    (const uint32_t*)w_packed_bytes,
    (const float*)   scales_bytes,
    (const float*)   biases_bytes,
    (float*)y_bytes,
    S, in_features, out_features, group_size);
  return r == MLX2_MATMUL_OK ? BACKEND_OK : BACKEND_LAUNCH_FAIL;
}

extern "C" backend_status
backend_rms_norm_fp32(const void* x_bytes,
                      const void* gamma_bytes,
                      float       eps,
                      void*       y_bytes,
                      size_t      S,
                      size_t      D)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  rms_norm_status r = rms_norm_fp32(
    (const float*)x_bytes, (const float*)gamma_bytes, eps, (float*)y_bytes, S, D);
  if ( r == RMSN_OK )         return BACKEND_OK;
  if ( r == RMSN_ALLOC_FAIL ) return BACKEND_ALLOC_FAIL;
  if ( r == RMSN_INVALID_ARG )return BACKEND_INVALID_ARG;
  return BACKEND_LAUNCH_FAIL;
}

extern "C" backend_status
backend_rope_apply_fp32(const void* x_bytes,
                        const void* cos_bytes,
                        const void* sin_bytes,
                        void*       y_bytes,
                        size_t      S,
                        size_t      H,
                        size_t      Dh)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  rope_apply_status r = rope_apply_fp32(
    (const float*)x_bytes, (const float*)cos_bytes, (const float*)sin_bytes,
    (float*)y_bytes, S, H, Dh);
  if ( r == ROPE_OK )         return BACKEND_OK;
  if ( r == ROPE_ALLOC_FAIL ) return BACKEND_ALLOC_FAIL;
  if ( r == ROPE_INVALID_ARG )return BACKEND_INVALID_ARG;
  return BACKEND_LAUNCH_FAIL;
}

extern "C" backend_status
backend_silu_mul_fp32(const void* a_bytes,
                      const void* b_bytes,
                      void*       y_bytes,
                      size_t      N)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  silu_mul_status r = silu_mul_fp32(
    (const float*)a_bytes, (const float*)b_bytes, (float*)y_bytes, N);
  if ( r == SILU_OK )         return BACKEND_OK;
  if ( r == SILU_ALLOC_FAIL ) return BACKEND_ALLOC_FAIL;
  if ( r == SILU_INVALID_ARG )return BACKEND_INVALID_ARG;
  return BACKEND_LAUNCH_FAIL;
}

extern "C" backend_status
backend_gqa_attention_fp32(const void* q_bytes,
                           const void* k_bytes,
                           const void* v_bytes,
                           void*       y_bytes,
                           size_t      S,
                           size_t      H,
                           size_t      KH,
                           size_t      Dh)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  gqa_attention_status r = gqa_attention_fp32(
    (const float*)q_bytes, (const float*)k_bytes, (const float*)v_bytes,
    (float*)y_bytes, S, H, KH, Dh);
  if ( r == GQA_OK )         return BACKEND_OK;
  if ( r == GQA_ALLOC_FAIL ) return BACKEND_ALLOC_FAIL;
  if ( r == GQA_INVALID_ARG )return BACKEND_INVALID_ARG;
  return BACKEND_LAUNCH_FAIL;
}

extern "C" backend_status
backend_mmul_mlx2_cached(const void* x_bytes,
                         uintptr_t   w_dptr,
                         uintptr_t   s_dptr,
                         uintptr_t   b_dptr,
                         void*       y_bytes,
                         size_t      S,
                         size_t      in_features,
                         size_t      out_features,
                         size_t      group_size)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  if ( w_dptr == 0 || s_dptr == 0 || b_dptr == 0 ) return BACKEND_INVALID_ARG;
  mlx2_matmul_status r = mlx2_matmul_cached(
    (const float*)x_bytes, w_dptr, s_dptr, b_dptr, (float*)y_bytes,
    S, in_features, out_features, group_size);
  return r == MLX2_MATMUL_OK ? BACKEND_OK : BACKEND_LAUNCH_FAIL;
}

extern "C" backend_status
backend_run_qwen3_block_fp32(
    const void* x_bytes, void* y_bytes,
    uintptr_t q_w,  uintptr_t q_s,  uintptr_t q_b,
    uintptr_t k_w,  uintptr_t k_s,  uintptr_t k_b,
    uintptr_t v_w,  uintptr_t v_s,  uintptr_t v_b,
    uintptr_t o_w,  uintptr_t o_s,  uintptr_t o_b,
    uintptr_t gate_w, uintptr_t gate_s, uintptr_t gate_b,
    uintptr_t up_w,   uintptr_t up_s,   uintptr_t up_b,
    uintptr_t down_w, uintptr_t down_s, uintptr_t down_b,
    uintptr_t input_ln_dptr, uintptr_t post_ln_dptr,
    uintptr_t q_norm_dptr,   uintptr_t k_norm_dptr,
    uintptr_t cos_dptr,      uintptr_t sin_dptr,
    size_t S, size_t D, size_t D_ff,
    size_t H, size_t KH, size_t Dh,
    size_t group_size, float rms_eps)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  qw3_block_status r = qw3_block_fp32(
    (const float*)x_bytes, (float*)y_bytes,
    q_w, q_s, q_b,  k_w, k_s, k_b,  v_w, v_s, v_b,  o_w, o_s, o_b,
    gate_w, gate_s, gate_b,  up_w, up_s, up_b,  down_w, down_s, down_b,
    input_ln_dptr, post_ln_dptr, q_norm_dptr, k_norm_dptr,
    cos_dptr, sin_dptr,
    S, D, D_ff, H, KH, Dh, group_size, rms_eps);
  if ( r == QW3_OK )         return BACKEND_OK;
  if ( r == QW3_ALLOC_FAIL ) return BACKEND_ALLOC_FAIL;
  if ( r == QW3_INVALID_ARG )return BACKEND_INVALID_ARG;
  return BACKEND_LAUNCH_FAIL;
}

extern "C" backend_status
backend_run_qwen3_forward_fp32(
    const void* x_bytes, void* y_bytes,
    const qw3_block_dptrs* blocks, size_t n_blocks,
    uintptr_t cos_dptr, uintptr_t sin_dptr,
    size_t S, size_t D, size_t D_ff,
    size_t H, size_t KH, size_t Dh,
    size_t group_size, float rms_eps,
    const uintptr_t* out_k_dptrs, const uintptr_t* out_v_dptrs)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  qw3_block_status r = qw3_forward_fp32(
    (const float*)x_bytes, (float*)y_bytes,
    blocks, n_blocks,
    cos_dptr, sin_dptr,
    S, D, D_ff, H, KH, Dh, group_size, rms_eps,
    out_k_dptrs, out_v_dptrs);
  if ( r == QW3_OK )         return BACKEND_OK;
  if ( r == QW3_ALLOC_FAIL ) return BACKEND_ALLOC_FAIL;
  if ( r == QW3_INVALID_ARG )return BACKEND_INVALID_ARG;
  return BACKEND_LAUNCH_FAIL;
}

extern "C" int
backend_vram_probe(uint32_t hash, size_t n_bytes, const uint8_t* sentinel,
                   uintptr_t* out_dptr)
{
  if ( !backend_available() ) return 0;
  return vram_cache_probe(hash, n_bytes, sentinel, out_dptr) == VRAM_CACHE_OK
    ? 1 : 0;
}

extern "C" backend_status
backend_vram_upload(const void* bytes, size_t n_bytes, uint32_t hash,
                    uintptr_t* out_dptr)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  vram_cache_status r = vram_cache_get_or_upload(bytes, n_bytes, hash, out_dptr);
  if ( r == VRAM_CACHE_OK )         return BACKEND_OK;
  if ( r == VRAM_CACHE_ALLOC_FAIL ) return BACKEND_ALLOC_FAIL;
  return BACKEND_INVALID_ARG;
}

extern "C" int
backend_kv_probe(uint64_t key, uintptr_t* out_dptr, size_t* out_n_bytes)
{
  if ( !backend_available() ) return 0;
  return vram_cache_probe64_keyonly(key, out_dptr, out_n_bytes) == VRAM_CACHE_OK
    ? 1 : 0;
}

extern "C" backend_status
backend_kv_alloc(uint64_t key, size_t n_bytes, uintptr_t* out_dptr)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  vram_cache_status r = vram_cache_alloc64(key, n_bytes, out_dptr);
  if ( r == VRAM_CACHE_OK )         return BACKEND_OK;
  if ( r == VRAM_CACHE_ALLOC_FAIL ) return BACKEND_ALLOC_FAIL;
  return BACKEND_INVALID_ARG;
}

extern "C" size_t
backend_kv_drop_by_mask(uint64_t mask_bits)
{
  if ( !backend_available() ) return 0;
  return vram_cache_drop_by_mask(mask_bits);
}

extern "C" int
backend_kv_drop(uint64_t key)
{
  if ( !backend_available() ) return 0;
  return vram_cache_drop(key);
}

extern "C" size_t
backend_kv_drop_if(uint64_t mask, uint64_t expect)
{
  if ( !backend_available() ) return 0;
  return vram_cache_drop_if(mask, expect);
}

extern "C" backend_status
backend_run_qwen3_decode_fp32(
    const void* x_bytes, void* y_bytes,
    const qw3_block_dptrs* blocks, size_t n_blocks,
    uintptr_t cos_dptr, uintptr_t sin_dptr,
    size_t position,
    const uintptr_t* kv_k_dptrs, const uintptr_t* kv_v_dptrs,
    size_t D, size_t D_ff,
    size_t H, size_t KH, size_t Dh,
    size_t group_size, float rms_eps)
{
  if ( !backend_available() ) return BACKEND_NO_CUDA;
  qw3_block_status r = qw3_decode_fp32(
    (const float*)x_bytes, (float*)y_bytes,
    blocks, n_blocks,
    cos_dptr, sin_dptr, position,
    kv_k_dptrs, kv_v_dptrs,
    D, D_ff, H, KH, Dh, group_size, rms_eps);
  if ( r == QW3_OK )         return BACKEND_OK;
  if ( r == QW3_ALLOC_FAIL ) return BACKEND_ALLOC_FAIL;
  if ( r == QW3_INVALID_ARG )return BACKEND_INVALID_ARG;
  return BACKEND_LAUNCH_FAIL;
}

extern "C" void
backend_get_cache_stats(backend_cache_stats* out)
{
  if ( !out ) return;
  vram_cache_stats s;
  vram_cache_get_stats(&s);
  out->resident_bytes = s.resident_bytes;
  out->budget_bytes   = s.budget_bytes;
  out->n_entries      = s.n_entries;
  out->n_hits         = s.n_hits;
  out->n_misses       = s.n_misses;
  out->n_evictions    = s.n_evictions;
}
