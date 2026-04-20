/*
 * det_test.c — bit-exact equivalence test: GPU sgemm_det vs CPU sgemm_ref.
 *
 * Runs a set of shapes and seeded random inputs; compares outputs bit-by-bit.
 * Also runs the GPU kernel repeatedly with the same inputs to confirm that
 * run-to-run determinism holds on-device.
 *
 * Build (standalone):
 *   nvcc -O2 -std=c++14 sgemm_det.cu sgemm_ref.c det_test.c -o det_test -lm
 *
 * Exit 0 on full pass, 1 on any mismatch.
 */

#include "sgemm_det.h"
#include "sgemm_ref.h"
#include "vram_cache.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t
xorshift32(uint32_t* state)
{
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static float
rand_fp32(uint32_t* state)
{
  /* uniform in [-1, 1] with reasonable distribution of exponents */
  uint32_t u = xorshift32(state);
  /* map top 24 bits to [-1, 1] */
  int32_t s = (int32_t)u;
  return (float)s / (float)0x7FFFFFFF;
}

/* Adversarial: full random bit patterns.  Will produce subnormals,
 * denormals, near-inf, occasionally NaN — exactly the conditions where
 * a non-IEEE kernel (ftz=on or fast-math) would diverge from the CPU. */
static float
rand_fp32_adversarial(uint32_t* state)
{
  uint32_t u = xorshift32(state);
  /* Skew: 1/8 chance of a subnormal (exp = 0); 1/16 chance of near-inf
   * (exp > 250); otherwise a normal. */
  uint32_t roll = u & 0xF;
  uint32_t sign = (u >> 31) & 1;
  uint32_t mant = u & 0x7FFFFF;
  uint32_t exp_;
  if ( roll < 2 ) {
    exp_ = 0;  /* subnormal / zero */
  } else if ( roll == 2 ) {
    exp_ = 250 + (u >> 28);  /* near-inf */
  } else {
    exp_ = 100 + ((u >> 20) & 0x3F);  /* moderate range, no inf */
  }
  uint32_t bits = (sign << 31) | (exp_ << 23) | mant;
  /* skip NaN (exp=255, mant!=0) — equality comparison traps on those */
  if ( exp_ == 255 && mant != 0 ) bits = (sign << 31) | (128 << 23);
  float f;
  memcpy(&f, &bits, 4);
  return f;
}

/* NaN bit-patterns aren't IEEE-standardized — x86 libm canonicalizes to
 * 0xffc00000, CUDA to 0x7fffffff.  Both ARE NaN; treat them as equal for
 * bit-exactness checks.  This only affects adversarial inputs that drive
 * the computation through 0·inf or inf−inf; real transformer activations
 * never produce NaN. */
static int _is_nan32(uint32_t u) {
  return (u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) != 0u;
}

static int
compare_bitwise(const float* a, const float* b, size_t n, const char* label)
{
  size_t diff = 0;
  size_t first_m = (size_t)-1;
  for ( size_t i = 0; i < n; i++ ) {
    uint32_t ua, ub;
    memcpy(&ua, &a[i], 4);
    memcpy(&ub, &b[i], 4);
    if ( ua != ub && !(_is_nan32(ua) && _is_nan32(ub)) ) {
      if ( first_m == (size_t)-1 ) first_m = i;
      diff++;
    }
  }
  if ( diff == 0 ) {
    fprintf(stderr, "  %-32s  BIT-EXACT (%zu elements)\n", label, n);
    return 1;
  }
  uint32_t ua, ub;
  memcpy(&ua, &a[first_m], 4);
  memcpy(&ub, &b[first_m], 4);
  fprintf(stderr, "  %-32s  MISMATCH: %zu / %zu elements; first at %zu  "
                  "a=0x%08x b=0x%08x  (%.8g vs %.8g)\n",
          label, diff, n, first_m, ua, ub, a[first_m], b[first_m]);
  return 0;
}

static int
run_one(size_t M, size_t K, size_t N, uint32_t seed, int adversarial)
{
  size_t n_a = M * K, n_b = K * N, n_c = M * N;
  float* a   = (float*)malloc(n_a * sizeof(float));
  float* b   = (float*)malloc(n_b * sizeof(float));
  float* c_r = (float*)malloc(n_c * sizeof(float));
  float* c_g = (float*)malloc(n_c * sizeof(float));
  float* c_g2= (float*)malloc(n_c * sizeof(float));

  uint32_t s = seed ? seed : 0xdeadbeefu;
  float (*rng)(uint32_t*) = adversarial ? rand_fp32_adversarial : rand_fp32;
  for ( size_t i = 0; i < n_a; i++ ) a[i] = rng(&s);
  for ( size_t i = 0; i < n_b; i++ ) b[i] = rng(&s);

  sgemm_ref_row_major(a, b, c_r, M, K, N);
  sgemm_det_status sg  = sgemm_det_row_major(a, b, c_g,  M, K, N);
  sgemm_det_status sg2 = sgemm_det_row_major(a, b, c_g2, M, K, N);
  if ( sg != SGEMM_DET_OK || sg2 != SGEMM_DET_OK ) {
    fprintf(stderr, "  [%zux%zux%zu] GPU launch failed: %d/%d\n", M, K, N, sg, sg2);
    free(a); free(b); free(c_r); free(c_g); free(c_g2);
    return 0;
  }

  char lbl[80];
  const char* tag = adversarial ? "adv" : "std";
  snprintf(lbl, sizeof lbl, "%s M=%zu K=%zu N=%zu cpu==gpu ", tag, M, K, N);
  int ok1 = compare_bitwise(c_r, c_g, n_c, lbl);
  snprintf(lbl, sizeof lbl, "%s M=%zu K=%zu N=%zu gpu==gpu ", tag, M, K, N);
  int ok2 = compare_bitwise(c_g, c_g2, n_c, lbl);

  free(a); free(b); free(c_r); free(c_g); free(c_g2);
  return ok1 && ok2;
}

#include <time.h>

static double now_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void
bench(size_t M, size_t K, size_t N, int iters)
{
  size_t n_a = M * K, n_b = K * N, n_c = M * N;
  float* a = (float*)malloc(n_a * sizeof(float));
  float* b = (float*)malloc(n_b * sizeof(float));
  float* c = (float*)malloc(n_c * sizeof(float));
  uint32_t s = 42;
  for ( size_t i = 0; i < n_a; i++ ) a[i] = rand_fp32(&s);
  for ( size_t i = 0; i < n_b; i++ ) b[i] = rand_fp32(&s);

  sgemm_det_row_major(a, b, c, M, K, N);  /* warmup */

  double t0 = now_s();
  for ( int i = 0; i < iters; i++ )
    sgemm_det_row_major(a, b, c, M, K, N);
  double t1 = now_s();
  double gpu_ms = (t1 - t0) * 1000.0 / iters;

  /* cached path: upload B once via the cache, then loop the matmul */
  uintptr_t b_dptr = 0;
  uint32_t fake_hash = (uint32_t)(M * 131u + K * 17u + N * 7u + s);
  vram_cache_get_or_upload(b, n_b * sizeof(float), fake_hash, &b_dptr);
  /* warmup cached path */
  sgemm_det_row_major_cached_b(a, b_dptr, c, M, K, N);

  t0 = now_s();
  for ( int i = 0; i < iters; i++ )
    sgemm_det_row_major_cached_b(a, b_dptr, c, M, K, N);
  t1 = now_s();
  double gpu_cached_ms = (t1 - t0) * 1000.0 / iters;

  t0 = now_s();
  sgemm_ref_row_major(a, b, c, M, K, N);
  t1 = now_s();
  double cpu_ms = (t1 - t0) * 1000.0;

  fprintf(stderr, "  M=%4zu K=%4zu N=%4zu  GPU-fresh %7.3f ms  GPU-cached %7.3f ms  CPU-ref %9.3f ms\n",
          M, K, N, gpu_ms, gpu_cached_ms, cpu_ms);

  free(a); free(b); free(c);
}

int
main(void)
{
  fprintf(stderr, "sgemm_det bit-exactness test\n");
  fprintf(stderr, "----------------------------\n");

  struct { size_t M, K, N; uint32_t seed; } cases[] = {
    {   1,    1,    1, 1 },
    {   2,    3,    4, 2 },
    {  16,   16,   16, 3 },
    {  17,   33,   19, 4 },
    {  64,  128,   64, 5 },
    { 256,  512,  256, 6 },
    {   1, 2048, 2048, 7 },  /* Qwen3 single-token shape */
    {   4, 2048, 2048, 8 },  /* Qwen3 4-token shape */
  };
  size_t n_cases = sizeof(cases) / sizeof(cases[0]);

  int all_ok = 1;
  for ( size_t i = 0; i < n_cases; i++ ) {
    if ( !run_one(cases[i].M, cases[i].K, cases[i].N, cases[i].seed, 0) )
      all_ok = 0;
  }
  fprintf(stderr, "\nadversarial inputs (subnormals, denormals, near-inf):\n");
  for ( size_t i = 0; i < n_cases; i++ ) {
    if ( !run_one(cases[i].M, cases[i].K, cases[i].N, cases[i].seed + 1000, 1) )
      all_ok = 0;
  }

  fprintf(stderr, "----------------------------\n");
  fprintf(stderr, "%s\n", all_ok ? "ALL PASS" : "FAIL");

  fprintf(stderr, "\nperf (single-threaded CPU-ref vs uncached GPU):\n");
  bench(   1, 2048, 2048, 5 );
  bench(   4, 2048, 2048, 5 );
  bench(  64, 2048, 2048, 3 );
  bench(2048, 2048, 2048, 2 );

  sgemm_det_shutdown();
  return all_ok ? 0 : 1;
}
