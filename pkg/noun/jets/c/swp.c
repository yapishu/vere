/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qc_swp(u3_atom a,
         u3_atom b)
{
  if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
    return u3m_bail(c3__fail);
  }
  c3_g a_g = a;
  c3_w len_w = u3r_met(a_g, b);
  u3i_slab sab_u;

  if ( 3 == a_g ) {
    if ( len_w <= 1 ) {
      return u3k(b);
    }
    STATIC_ASSERT(c3_endian == c3_endian_little, "");
    c3_w wor_w;
    const c3_y* end_y = (const c3_y*)u3r_word_buffer(&b, &wor_w) + len_w;
    u3i_slab_bare(&sab_u, 3, len_w);
    // Clear the padding above the final byte in the slab's last word.
    sab_u.buf_w[sab_u.len_w - 1] = 0;

    // The disjoint byte ranges let the compiler shuffle several bytes at once.
    // https://llvm.org/docs/Vectorizers.html#reverse-iterators
    c3_y* restrict dst_y = sab_u.buf_y;
    for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
      dst_y[i_w] = *--end_y;
    }
    return u3i_slab_mint(&sab_u);
  }

  u3i_slab_init(&sab_u, a_g, len_w);
  for (c3_w i = 0; i < len_w; i++) {
    u3r_chop(a_g, i, 1, len_w - i - 1, sab_u.buf_w, b);
  }
         
  return u3i_slab_mint(&sab_u);
}

u3_noun
u3wc_swp(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b});

  if (  (c3n == u3ud(a))
     || (c3n == u3ud(b)) )
  {
    return u3m_bail(c3__exit);
  }

  return u3qc_swp(a, b);
 }

u3_noun
u3kc_swp(u3_atom a,
         u3_atom b)
{
  u3_noun pro = u3qc_swp(a, b);
  u3z(a); u3z(b);
  return pro;
}
