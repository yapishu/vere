/*
 * s1_matmul_test — byte-exact regression for S=1 optimized kernels.
 *
 * mlx2_matmul_s1 vs mlx2_matmul              (at S=1)
 * gate_up_silu_mlx2_s1 vs gate_up_silu_mlx2  (at S=1)
 *
 * Both pairs must produce bit-identical fp32 bytes — the S=1 variants
 * only change the thread/block layout and memory access pattern, not
 * the math.
 */

#include "mlx2_matmul.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

mlx2_matmul_status
mlx2_matmul_s1_fresh(const float* x, const uint32_t* w,
                     const float* scales, const float* biases, float* y,
                     size_t in_f, size_t out_f, size_t group);
mlx2_matmul_status
gate_up_silu_mlx2_fresh(const float* x,
                        const uint32_t* gate_w, const float* gate_s, const float* gate_b,
                        const uint32_t* up_w,   const float* up_s,   const float* up_b,
                        float* hid,
                        size_t S, size_t in_f, size_t out_f, size_t group);
mlx2_matmul_status
gate_up_silu_mlx2_s1_fresh(const float* x,
                           const uint32_t* gate_w, const float* gate_s, const float* gate_b,
                           const uint32_t* up_w,   const float* up_s,   const float* up_b,
                           float* hid,
                           size_t in_f, size_t out_f, size_t group);

static uint64_t _rng = 0x5eedABBA5A1A0001ULL;
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
compare_fp32(const char* label, const float* a, const float* b, size_t n)
{
  int mis = 0; float mxd = 0.0f;
  for ( size_t i = 0; i < n; i++ ) {
    if ( a[i] != b[i] ) {
      if ( mis < 5 ) {
        uint32_t ab, bb;
        memcpy(&ab, &a[i], 4);
        memcpy(&bb, &b[i], 4);
        fprintf(stderr, "  [%s] at %zu: ref=%.9g (0x%08x) new=%.9g (0x%08x)\n",
                label, i, (double)a[i], ab, (double)b[i], bb);
      }
      float d = fabsf(a[i] - b[i]);
      if ( d > mxd ) mxd = d;
      mis++;
    }
  }
  if ( mis == 0 ) { printf("[%s] OK (%zu elems byte-exact)\n", label, n); return 0; }
  printf("[%s] FAIL %d/%zu mismatches, max abs %.3g\n", label, mis, n, (double)mxd);
  return 1;
}

static int
test_plain_matmul(const char* label, size_t D, size_t out_f, size_t group_size)
{
  size_t packed_cols    = D / 16;
  size_t groups_per_row = D / group_size;

  float*    x      = (float*)malloc(D * sizeof(float));
  uint32_t* w      = (uint32_t*)malloc(out_f * packed_cols * sizeof(uint32_t));
  float*    scales = (float*)malloc(out_f * groups_per_row * sizeof(float));
  float*    biases = (float*)malloc(out_f * groups_per_row * sizeof(float));
  float*    y_ref  = (float*)malloc(out_f * sizeof(float));
  float*    y_s1   = (float*)malloc(out_f * sizeof(float));

  _rng = 0x5eedABBA5A1A0001ULL;
  fill_random_fp32(x,      D);
  fill_random_u32 (w,      out_f * packed_cols);
  fill_random_fp32(scales, out_f * groups_per_row);
  fill_random_fp32(biases, out_f * groups_per_row);

  mlx2_matmul_fresh(x, w, scales, biases, y_ref, 1, D, out_f, group_size);
  mlx2_matmul_s1_fresh(x, w, scales, biases, y_s1, D, out_f, group_size);

  int r = compare_fp32(label, y_ref, y_s1, out_f);
  free(x); free(w); free(scales); free(biases); free(y_ref); free(y_s1);
  return r;
}

static int
test_fused_silu(const char* label, size_t D, size_t D_ff, size_t group_size)
{
  size_t packed_cols    = D / 16;
  size_t groups_per_row = D / group_size;

  float*    x      = (float*)malloc(D * sizeof(float));
  uint32_t* gate_w = (uint32_t*)malloc(D_ff * packed_cols * sizeof(uint32_t));
  float*    gate_s = (float*)malloc(D_ff * groups_per_row * sizeof(float));
  float*    gate_b = (float*)malloc(D_ff * groups_per_row * sizeof(float));
  uint32_t* up_w   = (uint32_t*)malloc(D_ff * packed_cols * sizeof(uint32_t));
  float*    up_s   = (float*)malloc(D_ff * groups_per_row * sizeof(float));
  float*    up_b   = (float*)malloc(D_ff * groups_per_row * sizeof(float));
  float*    h_ref  = (float*)malloc(D_ff * sizeof(float));
  float*    h_s1   = (float*)malloc(D_ff * sizeof(float));

  _rng = 0x5eedABBA5A1A0001ULL;
  fill_random_fp32(x,      D);
  fill_random_u32 (gate_w, D_ff * packed_cols);
  fill_random_fp32(gate_s, D_ff * groups_per_row);
  fill_random_fp32(gate_b, D_ff * groups_per_row);
  fill_random_u32 (up_w,   D_ff * packed_cols);
  fill_random_fp32(up_s,   D_ff * groups_per_row);
  fill_random_fp32(up_b,   D_ff * groups_per_row);

  gate_up_silu_mlx2_fresh(x, gate_w, gate_s, gate_b, up_w, up_s, up_b, h_ref,
                          1, D, D_ff, group_size);
  gate_up_silu_mlx2_s1_fresh(x, gate_w, gate_s, gate_b, up_w, up_s, up_b, h_s1,
                             D, D_ff, group_size);

  int r = compare_fp32(label, h_ref, h_s1, D_ff);
  free(x); free(gate_w); free(gate_s); free(gate_b);
  free(up_w); free(up_s); free(up_b); free(h_ref); free(h_s1);
  return r;
}

int
main(void)
{
  int failed = 0;
  printf("=== mlx2_matmul_s1 vs mlx2_matmul (S=1) ===\n");
  failed += test_plain_matmul("q-proj    D=1024 out=1024 group=64", 1024, 1024, 64);
  failed += test_plain_matmul("kv-proj   D=1024 out=512  group=64", 1024,  512, 64);
  failed += test_plain_matmul("gate/up   D=1024 out=3072 group=64", 1024, 3072, 64);
  failed += test_plain_matmul("down     D=3072 out=1024 group=64",  3072, 1024, 64);
  failed += test_plain_matmul("tiny      D=128  out=64   group=32",  128,   64, 32);

  printf("\n=== gate_up_silu_mlx2_s1 vs gate_up_silu_mlx2 (S=1) ===\n");
  failed += test_fused_silu("mlp       D=1024 D_ff=3072 group=64", 1024, 3072, 64);
  failed += test_fused_silu("tiny      D=128  D_ff=256  group=32",  128,  256, 32);

  if ( failed ) {
    fprintf(stderr, "\n%d configuration(s) failed\n", failed);
    return 1;
  }
  printf("\nALL PASS — S=1 kernels are byte-exact with S=1 calls of the generic kernels.\n");
  return 0;
}
