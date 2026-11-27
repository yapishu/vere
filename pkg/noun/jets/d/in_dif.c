/// @file

#include "jets/w.h"
#include "tree_dif.h"

u3_noun
u3qdi_dif(u3_noun a, u3_noun b)
{
  return _tree_dif(a, b, 0);
}

u3_noun
u3wdi_dif(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdi_dif(a, b);
}
