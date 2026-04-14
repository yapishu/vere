/// @file  math_rs.c
///
/// Deterministic jets for /lib/math rs door.
///
/// Every float operation goes through SoftFloat, so results are
/// bit-identical across platforms and match the Hoon reference, which
/// performs the same sequence of SoftFloat ops via rs:math.
///
/// Algorithms:
///   exp(x) = 2^k * p(r), where x = k*ln2 + r, |r| <= ln2/2
///            p(r) is a 6th-degree Horner polynomial of 1/n! coefficients
///            2^k is applied by integer addition to the fp32 exponent field
///
///   log(x) = e*ln2 + 2*(f + f^3/3 + f^5/5 + f^7/7 + f^9/9)
///            where x = m * 2^e, 1 <= m < 2, f = (m-1)/(m+1), |f| < 1/3
///            e is extracted from the fp32 exponent field
///
/// The Hoon reference MUST use the identical constants (@rs bit patterns)
/// and identical Horner evaluation order. See /lib/math.hoon ++exp / ++log.

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "softfloat.h"

#include <string.h>

#define SINGNAN 0x7fc00000

  union sing {
    float32_t s;
    c3_w      c;
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

  static inline float32_t
  _bits(uint32_t c)
  {
    union sing u;
    u.c = c;
    return u.s;
  }

  static inline uint32_t
  _word(float32_t s)
  {
    union sing u;
    u.s = s;
    return u.c;
  }

//  Constants as fp32 bit patterns. Must match /lib/math.hoon exactly.
#define C_ONE       0x3f800000u  //  1.0
#define C_ZERO      0x00000000u  //  0.0
#define C_INF_POS   0x7f800000u  //  +inf
#define C_INF_NEG   0xff800000u  //  -inf
#define C_NAN       0x7fc00000u  //  canonical NaN
#define C_LN2       0x3f317218u  //  ln(2) ~ 0.6931472
#define C_INVLN2    0x3fb8aa3bu  //  1/ln(2) ~ 1.442695
#define C_EXP_HI    0x42b17218u  //  88.72283,  exp overflows above this
#define C_EXP_LO    0xc2aeac50u  //  -87.33655, exp underflows below this
#define C_HALF      0x3f000000u  //  0.5
#define C_THIRD     0x3eaaaaabu  //  1/3
#define C_FIFTH     0x3e4ccccdu  //  1/5
#define C_SEVENTH   0x3e124925u  //  1/7
#define C_NINTH     0x3de38e39u  //  1/9
#define C_TWO       0x40000000u  //  2.0

//  exp Taylor-poly coefficients: 1/n! for n=0..6
#define C_E0        0x3f800000u  //  1        = 1/0!
#define C_E1        0x3f800000u  //  1        = 1/1!
#define C_E2        0x3f000000u  //  0.5      = 1/2!
#define C_E3        0x3e2aaaabu  //  0.166... = 1/3!
#define C_E4        0x3d2aaaabu  //  0.041... = 1/4!
#define C_E5        0x3c088889u  //  0.008... = 1/5!
#define C_E6        0x3ab60b61u  //  0.001... = 1/6!

/* exp(x): deterministic, SoftFloat throughout.
*/
  u3_noun
  u3qe_math_rs_exp(u3_atom x, u3_atom r)
  {
    _set_rounding(r);

    uint32_t xb = u3r_word(0, x);

    //  specials: follow Hoon's ++exp ordering exactly
    if ( xb == C_ZERO || xb == (C_ZERO | 0x80000000u) )  return u3i_word(C_ONE);
    if ( xb == C_INF_POS )                               return u3i_word(C_INF_POS);
    if ( xb == C_INF_NEG )                               return u3i_word(C_ZERO);
    //  NaN: any value with exp=0xff and non-zero mantissa
    if ( (xb & 0x7f800000u) == 0x7f800000u
         && (xb & 0x007fffffu) != 0 )                     return u3i_word(C_NAN);

    float32_t xf = _bits(xb);

    //  overflow -> +inf, underflow -> 0
    if ( f32_lt(_bits(C_EXP_HI), xf) )                   return u3i_word(C_INF_POS);
    if ( f32_lt(xf, _bits(C_EXP_LO)) )                   return u3i_word(C_ZERO);

    //  range reduction: k = round_to_nearest_int(x * 1/ln2), r = x - k*ln2
    //  bias: keep the same rounding mode the caller asked for
    float32_t scaled = f32_mul(xf, _bits(C_INVLN2));
    int_fast32_t k = f32_to_i32(scaled, softfloat_roundingMode, false);
    float32_t kf = i32_to_f32((int32_t)k);
    float32_t rr = f32_sub(xf, f32_mul(kf, _bits(C_LN2)));

    //  Horner polynomial in rr:
    //    p = ((((((c6*rr + c5)*rr + c4)*rr + c3)*rr + c2)*rr + c1)*rr + c0
    float32_t p = _bits(C_E6);
    p = f32_add(f32_mul(p, rr), _bits(C_E5));
    p = f32_add(f32_mul(p, rr), _bits(C_E4));
    p = f32_add(f32_mul(p, rr), _bits(C_E3));
    p = f32_add(f32_mul(p, rr), _bits(C_E2));
    p = f32_add(f32_mul(p, rr), _bits(C_E1));
    p = f32_add(f32_mul(p, rr), _bits(C_E0));

    //  multiply by 2^k by adding k to the exponent field.
    //  safe because specials above ensured 2^k * p stays normal.
    uint32_t pb = _word(p);
    int32_t exp_field = (int32_t)((pb >> 23) & 0xff) + (int32_t)k;
    if ( exp_field >= 0xff ) return u3i_word(C_INF_POS);
    if ( exp_field <= 0 )    return u3i_word(C_ZERO);
    pb = (pb & 0x807fffffu) | ((uint32_t)exp_field << 23);

    return u3i_word(_word(_nan_unify(_bits(pb))));
  }

  u3_noun
  u3we_math_rs_exp(u3_noun cor)
  {
    u3_noun a;
    if ( c3n == u3r_mean(cor, u3x_sam, &a, 0) || c3n == u3ud(a) ) {
      return u3m_bail(c3__exit);
    }
    //  rs door sample is [r rtol]; r at axis 60 (head of 30)
    return u3qe_math_rs_exp(a, u3x_at(60, cor));
  }

/* log(x): deterministic, SoftFloat throughout.
**
** x = m * 2^e, 1 <= m < 2
** f = (m-1)/(m+1), |f| < 1/3
** log(x) = e*ln2 + 2*f*(1 + f^2/3 + f^4/5 + f^6/7 + f^8/9)
*/
  u3_noun
  u3qe_math_rs_log(u3_atom x, u3_atom r)
  {
    _set_rounding(r);

    uint32_t xb = u3r_word(0, x);

    //  specials first
    if ( xb == C_ZERO || xb == (C_ZERO | 0x80000000u) )  return u3i_word(C_INF_NEG);
    if ( xb & 0x80000000u )                              return u3i_word(C_NAN);  // negative
    if ( xb == C_INF_POS )                               return u3i_word(C_INF_POS);
    if ( (xb & 0x7f800000u) == 0x7f800000u
         && (xb & 0x007fffffu) != 0 )                     return u3i_word(C_NAN);
    if ( xb == C_ONE )                                   return u3i_word(C_ZERO);

    //  decompose x = m * 2^e, m in [1,2)
    int32_t  e  = (int32_t)((xb >> 23) & 0xff) - 127;
    uint32_t mb = (xb & 0x007fffffu) | 0x3f800000u;  //  m with exponent 0 (i.e. 2^0=1)
    float32_t m = _bits(mb);

    //  f = (m - 1) / (m + 1)
    float32_t m_minus_1 = f32_sub(m, _bits(C_ONE));
    float32_t m_plus_1  = f32_add(m, _bits(C_ONE));
    float32_t f         = f32_div(m_minus_1, m_plus_1);
    float32_t f2        = f32_mul(f, f);

    //  Horner of 1 + f^2/3 + f^4/5 + f^6/7 + f^8/9 in f^2
    float32_t p = _bits(C_NINTH);
    p = f32_add(f32_mul(p, f2), _bits(C_SEVENTH));
    p = f32_add(f32_mul(p, f2), _bits(C_FIFTH));
    p = f32_add(f32_mul(p, f2), _bits(C_THIRD));
    p = f32_add(f32_mul(p, f2), _bits(C_ONE));

    //  log(m) = 2*f*p
    float32_t log_m = f32_mul(_bits(C_TWO), f32_mul(f, p));

    //  log(x) = e*ln2 + log(m)
    float32_t ef = i32_to_f32(e);
    float32_t e_ln2 = f32_mul(ef, _bits(C_LN2));
    float32_t result = f32_add(e_ln2, log_m);

    return u3i_word(_word(_nan_unify(result)));
  }

  u3_noun
  u3we_math_rs_log(u3_noun cor)
  {
    u3_noun a;
    if ( c3n == u3r_mean(cor, u3x_sam, &a, 0) || c3n == u3ud(a) ) {
      return u3m_bail(c3__exit);
    }
    return u3qe_math_rs_log(a, u3x_at(60, cor));
  }

/* eml(x, y) = exp(x) - log(y), composed of the deterministic exp and log.
*/
  u3_noun
  u3qe_math_rs_eml(u3_atom x, u3_atom y, u3_atom r)
  {
    u3_noun ex  = u3qe_math_rs_exp(x, r);
    u3_noun ly  = u3qe_math_rs_log(y, r);
    _set_rounding(r);
    float32_t ef = _bits(u3r_word(0, ex));
    float32_t lf = _bits(u3r_word(0, ly));
    float32_t rs = _nan_unify(f32_sub(ef, lf));
    u3z(ex); u3z(ly);
    return u3i_word(_word(rs));
  }

  u3_noun
  u3we_math_rs_eml(u3_noun cor)
  {
    u3_noun a, b;
    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ||
         c3n == u3ud(a) || c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    return u3qe_math_rs_eml(a, b, u3x_at(60, cor));
  }
