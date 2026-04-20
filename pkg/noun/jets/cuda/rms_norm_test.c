/*
 * rms_norm_test.c — bit-exactness check for rms_norm_fp32 vs a CPU
 * reference using the same sequential fmaf reduction.  Covers tiny
 * and transformer-scale shapes with adversarial inputs (subnormals /
 * near-inf) where non-IEEE kernels would diverge.
 */

#include "rms_norm.h"

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
rand_small(uint32_t* st)
{
  int32_t s = (int32_t)xorshift32(st);
  return (float)s / (float)0x7FFFFFFF;
}

static float
rand_adversarial(uint32_t* st)
{
  uint32_t u = xorshift32(st);
  uint32_t roll = u & 0xF;
  uint32_t sign = (u >> 31) & 1;
  uint32_t mant = u & 0x7FFFFF;
  uint32_t exp_ = roll < 2 ? 0
               : roll == 2 ? 240 + (u >> 28)
               : 100 + ((u >> 20) & 0x3F);
  uint32_t bits = (sign << 31) | (exp_ << 23) | mant;
  if ( exp_ == 255 && mant != 0 ) bits = (sign << 31) | (128 << 23);
  float f; memcpy(&f, &bits, 4); return f;
}

static void
rms_norm_ref(const float* x, const float* g, float eps, float* y,
             size_t S, size_t D)
{
  for ( size_t s = 0; s < S; s++ ) {
    const float* xr = x + s * D;
    float* yr = y + s * D;
    float sumsq = 0.0f;
    for ( size_t d = 0; d < D; d++ ) sumsq = fmaf(xr[d], xr[d], sumsq);
    float rms = sqrtf(sumsq / (float)D + eps);
    for ( size_t d = 0; d < D; d++ ) yr[d] = (xr[d] / rms) * g[d];
  }
}

static int
compare(const float* a, const float* b, size_t n, const char* label)
{
  size_t diff = 0, first = (size_t)-1;
  for ( size_t i = 0; i < n; i++ ) {
    uint32_t ua, ub;
    memcpy(&ua, &a[i], 4); memcpy(&ub, &b[i], 4);
    if ( ua != ub ) { if ( first == (size_t)-1 ) first = i; diff++; }
  }
  if ( diff == 0 ) {
    fprintf(stderr, "  %-48s BIT-EXACT (%zu)\n", label, n);
    return 1;
  }
  uint32_t ua, ub;
  memcpy(&ua, &a[first], 4); memcpy(&ub, &b[first], 4);
  fprintf(stderr, "  %-48s MISMATCH %zu/%zu; first@%zu a=%08x b=%08x\n",
          label, diff, n, first, ua, ub);
  return 0;
}

static int
run_case(size_t S, size_t D, uint32_t seed, int adversarial)
{
  float* x = (float*)malloc(S * D * sizeof(float));
  float* g = (float*)malloc(D * sizeof(float));
  float* y_cpu = (float*)malloc(S * D * sizeof(float));
  float* y_gpu = (float*)malloc(S * D * sizeof(float));
  uint32_t st = seed;
  float (*rng)(uint32_t*) = adversarial ? rand_adversarial : rand_small;
  for ( size_t i = 0; i < S * D; i++ ) x[i] = rng(&st);
  for ( size_t i = 0; i < D;     i++ ) g[i] = rand_small(&st) + 1.0f;

  float eps = 1e-6f;
  rms_norm_ref(x, g, eps, y_cpu, S, D);
  rms_norm_status r = rms_norm_fp32(x, g, eps, y_gpu, S, D);
  if ( r != RMSN_OK ) {
    fprintf(stderr, "  GPU failed: %d\n", r);
    free(x); free(g); free(y_cpu); free(y_gpu);
    return 0;
  }
  char lbl[80];
  snprintf(lbl, sizeof lbl, "%s S=%zu D=%zu", adversarial ? "adv" : "std", S, D);
  int ok = compare(y_cpu, y_gpu, S * D, lbl);
  free(x); free(g); free(y_cpu); free(y_gpu);
  return ok;
}

int
main(void)
{
  fprintf(stderr, "rms_norm bit-exactness\n");
  fprintf(stderr, "----------------------\n");
  struct { size_t S, D; uint32_t seed; } cases[] = {
    { 1,    1, 1 },
    { 1,    4, 2 },
    { 4,  128, 3 },
    { 4, 2048, 4 },   /* Qwen3 */
    { 4, 6144, 5 },   /* Qwen3 MLP hidden */
    { 1,  768, 6 },   /* GPT-2 small */
  };
  int all = 1;
  for ( size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++ )
    all &= run_case(cases[i].S, cases[i].D, cases[i].seed, 0);
  fprintf(stderr, "\nadversarial inputs (subnormals, near-inf):\n");
  for ( size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++ )
    all &= run_case(cases[i].S, cases[i].D, cases[i].seed + 1000, 1);
  fprintf(stderr, "----------------------\n%s\n", all ? "ALL PASS" : "FAIL");
  return all ? 0 : 1;
}
