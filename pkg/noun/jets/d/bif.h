/// @file
#pragma once

#include "tree_find.h"

// Split on one search path. Rebuilding it from the pivot outwards lets
// each partition retain the branches that already lie wholly on its side.
// A root match returns the borrowed branch pair. Otherwise [parts] receives
// two owned nouns and the caller decides whether to allocate a pair.
// https://www.cs.cmu.edu/~scandal/papers/treaps-spaa98.pdf
static u3_weak
_bif_parts(u3_noun tree, u3_noun key, c3_t map_t, u3_noun parts[2])
{
  typedef struct { u3_noun tree; c3_o left_o; } frame;
  u3a_pile pile;
  u3a_pile_prep(&pile, sizeof(frame), alignof(frame));

  c3_h mug = 0;
  while ( u3_nul != tree ) {
    u3_noun node, branches;
    u3x_cell(tree, &node, &branches);
    u3_noun pivot = map_t ? u3h(node) : node;

    if ( map_t && c3n == u3du(branches) ) return u3m_bail(c3__exit);
    if ( !mug ) {
      if ( key == pivot || c3y == u3r_sing(key, pivot) ) {
        u3R->cap_p = pile.beg_p;
        return u3t(tree);
      }
      mug = _find_mug(key);
    }
    else if ( key == pivot ) break;
    c3_h other = _find_mug(pivot);
    c3_o left_o = __(mug < other);
    if ( mug == other ) {
      if ( c3y == u3r_sing(key, pivot) ) break;
      left_o = u3qc_dor(key, pivot);
    }
    *(frame*)u3a_push(&pile) = (frame){tree, left_o};
    branches = u3t(tree);
    tree = (c3y == left_o) ? u3h(branches) : u3t(branches);
  }

  u3_noun left = u3_nul, right = u3_nul;
  if ( u3_nul != tree ) {
    left = u3k(u3h(u3t(tree)));
    right = u3k(u3t(u3t(tree)));
  }
  while ( c3n == u3a_pile_done(&pile) ) {
    frame f = *(frame*)u3a_peek(&pile);
    u3a_drop(&pile);
    u3_noun branches = u3t(f.tree);
    if ( c3y == f.left_o ) {
      if ( right == u3h(branches) ) {
        u3z(right);
        right = u3k(f.tree);
      }
      else {
        right = u3nt(u3k(u3h(f.tree)), right, u3k(u3t(branches)));
      }
    }
    else {
      if ( left == u3t(branches) ) {
        u3z(left);
        left = u3k(f.tree);
      }
      else {
        left = u3nt(u3k(u3h(f.tree)), u3k(u3h(branches)), left);
      }
    }
  }
  parts[0] = left;
  parts[1] = right;
  return u3_none;
}

static inline u3_noun
_bif_split(u3_noun tree, u3_noun key, c3_t map_t)
{
  u3_noun parts[2];
  u3_weak pair = _bif_parts(tree, key, map_t, parts);
  return (u3_none == pair) ? u3nc(parts[0], parts[1]) : u3k(pair);
}
