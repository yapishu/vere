/*
 * mlx2_test.c — bit-exactness for fused MLX2 dequant+matmul.
 *
 * CPU reference does exactly the same per-element operations as the GPU
 * kernel: dequant one weight with fmaf(scale, q, bias), then fmaf(x, w_fp, acc).
 * No intermediate materialization of a full fp32 weight matrix; that would
 * change the rounding of the outer reduction (it shouldn't, given fmaf
 * rounds once, but we keep it identical for maximum clarity).
 */

#include "mlx2_matmul.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

static uint32_t
xorshift32(uint32_t* state)
{
  uint32_t x = *state;
  x ^= x << 13;  x ^= x >> 17;  x ^= x << 5;
  *state = x;
  return x;
}

static float
rand_small_fp32(uint32_t* state)
{
  int32_t s = (int32_t)xorshift32(state);
  return (float)s / (float)0x7FFFFFFF * 0.1f;  /* ~[-0.1, 0.1] */
}

static void
mlx2_ref(const float*    x,
         const uint32_t* w,
         const float*    scales,
         const float*    biases,
         float*          y,
         size_t S, size_t in_f, size_t out_f, size_t group)
{
  size_t packed_cols    = in_f / 16;
  size_t groups_per_row = in_f / group;
  for ( size_t s = 0; s < S; s++ ) {
    for ( size_t o = 0; o < out_f; o++ ) {
      size_t gpr_offset = o * groups_per_row;
      size_t w_row      = o * packed_cols;
      const float* x_row = x + s * in_f;
      float acc = 0.0f;
      for ( size_t wc = 0; wc < packed_cols; wc++ ) {
        uint32_t word = w[w_row + wc];
        size_t i_base = wc * 16;
        for ( int k = 0; k < 16; k++ ) {
          size_t i = i_base + (size_t)k;
          size_t grp = i / group;
          float scale = scales[gpr_offset + grp];
          float bias  = biases[gpr_offset + grp];
          uint32_t q  = (word >> (k * 2)) & 0x3u;
          /* Separate mul+add (two IEEE roundings) to match Hoon. */
          float w_fp  = (scale * (float)q) + bias;
          acc = acc + x_row[i] * w_fp;
        }
      }
      y[s * out_f + o] = acc;
    }
  }
}

static int
compare_bitwise(const float* a, const float* b, size_t n, const char* label)
{
  size_t diff = 0, first_m = (size_t)-1;
  for ( size_t i = 0; i < n; i++ ) {
    uint32_t ua, ub;
    memcpy(&ua, &a[i], 4);
    memcpy(&ub, &b[i], 4);
    if ( ua != ub ) { if ( first_m == (size_t)-1 ) first_m = i; diff++; }
  }
  if ( diff == 0 ) {
    fprintf(stderr, "  %-40s BIT-EXACT (%zu)\n", label, n);
    return 1;
  }
  uint32_t ua, ub;
  memcpy(&ua, &a[first_m], 4);
  memcpy(&ub, &b[first_m], 4);
  fprintf(stderr, "  %-40s MISMATCH %zu/%zu; first@%zu a=%08x b=%08x (%.6g vs %.6g)\n",
          label, diff, n, first_m, ua, ub, a[first_m], b[first_m]);
  return 0;
}

