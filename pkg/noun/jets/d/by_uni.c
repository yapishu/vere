/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

// Union visits every node of a shared branch. Validate the cells it
// destructures before returning the branch with its existing sharing.
static void
_by_uni_check(u3_noun tree)
{
  while ( u3_nul != tree ) {
    u3_noun pair, left, right;
    u3x_trel(tree, &pair, &left, &right);
    if ( c3n == u3du(pair) ) u3m_bail(c3__exit);
    if ( left != right ) _by_uni_check(left);
    tree = right;
  }
}

static u3_noun
_by_uni(u3_noun a, u3_noun b)
{
  if ( u3_nul == b ) {
    return u3k(a);
  }
  else if ( u3_nul == a ) {
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

    if ( c3y == u3r_sing(p_n_a, p_n_b) ) {
      if ( a == b ) {
        if ( l_b != r_b ) _by_uni_check(l_b);
        _by_uni_check(r_b);
        return u3k(b);
      }
      u3_noun left = _by_uni(l_a, l_b);
      u3_noun right = _by_uni(r_a, r_b);
      if ( left == l_b && right == r_b ) {
        u3z(left);
        u3z(right);
        return u3k(b);
      }
      return u3nt(u3k(n_b), left, right);
    }
    else if ( c3y == u3qc_mor(p_n_a, p_n_b) ) {
      u3_noun new_a, old_b;

      if ( c3y == u3qc_gor(p_n_b, p_n_a) ) {
        u3_noun new_b  = u3nt(u3k(n_b), u3k(l_b), u3_nul);
        u3_noun new_la = _by_uni(l_a, new_b);
        u3z(new_b);

        new_a = u3nt(u3k(n_a), new_la, u3k(r_a));
        old_b = r_b;
      }
      else {
        u3_noun new_b  = u3nt(u3k(n_b), u3_nul, u3k(r_b));
        u3_noun new_ra = _by_uni(r_a, new_b);
        u3z(new_b);

        new_a = u3nt(u3k(n_a), u3k(l_a), new_ra);
        old_b = l_b;
      }

      {
        u3_noun pro = _by_uni(new_a, old_b);
        u3z(new_a);
        return pro;
      }
    }
    else {
      u3_noun old_a, new_b;

      if ( c3y == u3qc_gor(p_n_a, p_n_b) ) {
        u3_noun new_a  = u3nt(u3k(n_a), u3k(l_a), u3_nul);
        u3_noun new_lb = _by_uni(new_a, l_b);
        u3z(new_a);

        new_b = u3nt(u3k(n_b), new_lb, u3k(r_b));
        old_a = r_a;
      }
      else {
        u3_noun new_a  = u3nt(u3k(n_a), u3_nul, u3k(r_a));
        u3_noun new_rb = _by_uni(new_a, r_b);
        u3z(new_a);

        new_b = u3nt(u3k(n_b), u3k(l_b), new_rb);
        old_a = l_a;
      }

      {
        u3_noun pro = _by_uni(old_a, new_b);
        u3z(new_b);
        return pro;
      }
    }
  }
}

// Canonical maps are ordered by gor and heap-ordered by mor. A merge in
// gor order leaves only the right spine of the output open (Cartesian trees,
// https://doi.org/10.1016/0020-0190(94)00150-2).
typedef struct {
  u3_noun tree;
  c3_h    mug, priority;
} _uni_node;

static c3_t
_uni_read(u3_noun tree, _uni_node* out)
{
  u3_noun item, children;
  if ( c3n == u3r_cell(tree, &item, &children) ||
       c3n == u3du(item) || c3n == u3du(children) ) return 0;
  c3_h mug = u3r_mug(u3h(item));
  *out = (_uni_node){tree, mug, u3r_mug_word(mug)};
  return 1;
}

// Cursors borrow nodes of both inputs. A key can contain one of those
// nodes, so compare without unifying its subtrees. A long tie resumes
// ordinary union, which handles arbitrarily shared keys.
enum { _uni_unknown = 2 };

static c3_i
_uni_order(u3_noun a, u3_noun b, unsigned* remaining)
{
  while ( a != b ) {
    if ( c3y == u3ud(a) ) {
      if ( c3n == u3ud(b) ) return -1;
      if ( c3y == u3r_sing(a, b) ) return 0;
      return c3y == u3qa_lth(a, b) ? -1 : 1;
    }
    if ( c3y == u3ud(b) ) return 1;
    if ( !*remaining ) return _uni_unknown;
    --*remaining;
    c3_i cmp = _uni_order(u3h(a), u3h(b), remaining);
    if ( cmp ) return cmp;
    a = u3t(a);
    b = u3t(b);
  }
  return 0;
}

static c3_i
_uni_cmp(_uni_node a, c3_h x, _uni_node b, c3_h y)
{
  if ( x != y ) return x < y ? -1 : 1;
  u3_noun ka = u3h(u3h(a.tree)), kb = u3h(u3h(b.tree));
  unsigned remaining = 64;
  return _uni_order(ka, kb, &remaining);
}

// The kth smallest priority estimates inverse size without walking either
// map. The estimate selects between two union algorithms.
// https://www.cs.tau.ac.il/~haimk/papers/p225-cohen.pdf
enum { _uni_sample = 12, _uni_slots = 64 };

