/// @file  math_rs.c
///
/// Jets for /lib/math rs door.
/// eml(x, y) = exp(x) - ln(y)

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "softfloat.h"

#include <math.h>

#define SINGNAN 0x7fc00000

  union sing {
    float32_t s;
    c3_w c;
  };

  static inline c3_t
  _nan_test(float32_t a)
  {
    return !f32_eq(a, a);
  }

  static inline float32_t
  _nan_unify(float32_t a)
  {
    if ( _nan_test(a) )
    {
      *(c3_w*)(&a) = SINGNAN;
    }
    return a;
  }

  static inline void
  _set_rounding(c3_w a)
  {
    switch ( a )
    {
    default:
      u3m_bail(c3__fail);
      break;
    case c3__n:
      softfloat_roundingMode = softfloat_round_near_even;
      break;
    case c3__z:
      softfloat_roundingMode = softfloat_round_minMag;
      break;
    case c3__u:
      softfloat_roundingMode = softfloat_round_max;
      break;
    case c3__d:
      softfloat_roundingMode = softfloat_round_min;
      break;
    }
  }

/* eml: exp(x) - ln(y)
**
** Uses hardware libm for transcendentals, SoftFloat for the subtraction.
** This ensures the final result respects the Urbit rounding mode.
*/
  u3_noun
  u3qe_math_rs_eml(u3_atom x,
                    u3_atom y,
                    u3_atom r)
  {
    union sing xu, yu, expu, logu, res;
    float xf, yf, expf_val, logf_val;

    _set_rounding(r);

    xu.c = u3r_word(0, x);
    yu.c = u3r_word(0, y);

    //  convert to hardware float for transcendentals
    memcpy(&xf, &xu.c, sizeof(float));
    memcpy(&yf, &yu.c, sizeof(float));

    //  compute exp(x) and ln(y) using libm
    expf_val = expf(xf);
    logf_val = logf(yf);

    //  pack back to SoftFloat for the subtraction
    memcpy(&expu.c, &expf_val, sizeof(float));
    memcpy(&logu.c, &logf_val, sizeof(float));

    //  exp(x) - ln(y) via SoftFloat (respects rounding mode)
    res.s = _nan_unify(f32_sub(expu.s, logu.s));

    return u3i_words(1, &res.c);
  }

  u3_noun
  u3we_math_rs_eml(u3_noun cor)
  {
    u3_noun a, b;

    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ||
         c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      //  rounding mode is at axis 30 in the door core
      return u3qe_math_rs_eml(a, b, u3x_at(30, cor));
    }
  }
