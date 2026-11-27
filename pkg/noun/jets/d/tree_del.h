/// @file
#pragma once

#include "tree_find.h"

// Find the key, then join its children in heap order. Both walks leave
// one child to replace in each ancestor. A miss keeps the original root.
// https://www.cs.cmu.edu/~scandal/papers/treaps-spaa98.html
static u3_noun
_tree_del(u3_noun root, u3_noun key, c3_t map_t)
{
  if ( !root ) return u3_nul;

  typedef struct { u3_noun tree; c3_o left_o; } frame;
  u3a_pile pile;
  u3a_pile_prep(&pile, sizeof(frame), alignof(frame));
  u3_noun at = root;
  c3_h mug = 0;
  while ( at ) {
    u3_noun item, left, right;
    u3x_trel(at, &item, &left, &right);
    u3_noun pivot = map_t ? u3h(item) : item;
    if ( key == pivot ) break;
    if ( !mug ) {
      if ( c3y == u3r_sing(pivot, key) ) break;
      if ( !left && !right ) {
        at = u3_nul;
        break;
      }
      mug = _find_mug(key);
    }
    c3_h other = _find_mug(pivot);
    c3_o before = __(mug < other);
    if ( mug == other ) {
      if ( c3y == u3r_sing(pivot, key) ) break;
      before = u3qc_dor(key, pivot);
    }
    *(frame*)u3a_push(&pile) = (frame){at, before};
    u3_noun branches = u3t(at);
    at = (c3y == before) ? u3h(branches) : u3t(branches);
  }
  if ( !at ) {
    u3R->cap_p = pile.beg_p;
    return u3k(root);
  }

  u3_noun left, right;
  u3x_cell(u3t(at), &left, &right);
  while ( left && right ) {
    u3_noun x, y;
    u3x_trel(left, &x, NULL, NULL);
    u3x_trel(right, &y, NULL, NULL);
    if ( c3y == u3qc_mor(map_t ? u3h(x) : x, map_t ? u3h(y) : y) ) {
      *(frame*)u3a_push(&pile) = (frame){left, c3n};
      left = u3t(u3t(left));
    }
    else {
      *(frame*)u3a_push(&pile) = (frame){right, c3y};
      right = u3h(u3t(right));
    }
  }

  u3_noun out = u3k(left ? left : right);
  while ( c3n == u3a_pile_done(&pile) ) {
    frame f = *(frame*)u3a_peek(&pile);
    u3a_drop(&pile);
    // Key comparisons can unify inner nouns. Read the surviving sibling here.
    u3_noun branches = u3t(f.tree), item = u3k(u3h(f.tree));
    out = (c3y == f.left_o) ? u3nt(item, out, u3k(u3t(branches)))
                            : u3nt(item, u3k(u3h(branches)), out);
  }
  return out;
}
