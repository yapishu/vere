/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "c3/motes.h"

#include "noun.h"
#include "softfloat.h"
#include "softblas.h"

#include "jets/cuda/backend.h"
#include "jets/cuda/expf_hoon.cuh"

#include <math.h>  // for pow()
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>  // for qsort

#define f16_ceil(a) f16_roundToInt( a, softfloat_round_max, false )
#define f32_ceil(a) f32_roundToInt( a, softfloat_round_max, false )
#define f64_ceil(a) f64_roundToInt( a, softfloat_round_max, false )
#define f128M_ceil(a, b) f128M_roundToInt( a, softfloat_round_max, false, b )

  union half {
    float16_t h;
    c3_s c;
  };

  union sing {
    float32_t s;
    c3_h c;
  };

  union doub {
    float64_t d;
    c3_d c;
  };

  union quad {
    float128_t q;
    c3_d c[2];
  };

  //  $?(%n %u %d %z %a)
  static inline void
  _set_rounding(c3_y a)
  {
    // We could use SoftBLAS set_rounding() to set the SoftFloat
    // mode as well, but it's more explicit to do it here since
    // we may use SoftFloat in any given Lagoon jet and we want
    // you, dear developer, to see it set here.
    switch ( a )
    {
    default:
      u3m_bail(c3__fail);
      break;
    // %n - near
    case c3__n:
      softfloat_roundingMode = softfloat_round_near_even;
      softblas_roundingMode = 'n';
      break;
    // %z - zero
    case c3__z:
      softfloat_roundingMode = softfloat_round_minMag;
      softblas_roundingMode = 'z';
      break;
    // %u - up
    case c3__u:
      softfloat_roundingMode = softfloat_round_max;
      softblas_roundingMode = 'u';
      break;
    // %d - down
    case c3__d:
      softfloat_roundingMode = softfloat_round_min;
      softblas_roundingMode = 'd';
      break;
    // %a - away
    case c3__a:
      softfloat_roundingMode = softfloat_round_near_maxMag;
      softblas_roundingMode = 'a';
      break;
    }
  }

/* length of shape = x * y * z * w * ...
*/
  static inline c3_d _get_length(u3_noun shape)
  {
    c3_d len = 1;
    while (u3_nul != shape) {
      len = len * u3x_atom(u3h(shape));
      shape = u3t(shape);
    }
    return len;
  }

/* get dims from shape as array [x y z w ...]
*/
  static inline c3_d* _get_dims(u3_noun shape)
  {
    u3_atom len = u3qb_lent(shape);
    c3_d len_d = u3r_chub(0, len);
    c3_d* dims = (c3_d*)u3a_malloc(len_d*sizeof(c3_d));
    for (c3_d i = 0; i < len_d; i++) {
      dims[i] = u3r_chub(0, u3x_atom(u3h(shape)));
      shape = u3t(shape);
    }
    u3z(len);
    return dims;
  }

/* check consistency of array shape and bloq size
    |=  =ray
    ^-  ?
    .=  (roll shape.meta.ray ^mul)
    (dec (met bloq.meta.ray data.ray))
*/
  static inline c3_o _check(u3_noun ray)
  {
    //  Calculate expected size.
    u3_atom shp = u3h(u3h(ray));        // (reported) shape of ray, +4
    u3_atom blq = u3h(u3t(u3h(ray)));   // block size of ray, +10
    u3_atom sin = _get_length(shp);     // calculated length of ray

    //  Calculate actual size.
    u3_atom len = u3r_met(blq, u3t(ray));   // length of ray
    u3_atom dex = u3qa_dec(len);            // decrement length b/c of pinned 1

    return __(sin == dex);
  }

/* add - axpy = 1*x+y
*/
  u3_noun
  u3qi_la_add_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq
                   )
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);
    
    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        haxpy(len_x, (float16_t){SB_REAL16_ONE}, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);
        break;

      case 5:
        saxpy(len_x, (float32_t){SB_REAL32_ONE}, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);
        break;

      case 6:
        daxpy(len_x, (float64_t){SB_REAL64_ONE}, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);
        break;

      case 7:
        qaxpy(len_x, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* sub - axpy = -1*y+x
*/
  u3_noun
  u3qi_la_sub_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq
                   )
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);
    
    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        haxpy(len_x, (float16_t){SB_REAL16_NEGONE}, (float16_t*)y_bytes, 1, (float16_t*)x_bytes, 1);
        break;

      case 5:
        saxpy(len_x, (float32_t){SB_REAL32_NEGONE}, (float32_t*)y_bytes, 1, (float32_t*)x_bytes, 1);
        break;

      case 6:
        daxpy(len_x, (float64_t){SB_REAL64_NEGONE}, (float64_t*)y_bytes, 1, (float64_t*)x_bytes, 1);
        break;

      case 7:
        qaxpy(len_x, (float128_t){SB_REAL128L_NEGONE,SB_REAL128U_NEGONE}, (float128_t*)y_bytes, 1, (float128_t*)x_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }


/* mul - x.*y
   elementwise multiplication
*/
  u3_noun
  u3qi_la_mul_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)y_bytes)[i] = f16_mul(((float16_t*)x_bytes)[i], ((float16_t*)y_bytes)[i]);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)y_bytes)[i] = f32_mul(((float32_t*)x_bytes)[i], ((float32_t*)y_bytes)[i]);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)y_bytes)[i] = f64_mul(((float64_t*)x_bytes)[i], ((float64_t*)y_bytes)[i]);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          f128M_mul(&(((float128_t*)y_bytes)[i]), &(((float128_t*)x_bytes)[i]), &(((float128_t*)y_bytes)[i]));
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* div - x/y
   elementwise division
*/
  u3_noun
  u3qi_la_div_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)y_bytes)[i] = f16_div(((float16_t*)x_bytes)[i], ((float16_t*)y_bytes)[i]);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)y_bytes)[i] = f32_div(((float32_t*)x_bytes)[i], ((float32_t*)y_bytes)[i]);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)y_bytes)[i] = f64_div(((float64_t*)x_bytes)[i], ((float64_t*)y_bytes)[i]);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          f128M_div(&(((float128_t*)y_bytes)[i]), &(((float128_t*)x_bytes)[i]), &(((float128_t*)y_bytes)[i]));
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* mod - x % y = x - r*floor(x/r)
   remainder after division
*/
  u3_noun
  u3qi_la_mod_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          // Perform division x/n
          float16_t div_result16 = f16_div(x_val16, y_val16);
          // Compute floor of the division result
          c3_ds floor_result16 = f16_to_i64(div_result16, softfloat_round_minMag, false);
          float16_t floor_float16 = i64_to_f16(floor_result16);
          // Multiply n by floor(x/n)
          float16_t mult_result16 = f16_mul(y_val16, floor_float16);
          // Compute remainder: x - n * floor(x/n)
          ((float16_t*)y_bytes)[i] = f16_sub(x_val16, mult_result16);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          // Perform division x/n
          float32_t div_result32 = f32_div(x_val32, y_val32);
          // Compute floor of the division result
          c3_ds floor_result32 = f32_to_i64(div_result32, softfloat_round_minMag, false);
          float32_t floor_float32 = i64_to_f32(floor_result32);
          // Multiply n by floor(x/n)
          float32_t mult_result32 = f32_mul(y_val32, floor_float32);
          // Compute remainder: x - n * floor(x/n)
          ((float32_t*)y_bytes)[i] = f32_sub(x_val32, mult_result32);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          // Perform division x/n
          float64_t div_result64 = f64_div(x_val64, y_val64);
          // Compute floor of the division result
          c3_ds floor_result64 = f64_to_i64(div_result64, softfloat_round_minMag, false);
          float64_t floor_float64 = i64_to_f64(floor_result64);
          // Multiply n by floor(x/n)
          float64_t mult_result64 = f64_mul(y_val64, floor_float64);
          // Compute remainder: x - n * floor(x/n)
          ((float64_t*)y_bytes)[i] = f64_sub(x_val64, mult_result64);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          // Perform division x/n
          float128_t div_result128;
          f128M_div((float128_t*)&x_val128, (float128_t*)&y_val128, (float128_t*)&div_result128);
          // Compute floor of the division result
          c3_ds floor_result128 = f128M_to_i64(&div_result128, softfloat_round_minMag, false);
          float128_t floor_float128;
          i64_to_f128M(floor_result128, &floor_float128);
          // Multiply n by floor(x/n)
          float128_t mult_result128;
          f128M_mul(((float128_t*)&y_val128), ((float128_t*)&floor_float128), ((float128_t*)&mult_result128));
          // Compute remainder: x - n * floor(x/n)
          f128M_sub(((float128_t*)&x_val128), ((float128_t*)&mult_result128), &(((float128_t*)y_bytes)[i]));
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* cumsum - x[0] + x[1] + ... x[n]
*/
  u3_noun
  u3qi_la_cumsum_i754(u3_noun x_data,
                      u3_noun shape,
                      u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // y_bytes is the data array (w/ leading 0x1, skipped by for range)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t sum16[2];
        sum16[0] = (float16_t){SB_REAL16_ZERO};
        for (c3_d i = len_x; i > 0; i--) {
          sum16[0] = f16_add(sum16[0], ((float16_t*)x_bytes)[i-1]);
        }
        sum16[1].v = 0x1;
        r_data = u3i_bytes((2+1)*sizeof(c3_y), (c3_y*)sum16);
        break;}

      case 5: {
        float32_t sum32[2];
        sum32[0] = (float32_t){SB_REAL32_ZERO};
        for (c3_d i = len_x; i > 0; i--) {
          sum32[0] = f32_add(sum32[0], ((float32_t*)x_bytes)[i-1]);
        }
        sum32[1].v = 0x1;
        r_data = u3i_bytes((4+1)*sizeof(c3_y), (c3_y*)sum32);
        break;}

      case 6: {
        float64_t sum64[2];
        sum64[0] = (float64_t){SB_REAL64_ZERO};
        for (c3_d i = len_x; i > 0; i--) {
          sum64[0] = f64_add(sum64[0], ((float64_t*)x_bytes)[i-1]);
        }
        sum64[1].v = 0x1;
        r_data = u3i_bytes((8+1)*sizeof(c3_y), (c3_y*)sum64);
        break;}

      case 7: {
        float128_t sum128[2];
        sum128[0] = (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        for (c3_d i = len_x; i > 0; i--) {
          f128M_add(&(sum128[0]), &(((float128_t*)x_bytes)[i-1]), &(sum128[0]));
        }
        sum128[1] = (float128_t){0x1, 0x0};
        r_data = u3i_bytes((16+1)*sizeof(c3_y), (c3_y*)sum128);
        break;}
    }

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* argmin - argmin(x)
*/
  u3_noun
  u3qi_la_argmin_i754(u3_noun x_data,
                      u3_noun shape,
                      u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1, which doesn't matter here)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    c3_d min_idx = 0;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t min_val16 = ((float16_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f16_lt(((float16_t*)x_bytes)[i], min_val16)) {
             min_val16 = ((float16_t*)x_bytes)[i];
             min_idx = i;
           }
        }
        break;}

      case 5: {
        float32_t min_val32 = ((float32_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f32_lt(((float32_t*)x_bytes)[i], min_val32)) {
             min_val32 = ((float32_t*)x_bytes)[i];
             min_idx = i;
           }
        }
        break;}

      case 6: {
        float64_t min_val64 = ((float64_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f64_lt(((float64_t*)x_bytes)[i], min_val64)) {
             min_val64 = ((float64_t*)x_bytes)[i];
             min_idx = i;
           }
        }
        break;}

      case 7: {
        float128_t min_val128 = ((float128_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f128M_lt(&(((float128_t*)x_bytes)[i]), &min_val128)) {
             min_val128 = *f128M_min(&min_val128, &((float128_t*)x_bytes)[i]);
             min_idx = i;
           }
        }
        break;}
    }

    u3_noun r_data = u3i_chub(min_idx);

    return r_data;
  }

/* argmax - argmax(x)
*/
  u3_noun
  u3qi_la_argmax_i754(u3_noun x_data,
                      u3_noun shape,
                      u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1, which doesn't matter here)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    c3_d max_idx = 0;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t max_val16 = ((float16_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f16_gt(((float16_t*)x_bytes)[i], max_val16)) {
             max_val16 = ((float16_t*)x_bytes)[i];
             max_idx = i;
           }
        }
        break;}

      case 5: {
        float32_t max_val32 = ((float32_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f32_gt(((float32_t*)x_bytes)[i], max_val32)) {
             max_val32 = ((float32_t*)x_bytes)[i];
             max_idx = i;
           }
        }
        break;}

      case 6: {
        float64_t max_val64 = ((float64_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f64_gt(((float64_t*)x_bytes)[i], max_val64)) {
             max_val64 = ((float64_t*)x_bytes)[i];
             max_idx = i;
           }
        }
        break;}

      case 7: {
        float128_t max_val128 = ((float128_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f128M_gt(&(((float128_t*)x_bytes)[i]), &max_val128)) {
             max_val128 = *f128M_max(&max_val128, &((float128_t*)x_bytes)[i]);
             max_idx = i;
           }
        }
        break;}
    }

    u3_noun r_data = u3i_chub(max_idx);

    return r_data;
  }

