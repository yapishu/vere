/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

typedef struct {
  u3_noun value;
  c3_w    previous;
} _loss_match;

typedef struct {
  c3_w    end;
  u3_noun chain;
} _loss_best;

typedef struct {
  c3_d mask;
  c3_w previous, count;
} _loss_group;

// Bit j records an increase in LCS length at column j. Carry the shift
// and subtraction through the words as in Allison and Dix, section 3.
// https://www.cantab.net/users/mmlist/ll/Publications/1986IPL/
static void
_loss_row(const _loss_group* groups, c3_w head, c3_w words, c3_d* mask,
          const c3_d* old, c3_d* row)
{
  memset(mask, 0, words * sizeof(*mask));
  for ( c3_w pos = head; pos; pos = groups[pos - 1].previous ) {
    mask[(pos - 1) >> 6] = groups[pos - 1].mask;
  }
  c3_d shift = 1, borrow = 0;
  for ( c3_w w = 0; w < words; ++w ) {
    c3_d bits = old[w], x = bits | mask[w], y = (bits << 1) | shift;
    row[w] = x & ~(x - y - borrow);
    borrow = x < y || (borrow && x == y);
    shift = bits >> 63;
  }
}

static c3_w
_loss_rank(const c3_d* row, c3_w col)
{
  c3_w rank = 0, words = col >> 6;
  for ( c3_w w = 0; w < words; ++w ) rank += __builtin_popcountll(row[w]);
  if ( col & 63 ) rank += __builtin_popcountll(row[words] & ((1ULL << (col & 63)) - 1));
  return rank;
}

static u3_weak
_loss_dense(u3_noun list, c3_w rows, c3_w cols, const _loss_match* match,
            u3p(u3h_root) heads)
{
  enum { stripe = 64, budget = 1024 * 1024 };
  if ( rows < 64 || cols < 64 || rows > budget / sizeof(c3_w) ||
       cols > budget / sizeof(_loss_group) ) return u3_none;
  c3_w words = (cols + 63) >> 6;
  c3_w blocks = (rows + stripe - 1) / stripe;
  c3_d bytes = (c3_d)(blocks + stripe + 3) * words * sizeof(c3_d)
             + (c3_d)rows * sizeof(c3_w) + (c3_d)cols * sizeof(_loss_group);
  if ( bytes > budget ) return u3_none;

  // Look at a distributed sample of the indexed list before allocating.
  c3_w samples = 32, found = 0;
  for ( c3_w s = 0; s < samples; ++s ) {
    c3_w pos = 1 + (c3_d)s * cols / samples;
    for ( c3_w j = 0; pos && j < words; ++j ) {
      pos = match[pos - 1].previous;
      ++found;
    }
  }
  if ( found * 8 < words * samples ) return u3_none;

  // Values absent from the right list cannot contribute to the result.
  while ( list && u3_none == u3h_git(heads, u3h(list)) ) {
    list = u3t(list);
    --rows;
  }
  if ( !rows ) return u3_nul;

  _loss_group* groups = u3a_malloc(cols * sizeof(*groups));
  for ( c3_w i = 0; i < cols; ++i ) {
    c3_w prev = match[i].previous;
    groups[i] = (_loss_group){1ULL << (i & 63), prev, 1};
    if ( prev ) {
      groups[i].count += groups[prev - 1].count;
      if ( (i >> 6) == ((prev - 1) >> 6) ) {
        groups[i].mask |= groups[prev - 1].mask;
        groups[i].previous = groups[prev - 1].previous;
      }
    }
  }
  c3_w* ids = u3a_malloc(rows * sizeof(*ids));
  c3_d pairs = 0;
  for ( c3_w i = 0; i < rows; ++i, list = u3t(list) ) {
    u3_weak last = u3h_git(heads, u3h(list));
    ids[i] = last == u3_none ? 0 : last;
    if ( ids[i] ) pairs += groups[ids[i] - 1].count;
  }
  if ( pairs * 4 < (c3_d)rows * words ) {
    u3a_free(ids);
    u3a_free(groups);
    return u3_none;
  }

  // Keep one checkpoint per stripe. Reconstruct one stripe at a time as
  // traceback walks toward the start of the lists.
  size_t width = words * sizeof(c3_d);
  c3_d* saved = u3a_malloc((blocks + stripe + 3) * width);
  c3_d* work = saved + (size_t)(blocks + 1) * words;
  c3_d* mask = work + (stripe + 1) * words;
  memset(saved, 0, width);
  memcpy(work, saved, width);
  for ( c3_w i = 0; i < rows; ++i ) {
    _loss_row(groups, ids[i], words, mask, work, work);
    if ( (i + 1) % stripe == 0 ) {
      memcpy(saved + (size_t)((i + 1) / stripe) * words, work, width);
    }
  }
  c3_w i = rows, j = cols, length = _loss_rank(work, cols);
  u3_noun result = u3_nul;
  while ( length ) {
    c3_w start = ((i - 1) / stripe) * stripe;
    memcpy(work, saved + (size_t)(start / stripe) * words, width);
    c3_d* row = work;
    for ( c3_w k = start; k < i; ++k, row += words ) {
      _loss_row(groups, ids[k], words, mask, row, row + words);
    }
    while ( length && i > start ) {
      // Preserve the earliest right endpoint, then the earliest left row.
      if ( !(row[(j - 1) >> 6] & (1ULL << ((j - 1) & 63))) ) {
        --j;
        continue;
      }
      if ( _loss_rank(row - words, j) < length ) {
        result = u3nc(u3k(match[j - 1].value), result);
        --j;
        --length;
      }
      --i;
      row -= words;
    }
  }
  u3a_free(saved);
  u3a_free(ids);
  u3a_free(groups);
  return result;
}

