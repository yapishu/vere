/*
 * sgemm_det.h — deterministic fp32 matmul.
 *
 * Guarantees (strong):
 *   - bit-identical output run-to-run on a given GPU.
 *   - bit-identical output across any IEEE-754-compliant CUDA GPU
 *     (compute capability ≥ 2.0, i.e. effectively every NVIDIA GPU made
 *     since ~2010).  This holds because the kernel uses only `fmaf`
 *     (IEEE-754-mandated fused multiply-add with round-to-nearest-even)
 *     in a strictly sequential reduction — no atomics, no warp shuffle
 *     reductions, no tensor cores, no cuBLAS algorithm dispatch.
 *   - bit-identical output vs. a reference CPU implementation that
 *     uses the same sequential loop and C99 `fmaf`.
 *
 * Compile flags required to preserve the guarantee:
 *     --fmad=false       (no implicit mul+add → fma contraction)
 *     -ftz=false         (no flush-to-zero for subnormals)
 *     -prec-div=true     (IEEE-correct division if used)
 *     -prec-sqrt=true    (IEEE-correct sqrt if used)
 *     (do NOT use -use_fast_math or any -use_relaxed_* flags)
 *
 * Buffers are passed as host pointers; the function handles upload/download.
 * A cached variant that takes a VRAM-resident weight pointer is exposed
 * separately (see vram_cache.h).
 */

#ifndef URBIT_CUDA_SGEMM_DET_H
#define URBIT_CUDA_SGEMM_DET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* Success/error codes. */
typedef enum {
  SGEMM_DET_OK = 0,
  SGEMM_DET_NO_CUDA,        /* driver/runtime missing or init failed */
  SGEMM_DET_ALLOC_FAIL,     /* VRAM allocation failed */
  SGEMM_DET_LAUNCH_FAIL,    /* kernel launch error */
  SGEMM_DET_INVALID_ARG
} sgemm_det_status;

/*
 * Row-major fp32 matmul: c = a @ b.
 *   a: [M, K] row-major
 *   b: [K, N] row-major
 *   c: [M, N] row-major (output, caller-allocated)
 *
 * Deterministic — fixed reduction order over K, no atomics, no warp-shuffle
 * reductions that depend on block dims.
 */
sgemm_det_status
sgemm_det_row_major(const float* a,
                    const float* b,
                    float*       c,
                    size_t       M,
                    size_t       K,
                    size_t       N);

/*
 * Same as above, but takes a VRAM-resident B matrix (uintptr_t of CUdeviceptr
 * to keep the header CUDA-free for callers that don't include cuda.h).
 * a and c are host pointers; they're uploaded/downloaded per call.
 */
sgemm_det_status
sgemm_det_row_major_cached_b(const float* a,
                             uintptr_t    b_dptr,
                             float*       c,
                             size_t       M,
                             size_t       K,
                             size_t       N);

/* One-time init — call before first matmul.  Idempotent. */
sgemm_det_status sgemm_det_init(void);

/* Tear down CUDA context.  Not required for normal operation. */
void sgemm_det_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif  /* URBIT_CUDA_SGEMM_DET_H */
