/*
 * sgemm_ref.h — CPU reference matmul with the SAME reduction order as the
 * deterministic GPU kernel, for bit-exactness tests.
 *
 * Per output element (m, n):
 *     acc = 0.0f
 *     for k in 0..K-1:  acc = fmaf(A[m, k], B[k, n], acc)
 *     C[m, n] = acc
 *
 * Uses C99 fmaf (IEEE-754 fused multiply-add).  fmaf is mandated by C99 to
 * compute the product without intermediate rounding, so CPU and GPU agree
 * as long as both use IEEE-754 round-to-nearest-even (the default).
 */

#ifndef URBIT_CUDA_SGEMM_REF_H
#define URBIT_CUDA_SGEMM_REF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

void sgemm_ref_row_major(const float* a,
                         const float* b,
                         float*       c,
                         size_t       M,
                         size_t       K,
                         size_t       N);

#ifdef __cplusplus
}
#endif

#endif  /* URBIT_CUDA_SGEMM_REF_H */