typedef struct {
  u3_noun left, right;
  c3_w    row;
} _loss_cursor;

enum { _loss_limit = 64 };

// Keep each cursor alive while structural equality unifies equal subtrees.
static void
_loss_hold(_loss_cursor* cur, u3_noun left, u3_noun right)
{
  left = u3k(left);
  right = u3k(right);
  u3z(cur->left);
  u3z(cur->right);
  cur->left = left;
  cur->right = right;
}

static u3_noun
_loss_trace(u3_noun hev, c3_w rows, c3_w cols, c3_w a, c3_w b,
            c3_w* const reach[])
{
  // Prefix edit distance never decreases along a diagonal. Trace back
  // through the previous frontier, dropping a right item first on a
  // tie, as in +loss. A greedy forward path can choose a different LCS.
  c3_w i = rows, j = cols, skip[_loss_limit], count = 0;
  while ( a || b ) {
    const c3_w* prev = reach[a + b - 1];
    if ( b && j && prev[a] >= i ) {
      skip[count++] = --j;
      --b;
    }
    else if ( a && i && prev[a - 1] >= i - 1 ) {
      --i;
      --a;
    }
    else {
      --i;
      --j;
    }
  }

  u3_noun result, *end = &result;
  c3_w pos = 0;
  while ( count ) {
    c3_w stop = skip[--count];
    u3_noun run = hev;
    for ( ; pos < stop; ++pos ) hev = u3t(hev);
    end = u3i_list_prefix(end, run, hev);
    hev = u3t(hev);
    ++pos;
  }
  *end = u3k(hev);
  return result;
}