/* ravel - x -> ~[x[0], x[1], ... x[n]]
   entire nd-array busted out as a linear list
*/
  u3_noun
  u3qi_la_ravel_i754(u3_noun x_data,
                     u3_noun shape,
                     u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // r_data is the result noun of [data]
    u3_noun r_data = u3_nul;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          r_data = u3nc(u3i_half(x_val16.v), r_data);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          r_data = u3nc(u3i_half(x_val32.v), r_data);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          r_data = u3nc(u3i_chub(x_val64.v), r_data);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          r_data = u3nc(u3i_chubs(2, (c3_d*)&(x_val128.v)), r_data);
        }
        break;
    }

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* min - min(x,y)
*/
  u3_noun
  u3qi_la_min_i754(u3_noun x_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by for range)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t min_val16 = ((float16_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          min_val16 = f16_min(min_val16, ((float16_t*)x_bytes)[i]);
        }
        float16_t r16[2];
        r16[0] = min_val16;
        r16[1].v = 0x1;
        r_data = u3i_bytes((2+1)*sizeof(c3_y), (c3_y*)r16);
        break;}

      case 5: {
        float32_t min_val32 = ((float32_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          min_val32 = f32_min(min_val32, ((float32_t*)x_bytes)[i]);
        }
        float32_t r32[2];
        r32[0] = min_val32;
        r32[1].v = 0x1;
        r_data = u3i_bytes((4+1)*sizeof(c3_y), (c3_y*)r32);
        break;}

      case 6: {
        float64_t min_val64 = ((float64_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          min_val64 = f64_min(min_val64, ((float64_t*)x_bytes)[i]);
        }
        float64_t r64[2];
        r64[0] = min_val64;
        r64[1].v = 0x1;
        r_data = u3i_bytes((8+1)*sizeof(c3_y), (c3_y*)r64);
        break;}

      case 7: {
        float128_t min_val128 = ((float128_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          min_val128 = *f128M_min(&min_val128, &((float128_t*)x_bytes)[i]);
        }
        float128_t r128[2];
        r128[0] = min_val128;
        r128[1] = (float128_t){0x1, 0x0};
        r_data = u3i_bytes((16+1)*sizeof(c3_y), (c3_y*)r128);
        break;}
    }

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* max - max(x,y)
*/
  u3_noun
  u3qi_la_max_i754(u3_noun x_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by for range)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t max_val16 = ((float16_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          max_val16 = f16_max(max_val16, ((float16_t*)x_bytes)[i]);
        }
        float16_t r16[2];
        r16[0] = max_val16;
        r16[1].v = 0x1;
        r_data = u3i_bytes((2+1)*sizeof(c3_y), (c3_y*)r16);
        break;}

      case 5: {
        float32_t max_val32 = ((float32_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          max_val32 = f32_max(max_val32, ((float32_t*)x_bytes)[i]);
        }
        float32_t r32[2];
        r32[0] = max_val32;
        r32[1].v = 0x1;
        r_data = u3i_bytes((4+1)*sizeof(c3_y), (c3_y*)r32);
        break;}

      case 6: {
        float64_t max_val64 = ((float64_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          max_val64 = f64_max(max_val64, ((float64_t*)x_bytes)[i]);
        }
        float64_t r64[2];
        r64[0] = max_val64;
        r64[1].v = 0x1;
        r_data = u3i_bytes((8+1)*sizeof(c3_y), (c3_y*)r64);
        break;}

      case 7: {
        float128_t max_val128 = ((float128_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          max_val128 = *f128M_max(&max_val128, &((float128_t*)x_bytes)[i]);
        }
        float128_t r128[2];
        r128[0] = max_val128;
        r128[1] = (float128_t){0x1, 0x0};
        r_data = u3i_bytes((16+1)*sizeof(c3_y), (c3_y*)r128);
        break;}
    }

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* abs - |x|
*/
  u3_noun
  u3qi_la_abs_i754(u3_noun x_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by for range)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)x_bytes)[i] = f16_abs(((float16_t*)x_bytes)[i]);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)x_bytes)[i] = f32_abs(((float32_t*)x_bytes)[i]);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)x_bytes)[i] = f64_abs(((float64_t*)x_bytes)[i]);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          ((float128_t*)x_bytes)[i] = f128_abs(((float128_t*)x_bytes)[i]);
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* gth - x > y
*/
  u3_noun
  u3qi_la_gth_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          ((float16_t*)y_bytes)[i] = f16_gt(x_val16, y_val16) ? (float16_t){SB_REAL16_ONE} : (float16_t){SB_REAL16_ZERO};
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          ((float32_t*)y_bytes)[i] = f32_gt(x_val32, y_val32) ? (float32_t){SB_REAL32_ONE} : (float32_t){SB_REAL32_ZERO};
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          ((float64_t*)y_bytes)[i] = f64_gt(x_val64, y_val64) ? (float64_t){SB_REAL64_ONE} : (float64_t){SB_REAL64_ZERO};
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          ((float128_t*)y_bytes)[i] = f128M_gt(((float128_t*)&x_val128), ((float128_t*)&y_val128)) ? (float128_t){SB_REAL128L_ONE, SB_REAL128U_ONE} : (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* gte - x > y
*/
  u3_noun
  u3qi_la_gte_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          ((float16_t*)y_bytes)[i] = f16_ge(x_val16, y_val16) ? (float16_t){SB_REAL16_ONE} : (float16_t){SB_REAL16_ZERO};
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          ((float32_t*)y_bytes)[i] = f32_ge(x_val32, y_val32) ? (float32_t){SB_REAL32_ONE} : (float32_t){SB_REAL32_ZERO};
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          ((float64_t*)y_bytes)[i] = f64_ge(x_val64, y_val64) ? (float64_t){SB_REAL64_ONE} : (float64_t){SB_REAL64_ZERO};
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          ((float128_t*)y_bytes)[i] = f128M_ge(((float128_t*)&x_val128), ((float128_t*)&y_val128)) ? (float128_t){SB_REAL128L_ONE, SB_REAL128U_ONE} : (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* lth - x > y
*/
  u3_noun
  u3qi_la_lth_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          ((float16_t*)y_bytes)[i] = f16_lt(x_val16, y_val16) ? (float16_t){SB_REAL16_ONE} : (float16_t){SB_REAL16_ZERO};
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          ((float32_t*)y_bytes)[i] = f32_lt(x_val32, y_val32) ? (float32_t){SB_REAL32_ONE} : (float32_t){SB_REAL32_ZERO};
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          ((float64_t*)y_bytes)[i] = f64_lt(x_val64, y_val64) ? (float64_t){SB_REAL64_ONE} : (float64_t){SB_REAL64_ZERO};
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          ((float128_t*)y_bytes)[i] = f128M_lt(((float128_t*)&x_val128), ((float128_t*)&y_val128)) ? (float128_t){SB_REAL128L_ONE, SB_REAL128U_ONE} : (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* lte - x > y
*/
  u3_noun
  u3qi_la_lte_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          ((float16_t*)y_bytes)[i] = f16_le(x_val16, y_val16) ? (float16_t){SB_REAL16_ONE} : (float16_t){SB_REAL16_ZERO};
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          ((float32_t*)y_bytes)[i] = f32_le(x_val32, y_val32) ? (float32_t){SB_REAL32_ONE} : (float32_t){SB_REAL32_ZERO};
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          ((float64_t*)y_bytes)[i] = f64_le(x_val64, y_val64) ? (float64_t){SB_REAL64_ONE} : (float64_t){SB_REAL64_ZERO};
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          ((float128_t*)y_bytes)[i] = f128M_le(((float128_t*)&x_val128), ((float128_t*)&y_val128)) ? (float128_t){SB_REAL128L_ONE, SB_REAL128U_ONE} : (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* adds - axpy = 1*x+[n]
*/
  u3_noun
  u3qi_la_adds_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    //  Switch on the block size.  We assume that n fits in the target block size; Hoon typecheck should prevent.
    switch (u3x_atom(bloq)) {
      case 4:
        u3r_bytes(0, 2, (c3_y*)&(n16.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)y_bytes)[i] = n16;
        }
        haxpy(len_x, (float16_t){SB_REAL16_ONE}, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);
        break;

      case 5:
        u3r_bytes(0, 4, (c3_y*)&(n32.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)y_bytes)[i] = n32;
        }
        saxpy(len_x, (float32_t){SB_REAL32_ONE}, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);
        break;

      case 6:
        u3r_bytes(0, 8, (c3_y*)&(n64.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)y_bytes)[i] = n64;
        }
        daxpy(len_x, (float64_t){SB_REAL64_ONE}, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);
        break;

      case 7:
        u3r_bytes(0, 16, (c3_y*)&(n128.v[0]), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float128_t*)y_bytes)[i] = (float128_t){n128.v[0], n128.v[1]};
        }
        qaxpy(len_x, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    y_bytes[syz_x] = 0x1;  // pin head
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* subs - axpy = -1*[n]+x
*/
  u3_noun
  u3qi_la_subs_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/o leading 0x1)
    c3_y* y_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    //  Switch on the block size.  We assume that n fits in the target block size; Hoon typecheck should prevent.
    switch (u3x_atom(bloq)) {
      case 4:
        u3r_bytes(0, 2, (c3_y*)&(n16.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)y_bytes)[i] = n16;
        }
        haxpy(len_x, (float16_t){SB_REAL16_NEGONE}, (float16_t*)y_bytes, 1, (float16_t*)x_bytes, 1);
        break;

      case 5:
        u3r_bytes(0, 4, (c3_y*)&(n32.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)y_bytes)[i] = n32;
        }
        saxpy(len_x, (float32_t){SB_REAL32_NEGONE}, (float32_t*)y_bytes, 1, (float32_t*)x_bytes, 1);
        break;

      case 6:
        u3r_bytes(0, 8, (c3_y*)&(n64.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)y_bytes)[i] = n64;
        }
        daxpy(len_x, (float64_t){SB_REAL64_NEGONE}, (float64_t*)y_bytes, 1, (float64_t*)x_bytes, 1);
        break;

      case 7:
        u3r_bytes(0, 16, (c3_y*)&(n128.v[0]), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float128_t*)y_bytes)[i] = (float128_t){n128.v[0], n128.v[1]};
        }
        qaxpy(len_x, (float128_t){SB_REAL128L_NEGONE,SB_REAL128U_NEGONE}, (float128_t*)y_bytes, 1, (float128_t*)x_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    x_bytes[syz_x] = 0x1;  // pin head
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* muls - ?scal n * x
   elementwise multiplication
*/
  u3_noun
  u3qi_la_muls_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);
    x_bytes[syz_x] = 0x1;  // pin head

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        u3r_bytes(0, 2, (c3_y*)&(n16.v), n);
        hscal(len_x, n16, (float16_t*)x_bytes, 1);
        break;

      case 5:
        u3r_bytes(0, 4, (c3_y*)&(n32.v), n);
        sscal(len_x, n32, (float32_t*)x_bytes, 1);
        break;

      case 6:
        u3r_bytes(0, 8, (c3_y*)&(n64.v), n);
        dscal(len_x, n64, (float64_t*)x_bytes, 1);
        break;

      case 7:
        u3r_bytes(0, 16, (c3_y*)&(n128.v[0]), n);
        qscal(len_x, n128, (float128_t*)x_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* divs - ?scal 1/n * x
   elementwise division
*/
  u3_noun
  u3qi_la_divs_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);
    x_bytes[syz_x] = 0x1;  // pin head

    float16_t in16;
    float32_t in32;
    float64_t in64;
    float128_t in128;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        //  XX note that in16 is doing double duty here
        u3r_bytes(0, 2, (c3_y*)&(in16.v), n);
        in16 = f16_div((float16_t){SB_REAL16_ONE}, in16);
        hscal(len_x, in16, (float16_t*)x_bytes, 1);
        break;

      case 5:
        //  XX note that in32 is doing double duty here
        u3r_bytes(0, 4, (c3_y*)&(in32.v), n);
        in32 = f32_div((float32_t){SB_REAL32_ONE}, in32);
        sscal(len_x, in32, (float32_t*)x_bytes, 1);
        break;

      case 6:
        //  XX note that in64 is doing double duty here
        u3r_bytes(0, 8, (c3_y*)&(in64.v), n);
        in64 = f64_div((float64_t){SB_REAL64_ONE}, in64);
        dscal(len_x, in64, (float64_t*)x_bytes, 1);
        break;

      case 7:
        //  XX note that in128 is doing double duty here
        u3r_bytes(0, 16, (c3_y*)&(in128.v[0]), n);
        f128M_div(&((float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}), &in128, &in128);
        qscal(len_x, in128, (float128_t*)x_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* mods - x % [n] = x - r*floor(x/r)
   remainder after scalar division
*/
  u3_noun
  u3qi_la_mods_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    // we reuse it for results for parsimony
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    float16_t n16, in16;
    float32_t n32, in32;
    float64_t n64, in64;
    float128_t n128, in128;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        u3r_bytes(0, 2, (c3_y*)&(n16.v), n);
        in16 = f16_div((float16_t){SB_REAL16_ONE}, n16);

        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          // Perform division x/n
          float16_t div_result16 = f16_mul(in16, x_val16);
          // Compute floor of the division result
          c3_ds floor_result16 = f16_to_i64(div_result16, softfloat_round_minMag, false);
          float16_t floor_float16 = i64_to_f16(floor_result16);
          // Multiply n by floor(x/n)
          float16_t mult_result16 = f16_mul(n16, floor_float16);
          // Compute remainder: x - n * floor(x/n)
          ((float16_t*)x_bytes)[i] = f16_sub(x_val16, mult_result16);
        }
        break;

      case 5:
        u3r_bytes(0, 4, (c3_y*)&(n32.v), n);
        in32 = f32_div((float32_t){SB_REAL32_ONE}, n32);

        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          // Perform division x/n
          float32_t div_result32 = f32_mul(in32, x_val32);
          // Compute floor of the division result
          c3_ds floor_result32 = f32_to_i64(div_result32, softfloat_round_minMag, false);
          float32_t floor_float32 = i64_to_f32(floor_result32);
          // Multiply n by floor(x/n)
          float32_t mult_result32 = f32_mul(n32, floor_float32);
          // Compute remainder: x - n * floor(x/n)
          ((float32_t*)x_bytes)[i] = f32_sub(x_val32, mult_result32);
        }
        break;

      case 6:
        u3r_bytes(0, 8, (c3_y*)&(n64.v), n);
        in64 = f64_div((float64_t){SB_REAL64_ONE}, n64);

        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          // Perform division x/n
          float64_t div_result64 = f64_mul(in64, x_val64);
          // Compute floor of the division result
          c3_ds floor_result64 = f64_to_i64(div_result64, softfloat_round_minMag, false);
          float64_t floor_float64 = i64_to_f64(floor_result64);
          // Multiply n by floor(x/n)
          float64_t mult_result64 = f64_mul(n64, floor_float64);
          // Compute remainder: x - n * floor(x/n)
          ((float64_t*)x_bytes)[i] = f64_sub(x_val64, mult_result64);
        }
        break;

      case 7:
        u3r_bytes(0, 16, (c3_y*)&(n128.v[0]), n);
        f128M_div(&((float128_t){SB_REAL128L_ONE,SB_REAL128U_ZERO}), &n128, &in128);

        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          // Perform division x/n
          float128_t div_result128;
          f128M_mul((float128_t*)&in128, (float128_t*)&x_val128, (float128_t*)&div_result128);
          // Compute floor of the division result
          c3_ds floor_result128 = f128M_to_i64(&div_result128, softfloat_round_minMag, false);
          float128_t floor_float128;
          i64_to_f128M(floor_result128, &floor_float128);
          // Multiply n by floor(x/n)
          float128_t mult_result128;
          f128M_mul(((float128_t*)&n128), ((float128_t*)&floor_float128), ((float128_t*)&mult_result128));
          // Compute remainder: x - n * floor(x/n)
          f128M_sub(((float128_t*)&x_val128), ((float128_t*)&mult_result128), &(((float128_t*)x_bytes)[i]));
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* dot - ?dot = x · y
*/
  u3_noun
  u3qi_la_dot_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t r16[2];
        r16[0] = hdot(len_x, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);
        r16[1].v = 0x1;
        r_data = u3i_bytes((2+1)*sizeof(c3_y), (c3_y*)r16);
        break;}

      case 5: {
        float32_t r32[2];
        r32[0] = sdot(len_x, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);
        r32[1].v = 0x1;
        r_data = u3i_bytes((4+1)*sizeof(c3_y), (c3_y*)r32);
        break;}

      case 6: {
        float64_t r64[2];
        r64[0] = ddot(len_x, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);
        r64[1].v = 0x1;
        r_data = u3i_bytes((8+1)*sizeof(c3_y), (c3_y*)r64);
        break;}

      case 7: {
        float128_t r128[2];
        r128[0] = qdot(len_x, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);
        r128[1] = (float128_t){0x1, 0x0};
        r_data = u3i_bytes((16+1)*sizeof(c3_y), (c3_y*)r128);
        break;}
    }

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* diag - diag(x)
*/
  u3_noun
  u3qi_la_diag(u3_noun x_data,
               u3_noun shape,
               u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }
    //  Assert length of dims is 2.
    if (u3qb_lent(shape) != 2) {
      return u3m_bail(c3__exit);
    }
    //  Unpack shape into an array of dimensions.
    c3_d *dims = _get_dims(shape);
    if (dims[0] != dims[1]) {
      return u3m_bail(c3__exit);
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    c3_d len_x = _get_length(shape);
    c3_d syz_x = len_x * pow(2, bloq - 3);
    c3_d wyd = pow(2, bloq - 3);
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);
    c3_d syz_y = wyd * dims[1];
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_y+1)*sizeof(c3_y));

    u3_noun r_data;

    // Grab the index at i*n_x+j in bytes; put it at j.
    for (c3_d i = 0; i < dims[1]; i++) {
      // Scan across whole field width.
      for (c3_y k = 0; k < wyd; k++) {
        y_bytes[i*wyd+k] = x_bytes[(i*dims[0]+i)*wyd+k];
      }
    }
    y_bytes[syz_y] = 0x1;  // pin head

    //  Unpack the result back into a noun.
    r_data = u3i_bytes((syz_y+1)*sizeof(c3_y), y_bytes);
    
    u3a_free(x_bytes);
    u3a_free(y_bytes);
    u3a_free(dims);

    return r_data;
  }

/* transpose - x'
*/
  u3_noun
  u3qi_la_transpose(u3_noun x_data,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Assert length of dims is 2.
    if (u3qb_lent(shape) != 2) {
      return u3m_bail(c3__exit);
    }
    //  Unpack shape into an array of dimensions.
    c3_d *dims = _get_dims(shape);

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    c3_d len_x = _get_length(shape);
    c3_d syz_x = len_x * pow(2, bloq - 3);
    c3_d wyd = pow(2, bloq - 3);
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));

    u3_noun r_data;

    // Grab the index at i*n_x+j in bytes; put it at j.
    for (c3_d i = 0; i < dims[1]; i++) {
      for (c3_d j = 0; j < dims[0]; j++) {
        // Scan across whole field width.
        for (c3_y k = 0; k < wyd; k++) {
          y_bytes[(j*dims[1]+i)*wyd+k] = x_bytes[(i*dims[0]+j)*wyd+k];
        }
      }
    }
    y_bytes[syz_x] = 0x1;  // pin head

    //  Unpack the result back into a noun.
    r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    u3a_free(x_bytes);
    u3a_free(y_bytes);
    u3a_free(dims);

    return r_data;
  }

/* linspace - [a a+(b-a)/n ... b]
*/
  u3_noun
  u3qi_la_linspace_i754(u3_noun a,
                        u3_noun b,
                        u3_noun n,
                        u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    u3_noun r_data;

    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t a16, b16;
        u3r_bytes(0, 2, (c3_y*)&(a16.v), a);
        u3r_bytes(0, 2, (c3_y*)&(b16.v), b);
        float16_t span16 = f16_sub(b16, a16);
        float16_t interval16 = f16_div(span16, i32_to_f16(n-1));
        c3_y* x_bytes16 = (c3_y*)u3a_malloc((n*2+1)*sizeof(c3_y));
        for (c3_d i = 1; i < n-1; i++) {
          ((float16_t*)x_bytes16)[i] = f16_add(a16, f16_mul(i32_to_f16(i), interval16));
        }
        //  Assign in reverse order so that n=1 case is correctly left-hand bound.
        ((float16_t*)x_bytes16)[n-1] = b16;
        ((float16_t*)x_bytes16)[0] = a16;
        x_bytes16[n*2] = 0x1;  // pin head
        r_data = u3i_bytes((n*2+1)*sizeof(c3_y), x_bytes16);
        u3a_free(x_bytes16);
        break;}
      
      case 5: {
        float32_t a32, b32;
        u3r_bytes(0, 4, (c3_y*)&(a32.v), a);
        u3r_bytes(0, 4, (c3_y*)&(b32.v), b);
        float32_t span32 = f32_sub(b32, a32);
        float32_t interval32 = f32_div(span32, i32_to_f32(n-1));
        c3_y* x_bytes32 = (c3_y*)u3a_malloc((n*4+1)*sizeof(c3_y));
        for (c3_d i = 1; i < n-1; i++) {
          ((float32_t*)x_bytes32)[i] = f32_add(a32, f32_mul(i32_to_f32(i), interval32));
        }
        ((float32_t*)x_bytes32)[n-1] = b32;
        ((float32_t*)x_bytes32)[0] = a32;
        x_bytes32[n*4] = 0x1;  // pin head
        r_data = u3i_bytes((n*4+1)*sizeof(c3_y), x_bytes32);
        u3a_free(x_bytes32);
        break;}

      case 6: {
        float64_t a64, b64;
        u3r_bytes(0, 8, (c3_y*)&(a64.v), a);
        u3r_bytes(0, 8, (c3_y*)&(b64.v), b);
        float64_t span64 = f64_sub(b64, a64);
        float64_t interval64 = f64_div(span64, i32_to_f64(n-1));
        c3_y* x_bytes64 = (c3_y*)u3a_malloc((n*8+1)*sizeof(c3_y));
        for (c3_d i = 1; i < n-1; i++) {
          ((float64_t*)x_bytes64)[i] = f64_add(a64, f64_mul(i32_to_f64(i), interval64));
        }
        ((float64_t*)x_bytes64)[n-1] = b64;
        ((float64_t*)x_bytes64)[0] = a64;
        x_bytes64[n*8] = 0x1;  // pin head
        r_data = u3i_bytes((n*8+1)*sizeof(c3_y), x_bytes64);
        u3a_free(x_bytes64);
        break;}
      
      case 7: {
        float128_t a128, b128;
        u3r_bytes(0, 16, (c3_y*)&(a128.v[0]), a);
        u3r_bytes(0, 16, (c3_y*)&(b128.v[0]), b);
        float128_t span128;
        f128M_sub(&b128, &a128, &span128);
        float128_t interval128;
        float128_t n128;
        i32_to_f128M(n-1, &n128);
        f128M_div(&span128, &n128, &interval128);
        c3_y* x_bytes128 = (c3_y*)u3a_malloc((n*16+1)*sizeof(c3_y));
        float128_t i128;
        for (c3_d i = 1; i < n-1; i++) {
          i32_to_f128M(i, &i128);
          f128M_mul(&i128, &interval128, &((float128_t*)x_bytes128)[i]);
          f128M_add(&a128, &((float128_t*)x_bytes128)[i], &((float128_t*)x_bytes128)[i]);
        }
        ((float128_t*)x_bytes128)[n-1] = b128;
        ((float128_t*)x_bytes128)[0] = a128;
        x_bytes128[n*16] = 0x1;  // pin head
        r_data = u3i_bytes((n*16+1)*sizeof(c3_y), x_bytes128);
        u3a_free(x_bytes128);
        break;}
    }

    return r_data;
  }

