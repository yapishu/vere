/// @file
#pragma once

#include "jets/q.h"
#include "noun.h"

// A pivot often already has the mug that placed it in the tree.
// A forwarding pointer in the mug slot is resolved by the noun reader.
static inline c3_h
_find_mug(u3_noun key)
{
  if ( c3y == u3a_is_cat(key) ) return u3r_mug_word(key);
  c3_w mug = ((u3a_noun*)u3a_to_ptr(key))->mug_w;
  return (mug && !(mug >> 31)) ? (c3_h)mug : u3r_mug(key);
}

// Return the borrowed item. Different mugs select a branch immediately.
// Equal mugs still require noun equality and dor order, as in gor.
static u3_weak
_tree_find(u3_noun tree, u3_noun key, c3_t map_t)
{
  c3_h mug = 0;
  while ( u3_nul != tree ) {
    u3_noun item, branches;
    u3x_cell(tree, &item, &branches);
    u3_noun pivot = map_t ? u3h(item) : item;
    if ( key == pivot ) return item;
    if ( !mug ) {
      // A root match or identical branches need no search hash.
      if ( c3y == u3r_sing(key, pivot) ) return item;
      u3_noun left, right;
      u3x_cell(branches, &left, &right);
      if ( left == right ) {
        tree = left;
        continue;
      }
      mug = _find_mug(key);
    }
    c3_h other = _find_mug(pivot);
    c3_o left_o = __(mug < other);
    if ( mug == other ) {
      if ( c3y == u3r_sing(key, pivot) ) return item;
      left_o = u3qc_dor(key, pivot);
    }
    tree = (c3y == left_o) ? u3h(branches) : u3t(branches);
  }
  return u3_none;
}
