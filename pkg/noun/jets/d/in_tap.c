/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

// A pending subtree inserts its keys into a private list at [out]. After
// inserting the root, the right child inserts before it and the left child
// after it. Disjoint insertion points can be filled in any order.
typedef struct {
  u3_noun  tree;
  u3_noun* out;
} _tap_job;

static inline void
_tap_prefetch(u3_noun tree)
{
  if ( c3y == u3du(tree) ) __builtin_prefetch(u3a_to_ptr(tree));
}

// Interleave independent reads through tree nodes and their child pairs.
// https://www.vldb.org/pvldb/vol9/p252-kocberber.pdf
static u3_noun
_tap_walk(u3_noun root, u3_noun result)
{
  enum { width = 64 };
  typedef _tap_job block[width];
  _tap_job pending[3 * width];
  u3_noun* slots[width];
  u3_noun keys[width], branches[width];
  u3a_pile spill;
  u3a_pile_prep(&spill, sizeof(block), alignof(block));
  unsigned len = 1;
  pending[0] = (_tap_job){root, &result};
  for (;;) {
    unsigned count = c3_min(width, len);
    len -= count;
    for ( unsigned i = 0; i < count; ++i ) {
      _tap_job job = pending[len + i];
      slots[i] = job.out;
      if ( c3n == u3r_cell(job.tree, &keys[i], &branches[i]) ) goto fail;
      _tap_prefetch(branches[i]);
    }
    for ( unsigned i = 0; i < count; ++i ) {
      u3_noun left, right;
      if ( c3n == u3r_cell(branches[i], &left, &right) ) goto fail;
      u3_noun* out = slots[i];
      *out = u3nc(u3k(keys[i]), *out);
      if ( left ) {
        _tap_prefetch(left);
        u3a_cell* cell = u3a_to_ptr(*out);
        pending[len++] = (_tap_job){left, &cell->tel};
      }
      if ( right ) {
        _tap_prefetch(right);
        pending[len++] = (_tap_job){right, out};
      }
    }
    // A batch adds at most [width] jobs. Spill one block to leave
    // room for both children of every job in the next batch.
    if ( len >= 2 * width ) {
      len -= width;
      memcpy(u3a_push(&spill), pending + len, sizeof(block));
    }
    else if ( !len ) {
      if ( c3y == u3a_pile_done(&spill) ) return result;
      memcpy(pending, u3a_peek(&spill), sizeof(block));
      u3a_drop(&spill);
      len = width;
    }
  }
fail:
  u3R->cap_p = spill.beg_p;
  u3z(result);
  return u3m_bail(c3__exit);
}

// Small trees complete in the scalar accumulator. Limit both the number
// of scalar steps and the number of left descents on the native stack.
static u3_noun
_tap_start(u3_noun tree, u3_noun tail, unsigned* remaining,
           unsigned depth, c3_t senior)
{
  while ( tree ) {
    if ( !*remaining ) return _tap_walk(tree, tail);
    --*remaining;
    u3_noun key, left, right;
    if ( c3n == u3r_trel(tree, &key, &left, &right) ) {
      u3z(tail);
      return u3m_bail(c3__exit);
    }
    if ( left ) {
      tail = depth ? _tap_start(left, tail, remaining, depth - 1, senior)
                   : _tap_walk(left, tail);
    }
    tail = u3nc(senior ? key : u3k(key), tail);
    tree = right;
  }
  return tail;
}

u3_noun
u3qdi_tap(u3_noun a)
{
  if ( !a ) return u3_nul;
  unsigned remaining = 256;
  // Descendants of a senior root are also senior or direct atoms.
  return _tap_start(a, u3_nul, &remaining, 24, c3y == u3a_is_senior(u3R, a));
}

u3_noun
u3wdi_tap(u3_noun cor)
{
  u3_noun a;
  u3x_mean(cor, {u3x_con_sam, &a});
  return u3qdi_tap(a);
}

u3_noun
u3kdi_tap(u3_noun a)
{
  u3_noun b = u3qdi_tap(a);
  u3z(a);
  return b;
}
