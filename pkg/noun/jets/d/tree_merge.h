/// @file
#pragma once

#include "noun.h"

// Validate the node shapes visited by self-union or self-intersection.
void u3q_tree_check(u3_noun tree, c3_t map_t);

typedef enum {
  u3_tree_map_union,
  u3_tree_map_intersection,
  u3_tree_set_intersection
} u3_tree_op;

// Try an ordered merge of two nonempty trees. Equal keys select the right item.
u3_weak u3q_tree_merge(u3_noun a, u3_noun b, u3_tree_op op);

// Intersect a singleton with a tree, retaining the right map's value.
u3_noun u3q_tree_int_leaf(u3_noun leaf, u3_noun tree, c3_t map_t, c3_t right_t);
