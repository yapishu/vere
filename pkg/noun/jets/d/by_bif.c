/// @file

#include "jets/w.h"

#include "bif.h"

u3_noun
u3wdb_bif(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdb_bif(a, b);
}

u3_noun
u3qdb_bif(u3_noun a,
          u3_noun b)
{
  return _bif_split(a, b, 1);
}
