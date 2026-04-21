/*
 * fused_rmsnorm_mm_test — byte-exact regression for rmsnorm_mlx2_matmul.
 *
 * Runs the unfused pipeline (rms_norm_fp32 → mlx2_matmul_fresh) and the
 * fused kernel (rmsnorm_mlx2_matmul_fresh) on the same inputs, then
 * memcmp's the two fp32 outputs.  Any drift = the fusion changed the
 * math.  We sweep several shape configurations covering the Qwen3
 * block-internal matmuls:
 *   S=1  / D=1024 / out=1024   (Q, O projections at decode)
 *   S=1  / D=1024 / out=512    (K, V)
 *   S=1  / D=1024 / out=3072   (gate, up)
 *   S=5  / D=1024 / out=1024   (prefill at a small prompt length)
 *
 * Inputs are populated with a deterministic PRNG so re-runs are stable.
 */

#include "mlx2_matmul.h"
#include "rms_norm.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Public entry points we're comparing. */
mlx2_matmul_status
rmsnorm_mlx2_matmul_fresh(const float* x, const float* gamma, float eps,
                          const uint32_t* w, const float* scales,
                          const float* biases, float* y,
                          size_t S, size_t in_f, size_t out_f, size_t group);

/* Simple LCG PRNG: deterministic across machines, good enough for us. */
static uint64_t _rng = 0xdeadbeefcafebabeULL;
static uint32_t
rnd_u32(void) {
  _rng = _rng * 6364136223846793005ULL + 1442695040888963407ULL;
  return (uint32_t)(_rng >> 32);
}
static float
rnd_fp32_small(void) {
  /* Small-ish values: uniform in [-2, 2).  Enough dynamic range to
   * exercise normalization without inf/nan. */
  uint32_t u = rnd_u32();
  float f = (float)((int32_t)u) / 2147483648.0f;   /* [-1, 1) */
  return f + f;                                     /* [-2, 2) */
}

static void
fill_random_fp32(float* p, size_t n) {
  for ( size_t i = 0; i < n; i++ ) p[i] = rnd_fp32_small();
}
static void
fill_random_u32(uint32_t* p, size_t n) {
  for ( size_t i = 0; i < n; i++ ) p[i] = rnd_u32();
}

static int
run_one(const char* label, size_t S, size_t D, size_t out_f, size_t group_size)
{
  size_t packed_cols    = D / 16;
  size_t groups_per_row = D / group_size;

  float    *x      = (float*)malloc(S * D * sizeof(float));
  float    *gamma  = (float*)malloc(D * sizeof(float));
  uint32_t *w      = (uint32_t*)malloc(out_f * packed_cols * sizeof(uint32_t));
  float    *scales = (float*)malloc(out_f * groups_per_row * sizeof(float));
  float    *biases = (float*)malloc(out_f * groups_per_row * sizeof(float));
  float    *x1     = (float*)malloc(S * D * sizeof(float));
  float    *y_ref  = (float*)malloc(S * out_f * sizeof(float));
  float    *y_fus  = (float*)malloc(S * out_f * sizeof(float));

  _rng = 0xdeadbeefcafebabeULL;
  fill_random_fp32(x,      S * D);
  fill_random_fp32(gamma,  D);
  fill_random_u32 (w,      out_f * packed_cols);
  fill_random_fp32(scales, out_f * groups_per_row);
  fill_random_fp32(biases, out_f * groups_per_row);

  const float eps = 1.0e-6f;

  /* Unfused pipeline. */
  rms_norm_status rs_st = rms_norm_fp32(x, gamma, eps, x1, S, D);
  if ( rs_st != RMSN_OK ) {
    fprintf(stderr, "[%s] rms_norm_fp32 failed (%d)\n", label, (int)rs_st);
    goto fail_free;
  }
  mlx2_matmul_status mm_st = mlx2_matmul_fresh(
    x1, w, scales, biases, y_ref, S, D, out_f, group_size);
  if ( mm_st != MLX2_MATMUL_OK ) {
    fprintf(stderr, "[%s] mlx2_matmul_fresh failed (%d)\n", label, (int)mm_st);
    goto fail_free;
  }

  /* Fused kernel. */
  mlx2_matmul_status f_st = rmsnorm_mlx2_matmul_fresh(
    x, gamma, eps, w, scales, biases, y_fus,
    S, D, out_f, group_size);
  if ( f_st != MLX2_MATMUL_OK ) {
    fprintf(stderr, "[%s] rmsnorm_mlx2_matmul_fresh failed (%d)\n", label, (int)f_st);
    goto fail_free;
  }

  /* Byte-exact comparison. */
  int mismatches = 0;
  float max_abs = 0.0f;
  for ( size_t i = 0; i < S * out_f; i++ ) {
    if ( y_ref[i] != y_fus[i] ) {
      if ( mismatches < 5 ) {
        uint32_t rb, fb;
        memcpy(&rb, &y_ref[i], 4);
        memcpy(&fb, &y_fus[i], 4);
        fprintf(stderr,
          "  [%s] at %zu: ref=%.9g (0x%08x) fus=%.9g (0x%08x) delta=%.3g\n",
          label, i, (double)y_ref[i], rb, (double)y_fus[i], fb,
          (double)(y_fus[i] - y_ref[i]));
      }
      float d = fabsf(y_ref[i] - y_fus[i]);
      if ( d > max_abs ) max_abs = d;
      mismatches++;
    }
  }

  free(x); free(gamma); free(w); free(scales); free(biases);
  free(x1); free(y_ref); free(y_fus);

  if ( mismatches == 0 ) {
    printf("[%s] S=%zu D=%zu out=%zu group=%zu  OK (byte-exact, %zu elems)\n",
           label, S, D, out_f, group_size, S * out_f);
    return 0;
  }
  printf("[%s] S=%zu D=%zu out=%zu group=%zu  FAIL: %d / %zu mismatches, max abs delta %.3g\n",
         label, S, D, out_f, group_size, mismatches, S * out_f, (double)max_abs);
  return 1;

fail_free:
  free(x); free(gamma); free(w); free(scales); free(biases);
  free(x1); free(y_ref); free(y_fus);
  return 1;
}

int
main(void)
{
  int failed = 0;
  failed += run_one("q-proj (decode)",   1, 1024, 1024, 64);
  failed += run_one("kv-proj (decode)",  1, 1024,  512, 64);
  failed += run_one("gate-proj (decode)",1, 1024, 3072, 64);
  failed += run_one("q-proj (prefill)",  5, 1024, 1024, 64);
  failed += run_one("tiny",              1,  128,   64, 32);
  if ( failed ) {
    fprintf(stderr, "%d configuration(s) failed\n", failed);
    return 1;
  }
  printf("\nALL PASS — fused rmsnorm+matmul is byte-exact with unfused pipeline.\n");
  return 0;
}
