/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "gas.h"

u3_noun
u3qdi_gas(u3_noun a,
          u3_noun b)
{
  return u3q_tree_gas(a, b, 0);
}

u3_noun
u3wdi_gas(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdi_gas(a, b);
}

u3_noun
u3kdi_gas(u3_noun a,
          u3_noun b)
{
  u3_noun c = u3qdi_gas(a, b);
  u3z(a); u3z(b);
  return c;
}
