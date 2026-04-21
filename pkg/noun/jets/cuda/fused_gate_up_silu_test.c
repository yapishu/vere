/*
 * fused_gate_up_silu_test — byte-exact regression for gate+up+silu fusion.
 *
 * Unfused:  mlx2_matmul(x, gate_w) -> gate
 *           mlx2_matmul(x, up_w)   -> up
 *           silu_mul(gate, up)     -> hid
 * Fused:    gate_up_silu_mlx2(x, gate_*, up_*) -> hid
 *
 * Compares fp32 bytes between the two paths across the shapes seen in
 * Qwen3 block MLP.
 */

#include "mlx2_matmul.h"
#include "silu_mul.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

mlx2_matmul_status
gate_up_silu_mlx2_fresh(const float* x,
                        const uint32_t* gate_w, const float* gate_s, const float* gate_b,
                        const uint32_t* up_w,   const float* up_s,   const float* up_b,
                        float* hid,
                        size_t S, size_t in_f, size_t out_f, size_t group);

static uint64_t _rng = 0xbada55c0ffee1234ULL;
static uint32_t
rnd_u32(void) {
  _rng = _rng * 6364136223846793005ULL + 1442695040888963407ULL;
  return (uint32_t)(_rng >> 32);
}
static float
rnd_fp32_small(void) {
  uint32_t u = rnd_u32();
  float f = (float)((int32_t)u) / 2147483648.0f;
  return f + f;
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
run_one(const char* label, size_t S, size_t D, size_t D_ff, size_t group_size)
{
  size_t packed_cols    = D / 16;
  size_t groups_per_row = D / group_size;

  float*    x      = (float*)malloc(S * D * sizeof(float));
  uint32_t* gate_w = (uint32_t*)malloc(D_ff * packed_cols * sizeof(uint32_t));
  float*    gate_s = (float*)malloc(D_ff * groups_per_row * sizeof(float));
  float*    gate_b = (float*)malloc(D_ff * groups_per_row * sizeof(float));
  uint32_t* up_w   = (uint32_t*)malloc(D_ff * packed_cols * sizeof(uint32_t));
  float*    up_s   = (float*)malloc(D_ff * groups_per_row * sizeof(float));
  float*    up_b   = (float*)malloc(D_ff * groups_per_row * sizeof(float));
  float*    gate   = (float*)malloc(S * D_ff * sizeof(float));
  float*    up     = (float*)malloc(S * D_ff * sizeof(float));
  float*    hid_ref = (float*)malloc(S * D_ff * sizeof(float));
  float*    hid_fus = (float*)malloc(S * D_ff * sizeof(float));

  _rng = 0xbada55c0ffee1234ULL;
  fill_random_fp32(x,      S * D);
  fill_random_u32 (gate_w, D_ff * packed_cols);
  fill_random_fp32(gate_s, D_ff * groups_per_row);
  fill_random_fp32(gate_b, D_ff * groups_per_row);
  fill_random_u32 (up_w,   D_ff * packed_cols);
  fill_random_fp32(up_s,   D_ff * groups_per_row);
  fill_random_fp32(up_b,   D_ff * groups_per_row);

  /* Unfused pipeline. */
  mlx2_matmul_status r1 = mlx2_matmul_fresh(x, gate_w, gate_s, gate_b, gate, S, D, D_ff, group_size);
  mlx2_matmul_status r2 = mlx2_matmul_fresh(x, up_w,   up_s,   up_b,   up,   S, D, D_ff, group_size);
  silu_mul_status    r3 = silu_mul_fp32(gate, up, hid_ref, S * D_ff);
  if ( r1 != MLX2_MATMUL_OK || r2 != MLX2_MATMUL_OK || r3 != SILU_OK ) {
    fprintf(stderr, "[%s] unfused pipeline failed (%d %d %d)\n", label, r1, r2, r3);
    goto fail_free;
  }

  /* Fused. */
  mlx2_matmul_status rf = gate_up_silu_mlx2_fresh(
    x, gate_w, gate_s, gate_b, up_w, up_s, up_b, hid_fus,
    S, D, D_ff, group_size);
  if ( rf != MLX2_MATMUL_OK ) {
    fprintf(stderr, "[%s] fused kernel failed (%d)\n", label, rf);
    goto fail_free;
  }

  int mismatches = 0;
  float max_abs = 0.0f;
  for ( size_t i = 0; i < S * D_ff; i++ ) {
    if ( hid_ref[i] != hid_fus[i] ) {
      if ( mismatches < 5 ) {
        uint32_t rb, fb;
        memcpy(&rb, &hid_ref[i], 4);
        memcpy(&fb, &hid_fus[i], 4);
        fprintf(stderr,
          "  [%s] at %zu: ref=%.9g (0x%08x) fus=%.9g (0x%08x) delta=%.3g\n",
          label, i, (double)hid_ref[i], rb, (double)hid_fus[i], fb,
          (double)(hid_fus[i] - hid_ref[i]));
      }
      float d = fabsf(hid_ref[i] - hid_fus[i]);
      if ( d > max_abs ) max_abs = d;
      mismatches++;
    }
  }

  free(x); free(gate_w); free(gate_s); free(gate_b);
  free(up_w); free(up_s); free(up_b);
  free(gate); free(up); free(hid_ref); free(hid_fus);

  if ( mismatches == 0 ) {
    printf("[%s] S=%zu D=%zu D_ff=%zu group=%zu  OK (byte-exact, %zu elems)\n",
           label, S, D, D_ff, group_size, S * D_ff);
    return 0;
  }
  printf("[%s] S=%zu D=%zu D_ff=%zu group=%zu  FAIL: %d / %zu mismatches, max abs delta %.3g\n",
         label, S, D, D_ff, group_size, mismatches, S * D_ff, (double)max_abs);
  return 1;

fail_free:
  free(x); free(gate_w); free(gate_s); free(gate_b);
  free(up_w); free(up_s); free(up_b);
  free(gate); free(up); free(hid_ref); free(hid_fus);
  return 1;
}

int
main(void)
{
  int failed = 0;
  failed += run_one("mlp (decode)",  1, 1024, 3072, 64);
  failed += run_one("mlp (prefill)", 5, 1024, 3072, 64);
  failed += run_one("tiny",          1,  128,  256, 32);
  if ( failed ) {
    fprintf(stderr, "%d configuration(s) failed\n", failed);
    return 1;
  }
  printf("\nALL PASS — fused gate+up+silu is byte-exact with unfused pipeline.\n");
  return 0;
}
