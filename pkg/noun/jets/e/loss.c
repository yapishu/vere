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

u3_noun
u3qe_loss(u3_noun hel, u3_noun hev)
{
  u3_noun left = u3qb_lent(hel), right = u3qb_lent(hev);
  if ( c3n == u3a_is_cat(right) ) return u3m_bail(c3__fail);
  c3_w cap = c3_min(u3r_word(0, left), right);
  c3_w rows = c3y == u3a_is_cat(left) ? left : 0;
  u3z(left);
  if ( !cap ) return u3_nul;

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
