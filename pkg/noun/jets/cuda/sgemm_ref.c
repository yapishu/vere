#include "sgemm_ref.h"

/* CPU reference mirrors the GPU kernel exactly: explicit IEEE-754
 * mul then add, never fused.  This matches Hoon's softfloat
 * (add (mul a b) c) byte-for-byte. */
void
sgemm_ref_row_major(const float* a,
                    const float* b,
                    float*       c,
                    size_t       M,
                    size_t       K,
                    size_t       N)
{
  for ( size_t m = 0; m < M; m++ ) {
    for ( size_t n = 0; n < N; n++ ) {
      float acc = 0.0f;
      for ( size_t k = 0; k < K; k++ ) {
        acc = acc + a[m * K + k] * b[k * N + n];
      }
      c[m * N + n] = acc;
    }
  }
}
