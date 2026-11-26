/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  /* _rap_bytes(): concatenate the measured list directly into its atom.
  */
  static u3_atom
  _rap_bytes(u3_noun cab, c3_w tot_w)
  {
    u3i_slab sab_u;
    u3i_slab_bare(&sab_u, 3, tot_w);
    sab_u.buf_w[sab_u.len_w - 1] = 0;

    c3_y* out_y = sab_u.buf_y;
    for ( ; u3_nul != cab; cab = u3t(cab) ) {
      u3_atom dat = u3h(cab);

      if ( dat <= 255 ) {
        if ( dat ) {
          *out_y++ = dat;
        }
      }
      else {
        c3_w len_w = u3r_met(3, dat);
        u3r_bytes(0, len_w, out_y, dat);
        out_y += len_w;
      }
    }

    return u3i_slab_moot_bytes(&sab_u);
  }

  u3_noun
  u3qc_rap(u3_atom a,
           u3_noun b)
  {
    if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_g       a_g = a;
      c3_w     tot_w = 0;
      u3i_slab sab_u;

      /* Measure and validate the slab required.
      */
      {
        u3_noun cab = b;

        while ( 1 ) {
          u3_noun h_cab;
          c3_w    len_w;

          if ( 0 == cab ) {
            break;
          }
          else if ( c3n == u3du(cab) ) {
            return u3m_bail(c3__exit);
          }
          else if ( c3n == u3ud(h_cab = u3h(cab)) ) {
            return u3m_bail(c3__exit);
          }
          len_w = (3 == a_g && h_cab <= 255) ? (0 != h_cab) : u3r_met(a_g, h_cab);
          if ( len_w > ((c3_w)-1 - tot_w) ) {
            return u3m_bail(c3__fail);
          }
          tot_w += len_w;
          cab = u3t(cab);
        }

        if ( 0 == tot_w ) {
          return 0;
        }

        if ( 3 == a_g ) {
          return _rap_bytes(b, tot_w);
        }
        u3i_slab_init(&sab_u, a_g, tot_w);
      }

      /* Chop the list atoms in.
      */
      {
        u3_noun cab = b;
        c3_w  pos_w = 0;

        while ( 0 != cab ) {
          u3_noun h_cab = u3h(cab);
          c3_w    len_w = u3r_met(a_g, h_cab);

          u3r_chop(a_g, 0, len_w, pos_w, sab_u.buf_w, h_cab);
          pos_w += len_w;
          cab = u3t(cab);
        }
      }

      return u3i_slab_mint(&sab_u);
    }
  }
  u3_noun
  u3wc_rap(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun pro;

      pro = u3qc_rap(a, b);
      return pro;
    }
  }

