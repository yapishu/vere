/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#ifdef U3_LIST_SHARING
#include "list_walk.h"
#endif

u3_noun
u3qb_skip(u3_noun a, u3_noun b)
{
  u3_noun  pro;
  u3_noun* lit = &pro;

  if ( u3_nul != a) {
    u3_noun   i, t;
    u3_noun*   hed;
    u3_noun*   tel;
    u3j_site sit_u;
    u3j_gate_prep(&sit_u, u3k(b));
#ifdef U3_LIST_SHARING
    if ( c3y == u3j_gate_read_only(&sit_u) ) {
      pro = _walk_list(a, &sit_u, _walk_skip);
      u3j_gate_lose(&sit_u);
      return pro;
    }
#endif
    u3k(a);

    do {
      i = u3k(u3k(u3h(a)));

      if ( c3y == u3x_loob(u3j_gate_slam(&sit_u, i)) ) {
        u3z(i);
      }
      else {
        *lit = u3i_defcons(&hed, &tel);
        *hed = i;
        lit  = tel;
      }
      
      t = u3k(u3t(a));
      u3z(a), a = t;
    }
    while ( u3_nul != a );

    u3j_gate_lose(&sit_u);
  }

  *lit = u3_nul;

  return pro;
}

u3_noun
u3wb_skip(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b});
  return u3qb_skip(a, b);
}
