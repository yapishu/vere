/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "tree_find.h"

u3_noun
u3qdb_has(u3_noun a,
          u3_noun b)
{
  return __(_tree_find(a, b, 1) != u3_none);
}

u3_noun
u3wdb_has(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdb_has(a, b);
}

u3_noun
u3kdb_has(u3_noun a,
          u3_noun b)
{
  u3_noun c = u3qdb_has(a, b);
  u3z(a); u3z(b);
  return c;
}
