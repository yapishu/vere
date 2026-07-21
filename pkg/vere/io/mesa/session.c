/// @file

#ifndef u3_assert
#  include <stdlib.h>
#  define u3_assert(x) do { if (!(x)) { abort(); } } while (0)
#endif

#include "session.h"

#include <string.h>

/* _sess_key: two-word ship, hashable verstable key.
*/
typedef struct _sess_key {
  c3_d  hed_d;
  c3_d  tel_d;
} _sess_key;

/* _sess_bind: what a ship is bound to, and the freshness horizon.
*/
typedef struct _sess_bind {
  u3_sess*  ses_u;                        //  bound session
  u3_sess_fresh fre_u;                    //  last verified freshness value
} _sess_bind;

static uint64_t
_sess_key_hash(_sess_key key_u)
{
  uint64_t com_d = key_u.hed_d ^ (key_u.tel_d * 0x9e3779b97f4a7c15ull);
  com_d ^= com_d >> 23;
  com_d *= 0x2127599bf4325c37ull;
  com_d ^= com_d >> 47;
  return com_d;
}

static uint64_t
_sess_key_cmpr(_sess_key one_u, _sess_key two_u)
{
  return (one_u.hed_d == two_u.hed_d) && (one_u.tel_d == two_u.tel_d);
}

#define NAME sess_map
#define KEY_TY _sess_key
#define HASH_FN _sess_key_hash
#define CMPR_FN _sess_key_cmpr
#define VAL_TY _sess_bind
#include "verstable.h"

struct _u3_sess_tab {
  sess_map  map_u;                        //  ship -> binding
  u3_sess*  ses_u;                        //  all open sessions
  c3_d      nex_d;                        //  next session id
};

static _sess_key
_sess_key_of(const c3_d her_d[2])
{
  _sess_key key_u = { .hed_d = her_d[0], .tel_d = her_d[1] };
  return key_u;
}

static c3_i
_sess_fresh_cmp(u3_sess_fresh one_u, u3_sess_fresh two_u)
{
  if ( one_u.rif_w != two_u.rif_w ) {
    return (one_u.rif_w > two_u.rif_w) ? 1 : -1;
  }
  if ( one_u.bon_d != two_u.bon_d ) {
    return (one_u.bon_d > two_u.bon_d) ? 1 : -1;
  }
  if ( one_u.seq_d != two_u.seq_d ) {
    return (one_u.seq_d > two_u.seq_d) ? 1 : -1;
  }
  return 0;
}

u3_sess_tab*
u3_sess_tab_init(void)
{
  u3_sess_tab* tab_u = c3_calloc(sizeof(*tab_u));
  vt_init(&tab_u->map_u);
  tab_u->nex_d = 1;
  return tab_u;
}

void
u3_sess_tab_free(u3_sess_tab* tab_u)
{
  u3_sess* ses_u = tab_u->ses_u;
  while ( ses_u ) {
    u3_sess* nex_u = ses_u->nex_u;
    c3_free(ses_u);
    ses_u = nex_u;
  }
  vt_cleanup(&tab_u->map_u);
  c3_free(tab_u);
}

u3_sess*
u3_sess_open(u3_sess_tab* tab_u, void* bak_p)
{
  u3_sess* ses_u = c3_calloc(sizeof(*ses_u));
  ses_u->sid_d  = tab_u->nex_d++;
  ses_u->bak_p  = bak_p;
  ses_u->nex_u  = tab_u->ses_u;
  tab_u->ses_u  = ses_u;
  return ses_u;
}

c3_o
u3_sess_bind(u3_sess_tab* tab_u,
             const c3_d   her_d[2],
             u3_sess_fresh fre_u,
             u3_sess*     ses_u)
{
  _sess_key    key_u = _sess_key_of(her_d);
  sess_map_itr itr_u = vt_get(&tab_u->map_u, key_u);

  if (  !vt_is_end(itr_u)
     && (_sess_fresh_cmp(fre_u, itr_u.data->val.fre_u) <= 0) )
  {
    return c3n;                           //  stale: replayed or reordered
  }

  _sess_bind bin_u = { .ses_u = ses_u, .fre_u = fre_u };
  itr_u = vt_insert(&tab_u->map_u, key_u, bin_u);
  return vt_is_end(itr_u) ? c3n : c3y;
}

u3_sess*
u3_sess_find_sid(u3_sess_tab* tab_u, c3_d sid_d)
{
  u3_sess* ses_u = tab_u->ses_u;
  while ( ses_u ) {
    if ( sid_d == ses_u->sid_d ) {
      return ses_u;
    }
    ses_u = ses_u->nex_u;
  }
  return NULL;
}

u3_sess*
u3_sess_find(u3_sess_tab* tab_u, const c3_d her_d[2])
{
  sess_map_itr itr_u = vt_get(&tab_u->map_u, _sess_key_of(her_d));
  return vt_is_end(itr_u) ? NULL : itr_u.data->val.ses_u;
}

void
u3_sess_close(u3_sess_tab*   tab_u,
              u3_sess*       ses_u,
              u3_sess_fell_f fel_f,
              void*          ptr_v)
{
  //  unbind every ship on this session, notifying per ship.  the entry
  //  itself is kept (with no session) so the freshness horizon survives:
  //  otherwise a replayed old binding packet could capture a ship right
  //  after its session drops.
  //
  {
    sess_map_itr itr_u = vt_first(&tab_u->map_u);
    while ( !vt_is_end(itr_u) ) {
      if ( ses_u == itr_u.data->val.ses_u ) {
        c3_d her_d[2] = { itr_u.data->key.hed_d, itr_u.data->key.tel_d };
        itr_u.data->val.ses_u = NULL;
        if ( fel_f ) {
          fel_f(ptr_v, her_d);
        }
      }
      itr_u = vt_next(itr_u);
    }
  }
  //  unlink and free
  //
  {
    u3_sess** lin_u = &tab_u->ses_u;
    while ( *lin_u && (ses_u != *lin_u) ) {
      lin_u = &(*lin_u)->nex_u;
    }
    if ( *lin_u ) {
      *lin_u = ses_u->nex_u;
    }
    c3_free(ses_u);
  }
}
