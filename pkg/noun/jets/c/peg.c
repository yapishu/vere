#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

//  Shift the prefix into its final position and fill the unused low bits.
static inline void
_peg_shift(c3_w* restrict out_w, const c3_w* in_w, c3_w len_w,
           c3_w off_w, c3_w low_w, c3_w extra_w)
{
  c3_w back_w = u3a_word_bits - off_w;
  out_w[0] = (in_w[0] << off_w) | low_w;
  for ( c3_w i_w = 1; i_w < len_w; i_w++ ) {
    out_w[i_w] = (in_w[i_w] << off_w) | (in_w[i_w - 1] >> back_w);
  }
  if ( extra_w ) {
    out_w[len_w] = in_w[len_w - 1] >> back_w;
  }
}

static u3_atom
_peg_wide(u3_atom a, u3_atom b)
{
  if ( 1 == a ) {
    return u3k(b);
  }

  //  The leading one in b marks the join. Copy the words below it, then
  //  write a above the remaining direction bits in a single pass.
  c3_w len_a, len_b;
  const c3_w* src_a = u3r_word_buffer(&a, &len_a);
  const c3_w* src_b = u3r_word_buffer(&b, &len_b);
  c3_w top_w = src_b[len_b - 1];
  c3_w off_w = c3_bits_word(top_w) - 1;
  c3_w ext_w = (off_w + c3_bits_word(src_a[len_a - 1])) > u3a_word_bits;
  u3i_slab sab_u;
  u3i_slab_bare(&sab_u, u3a_word_bits_log, (c3_d)len_a + len_b - 1 + ext_w);
  c3_w* dst_w = sab_u.buf_w;
  memcpy(dst_w, src_b, (len_b - 1) * sizeof(c3_w));
  dst_w += len_b - 1;

  //  Appending one head or tail uses a constant shift across all words.
  if ( 1 == off_w ) {
    _peg_shift(dst_w, src_a, len_a, 1, top_w & 1, ext_w);
  }
  else if ( !off_w ) {
    memcpy(dst_w, src_a, len_a * sizeof(c3_w));
  }
  else {
    _peg_shift(dst_w, src_a, len_a, off_w, top_w ^ ((c3_w)1 << off_w), ext_w);
  }
  return u3i_slab_moot(&sab_u);
}

u3_noun
u3qc_peg(u3_atom a, u3_atom b)
{
  if ( (0 == a) || (0 == b) ) {
    return u3m_bail(c3__exit);
  }
  else if ( 1 == b ) {
    return u3k(a);
  }

  if ( (c3y == u3a_is_cat(a)) && (c3y == u3a_is_cat(b)) ) {
    c3_w dep_w = c3_bits_word(b) - 1;
    //  Removing one prefix unit cancels the leading bit already in b.
    if ( (32 == u3a_word_bits) || (a <= (UINT64_MAX >> dep_w)) ) {
      return u3i_chub((((c3_d)a - 1) << dep_w) + b);
    }
  }

  return _peg_wide(a, b);
}

u3_noun
u3wc_peg(u3_noun cor)
{
  u3_noun a, b;

  a = u3h(u3h(u3t(cor)));
  b = u3t(u3h(u3t(cor)));

  if ( (c3n == u3ud(b)) ||
       (c3n == u3ud(a) && b != 1) )
  {
    return u3m_bail(c3__exit);
  }
  else {
    return u3qc_peg(a, b);
  }
}
