/// @file

#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"

/* An axis is a binary path after its leading 1. A branch stores a prefix
** of an input axis and its two children. A leaf holds a complete edit.
** Inserting one edit creates at most one branch and one leaf.
** Morrison, PATRICIA: https://doi.org/10.1145/321479.321481
*/
typedef struct {
  u3_atom path;
  c3_w    bits, depth;
  c3_w    parent, child[2];
  u3_weak formula;
} _hike_node;

typedef struct {
  u3_atom axis;
  u3_noun formula;
} _hike_edit;

typedef struct {
  _hike_node* nodes;
  c3_w        used;
  u3_weak     root;
  u3_noun*    combined;
  c3_w        count;
} _hike_tree;

static c3_w
_hike_common(u3_atom axis, c3_w bits, const _hike_node* node, c3_w known)
{
  c3_w limit = c3_min(bits, node->depth);

  if ( c3y == u3a_is_cat(axis) && c3y == u3a_is_cat(node->path) ) {
    c3_w diff = (axis >> (bits - limit)) ^ (node->path >> (node->bits - limit));
    return limit - c3_bits_word(diff);
  }
  while ( known < limit &&
          u3r_bit(bits - known - 1, axis) ==
          u3r_bit(node->bits - known - 1, node->path) ) {
    known++;
  }
  return known;
}

/* +insert combines edits at immediate siblings with +cons. Axis 1 is a
** separate output edit. Writing it clears the tree, and later writes at
** axes 2 and above can coexist with it.
*/
static void
_hike_combine(_hike_tree* tree, c3_w idx)
{
  while ( idx ) {
    _hike_node* node = &tree->nodes[idx];
    c3_w left = node->child[0], right = node->child[1];
    if ( !left || !right ) return;

    _hike_node* l = &tree->nodes[left];
    _hike_node* r = &tree->nodes[right];
    if ( u3_none == l->formula || u3_none == r->formula ||
         node->depth + 1 != l->depth || node->depth + 1 != r->depth ) {
      return;
    }

    u3_noun formula = u3qf_cons(l->formula, r->formula);
    tree->combined[tree->count++] = formula;
    node->child[0] = node->child[1] = 0;
    if ( 1 == idx ) tree->root = formula;
    else            node->formula = formula;
    idx = node->parent;
  }
}

static void
_hike_insert(_hike_tree* tree, u3_atom axis, u3_noun formula)
{
  _hike_node* nodes = tree->nodes;
  if ( 1 == axis ) {
    nodes[1].child[0] = nodes[1].child[1] = 0;
    tree->root = formula;
    return;
  }

  c3_w bits = u3r_met(0, axis);
  for ( c3_w idx = 1; ; ) {
    c3_w side = u3r_bit(bits - nodes[idx].depth - 1, axis);
    c3_w sub = nodes[idx].child[side];
    _hike_node leaf = { .path = axis, .bits = bits, .depth = bits,
                        .parent = idx, .formula = formula };

    if ( !sub ) {
      nodes[idx].child[side] = tree->used;
      nodes[tree->used++] = leaf;
      _hike_combine(tree, idx);
      return;
    }

    c3_w common = _hike_common(axis, bits, &nodes[sub], nodes[idx].depth + 1);
    if ( common == nodes[sub].depth ) {
      if ( u3_none != nodes[sub].formula ) return;
      if ( bits != common ) {
        idx = sub;
        continue;
      }
    }
    if ( common == bits ) {
      nodes[sub] = leaf;
      _hike_combine(tree, idx);
      return;
    }

    c3_w branch = tree->used++, tip = tree->used++;
    c3_w dir = u3r_bit(bits - common - 1, axis);
    nodes[branch] = (_hike_node){ .path = axis, .bits = bits, .depth = common,
                                  .parent = idx, .formula = u3_none };
    nodes[idx].child[side] = branch;
    nodes[branch].child[dir] = tip;
    nodes[branch].child[dir ^ 1] = sub;
    nodes[sub].parent = leaf.parent = branch;
    nodes[tip] = leaf;
    _hike_combine(tree, branch);
    return;
  }
}

static u3_atom
_hike_axis(const _hike_node* node)
{
  c3_w shift = node->bits - node->depth;
  if ( !shift ) return u3k(node->path);
  if ( c3y == u3a_is_cat(node->path) ) return node->path >> shift;

  return u3qc_rsh(0, shift, node->path);
}

