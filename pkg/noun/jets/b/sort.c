/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "../c/order.h"


//  c3_w must be able to represent all possible list lengths
//
static_assert( (c3_w_max > u3a_cells),
               "length precision" );

static_assert(
  (u3a_cells < (SIZE_MAX / (3 * sizeof(u3_noun)))),
  "allocation size overflow"
);

static void
_quicksort(u3_noun* restrict arr_u,
    c3_w len_w,
    u3_noun* restrict lef_u,
    u3_noun* restrict rit_u,
    u3j_site* sit_u)
{
  if ( len_w <= 1 ) return;
  u3_noun pivot = arr_u[0];
  c3_w lef_w = 0, rit_w = 0;
  for (c3_w i_w = 1; i_w < len_w; i_w++)
  {
    u3_noun sam = u3nc(u3k(arr_u[i_w]), u3k(pivot));
    c3_o hoz_o = u3x_loob(u3j_gate_slam(sit_u, sam));
    if ( _(hoz_o) )
    {
      lef_u[lef_w++] = arr_u[i_w];
    }
    else
    {
      rit_u[rit_w++] = arr_u[i_w];
    }
  }

  arr_u[lef_w] = pivot;
  memcpy(arr_u,             lef_u, lef_w * sizeof(u3_noun));
  memcpy(arr_u + lef_w + 1, rit_u, rit_w * sizeof(u3_noun));

  _quicksort(arr_u,             lef_w, lef_u, rit_u, sit_u);
  _quicksort(arr_u + lef_w + 1, rit_w, lef_u, rit_u, sit_u);
}

typedef struct {
  u3_noun noun;
  c3_w    pre_w;
} _sort_key;

typedef enum { _sort_custom, _sort_alpha, _sort_depth } _sort_kind;

/* Gate preparation resolves the actual jet and checks its parent axis.
** Both noun orders are total. Distinct handles that tie denote equal
** nouns, so changing their order produces the same Nock list.
*/
static _sort_kind
_sort_site(const u3j_site* sit_u)
{
#ifndef U3_MEMORY_DEBUG
  if ( !(u3C.wag_h & (u3o_trace | u3o_debug_cpu)) &&
       u3_none != sit_u->loc && c3y == sit_u->jet_o &&
       c3y == sit_u->ham_u->liv && c3y == sit_u->ham_u->ice ) {
    if ( u3wc_aor == sit_u->ham_u->fun_f ) return _sort_alpha;
    if ( u3wc_dor == sit_u->ham_u->fun_f ) return _sort_depth;
  }
#endif
  return _sort_custom;
}

/* A normalized prefix of noun order fits in one word. On the left
** spine an atom sorts before a cell, so depth comes first. At the
** first atom +aor compares low bytes, and +dor compares size then
** high bytes. Saturation or truncation can only produce a tie.
** Normalized keys: https://www.wild-inter.net/publications/kuhrt-seeger-wild-graefe-2025
*/
static c3_w
_sort_prefix(u3_noun a, _sort_kind kin_e)
{
  c3_w dep_w = 0;
  while ( c3y == u3du(a) ) {
    if ( 255 == ++dep_w ) return dep_w << (u3a_word_bits - 8);
    a = u3h(a);
  }

  c3_w pre_w = dep_w << (u3a_word_bits - 8);
  c3_w len_w;
  const c3_w* buf_w = u3r_word_buffer(&a, &len_w);

  if ( _sort_alpha == kin_e ) {
    c3_w low_w = *buf_w;
    c3_w rev_w = (4 == sizeof(c3_w)) ? __builtin_bswap32((c3_h)low_w)
                                     : (c3_w)__builtin_bswap64(low_w);
    return pre_w | (rev_w >> 8);
  }

  c3_w byt_w = u3r_met(3, a);
  pre_w |= c3_min(byt_w, 255) << (u3a_word_bits - 16);
  if ( byt_w < 255 ) {
    const c3_y* buf_y = (const c3_y*)buf_w;
    for ( c3_w i_w = 0; i_w < c3_min(byt_w, sizeof(c3_w) - 2); i_w++ ) {
      pre_w |= (c3_w)buf_y[byt_w - 1 - i_w] << (u3a_word_bits - 24 - 8*i_w);
    }
  }
  return pre_w;
}

static inline c3_t
_sort_before(_sort_key a, _sort_key b, _sort_kind kin_e)
{
  if ( a.pre_w != b.pre_w ) return a.pre_w < b.pre_w;
  return c3y == _order_nouns(a.noun, b.noun, _sort_alpha == kin_e);
}

