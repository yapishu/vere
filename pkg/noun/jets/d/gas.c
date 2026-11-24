/// @file

#include "gas.h"
#include "jets/q.h"

typedef struct {
  u3_noun item;
  c3_h mug;
} _gas_row;

static inline u3_noun
_gas_key(u3_noun item, c3_t map)
{
  return map ? u3h(item) : item;
}

// Adjacent identical keys need one row. Keep the last map value.
static u3_noun
_gas_run(u3_noun* list, c3_t map)
{
  u3_noun item = u3h(*list);
  u3_noun key = _gas_key(item, map);
  *list = u3t(*list);
  while ( *list ) {
    u3_noun next = u3h(*list);
    if ( key != _gas_key(next, map) ) break;
    item = next;
    *list = u3t(*list);
  }
  return item;
}

static c3_t
_gas_before(_gas_row a, _gas_row b, c3_t map)
{
  if ( a.mug != b.mug ) return a.mug < b.mug;
  u3_noun x = _gas_key(a.item, map), y = _gas_key(b.item, map);
  return x == y || c3y == u3qc_dor(x, y);
}

// Stable comparison sort for small batches and equal-mug buckets.
static void
_gas_sort(_gas_row* rows, _gas_row* tmp, size_t n, c3_t map)
{
  if ( n < 2 ) return;
  size_t mid = n / 2;
  _gas_sort(rows, tmp, mid, map);
  _gas_sort(rows + mid, tmp + mid, n - mid, map);
  if ( _gas_before(rows[mid - 1], rows[mid], map) ) return;

  size_t l = 0, r = mid, i = 0;
  while ( l < mid && r < n ) {
    tmp[i++] = _gas_before(rows[l], rows[r], map) ? rows[l++] : rows[r++];
  }
  while ( l < mid ) tmp[i++] = rows[l++];
  memcpy(rows, tmp, i * sizeof(*rows));
}

// Four stable scatters order 31-bit mugs and leave equal keys in input order.
static void
_gas_radix(_gas_row* rows, _gas_row* tmp, size_t n, c3_t map)
{
  for ( unsigned shift = 0; shift < 32; shift += 8 ) {
    size_t bins[256] = {0};
    for ( size_t i = 0; i < n; ++i ) ++bins[(rows[i].mug >> shift) & 255];
    size_t start = 0;
    for ( unsigned k = 0; k < 256; ++k ) {
      size_t count = bins[k];
      bins[k] = start;
      start += count;
    }
    for ( size_t i = 0; i < n; ++i ) tmp[bins[(rows[i].mug >> shift) & 255]++] = rows[i];
    _gas_row* swap = rows; rows = tmp; tmp = swap;
  }
  for ( size_t lo = 0; lo < n; ) {
    size_t hi = lo + 1;
    while ( hi < n && rows[hi].mug == rows[lo].mug ) ++hi;
    if ( hi - lo > 1 ) _gas_sort(rows + lo, tmp, hi - lo, map);
    lo = hi;
  }
}

typedef struct {
  u3_noun item, left;
  c3_h priority;
} _gas_frame;

static u3_noun
_gas_close(u3a_pile* spine, u3_noun right)
{
  _gas_frame* top = u3a_peek(spine);
  u3_noun tree = u3nt(top->item, top->left, right);
  u3a_drop(spine);
  return tree;
}

// In key order, only the right spine is open. Close each lower-priority
// subtree once. The last subtree closed becomes the next node's left child.
static u3_noun
_gas_tree(_gas_row* rows, size_t n, c3_t map)
{
  u3a_pile spine;
  u3a_pile_prep(&spine, sizeof(_gas_frame), __alignof__(_gas_frame));
  for ( size_t i = 0; i < n; ++i ) {
    u3_noun item = rows[i].item, key = _gas_key(item, map);
    if ( i + 1 < n && rows[i].mug == rows[i + 1].mug
      && c3y == u3r_sing(key, _gas_key(rows[i + 1].item, map)) ) {
      u3z(item);
      continue;
    }
    c3_h priority = u3r_mug_word(rows[i].mug);
    u3_noun left = u3_nul;
    while ( c3n == u3a_pile_done(&spine) ) {
      _gas_frame* top = u3a_peek(&spine);
      if ( top->priority < priority ||
           (top->priority == priority && c3y == u3qc_dor(_gas_key(top->item, map), key)) )
        break;
      left = _gas_close(&spine, left);
    }
    *(_gas_frame*)u3a_push(&spine) = (_gas_frame){ item, left, priority };
  }
  u3_noun tree = u3_nul;
  while ( c3n == u3a_pile_done(&spine) ) tree = _gas_close(&spine, tree);
  return tree;
}

u3_noun
u3qdi_gas_list(u3_noun list, c3_t map)
{
  size_t n = 0;
  u3_noun rest = list, item = 0;
  while ( rest ) {
    item = _gas_run(&rest, map);
    ++n;
  }
  if ( n == 1 ) return u3nt(u3k(item), u3_nul, u3_nul);

  enum { small = 32 };
  _gas_row local[2 * small];
  if ( n > (SIZE_MAX - 256) / (2 * sizeof(_gas_row)) ) u3m_bail(c3__meme);
  _gas_row* rows = n <= small ? local : u3a_malloc(2 * n * sizeof(*rows));
  _gas_row* tmp = rows + n;

  for ( size_t i = 0; i < n; ++i ) {
    item = _gas_run(&list, map);
    rows[i] = (_gas_row){ u3k(item), u3r_mug(_gas_key(item, map)) };
  }
  if ( n <= small ) _gas_sort(rows, tmp, n, map);
  else _gas_radix(rows, tmp, n, map);
  u3_noun tree = _gas_tree(rows, n, map);
  if ( rows != local ) u3a_free(rows);
  return tree;
}
