/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "tree_find.h"

u3_noun
u3qdi_has(u3_noun a,
          u3_noun b)
{
  return __(_tree_find(a, b, 0) != u3_none);
}

u3_noun
u3wdi_has(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdi_has(a, b);
}

u3_noun
u3kdi_has(u3_noun a,
          u3_noun b)
{
  u3_noun c = u3qdi_has(a, b);
  u3z(a); u3z(b);
  return c;
}
