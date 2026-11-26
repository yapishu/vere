/// @file
#pragma once

#include "jets/q.h"
#include "noun.h"

static inline u3_noun
_put_node(u3_noun key, u3_noun value, c3_t map_t)
{
  return map_t ? u3nc(u3k(key), u3k(value)) : u3k(key);
}

static inline c3_o
_put_before(u3_noun a, c3_w x, u3_noun b, c3_w y)
{
  return x == y ? u3qc_dor(a, b) : __(x < y);
}

// Treap insertion rotates the winning root back up the search path.
// Carry the search mug down and the winning heap rank up. A hash tie
// uses noun order, as in gor and mor.
// https://faculty.washington.edu/aragon/pubs/rst89.pdf
static u3_noun
_put_walk(u3_noun tree, u3_noun key, u3_noun value, c3_t map_t,
          c3_w key_mug, c3_w* priority)
{
  u3_noun node, branches, left, right, pivot;
  u3x_cell(tree, &node, &branches);
  if ( map_t ) {
    u3_noun val;
    u3x_cell(branches, &left, &right);
    u3x_cell(node, &pivot, &val);
    if ( c3y == u3r_sing(pivot, key) ) {
      *priority = key_mug ? u3r_mug_word(key_mug) : 0;
      return (c3y == u3r_sing(val, value)) ? u3k(tree)
             : u3nt(_put_node(key, value, map_t), u3k(left), u3k(right));
    }
  }
  else {
    pivot = node;
    if ( c3y == u3r_sing(pivot, key) ) {
      *priority = key_mug ? u3r_mug_word(key_mug) : 0;
      return u3k(tree);
    }
    u3x_cell(branches, &left, &right);
  }

  if ( !key_mug ) key_mug = u3r_mug(key);
  c3_w mug = u3r_mug(pivot), rank = u3r_mug_word(mug);
  c3_o left_o = _put_before(key, key_mug, pivot, mug);
  u3_noun child = (c3y == left_o) ? left : right;
  u3_noun result;
  if ( u3_nul == child ) {
    u3_noun item = _put_node(key, value, map_t);
    c3_w key_rank = u3r_mug_word(key_mug);
    // At an empty branch, a rotation makes the entire old tree one child.
    if ( c3n == _put_before(pivot, rank, key, key_rank) ) {
      *priority = key_rank;
      return (c3y == left_o) ? u3nt(item, u3_nul, u3k(tree))
                             : u3nt(item, u3k(tree), u3_nul);
    }
    result = u3nt(item, u3_nul, u3_nul);
  }
  else {
    result = _put_walk(child, key, value, map_t, key_mug, priority);
    u3_noun item = u3h(result);
    if ( c3n == _put_before(pivot, rank, map_t ? u3h(item) : item, *priority) ) {
      u3_noun l, r, out;
      u3x_cell(u3t(result), &l, &r);
      out = (c3y == left_o)
          ? u3nt(u3k(item), u3k(l), u3nt(u3k(node), u3k(r), u3k(right)))
          : u3nt(u3k(item), u3nt(u3k(node), u3k(left), u3k(l)), u3k(r));
      u3z(result);
      return out;
    }
    if ( result == child ) {
      u3z(result);
      *priority = rank;
      return u3k(tree);
    }
  }
  *priority = rank;
  return (c3y == left_o) ? u3nt(u3k(node), result, u3k(right))
                         : u3nt(u3k(node), u3k(left), result);
}

static u3_noun
_tree_put(u3_noun tree, u3_noun key, u3_noun value, c3_t map_t)
{
  if ( u3_nul == tree ) {
    return u3nt(_put_node(key, value, map_t), u3_nul, u3_nul);
  }
  c3_w priority;
  return _put_walk(tree, key, value, map_t, 0, &priority);
}
