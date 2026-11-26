/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "tree_put.h"

u3_noun
u3qdb_put(u3_noun a,
          u3_noun b,
          u3_noun c)
{
  return _tree_put(a, b, c, 1);
}

u3_noun
u3wdb_put(u3_noun cor)
{
  u3_noun a, b, c;
  u3x_mean(cor, {u3x_sam_2,   &b},
                {u3x_sam_3,   &c},
                {u3x_con_sam, &a});
  return u3qdb_put(a, b, c);
}

u3_noun
u3kdb_put(u3_noun a,
          u3_noun b,
          u3_noun c)
{
  u3_noun pro = u3qdb_put(a, b, c);
  u3z(a); u3z(b); u3z(c);
  return pro;
}
