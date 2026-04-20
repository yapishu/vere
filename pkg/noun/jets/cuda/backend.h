/*
 * backend.h — public entrypoint that lagoon.c calls.
 *
 * Exposes "try GPU, fall back to softblas" primitives.  Callers pass noun
 * atom bytes + content hash (from u3r_mug); the backend handles VRAM cache
 * lookup, kernel launch, and host/device copies.  Returns a status code;
 * on anything other than BACKEND_OK, the caller should run its existing
 * CPU path.
 *
 * This header is pure C — no cuda.h, no C++.  Safe to include from any .c.
 */

#ifndef URBIT_CUDA_BACKEND_H
#define URBIT_CUDA_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "qwen3_block.h"  /* qw3_block_dptrs */

typedef enum {
  BACKEND_OK            = 0,
  BACKEND_NO_CUDA       = 1,  /* no driver / init failed — caller must fall back */
  BACKEND_UNSUPPORTED   = 2,  /* unsupported bloq / kind / shape — fall back */
  BACKEND_ALLOC_FAIL    = 3,  /* VRAM alloc failed — fall back */
  BACKEND_LAUNCH_FAIL   = 4,  /* kernel launch/copy failed — fall back */
  BACKEND_INVALID_ARG   = 5
} backend_status;

/* One-time init: loads CUDA, picks device, sets VRAM cache budget.
 * Safe to call many times.  Returns BACKEND_NO_CUDA if no GPU is available
 * — caller should note this and stop trying. */
backend_status backend_init(void);

/* True iff backend_init succeeded.  Called on every jet entry to decide
 * whether to take the GPU path at all. */
int backend_available(void);

/* fp32 row-major matmul:  c = a @ b
 *   a: [M, K] fp32 bytes
 *   b: [K, N] fp32 bytes
 *   c: [M, N] fp32 bytes
 *
 * b_hash is a stable content hash of b_bytes (u3r_mug is fine).  If b is
 * already in the VRAM cache it's reused; otherwise uploaded and cached.
 * Pass b_hash=0 to disable caching for this call.
 */
backend_status
backend_mmul_fp32(const void* a_bytes,
                  const void* b_bytes,
                  void*       c_bytes,
                  size_t      M,
                  size_t      K,
                  size_t      N,
                  uint32_t    b_hash);

/* Fused MLX 2-bit dequant + matmul:
 *   y[s, o] = sum_i  x[s, i] * (scales[o, i/g] * q[o, i] + biases[o, i/g])
 * where q[o, i] = (w[o, i/16] >> ((i%16)*2)) & 3.
 *
 * w_packed: [out_features, in_features/16] uint32 bytes
 * scales:   [out_features, in_features/group] fp32 bytes
 * biases:   [out_features, in_features/group] fp32 bytes
 * x:        [S, in_features] fp32 bytes
 * y:        [S, out_features] fp32 bytes
 *
 * w_hash / s_hash / b_hash are content hashes for cache lookup; pass 0 to
 * skip caching for a given buffer.
 */
/* Deterministic RMSNorm:  y[s, d] = (x[s, d] / rms(x[s, :])) * gamma[d]
 * x, gamma, y are host fp32 bytes; x and y are [S, D], gamma is [D].
 * eps is applied inside the sqrt (mean_sq + eps).
 */
backend_status
backend_rms_norm_fp32(const void* x_bytes,
                      const void* gamma_bytes,
                      float       eps,
                      void*       y_bytes,
                      size_t      S,
                      size_t      D);

/* RoPE (half-rotated) on an [S, H, Dh] fp32 tensor with cos/sin [S, Dh]. */
backend_status
backend_rope_apply_fp32(const void* x_bytes,
                        const void* cos_bytes,
                        const void* sin_bytes,
                        void*       y_bytes,
                        size_t      S,
                        size_t      H,
                        size_t      Dh);

/* Fused SiLU(a) * b elementwise, same-shape inputs, N total elements. */
backend_status
backend_silu_mul_fp32(const void* a_bytes,
                      const void* b_bytes,
                      void*       y_bytes,
                      size_t      N);

