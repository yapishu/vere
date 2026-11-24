/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qb_weld(u3_noun a, u3_noun b)
{
  u3_noun  pro;
  u3_noun* lit = &pro;

  lit = u3i_list_prefix(lit, a, u3_nul);

  *lit = u3k(b);

  return pro;
}

u3_noun
u3wb_weld(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b});
  return u3qb_weld(a, b);
}

u3_noun
u3kb_weld(u3_noun a, u3_noun b)
{
  u3_noun c = u3qb_weld(a, b);
  u3z(a); u3z(b);
  return c;
}
