/// @file

#include "jets/q.h"
#include "jets/w.h"
#include "order.h"

u3_noun
u3qc_dor(u3_noun a, u3_noun b)
{
  return _order_nouns(a, b, 0);
}

u3_noun
u3wc_dor(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b});
  return u3qc_dor(a, b);
}