/* Merge with scratch for the left half. The right half stays ahead of
** the write position until the left is exhausted.
*/
static void
_sort_range(_sort_key* arr_u, _sort_key* tmp_u, c3_w len_w, _sort_kind kin_e)
{
  if ( len_w <= 12 ) {
    for ( c3_w i_w = 1; i_w < len_w; i_w++ ) {
      _sort_key key_u = arr_u[i_w];
      c3_w      j_w = i_w;
      while ( j_w && !_sort_before(arr_u[j_w - 1], key_u, kin_e) ) {
        arr_u[j_w] = arr_u[j_w - 1];
        j_w--;
      }
      arr_u[j_w] = key_u;
    }
    return;
  }

  c3_w mid_w = len_w / 2;
  _sort_range(arr_u, tmp_u, mid_w, kin_e);
  _sort_range(arr_u + mid_w, tmp_u, len_w - mid_w, kin_e);
  if ( _sort_before(arr_u[mid_w - 1], arr_u[mid_w], kin_e) ) return;

  memcpy(tmp_u, arr_u, mid_w * sizeof(*tmp_u));
  c3_w lef_w = 0, rit_w = mid_w, pos_w = 0;
  while ( lef_w < mid_w && rit_w < len_w ) {
    if ( _sort_before(tmp_u[lef_w], arr_u[rit_w], kin_e) ) {
      arr_u[pos_w++] = tmp_u[lef_w++];
    }
    else arr_u[pos_w++] = arr_u[rit_w++];
  }
  memcpy(arr_u + pos_w, tmp_u + lef_w, (mid_w - lef_w) * sizeof(*tmp_u));
}

static u3_noun
_sort_known(u3_noun list, c3_w len_w, _sort_kind kin_e)
{
  _sort_key* arr_u = u3a_malloc((len_w + len_w / 2) * sizeof(*arr_u));
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    arr_u[i_w].noun = u3k(u3h(list));
    arr_u[i_w].pre_w = _sort_prefix(arr_u[i_w].noun, kin_e);
    list = u3t(list);
  }

  _sort_range(arr_u, arr_u + len_w, len_w, kin_e);
  u3_noun pro = u3_nul;
  for ( c3_w i_w = len_w; i_w--; ) pro = u3nc(arr_u[i_w].noun, pro);
  u3a_free(arr_u);
  return pro;
}

//  RETAINS list, transfers product
//
static u3_noun
_sort(u3j_site* sit_u, u3_noun list)
{
  if (u3_nul == list) return u3_nul;

  _sort_kind kin_e = _sort_site(sit_u);
  c3_t       ord_t = (_sort_custom != kin_e);
  c3_w       len_w = 1;

  for ( u3_noun cur = list, nex = u3t(cur); u3_nul != nex;
        cur = nex, nex = u3t(nex) ) {
    len_w++;
    if ( ord_t ) {
      ord_t = (c3y == _order_nouns(u3h(cur), u3h(nex), _sort_alpha == kin_e));
    }
  }
  if ( 1 == len_w ) return u3k(list);
  if ( _sort_custom != kin_e ) {
    u3n_arm_effect();
    return ord_t ? u3k(list) : _sort_known(list, len_w, kin_e);
  }

  u3_noun* arr_u = u3a_malloc(sizeof(u3_noun) * len_w * 3);
  u3_noun* lef_u = arr_u + len_w;
  u3_noun* rit_u = lef_u + len_w;
  for (c3_w i_w = 0; i_w < len_w; i_w++)
  {
    //  inlined u3r_cell without any checks
    //  since the list was already validated and measured
    //
    u3a_cell* cel_u = u3a_to_ptr(list);
    arr_u[i_w] = u3k(cel_u->hed);
    list = cel_u->tel;
  }

  _quicksort(arr_u, len_w, lef_u, rit_u, sit_u);

  u3_noun pro = u3_nul;
  for (c3_w i_w = len_w; i_w--;)
  {
    pro = u3nc(arr_u[i_w], pro);
  }
  
  u3a_free(arr_u);

  return pro;
}

u3_noun
u3qb_sort(u3_noun a,
          u3_noun b)
{
  u3_noun  pro;
  u3j_site sit_u;
  u3j_gate_prep(&sit_u, u3k(b));
  pro = _sort(&sit_u, a);
  u3j_gate_lose(&sit_u);
  return pro;
}

u3_noun
u3wb_sort(u3_noun cor)
{
  u3_noun a, b;

  a = u3h(u3h(u3t(cor)));
  b = u3t(u3h(u3t(cor)));
  return u3qb_sort(a, b);
}