static int
_hike_order(const void* a, const void* b)
{
  return u3r_comp(((const _hike_edit*)a)->axis, ((const _hike_edit*)b)->axis);
}

static u3_noun
_hike_formula(const _hike_tree* tree, _hike_edit* edits, u3_atom base)
{
  c3_w len = 0;
  if ( u3_none != tree->root ) {
    edits[len++] = (_hike_edit){1, tree->root};
  }

  // Walk the live branches. Subtrees covered by a later edit are unreachable.
  for ( c3_w idx = 1, prev = 0; idx; ) {
    const _hike_node* node = &tree->nodes[idx];
    c3_w next = node->parent;

    if ( prev == node->parent && u3_none != node->formula ) {
      edits[len++] = (_hike_edit){_hike_axis(node), node->formula};
    }
    else if ( prev == node->parent && node->child[0] ) next = node->child[0];
    else if ( node->child[1] && prev != node->child[1] ) next = node->child[1];

    prev = idx;
    idx = next;
  }

  qsort(edits, len, sizeof(*edits), _hike_order);
  u3_noun pro = u3nc(0, u3k(base));
  for ( c3_w i = 0; i < len; i++ ) {
    pro = u3nt(10, u3nc(edits[i].axis, u3k(edits[i].formula)), pro);
  }
  return pro;
}

static u3_noun
_hike_many(u3_atom base, u3_noun list, size_t len)
{
  _hike_node small_nodes[18];
  _hike_edit small_edits[8];
  u3_noun    small_combined[8];
  c3_t small = len <= c3_array_len(small_edits);

  _hike_node* nodes = small ? small_nodes : u3a_malloc((2 * len + 2) * sizeof(*nodes));
  _hike_edit* edits = small ? small_edits : u3a_malloc(len * sizeof(*edits));
  u3_noun* combined = small ? small_combined : u3a_malloc(len * sizeof(*combined));
  _hike_tree tree = { .nodes = nodes, .used = 2, .root = u3_none, .combined = combined };

  nodes[1] = (_hike_node){ .path = 1, .bits = 1, .depth = 1, .formula = u3_none };
  for ( ; u3_nul != list; list = u3t(list) ) {
    u3_noun edit = u3h(list);
    _hike_insert(&tree, u3h(edit), u3t(edit));
  }

  u3_noun pro = _hike_formula(&tree, edits, base);
  for ( c3_w i = 0; i < tree.count; i++ ) u3z(combined[i]);
  if ( !small ) {
    u3a_free(nodes);
    u3a_free(edits);
    u3a_free(combined);
  }
  return pro;
}

u3_noun
u3qf_hike(u3_noun base, u3_noun list)
{
  if ( c3n == u3ud(base) || !base ) return u3_none;
  if ( u3_nul == list ) return u3nc(0, u3k(base));

  size_t len = 0;
  for ( u3_noun cur = list; u3_nul != cur; cur = u3t(cur) ) {
    if ( c3n == u3du(cur) || c3n == u3du(u3h(cur)) ) return u3_none;
    u3_noun axis = u3h(u3h(cur));
    if ( c3n == u3ud(axis) || !axis ) return u3_none;
    // +contains passes the difference of two positive axis lengths to +rsh.
    // Its native shift count must fit in a direct atom.
    if ( c3n == u3a_is_cat(axis) &&
         u3r_met(u3a_word_bits_log, axis) > 1 + (u3a_direct_max >> u3a_word_bits_log) ) {
      return u3_none;
    }
    len++;
  }
  if ( 1 == len ) return u3nt(10, u3k(u3h(list)), u3nc(0, u3k(base)));
  if ( len > (c3_min(c3_w_max, SIZE_MAX / sizeof(_hike_node)) - 2) / 2 ) {
    return u3_none;
  }
  return _hike_many(base, list, len);
}

u3_noun
u3wf_hike(u3_noun cor)
{
  u3_noun base, list;
  if ( c3n == u3r_mean(cor, {u3x_sam_2, &base}, {u3x_sam_3, &list}) ) {
    return u3_none;
  }
  return u3qf_hike(base, list);
}
