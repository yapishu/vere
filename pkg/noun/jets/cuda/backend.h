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
