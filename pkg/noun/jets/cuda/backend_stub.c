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
backend_run_qwen3_block_fp32(
    const void* x, void* y,
    uintptr_t qw, uintptr_t qs, uintptr_t qb,
    uintptr_t kw, uintptr_t ks, uintptr_t kb,
    uintptr_t vw, uintptr_t vs, uintptr_t vb,
    uintptr_t ow, uintptr_t os, uintptr_t ob,
    uintptr_t gw, uintptr_t gs, uintptr_t gb,
    uintptr_t uw, uintptr_t us, uintptr_t ub,
    uintptr_t dw, uintptr_t ds, uintptr_t db,
    uintptr_t iln, uintptr_t pln, uintptr_t qn, uintptr_t kn,
    uintptr_t cs, uintptr_t sn,
    size_t S, size_t D, size_t Dff,
    size_t H, size_t KH, size_t Dh, size_t gs2, float eps)
{
  (void)x; (void)y;
  (void)qw;(void)qs;(void)qb;(void)kw;(void)ks;(void)kb;
  (void)vw;(void)vs;(void)vb;(void)ow;(void)os;(void)ob;
  (void)gw;(void)gs;(void)gb;(void)uw;(void)us;(void)ub;
  (void)dw;(void)ds;(void)db;
  (void)iln;(void)pln;(void)qn;(void)kn;(void)cs;(void)sn;
  (void)S;(void)D;(void)Dff;(void)H;(void)KH;(void)Dh;(void)gs2;(void)eps;
  return BACKEND_NO_CUDA;
}

backend_status
backend_vram_upload(const void* bytes, size_t n_bytes, uint32_t hash,
                    uintptr_t* out_dptr)
{
  (void)bytes; (void)n_bytes; (void)hash; (void)out_dptr;
  return BACKEND_NO_CUDA;
}

int
backend_kv_probe(uint64_t key, uintptr_t* out_dptr, size_t* out_n_bytes)
{
  (void)key; (void)out_dptr; (void)out_n_bytes;
  return 0;
}

backend_status
backend_kv_alloc(uint64_t key, size_t n_bytes, uintptr_t* out_dptr)
{
  (void)key; (void)n_bytes; (void)out_dptr;
  return BACKEND_NO_CUDA;
}

size_t
backend_kv_drop_by_mask(uint64_t mask_bits)
{
  (void)mask_bits;
  return 0;
}

int
backend_kv_drop(uint64_t key)
{
  (void)key;
  return 0;
}

size_t
backend_kv_drop_if(uint64_t mask, uint64_t expect)
{
  (void)mask; (void)expect;
  return 0;
}

backend_status
backend_run_qwen3_decode_fp32(
    const void* x, void* y,
    const qw3_block_dptrs* blocks, size_t n_blocks,
    uintptr_t cos_dptr, uintptr_t sin_dptr, size_t position,
    const uintptr_t* kk, const uintptr_t* vv,
    size_t D, size_t Dff, size_t H, size_t KH, size_t Dh,
    size_t gs, float eps)
{
  (void)x; (void)y; (void)blocks; (void)n_blocks;
  (void)cos_dptr; (void)sin_dptr; (void)position;
  (void)kk; (void)vv;
  (void)D; (void)Dff; (void)H; (void)KH; (void)Dh;
  (void)gs; (void)eps;
  return BACKEND_NO_CUDA;
}

backend_status
backend_run_qwen3_forward_fp32(
    const void* x, void* y,
    const qw3_block_dptrs* blocks, size_t n_blocks,
    uintptr_t cos_dptr, uintptr_t sin_dptr,
    size_t S, size_t D, size_t Dff,
    size_t H, size_t KH, size_t Dh,
    size_t group_size, float rms_eps,
    const uintptr_t* ok, const uintptr_t* ov)
{
  (void)x; (void)y; (void)blocks; (void)n_blocks;
  (void)cos_dptr; (void)sin_dptr;
  (void)S; (void)D; (void)Dff; (void)H; (void)KH; (void)Dh;
  (void)group_size; (void)rms_eps; (void)ok; (void)ov;
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

int backend_vram_probe(uint32_t h, size_t n, const uint8_t* s, uintptr_t* out)
{
  (void)h; (void)n; (void)s; (void)out;
  return 0;
}

backend_status
backend_mmul_mlx2_cached(const void* x, uintptr_t wd, uintptr_t sd, uintptr_t bd,
                        void* y, size_t S, size_t in, size_t out, size_t g)
{
  (void)x; (void)wd; (void)sd; (void)bd; (void)y;
  (void)S; (void)in; (void)out; (void)g;
  return BACKEND_NO_CUDA;
}

void backend_get_cache_stats(backend_cache_stats* out)
{
  if ( out ) {
    out->resident_bytes = 0; out->budget_bytes = 0; out->n_entries = 0;
    out->n_hits = 0; out->n_misses = 0; out->n_evictions = 0;
  }
}
