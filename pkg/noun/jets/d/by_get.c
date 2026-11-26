/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "tree_find.h"

u3_weak
u3qdb_git(u3_noun a, u3_noun b)
{
  u3_weak item = _tree_find(a, b, 1);
  return (u3_none == item) ? u3_none : u3t(item);
}

u3_noun
u3qdb_get(u3_noun a,
          u3_noun b)
{
  u3_weak value = u3qdb_git(a, b);
  return (u3_none == value) ? u3_nul : u3nc(u3_nul, u3k(value));
}

u3_noun
u3wdb_get(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdb_get(a, b);
}

u3_weak
u3kdb_get(u3_noun a,
          u3_noun b)
{
  u3_weak value = u3qdb_git(a, b);
  if ( u3_none != value ) value = u3k(value);
  u3z(a); u3z(b);
  return value;
}

u3_noun
u3kdb_got(u3_noun a,
          u3_noun b)
{
  return u3x_good(u3kdb_get(a, b));
}
