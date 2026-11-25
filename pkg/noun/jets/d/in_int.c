/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "tree_merge.h"

static u3_noun
_in_int(u3_noun a, u3_noun b)
{
  if (  (u3_nul == a)
     || (u3_nul == b) )
  {
    return u3_nul;
  }
  else if ( a == b ) {
    u3q_tree_check(b, 0);
    return u3k(b);
  }
  else {
    u3_noun n_a, l_a, r_a;
    u3_noun n_b, l_b, r_b;

    u3x_trel(a, &n_a, &l_a, &r_a);
    u3x_trel(b, &n_b, &l_b, &r_b);
    if ( !l_a && !r_a ) return u3q_tree_int_leaf(a, b, 0, 0);
    if ( !l_b && !r_b ) return u3q_tree_int_leaf(b, a, 0, 1);
    if ( c3y == u3r_sing(n_a, n_b) ) {
      u3_noun left = _in_int(l_a, l_b);
      u3_noun right = _in_int(r_a, r_b);
      if ( left == l_b && right == r_b ) {
        u3z(left); u3z(right);
        return u3k(b);
      }
      return u3nt(u3k(n_b), left, right);
    }
    if ( c3y == u3qc_mor(n_a, n_b) ) {
      u3_noun tmp;
      tmp = a;   a = b;     b = tmp;
      tmp = n_a; n_a = n_b; n_b = tmp;
      tmp = l_a; l_a = l_b; l_b = tmp;
      tmp = r_a; r_a = r_b; r_b = tmp;
    }
    if ( c3y == u3qc_gor(n_b, n_a) ) {
      u3_noun new_l_b = u3nt(u3k(n_b), u3k(l_b), u3_nul);
      u3_noun   new_a = _in_int(l_a, new_l_b);

      u3z(new_l_b);
      return u3kdi_uni(new_a, _in_int(a, r_b));
    }
    else {
      u3_noun new_r_b = u3nt(u3k(n_b), u3_nul, u3k(r_b));
      u3_noun   new_a = _in_int(r_a, new_r_b);

      u3z(new_r_b);
      return u3kdi_uni(new_a, _in_int(a, l_b));
    }
  }
}

u3_noun
u3qdi_int(u3_noun a, u3_noun b)
{
  if ( a && b && a != b ) {
    u3_weak pro = u3q_tree_merge(a, b, u3_tree_set_intersection);
    if ( u3_none != pro ) return pro;
  }
  return _in_int(a, b);
}

u3_noun
u3wdi_int(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdi_int(a, b);
}