static c3_h
_uni_rank(_uni_node root)
{
  _uni_node queue[_uni_sample + 1] = {root};
  unsigned len = 1;
  for ( unsigned rank = 1; len; ++rank ) {
    unsigned low = 0;
    for ( unsigned i = 1; i < len; ++i ) {
      if ( queue[i].priority < queue[low].priority ) low = i;
    }
    if ( rank == _uni_sample ) return queue[low].priority;
    u3_noun children = u3t(queue[low].tree);
    queue[low] = queue[--len];
    u3_noun sides[2] = {u3h(children), u3t(children)};
    for ( unsigned i = 0; i < 2; ++i ) {
      if ( sides[i] ) {
        if ( !_uni_read(sides[i], &queue[len]) ) return 0;
        ++len;
      }
    }
  }
  return 0;
}

typedef struct {
  _uni_node path[_uni_slots];
  unsigned  len;
} _uni_iter;

// Check heap order on descent and strict key order between successive
// items. An unsupported shape resumes ordinary union with its original args.
static c3_t
_uni_descend(_uni_iter* it, u3_noun tree, _uni_node parent)
{
  while ( tree ) {
    _uni_node node;
    if ( it->len == _uni_slots || !_uni_read(tree, &node) ||
         (parent.tree && _uni_cmp(parent, parent.priority, node, node.priority) >= 0) )
      return 0;
    it->path[it->len++] = parent = node;
    tree = u3h(u3t(tree));
  }
  return 1;
}

static c3_t
_uni_advance(_uni_iter* it)
{
  _uni_node old = it->path[--it->len];
  if ( !_uni_descend(it, u3t(u3t(old.tree)), old) ) return 0;
  if ( !it->len ) return 1;
  _uni_node next = it->path[it->len - 1];
  return _uni_cmp(old, old.mug, next, next.mug) < 0;
}

typedef struct {
  _uni_node node;
  u3_noun   left;
} _uni_frame;

static u3_noun
_uni_close(u3a_pile* pile, u3_noun right)
{
  _uni_frame top = *(_uni_frame*)u3a_peek(pile);
  u3a_drop(pile);
  u3_noun tree = top.node.tree, children = u3t(tree);
  if ( top.left == u3h(children) && right == u3t(children) ) {
    u3z(top.left); u3z(right);
    return u3k(tree);
  }
  return u3nt(u3k(u3h(tree)), top.left, right);
}

static u3_weak
_uni_merge(u3_noun a, u3_noun b)
{
  _uni_iter x, y;
  x.len = y.len = 0;
  if ( !_uni_descend(&x, a, (_uni_node){0}) ||
       !_uni_descend(&y, b, (_uni_node){0}) ) return u3_none;

  u3a_pile pile;
  u3a_pile_prep(&pile, sizeof(_uni_frame), __alignof__(_uni_frame));
  while ( x.len || y.len ) {
    _uni_node l = x.len ? x.path[x.len - 1] : (_uni_node){0};
    _uni_node r = y.len ? y.path[y.len - 1] : (_uni_node){0};
    c3_i cmp = !x.len ? 1 : !y.len ? -1 : _uni_cmp(l, l.mug, r, r.mug);
    _uni_node node = cmp < 0 ? l : r;  // right value wins on equal keys
    if ( cmp == _uni_unknown || (cmp <= 0 && !_uni_advance(&x)) ||
                               (cmp >= 0 && !_uni_advance(&y)) ) goto punt;
    u3_noun left = u3_nul;
    while ( c3n == u3a_pile_done(&pile) ) {
      _uni_node top = ((_uni_frame*)u3a_peek(&pile))->node;
      c3_i order = _uni_cmp(top, top.priority, node, node.priority);
      if ( order == _uni_unknown ) {
        u3z(left);
        goto punt;
      }
      if ( order < 0 ) break;
      left = _uni_close(&pile, left);
    }
    *(_uni_frame*)u3a_push(&pile) = (_uni_frame){node, left};
  }
  u3_noun tree = u3_nul;
  while ( c3n == u3a_pile_done(&pile) ) tree = _uni_close(&pile, tree);
  return tree;

punt:
  while ( c3n == u3a_pile_done(&pile) ) {
    u3_noun left = ((_uni_frame*)u3a_peek(&pile))->left;
    u3a_drop(&pile);
    u3z(left);
  }
  return u3_none;
}

u3_noun
u3qdb_uni(u3_noun a, u3_noun b)
{
  const c3_h cutoff = (0x7fffffffU / 128) * _uni_sample;
  _uni_node x, y;
  if ( a && b && a != b && _uni_read(a, &x) && _uni_read(b, &y) &&
       x.priority <= cutoff && y.priority <= cutoff &&
       x.mug != y.mug ) {
    c3_h p = _uni_rank(x), q = _uni_rank(y);
    c3_h small = c3_min(p, q), large = c3_max(p, q);
    if ( small && large <= cutoff &&
         large <= 16ULL * small ) {
      u3_weak merged = _uni_merge(a, b);
      if ( merged != u3_none ) return merged;
    }
  }
  return _by_uni(a, b);
}

u3_noun
u3wdb_uni(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
  return u3qdb_uni(a, b);
}

u3_noun
u3kdb_uni(u3_noun a, u3_noun b)
{
  u3_noun pro = u3qdb_uni(a, b);
  u3z(a); u3z(b);
  return pro;
}
