#ifndef U3_JETS_C_ORDER_H
#define U3_JETS_C_ORDER_H

#include "noun.h"

/* +aor compares atoms low byte first. Past the end, an atom has only
** zero bytes, so the shorter of two equal prefixes comes first.
*/
static inline c3_o
_order_alpha(u3_atom a, u3_atom b)
{
  c3_w        len_a = u3r_met(3, a);
  c3_w        len_b = u3r_met(3, b);
  c3_w        wor_w;
  const void* buf_a = u3r_word_buffer(&a, &wor_w);
  const void* buf_b = u3r_word_buffer(&b, &wor_w);

  STATIC_ASSERT(c3_endian == c3_endian_little, "atom byte order");
  int cmp_i = memcmp(buf_a, buf_b, c3_min(len_a, len_b));
  return cmp_i ? __(cmp_i < 0) : __(len_a <= len_b);
}

/* Whole-noun equality can recognize a shared graph in one walk even
** when its cells cannot be unified. Once a difference is known, only
** the first unequal field determines the order.
*/
static inline c3_o
_order_nouns(u3_noun a, u3_noun b, c3_t alpha_t)
{
  if ( a != b && c3y == u3du(a) && c3y == u3du(b) &&
       c3y == u3r_sing(a, b) ) {
    return c3y;
  }

  while ( a != b ) {
    if ( c3y == u3ud(a) ) {
      if ( c3n == u3ud(b) ) return c3y;
      return alpha_t ? _order_alpha(a, b) : __(u3r_comp(a, b) <= 0);
    }
    if ( c3y == u3ud(b) ) return c3n;

    u3_noun hed_a = u3h(a), hed_b = u3h(b);
    if ( c3y == u3r_sing(hed_a, hed_b) ) {
      a = u3t(a);
      b = u3t(b);
    }
    else {
      a = hed_a;
      b = hed_b;
    }
  }
  return c3y;
}

#endif
