/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"


  static u3_atom
  _cqe_shay(u3_atom wid,
            u3_atom dat)
  {
    c3_w len_w;
    if ( !u3r_word_fit(&len_w, wid) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y  out_y[32];
      c3_y* dat_y = u3r_bytes_alloc(0, len_w, dat);
      urcrypt_shay(dat_y, len_w, out_y);
      u3a_free(dat_y);
      return u3i_bytes(32, out_y);
    }
  }

  static u3_atom
  _cqe_shax(u3_atom a)
  {
    c3_w wor_w;
    c3_y out_y[32];
    const c3_y* dat_y = (const c3_y*)u3r_word_buffer(&a, &wor_w);
    urcrypt_shay(dat_y, u3r_met(3, a), out_y);
    return u3i_bytes(32, out_y);
  }

  static u3_atom
  _cqe_shal(u3_atom wid,
            u3_atom dat)
  {
    c3_w len_w;
    if ( !u3r_word_fit(&len_w, wid) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y  out_y[64];
      c3_y* dat_y = u3r_bytes_alloc(0, len_w, dat);
      urcrypt_shal(dat_y, len_w, out_y);
      u3a_free(dat_y);
      return u3i_bytes(64, out_y);
    }
  }

  static u3_atom
  _cqe_shas(u3_atom sal,
            u3_atom ruz)
  {
    c3_w sal_w, wor_w;
    c3_y out_y[32];
    //  urcrypt_shas writes into salts wider than one digest.
    //
    c3_y* sal_y = u3r_bytes_all(&sal_w, sal);
    const c3_y* ruz_y = (const c3_y*)u3r_word_buffer(&ruz, &wor_w);
    urcrypt_shas(sal_y, sal_w, ruz_y, u3r_met(3, ruz), out_y);
    u3a_free(sal_y);
    return u3i_bytes(32, out_y);
  }

  u3_noun
  u3we_shax(u3_noun cor)
  {
    u3_noun a;

    if ( (u3_none == (a = u3r_at(u3x_sam, cor))) ||
         (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqe_shax(a);
    }
  }

  u3_noun
  u3we_shay(u3_noun cor)
  {
    u3_noun a, b;

    if ( (u3_none == (a = u3r_at(u3x_sam_2, cor))) ||
         (u3_none == (b = u3r_at(u3x_sam_3, cor))) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqe_shay(a, b);
    }
  }

  u3_noun
  u3we_shal(u3_noun cor)
  {
    u3_noun a, b;

    if ( (u3_none == (a = u3r_at(u3x_sam_2, cor))) ||
         (u3_none == (b = u3r_at(u3x_sam_3, cor))) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqe_shal(a, b);
    }
  }

  u3_noun
  u3we_shas(u3_noun cor)
  {
    u3_noun sal, ruz;

    if ( (u3_none == (sal = u3r_at(u3x_sam_2, cor))) ||
         (u3_none == (ruz = u3r_at(u3x_sam_3, cor))) ||
         (c3n == u3ud(sal)) ||
         (c3n == u3ud(ruz)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqe_shas(sal, ruz);
    }
  }

  //  shas hashes the minimal byte representation of the mixed atom. Only
  //  its low 256 bits can change between blocks, even for a wider seed.
  //
  static void
  _og_block(c3_y tag_y, c3_w rem_w, c3_w len_w, const c3_y key_y[32],
            c3_y* msg_y, c3_y dig_y[32])
  {
    c3_y sal_y[4] = {'o', 'g', '-', tag_y};
    for ( c3_w i_w = 0; i_w < 32; i_w++ ) {
      msg_y[i_w] = key_y[i_w] ^ dig_y[i_w];
    }
    for ( c3_w i_w = 0; i_w < sizeof(rem_w); i_w++ ) {
      msg_y[i_w] ^= (c3_y)(rem_w >> (8 * i_w));
    }
    if ( len_w <= 32 ) {
      len_w = 32;
      while ( len_w && !msg_y[len_w - 1] ) len_w--;
    }
    urcrypt_shas(sal_y, sizeof(sal_y), msg_y, len_w, dig_y);
  }

  u3_noun
  u3qeo_raw(u3_atom a, u3_atom b)
  {
    if ( !_(u3a_is_cat(b)) ) return u3m_bail(c3__fail);
    if ( !b ) return 0;

    u3i_slab sab_u;
    u3i_slab_bare(&sab_u, 0, b);
    //  The last digest may fill only part of the last word.
    //
    sab_u.buf_w[sab_u.len_w - 1] = 0;

    c3_w len_w = u3r_met(3, a);
    c3_y key_y[32], buf_y[32], dig_y[32] = {0};
    c3_y* msg_y = len_w > 32 ? u3r_bytes_alloc(0, len_w, a) : buf_y;
    u3r_bytes(0, sizeof(key_y), key_y, a);
    _og_block('a', b, len_w, key_y, msg_y, dig_y);

    c3_y* out_y = sab_u.buf_y;
    c3_w rem_w = b;
    while ( rem_w ) {
      c3_w bit_w = c3_min(rem_w, 256);
      c3_w byt_w = (bit_w + 7) >> 3;
      _og_block('b', rem_w, len_w, key_y, msg_y, dig_y);
      memcpy(out_y, dig_y, byt_w);
      out_y += byt_w;
      rem_w -= bit_w;
    }
    if ( b & 7 ) out_y[-1] &= (1u << (b & 7)) - 1;
    if ( msg_y != buf_y ) u3a_free(msg_y);
    return u3i_slab_mint(&sab_u);
  }

  u3_noun
  u3weo_raw(u3_noun cor)
  {
    u3_noun a, b;
    u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
    return u3qeo_raw(u3x_atom(a), u3x_atom(b));
  }
