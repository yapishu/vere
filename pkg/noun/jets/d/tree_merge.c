/// @file

#include "jets/q.h"
#include "tree_merge.h"

void
u3q_tree_check(u3_noun tree, c3_t map_t)
{
  while ( u3_nul != tree ) {
    u3_noun item, left, right;
    u3x_trel(tree, &item, &left, &right);
    if ( map_t && c3n == u3du(item) ) u3m_bail(c3__exit);
    if ( left != right ) u3q_tree_check(left, map_t);
    tree = right;
  }
}

// With only one candidate key, every opposite branch intersects the empty
// tree. The recursive intersection therefore reduces to one search.
u3_noun
u3q_tree_int_leaf(u3_noun leaf, u3_noun tree, c3_t map_t, c3_t right_t)
{
  u3_noun item = u3h(leaf);
  u3_noun key = map_t ? u3h(item) : item;
  while ( u3_nul != tree ) {
    u3_noun node, left, right;
    u3x_trel(tree, &node, &left, &right);
    u3_noun pivot = map_t ? u3h(node) : node;
    if ( c3y == u3r_sing(key, pivot) ) {
      if ( !map_t || right_t || item == node ) return u3k(leaf);
      return (!left && !right) ? u3k(tree) : u3nt(u3k(node), u3_nul, u3_nul);
    }
    tree = (c3y == u3qc_gor(key, pivot)) ? left : right;
  }
  return u3_nul;
}

// Canonical sets and maps are ordered by gor and heap-ordered by mor. A merge in
// gor order leaves only the right spine of the output open (Cartesian trees,
// https://doi.org/10.1016/0020-0190(94)00150-2).
typedef struct {
  u3_noun tree, key;
  c3_h    mug, priority;
} _merge_node;

static c3_t
_merge_read(u3_noun tree, _merge_node* out, c3_t map_t)
{
  u3_noun item, children;
  if ( c3n == u3r_cell(tree, &item, &children) ||
       (map_t && c3n == u3du(item)) || c3n == u3du(children) ) return 0;
  u3_noun key = map_t ? u3h(item) : item;
  c3_h mug = u3r_mug(key);
  *out = (_merge_node){tree, key, mug, u3r_mug_word(mug)};
  return 1;
}

// Cursors borrow nodes of both inputs. A key can contain one of those
// nodes, so compare without unifying its subtrees. A long tie returns control
// to the recursive operation.
enum { _merge_unknown = 2, _merge_slots = 64 };

static c3_i
_merge_order(u3_noun a, u3_noun b, unsigned* remaining)
{
  while ( a != b ) {
    if ( c3y == u3ud(a) ) {
      if ( c3n == u3ud(b) ) return -1;
      if ( c3y == u3r_sing(a, b) ) return 0;
      return c3y == u3qa_lth(a, b) ? -1 : 1;
    }
    if ( c3y == u3ud(b) ) return 1;
    if ( !*remaining ) return _merge_unknown;
    --*remaining;
    c3_i cmp = _merge_order(u3h(a), u3h(b), remaining);
    if ( cmp ) return cmp;
    a = u3t(a);
    b = u3t(b);
  }
  return 0;
}

static inline c3_i
_merge_cmp(_merge_node a, c3_h x, _merge_node b, c3_h y)
{
  if ( x != y ) return x < y ? -1 : 1;
  unsigned remaining = 64;
  return _merge_order(a.key, b.key, &remaining);
}