// Myers stores the furthest reachable row on each diagonal. Wu et al.
// visit them in order of deletions from the shorter list.
// https://doi.org/10.1007/BF01840446
// https://doi.org/10.1016/0020-0190(90)90035-V
// reach[a+b][a] is the longest left prefix reachable after skipping
// a left items and b right items. The two outer edges supply the next band.
static u3_weak
_loss_frontier(u3_noun hel, u3_noun hev, c3_w rows, c3_w cols, c3_w* prefix)
{
  c3_w shorter = c3_min(rows, cols);
  c3_w a_max = rows - shorter, b_max = cols - shorter;
  if ( a_max + b_max > _loss_limit ) return u3_none;

  c3_w best = 0, depth = 0;
  c3_d work = 0;
  c3_w* reach[_loss_limit + 1];
  u3_post cap = u3R->cap_p;
  _loss_cursor lanes[2 * _loss_limit + 1] = {0};
  _loss_cursor* lane = lanes + _loss_limit;
  u3_weak result = u3_none;

  // Visit each outer edge from its axis to the corner. The neighboring
  // diagonals still hold the two predecessors when this diagonal advances.
  for ( ; a_max + b_max <= _loss_limit; ++a_max, ++b_max ) {
    for ( c3_w step = 0; step <= a_max + b_max; ++step ) {
      c3_w a = c3_min(step, a_max);
      c3_w b = step < a_max ? b_max : step - a_max;
      c3_w d = a + b, row = 0;
      _loss_cursor* cur = lane + a - b;
      u3_noun left = hel, right = hev;
      if ( a && (!b || cur[-1].row >= cur[1].row) ) {
        row = cur[-1].row + 1;
        left = cur[-1].left ? u3t(cur[-1].left) : 0;
        right = cur[-1].right;
      }
      else if ( b ) {
        row = cur[1].row;
        left = cur[1].left;
        right = cur[1].right ? u3t(cur[1].right) : 0;
      }
      if ( d == depth ) {
        u3a_pile pile;
        u3a_pile_prep(&pile, (d + 1) * sizeof(c3_w), alignof(c3_w));
        reach[depth++] = u3a_push(&pile);
      }
      c3_w start = row;
      while ( left && right ) {
        u3_noun x = u3h(left), y = u3h(right);
        if ( x != y ) {
          if ( c3y == u3a_is_cell(x) && c3y == u3a_is_cell(y) ) {
            _loss_hold(cur, left, right);
          }
          if ( c3n == u3r_sing(x, y) ) break;
        }
        left = u3t(left);
        right = u3t(right);
        ++row;
      }
      _loss_hold(cur, left, right);
      cur->row = row;
      reach[d][a] = row;
      if ( !d ) *prefix = row;
      if ( a == a_max && b == b_max && row >= rows ) {
        if ( !a )      result = u3k(hel);
        else if ( !b ) result = u3k(hev);
        else           result = _loss_trace(hev, rows, cols, a, b, reach);
        goto done;
      }
      // Stop when the search spends more on competing paths than it
      // gains in matched items. Unrelated lists use the indexed search.
      work += 1 + row - start;
      best = c3_max(best, row - a);
      if ( work > 4ULL * (best + 1) + (shorter >> 6) ) goto done;
    }
  }
done:
  u3R->cap_p = cap;
  for ( c3_w i = _loss_limit - c3_min(b_max, _loss_limit);
        i <= _loss_limit + c3_min(a_max, _loss_limit); ++i ) {
    u3z(lanes[i].left);
    u3z(lanes[i].right);
  }
  return result;
}