/* range - [a a+d ... b]
*/
  u3_noun
  u3qi_la_range_i754(u3_noun a,
                     u3_noun b,
                     u3_noun d,
                     u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    u3_noun r_data;

    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t a16, b16, interval16;
        u3r_bytes(0, 2, (c3_y*)&(a16.v), a);
        u3r_bytes(0, 2, (c3_y*)&(b16.v), b);
        u3r_bytes(0, 2, (c3_y*)&(interval16.v), d);
        c3_d n16 = f16_to_i64(f16_ceil(f16_div(f16_sub(b16, a16), interval16)), softfloat_round_minMag, false);
        c3_y* x_bytes16 = (c3_y*)u3a_malloc(((n16+1)*2)*sizeof(c3_y));
        ((float16_t*)x_bytes16)[0] = a16;
        for (c3_d i = 1; i < n16; i++) {
          ((float16_t*)x_bytes16)[i] = f16_add(a16, f16_mul(i32_to_f16(i), interval16));
        }
        ((float16_t*)x_bytes16)[n16].v = 0x1;  // pin head
        r_data = u3i_bytes(((n16+1)*2)*sizeof(c3_y), x_bytes16);
        u3a_free(x_bytes16);
        break;}
      
      case 5: {
        float32_t a32, b32, interval32;
        u3r_bytes(0, 4, (c3_y*)&(a32.v), a);
        u3r_bytes(0, 4, (c3_y*)&(b32.v), b);
        u3r_bytes(0, 4, (c3_y*)&(interval32.v), d);
        c3_d n32 = f32_to_i64(f32_ceil(f32_div(f32_sub(b32, a32), interval32)), softfloat_round_minMag, false);
        c3_y* x_bytes32 = (c3_y*)u3a_malloc(((n32+1)*4)*sizeof(c3_y));
        ((float32_t*)x_bytes32)[0] = a32;
        for (c3_d i = 1; i < n32; i++) {
          ((float32_t*)x_bytes32)[i] = f32_add(a32, f32_mul(i32_to_f32(i), interval32));
        }
        ((float32_t*)x_bytes32)[n32].v = 0x1;  // pin head
        r_data = u3i_bytes(((n32+1)*4)*sizeof(c3_y), x_bytes32);
        u3a_free(x_bytes32);
        break;}

      case 6: {
        float64_t a64, b64, interval64;
        u3r_bytes(0, 8, (c3_y*)&(a64.v), a);
        u3r_bytes(0, 8, (c3_y*)&(b64.v), b);
        u3r_bytes(0, 8, (c3_y*)&(interval64.v), d);
        c3_d n64 = f64_to_i64(f64_ceil(f64_div(f64_sub(b64, a64), interval64)), softfloat_round_minMag, false);
        c3_y* x_bytes64 = (c3_y*)u3a_malloc(((n64+1)*8)*sizeof(c3_y));
        ((float64_t*)x_bytes64)[0] = a64;
        for (c3_d i = 1; i < n64; i++) {
          ((float64_t*)x_bytes64)[i] = f64_add(a64, f64_mul(i32_to_f64(i), interval64));
        }
        ((float64_t*)x_bytes64)[n64].v = 0x1;  // pin head
        r_data = u3i_bytes(((n64+1)*8)*sizeof(c3_y), x_bytes64);
        u3a_free(x_bytes64);
        break;}
      
      case 7: {
        float128_t a128, b128, interval128;
        u3r_bytes(0, 16, (c3_y*)&(a128.v[0]), a);
        u3r_bytes(0, 16, (c3_y*)&(b128.v[0]), b);
        u3r_bytes(0, 16, (c3_y*)&(interval128.v[0]), d);
        float128_t tmp;
        f128M_sub(&b128, &a128, &tmp);
        f128M_div(&tmp, &interval128, &tmp);
        f128M_ceil(&tmp, &tmp);
        c3_d n128 = f128M_to_i64(&tmp, softfloat_round_minMag, false);
        c3_y* x_bytes128 = (c3_y*)u3a_malloc(((n128+1)*16)*sizeof(c3_y));
        float128_t i128;
        ((float128_t*)x_bytes128)[0] = a128;
        for (c3_d i = 1; i < n128; i++) {
          i32_to_f128M(i, &i128);
          f128M_mul(&i128, &interval128, &((float128_t*)x_bytes128)[i]);
          f128M_add(&a128, &((float128_t*)x_bytes128)[i], &((float128_t*)x_bytes128)[i]);
        }
        ((float128_t*)x_bytes128)[n128].v[0] = 0x1;  // pin head
        ((float128_t*)x_bytes128)[n128].v[1] = 0x0;  // pin head
        r_data = u3i_bytes(((n128+1)*16)*sizeof(c3_y), x_bytes128);
        u3a_free(x_bytes128);
        break;}
    }

    return r_data;
  }

/* trace - tr(x)
*/
  u3_noun
  u3qi_la_trace_i754(u3_noun x_data,
                     u3_noun shape,
                     u3_noun bloq)
  {
    u3_noun d_data = u3qi_la_diag(x_data, shape, bloq);
    c3_d len_x0 = _get_dims(shape)[0];
    u3_noun r_data = u3qi_la_dot_i754(d_data, d_data, u3nt(len_x0, 0x1, u3_nul), u3k(bloq));
    return r_data;
  }

/* mmul
*/
  u3_noun
  u3qi_la_mmul_i754(u3_noun x_data,
                    u3_noun y_data,
                    u3_noun x_shape,
                    u3_noun y_shape,
                    u3_noun bloq)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    c3_d M = u3x_atom(u3h(x_shape));
    c3_d Na= u3x_atom(u3h(u3t(x_shape)));
    c3_d Nb= u3x_atom(u3h(y_shape));
    c3_d P = u3x_atom(u3h(u3t(y_shape)));

    if ((u3_nul != u3t(u3t(x_shape))) ||
        (u3_nul != u3t(u3t(y_shape))) ||
        (Na != Nb)) {
      return u3m_bail(c3__exit);
    }
    c3_d N = Na;

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(x_shape);    // M*N

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);  // M*N

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // len_x is length in base units
    c3_d len_y = _get_length(y_shape);    // N*P

    // syz_x is length in bytes
    c3_d syz_y = len_y * pow(2, bloq-3);  // N*P

    // y_bytes is the data array (w/o leading 0x1)
    c3_y* y_bytes = (c3_y*)u3a_malloc(syz_y*sizeof(c3_y));
    u3r_bytes(0, syz_y, y_bytes, y_data);
    
    // len_r is length in base units
    c3_d len_r = M*P;                     // M*P

    // syz_r is length in bytes
    c3_d syz_r = len_r * pow(2, bloq-3);  // M*P

    // r_bytes is the result array
    c3_y* r_bytes = (c3_y*)u3a_malloc((syz_r+1)*sizeof(c3_y));
    r_bytes[syz_r] = 0x1;  // pin head
    // initialize with 0x0s
    for (c3_d i = 0; i < syz_r; i++) {
      r_bytes[i] = 0x0;
    }

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        hgemm('N', 'N', M, N, P, (float16_t){SB_REAL16_ONE}, (float16_t*)x_bytes, N, (float16_t*)y_bytes, P, (float16_t){SB_REAL16_ZERO}, (float16_t*)r_bytes, P);
        break;

      case 5: {
        //  Try GPU first (bit-exact against softblas-free CPU ref; softblas
        //  itself may not match exactly, but both are deterministic per-call
        //  so output is stable either way for Nock consensus).  On any GPU
        //  failure, fall through to softblas.
        //
        //  b_hash is the mug of y_data (right-hand weight), which is usually
        //  the large constant weight matrix in transformer inference.
        c3_w y_mug = u3r_mug(y_data);
        backend_status bs = backend_mmul_fp32(
          x_bytes, y_bytes, r_bytes, M, N, P, (uint32_t)y_mug);
        if ( bs == BACKEND_OK ) break;
        sgemm('N', 'N', M, N, P, (float32_t){SB_REAL32_ONE}, (float32_t*)x_bytes, N, (float32_t*)y_bytes, P, (float32_t){SB_REAL32_ZERO}, (float32_t*)r_bytes, P);
        break;
      }

      case 6:
        dgemm('N', 'N', M, N, P, (float64_t){SB_REAL64_ONE}, (float64_t*)x_bytes, N, (float64_t*)y_bytes, P, (float64_t){SB_REAL64_ZERO}, (float64_t*)r_bytes, P);
        break;

      case 7:
        qgemm('N', 'N', M, N, P, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)x_bytes, N, (float128_t*)y_bytes, P, (float128_t){SB_REAL128L_ZERO,SB_REAL128U_ZERO}, (float128_t*)r_bytes, P);
        break;
    }

    //  Unpack the result back into a noun.
    u3_noun r_data = u3i_bytes(syz_r+1, r_bytes);
    u3_noun M_ = u3i_chub(M);
    u3_noun P_ = u3i_chub(P);

    u3a_free(x_bytes);
    u3a_free(y_bytes);
    u3a_free(r_bytes);

    return u3nc(u3nq(u3nt(M_, P_, u3_nul), u3k(bloq), c3__i754, u3_nul), r_data);
  }

  u3_noun
  u3wi_la_add(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_add_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_sub(u3_noun cor)
  {
      // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_sub_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_mul(u3_noun cor)
  {
      // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_mul_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_div(u3_noun cor)
  {
      // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_div_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_mod(u3_noun cor)
  {
      // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_mod_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_cumsum(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_cumsum_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3nc(0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_argmin(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_argmin_i754(x_data, x_shape, x_bloq);
            // bare atom (@ index)
            return r_data;}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_ravel(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_ravel_i754(x_data, x_shape, x_bloq);
            // (list @)
            return r_data;}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_argmax(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_argmax_i754(x_data, x_shape, x_bloq);
            // bare atom (@ index)
            return r_data;}

          default:
            return u3_none;
        }
      }
    }
  }

/* dequant-mlx2 - MLX 2-bit packed weight -> fp32 ray (produces
   [in, out] shape, i.e. already transposed for mmul).
   Pure-C dequant for the maroon Qwen3 / Llama family loader.

     w shape:       [out, in/16]   uint32 (16 int2 per word, LSB-first)
     scales/biases: [out, in/G]    fp32 (gguf2jam promotes from fp16)
     out shape:     [in, out]      fp32   <-- transposed
   Per-element: fp[i, o] = scales[o, i/G] * ((w[o, i/16] >> ((i%16)*2)) & 3)
                           + biases[o, i/G]
*/
  u3_noun
  u3qi_la_dequant_mlx2(u3_noun w_data,
                       u3_noun w_shape,
                       u3_noun s_data,
                       u3_noun b_data,
                       u3_noun grp_atom)
  {
    if ( c3n == u3a_is_cat(grp_atom) ) {
      return u3m_bail(c3__exit);
    }
    c3_w group = u3x_atom(grp_atom);
    if ( group == 0 ) return u3m_bail(c3__exit);

    //  w_shape is [out, packed_cols, ~]. Read first two dims.
    u3_noun out_atom = u3h(w_shape);
    u3_noun rest1    = u3t(w_shape);
    u3_noun pcols_atom = u3h(rest1);
    if ( c3n == u3a_is_cat(out_atom) || c3n == u3a_is_cat(pcols_atom) ) {
      return u3m_bail(c3__exit);
    }
    c3_w out_features = u3x_atom(out_atom);
    c3_w packed_cols  = u3x_atom(pcols_atom);
    c3_w in_features  = packed_cols * 16;
    c3_w groups_per_row = in_features / group;
    c3_d total = (c3_d)out_features * (c3_d)in_features;

    //  Materialize input bytes off-loom. MLX2 data is always 4-byte words
    //  (packed uint32 for w, fp32 for s/b), so use c3_h (uint32_t) rather
    //  than c3_w — which is 8 bytes in VERE64 and would stride wrong.
    c3_d w_bytes = (c3_d)out_features * (c3_d)packed_cols * 4;
    c3_d s_bytes = (c3_d)out_features * (c3_d)groups_per_row * 4;
    c3_h* w_buf = (c3_h*)u3a_malloc(w_bytes);
    c3_h* s_buf = (c3_h*)u3a_malloc(s_bytes);
    c3_h* b_buf = (c3_h*)u3a_malloc(s_bytes);
    u3r_bytes(0, w_bytes, (c3_y*)w_buf, w_data);
    u3r_bytes(0, s_bytes, (c3_y*)s_buf, s_data);
    u3r_bytes(0, s_bytes, (c3_y*)b_buf, b_data);

    //  Output: total fp32 vals in [in, out] layout (row = input idx i,
    //  col = output neuron o). +1 byte MSB pin.
    c3_d out_bytes = total * 4;
    c3_y* out_buf = (c3_y*)u3a_malloc(out_bytes + 1);

    for ( c3_w o = 0; o < out_features; o++ ) {
      c3_w gpr_offset = o * groups_per_row;
      for ( c3_w wc = 0; wc < packed_cols; wc++ ) {
        c3_h word = w_buf[o * packed_cols + wc];
        c3_w i_base = wc * 16;
        for ( c3_w k = 0; k < 16; k++ ) {
          c3_w i = i_base + k;
          c3_w grp = i / group;
          float scale = ((float*)s_buf)[gpr_offset + grp];
          float bias  = ((float*)b_buf)[gpr_offset + grp];
          c3_h q = (word >> (k * 2)) & 0x3;
          float val = scale * (float)q + bias;
          //  Emit at transposed position: row=i, col=o in [in_features, out_features].
          ((float*)out_buf)[(c3_d)i * out_features + o] = val;
        }
      }
    }

    out_buf[out_bytes] = 0x01;

    u3_noun r_data = u3i_bytes(out_bytes + 1, out_buf);

    u3a_free(w_buf);
    u3a_free(s_buf);
    u3a_free(b_buf);
    u3a_free(out_buf);
    return r_data;
  }

