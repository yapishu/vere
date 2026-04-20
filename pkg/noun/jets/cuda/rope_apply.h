/*
 * rope_apply.h — deterministic fp32 RoPE (rotary positional embedding)
 * applied to an [S, H, Dh] tensor given cos/sin tables of shape [S, Dh].
 *
 *   half = Dh / 2
 *   rotate_half(x)[p, h, j] =
 *     j < half ?  -x[p, h, j + half]
 *              :   x[p, h, j - half]
 *   y[p, h, j] = fmaf(x[p, h, j],             cos[p, j],
 *                fmaf(rotate_half(x)[p,h,j],  sin[p, j],
 *                     0.0f))
 *
 * Strictly elementwise — trivially deterministic (no reduction).  One
 * thread per output element, each does exactly 2 fmaf calls + 1 load.
 * Model-agnostic: any transformer using half-rotated RoPE.
 */

#ifndef URBIT_CUDA_ROPE_APPLY_H
#define URBIT_CUDA_ROPE_APPLY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef enum {
  ROPE_OK = 0,
  ROPE_NO_CUDA,
  ROPE_ALLOC_FAIL,
  ROPE_LAUNCH_FAIL,
  ROPE_INVALID_ARG
} rope_apply_status;

rope_apply_status
rope_apply_fp32(const float* x,
                const float* cos_tbl,
                const float* sin_tbl,
                float*       y,
                size_t       S,
                size_t       H,
                size_t       Dh);

#ifdef __cplusplus
}
#endif

#endif
