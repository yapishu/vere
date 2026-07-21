/// @file
///
/// Link probe for the portable Mesa session table under wasm32-wasi.
///
/// This is intentionally narrower than a Vere-in-browser runtime: it keeps the
/// session binding core honest without pulling in the loom, libuv, LMDB, or a
/// browser transport host.

#include "session.h"

typedef struct _probe_falls {
  c3_w  len_w;
  c3_d  her_d[2][2];
} _probe_falls;

static void
_probe_fell(void* ptr_v, const c3_d her_d[2])
{
  _probe_falls* fal_u = ptr_v;
  if ( fal_u->len_w < 2 ) {
    fal_u->her_d[fal_u->len_w][0] = her_d[0];
    fal_u->her_d[fal_u->len_w][1] = her_d[1];
  }
  fal_u->len_w++;
}

static c3_i
_probe_expect(c3_o val_o, c3_o exp_o)
{
  return ( val_o == exp_o ) ? 0 : 1;
}

int
main(void)
{
  u3_sess_tab* tab_u = u3_sess_tab_init();
  c3_i         bak_a = 1;
  c3_i         bak_b = 2;
  u3_sess*     ses_a = u3_sess_open(tab_u, &bak_a);
  u3_sess*     ses_b = u3_sess_open(tab_u, &bak_b);
  c3_d         zod_d[2] = { 0, 0 };
  c3_d         nec_d[2] = { 0x1234, 0x5678 };

  u3_sess_fresh fre_1 = { .rif_w = 1, .bon_d = 2, .seq_d = 3 };
  u3_sess_fresh fre_0 = { .rif_w = 1, .bon_d = 2, .seq_d = 2 };
  u3_sess_fresh fre_2 = { .rif_w = 1, .bon_d = 2, .seq_d = 4 };

  if ( NULL != u3_sess_find(tab_u, zod_d) ) {
    return 1;
  }
  if ( ses_a != u3_sess_find_sid(tab_u, ses_a->sid_d) ) {
    return 1;
  }
  if ( 0 != _probe_expect(u3_sess_bind(tab_u, zod_d, fre_1, ses_a), c3y) ) {
    return 1;
  }
  if ( ses_a != u3_sess_find(tab_u, zod_d) ) {
    return 1;
  }
  if ( 0 != _probe_expect(u3_sess_bind(tab_u, zod_d, fre_0, ses_b), c3n) ) {
    return 1;
  }
  if ( ses_a != u3_sess_find(tab_u, zod_d) ) {
    return 1;
  }
  if ( 0 != _probe_expect(u3_sess_bind(tab_u, zod_d, fre_2, ses_b), c3y) ) {
    return 1;
  }
  if ( ses_b != u3_sess_find(tab_u, zod_d) ) {
    return 1;
  }
  if ( 0 != _probe_expect(u3_sess_bind(tab_u, nec_d, fre_1, ses_a), c3y) ) {
    return 1;
  }

  _probe_falls fal_u = {0};
  u3_sess_close(tab_u, ses_a, _probe_fell, &fal_u);
  if ( 1 != fal_u.len_w ) {
    return 1;
  }
  if ( (nec_d[0] != fal_u.her_d[0][0]) || (nec_d[1] != fal_u.her_d[0][1]) ) {
    return 1;
  }
  if ( NULL != u3_sess_find(tab_u, nec_d) ) {
    return 1;
  }
  if ( ses_b != u3_sess_find(tab_u, zod_d) ) {
    return 1;
  }

  u3_sess_close(tab_u, ses_b, _probe_fell, &fal_u);
  if ( 2 != fal_u.len_w ) {
    return 1;
  }
  if ( NULL != u3_sess_find(tab_u, zod_d) ) {
    return 1;
  }

  u3_sess_tab_free(tab_u);
  return 0;
}