/* Fused causal GQA attention.  q: [S, H, Dh]; k/v: [S, KH, Dh]; out:
 * [S, H*Dh] (heads concatenated in last dim). */
backend_status
backend_gqa_attention_fp32(const void* q_bytes,
                           const void* k_bytes,
                           const void* v_bytes,
                           void*       y_bytes,
                           size_t      S,
                           size_t      H,
                           size_t      KH,
                           size_t      Dh);

backend_status
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
                  uint32_t    b_hash);

/* Pre-resolved variant: caller has already probed the VRAM cache and
 * supplies device pointers for w/s/b.  x is uploaded from host, y is
 * downloaded to host.  Saves u3r_bytes cost on the caller side for
 * weights that are already resident. */
backend_status
backend_mmul_mlx2_cached(const void* x_bytes,
                         uintptr_t   w_dptr,
                         uintptr_t   s_dptr,
                         uintptr_t   b_dptr,
                         void*       y_bytes,
                         size_t      S,
                         size_t      in_features,
                         size_t      out_features,
                         size_t      group_size);

/*
 * Content-hashed VRAM lookup.  Does NOT upload; returns 1 with the
 * cached device pointer (as uintptr_t) in *out_dptr iff a matching
 * entry is resident.  Returns 0 otherwise.
 *
 * sentinel: first min(16, n_bytes) bytes of the payload, used as a
 * cheap guard against 32-bit hash collisions.  Callers can read just
 * those bytes from a loom atom via a 16-byte u3r_bytes call, avoiding
 * the full payload copy entirely on cache hits.
 */
int
backend_vram_probe(uint32_t       hash,
                   size_t         n_bytes,
                   const uint8_t* sentinel,
                   uintptr_t*     out_dptr);

/* Upload host bytes to VRAM and store in the content-hashed cache.
 * On cache hit (same hash+bytes+n_bytes), returns the cached dptr
 * without re-uploading.  Idempotent: same input → same dptr.  Safe
 * to call without a prior probe (just probe+insert combined). */
backend_status
backend_vram_upload(const void* bytes,
                    size_t      n_bytes,
                    uint32_t    hash,
                    uintptr_t*  out_dptr);

/* Fused per-transformer-block Qwen3 forward.  All weight/norm/rope
 * arguments are VRAM device pointers (caller resolves via vram_cache
 * and vram_upload).  Only the activation x is uploaded HtoD; the
 * result y is downloaded DtoH.  Eliminates ~7 redundant HtoD uploads
 * per block call, since gammas and cos/sin are referenced across all
 * 28 block calls of a forward (and across forwards for gammas). */
backend_status
backend_run_qwen3_block_fp32(
    const void* x_bytes,   void* y_bytes,
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
    size_t group_size, float rms_eps);

/* Fused whole-forward Qwen3 kernel.  Runs all `n_blocks` transformer
 * blocks on GPU, keeping x resident in VRAM across blocks.  Requires
 * all per-block weights + gammas and the rope cos/sin to already have
 * VRAM dptrs (caller looks them up via vram_cache_probe / upload). */
backend_status
backend_run_qwen3_forward_fp32(
    const void*             x_bytes,
    void*                   y_bytes,
    const qw3_block_dptrs*  blocks,
    size_t                  n_blocks,
    uintptr_t               cos_dptr,
    uintptr_t               sin_dptr,
    size_t                  S,
    size_t                  D,
    size_t                  D_ff,
    size_t                  H,
    size_t                  KH,
    size_t                  Dh,
    size_t                  group_size,
    float                   rms_eps);

/* Cache stats for introspection. */
typedef struct {
  size_t   resident_bytes;
  size_t   budget_bytes;
  size_t   n_entries;
  uint64_t n_hits;
  uint64_t n_misses;
  uint64_t n_evictions;
} backend_cache_stats;

void backend_get_cache_stats(backend_cache_stats* out);

#ifdef __cplusplus
}
#endif

#endif  /* URBIT_CUDA_BACKEND_H */
