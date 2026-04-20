/*
 * silu_mul.h — fused SiLU(a) * b elementwise on fp32 tensors of the
 * same shape.  y[i] = a[i] * (1 / (1 + exp(-a[i]))) * b[i].
 *
 * Used by SwiGLU-style MLPs in any transformer (Llama/Qwen/Mistral/...).
 * Trivially deterministic: no reduction, one thread per element.
 */

#ifndef URBIT_CUDA_SILU_MUL_H
#define URBIT_CUDA_SILU_MUL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef enum {
  SILU_OK = 0,
  SILU_NO_CUDA,
  SILU_ALLOC_FAIL,
  SILU_LAUNCH_FAIL,
  SILU_INVALID_ARG
} silu_mul_status;

silu_mul_status
silu_mul_fp32(const float* a, const float* b, float* y, size_t N);

#ifdef __cplusplus
}
#endif

#endif
