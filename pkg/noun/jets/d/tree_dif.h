/// @file
#pragma once

#include "bif.h"
#include "tree_merge.h"

// Split around the right root, subtract each child, then join the survivors.
// https://arxiv.org/abs/1602.02120
static u3_noun
_dif_join(u3_noun a, u3_noun b, c3_t map_t)
{
  if ( !a || !b ) return u3k(a ? a : b);

  u3_noun x = u3h(a), y = u3h(b);
  if ( c3y == u3qc_mor(map_t ? u3h(x) : x, map_t ? u3h(y) : y) ) {
    u3_noun right = _dif_join(u3t(u3t(a)), b, map_t);
    return u3nt(u3k(u3h(a)), u3k(u3h(u3t(a))), right);
  }
  u3_noun left = _dif_join(a, u3h(u3t(b)), map_t);
  return u3nt(u3k(u3h(b)), left, u3k(u3t(u3t(b))));
}

static u3_noun
_tree_dif(u3_noun a, u3_noun b, c3_t map_t)
{
  if ( !b ) return u3k(a);
  if ( !a || a == b ) {
    // The gate visits every node of the right tree even with an empty result.
    u3q_tree_check(b, map_t);
    return u3_nul;
  }

  u3_noun item;
  u3x_trel(b, &item, 0, 0);
  u3_noun key = map_t ? u3h(item) : item;
  u3_noun parts[2];
  u3_weak pair = _bif_parts(a, key, map_t, parts);
  if ( u3_none != pair ) u3x_cell(pair, &parts[0], &parts[1]);
  u3_noun left = _tree_dif(parts[0], u3h(u3t(b)), map_t);
  // Equality in the left subtree can unify children. Read borrowed fields again.
  u3_noun ra = (u3_none == pair) ? parts[1] : u3t(u3t(a));
  u3_noun right = _tree_dif(ra, u3t(u3t(b)), map_t);
  if ( u3_none == pair ) {
    u3z(parts[0]); u3z(parts[1]);
  }
  if ( !left || !right ) return left ? left : right;
  u3_noun out = _dif_join(left, right, map_t);
  u3z(left); u3z(right);
  return out;
}
