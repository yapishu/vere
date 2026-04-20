#include "sgemm_ref.h"
#include <math.h>

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
        acc = fmaf(a[m * K + k], b[k * N + n], acc);
      }
      c[m * N + n] = acc;
    }
  }
}
