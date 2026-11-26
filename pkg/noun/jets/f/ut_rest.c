/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qfu_rest_key(u3_noun van, u3_noun sut, u3_noun leg)
{
  u3_weak vet = u3r_at(u3qfu_van_vet, van);
  c3_m fun_m = 141 + c3__rest + ((!!vet) << 8);
  return u3z_key_3(fun_m, sut, leg, u3h(van));
}

//  +repo in Kelvin 135 passes the leg itself. Kelvin 136 and 137 pass
//  a singleton list. A miss leaves expansion to the caller's Hoon arm.
//
u3_weak
u3qfu_rest_find(u3_noun van, u3_noun sut, c3_y kel)
{
  if ( u3r_at(u3qfu_van_vet, van) > 1 ) return u3_none;
  u3_noun item = u3t(sut);
  u3_noun leg = kel == 135 ? u3k(item) : u3nc(u3k(item), u3_nul);
  u3_noun key = u3qfu_rest_key(van, sut, leg);
  u3_weak typ = u3z_find(u3z_memo_toss, key);
  u3z(key); u3z(leg);
  return typ;
}

u3_noun
u3wfu_rest(u3_noun cor)
{
  u3_noun sut, leg, van;

  if ( (c3n == u3r_mean(cor, {u3x_sam, &leg}, {u3x_con, &van}))
     || (u3_none == (sut = u3r_at(u3x_sam, van))) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    u3_noun key = u3qfu_rest_key(van, sut, leg);
    u3_weak pro = u3z_find(u3z_memo_toss, key);

    if ( u3_none != pro ) {
      u3z(key);
      return pro;
    }
    else {
      pro = u3n_nock_on(u3k(cor), u3k(u3x_at(u3x_bat, cor)));
      return u3z_save(u3z_memo_toss, key, pro);
    }
  }
}
