/*
 * rms_norm.h — deterministic fp32 RMSNorm over the last dim of [S, D].
 *
 *   y[s, d] = (x[s, d] / rms(x[s, :])) * gamma[d]
 *   rms(x[s, :]) = sqrt(mean(x[s, :]^2) + eps)
 *
 * Reduction order is fixed (sequential fmaf over d=0..D-1 per row),
 * matching the same determinism contract as sgemm_det.
 *
 * Model-agnostic: any transformer with a row-wise RMSNorm step can call
 * this (input_ln, post_attn_ln, ln_f, per-head q_norm/k_norm).
 */

#ifndef URBIT_CUDA_RMS_NORM_H
#define URBIT_CUDA_RMS_NORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum {
  RMSN_OK = 0,
  RMSN_NO_CUDA,
  RMSN_ALLOC_FAIL,
  RMSN_LAUNCH_FAIL,
  RMSN_INVALID_ARG
} rms_norm_status;

rms_norm_status
rms_norm_fp32(const float* x,
              const float* gamma,
              float        eps,
              float*       y,
              size_t       S,
              size_t       D);

#ifdef __cplusplus
}
#endif

#endif