// The parent is one of the two bounds on key order. Carry both ranks
// down the tree so each node needs one mug and one heap priority.
static u3_weak
_tree_apt(u3_noun tree, _merge_node lo, _merge_node hi, c3_t from_left,
          c3_t map_t, c3_t strict_t, unsigned depth)
{
  while ( tree ) {
    _merge_node node, parent = from_left ? hi : lo;
    if ( !_merge_read(tree, &node, map_t) ) return u3_none;
    if ( parent.tree ) {
      c3_i cmp = _merge_cmp(parent, parent.priority, node, node.priority);
      if ( cmp == _merge_unknown ) return u3_none;
      if ( cmp > 0 || (strict_t && !cmp) ) return c3n;
    }
    if ( hi.tree ) {
      c3_i cmp = _merge_cmp(node, node.mug, hi, hi.mug);
      if ( cmp == _merge_unknown ) return u3_none;
      if ( cmp > 0 || (strict_t && !cmp) ) return c3n;
    }
    if ( lo.tree ) {
      c3_i cmp = _merge_cmp(lo, lo.mug, node, node.mug);
      if ( cmp == _merge_unknown ) return u3_none;
      if ( cmp > 0 || (strict_t && !cmp) ) return c3n;
    }
    u3_noun branches = u3t(tree);
    if ( u3h(branches) ) {
      if ( depth == _merge_slots ) return u3_none;
      u3_weak ok = _tree_apt(u3h(branches), lo, node, 1, map_t, strict_t, depth + 1);
      if ( ok != c3y ) return ok;
    }
    tree = u3t(branches);
    from_left = 0;
    lo = node;
  }
  return c3y;
}

u3_weak
u3q_tree_apt(u3_noun tree, c3_t map_t, c3_t strict_t)
{
  // A leaf has no ordering constraints, even when its key is expensive to hash.
  u3_noun item, left, right;
  if ( !tree ) return c3y;
  if ( c3n == u3r_trel(tree, &item, &left, &right) ||
       (map_t && c3n == u3du(item)) ) return u3_none;
  if ( !left && !right ) return c3y;
  _merge_node empty = {0};
  return _tree_apt(tree, empty, empty, 0, map_t, strict_t, 0);
}

// The kth smallest priority estimates inverse size without walking either
// tree. The estimate selects an ordered scan for comparable inputs.
// https://www.cs.tau.ac.il/~haimk/papers/p225-cohen.pdf
enum { _merge_sample = 12 };

static c3_h
_merge_rank(_merge_node root, c3_t map_t)
{
  _merge_node queue[_merge_sample + 1] = {root};
  unsigned len = 1;
  for ( unsigned rank = 1; len; ++rank ) {
    unsigned low = 0;
    for ( unsigned i = 1; i < len; ++i ) {
      if ( queue[i].priority < queue[low].priority ) low = i;
    }
    if ( rank == _merge_sample ) return queue[low].priority;
    u3_noun children = u3t(queue[low].tree);
    queue[low] = queue[--len];
    u3_noun sides[2] = {u3h(children), u3t(children)};
    for ( unsigned i = 0; i < 2; ++i ) {
      if ( sides[i] ) {
        if ( !_merge_read(sides[i], &queue[len], map_t) ) return 0;
        ++len;
      }
    }
  }
  return 0;
}

typedef struct {
  _merge_node path[_merge_slots];
  unsigned    len;
} _merge_iter;

// Check heap order on descent and strict key order between successive
// items. Unsupported shapes resume the recursive operation.
static c3_t
_merge_descend(_merge_iter* it, u3_noun tree, _merge_node parent, c3_t map_t)
{
  while ( tree ) {
    _merge_node node;
    if ( it->len == _merge_slots || !_merge_read(tree, &node, map_t) ||
         (parent.tree &&
          _merge_cmp(parent, parent.priority, node, node.priority) >= 0) )
    {
      return 0;
    }
    it->path[it->len++] = parent = node;
    tree = u3h(u3t(tree));
  }
  return 1;
}

static c3_t
_merge_advance(_merge_iter* it, c3_t map_t)
{
  _merge_node old = it->path[--it->len];
  if ( !_merge_descend(it, u3t(u3t(old.tree)), old, map_t) ) return 0;
  if ( !it->len ) return 1;
  _merge_node next = it->path[it->len - 1];
  return _merge_cmp(old, old.mug, next, next.mug) < 0;
}

