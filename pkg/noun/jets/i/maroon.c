/// @file  maroon.c
///
/// Jets for /lib/maroon.
///
/// dequant-q8: int8 ray + scale -> fp32 ray.
///
/// Each int8 byte is interpreted as a signed 8-bit integer, converted to
/// fp32 and multiplied by the scale using SoftFloat for bit-deterministic
/// results across platforms. Output is stored as IEEE 754 single-precision
/// in a new ray with the same shape but bloq=5, kind=%i754.

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "softfloat.h"

#include <string.h>

  union sing_m {
    float32_t s;
    uint32_t  c;
  };

/* dequant-q8(r=ray scale=@rs) -> ray
**
** A ray is [meta data=@ux] where meta=[shape bloq kind tail].
** For int8 ray: bloq=3, kind=%uint, data=N bytes + 1 MSB pin byte.
** For fp32 ray: bloq=5, kind=%i754, data=N*4 bytes + 1 pin byte at bit N*32.
**
** Uses SoftFloat (round-nearest-even) for int->float conversion and mul,
** matching the Hoon reference which uses `~(. rs:math [%n .1e-5])`.
*/
  u3_noun
  u3qi_maroon_dequant_q8(u3_noun r_meta,
                         u3_atom r_data,
                         u3_atom scale_atom)
  {
    union sing_m scale_u;
    scale_u.c = u3r_word(0, scale_atom);
    float32_t scale_f = scale_u.s;

    u3_noun shape_list = u3h(r_meta);

    uint64_t n_elements = 1;
    {
      u3_noun cur = u3k(shape_list);
      while (u3_nul != cur) {
        if (c3n == u3du(cur)) { u3z(cur); return u3_none; }
        u3_noun head = u3h(cur);
        if (c3n == u3ud(head)) { u3z(cur); return u3_none; }
        n_elements *= (uint64_t)head;
        u3_noun next = u3k(u3t(cur));
        u3z(cur);
        cur = next;
      }
    }

    if (n_elements == 0 || n_elements > (1ULL << 30)) {
      return u3_none;
    }

    uint64_t in_bytes = n_elements;
    uint64_t out_bytes = n_elements * 4;

    uint8_t* in_buf = (uint8_t*)c3_malloc(in_bytes);
    u3r_bytes(0, in_bytes, in_buf, r_data);

    uint8_t* out_buf = (uint8_t*)c3_calloc(out_bytes + 1);

    //  Hoon reference uses round-nearest-even throughout; set it here and
    //  restore after (don't trust that callers always set it).
    uint_fast8_t saved_mode = softfloat_roundingMode;
    softfloat_roundingMode = softfloat_round_near_even;

    for (uint64_t i = 0; i < n_elements; i++) {
      int8_t signed_val = (int8_t)in_buf[i];
      float32_t fv = i32_to_f32((int32_t)signed_val);
      float32_t result = f32_mul(fv, scale_f);
      union sing_m u;
      u.s = result;
      out_buf[i * 4 + 0] = (uint8_t)(u.c & 0xff);
      out_buf[i * 4 + 1] = (uint8_t)((u.c >> 8) & 0xff);
      out_buf[i * 4 + 2] = (uint8_t)((u.c >> 16) & 0xff);
      out_buf[i * 4 + 3] = (uint8_t)((u.c >> 24) & 0xff);
    }

    softfloat_roundingMode = saved_mode;

    out_buf[out_bytes] = 0x01;

    c3_free(in_buf);

    u3_atom out_data = u3i_bytes((c3_w)(out_bytes + 1), out_buf);
    c3_free(out_buf);

    //  %i754 as @tas: bytes "i754" little-endian
    u3_atom kind_i754 = (u3_atom)0x34353769;

    u3_noun new_meta = u3nq(u3k(shape_list),
                            u3i_word(5),
                            u3k(kind_i754),
                            u3i_word(0));

    return u3nc(new_meta, out_data);
  }

  u3_noun
  u3wi_maroon_dequant_q8(u3_noun cor)
  {
    u3_noun r_meta, r_data, scale;

    if (c3n == u3r_mean(cor,
                        u3x_sam_4, &r_meta,
                        u3x_sam_5, &r_data,
                        u3x_sam_3, &scale,
                        0))
    {
      return u3m_bail(c3__exit);
    }

    u3_noun result = u3qi_maroon_dequant_q8(r_meta, r_data, scale);
    if (u3_none == result) {
      return u3m_bail(c3__exit);
    }
    return result;
  }