static u3_noun
_loss_index(u3_noun hel, u3_noun hev, c3_w rows, c3_w right, c3_w cap)
{
  static_assert(u3a_cells < SIZE_MAX / sizeof(_loss_match), "match array size");
  static_assert(u3a_cells < SIZE_MAX / sizeof(_loss_best), "candidate array size");
  _loss_match* match = u3a_malloc(right * sizeof(*match));
  u3p(u3h_root) heads = u3h_new();
  c3_w len = 0;

  // Position zero ends each occurrence chain. Each key owns one table slot.
  for ( c3_w i = 0; i < right; ++i, hev = u3t(hev) ) {
    u3_noun value = u3h(hev);
    u3_weak last = u3h_git(heads, value);
    match[i].value = value;
    match[i].previous = (last == u3_none) ? 0 : last;
    u3h_put(heads, value, i + 1);
  }

  u3_weak dense = _loss_dense(hel, rows, right, match, heads);
  if ( dense != u3_none ) {
    u3h_free(heads);
    u3a_free(match);
    return dense;
  }
  _loss_best* best = u3a_malloc(cap * sizeof(*best));
  for ( ; hel != u3_nul; hel = u3t(hel) ) {
    u3_weak last = u3h_git(heads, u3h(hel));
    if ( last == u3_none ) continue;

    // Walk positions from right to left. A candidate can extend only a
    // chain from a previous input row. Equal endpoints keep their chain.
    c3_w limit = len;
    for ( c3_w pos = last; pos; pos = match[pos - 1].previous ) {
      c3_w lo = limit, hi = limit;
      for ( c3_w step = 1; lo && best[lo - 1].end >= pos; step *= 2 ) {
        lo -= c3_min(lo, step);
      }
      while ( lo < hi ) {
        c3_w mid = lo + (hi - lo) / 2;
        if ( best[mid].end < pos ) lo = mid + 1;
        else hi = mid;
      }
      limit = lo;
      if ( lo < len && best[lo].end == pos ) continue;
      u3_noun prev = lo ? u3k(best[lo - 1].chain) : u3_nul;
      if ( lo == len ) ++len;
      else u3z(best[lo].chain);
      best[lo].end = pos;
      best[lo].chain = u3nc(u3k(match[pos - 1].value), prev);
    }
  }

  u3_noun pro = len ? u3kb_flop(u3k(best[len - 1].chain)) : u3_nul;
  for ( c3_w i = 0; i < len; ++i ) u3z(best[i].chain);
  u3h_free(heads);
  u3a_free(best);
  u3a_free(match);
  return pro;
}

u3_noun
u3qe_loss(u3_noun hel, u3_noun hev)
{
  u3_noun left = u3qb_lent(hel), right = u3qb_lent(hev);
  if ( c3n == u3a_is_cat(right) ) return u3m_bail(c3__fail);
  c3_w cap = c3_min(u3r_word(0, left), right);
  c3_w rows = c3y == u3a_is_cat(left) ? left : 0;
  u3z(left);
  if ( !cap ) return u3_nul;
  c3_w prefix = 0;
  if ( rows && cap >= 32 ) {
    u3_weak pro = _loss_frontier(hel, hev, rows, right, &prefix);
    if ( pro != u3_none ) return pro;
  }
  if ( prefix < (cap + 3) / 4 ) return _loss_index(hel, hev, rows, right, cap);
  if ( prefix == cap ) return u3k(rows == cap ? hel : hev);

  // The first k equal heads attain the earliest possible right endpoint
  // for each length through k. Every later candidate extends that prefix.
  u3_noun start = hel, result;
  for ( c3_w i = 0; i < prefix; ++i ) {
    hel = u3t(hel);
    hev = u3t(hev);
  }
  u3_noun* end = u3i_list_prefix(&result, start, hel);
  *end = u3_nul;
  *end = _loss_index(hel, hev, rows - prefix, right - prefix, cap - prefix);
  return result;
}

static u3_noun
_listp(u3_noun lix)
{
  while ( 1 ) {
    if ( u3_nul == lix ) return c3y;
    if ( c3n == u3du(lix) ) return c3n;
    lix = u3t(lix);
  }
}

u3_noun
u3we_loss(u3_noun cor)
{
  u3_noun hel, hev;

  if ( (u3_none == (hel = u3r_at(u3x_sam_2, cor))) ||
       (u3_none == (hev = u3r_at(u3x_sam_3, cor))) ||
       (c3n == _listp(hel)) ||
       (c3n == _listp(hev)) )
  {
    return u3m_bail(c3__fail);
  } else {
    return u3qe_loss(hel, hev);
  }
}