typedef struct {
  _merge_node node;
  u3_noun     left;
} _merge_frame;

static u3_noun
_merge_close(u3a_pile* pile, u3_noun right)
{
  _merge_frame top = *(_merge_frame*)u3a_peek(pile);
  u3a_drop(pile);
  u3_noun tree = top.node.tree, children = u3t(tree);
  if ( top.left == u3h(children) && right == u3t(children) ) {
    u3z(top.left); u3z(right);
    return u3k(tree);
  }
  return u3nt(u3k(u3h(tree)), top.left, right);
}

static u3_weak
_merge_walk(u3_noun a, u3_noun b, c3_t map_t, c3_t intersect_t)
{
  _merge_iter x, y;
  x.len = y.len = 0;
  if ( !_merge_descend(&x, a, (_merge_node){0}, map_t) ||
       !_merge_descend(&y, b, (_merge_node){0}, map_t) ) return u3_none;

  u3a_pile pile;
  u3a_pile_prep(&pile, sizeof(_merge_frame), __alignof__(_merge_frame));
  while ( x.len || y.len ) {
    _merge_node l = x.len ? x.path[x.len - 1] : (_merge_node){0};
    _merge_node r = y.len ? y.path[y.len - 1] : (_merge_node){0};
    c3_i cmp = !x.len ? 1 : !y.len ? -1 : _merge_cmp(l, l.mug, r, r.mug);
    _merge_node node = cmp < 0 ? l : r;  // right value wins on equal keys
    if ( cmp == _merge_unknown || (cmp <= 0 && !_merge_advance(&x, map_t)) ||
                               (cmp >= 0 && !_merge_advance(&y, map_t)) ) goto punt;
    if ( intersect_t && cmp ) continue;
    u3_noun left = u3_nul;
    while ( c3n == u3a_pile_done(&pile) ) {
      _merge_node top = ((_merge_frame*)u3a_peek(&pile))->node;
      c3_i order = _merge_cmp(top, top.priority, node, node.priority);
      if ( order == _merge_unknown ) {
        u3z(left);
        goto punt;
      }
      if ( order < 0 ) break;
      left = _merge_close(&pile, left);
    }
    *(_merge_frame*)u3a_push(&pile) = (_merge_frame){node, left};
  }
  u3_noun tree = u3_nul;
  while ( c3n == u3a_pile_done(&pile) ) tree = _merge_close(&pile, tree);
  return tree;

punt:
  while ( c3n == u3a_pile_done(&pile) ) {
    u3_noun left = ((_merge_frame*)u3a_peek(&pile))->left;
    u3a_drop(&pile);
    u3z(left);
  }
  return u3_none;
}

u3_weak
u3q_tree_merge(u3_noun a, u3_noun b, u3_tree_op op)
{
  c3_t map_t = (op != u3_tree_set_intersection);
  c3_t intersect_t = (op != u3_tree_map_union);
  const c3_h cutoff = (0x7fffffffU / 128) * _merge_sample;
  _merge_node x, y;
  if ( !_merge_read(a, &x, map_t) || x.priority > cutoff ||
       !_merge_read(b, &y, map_t) || y.priority > cutoff ) return u3_none;
  if ( x.mug == y.mug ) {
    if ( !intersect_t ) return u3_none;
    // Persistent edits usually leave one of the root's children untouched.
    // Recursion can return those children directly.
    u3_noun l = u3t(a), r = u3t(b);
    if ( (u3h(l) && u3h(l) == u3h(r)) ||
         (u3t(l) && u3t(l) == u3t(r)) ) return u3_none;
  }
  c3_h p = _merge_rank(x, map_t), q = _merge_rank(y, map_t);
  c3_h small = c3_min(p, q), large = c3_max(p, q);
  if ( small && large <= cutoff && large <= 16ULL * small ) {
    return _merge_walk(a, b, map_t, intersect_t);
  }
  return u3_none;
}
