/// @file
#pragma once

#include "jets/q.h"
#include "noun.h"

// Split on one search path. Rebuilding it from the pivot outwards lets
// each partition retain the branches that already lie wholly on its side.
// https://www.cs.cmu.edu/~scandal/papers/treaps-spaa98.pdf
static u3_noun
_bif_split(u3_noun tree, u3_noun key, c3_t map_t)
{
  typedef struct { u3_noun tree; c3_o left_o; } frame;
  u3a_pile pile;
  u3a_pile_prep(&pile, sizeof(frame), alignof(frame));

  while ( u3_nul != tree ) {
    u3_noun node, branches;
    u3x_cell(tree, &node, &branches);
    u3_noun pivot = map_t ? u3h(node) : node;

    if ( map_t && c3n == u3du(branches) ) return u3m_bail(c3__exit);
    if ( c3y == u3r_sing(key, pivot) ) {
      if ( c3y == u3a_pile_done(&pile) ) return u3k(branches);
      break;
    }

    c3_o left_o = u3qc_gor(key, pivot);
    *(frame*)u3a_push(&pile) = (frame){tree, left_o};
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
  return u3nc(left, right);
}
