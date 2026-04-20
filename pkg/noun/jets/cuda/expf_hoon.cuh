/*
 * expf_hoon.cuh — fp32 exp that byte-exactly matches Hoon's rs:math exp
 * (saloon/desk/lib/math.hoon +exp at line ~466).
 *
 * Algorithm (ported verbatim from the Hoon):
 *   1. Handle specials: ±0, ±inf, NaN, overflow/underflow thresholds.
 *   2. Range reduction:  x = k·ln(2) + r,  k integer (round-to-nearest).
 *   3. Horner polynomial for exp(r) with 7 fp32 coefficients stored as
 *      the same exact hex bit-patterns Hoon uses.
 *   4. Multiply by 2^k via exponent-field integer arithmetic.
 *
 * Determinism: every op is IEEE-754 mul or add (separate roundings —
 * never fused).  Constants are raw bit-patterns.  On any IEEE-754-
 * compliant fp32 (CUDA CC ≥ 2.0, any libm soft/hardfloat) the result is
 * identical to Hoon's jetted +exp byte-for-byte.
 */

#ifndef URBIT_CUDA_EXPF_HOON_CUH
#define URBIT_CUDA_EXPF_HOON_CUH

#include <stdint.h>
#include <math.h>

#if defined(__CUDACC__)
  #define EXPFH_DEVICE __host__ __device__ __forceinline__
#else
  #define EXPFH_DEVICE static inline
#endif

/* Bit-pattern → float without type-punning UB.  Use CUDA intrinsics
 * only when compiling *device* code (__CUDA_ARCH__); on host use a
 * union copy regardless of whether the compiler is nvcc or gcc. */
EXPFH_DEVICE float _expfh_u2f(uint32_t u)
{
#if defined(__CUDA_ARCH__)
  return __int_as_float((int)u);
#else
  union { uint32_t u; float f; } v; v.u = u; return v.f;
#endif
}

EXPFH_DEVICE uint32_t _expfh_f2u(float f)
{
#if defined(__CUDA_ARCH__)
  return (uint32_t)__float_as_int(f);
#else
  union { uint32_t u; float f; } v; v.f = f; return v.u;
#endif
}

EXPFH_DEVICE float expf_hoon(float x)
{
  uint32_t xb = _expfh_f2u(x);

  /* Specials (bit-pattern checks — must match Hoon exactly). */
  if ( xb == 0x00000000u || xb == 0x80000000u ) return _expfh_u2f(0x3f800000u); /* ±0 → 1 */
  if ( xb == 0x7f800000u ) return _expfh_u2f(0x7f800000u);  /* +inf */
  if ( xb == 0xff800000u ) return 0.0f;                      /* -inf → +0 */
  if ( (xb & 0x7f800000u) == 0x7f800000u &&
       (xb & 0x007fffffu) != 0u ) {
    return _expfh_u2f(0x7fc00000u);                          /* NaN */
  }

  /* Overflow / underflow thresholds (Hoon's literals). */
  float x_over  = _expfh_u2f(0x42b17218u);  /* 88.72284 */
  float x_under = _expfh_u2f(0xc2aeac50u);  /* -87.33655 */
  if ( x > x_over  ) return _expfh_u2f(0x7f800000u);
  if ( x < x_under ) return 0.0f;

  /* Range reduction: r = x - k·ln(2),  k = round(x / ln(2)). */
  float ln2     = _expfh_u2f(0x3f317218u);
  float invln2  = _expfh_u2f(0x3fb8aa3bu);
  float scaled  = x * invln2;

  /* Hoon's `(need (toi scaled))` uses softfloat's default round-to-
   * nearest-EVEN.  On device use __float2int_rn (round-to-nearest-even).
   * On host, rintf respects the prevailing rounding mode which is
   * FE_TONEAREST (round-to-nearest-even) by default on x86 — matches. */
#if defined(__CUDA_ARCH__)
  int k = __float2int_rn(scaled);
#else
  int k = (int)rintf(scaled);
#endif
  float kf = (float)k;

  /* r = x - kf * ln2  — separate mul and sub, each one IEEE rounding. */
  float mulkln2 = kf * ln2;
  float r       = x - mulkln2;

  /* Horner polynomial: p = ((((((c6*r + c5)*r + c4)*r + c3)*r + c2)*r + c1)*r + c0).
   * Hoon does (add (mul p r) cN) — explicit mul+add, never fused. */
  float p = _expfh_u2f(0x3ab60b61u);                /* 1/720 */
  p = (p * r) + _expfh_u2f(0x3c088889u);            /* 1/120 */
  p = (p * r) + _expfh_u2f(0x3d2aaaabu);            /* 1/24  */
  p = (p * r) + _expfh_u2f(0x3e2aaaabu);            /* 1/6   */
  p = (p * r) + _expfh_u2f(0x3f000000u);            /* 1/2   */
  p = (p * r) + _expfh_u2f(0x3f800000u);            /* 1     */
  p = (p * r) + _expfh_u2f(0x3f800000u);            /* 1     */

  /* Scale p by 2^k via exponent-field add. */
  uint32_t pb = _expfh_f2u(p);
  uint32_t mant_sign = pb & 0x807fffffu;
  uint32_t exp_f     = (pb >> 23) & 0xffu;

  if ( k >= 0 ) {
    uint32_t ne = exp_f + (uint32_t)k;
    if ( ne >= 0xffu ) return _expfh_u2f(0x7f800000u);  /* overflow → +inf */
    return _expfh_u2f(mant_sign | (ne << 23));
  } else {
    uint32_t km = (uint32_t)(-k);
    if ( exp_f < km ) return 0.0f;                      /* underflow → +0 */
    uint32_t ne = exp_f - km;
    if ( ne == 0u ) return 0.0f;
    return _expfh_u2f(mant_sign | (ne << 23));
  }
}

#undef EXPFH_DEVICE

#endif
