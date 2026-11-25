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

u3_noun
u3qe_loss(u3_noun hel, u3_noun hev)
{
  u3_noun left = u3qb_lent(hel), right = u3qb_lent(hev);
  if ( c3n == u3a_is_cat(right) ) return u3m_bail(c3__fail);
  c3_w cap = c3_min(u3r_word(0, left), right);
  u3z(left);
  if ( !cap ) return u3_nul;

  static_assert(u3a_cells < SIZE_MAX / sizeof(_loss_match), "match array size");
  static_assert(u3a_cells < SIZE_MAX / sizeof(_loss_best), "candidate array size");
  _loss_match* match = u3a_malloc(right * sizeof(*match));
  _loss_best* best = u3a_malloc(cap * sizeof(*best));
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