static double
now_s(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static int
run_case(size_t S, size_t in_f, size_t out_f, size_t group, uint32_t seed)
{
  size_t packed_cols    = in_f / 16;
  size_t groups_per_row = in_f / group;

  float*    x      = (float*)   malloc(S * in_f * sizeof(float));
  uint32_t* w      = (uint32_t*)malloc(out_f * packed_cols * sizeof(uint32_t));
  float*    scales = (float*)   malloc(out_f * groups_per_row * sizeof(float));
  float*    biases = (float*)   malloc(out_f * groups_per_row * sizeof(float));
  float*    y_gpu  = (float*)   malloc(S * out_f * sizeof(float));
  float*    y_ref  = (float*)   malloc(S * out_f * sizeof(float));

  uint32_t st = seed;
  for ( size_t i = 0; i < S * in_f; i++ )                  x[i]      = rand_small_fp32(&st);
  for ( size_t i = 0; i < out_f * packed_cols; i++ )       w[i]      = xorshift32(&st);
  for ( size_t i = 0; i < out_f * groups_per_row; i++ )    scales[i] = rand_small_fp32(&st);
  for ( size_t i = 0; i < out_f * groups_per_row; i++ )    biases[i] = rand_small_fp32(&st);

  mlx2_ref(x, w, scales, biases, y_ref, S, in_f, out_f, group);
  mlx2_matmul_status st_gpu =
    mlx2_matmul_fresh(x, w, scales, biases, y_gpu, S, in_f, out_f, group);
  if ( st_gpu != MLX2_MATMUL_OK ) {
    fprintf(stderr, "  GPU launch failed: %d\n", st_gpu);
    free(x); free(w); free(scales); free(biases); free(y_gpu); free(y_ref);
    return 0;
  }

  char lbl[80];
  snprintf(lbl, sizeof lbl, "S=%zu in=%zu out=%zu g=%zu cpu==gpu",
           S, in_f, out_f, group);
  int ok = compare_bitwise(y_ref, y_gpu, S * out_f, lbl);

  free(x); free(w); free(scales); free(biases); free(y_gpu); free(y_ref);
  return ok;
}

int
main(void)
{
  fprintf(stderr, "mlx2 fused matmul bit-exactness\n");
  fprintf(stderr, "-------------------------------\n");
  struct { size_t S, in_f, out_f, g; uint32_t seed; } cases[] = {
    { 1,  256,  128, 128, 1 },
    { 1, 2048, 2048, 128, 2 },  /* Qwen3 q_proj / single token */
    { 4, 2048, 2048, 128, 3 },  /* Qwen3 q_proj / 4 tokens */
    { 4, 2048, 1024, 128, 4 },  /* Qwen3 k_proj / v_proj */
    { 4, 2048, 6144, 128, 5 },  /* Qwen3 gate/up_proj */
    { 4, 6144, 2048, 128, 6 },  /* Qwen3 down_proj */
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  int ok = 1;
  for ( size_t i = 0; i < n; i++ ) {
    if ( !run_case(cases[i].S, cases[i].in_f, cases[i].out_f,
                   cases[i].g, cases[i].seed) ) ok = 0;
  }

  fprintf(stderr, "-------------------------------\n%s\n", ok ? "ALL PASS" : "FAIL");

  /* Rough perf (cached path, one matmul per call after warmup). */
  fprintf(stderr, "\nperf (Qwen3-shaped, fused GPU with cache vs uncached):\n");
  struct { size_t S, in_f, out_f, g; } perf_cases[] = {
    { 1, 2048, 2048, 128 },
    { 4, 2048, 2048, 128 },
    { 1, 2048, 6144, 128 },
    { 1, 6144, 2048, 128 },
  };
  for ( size_t p = 0; p < sizeof(perf_cases)/sizeof(perf_cases[0]); p++ ) {
    size_t S = perf_cases[p].S, in_f = perf_cases[p].in_f;
    size_t out_f = perf_cases[p].out_f, g = perf_cases[p].g;
    size_t pc = in_f / 16, gpr = in_f / g;
    float* x = (float*)malloc(S * in_f * sizeof(float));
    uint32_t* w = (uint32_t*)malloc(out_f * pc * sizeof(uint32_t));
    float* sc = (float*)malloc(out_f * gpr * sizeof(float));
    float* bi = (float*)malloc(out_f * gpr * sizeof(float));
    float* y  = (float*)malloc(S * out_f * sizeof(float));
    uint32_t st = 99;
    for ( size_t i = 0; i < S * in_f; i++ )      x[i]  = rand_small_fp32(&st);
    for ( size_t i = 0; i < out_f * pc; i++ )    w[i]  = xorshift32(&st);
    for ( size_t i = 0; i < out_f * gpr; i++ )   sc[i] = rand_small_fp32(&st);
    for ( size_t i = 0; i < out_f * gpr; i++ )   bi[i] = rand_small_fp32(&st);

    /* warm, fresh */
    mlx2_matmul_fresh(x, w, sc, bi, y, S, in_f, out_f, g);
    double t0 = now_s();
    int iters = 10;
    for ( int i = 0; i < iters; i++ )
      mlx2_matmul_fresh(x, w, sc, bi, y, S, in_f, out_f, g);
    double fresh_ms = (now_s() - t0) * 1000.0 / iters;

    fprintf(stderr, "  S=%zu in=%zu out=%zu  fresh %7.3f ms  (%.1f GFLOPs eff.)\n",
            S, in_f, out_f, fresh_ms,
            (double)S * in_f * out_f * 2.0 / fresh_ms / 1e6);
    free(x); free(w); free(sc); free(bi); free(y);
  }
  return ok ? 0 : 1;
}
