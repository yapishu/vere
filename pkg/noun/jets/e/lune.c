/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qe_lune(u3_atom lub)
  {
    c3_w len_w = u3r_met(3, lub);

    if ( len_w && (10 != u3r_byte(len_w - 1, lub)) ) {
      return u3m_error("noeol");
    }
    return u3qe_leer(lub);
  }

  u3_noun
  u3we_lune(u3_noun cor)
  {
    u3_noun lub;

    if ( (u3_none == (lub = u3r_at(u3x_sam, cor))) ||
         (c3n == u3ud(lub)) )
    {
      return u3m_bail(c3__fail);
    } else {
      return u3qe_lune(lub);
    }
  }
