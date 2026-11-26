/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "tree_put.h"

u3_noun
u3qdi_put(u3_noun a,
          u3_noun b)
{
  return _tree_put(a, b, u3_nul, 0);
}

u3_noun
u3wdi_put(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdi_put(a, b);
}

u3_noun
u3kdi_put(u3_noun a,
          u3_noun b)
{
  u3_noun pro = u3qdi_put(a, b);
  u3z(a); u3z(b);
  return pro;
}
