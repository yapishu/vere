/// @file

#include "jets/w.h"
#include "tree_del.h"

u3_noun
u3qdi_del(u3_noun a, u3_noun b)
{
  return _tree_del(a, b, 0);
}

u3_noun
u3wdi_del(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdi_del(a, b);
}
