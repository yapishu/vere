/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "tree_merge.h"

static u3_noun
_by_int(u3_noun a, u3_noun b)
{
  if (  (u3_nul == a)
     || (u3_nul == b) )
  {
    return u3_nul;
  }
  else if ( a == b ) {
    u3q_tree_check(b, 1);
    return u3k(b);
  }
  else {
    u3_noun n_a, l_a, r_a;
    u3_noun n_b, l_b, r_b;
    u3_noun p_n_a, q_n_a;
    u3_noun p_n_b, q_n_b;
    u3x_trel(a, &n_a, &l_a, &r_a);
    u3x_trel(b, &n_b, &l_b, &r_b);
    u3x_cell(n_a, &p_n_a, &q_n_a);
    u3x_cell(n_b, &p_n_b, &q_n_b);

    if ( !l_a && !r_a ) return u3q_tree_int_leaf(a, b, 1, 0);
    if ( !l_b && !r_b ) return u3q_tree_int_leaf(b, a, 1, 1);
    if ( c3y == u3r_sing(p_n_a, p_n_b) ) {
      u3_noun left = _by_int(l_a, l_b);
      u3_noun right = _by_int(r_a, r_b);
      if ( left == l_b && right == r_b ) {
        u3z(left); u3z(right);
        return u3k(b);
      }
      return u3nt(u3k(n_b), left, right);
    }
    if ( c3y == u3qc_mor(p_n_a, p_n_b) ) {
      if ( c3y == u3qc_gor(p_n_b, p_n_a) ) {
        u3_noun new_l_b = u3nt(u3k(n_b), u3k(l_b), u3_nul);
        u3_noun   new_a = _by_int(l_a, new_l_b);

        u3z(new_l_b);
        return u3kdb_uni(new_a, _by_int(a, r_b));
      }
      else {
        u3_noun new_r_b = u3nt(u3k(n_b), u3_nul, u3k(r_b));
        u3_noun   new_a = _by_int(r_a, new_r_b);

        u3z(new_r_b);
        return u3kdb_uni(new_a, _by_int(a, l_b));
      }
    }
    else if ( c3y == u3qc_gor(p_n_a, p_n_b) ) {
      u3_noun new_l_a = u3nt(u3k(n_a), u3k(l_a), u3_nul);
      u3_noun   new_a = _by_int(new_l_a, l_b);

      u3z(new_l_a);
      return u3kdb_uni(new_a, _by_int(r_a, b));
    }
    else {
      u3_noun new_r_a = u3nt(u3k(n_a), u3_nul, u3k(r_a));
      u3_noun   new_a = _by_int(new_r_a, r_b);

      u3z(new_r_a);
      return u3kdb_uni(new_a, _by_int(l_a, b));
    }
  }
}

u3_noun
u3qdb_int(u3_noun a, u3_noun b)
{
  if ( a && b && a != b ) {
    u3_weak pro = u3q_tree_merge(a, b, u3_tree_map_intersection);
    if ( u3_none != pro ) return pro;
  }
  return _by_int(a, b);
}

u3_noun
u3wdb_int(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdb_int(a, b);
}