/* logits-tied-mlx2 - [1, vocab] = x @ dequant(wte).T, where wte is mlx2-packed.
   Streams over vocab dequanting one row at a time, computing dot(x, wte[v])
   and writing scalar to output. Avoids materializing the full fp32 wte tensor.
   Produces [vocab] fp32 atom (~600 KB for Qwen3 1.7B instead of 1.2 GB).
*/
  u3_noun
  u3qi_la_logits_tied_mlx2(u3_noun x_data,      //  [d_model] fp32 (the last row's embedding)
                           u3_noun x_shape,
                           u3_noun w_data,      //  [vocab, d_model/16] uint32
                           u3_noun w_shape,
                           u3_noun s_data,      //  [vocab, d_model/G] fp32
                           u3_noun b_data,      //  [vocab, d_model/G] fp32
                           u3_noun grp_atom)
  {
    if ( c3n == u3a_is_cat(grp_atom) ) return u3m_bail(c3__exit);
    c3_w group = u3x_atom(grp_atom);
    if ( group == 0 ) return u3m_bail(c3__exit);

    u3_noun vocab_atom = u3h(w_shape);
    u3_noun pcols_atom = u3h(u3t(w_shape));
    c3_w vocab       = u3x_atom(vocab_atom);
    c3_w packed_cols = u3x_atom(pcols_atom);
    c3_w d_model     = packed_cols * 16;
    c3_w groups_per_row = d_model / group;

    c3_d w_bytes = (c3_d)vocab * (c3_d)packed_cols * 4;
    c3_d s_bytes = (c3_d)vocab * (c3_d)groups_per_row * 4;
    c3_d x_bytes = (c3_d)d_model * 4;

    //  Use c3_h (uint32_t) for packed-word buffer; c3_w would stride wrong on VERE64.
    c3_h* w_buf = (c3_h*)u3a_malloc(w_bytes);
    c3_h* s_buf = (c3_h*)u3a_malloc(s_bytes);
    c3_h* b_buf = (c3_h*)u3a_malloc(s_bytes);
    float* x_buf = (float*)u3a_malloc(x_bytes);

    u3r_bytes(0, w_bytes, (c3_y*)w_buf, w_data);
    u3r_bytes(0, s_bytes, (c3_y*)s_buf, s_data);
    u3r_bytes(0, s_bytes, (c3_y*)b_buf, b_data);
    u3r_bytes(0, x_bytes, (c3_y*)x_buf, x_data);

    //  Output: [vocab] fp32 + 1 byte pin.
    c3_d out_bytes = (c3_d)vocab * 4;
    c3_y* out_buf = (c3_y*)u3a_malloc(out_bytes + 1);

    for ( c3_w v = 0; v < vocab; v++ ) {
      c3_w gpr_offset = v * groups_per_row;
      float acc = 0.0f;
      for ( c3_w wc = 0; wc < packed_cols; wc++ ) {
        c3_h word = w_buf[v * packed_cols + wc];
        c3_w i_base = wc * 16;
        for ( c3_w k = 0; k < 16; k++ ) {
          c3_w i = i_base + k;
          c3_w grp = i / group;
          float scale = ((float*)s_buf)[gpr_offset + grp];
          float bias  = ((float*)b_buf)[gpr_offset + grp];
          c3_h q = (word >> (k * 2)) & 0x3;
          float w_fp = scale * (float)q + bias;
          acc += x_buf[i] * w_fp;
        }
      }
      ((float*)out_buf)[v] = acc;
    }

    out_buf[out_bytes] = 0x01;
    u3_noun r_data = u3i_bytes(out_bytes + 1, out_buf);

    u3a_free(w_buf);
    u3a_free(s_buf);
    u3a_free(b_buf);
    u3a_free(x_buf);
    u3a_free(out_buf);
    return r_data;
  }

  u3_noun
  u3wi_la_logits_tied_mlx2(u3_noun cor)
  {
    //  Sample = [x=ray wte-w=ray wte-s=ray wte-b=ray group-size=@]
    //  x is a single row [d_model]; wte is [vocab, d_model].  The
    //  resulting matmul is mathematically identical to mmul-mlx2 with
    //  S=1, in=d_model, out=vocab, so route through backend_mmul_mlx2
    //  for GPU execution with VRAM-cached embedding weights.
    u3_noun x_meta, x_data, w_meta, w_data, s_data, b_data, grp;
    if ( c3n == u3r_mean(cor,
                         u3x_sam_4,    &x_meta,
                         u3x_sam_5,    &x_data,
                         u3x_sam_12,   &w_meta,
                         u3x_sam_13,   &w_data,
                         (c3_w)109,    &s_data,
                         (c3_w)221,    &b_data,
                         (c3_w)111,    &grp,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }
    if ( c3n == u3a_is_cat(grp) ) return u3_none;
    c3_w group = u3x_atom(grp);
    if ( group == 0 ) return u3_none;

    u3_noun x_shape = u3h(x_meta);
    u3_noun w_shape = u3h(w_meta);
    u3_noun vocab_atom = u3h(w_shape);
    u3_noun pcols_atom = u3h(u3t(w_shape));
    if ( c3n == u3a_is_cat(vocab_atom) || c3n == u3a_is_cat(pcols_atom) ) {
      return u3_none;
    }
    c3_w vocab        = u3x_atom(vocab_atom);
    c3_w packed_cols  = u3x_atom(pcols_atom);
    c3_w in_features  = packed_cols * 16;
    if ( (in_features % group) != 0 ) return u3_none;
    c3_w groups_per_row = in_features / group;

    size_t S = 1;
    c3_d x_bytes  = (c3_d)in_features * 4;
    c3_d w_bytes  = (c3_d)vocab * (c3_d)packed_cols * 4;
    c3_d sb_bytes = (c3_d)vocab * (c3_d)groups_per_row * 4;
    c3_d y_bytes  = (c3_d)vocab * 4;

    c3_y* y_buf = (c3_y*)u3a_malloc(y_bytes + 1);

    c3_w w_mug = u3r_mug(w_data);
    c3_w s_mug = u3r_mug(s_data);
    c3_w b_mug = u3r_mug(b_data);

    uint8_t w_sent[16], s_sent[16], b_sent[16];
    size_t w_spot = w_bytes < 16 ? (size_t)w_bytes : 16;
    size_t s_spot = sb_bytes < 16 ? (size_t)sb_bytes : 16;
    u3r_bytes(0, (c3_w)w_spot, w_sent, w_data);
    u3r_bytes(0, (c3_w)s_spot, s_sent, s_data);
    u3r_bytes(0, (c3_w)s_spot, b_sent, b_data);

    uintptr_t w_dptr = 0, s_dptr = 0, b_dptr = 0;
    int w_hit = backend_vram_probe((uint32_t)w_mug, w_bytes, w_sent, &w_dptr);
    int s_hit = backend_vram_probe((uint32_t)s_mug, sb_bytes, s_sent, &s_dptr);
    int b_hit = backend_vram_probe((uint32_t)b_mug, sb_bytes, b_sent, &b_dptr);

    c3_y* x_buf = (c3_y*)u3a_malloc(x_bytes);
    u3r_bytes(0, (c3_w)x_bytes, x_buf, x_data);

    c3_y* w_buf = NULL;
    c3_y* s_buf = NULL;
    c3_y* b_buf = NULL;
    if ( !w_hit ) {
      w_buf = (c3_y*)u3a_malloc(w_bytes);
      u3r_bytes(0, (c3_w)w_bytes, w_buf, w_data);
    }
    if ( !s_hit ) {
      s_buf = (c3_y*)u3a_malloc(sb_bytes);
      u3r_bytes(0, (c3_w)sb_bytes, s_buf, s_data);
    }
    if ( !b_hit ) {
      b_buf = (c3_y*)u3a_malloc(sb_bytes);
      u3r_bytes(0, (c3_w)sb_bytes, b_buf, b_data);
    }

    backend_status bs;
    if ( w_hit && s_hit && b_hit ) {
      bs = backend_mmul_mlx2_cached(
        x_buf, w_dptr, s_dptr, b_dptr, y_buf,
        S, in_features, vocab, group);
    } else {
      bs = backend_mmul_mlx2(
        x_buf, w_buf, s_buf, b_buf, y_buf,
        S, in_features, vocab, group,
        w_mug, s_mug, b_mug);
    }

    u3a_free(x_buf);
    if ( w_buf ) u3a_free(w_buf);
    if ( s_buf ) u3a_free(s_buf);
    if ( b_buf ) u3a_free(b_buf);

    if ( bs != BACKEND_OK ) {
      u3a_free(y_buf);
      //  CPU fallback: original streaming dequant + dot.
      u3_noun out_data = u3qi_la_logits_tied_mlx2(
        x_data, x_shape, w_data, w_shape, s_data, b_data, grp);
      if ( out_data == u3_none ) return u3_none;
      u3_noun out_shape = u3nt(u3i_word(1), u3k(vocab_atom), u3_nul);
      u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
      return u3nc(out_meta, out_data);
    }

    y_buf[y_bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(y_bytes + 1), y_buf);
    u3a_free(y_buf);

    u3_noun out_shape = u3nt(u3i_word(1), u3k(vocab_atom), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, r_data);
  }

  /* +dequant-mlx2-row jet.  Sample = [w=ray scales=ray biases=ray
   *                                    group-size=@ud row=@ud]
   * 5-tuple axes:
   *   w:          +12   meta +24   data +25
   *   scales:     +26   meta +52   data +53
   *   biases:     +54   meta +108  data +109
   *   group-size: +110
   *   row:        +111
   * Dequants a single packed row of an mlx2 weight [vocab, D/16] into
   * an fp32 tensor of shape [D].  Pure Hoon this does 1024 softfloat
   * muls+adds per call; jet is microseconds. */
  u3_noun
  u3wi_la_dequant_mlx2_row(u3_noun cor)
  {
    u3_noun w_meta, w_data, s_data, b_data, grp_atom, row_atom;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24,  &w_meta,
                         (c3_w)25,  &w_data,
                         (c3_w)53,  &s_data,
                         (c3_w)109, &b_data,
                         (c3_w)110, &grp_atom,
                         (c3_w)111, &row_atom,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }
    if ( c3n == u3a_is_cat(grp_atom) || c3n == u3a_is_cat(row_atom) ) {
      return u3_none;
    }
    c3_w group = u3x_atom(grp_atom);
    c3_w row   = u3x_atom(row_atom);
    if ( group == 0 ) return u3_none;

    u3_noun w_shape = u3h(w_meta);
    u3_noun vocab_atom = u3h(w_shape);
    u3_noun pcols_atom = u3h(u3t(w_shape));
    if ( c3n == u3a_is_cat(vocab_atom) || c3n == u3a_is_cat(pcols_atom) ) {
      return u3_none;
    }
    c3_w vocab       = u3x_atom(vocab_atom);
    c3_w packed_cols = u3x_atom(pcols_atom);
    c3_w D           = packed_cols * 16;
    if ( row >= vocab || (D % group) != 0 ) return u3_none;
    c3_w groups_per_row = D / group;

    /* Byte offsets into the packed atoms for the requested row. */
    c3_d w_row_bytes = (c3_d)packed_cols * 4;
    c3_d s_row_bytes = (c3_d)groups_per_row * 4;
    c3_d w_off       = (c3_d)row * w_row_bytes;
    c3_d s_off       = (c3_d)row * s_row_bytes;

    /* Scratch for the packed row + scale/bias rows. */
    uint32_t* w_row = (uint32_t*)u3a_malloc(w_row_bytes);
    float*    s_row = (float*)u3a_malloc(s_row_bytes);
    float*    b_row = (float*)u3a_malloc(s_row_bytes);
    u3r_bytes(w_off, (c3_w)w_row_bytes, (c3_y*)w_row, w_data);
    u3r_bytes(s_off, (c3_w)s_row_bytes, (c3_y*)s_row, s_data);
    u3r_bytes(s_off, (c3_w)s_row_bytes, (c3_y*)b_row, b_data);

    /* Output row: D fp32 values.  Bit-exact with the Hoon body: mul
     * then add, no FMA — standard IEEE fp32 on the host. */
    c3_d out_bytes = (c3_d)D * 4;
    c3_y* out_buf = (c3_y*)u3a_malloc(out_bytes + 1);
    float* out = (float*)out_buf;
    for ( c3_w wc = 0; wc < packed_cols; wc++ ) {
      uint32_t word = w_row[wc];
      c3_w i_base   = wc * 16;
      for ( c3_w k = 0; k < 16; k++ ) {
        c3_w i   = i_base + k;
        c3_w gr  = i / group;
        float sc = s_row[gr];
        float bi = b_row[gr];
        uint32_t q = (word >> (k * 2)) & 0x3u;
        float qf = (float)q;
        out[i] = sc * qf + bi;
      }
    }

    u3a_free(w_row); u3a_free(s_row); u3a_free(b_row);

    out_buf[out_bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(out_bytes + 1), out_buf);
    u3a_free(out_buf);

    /* shape = ~[D] — a one-element list. */
    u3_noun out_shape = u3nc(u3i_word(D), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, r_data);
  }

  u3_noun
  u3wi_la_dequant_mlx2(u3_noun cor)
  {
    //  Sample = [w=ray scales=ray biases=ray group-size=@]
    //    w        = +12 (sam_2)   w_meta = +24 (sam_4)   w_data = +25 (sam_5)
    //    bcd      = +13 (sam_3)
    //    b        = +26 (sam_6)   s_meta = +52 (sam_12)  s_data = +53 (sam_13)
    //    cd       = +27 (sam_7)
    //    c        = +54 (sam_14)  b_meta = +108           b_data = +109
    //    group    = +55 (sam_15)
    u3_noun w_meta, w_data, s_data, b_data, grp;
    if ( c3n == u3r_mean(cor,
                         u3x_sam_4,    &w_meta,
                         u3x_sam_5,    &w_data,
                         u3x_sam_13,   &s_data,
                         (c3_w)109,    &b_data,
                         u3x_sam_15,   &grp,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }
    u3_noun w_shape = u3h(w_meta);
    u3_noun out_data = u3qi_la_dequant_mlx2(w_data, w_shape, s_data, b_data, grp);
    if ( out_data == u3_none ) return u3_none;

    //  Output shape is [in_features, out_features] (transposed).
    u3_noun out_atom   = u3h(w_shape);
    u3_noun pcols_atom = u3h(u3t(w_shape));
    c3_w in_features = u3x_atom(pcols_atom) * 16;
    u3_noun out_shape = u3nt(u3i_word(in_features), u3k(out_atom), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, out_data);
  }

  /* +mmul-mlx2 jet — fused MLX2 dequant + fp32 matmul.
     Sample = [x=ray w=ray scales=ray biases=ray group-size=@].
     Tree-order axes for a 5-tuple gate sample:
       x        = +12       x_meta  = +24        x_data  = +25
       w        = +26       w_meta  = +52        w_data  = +53
       scales   = +54       s_meta  = +108       s_data  = +109
       biases   = +110      b_meta  = +220       b_data  = +221
       group    = +111
     Routes to backend_mmul_mlx2 which runs on GPU with VRAM-cached
     weights when CUDA is built in; otherwise returns u3_none and the
     Hoon fallback runs. */
  u3_noun
  u3wi_la_mmul_mlx2(u3_noun cor)
  {
    u3_noun x_meta, x_data, w_meta, w_data, s_data, b_data, grp;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24,    &x_meta,
                         (c3_w)25,    &x_data,
                         (c3_w)52,    &w_meta,
                         (c3_w)53,    &w_data,
                         (c3_w)109,   &s_data,
                         (c3_w)221,   &b_data,
                         (c3_w)111,   &grp,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }
    if ( c3n == u3a_is_cat(grp) ) return u3_none;
    c3_w group = u3x_atom(grp);
    if ( group == 0 ) return u3_none;

    //  Extract shapes.
    u3_noun x_shape = u3h(x_meta);
    u3_noun w_shape = u3h(w_meta);
    u3_noun S_atom  = u3h(x_shape);
    u3_noun xin_a   = u3h(u3t(x_shape));
    u3_noun out_a   = u3h(w_shape);
    u3_noun pcols_a = u3h(u3t(w_shape));
    if ( c3n == u3a_is_cat(S_atom) || c3n == u3a_is_cat(xin_a) ||
         c3n == u3a_is_cat(out_a)  || c3n == u3a_is_cat(pcols_a) ) {
      return u3_none;
    }
    c3_w S            = u3x_atom(S_atom);
    c3_w x_in         = u3x_atom(xin_a);
    c3_w out_features = u3x_atom(out_a);
    c3_w packed_cols  = u3x_atom(pcols_a);
    c3_w in_features  = packed_cols * 16;
    if ( x_in != in_features || (in_features % group) != 0 ) return u3_none;
    c3_w groups_per_row = in_features / group;

    c3_d x_bytes  = (c3_d)S * (c3_d)in_features * 4;
    c3_d w_bytes  = (c3_d)out_features * (c3_d)packed_cols * 4;
    c3_d sb_bytes = (c3_d)out_features * (c3_d)groups_per_row * 4;
    c3_d y_bytes  = (c3_d)S * (c3_d)out_features * 4;

    c3_y* y_buf = (c3_y*)u3a_malloc(y_bytes + 1);

    //  Probe the VRAM cache for each weight buffer first.  On hit we
    //  skip the u3r_bytes copy entirely — the 1-MB-ish weights then
    //  cost only a 16-byte sentinel read per call across the whole
    //  transformer stack.  This is pure performance, no state: cache
    //  hit or miss yields the same bit-exact output.
    c3_w w_mug = u3r_mug(w_data);
    c3_w s_mug = u3r_mug(s_data);
    c3_w b_mug = u3r_mug(b_data);

    uint8_t w_sent[16], s_sent[16], b_sent[16];
    size_t w_spot = w_bytes < 16 ? (size_t)w_bytes : 16;
    size_t s_spot = sb_bytes < 16 ? (size_t)sb_bytes : 16;
    u3r_bytes(0, (c3_w)w_spot, w_sent, w_data);
    u3r_bytes(0, (c3_w)s_spot, s_sent, s_data);
    u3r_bytes(0, (c3_w)s_spot, b_sent, b_data);

    uintptr_t w_dptr = 0, s_dptr = 0, b_dptr = 0;
    int w_hit = backend_vram_probe((uint32_t)w_mug, w_bytes, w_sent, &w_dptr);
    int s_hit = backend_vram_probe((uint32_t)s_mug, sb_bytes, s_sent, &s_dptr);
    int b_hit = backend_vram_probe((uint32_t)b_mug, sb_bytes, b_sent, &b_dptr);

    //  x is an activation — unique per call, no cache benefit.
    c3_y* x_buf = (c3_y*)u3a_malloc(x_bytes);
    u3r_bytes(0, (c3_w)x_bytes, x_buf, x_data);

    //  Only read bytes for weights that missed.
    c3_y* w_buf = NULL;
    c3_y* s_buf = NULL;
    c3_y* b_buf = NULL;
    if ( !w_hit ) {
      w_buf = (c3_y*)u3a_malloc(w_bytes);
      u3r_bytes(0, (c3_w)w_bytes, w_buf, w_data);
    }
    if ( !s_hit ) {
      s_buf = (c3_y*)u3a_malloc(sb_bytes);
      u3r_bytes(0, (c3_w)sb_bytes, s_buf, s_data);
    }
    if ( !b_hit ) {
      b_buf = (c3_y*)u3a_malloc(sb_bytes);
      u3r_bytes(0, (c3_w)sb_bytes, b_buf, b_data);
    }

    backend_status bs;
    if ( w_hit && s_hit && b_hit ) {
      //  All weights resident — skip the upload path entirely.
      bs = backend_mmul_mlx2_cached(
        x_buf, w_dptr, s_dptr, b_dptr, y_buf,
        S, in_features, out_features, group);
    } else {
      //  At least one miss — use the get-or-upload path.  Pre-resolved
      //  hits are a no-op on upload because they'll cache-hit inside.
      bs = backend_mmul_mlx2(
        x_buf, w_buf, s_buf, b_buf, y_buf,
        S, in_features, out_features, group,
        w_mug, s_mug, b_mug);
    }

    u3a_free(x_buf);
    if ( w_buf ) u3a_free(w_buf);
    if ( s_buf ) u3a_free(s_buf);
    if ( b_buf ) u3a_free(b_buf);

    if ( bs != BACKEND_OK ) {
      u3a_free(y_buf);
      return u3_none;  //  Hoon fallback: (mmul x (dequant-mlx2-ray ...))
    }

    y_buf[y_bytes] = 0x01;  //  MSB pin so u3i_bytes doesn't strip
    u3_noun r_data = u3i_bytes((c3_w)(y_bytes + 1), y_buf);
    u3a_free(y_buf);

    u3_noun out_shape = u3nt(u3k(S_atom), u3k(out_a), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, r_data);
  }

  /* +rms-norm-2d jet.  Sample = [x=ray gamma=ray eps=@rs].
     Tree-order axes for a 3-tuple sample:
       x       = +12     x_meta = +24     x_data = +25
       gamma   = +26     g_meta = +52     g_data = +53
       eps     = +27
     x has shape [S, D]; gamma has shape [D].  Output: [S, D] fp32. */
  u3_noun
  u3wi_la_rms_norm(u3_noun cor)
  {
    u3_noun x_meta, x_data, g_meta, g_data, eps_atom;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24, &x_meta,
                         (c3_w)25, &x_data,
                         (c3_w)52, &g_meta,
                         (c3_w)53, &g_data,
                         (c3_w)27, &eps_atom,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }

    u3_noun x_shape = u3h(x_meta);
    u3_noun g_shape = u3h(g_meta);
    u3_noun S_atom  = u3h(x_shape);
    u3_noun D_atom  = u3h(u3t(x_shape));
    u3_noun Dg_atom = u3h(g_shape);
    if ( c3n == u3a_is_cat(S_atom) || c3n == u3a_is_cat(D_atom) ||
         c3n == u3a_is_cat(Dg_atom) ) {
      return u3_none;
    }
    c3_w S = u3x_atom(S_atom);
    c3_w D = u3x_atom(D_atom);
    if ( D != u3x_atom(Dg_atom) || S == 0 || D == 0 ) return u3_none;

    c3_d x_bytes = (c3_d)S * (c3_d)D * 4;
    c3_d g_bytes = (c3_d)D * 4;

    c3_y* x_buf = (c3_y*)u3a_malloc(x_bytes);
    c3_y* g_buf = (c3_y*)u3a_malloc(g_bytes);
    c3_y* y_buf = (c3_y*)u3a_malloc(x_bytes + 1);
    u3r_bytes(0, (c3_w)x_bytes, x_buf, x_data);
    u3r_bytes(0, (c3_w)g_bytes, g_buf, g_data);

    /* eps is @rs — 32 bits as a direct atom. */
    c3_w eps_u32 = u3x_atom(eps_atom);
    float eps;
    memcpy(&eps, &eps_u32, 4);

    backend_status bs = backend_rms_norm_fp32(
      x_buf, g_buf, eps, y_buf, S, D);

    u3a_free(x_buf);
    u3a_free(g_buf);

    if ( bs != BACKEND_OK ) {
      u3a_free(y_buf);
      return u3_none;
    }

    y_buf[x_bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(x_bytes + 1), y_buf);
    u3a_free(y_buf);

    u3_noun out_shape = u3nt(u3k(S_atom), u3k(D_atom), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, r_data);
  }

  /* +rope-apply-row jet.  Sample = [x=ray cos=ray sin=ray].
     3-tuple axes: x=+12 (meta=24 data=25), cos=+26 (52/53),
                   sin=+27 (54/55).
     x: [S, H, Dh] fp32; cos/sin: [S, Dh] fp32; out: [S, H, Dh] fp32.
     Dh must be even (half-rotated convention). */
  u3_noun
  u3wi_la_rope_apply(u3_noun cor)
  {
    u3_noun x_meta, x_data, c_meta, c_data, s_meta, s_data;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24, &x_meta,
                         (c3_w)25, &x_data,
                         (c3_w)52, &c_meta,
                         (c3_w)53, &c_data,
                         (c3_w)54, &s_meta,
                         (c3_w)55, &s_data,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }

    u3_noun x_shape = u3h(x_meta);
    u3_noun S_a  = u3h(x_shape);
    u3_noun H_a  = u3h(u3t(x_shape));
    u3_noun Dh_a = u3h(u3t(u3t(x_shape)));
    if ( c3n == u3a_is_cat(S_a) || c3n == u3a_is_cat(H_a) ||
         c3n == u3a_is_cat(Dh_a) ) return u3_none;

    c3_w S  = u3x_atom(S_a);
    c3_w H  = u3x_atom(H_a);
    c3_w Dh = u3x_atom(Dh_a);
    if ( S == 0 || H == 0 || Dh == 0 || (Dh & 1) ) return u3_none;

    c3_d x_bytes  = (c3_d)S * (c3_d)H * (c3_d)Dh * 4;
    c3_d cs_bytes = (c3_d)S * (c3_d)Dh * 4;

    c3_y* x_buf = (c3_y*)u3a_malloc(x_bytes);
    c3_y* c_buf = (c3_y*)u3a_malloc(cs_bytes);
    c3_y* s_buf = (c3_y*)u3a_malloc(cs_bytes);
    c3_y* y_buf = (c3_y*)u3a_malloc(x_bytes + 1);
    u3r_bytes(0, (c3_w)x_bytes,  x_buf, x_data);
    u3r_bytes(0, (c3_w)cs_bytes, c_buf, c_data);
    u3r_bytes(0, (c3_w)cs_bytes, s_buf, s_data);

    backend_status bs = backend_rope_apply_fp32(
      x_buf, c_buf, s_buf, y_buf, S, H, Dh);

    u3a_free(x_buf);
    u3a_free(c_buf);
    u3a_free(s_buf);

    if ( bs != BACKEND_OK ) {
      u3a_free(y_buf);
      return u3_none;
    }

    y_buf[x_bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(x_bytes + 1), y_buf);
    u3a_free(y_buf);

    u3_noun out_shape = u3nq(u3k(S_a), u3k(H_a), u3k(Dh_a), u3_nul);
    u3_noun out_meta = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, r_data);
  }

  /* +silu-mul-ray jet.  Sample = [a=ray b=ray].
     2-tuple axes: a=+12 (meta=24 data=25), b=+13 (meta=26 data=27).
     a and b are any matching-shape fp32 tensors; output shape = a's. */
  u3_noun
  u3wi_la_silu_mul(u3_noun cor)
  {
    u3_noun a_meta, a_data, b_meta, b_data;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24, &a_meta,
                         (c3_w)25, &a_data,
                         (c3_w)26, &b_meta,
                         (c3_w)27, &b_data,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }
    /* Count total elements from a's shape (list @). */
    u3_noun shape = u3h(a_meta);
    c3_d N = 1;
    u3_noun cur = shape;
    while ( u3_nul != cur ) {
      u3_noun dim = u3h(cur);
      if ( c3n == u3a_is_cat(dim) ) return u3_none;
      N *= u3x_atom(dim);
      cur = u3t(cur);
    }
    if ( N == 0 ) return u3_none;

    c3_d bytes = N * 4;
    c3_y* a_buf = (c3_y*)u3a_malloc(bytes);
    c3_y* b_buf = (c3_y*)u3a_malloc(bytes);
    c3_y* y_buf = (c3_y*)u3a_malloc(bytes + 1);
    u3r_bytes(0, (c3_w)bytes, a_buf, a_data);
    u3r_bytes(0, (c3_w)bytes, b_buf, b_data);

    backend_status bs = backend_silu_mul_fp32(a_buf, b_buf, y_buf, N);

    u3a_free(a_buf);
    u3a_free(b_buf);
    if ( bs != BACKEND_OK ) { u3a_free(y_buf); return u3_none; }

    y_buf[bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(bytes + 1), y_buf);
    u3a_free(y_buf);
    /* preserve a_meta exactly (shape, bloq, kind, tail) */
    return u3nc(u3k(a_meta), r_data);
  }

  /* +gqa-attention-ray jet.  Sample = [q=ray k=ray v=ray].
     3-tuple axes: q=+12 (24/25), k=+26 (52/53), v=+27 (54/55).
     q: [S, H, Dh]; k, v: [S, KH, Dh].  Output: [S, H*Dh] fp32. */
  u3_noun
  u3wi_la_gqa_attention(u3_noun cor)
  {
    u3_noun q_meta, q_data, k_meta, k_data, v_meta, v_data;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24, &q_meta,
                         (c3_w)25, &q_data,
                         (c3_w)52, &k_meta,
                         (c3_w)53, &k_data,
                         (c3_w)54, &v_meta,
                         (c3_w)55, &v_data,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }

    u3_noun q_shape = u3h(q_meta);
    u3_noun k_shape = u3h(k_meta);
    u3_noun S_a  = u3h(q_shape);
    u3_noun H_a  = u3h(u3t(q_shape));
    u3_noun Dh_a = u3h(u3t(u3t(q_shape)));
    u3_noun KH_a = u3h(u3t(k_shape));
    if ( c3n == u3a_is_cat(S_a)  || c3n == u3a_is_cat(H_a)  ||
         c3n == u3a_is_cat(Dh_a) || c3n == u3a_is_cat(KH_a) ) {
      return u3_none;
    }
    c3_w S  = u3x_atom(S_a);
    c3_w H  = u3x_atom(H_a);
    c3_w Dh = u3x_atom(Dh_a);
    c3_w KH = u3x_atom(KH_a);
    if ( S == 0 || H == 0 || KH == 0 || Dh == 0 || (H % KH) ) return u3_none;

    c3_d q_bytes  = (c3_d)S * (c3_d)H  * (c3_d)Dh * 4;
    c3_d kv_bytes = (c3_d)S * (c3_d)KH * (c3_d)Dh * 4;

    c3_y* q_buf = (c3_y*)u3a_malloc(q_bytes);
    c3_y* k_buf = (c3_y*)u3a_malloc(kv_bytes);
    c3_y* v_buf = (c3_y*)u3a_malloc(kv_bytes);
    c3_y* y_buf = (c3_y*)u3a_malloc(q_bytes + 1);
    u3r_bytes(0, (c3_w)q_bytes,  q_buf, q_data);
    u3r_bytes(0, (c3_w)kv_bytes, k_buf, k_data);
    u3r_bytes(0, (c3_w)kv_bytes, v_buf, v_data);

    backend_status bs = backend_gqa_attention_fp32(
      q_buf, k_buf, v_buf, y_buf, S, H, KH, Dh);

    u3a_free(q_buf);
    u3a_free(k_buf);
    u3a_free(v_buf);
    if ( bs != BACKEND_OK ) { u3a_free(y_buf); return u3_none; }

    y_buf[q_bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(q_bytes + 1), y_buf);
    u3a_free(y_buf);

    /* Output shape: [S, H*Dh]. */
    u3_noun out_shape = u3nt(u3k(S_a), u3i_word(H * Dh), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, r_data);
  }

  /* ==== sampling primitives =====================================
   * Three small pure-C jets for the token-sampling critical path.
   * All byte-exact against the Hoon reference (softfloat semantics)
   * because they use only IEEE mul/add/div/sub + expf_hoon.
   * No CUDA — sampling runs once per token over ~100k vocab; a
   * well-tuned CPU path is fast enough and keeps things modular.
   */

  /* qsort comparator: descending fp32.  Bit-pattern comparison would
   * sort NaNs/negatives incorrectly; use IEEE > on the floats. */
  static int _fp32_desc_cmp(const void* a, const void* b)
  {
    float fa, fb;
    memcpy(&fa, a, 4);
    memcpy(&fb, b, 4);
    if ( fa > fb ) return -1;
    if ( fa < fb ) return 1;
    return 0;
  }

  /* +softmax-row-ray jet.  Sample = tensor (single-arg gate).
   * Row-wise softmax over the full flat element count, matching
   * saloon's (softmax a): find max, subtract, expf_hoon, cumsum, divide.
   *     logits -> probs   (same shape) */
  u3_noun
  u3wi_la_softmax_row(u3_noun cor)
  {
    /* Single-arg gate: the tensor itself is the sample at +6.  So
     * meta is at +12, data at +13. */
    u3_noun x_meta, x_data;
    if ( c3n == u3r_mean(cor,
                         (c3_w)12, &x_meta,
                         (c3_w)13, &x_data,
                         u3_nul) ) {
      return u3m_bail(c3__exit);
    }

    /* Product of shape dims = total N elements. */
    u3_noun shape = u3h(x_meta);
    c3_d N = 1;
    u3_noun cur = shape;
    while ( u3_nul != cur ) {
      u3_noun dim = u3h(cur);
      if ( c3n == u3a_is_cat(dim) ) return u3_none;
      N *= u3x_atom(dim);
      cur = u3t(cur);
    }
    if ( N == 0 ) return u3_none;

    c3_d bytes = N * 4;
    c3_y* x_buf = (c3_y*)u3a_malloc(bytes);
    c3_y* y_buf = (c3_y*)u3a_malloc(bytes + 1);
    u3r_bytes(0, (c3_w)bytes, x_buf, x_data);

    float* xf = (float*)x_buf;
    float* yf = (float*)y_buf;

    /* max */
    float mx = xf[0];
    for ( c3_d i = 1; i < N; i++ ) if ( xf[i] > mx ) mx = xf[i];
    /* exp(x - mx), cumulative sum */
    float sum = 0.0f;
    for ( c3_d i = 0; i < N; i++ ) {
      float e = expf_hoon(xf[i] - mx);
      yf[i] = e;
      sum = sum + e;
    }
    /* divide each by sum */
    for ( c3_d i = 0; i < N; i++ ) yf[i] = yf[i] / sum;

    u3a_free(x_buf);
    y_buf[bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(bytes + 1), y_buf);
    u3a_free(y_buf);
    return u3nc(u3k(x_meta), r_data);
  }

  /* +mask-top-p-ray jet.  Sample = [logits=tensor p=@rs].
   *   logits at +12 (meta=24, data=25), p at +13.
   * Sorts logits descending, softmaxes in that order, walks cumulative
   * probability until >= p; any logit strictly less than the threshold
   * at that cutoff is replaced with -inf (0xff800000). */
  u3_noun
  u3wi_la_mask_top_p(u3_noun cor)
  {
    u3_noun x_meta, x_data, p_atom;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24, &x_meta,
                         (c3_w)25, &x_data,
                         (c3_w)13, &p_atom,
                         u3_nul) ) {
      return u3m_bail(c3__exit);
    }

    u3_noun shape = u3h(x_meta);
    c3_d N = 1;
    u3_noun cur = shape;
    while ( u3_nul != cur ) {
      u3_noun dim = u3h(cur);
      if ( c3n == u3a_is_cat(dim) ) return u3_none;
      N *= u3x_atom(dim);
      cur = u3t(cur);
    }
    if ( N == 0 ) return u3_none;

    c3_w p_u32 = u3x_atom(p_atom);
    float p;
    memcpy(&p, &p_u32, 4);

    c3_d bytes = N * 4;
    c3_y* x_buf    = (c3_y*)u3a_malloc(bytes);
    c3_y* sort_buf = (c3_y*)u3a_malloc(bytes);
    c3_y* y_buf    = (c3_y*)u3a_malloc(bytes + 1);
    u3r_bytes(0, (c3_w)bytes, x_buf, x_data);

    float* xf = (float*)x_buf;
    float* sf = (float*)sort_buf;
    float* yf = (float*)y_buf;

    /* sort descending */
    memcpy(sf, xf, bytes);
    qsort(sf, N, sizeof(float), _fp32_desc_cmp);

    /* softmax on sorted */
    float mx = sf[0];
    float* probs = (float*)u3a_malloc(bytes);
    float sum = 0.0f;
    for ( c3_d i = 0; i < N; i++ ) {
      float e = expf_hoon(sf[i] - mx);
      probs[i] = e;
      sum = sum + e;
    }
    for ( c3_d i = 0; i < N; i++ ) probs[i] = probs[i] / sum;

    /* walk cum until >= p; threshold = sorted[cutoff_i]; default = last */
    float threshold = sf[N - 1];
    float cum = 0.0f;
    for ( c3_d i = 0; i < N; i++ ) {
      cum = cum + probs[i];
      if ( cum >= p ) { threshold = sf[i]; break; }
    }

    /* build output: keep >= threshold, mask to -inf otherwise */
    float neg_inf;
    uint32_t neg_inf_bits = 0xff800000u;
    memcpy(&neg_inf, &neg_inf_bits, 4);
    for ( c3_d i = 0; i < N; i++ ) {
      yf[i] = (xf[i] >= threshold) ? xf[i] : neg_inf;
    }

    u3a_free(x_buf);
    u3a_free(sort_buf);
    u3a_free(probs);
    y_buf[bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(bytes + 1), y_buf);
    u3a_free(y_buf);
    return u3nc(u3k(x_meta), r_data);
  }

  /* +sample-from-dist-ray jet.  Sample = [probs=tensor eny=@].
   *   probs at +12 (meta=24, data=25), eny at +13.
   * Returns @ud index: walks cumulative probability, emits first
   * index where cum >= r, with r = (eny mod 1e6) / 1e6 as fp32. */
  u3_noun
  u3wi_la_sample_from_dist(u3_noun cor)
  {
    u3_noun p_meta, p_data, eny_atom;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24, &p_meta,
                         (c3_w)25, &p_data,
                         (c3_w)13, &eny_atom,
                         u3_nul) ) {
      return u3m_bail(c3__exit);
    }

    u3_noun shape = u3h(p_meta);
    c3_d N = 1;
    u3_noun cur = shape;
    while ( u3_nul != cur ) {
      u3_noun dim = u3h(cur);
      if ( c3n == u3a_is_cat(dim) ) return u3_none;
      N *= u3x_atom(dim);
      cur = u3t(cur);
    }
    if ( N == 0 ) return u3_none;

    /* eny mod 1_000_000 using arbitrary-precision (eny may be > 64 bits). */
    u3_noun mod_atom = u3qa_mod(u3k(eny_atom), u3i_word(1000000));
    c3_w r_n = u3x_atom(mod_atom);
    u3z(mod_atom);
    float r = (float)r_n / 1000000.0f;

    c3_d bytes = N * 4;
    c3_y* p_buf = (c3_y*)u3a_malloc(bytes);
    u3r_bytes(0, (c3_w)bytes, p_buf, p_data);
    float* pf = (float*)p_buf;

    c3_d idx = N - 1;  /* fallback */
    float cum = 0.0f;
    for ( c3_d i = 0; i < N; i++ ) {
      cum = cum + pf[i];
      if ( cum >= r ) { idx = i; break; }
    }
    u3a_free(p_buf);
    return u3i_chub(idx);
  }

  /* +run-block-qwen3 jet.  Sample = [x bw cfg cos sin]. One call
   * runs an entire Qwen3 transformer block end-to-end on GPU with
   * every intermediate tensor resident in VRAM (one HtoD for x, one
   * DtoH for the new x).  Weights must already be cache-resident.
   *
   * This is the biggest performance win: it collapses the ~16 per-op
   * jet dispatches per block into a single jet call whose overhead is
   * the same as any other single-op jet. */

  /* +embed-tied-mlx2 jet.  Sample = [tokens=(list @ud) wte=weight-tensor
   *                                  cfg=model-config-qwen3].
   * 3-tuple axes:
   *   tokens: +12
   *   wte:    +26      tag %mlx2 at +52, body at +53.
   *     wte body = [wq scales biases group-size]
   *       wq:    +106   meta +212  data +213
   *       scales:+214   meta +428  data +429
   *       biases:+430   meta +860  data +861
   *       group: +431
   *   cfg:    +27      unused (d_model derived from wte shape).
   * Dequants the row of wte for each token and concatenates into
   * [S, D] fp32.  Replaces a pure-Hoon S-iteration set-row loop that
   * was the dominant per-forward overhead for long prompts. */
  u3_noun
  u3wi_la_embed_tied_mlx2(u3_noun cor)
  {
    u3_noun tokens, wte_tag, wq_meta, wq_data, s_data, b_data, grp_atom;
    if ( c3n == u3r_mean(cor,
                         u3x_sam_2,  &tokens,
                         (c3_w)52,   &wte_tag,
                         (c3_w)212,  &wq_meta,
                         (c3_w)213,  &wq_data,
                         (c3_w)429,  &s_data,
                         (c3_w)861,  &b_data,
                         (c3_w)431,  &grp_atom,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }
    if ( wte_tag != c3_s4('m','l','x','2') ) return u3_none;
    if ( c3n == u3a_is_cat(grp_atom) ) return u3_none;
    c3_w group = u3x_atom(grp_atom);
    if ( group == 0 ) return u3_none;

    u3_noun w_shape = u3h(wq_meta);
    u3_noun vocab_a = u3h(w_shape);
    u3_noun pcols_a = u3h(u3t(w_shape));
    if ( c3n == u3a_is_cat(vocab_a) || c3n == u3a_is_cat(pcols_a) ) {
      return u3_none;
    }
    c3_w vocab       = u3x_atom(vocab_a);
    c3_w packed_cols = u3x_atom(pcols_a);
    c3_w D           = packed_cols * 16;
    if ( (D % group) != 0 ) return u3_none;
    c3_w groups_per_row = D / group;

    /* Count tokens. */
    u3_noun tl = tokens;
    c3_w S = 0;
    while ( tl != u3_nul ) { S++; tl = u3t(tl); }
    if ( S == 0 ) return u3_none;

    c3_d w_row_bytes = (c3_d)packed_cols * 4;
    c3_d s_row_bytes = (c3_d)groups_per_row * 4;
    c3_d out_bytes   = (c3_d)S * (c3_d)D * 4;

    c3_y* out_buf = (c3_y*)u3a_malloc(out_bytes + 1);
    float* out    = (float*)out_buf;
    uint32_t* w_row = (uint32_t*)u3a_malloc(w_row_bytes);
    float*    s_row = (float*)u3a_malloc(s_row_bytes);
    float*    b_row = (float*)u3a_malloc(s_row_bytes);

    tl = tokens;
    for ( c3_w i = 0; i < S; i++ ) {
      u3_noun tok_atom = u3h(tl);
      if ( c3n == u3a_is_cat(tok_atom) ) {
        u3a_free(w_row); u3a_free(s_row); u3a_free(b_row);
        u3a_free(out_buf);
        return u3_none;
      }
      c3_w tok_id = u3x_atom(tok_atom);
      if ( tok_id >= vocab ) {
        u3a_free(w_row); u3a_free(s_row); u3a_free(b_row);
        u3a_free(out_buf);
        return u3_none;
      }
      c3_d w_off = (c3_d)tok_id * w_row_bytes;
      c3_d s_off = (c3_d)tok_id * s_row_bytes;
      u3r_bytes(w_off, (c3_w)w_row_bytes, (c3_y*)w_row, wq_data);
      u3r_bytes(s_off, (c3_w)s_row_bytes, (c3_y*)s_row, s_data);
      u3r_bytes(s_off, (c3_w)s_row_bytes, (c3_y*)b_row, b_data);

      float* dst = out + (c3_d)i * D;
      for ( c3_w wc = 0; wc < packed_cols; wc++ ) {
        uint32_t word = w_row[wc];
        c3_w i_base   = wc * 16;
        for ( c3_w k = 0; k < 16; k++ ) {
          c3_w ii    = i_base + k;
          c3_w gr    = ii / group;
          float sc   = s_row[gr];
          float bi   = b_row[gr];
          uint32_t q = (word >> (k * 2)) & 0x3u;
          dst[ii]    = sc * (float)q + bi;
        }
      }
      tl = u3t(tl);
    }

    u3a_free(w_row); u3a_free(s_row); u3a_free(b_row);

    out_buf[out_bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(out_bytes + 1), out_buf);
    u3a_free(out_buf);

    u3_noun out_shape = u3nt(u3i_word(S), u3i_word(D), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, r_data);
  }

  /* +rope-cos-sin jet — byte-exact port of Hoon's sin:rs / cos:rs
   * Taylor series.  Pure Hoon at seq_len=10, head_dim=64 runs ~1 second
   * per forward; the jet is under a millisecond.  Uses only standard
   * IEEE fp32 ops so the result matches Hoon bit-for-bit on any host. */

  static inline float
  _hoon_floor_pos_rs(float x)
  {
    /* Hoon's +mod: a - b * floor(a/b).  For positive a,b the `floor` is
     * done as `san (need (toi (div a b)))` — toi truncates toward zero
     * and for non-negative values that equals floor.  C's (float)(int32)
     * does the same truncation on the narrow range we need. */
    return (float)(int64_t)x;
  }

  static float
  _hoon_mod_rs(float a, float b)
  {
    /* Only called with positive a,b here (angles from p * inv_freq,
     * all non-negative).  Recurse on negatives only for completeness. */
    if ( a < 0.0f ) return b - _hoon_mod_rs(-a, b);
    float quot = a / b;
    float f    = _hoon_floor_pos_rs(quot);
    return a - b * f;
  }

  /* Tau and rtol match the fp32 rs core in lib/math.hoon:
   *   tau  = .6.2831855   (0x40c90fdb, 2*pi rounded to fp32)
   *   rtol = .1e-5        (0x3727c5ac) */
  static const float _HOON_TAU_RS  = 6.2831855f;
  static const float _HOON_RTOL_RS = 1.0e-5f;

  /* |x| for fp32 via bit clear of sign. */
  static inline float
  _hoon_abs_rs(float x)
  {
    uint32_t u;
    memcpy(&u, &x, 4);
    u &= 0x7fffffff;
    float r; memcpy(&r, &u, 4);
    return r;
  }

  /* Hoon sin:rs Taylor series.
   *   p = x
   *   term = x
   *   for i = 1, 2, ...:
   *     if |term| <= rtol: return p
   *     i2 = 2*i
   *     term = -term * (x*x) / (i2 * (i2 + 1))
   *     p += term */
  static float
  _hoon_sin_rs(float x)
  {
    x = _hoon_mod_rs(x, _HOON_TAU_RS);
    float p    = x;
    float term = x;
    int   i    = 1;
    while ( _hoon_abs_rs(term) > _HOON_RTOL_RS ) {
      float i2   = (float)(2 * i);
      float num  = -term * (x * x);
      float den  = i2 * (i2 + 1.0f);
      term       = num / den;
      p          = p + term;
      i++;
      if ( i > 64 ) break;  /* safety bound; series converges well before */
    }
    return p;
  }

  /* Hoon cos:rs Taylor series.
   *   p = 1
   *   term = 1
   *   for i = 1, 2, ...:
   *     if |term| <= rtol: return p
   *     i2 = 2*i
   *     term = -term * (x*x) / (i2 * (i2 - 1))
   *     p += term */
  static float
  _hoon_cos_rs(float x)
  {
    x = _hoon_mod_rs(x, _HOON_TAU_RS);
    float p    = 1.0f;
    float term = 1.0f;
    int   i    = 1;
    while ( _hoon_abs_rs(term) > _HOON_RTOL_RS ) {
      float i2   = (float)(2 * i);
      float num  = -term * (x * x);
      float den  = i2 * (i2 - 1.0f);
      term       = num / den;
      p          = p + term;
      i++;
      if ( i > 64 ) break;
    }
    return p;
  }

  /* +rope-cos-sin jet.  Sample = [seq-len=@ud head-dim=@ud
   *                               inv-freq=tensor attn-factor=@rs]
   * 4-tuple axes:
   *   seq-len:     +12 (sam_2)
   *   head-dim:    +26 (sam_6)
   *   inv-freq:    +54 (sam_14)     meta +108 (sam_28)  data +109
   *   attn-factor: +55 (sam_15)
   * inv-freq is [half] fp32 where half = head_dim / 2.
   * Output: cell [cos=tensor sin=tensor], each [seq_len, head_dim]. */
  u3_noun
  u3wi_la_rope_cos_sin(u3_noun cor)
  {
    u3_noun S_atom, Dh_atom, inv_meta, inv_data, attn_atom;
    if ( c3n == u3r_mean(cor,
                         u3x_sam_2,   &S_atom,
                         u3x_sam_6,   &Dh_atom,
                         (c3_w)108,   &inv_meta,
                         (c3_w)109,   &inv_data,
                         u3x_sam_15,  &attn_atom,
                         u3_nul) )
    {
      return u3m_bail(c3__exit);
    }
    if ( c3n == u3a_is_cat(S_atom) || c3n == u3a_is_cat(Dh_atom) ) {
      return u3_none;
    }
    c3_w S  = u3x_atom(S_atom);
    c3_w Dh = u3x_atom(Dh_atom);
    if ( S == 0 || Dh == 0 || (Dh & 1) ) return u3_none;
    c3_w half = Dh / 2;

    /* inv_meta.shape[0] should equal half. */
    u3_noun inv_shape = u3h(inv_meta);
    u3_noun half_atom = u3h(inv_shape);
    if ( c3n == u3a_is_cat(half_atom) ) return u3_none;
    if ( u3x_atom(half_atom) != half ) return u3_none;

    c3_d inv_bytes = (c3_d)half * 4;
    float* inv = (float*)u3a_malloc(inv_bytes);
    u3r_bytes(0, (c3_w)inv_bytes, (c3_y*)inv, inv_data);

    float attn;
    c3_w attn_bits = u3x_atom(attn_atom);
    memcpy(&attn, &attn_bits, 4);

    c3_d cs_bytes = (c3_d)S * (c3_d)Dh * 4;
    float* cos_out = (float*)u3a_malloc(cs_bytes);
    float* sin_out = (float*)u3a_malloc(cs_bytes);

    for ( c3_w p = 0; p < S; p++ ) {
      float pf = (float)p;
      for ( c3_w j = 0; j < Dh; j++ ) {
        c3_w  j_mod = (j < half) ? j : (j - half);
        float inv_j = inv[j_mod];
        float angle = pf * inv_j;
        float c     = _hoon_cos_rs(angle) * attn;
        float s     = _hoon_sin_rs(angle) * attn;
        cos_out[(c3_d)p * Dh + j] = c;
        sin_out[(c3_d)p * Dh + j] = s;
      }
    }

    u3a_free(inv);

    /* Emit two tensors with shared [S, Dh] meta. */
    c3_y* cos_buf = (c3_y*)u3a_malloc(cs_bytes + 1);
    c3_y* sin_buf = (c3_y*)u3a_malloc(cs_bytes + 1);
    memcpy(cos_buf, cos_out, cs_bytes);
    memcpy(sin_buf, sin_out, cs_bytes);
    cos_buf[cs_bytes] = 0x01;
    sin_buf[cs_bytes] = 0x01;
    u3_noun cos_data = u3i_bytes((c3_w)(cs_bytes + 1), cos_buf);
    u3_noun sin_data = u3i_bytes((c3_w)(cs_bytes + 1), sin_buf);
    u3a_free(cos_out); u3a_free(sin_out);
    u3a_free(cos_buf); u3a_free(sin_buf);

    u3_noun cos_meta = u3nq(u3nt(u3i_word(S), u3i_word(Dh), u3_nul),
                            u3i_word(5), c3__i754, 0);
    u3_noun sin_meta = u3nq(u3nt(u3i_word(S), u3i_word(Dh), u3_nul),
                            u3i_word(5), c3__i754, 0);
    u3_noun cos_tn   = u3nc(cos_meta, cos_data);
    u3_noun sin_tn   = u3nc(sin_meta, sin_data);
    return u3nc(cos_tn, sin_tn);
  }

  /* +run-blocks-qwen3 jet.  Sample = [x=tensor blocks=(list block)
   *                                   cfg=model-config cos=tensor sin=tensor]
   * 5-tuple axes:
   *   x:        +12 → meta +24,  data +25
   *   blocks:   +26 (a list of block-weights-qwen3)
   *   cfg:      +54
   *   cos:      +110 → meta +220, data +221
   *   sin:      +222 → meta +444, data +445
   *   seq-hash: +223  (mug of the full token sequence covered by x;
   *                     0 disables KV-cache emission — pure recompute)
   * Collapses the entire per-block loop into one GPU kernel.  All 28
   * blocks run with x resident in VRAM; one HtoD + one DtoH + one sync
   * barrier, instead of one set per block. */
  u3_noun
  u3wi_la_run_qwen3_forward(u3_noun cor)
  {
    u3_noun x_meta, x_data, blocks, cfg, cos_data, sin_data, seq_hash_atom;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24,  &x_meta,
                         (c3_w)25,  &x_data,
                         (c3_w)26,  &blocks,
                         (c3_w)54,  &cfg,
                         (c3_w)221, &cos_data,
                         (c3_w)445, &sin_data,
                         (c3_w)223, &seq_hash_atom,
                         u3_nul) ) {
      return u3m_bail(c3__exit);
    }

    u3_noun x_shape = u3h(x_meta);
    u3_noun S_atom  = u3h(x_shape);
    u3_noun D_atom  = u3h(u3t(x_shape));
    if ( c3n == u3a_is_cat(S_atom) || c3n == u3a_is_cat(D_atom) ) return u3_none;
    c3_w S = u3x_atom(S_atom);
    c3_w D = u3x_atom(D_atom);

    u3_noun c = cfg;
    u3_noun cfg_d_model  = u3h(c); c = u3t(c);
    u3_noun cfg_n_heads  = u3h(c); c = u3t(c);
    u3_noun cfg_n_kv_h   = u3h(c); c = u3t(c);
                                   c = u3t(c);  /* n-layers */
    u3_noun cfg_d_ff     = u3h(c); c = u3t(c);
                                   c = u3t(c);  /* vocab */
                                   c = u3t(c);  /* max-seq */
    u3_noun cfg_head_dim = u3h(c); c = u3t(c);
    u3_noun cfg_rms_eps  = u3h(c);

    c3_w H  = u3x_atom(cfg_n_heads);
    c3_w KH = u3x_atom(cfg_n_kv_h);
    c3_w Dh = u3x_atom(cfg_head_dim);
    c3_w D_ff = u3x_atom(cfg_d_ff);
    if ( u3x_atom(cfg_d_model) != D || H * Dh != D || (H % KH) != 0 )
      return u3_none;
    c3_w eps_u32 = u3x_atom(cfg_rms_eps);
    float rms_eps;
    memcpy(&rms_eps, &eps_u32, 4);

    c3_d x_bytes     = (c3_d)S * D * 4;
    c3_d y_bytes     = x_bytes;
    c3_d wD_bytes    = (c3_d)D * (D / 16) * 4;
    c3_d wKV_bytes   = (c3_d)(KH * Dh) * (D / 16) * 4;
    c3_d wFF_bytes   = (c3_d)D_ff * (D / 16) * 4;
    c3_d wDown_bytes = (c3_d)D * (D_ff / 16) * 4;
    c3_d gamma_D     = (c3_d)D * 4;
    c3_d gamma_Dh    = (c3_d)Dh * 4;
    c3_d cs_bytes    = (c3_d)S * Dh * 4;

    /* Count blocks + allocate dptrs array. */
    size_t n_blocks = 0;
    { u3_noun t = blocks; while ( t != u3_nul ) { n_blocks++; t = u3t(t); } }
    if ( n_blocks == 0 ) return u3_none;

    qw3_block_dptrs* arr =
      (qw3_block_dptrs*)u3a_malloc(n_blocks * sizeof(qw3_block_dptrs));
    if ( !arr ) return u3_none;
    memset(arr, 0, n_blocks * sizeof(qw3_block_dptrs));

    /* Shared macros: PROBE_REQ = must be cached (weights); PROBE_OR_UP =
     * content-hashed get-or-upload (gammas + cos/sin). */
    #define PROBE_REQ(dst, data_atom, total_bytes) do {                  \
        c3_w _mug = u3r_mug(data_atom);                                  \
        uint8_t _sent[16];                                               \
        size_t _spot = total_bytes < 16 ? (size_t)total_bytes : 16;      \
        u3r_bytes(0, (c3_w)_spot, _sent, data_atom);                     \
        if ( !backend_vram_probe((uint32_t)_mug, total_bytes, _sent, &dst) ) { \
          u3a_free(arr); return u3_none;                                 \
        }                                                                \
      } while (0)
    #define PROBE_OR_UP(dst, data_atom, total_bytes) do {                \
        c3_w _mug = u3r_mug(data_atom);                                  \
        uint8_t _sent[16];                                               \
        size_t _spot = total_bytes < 16 ? (size_t)total_bytes : 16;      \
        u3r_bytes(0, (c3_w)_spot, _sent, data_atom);                     \
        if ( !backend_vram_probe((uint32_t)_mug, total_bytes, _sent, &dst) ) { \
          c3_y* _tmp = (c3_y*)u3a_malloc(total_bytes);                   \
          u3r_bytes(0, (c3_w)total_bytes, _tmp, data_atom);              \
          backend_status _bs = backend_vram_upload(                      \
            _tmp, total_bytes, (uint32_t)_mug, &dst);                    \
          u3a_free(_tmp);                                                \
          if ( _bs != BACKEND_OK ) { u3a_free(arr); return u3_none; }    \
        }                                                                \
      } while (0)
    #define MLX2_PROJ(proj, WD, SD, BD, GRP) do {                        \
        if ( u3h(proj) != c3_s4('m','l','x','2') )                       \
          { u3a_free(arr); return u3_none; }                             \
        u3_noun _body = u3t(proj);                                       \
        u3_noun _wq   = u3h(_body);                                      \
        u3_noun _r1   = u3t(_body);                                      \
        u3_noun _sc   = u3h(_r1);                                        \
        u3_noun _r2   = u3t(_r1);                                        \
        u3_noun _bi   = u3h(_r2);                                        \
        u3_noun _grp  = u3t(_r2);                                        \
        WD = u3t(_wq);                                                   \
        SD = u3t(_sc);                                                   \
        BD = u3t(_bi);                                                   \
        GRP = u3x_atom(_grp);                                            \
      } while (0)

    /* group_size is per-block in the Hoon but we require it to match
     * across blocks (it does for Qwen3 — MLX 2-bit uses a single group
     * size for the whole model).  Captured from block 0. */
    c3_w group_size = 0;

    u3_noun tl = blocks;
    for ( size_t i = 0; i < n_blocks; i++ ) {
      if ( tl == u3_nul ) { u3a_free(arr); return u3_none; }
      u3_noun bw = u3h(tl);
      tl = u3t(tl);

      /* Walk the 11-tuple: [q-proj k-proj v-proj o-proj gate-proj up-proj
       *                     down-proj input-ln post-attn-ln q-norm k-norm]. */
      u3_noun b = bw;
      u3_noun q_proj = u3h(b); b = u3t(b);
      u3_noun k_proj = u3h(b); b = u3t(b);
      u3_noun v_proj = u3h(b); b = u3t(b);
      u3_noun o_proj = u3h(b); b = u3t(b);
      u3_noun gate_proj = u3h(b); b = u3t(b);
      u3_noun up_proj   = u3h(b); b = u3t(b);
      u3_noun down_proj = u3h(b); b = u3t(b);
      u3_noun input_ln  = u3h(b); b = u3t(b);
      u3_noun post_ln   = u3h(b); b = u3t(b);
      u3_noun q_norm    = u3h(b);
      u3_noun k_norm    = u3t(b);

      u3_noun qw_data, qs_data, qb_data;
      u3_noun kw_data, ks_data, kb_data;
      u3_noun vw_data, vs_data, vb_data;
      u3_noun ow_data, os_data, ob_data;
      u3_noun gw_data, gs_data, gb_data;
      u3_noun uw_data, us_data, ub_data;
      u3_noun dw_data, ds_data, db_data;
      c3_w grp;

      MLX2_PROJ(q_proj,    qw_data, qs_data, qb_data, grp);
      if ( i == 0 ) group_size = grp;
      else if ( grp != group_size ) { u3a_free(arr); return u3_none; }
      MLX2_PROJ(k_proj,    kw_data, ks_data, kb_data, grp);
      MLX2_PROJ(v_proj,    vw_data, vs_data, vb_data, grp);
      MLX2_PROJ(o_proj,    ow_data, os_data, ob_data, grp);
      MLX2_PROJ(gate_proj, gw_data, gs_data, gb_data, grp);
      MLX2_PROJ(up_proj,   uw_data, us_data, ub_data, grp);
      MLX2_PROJ(down_proj, dw_data, ds_data, db_data, grp);

      c3_d sD_bytes    = (c3_d)D * (D / group_size) * 4;
      c3_d sKV_bytes   = (c3_d)(KH * Dh) * (D / group_size) * 4;
      c3_d sFF_bytes   = (c3_d)D_ff * (D / group_size) * 4;
      c3_d sDown_bytes = (c3_d)D * (D_ff / group_size) * 4;

      /* Weights: require already cached. */
      PROBE_REQ(arr[i].qw, qw_data, wD_bytes);
      PROBE_REQ(arr[i].qs, qs_data, sD_bytes);
      PROBE_REQ(arr[i].qb, qb_data, sD_bytes);
      PROBE_REQ(arr[i].kw, kw_data, wKV_bytes);
      PROBE_REQ(arr[i].ks, ks_data, sKV_bytes);
      PROBE_REQ(arr[i].kb, kb_data, sKV_bytes);
      PROBE_REQ(arr[i].vw, vw_data, wKV_bytes);
      PROBE_REQ(arr[i].vs, vs_data, sKV_bytes);
      PROBE_REQ(arr[i].vb, vb_data, sKV_bytes);
      PROBE_REQ(arr[i].ow, ow_data, wD_bytes);
      PROBE_REQ(arr[i].os, os_data, sD_bytes);
      PROBE_REQ(arr[i].ob, ob_data, sD_bytes);
      PROBE_REQ(arr[i].gate_w, gw_data, wFF_bytes);
      PROBE_REQ(arr[i].gate_s, gs_data, sFF_bytes);
      PROBE_REQ(arr[i].gate_b, gb_data, sFF_bytes);
      PROBE_REQ(arr[i].up_w,   uw_data, wFF_bytes);
      PROBE_REQ(arr[i].up_s,   us_data, sFF_bytes);
      PROBE_REQ(arr[i].up_b,   ub_data, sFF_bytes);
      PROBE_REQ(arr[i].down_w, dw_data, wDown_bytes);
      PROBE_REQ(arr[i].down_s, ds_data, sDown_bytes);
      PROBE_REQ(arr[i].down_b, db_data, sDown_bytes);

      /* Gammas: probe-or-upload. */
      u3_noun iln_data = u3t(input_ln);
      u3_noun pln_data = u3t(post_ln);
      u3_noun qn_data  = u3t(q_norm);
      u3_noun kn_data  = u3t(k_norm);
      PROBE_OR_UP(arr[i].input_ln, iln_data, gamma_D);
      PROBE_OR_UP(arr[i].post_ln,  pln_data, gamma_D);
      PROBE_OR_UP(arr[i].q_norm,   qn_data,  gamma_Dh);
      PROBE_OR_UP(arr[i].k_norm,   kn_data,  gamma_Dh);
    }

    /* cos/sin shared across all blocks.  Use the atom's actual byte
     * count (minus the MSB-pin sentinel) for the cache size so one
     * precomputed-at-chat-start rope table can be reused across ticks
     * even when S varies — the kernel still only reads first S rows. */
    uintptr_t d_cos = 0, d_sin = 0;
    c3_d cos_atom_bytes = u3r_met(3, cos_data);
    c3_d sin_atom_bytes = u3r_met(3, sin_data);
    if ( cos_atom_bytes > 0 ) cos_atom_bytes--;
    if ( sin_atom_bytes > 0 ) sin_atom_bytes--;
    if ( cos_atom_bytes < cs_bytes || sin_atom_bytes < cs_bytes ) {
      u3a_free(arr);
      return u3_none;
    }
    PROBE_OR_UP(d_cos, cos_data, cos_atom_bytes);
    PROBE_OR_UP(d_sin, sin_data, sin_atom_bytes);
    #undef PROBE_REQ
    #undef PROBE_OR_UP
    #undef MLX2_PROJ

    c3_y* x_buf = (c3_y*)u3a_malloc(x_bytes);
    c3_y* y_buf = (c3_y*)u3a_malloc(y_bytes + 1);
    u3r_bytes(0, (c3_w)x_bytes, x_buf, x_data);

    /* KV cache emission: if caller supplied a non-zero seq-hash, allocate
     * per-layer K/V dptrs and pass them to prefill.  The kernel memcpy's
     * K (post-RoPE) and V into these slots.  Decode then reads them. */
    uintptr_t* kv_k_dptrs = NULL;
    uintptr_t* kv_v_dptrs = NULL;
    c3_d kv_row_bytes = (c3_d)S * (c3_d)(KH * Dh) * 4;
    c3_w seq_hash = 0;
    if ( c3y == u3a_is_cat(seq_hash_atom) ) {
      seq_hash = u3x_atom(seq_hash_atom);
    }
    if ( seq_hash != 0 ) {
      kv_k_dptrs = (uintptr_t*)u3a_malloc(n_blocks * sizeof(uintptr_t));
      kv_v_dptrs = (uintptr_t*)u3a_malloc(n_blocks * sizeof(uintptr_t));
      for ( size_t li = 0; li < n_blocks; li++ ) {
        /* key layout: top bit = kv marker; then seq_hash<<16, layer<<4, kind */
        uint64_t key_k = (1ULL << 63)
                       | ((uint64_t)seq_hash << 16)
                       | ((uint64_t)li       << 4)
                       | 0ULL;
        uint64_t key_v = key_k | 1ULL;
        if ( backend_kv_alloc(key_k, kv_row_bytes, &kv_k_dptrs[li]) != BACKEND_OK ||
             backend_kv_alloc(key_v, kv_row_bytes, &kv_v_dptrs[li]) != BACKEND_OK ) {
          /* Alloc failed — disable KV emission for this call and fall
           * back to pure recompute.  Not an error at the API level. */
          u3a_free(kv_k_dptrs); u3a_free(kv_v_dptrs);
          kv_k_dptrs = kv_v_dptrs = NULL;
          break;
        }
      }
    }

    backend_status bs = backend_run_qwen3_forward_fp32(
      x_buf, y_buf,
      arr, n_blocks,
      d_cos, d_sin,
      S, D, D_ff, H, KH, Dh, group_size, rms_eps,
      kv_k_dptrs, kv_v_dptrs);

    if ( kv_k_dptrs ) u3a_free(kv_k_dptrs);
    if ( kv_v_dptrs ) u3a_free(kv_v_dptrs);
    u3a_free(arr);
    u3a_free(x_buf);

    if ( bs != BACKEND_OK ) {
      u3a_free(y_buf);
      return u3_none;
    }
    y_buf[y_bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(y_bytes + 1), y_buf);
    u3a_free(y_buf);

    u3_noun out_shape = u3nt(u3k(S_atom), u3k(D_atom), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, r_data);
  }

  /* +run-decode-qwen3 jet.  Sample = 8-tuple
   *   [x=tensor blocks=(list block) cfg=model-config cos=tensor sin=tensor
   *    position=@ud prev-seq-hash=@ud curr-seq-hash=@ud]
   * Axes:
   *   x:        +12 → meta +24,  data +25
   *   blocks:   +26
   *   cfg:      +54
   *   cos:      +110 → meta +220, data +221
   *   sin:      +222 → meta +444, data +445
   *   position: +446
   *   prev-seq-hash: +894
   *   curr-seq-hash: +895
   * Returns `(unit tensor)` — `[~ new-activation-at-position]` on the
   * fast path, `~` if KV cache misses (caller must fall back). */
  u3_noun
  u3wi_la_run_qwen3_decode(u3_noun cor)
  {
    u3_noun x_meta, x_data, blocks, cfg, cos_data, sin_data;
    u3_noun pos_atom, prev_hash_atom, curr_hash_atom;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24,  &x_meta,
                         (c3_w)25,  &x_data,
                         (c3_w)26,  &blocks,
                         (c3_w)54,  &cfg,
                         (c3_w)221, &cos_data,
                         (c3_w)445, &sin_data,
                         (c3_w)446, &pos_atom,
                         (c3_w)894, &prev_hash_atom,
                         (c3_w)895, &curr_hash_atom,
                         u3_nul) ) {
      return u3m_bail(c3__exit);
    }
    if ( c3n == u3a_is_cat(pos_atom) ||
         c3n == u3a_is_cat(prev_hash_atom) ||
         c3n == u3a_is_cat(curr_hash_atom) ) {
      return u3_none;
    }
    c3_w position  = u3x_atom(pos_atom);
    c3_w prev_hash = u3x_atom(prev_hash_atom);
    c3_w curr_hash = u3x_atom(curr_hash_atom);
    if ( prev_hash == 0 || curr_hash == 0 ) return u3_none;

    u3_noun x_shape = u3h(x_meta);
    u3_noun S_atom  = u3h(x_shape);
    u3_noun D_atom  = u3h(u3t(x_shape));
    if ( c3n == u3a_is_cat(S_atom) || c3n == u3a_is_cat(D_atom) ) return u3_none;
    c3_w S = u3x_atom(S_atom);
    c3_w D = u3x_atom(D_atom);
    if ( S != 1 ) return u3_none;  /* decode processes one token at a time */

    u3_noun c = cfg;
    u3_noun cfg_d_model  = u3h(c); c = u3t(c);
    u3_noun cfg_n_heads  = u3h(c); c = u3t(c);
    u3_noun cfg_n_kv_h   = u3h(c); c = u3t(c);
                                   c = u3t(c);  /* n-layers */
    u3_noun cfg_d_ff     = u3h(c); c = u3t(c);
                                   c = u3t(c);  /* vocab */
                                   c = u3t(c);  /* max-seq */
    u3_noun cfg_head_dim = u3h(c); c = u3t(c);
    u3_noun cfg_rms_eps  = u3h(c);

    c3_w H  = u3x_atom(cfg_n_heads);
    c3_w KH = u3x_atom(cfg_n_kv_h);
    c3_w Dh = u3x_atom(cfg_head_dim);
    c3_w D_ff = u3x_atom(cfg_d_ff);
    if ( u3x_atom(cfg_d_model) != D || H * Dh != D || (H % KH) != 0 )
      return u3_none;
    c3_w eps_u32 = u3x_atom(cfg_rms_eps);
    float rms_eps;
    memcpy(&rms_eps, &eps_u32, 4);

    c3_d x_bytes     = (c3_d)S * D * 4;
    c3_d y_bytes     = x_bytes;
    c3_d wD_bytes    = (c3_d)D * (D / 16) * 4;
    c3_d wKV_bytes   = (c3_d)(KH * Dh) * (D / 16) * 4;
    c3_d wFF_bytes   = (c3_d)D_ff * (D / 16) * 4;
    c3_d wDown_bytes = (c3_d)D * (D_ff / 16) * 4;
    c3_d gamma_D     = (c3_d)D * 4;
    c3_d gamma_Dh    = (c3_d)Dh * 4;
    /* cos/sin shape is [position+1, Dh] — the full table for the
     * current sequence length.  Kernel slices to row `position`. */
    c3_d cs_bytes    = (c3_d)(position + 1) * Dh * 4;

    /* Walk blocks, extract per-layer weights (must all be cache-resident). */
    size_t n_blocks = 0;
    { u3_noun t = blocks; while ( t != u3_nul ) { n_blocks++; t = u3t(t); } }
    if ( n_blocks == 0 ) return u3_none;

    qw3_block_dptrs* arr =
      (qw3_block_dptrs*)u3a_malloc(n_blocks * sizeof(qw3_block_dptrs));
    if ( !arr ) return u3_none;
    memset(arr, 0, n_blocks * sizeof(qw3_block_dptrs));

    uintptr_t* kv_k_prev =
      (uintptr_t*)u3a_malloc(n_blocks * sizeof(uintptr_t));
    uintptr_t* kv_v_prev =
      (uintptr_t*)u3a_malloc(n_blocks * sizeof(uintptr_t));
    uintptr_t* kv_k_curr =
      (uintptr_t*)u3a_malloc(n_blocks * sizeof(uintptr_t));
    uintptr_t* kv_v_curr =
      (uintptr_t*)u3a_malloc(n_blocks * sizeof(uintptr_t));

    #define DEC_FAIL() do {                                         \
        u3a_free(arr);                                              \
        u3a_free(kv_k_prev); u3a_free(kv_v_prev);                   \
        u3a_free(kv_k_curr); u3a_free(kv_v_curr);                   \
        return u3_none;                                             \
      } while (0)

    #define PROBE_REQ_DEC(dst, data_atom, total_bytes) do {         \
        c3_w _mug = u3r_mug(data_atom);                             \
        uint8_t _sent[16];                                          \
        size_t _spot = total_bytes < 16 ? (size_t)total_bytes : 16; \
        u3r_bytes(0, (c3_w)_spot, _sent, data_atom);                \
        if ( !backend_vram_probe((uint32_t)_mug, total_bytes, _sent, &dst) ) \
          DEC_FAIL();                                               \
      } while (0)
    #define PROBE_OR_UP_DEC(dst, data_atom, total_bytes) do {       \
        c3_w _mug = u3r_mug(data_atom);                             \
        uint8_t _sent[16];                                          \
        size_t _spot = total_bytes < 16 ? (size_t)total_bytes : 16; \
        u3r_bytes(0, (c3_w)_spot, _sent, data_atom);                \
        if ( !backend_vram_probe((uint32_t)_mug, total_bytes, _sent, &dst) ) { \
          c3_y* _tmp = (c3_y*)u3a_malloc(total_bytes);              \
          u3r_bytes(0, (c3_w)total_bytes, _tmp, data_atom);         \
          backend_status _bs = backend_vram_upload(                 \
            _tmp, total_bytes, (uint32_t)_mug, &dst);               \
          u3a_free(_tmp);                                           \
          if ( _bs != BACKEND_OK ) DEC_FAIL();                      \
        }                                                           \
      } while (0)
    #define MLX2_PROJ_DEC(proj, WD, SD, BD, GRP) do {               \
        if ( u3h(proj) != c3_s4('m','l','x','2') ) DEC_FAIL();      \
        u3_noun _body = u3t(proj);                                  \
        u3_noun _wq   = u3h(_body);                                 \
        u3_noun _r1   = u3t(_body);                                 \
        u3_noun _sc   = u3h(_r1);                                   \
        u3_noun _r2   = u3t(_r1);                                   \
        u3_noun _bi   = u3h(_r2);                                   \
        u3_noun _grp  = u3t(_r2);                                   \
        WD = u3t(_wq);                                              \
        SD = u3t(_sc);                                              \
        BD = u3t(_bi);                                              \
        GRP = u3x_atom(_grp);                                       \
      } while (0)

    c3_w group_size = 0;
    c3_d kv_prev_bytes = (c3_d)position * (KH * Dh) * 4;
    c3_d kv_curr_bytes = (c3_d)(position + 1) * (KH * Dh) * 4;

    u3_noun tl = blocks;
    for ( size_t i = 0; i < n_blocks; i++ ) {
      if ( tl == u3_nul ) DEC_FAIL();
      u3_noun bw = u3h(tl);
      tl = u3t(tl);

      u3_noun b = bw;
      u3_noun q_proj = u3h(b); b = u3t(b);
      u3_noun k_proj = u3h(b); b = u3t(b);
      u3_noun v_proj = u3h(b); b = u3t(b);
      u3_noun o_proj = u3h(b); b = u3t(b);
      u3_noun gate_proj = u3h(b); b = u3t(b);
      u3_noun up_proj   = u3h(b); b = u3t(b);
      u3_noun down_proj = u3h(b); b = u3t(b);
      u3_noun input_ln  = u3h(b); b = u3t(b);
      u3_noun post_ln   = u3h(b); b = u3t(b);
      u3_noun q_norm    = u3h(b);
      u3_noun k_norm    = u3t(b);

      u3_noun qw_data, qs_data, qb_data;
      u3_noun kw_data, ks_data, kb_data;
      u3_noun vw_data, vs_data, vb_data;
      u3_noun ow_data, os_data, ob_data;
      u3_noun gw_data, gs_data, gb_data;
      u3_noun uw_data, us_data, ub_data;
      u3_noun dw_data, ds_data, db_data;
      c3_w grp;

      MLX2_PROJ_DEC(q_proj,    qw_data, qs_data, qb_data, grp);
      if ( i == 0 ) group_size = grp;
      else if ( grp != group_size ) DEC_FAIL();
      MLX2_PROJ_DEC(k_proj,    kw_data, ks_data, kb_data, grp);
      MLX2_PROJ_DEC(v_proj,    vw_data, vs_data, vb_data, grp);
      MLX2_PROJ_DEC(o_proj,    ow_data, os_data, ob_data, grp);
      MLX2_PROJ_DEC(gate_proj, gw_data, gs_data, gb_data, grp);
      MLX2_PROJ_DEC(up_proj,   uw_data, us_data, ub_data, grp);
      MLX2_PROJ_DEC(down_proj, dw_data, ds_data, db_data, grp);

      c3_d sD_bytes    = (c3_d)D * (D / group_size) * 4;
      c3_d sKV_bytes   = (c3_d)(KH * Dh) * (D / group_size) * 4;
      c3_d sFF_bytes   = (c3_d)D_ff * (D / group_size) * 4;
      c3_d sDown_bytes = (c3_d)D * (D_ff / group_size) * 4;

      PROBE_REQ_DEC(arr[i].qw, qw_data, wD_bytes);
      PROBE_REQ_DEC(arr[i].qs, qs_data, sD_bytes);
      PROBE_REQ_DEC(arr[i].qb, qb_data, sD_bytes);
      PROBE_REQ_DEC(arr[i].kw, kw_data, wKV_bytes);
      PROBE_REQ_DEC(arr[i].ks, ks_data, sKV_bytes);
      PROBE_REQ_DEC(arr[i].kb, kb_data, sKV_bytes);
      PROBE_REQ_DEC(arr[i].vw, vw_data, wKV_bytes);
      PROBE_REQ_DEC(arr[i].vs, vs_data, sKV_bytes);
      PROBE_REQ_DEC(arr[i].vb, vb_data, sKV_bytes);
      PROBE_REQ_DEC(arr[i].ow, ow_data, wD_bytes);
      PROBE_REQ_DEC(arr[i].os, os_data, sD_bytes);
      PROBE_REQ_DEC(arr[i].ob, ob_data, sD_bytes);
      PROBE_REQ_DEC(arr[i].gate_w, gw_data, wFF_bytes);
      PROBE_REQ_DEC(arr[i].gate_s, gs_data, sFF_bytes);
      PROBE_REQ_DEC(arr[i].gate_b, gb_data, sFF_bytes);
      PROBE_REQ_DEC(arr[i].up_w,   uw_data, wFF_bytes);
      PROBE_REQ_DEC(arr[i].up_s,   us_data, sFF_bytes);
      PROBE_REQ_DEC(arr[i].up_b,   ub_data, sFF_bytes);
      PROBE_REQ_DEC(arr[i].down_w, dw_data, wDown_bytes);
      PROBE_REQ_DEC(arr[i].down_s, ds_data, sDown_bytes);
      PROBE_REQ_DEC(arr[i].down_b, db_data, sDown_bytes);

      u3_noun iln_data = u3t(input_ln);
      u3_noun pln_data = u3t(post_ln);
      u3_noun qn_data  = u3t(q_norm);
      u3_noun kn_data  = u3t(k_norm);
      PROBE_OR_UP_DEC(arr[i].input_ln, iln_data, gamma_D);
      PROBE_OR_UP_DEC(arr[i].post_ln,  pln_data, gamma_D);
      PROBE_OR_UP_DEC(arr[i].q_norm,   qn_data,  gamma_Dh);
      PROBE_OR_UP_DEC(arr[i].k_norm,   kn_data,  gamma_Dh);

      /* KV cache: probe prev (must hit) and alloc curr. */
      uint64_t key_k_prev = (1ULL << 63) | ((uint64_t)prev_hash << 16)
                          | ((uint64_t)i << 4) | 0ULL;
      uint64_t key_v_prev = key_k_prev | 1ULL;
      uint64_t key_k_curr = (1ULL << 63) | ((uint64_t)curr_hash << 16)
                          | ((uint64_t)i << 4) | 0ULL;
      uint64_t key_v_curr = key_k_curr | 1ULL;

      size_t prev_b_out = 0, unused = 0;
      if ( !backend_kv_probe(key_k_prev, &kv_k_prev[i], &prev_b_out) ) DEC_FAIL();
      if ( prev_b_out != kv_prev_bytes ) DEC_FAIL();
      if ( !backend_kv_probe(key_v_prev, &kv_v_prev[i], &unused) ) DEC_FAIL();

      if ( backend_kv_alloc(key_k_curr, kv_curr_bytes, &kv_k_curr[i]) != BACKEND_OK )
        DEC_FAIL();
      if ( backend_kv_alloc(key_v_curr, kv_curr_bytes, &kv_v_curr[i]) != BACKEND_OK )
        DEC_FAIL();
    }
    #undef PROBE_REQ_DEC
    #undef PROBE_OR_UP_DEC
    #undef MLX2_PROJ_DEC

    /* cos/sin for this sequence length — probe-or-upload like prefill.
     * Use the atom's actual byte count (minus the MSB sentinel) so a
     * single precomputed rope table can be reused across ticks; the
     * kernel only reads the first (position+1) rows regardless. */
    uintptr_t d_cos = 0, d_sin = 0;
    {
      c3_d cos_atom_bytes = u3r_met(3, cos_data);
      c3_d sin_atom_bytes = u3r_met(3, sin_data);
      if ( cos_atom_bytes > 0 ) cos_atom_bytes--;
      if ( sin_atom_bytes > 0 ) sin_atom_bytes--;
      if ( cos_atom_bytes < cs_bytes || sin_atom_bytes < cs_bytes ) DEC_FAIL();

      c3_w _mug;
      uint8_t _sent[16];
      size_t _spot;
      #define POU_CS(dst, data_atom, atom_bytes) do {                 \
          _mug = u3r_mug(data_atom);                                  \
          _spot = atom_bytes < 16 ? (size_t)atom_bytes : 16;          \
          u3r_bytes(0, (c3_w)_spot, _sent, data_atom);                \
          if ( !backend_vram_probe((uint32_t)_mug, atom_bytes, _sent, &dst) ) { \
            c3_y* _tmp = (c3_y*)u3a_malloc(atom_bytes);               \
            u3r_bytes(0, (c3_w)atom_bytes, _tmp, data_atom);          \
            backend_status _bs = backend_vram_upload(                 \
              _tmp, atom_bytes, (uint32_t)_mug, &dst);                \
            u3a_free(_tmp);                                           \
            if ( _bs != BACKEND_OK ) DEC_FAIL();                      \
          }                                                           \
        } while (0)
      POU_CS(d_cos, cos_data, cos_atom_bytes);
      POU_CS(d_sin, sin_data, sin_atom_bytes);
      #undef POU_CS
    }

    c3_y* x_buf = (c3_y*)u3a_malloc(x_bytes);
    c3_y* y_buf = (c3_y*)u3a_malloc(y_bytes + 1);
    u3r_bytes(0, (c3_w)x_bytes, x_buf, x_data);

    backend_status bs = backend_run_qwen3_decode_fp32(
      x_buf, y_buf,
      arr, n_blocks,
      d_cos, d_sin,
      position,
      kv_k_prev, kv_v_prev, kv_k_curr, kv_v_curr,
      D, D_ff, H, KH, Dh, group_size, rms_eps);

    u3a_free(arr);
    u3a_free(kv_k_prev); u3a_free(kv_v_prev);
    u3a_free(kv_k_curr); u3a_free(kv_v_curr);
    u3a_free(x_buf);

    #undef DEC_FAIL

    if ( bs != BACKEND_OK ) {
      u3a_free(y_buf);
      return u3_none;
    }
    y_buf[y_bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(y_bytes + 1), y_buf);
    u3a_free(y_buf);

    u3_noun out_shape = u3nt(u3k(S_atom), u3k(D_atom), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    u3_noun tensor    = u3nc(out_meta, r_data);
    /* Return `[~ tensor]` (unit-just) so Hoon callers can pattern-match. */
    return u3nc(u3_nul, tensor);
  }

  u3_noun
  u3wi_la_run_qwen3_block(u3_noun cor)
  {
    /* Sample axes for 5-tuple [x bw cfg cos sin]:
         x        +12   (meta 24, data 25)
         bw       +26
         cfg      +54
         cos      +110  (meta 220, data 221)
         sin      +111  (meta 222, data 223) */
    u3_noun x_meta, x_data, bw, cfg, cos_data, sin_data;
    if ( c3n == u3r_mean(cor,
                         (c3_w)24,  &x_meta,
                         (c3_w)25,  &x_data,
                         (c3_w)26,  &bw,
                         (c3_w)54,  &cfg,
                         (c3_w)221, &cos_data,
                         (c3_w)223, &sin_data,
                         u3_nul) ) {
      return u3m_bail(c3__exit);
    }

    /* Extract x shape: [S, D]. */
    u3_noun x_shape = u3h(x_meta);
    u3_noun S_atom  = u3h(x_shape);
    u3_noun D_atom  = u3h(u3t(x_shape));
    if ( c3n == u3a_is_cat(S_atom) || c3n == u3a_is_cat(D_atom) ) return u3_none;
    c3_w S = u3x_atom(S_atom);
    c3_w D = u3x_atom(D_atom);

    /* Walk config cell: [d-model n-heads n-kv-heads n-layers d-ff
     *                    vocab max-seq head-dim rms-eps ...]. */
    u3_noun c = cfg;
    u3_noun cfg_d_model   = u3h(c); c = u3t(c);
    u3_noun cfg_n_heads   = u3h(c); c = u3t(c);
    u3_noun cfg_n_kv_h    = u3h(c); c = u3t(c);
                                    c = u3t(c);       /* n-layers unused */
    u3_noun cfg_d_ff      = u3h(c); c = u3t(c);
                                    c = u3t(c);       /* vocab unused */
                                    c = u3t(c);       /* max-seq unused */
    u3_noun cfg_head_dim  = u3h(c); c = u3t(c);
    u3_noun cfg_rms_eps   = u3h(c);                    /* @rs */

    c3_w H  = u3x_atom(cfg_n_heads);
    c3_w KH = u3x_atom(cfg_n_kv_h);
    c3_w Dh = u3x_atom(cfg_head_dim);
    c3_w D_ff = u3x_atom(cfg_d_ff);
    if ( u3x_atom(cfg_d_model) != D || H * Dh != D || (H % KH) != 0 )
      return u3_none;
    c3_w eps_u32 = u3x_atom(cfg_rms_eps);
    float rms_eps;
    memcpy(&rms_eps, &eps_u32, 4);

    /* Walk block weights:  bw = [q-proj k-proj v-proj o-proj
     *                            gate-proj up-proj down-proj
     *                            input-ln post-attn-ln q-norm k-norm]
     * Projections are [%mlx2 wq scales biases group-size]; gammas are
     * plain tensors. */
    u3_noun b = bw;
    u3_noun q_proj = u3h(b); b = u3t(b);
    u3_noun k_proj = u3h(b); b = u3t(b);
    u3_noun v_proj = u3h(b); b = u3t(b);
    u3_noun o_proj = u3h(b); b = u3t(b);
    u3_noun gate_proj = u3h(b); b = u3t(b);
    u3_noun up_proj   = u3h(b); b = u3t(b);
    u3_noun down_proj = u3h(b); b = u3t(b);
    u3_noun input_ln  = u3h(b); b = u3t(b);
    u3_noun post_ln   = u3h(b); b = u3t(b);
    u3_noun q_norm    = u3h(b);
    u3_noun k_norm    = u3t(b);

    /* Helper lambda via local var: extract mlx2 projection into (w, s, b,
     * group) atoms — manual because C has no closures. */
    #define MLX2_PROJ(proj, WD, SD, BD, GRP) do { \
        if ( u3h(proj) != c3_s4('m','l','x','2') ) return u3_none; \
        u3_noun _body = u3t(proj);                   \
        u3_noun _wq   = u3h(_body);                  \
        u3_noun _r1   = u3t(_body);                  \
        u3_noun _sc   = u3h(_r1);                    \
        u3_noun _r2   = u3t(_r1);                    \
        u3_noun _bi   = u3h(_r2);                    \
        u3_noun _grp  = u3t(_r2);                    \
        WD = u3t(_wq);                                \
        SD = u3t(_sc);                                \
        BD = u3t(_bi);                                \
        GRP = u3x_atom(_grp);                         \
      } while (0)

    u3_noun qw_data, qs_data, qb_data;
    u3_noun kw_data, ks_data, kb_data;
    u3_noun vw_data, vs_data, vb_data;
    u3_noun ow_data, os_data, ob_data;
    u3_noun gw_data, gs_data, gb_data;
    u3_noun uw_data, us_data, ub_data;
    u3_noun dw_data, ds_data, db_data;
    c3_w group_size;
    MLX2_PROJ(q_proj,    qw_data, qs_data, qb_data, group_size);
    MLX2_PROJ(k_proj,    kw_data, ks_data, kb_data, group_size);
    MLX2_PROJ(v_proj,    vw_data, vs_data, vb_data, group_size);
    MLX2_PROJ(o_proj,    ow_data, os_data, ob_data, group_size);
    MLX2_PROJ(gate_proj, gw_data, gs_data, gb_data, group_size);
    MLX2_PROJ(up_proj,   uw_data, us_data, ub_data, group_size);
    MLX2_PROJ(down_proj, dw_data, ds_data, db_data, group_size);
    #undef MLX2_PROJ

    /* Extract gamma data atoms. */
    u3_noun iln_data = u3t(input_ln);
    u3_noun pln_data = u3t(post_ln);
    u3_noun qn_data  = u3t(q_norm);
    u3_noun kn_data  = u3t(k_norm);

    /* Byte sizes. */
    c3_d x_bytes     = (c3_d)S * D * 4;
    c3_d y_bytes     = x_bytes;
    c3_d wD_bytes    = (c3_d)D * (D / 16) * 4;
    c3_d sD_bytes    = (c3_d)D * (D / group_size) * 4;
    c3_d wKV_bytes   = (c3_d)(KH * Dh) * (D / 16) * 4;
    c3_d sKV_bytes   = (c3_d)(KH * Dh) * (D / group_size) * 4;
    c3_d wFF_bytes   = (c3_d)D_ff * (D / 16) * 4;
    c3_d sFF_bytes   = (c3_d)D_ff * (D / group_size) * 4;
    c3_d wDown_bytes = (c3_d)D * (D_ff / 16) * 4;
    c3_d sDown_bytes = (c3_d)D * (D_ff / group_size) * 4;
    c3_d gamma_D     = (c3_d)D * 4;
    c3_d gamma_Dh    = (c3_d)Dh * 4;
    c3_d cs_bytes    = (c3_d)S * Dh * 4;

    /* Helper: resolve a weight atom to a VRAM dptr, either via cache
     * probe (hit → skip host read) or by u3r_bytes + full upload on
     * miss.  On miss, caller is responsible for u3a_free'ing the host
     * buffer after the backend call returns. */
    #define PROBE_OR_UPLOAD(dst_dptr, dst_host, data_atom, total_bytes) do {  \
        c3_w _mug = u3r_mug(data_atom);                                       \
        uint8_t _sent[16];                                                    \
        size_t _spot = total_bytes < 16 ? (size_t)total_bytes : 16;           \
        u3r_bytes(0, (c3_w)_spot, _sent, data_atom);                          \
        uintptr_t _dptr = 0;                                                  \
        int _hit = backend_vram_probe((uint32_t)_mug, total_bytes, _sent, &_dptr); \
        if ( _hit ) { dst_dptr = _dptr; dst_host = NULL; }                    \
        else {                                                                \
          dst_host = (c3_y*)u3a_malloc(total_bytes);                          \
          u3r_bytes(0, (c3_w)total_bytes, dst_host, data_atom);               \
          /* Will be uploaded by the backend's warm-cache path below. */      \
          dst_dptr = 0;                                                       \
        }                                                                     \
      } while (0)

    /* For this first cut, weights MUST be cache-resident.  Fall back
     * to u3_none (Hoon path) otherwise; the fallback will populate the
     * cache via the existing mmul-mlx2 jet calls, so a subsequent
     * forward will find everything warm. */
    uintptr_t d_qw=0,d_qs=0,d_qb=0, d_kw=0,d_ks=0,d_kb=0;
    uintptr_t d_vw=0,d_vs=0,d_vb=0, d_ow=0,d_os=0,d_ob=0;
    uintptr_t d_gw=0,d_gs=0,d_gb=0, d_uw=0,d_us=0,d_ub=0;
    uintptr_t d_dw=0,d_ds=0,d_db=0;

    #define PROBE_REQ(dst, data_atom, total_bytes) do { \
        c3_w _mug = u3r_mug(data_atom);                 \
        uint8_t _sent[16];                              \
        size_t _spot = total_bytes < 16 ? (size_t)total_bytes : 16; \
        u3r_bytes(0, (c3_w)_spot, _sent, data_atom);    \
        if ( !backend_vram_probe((uint32_t)_mug, total_bytes, _sent, &dst) ) \
          return u3_none;                                \
      } while (0)

    PROBE_REQ(d_qw, qw_data, wD_bytes);
    PROBE_REQ(d_qs, qs_data, sD_bytes);
    PROBE_REQ(d_qb, qb_data, sD_bytes);
    PROBE_REQ(d_kw, kw_data, wKV_bytes);
    PROBE_REQ(d_ks, ks_data, sKV_bytes);
    PROBE_REQ(d_kb, kb_data, sKV_bytes);
    PROBE_REQ(d_vw, vw_data, wKV_bytes);
    PROBE_REQ(d_vs, vs_data, sKV_bytes);
    PROBE_REQ(d_vb, vb_data, sKV_bytes);
    PROBE_REQ(d_ow, ow_data, wD_bytes);
    PROBE_REQ(d_os, os_data, sD_bytes);
    PROBE_REQ(d_ob, ob_data, sD_bytes);
    PROBE_REQ(d_gw, gw_data, wFF_bytes);
    PROBE_REQ(d_gs, gs_data, sFF_bytes);
    PROBE_REQ(d_gb, gb_data, sFF_bytes);
    PROBE_REQ(d_uw, uw_data, wFF_bytes);
    PROBE_REQ(d_us, us_data, sFF_bytes);
    PROBE_REQ(d_ub, ub_data, sFF_bytes);
    PROBE_REQ(d_dw, dw_data, wDown_bytes);
    PROBE_REQ(d_ds, ds_data, sDown_bytes);
    PROBE_REQ(d_db, db_data, sDown_bytes);
    #undef PROBE_REQ
    #undef PROBE_OR_UPLOAD

    /* Probe-or-upload for cos/sin and the four gammas.  Content-hashed
     * by u3r_mug of the data atom: cache hit on repeat (every block of
     * one forward sees the same cos/sin; every forward sees the same
     * gammas per block); miss on first sighting. */
    #define PROBE_OR_UP(dst, data_atom, total_bytes) do {               \
        c3_w _mug = u3r_mug(data_atom);                                 \
        uint8_t _sent[16];                                              \
        size_t _spot = total_bytes < 16 ? (size_t)total_bytes : 16;     \
        u3r_bytes(0, (c3_w)_spot, _sent, data_atom);                    \
        if ( !backend_vram_probe((uint32_t)_mug, total_bytes, _sent, &dst) ) { \
          c3_y* _tmp = (c3_y*)u3a_malloc(total_bytes);                  \
          u3r_bytes(0, (c3_w)total_bytes, _tmp, data_atom);             \
          backend_status _bs = backend_vram_upload(_tmp, total_bytes, (uint32_t)_mug, &dst); \
          u3a_free(_tmp);                                               \
          if ( _bs != BACKEND_OK ) return u3_none;                      \
        }                                                               \
      } while (0)

    uintptr_t d_iln=0, d_pln=0, d_qn=0, d_kn=0, d_cos=0, d_sin=0;
    PROBE_OR_UP(d_iln, iln_data, gamma_D);
    PROBE_OR_UP(d_pln, pln_data, gamma_D);
    PROBE_OR_UP(d_qn,  qn_data,  gamma_Dh);
    PROBE_OR_UP(d_kn,  kn_data,  gamma_Dh);
    PROBE_OR_UP(d_cos, cos_data, cs_bytes);
    PROBE_OR_UP(d_sin, sin_data, cs_bytes);
    #undef PROBE_OR_UP

    /* Only x still goes HtoD per call. */
    c3_y* x_buf = (c3_y*)u3a_malloc(x_bytes);
    c3_y* y_buf = (c3_y*)u3a_malloc(y_bytes + 1);
    u3r_bytes(0, (c3_w)x_bytes, x_buf, x_data);

    backend_status bs = backend_run_qwen3_block_fp32(
      x_buf, y_buf,
      d_qw, d_qs, d_qb,  d_kw, d_ks, d_kb,  d_vw, d_vs, d_vb,  d_ow, d_os, d_ob,
      d_gw, d_gs, d_gb,  d_uw, d_us, d_ub,  d_dw, d_ds, d_db,
      d_iln, d_pln, d_qn, d_kn,
      d_cos, d_sin,
      S, D, D_ff, H, KH, Dh, group_size, rms_eps);

    u3a_free(x_buf);

    if ( bs != BACKEND_OK ) {
      u3a_free(y_buf);
      return u3_none;
    }
    y_buf[y_bytes] = 0x01;
    u3_noun r_data = u3i_bytes((c3_w)(y_bytes + 1), y_buf);
    u3a_free(y_buf);

    /* Output shape = input shape [S, D]. */
    u3_noun out_shape = u3nt(u3k(S_atom), u3k(D_atom), u3_nul);
    u3_noun out_meta  = u3nq(out_shape, u3i_word(5), c3__i754, 0);
    return u3nc(out_meta, r_data);
  }

  u3_noun
  u3wi_la_min(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_min_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3nt(0x1, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_max(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_max_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3nt(0x1, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_abs(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_abs_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_gth(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_gth_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3k(x_meta), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_gte(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_gte_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3k(x_meta), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_lth(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_lth_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3k(x_meta), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_lte(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_lte_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3k(x_meta), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_adds(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         u3_nul) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_adds_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_subs(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         u3_nul) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_subs_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_muls(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         u3_nul) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
            x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_muls_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_divs(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         u3_nul) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
            x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_divs_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_mods(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         u3_nul) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
            x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_mods_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_dot(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_dot_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            c3_d len_x0 = _get_dims(x_shape)[0];
            return u3nc(u3nq(u3nt(len_x0, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_transpose(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(cor)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        u3_noun r_data = u3qi_la_transpose(x_data, x_shape, x_bloq);
        if (r_data == u3_none) { return u3_none; }
        return u3nc(u3nq(u3nt(u3k(u3h(x_shape)), u3k(u3h(u3t(x_shape))), u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);
      }
    }
  }

  u3_noun
  u3wi_la_linspace(u3_noun cor)
  {
    u3_noun x_meta, a, b, n, rnd;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_12, &a,
                         u3x_sam_13, &b,
                         u3x_sam_7, &n,
                         u3_nul))
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(n) ||
           (n < 1)                    // crash on zero size
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_linspace_i754(a, b, n, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            x_shape = u3nc(u3x_atom(n), u3_nul);
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_range(u3_noun cor)
  {
    u3_noun x_meta, a, b, d, rnd;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_12, &a,
                         u3x_sam_13, &b,
                         u3x_sam_7, &d,
                         u3_nul))
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_range_i754(a, b, d, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            c3_d a_, b_, d_;
            c3_ds n_;
            switch (x_bloq) {
              case 4:
                u3r_bytes(0, 2, (c3_y*)&a_, a);
                u3r_bytes(0, 2, (c3_y*)&b_, b);
                u3r_bytes(0, 2, (c3_y*)&d_, d);
                n_ = f16_to_i64(f16_ceil(f16_div(f16_sub((float16_t){b_}, (float16_t){a_}), (float16_t){d_})), softfloat_round_minMag, false) - 1;
                break;
              case 5:
                u3r_bytes(0, 4, (c3_y*)&a_, a);
                u3r_bytes(0, 4, (c3_y*)&b_, b);
                u3r_bytes(0, 4, (c3_y*)&d_, d);
                n_ = f32_to_i64(f32_ceil(f32_div(f32_sub((float32_t){b_}, (float32_t){a_}), (float32_t){d_})), softfloat_round_minMag, false) - 1;
                break;
              case 6:
                u3r_bytes(0, 8, (c3_y*)&a_, a);
                u3r_bytes(0, 8, (c3_y*)&b_, b);
                u3r_bytes(0, 8, (c3_y*)&d_, d);
                n_ = f64_to_i64(f64_ceil(f64_div(f64_sub((float64_t){b_}, (float64_t){a_}), (float64_t){d_})), softfloat_round_minMag, false) - 1;
                break;
              case 7: {
                c3_d a__[2], b__[2], d__[2];
                u3r_bytes(0, 16, (c3_y*)&a__, a);
                u3r_bytes(0, 16, (c3_y*)&b__, b);
                u3r_bytes(0, 16, (c3_y*)&d__, d);
                float128_t tmp;
                f128M_sub((float128_t*)&b__, (float128_t*)&a__, &tmp);
                f128M_div(&tmp, (float128_t*)&d__, &tmp);
                f128M_ceil(&tmp, &tmp);
                n_ = f128M_to_i64(&tmp, softfloat_round_minMag, false) - 1;
                break;}
            }
            u3_noun n = u3i_chub(n_+1);
            x_shape = u3nc(u3k(n), u3_nul);
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_diag(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(cor)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        u3_noun r_data = u3qi_la_diag(x_data, x_shape, x_bloq);
        if (r_data == u3_none) { return u3_none; }
        c3_d len_x0 = _get_dims(x_shape)[0];
        return u3nc(u3nq(u3nt(len_x0, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);
      }
    }
  }

  u3_noun
  u3wi_la_trace(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         u3_nul) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      if ( c3n == u3r_mean(x_meta,
                            2, &x_shape,
                            6, &x_bloq,
                           14, &x_kind,
                           15, &x_tail,
                            u3_nul)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_trace_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3nt(0x1, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_mmul(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         u3_nul) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind,
              y_shape,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      y_shape = u3h(y_meta);          //  2
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == _check(u3nc(x_meta, x_data)) ||
           c3n == _check(u3nc(y_meta, y_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_mmul_i754(x_data, y_data, x_shape, y_shape, x_bloq);
            // result is already [meta data]
            return r_data;

          default:
            return u3_none;
        }
      }
    }
  }
