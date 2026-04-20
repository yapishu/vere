/*
 * backend_stub.c — no-op backend for builds without CUDA.
 *
 * Every entrypoint returns BACKEND_NO_CUDA so callers fall back to their
 * existing CPU implementations.  Included in the build when -Dcuda=false.
 */

#include "backend.h"

backend_status backend_init(void) { return BACKEND_NO_CUDA; }
int            backend_available(void) { return 0; }

backend_status
backend_mmul_fp32(const void* a, const void* b, void* c,
                  size_t M, size_t K, size_t N, uint32_t h)
{
  (void)a; (void)b; (void)c; (void)M; (void)K; (void)N; (void)h;
  return BACKEND_NO_CUDA;
}

backend_status
backend_rms_norm_fp32(const void* x, const void* g, float eps, void* y,
                      size_t S, size_t D)
{
  (void)x; (void)g; (void)eps; (void)y; (void)S; (void)D;
  return BACKEND_NO_CUDA;
}

backend_status
backend_rope_apply_fp32(const void* x, const void* c, const void* s, void* y,
                        size_t S, size_t H, size_t Dh)
{
  (void)x; (void)c; (void)s; (void)y; (void)S; (void)H; (void)Dh;
  return BACKEND_NO_CUDA;
}

backend_status
backend_silu_mul_fp32(const void* a, const void* b, void* y, size_t N)
{
  (void)a; (void)b; (void)y; (void)N;
  return BACKEND_NO_CUDA;
}

backend_status
backend_gqa_attention_fp32(const void* q, const void* k, const void* v, void* y,
                           size_t S, size_t H, size_t KH, size_t Dh)
{
  (void)q; (void)k; (void)v; (void)y;
  (void)S; (void)H; (void)KH; (void)Dh;
  return BACKEND_NO_CUDA;
}

backend_status
backend_mmul_mlx2(const void* x, const void* w, const void* s, const void* b,
                  void* y, size_t S, size_t in_f, size_t out_f, size_t g,
                  uint32_t wh, uint32_t sh, uint32_t bh)
{
  (void)x; (void)w; (void)s; (void)b; (void)y;
  (void)S; (void)in_f; (void)out_f; (void)g; (void)wh; (void)sh; (void)bh;
  return BACKEND_NO_CUDA;
}

void backend_get_cache_stats(backend_cache_stats* out)
{
  if ( out ) {
    out->resident_bytes = 0; out->budget_bytes = 0; out->n_entries = 0;
    out->n_hits = 0; out->n_misses = 0; out->n_evictions = 0;
  }
}
