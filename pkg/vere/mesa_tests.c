/// @file
///
/// Integration tests for the mesa fragment-reassembly path. These drive the
/// request state machine directly (mirroring the state that
/// _mesa_req_pact_init builds for a first PAGE) and feed it the kind of
/// attacker-controlled follow-up fragment a malicious peer could send.

#include "./io/mesa.c"
#include "./io/mesa/quic.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 22);
  u3m_pave(c3y);
}

/* _mk_req(): build the pending-request state a normal first PAGE produces.
**
**  Mirrors the relevant allocations in _mesa_req_pact_init: a destination
**  buffer [dat_y] sized by the first page's [tob_d] assuming 1024-byte
**  (boq 13) fragments, plus the received-bitset. The lss verifier, gauge,
**  and per-fragment state are left null: the malicious fragments below are
**  rejected before any of those are touched.
*/
static void
_mk_req(u3_pend_req* req_u,
        u3_peer*     per_u,
        u3_mesa*     sam_u,
        arena*       are_u,
        c3_d         tob_d)
{
  memset(sam_u, 0, sizeof(*sam_u));
  memset(per_u, 0, sizeof(*per_u));
  memset(req_u, 0, sizeof(*req_u));

  per_u->sam_u = sam_u;

  req_u->per_u = per_u;
  req_u->tob_d = tob_d;
  req_u->tof_d = mesa_num_leaves(tob_d);
  req_u->dat_y = new(are_u, c3_y, tob_d);   //  destination sized by first page
  bitset_init(&req_u->was_u, 1024, are_u);
}

/* _test_frag_heap_overflow(): C2 (SECURITY-AUDIT) — remote heap overflow.
**
**  The destination buffer [req_u->dat_y] is allocated once, sized by the
**  first page's [tob_d] assuming boq 13. Requests are keyed only on the path
**  string, so every subsequent fragment maps to the same request while
**  carrying its own attacker-controlled [tob_d]/[boq_y]/[fra_d]/[len_w]. The
**  write in _mesa_req_pact_done uses those fields with no check that they
**  match what the buffer was sized for:
**
**    c3_w siz_w = (1 << (nam_u->boq_y - 3));
**    memcpy(req_u->dat_y + (siz_w * nam_u->fra_d), dat_u->fra_y, dat_u->len_w);
**
**  Here boq_y = 31 makes siz_w = 1<<28 (256 MB) and fra_d = 1, so the write
**  lands ~256 MB past a 2 KB buffer: a wild heap write with attacker bytes
**  and offset. Pre-fix this crashes; post-fix the fragment must be dropped.
*/
static c3_i
_test_frag_heap_overflow(void)
{
  arena       are_u = arena_create(1 << 20);
  u3_mesa     sam_u;
  u3_peer     per_u;
  u3_pend_req req_u;

  //  first page set up a 2048-byte (2-leaf, boq 13) message
  //
  _mk_req(&req_u, &per_u, &sam_u, &are_u, 2048);

  //  malicious follow-up fragment
  //
  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.fra_d = 1;
  nam_u.boq_y = 31;                 //  siz_w = 1<<28 == 256 MB

  c3_y fra_y[16];
  memset(fra_y, 0x41, sizeof(fra_y));

  u3_mesa_data dat_u;
  memset(&dat_u, 0, sizeof(dat_u));
  dat_u.tob_d        = 1u << 24;    //  16 MB -> num_leaves >> fra_d, clears 1132 check
  dat_u.len_w        = sizeof(fra_y);
  dat_u.fra_y        = fra_y;
  dat_u.aut_u.typ_e  = AUTH_SIGN;   //  not AUTH_PAIR

  sockaddr_in lan_u;
  memset(&lan_u, 0, sizeof(lan_u));

  //  pre-fix: wild heap write ~256 MB past dat_y -> crash
  //  post-fix: rejected (boq != 13 / tob mismatch / offset OOB) -> drop
  //
  _mesa_req_pact_done(&req_u, &nam_u, &dat_u, 0, _mesa_lane_udp4(lan_u));

  //  if we survive, the malicious fragment must have been dropped, not
  //  processed (the accept path decrements out_d / records the fragment)
  //
  if ( 0 != req_u.out_d ) {
    fprintf(stderr, "mesa: C2 — malicious fragment was accepted\r\n");
    arena_free(&are_u);
    return 1;
  }

  arena_free(&are_u);
  return 0;
}

/* _test_frag_boq_overflow(): C2 variant — small boq, large fra_d.
**
**  The same write with boq_y = 13 (siz_w = 1024) and fra_d = 5000 lands at
**  offset 5,120,000 into the 2 KB buffer. [fra_d] is bounded only against
**  num_leaves(dat_u->tob_d) — *this* packet's tob_d — so an attacker sets
**  tob_d huge to clear that gate while fra_d still points far past dat_y.
*/
static c3_i
_test_frag_boq_overflow(void)
{
  arena       are_u = arena_create(1 << 20);
  u3_mesa     sam_u;
  u3_peer     per_u;
  u3_pend_req req_u;

  _mk_req(&req_u, &per_u, &sam_u, &are_u, 2048);

  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.fra_d = 5000;               //  offset 1024 * 5000 = 5,120,000
  nam_u.boq_y = 13;

  c3_y fra_y[16];
  memset(fra_y, 0x42, sizeof(fra_y));

  u3_mesa_data dat_u;
  memset(&dat_u, 0, sizeof(dat_u));
  dat_u.tob_d        = 16u << 20;   //  16 MB -> num_leaves ~16384 > 5000
  dat_u.len_w        = sizeof(fra_y);
  dat_u.fra_y        = fra_y;
  dat_u.aut_u.typ_e  = AUTH_SIGN;

  sockaddr_in lan_u;
  memset(&lan_u, 0, sizeof(lan_u));

  _mesa_req_pact_done(&req_u, &nam_u, &dat_u, 0, _mesa_lane_udp4(lan_u));

  if ( 0 != req_u.out_d ) {
    fprintf(stderr, "mesa: C2 — oversize fra_d fragment was accepted\r\n");
    arena_free(&are_u);
    return 1;
  }

  arena_free(&are_u);
  return 0;
}

/* _test_frag_valid_accepted(): the C2 fix must not reject legitimate traffic.
**
**  A well-formed fragment — matching tob_d, boq 13, in-range fra_d, fitting
**  within the buffer — must still be written into dat_y and recorded. Drives
**  the misordered-queue branch (los_u->counter != fra_d) so no lss crypto or
**  resend timer is needed.
*/
static c3_i
_test_frag_valid_accepted(void)
{
  arena       are_u = arena_create(1 << 20);
  u3_mesa     sam_u;
  u3_peer     per_u;
  u3_pend_req req_u;

  _mk_req(&req_u, &per_u, &sam_u, &are_u, 2048);

  //  zero the destination (arena memory is not zeroed) so we can see the write
  //
  memset(req_u.dat_y, 0, 2048);
  req_u.out_d = 1;                       //  accept path decrements this to 0

  //  minimal state for the misordered-queue branch
  //
  req_u.los_u = new(&are_u, lss_verifier, 1);
  memset(req_u.los_u, 0, sizeof(*req_u.los_u));
  req_u.los_u->counter = 0;              //  != fra_d below -> misordered branch

  req_u.gag_u = new(&are_u, u3_gage, 1);
  _init_gage(req_u.gag_u);

  req_u.wat_u = new(&are_u, u3_pact_stat, req_u.tof_d + 2);
  memset(req_u.wat_u, 0, sizeof(u3_pact_stat) * (req_u.tof_d + 2));

  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.fra_d = 1;                       //  in range (tof_d == 2)
  nam_u.boq_y = 13;

  c3_y fra_y[16];
  memset(fra_y, 0x43, sizeof(fra_y));

  u3_mesa_data dat_u;
  memset(&dat_u, 0, sizeof(dat_u));
  dat_u.tob_d        = 2048;             //  matches the request
  dat_u.len_w        = sizeof(fra_y);
  dat_u.fra_y        = fra_y;
  dat_u.aut_u.typ_e  = AUTH_SIGN;

  sockaddr_in lan_u;
  memset(&lan_u, 0, sizeof(lan_u));

  _mesa_req_pact_done(&req_u, &nam_u, &dat_u, 0, _mesa_lane_udp4(lan_u));

  c3_i ret_i = 0;

  if ( c3n == bitset_has(&req_u.was_u, nam_u.fra_d) ) {
    fprintf(stderr, "mesa: valid fragment was not recorded\r\n");
    ret_i = 1;
  }

  //  written at offset siz_w * fra_d == 1024 * 1
  //
  if ( 0 != memcmp(req_u.dat_y + 1024, fra_y, sizeof(fra_y)) ) {
    fprintf(stderr, "mesa: valid fragment was not written to dat_y\r\n");
    ret_i = 1;
  }

  arena_free(&are_u);
  return ret_i;
}

/* _mk_driver(): stand up the minimal mesa driver state for hear-path tests.
**
**  Mirrors the parts of u3_mesa_io_init the receive path actually touches:
**  permanent + per-packet arenas, the verstable maps, and a pier stub whose
**  only field read here is who_d. The pit-clear libuv timer is skipped — the
**  paths under test never arm it.
*/
static u3_mesa*
_mk_driver(u3_ship who_u)
{
  arena    par_u = arena_create(1 << 20);
  u3_mesa* sam_u = new(&par_u, u3_mesa, 1);
  memset(sam_u, 0, sizeof(*sam_u));
  sam_u->par_u = par_u;
  sam_u->are_u = arena_create(1 << 20);

  u3_pier* pir_u = c3_calloc(sizeof(*pir_u));
  pir_u->who_d[0] = who_u[0];
  pir_u->who_d[1] = who_u[1];
  pir_u->fak_o    = c3y;
  sam_u->pir_u    = pir_u;

  vt_init(&sam_u->pit_u);
  vt_init(&sam_u->per_u);
  vt_init(&sam_u->gag_u);
  vt_init(&sam_u->jum_u);
  vt_init(&sam_u->req_u);

  return sam_u;
}

/* _test_page_init_bad_boq(): H2 (SECURITY-AUDIT) — driver-level harness.
**
**  Drives the real receive dispatch: a PAGE responding to a peek we have
**  outstanding (request in CTAG_WAIT) flows through _mesa_hear_page. The
**  leaf-count math (mesa_num_leaves) assumes boq 13 (1024-byte leaves), and
**  _mesa_req_pact_init's buffer sizing relies on it; a first page carrying
**  any other block size is now rejected in _mesa_hear_page before that math
**  runs, rather than aborting deeper in the request path.
**
**  We pre-insert the peer so _meet_peer (which scries the pier) is skipped,
**  and register the CTAG_WAIT request under the packet's path.
*/
static c3_i
_test_page_init_bad_boq(void)
{
  u3_ship  who_u = { 0x42, 0 };
  u3_mesa* sam_u = _mk_driver(who_u);

  //  pre-existing peer for the sender (== us), so no pier scry is needed
  //
  u3_peer* per_u = new(&sam_u->par_u, u3_peer, 1);
  _init_peer(sam_u, per_u);
  per_u->her_u[0] = who_u[0];
  per_u->her_u[1] = who_u[1];
  _mesa_put_peer(sam_u, who_u, per_u);

  //  the path keys the request; the incoming page must carry the same path
  //
  c3_c* pax_c = "/test/path";

  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.her_u[0] = who_u[0];
  nam_u.her_u[1] = who_u[1];
  nam_u.boq_y    = 31;             //  H2 trigger: not boq 13
  nam_u.fra_d    = 0;
  nam_u.nit_o    = c3y;
  nam_u.pat_c    = pax_c;
  nam_u.pat_s    = strlen(pax_c);

  //  we sent a peek for this path -> request is waiting
  //
  _mesa_put_request(sam_u, &nam_u, (u3_pend_req*)CTAG_WAIT);

  //  craft the malicious first PAGE: bad block size, multi-fragment size
  //
  u3_mesa_pict* pic_u = new(&sam_u->are_u, u3_mesa_pict, 1);
  memset(pic_u, 0, sizeof(*pic_u));
  pic_u->sam_u                       = sam_u;
  pic_u->pac_u.hed_u.typ_y           = PACT_PAGE;
  pic_u->pac_u.hed_u.hop_y           = 0;
  pic_u->pac_u.pag_u.nam_u           = nam_u;
  pic_u->pac_u.pag_u.dat_u.tob_d     = 1u << 20;   //  multi-fragment
  pic_u->pac_u.pag_u.dat_u.len_w     = 0;
  pic_u->pac_u.pag_u.dat_u.aut_u.typ_e = AUTH_SIGN;

  sockaddr_in lan_u;
  memset(&lan_u, 0, sizeof(lan_u));
  lan_u.sin_family = AF_INET;

  //  post-fix: dropped at the boq guard (STRANGE bumps dop_w); the request
  //  stays CTAG_WAIT and never reaches _mesa_req_pact_init
  //
  _mesa_hear_page(pic_u, _mesa_lane_udp4(lan_u));

  c3_i ret_i = 0;
  if ( 1 != sam_u->sat_u.dop_w ) {
    fprintf(stderr, "mesa: H2 — bad-boq page not flagged STRANGE "
                    "(dop_w=%u)\r\n", sam_u->sat_u.dop_w);
    ret_i = 1;
  }
  if ( (u3_pend_req*)CTAG_WAIT != _mesa_get_request(sam_u, &nam_u) ) {
    fprintf(stderr, "mesa: H2 — bad-boq page was not dropped cleanly\r\n");
    ret_i = 1;
  }

  c3_free(sam_u->pir_u);
  arena_free(&sam_u->are_u);
  arena_free(&sam_u->par_u);
  return ret_i;
}

/* _test_wire_page_init_bad_boq(): H2 — full wire-level harness.
**
**  Like _test_page_init_bad_boq, but drives the true UDP entry point
**  _mesa_hear from raw bytes: a PAGE pact is encoded with
**  mesa_etch_pact_to_buf (which computes the header mug), then handed to
**  _mesa_hear, which runs mesa_is_new_pact + mesa_sift_pact_from_buf before
**  dispatching to _mesa_hear_page. This exercises the packet codec in
**  addition to the request path. The encoded bad block size must round-trip
**  through sift and then be dropped by the H2 boq guard.
*/
static c3_i
_test_wire_page_init_bad_boq(void)
{
  u3_ship  who_u = { 0x42, 0 };
  u3_mesa* sam_u = _mk_driver(who_u);

  u3_peer* per_u = new(&sam_u->par_u, u3_peer, 1);
  _init_peer(sam_u, per_u);
  per_u->her_u[0] = who_u[0];
  per_u->her_u[1] = who_u[1];
  _mesa_put_peer(sam_u, who_u, per_u);

  c3_c* pax_c = "/test/path";

  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.her_u[0] = who_u[0];
  nam_u.her_u[1] = who_u[1];
  nam_u.rif_w    = 0;
  nam_u.boq_y    = 31;            //  H2 trigger: not boq 13
  nam_u.nit_o    = c3y;           //  init page; fra_d implicitly 0
  nam_u.fra_d    = 0;
  nam_u.pat_c    = pax_c;
  nam_u.pat_s    = strlen(pax_c);

  _mesa_put_request(sam_u, &nam_u, (u3_pend_req*)CTAG_WAIT);

  //  build a real PAGE pact and encode it to wire bytes
  //
  c3_y sig_y[64];
  memset(sig_y, 0, sizeof(sig_y));

  u3_mesa_pact pac_u;
  memset(&pac_u, 0, sizeof(pac_u));
  pac_u.hed_u.pro_y              = MESA_VER;   //  required by etch/sift
  pac_u.hed_u.nex_y              = HOP_NONE;
  pac_u.hed_u.typ_y              = PACT_PAGE;
  pac_u.hed_u.hop_y              = 0;
  pac_u.pag_u.nam_u              = nam_u;
  pac_u.pag_u.dat_u.tob_d        = 1u << 20;   //  multi-fragment
  pac_u.pag_u.dat_u.len_w        = 0;
  pac_u.pag_u.dat_u.fra_y        = sig_y;      //  unused (len 0), non-NULL
  pac_u.pag_u.dat_u.aut_u.typ_e  = AUTH_SIGN;
  memcpy(pac_u.pag_u.dat_u.aut_u.sig_y, sig_y, sizeof(sig_y));

  c3_y buf_y[PACT_SIZE];
  memset(buf_y, 0, sizeof(buf_y));
  c3_w len_w = mesa_etch_pact_to_buf(buf_y, PACT_SIZE, &pac_u);

  c3_i ret_i = 0;

  //  the encoded bytes must be recognized as a new mesa packet
  //
  if ( c3n == mesa_is_new_pact(buf_y, len_w) ) {
    fprintf(stderr, "mesa: wire — encoded packet not recognized as mesa\r\n");
    ret_i = 1;
  }
  else {
    sockaddr_in lan_u;
    memset(&lan_u, 0, sizeof(lan_u));
    lan_u.sin_family = AF_INET;

    //  true wire entry: sift + dispatch into _mesa_hear_page
    //
    _mesa_hear(sam_u, (const struct sockaddr*)&lan_u, len_w, buf_y);

    //  the packet must actually have traversed sift -> dispatch and hit the
    //  H2 boq guard (STRANGE bumps dop_w). Without this, a silent sift
    //  failure would also leave the request CTAG_WAIT and masquerade as a
    //  pass.
    //
    if ( 1 != sam_u->sat_u.dop_w ) {
      fprintf(stderr, "mesa: H2 (wire) — page did not reach the boq guard "
                      "(dop_w=%u)\r\n", sam_u->sat_u.dop_w);
      ret_i = 1;
    }

    if ( (u3_pend_req*)CTAG_WAIT != _mesa_get_request(sam_u, &nam_u) ) {
      fprintf(stderr, "mesa: H2 (wire) — bad-boq page not dropped cleanly\r\n");
      ret_i = 1;
    }
  }

  c3_free(sam_u->pir_u);
  arena_free(&sam_u->are_u);
  arena_free(&sam_u->par_u);
  return ret_i;
}

/* _sess_fell_log: ships reported dead by u3_sess_close, in order.
*/
typedef struct _sess_fell_log {
  c3_w  len_w;
  c3_d  her_d[8][2];
} _sess_fell_log;

static void
_test_sess_fell_cb(void* ptr_v, const c3_d her_d[2])
{
  _sess_fell_log* log_u = ptr_v;
  log_u->her_d[log_u->len_w][0] = her_d[0];
  log_u->her_d[log_u->len_w][1] = her_d[1];
  log_u->len_w++;
}

static u3_sess_fresh
_test_sess_fresh(c3_w rif_w, c3_d bon_d, c3_d seq_d)
{
  return (u3_sess_fresh){
    .rif_w = rif_w,
    .bon_d = bon_d,
    .seq_d = seq_d,
  };
}

/* _test_sess_binding(): session table: bind, freshness, supersession, close.
*/
static c3_i
_test_sess_binding(void)
{
  c3_i ret_i = 0;

  u3_sess_tab* tab_u = u3_sess_tab_init();
  u3_sess*     one_u = u3_sess_open(tab_u, (void*)0x1);
  u3_sess*     two_u = u3_sess_open(tab_u, (void*)0x2);
  c3_d         one_sid_d = one_u->sid_d;

  c3_d her_d[2] = { 0xdead, 0xbeef };
  c3_d you_d[2] = { 0xcafe, 0xfeed };

  //  first verified packet binds
  //
  if ( c3y != u3_sess_bind(tab_u, her_d, _test_sess_fresh(0, 7, 5), one_u) ) {
    fprintf(stderr, "sess: initial bind rejected\r\n");
    ret_i = 1;
  }
  if ( one_u != u3_sess_find(tab_u, her_d) ) {
    fprintf(stderr, "sess: bound session not found\r\n");
    ret_i = 1;
  }
  if ( NULL != u3_sess_find(tab_u, you_d) ) {
    fprintf(stderr, "sess: unbound ship found\r\n");
    ret_i = 1;
  }
  if ( one_u != u3_sess_find_sid(tab_u, one_u->sid_d) ) {
    fprintf(stderr, "sess: session id lookup failed\r\n");
    ret_i = 1;
  }

  //  a replayed (stale-seq) binding must not steal the session
  //
  if ( c3n != u3_sess_bind(tab_u, her_d, _test_sess_fresh(0, 7, 5), two_u) ) {
    fprintf(stderr, "sess: replayed seq accepted\r\n");
    ret_i = 1;
  }
  if ( c3n != u3_sess_bind(tab_u, her_d, _test_sess_fresh(0, 7, 3), two_u) ) {
    fprintf(stderr, "sess: stale seq accepted\r\n");
    ret_i = 1;
  }
  if ( c3n != u3_sess_bind(tab_u, her_d, _test_sess_fresh(0, 6, 100), two_u) ) {
    fprintf(stderr, "sess: stale bone accepted\r\n");
    ret_i = 1;
  }
  if ( one_u != u3_sess_find(tab_u, her_d) ) {
    fprintf(stderr, "sess: binding stolen by stale seq\r\n");
    ret_i = 1;
  }

  //  a fresher verified binding supersedes (browser refresh)
  //
  if ( c3y != u3_sess_bind(tab_u, her_d, _test_sess_fresh(0, 7, 6), two_u) ) {
    fprintf(stderr, "sess: fresh rebind rejected\r\n");
    ret_i = 1;
  }
  if ( two_u != u3_sess_find(tab_u, her_d) ) {
    fprintf(stderr, "sess: fresh rebind did not supersede\r\n");
    ret_i = 1;
  }

  //  closing a session drops only its own bindings, reporting each ship
  //
  {
    _sess_fell_log log_u = {0};
    if ( c3y != u3_sess_bind(tab_u, you_d, _test_sess_fresh(0, 2, 4), one_u) ) {
      fprintf(stderr, "sess: second-ship bind rejected\r\n");
      ret_i = 1;
    }
    u3_sess_close(tab_u, one_u, _test_sess_fell_cb, &log_u);

    if ( 1 != log_u.len_w ) {
      fprintf(stderr, "sess: close reported %u ships, not 1\r\n", log_u.len_w);
      ret_i = 1;
    }
    else if ( (you_d[0] != log_u.her_d[0][0]) ||
              (you_d[1] != log_u.her_d[0][1]) ) {
      fprintf(stderr, "sess: close reported wrong ship\r\n");
      ret_i = 1;
    }
    if ( NULL != u3_sess_find(tab_u, you_d) ) {
      fprintf(stderr, "sess: binding survived session close\r\n");
      ret_i = 1;
    }
    if ( two_u != u3_sess_find(tab_u, her_d) ) {
      fprintf(stderr, "sess: unrelated binding dropped by close\r\n");
      ret_i = 1;
    }
    if ( NULL != u3_sess_find_sid(tab_u, one_sid_d) ) {
      fprintf(stderr, "sess: closed session id survived close\r\n");
      ret_i = 1;
    }
  }

  //  the freshness horizon survives session close: a replayed old
  //  binding cannot capture a just-disconnected ship
  //
  if ( c3n != u3_sess_bind(tab_u, you_d, _test_sess_fresh(0, 2, 3), two_u) ) {
    fprintf(stderr, "sess: freshness horizon lost after close\r\n");
    ret_i = 1;
  }
  if ( c3y != u3_sess_bind(tab_u, you_d, _test_sess_fresh(0, 2, 5), two_u) ) {
    fprintf(stderr, "sess: fresh rebind after close rejected\r\n");
    ret_i = 1;
  }
  if ( two_u != u3_sess_find(tab_u, you_d) ) {
    fprintf(stderr, "sess: rebind after close not found\r\n");
    ret_i = 1;
  }

  u3_sess_tab_free(tab_u);
  return ret_i;
}

static u3_noun
_test_bind_dat(c3_d her_d[2],
               c3_w rif_w,
               c3_d bon_d,
               c3_d seq_d,
               u3_noun lan)
{
  return u3nc(u3i_chubs(2, her_d),
              u3nq(u3i_word(rif_w), u3i_chub(bon_d),
                   u3i_chub(seq_d), lan));
}

/* _test_session_bind_gift(): authenticated %bind gifts drive session table.
*/
static c3_i
_test_session_bind_gift(void)
{
  u3_mesa sam_u;
  memset(&sam_u, 0, sizeof(sam_u));
  sam_u.sab_u = u3_sess_tab_init();

  u3_sess* one_u = u3_sess_open(sam_u.sab_u, (void*)0x1);
  u3_sess* two_u = u3_sess_open(sam_u.sab_u, (void*)0x2);
  c3_d her_d[2] = { 0xaaaa, 0xbbbb };

  c3_i ret_i = 0;

  if ( c3y != _mesa_kick(&sam_u,
                         c3__bind,
                         _test_bind_dat(her_d, 0, 7, 5,
                                        u3i_chub(_mesa_encode_session_lane(one_u->sid_d)))) )
  {
    fprintf(stderr, "mesa: session bind gift rejected\r\n");
    ret_i = 1;
  }
  if ( one_u != u3_sess_find(sam_u.sab_u, her_d) ) {
    fprintf(stderr, "mesa: bind gift did not bind session\r\n");
    ret_i = 1;
  }

  if ( c3y != _mesa_kick(&sam_u,
                         c3__bind,
                         _test_bind_dat(her_d, 0, 7, 4,
                                        u3i_chub(_mesa_encode_session_lane(two_u->sid_d)))) )
  {
    fprintf(stderr, "mesa: stale bind gift rejected instead of ignored\r\n");
    ret_i = 1;
  }
  if ( one_u != u3_sess_find(sam_u.sab_u, her_d) ) {
    fprintf(stderr, "mesa: stale bind gift stole session\r\n");
    ret_i = 1;
  }

  if ( c3y != _mesa_kick(&sam_u,
                         c3__bind,
                         _test_bind_dat(her_d, 0, 7, 6,
                                        u3nt(c3__if, 0, 0))) )
  {
    fprintf(stderr, "mesa: non-session bind gift rejected instead of ignored\r\n");
    ret_i = 1;
  }
  if ( one_u != u3_sess_find(sam_u.sab_u, her_d) ) {
    fprintf(stderr, "mesa: non-session bind gift changed binding\r\n");
    ret_i = 1;
  }

  if ( c3y != _mesa_kick(&sam_u,
                         c3__bind,
                         _test_bind_dat(her_d, 1, 1, 1,
                                        u3i_chub(_mesa_encode_session_lane(two_u->sid_d)))) )
  {
    fprintf(stderr, "mesa: fresh-rift bind gift rejected\r\n");
    ret_i = 1;
  }
  if ( two_u != u3_sess_find(sam_u.sab_u, her_d) ) {
    fprintf(stderr, "mesa: fresh-rift bind gift did not supersede\r\n");
    ret_i = 1;
  }

  u3_sess_tab_free(sam_u.sab_u);
  return ret_i;
}

typedef struct _test_sess_send_state {
  c3_w        hit_w;
  u3_sess*    ses_u;
  c3_w        len_w;
  c3_y        one_y;
  c3_w        adr_hit_w;
  sockaddr_in adr_u;
  c3_w        adr_len_w;
  c3_y        adr_one_y;
} _test_sess_send_state;

static void
_test_sess_send_cb(void* bak_v, u3_sess* ses_u, c3_y* buf_y, c3_w len_w)
{
  _test_sess_send_state* sat_u = bak_v;

  sat_u->hit_w++;
  sat_u->ses_u = ses_u;
  sat_u->len_w = len_w;
  sat_u->one_y = buf_y[0];
  c3_free(buf_y);
}

static void
_test_addr_send_cb(void* bak_v, const struct sockaddr* adr_u,
                   c3_y* buf_y, c3_w len_w)
{
  _test_sess_send_state* sat_u = bak_v;

  sat_u->adr_hit_w++;
  sat_u->adr_len_w = len_w;
  sat_u->adr_one_y = buf_y[0];
  if ( AF_INET == adr_u->sa_family ) {
    memcpy(&sat_u->adr_u, adr_u, sizeof(sat_u->adr_u));
  }
  c3_free(buf_y);
}

/* _test_session_lane_dispatch(): PIT session lanes use transport vtable.
*/
static c3_i
_test_session_lane_dispatch(void)
{
  u3_mesa sam_u;
  memset(&sam_u, 0, sizeof(sam_u));

  _test_sess_send_state sat_u = {0};
  sam_u.qit_u = (u3_mesa_tran){
    .sen_f = _test_sess_send_cb,
    .bak_v = &sat_u,
  };

  u3_sess ses_u = {
    .sid_d = 42,
    .bak_p = &ses_u,
  };
  u3_pit_addr adr_u = {
    .lan_u = _mesa_lane_sess(&ses_u),
    .nex_p = NULL,
  };
  c3_y buf_y[3] = { 7, 8, 9 };

  _mesa_send_bufs(&sam_u, NULL, buf_y, sizeof(buf_y), &adr_u);

  c3_i ret_i = 0;
  if ( 1 != sat_u.hit_w ) {
    fprintf(stderr, "mesa: session lane did not dispatch once\r\n");
    ret_i = 1;
  }
  if ( &ses_u != sat_u.ses_u ) {
    fprintf(stderr, "mesa: session lane dispatched wrong session\r\n");
    ret_i = 1;
  }
  if ( (sizeof(buf_y) != sat_u.len_w) || (7 != sat_u.one_y) ) {
    fprintf(stderr, "mesa: session lane dispatched wrong packet\r\n");
    ret_i = 1;
  }

  u3_noun lan = u3_mesa_encode_lane(_mesa_lane_sess(&ses_u));
  c3_d sid_d;
  if (  (c3y != _mesa_decode_session_lane(lan, &sid_d))
     || (42 != sid_d) )
  {
    fprintf(stderr, "mesa: session lane encoded incorrectly\r\n");
    ret_i = 1;
  }
  //  a session lane atom must never decode as a udp lane
  if ( 0 != u3_mesa_decode_lane(u3k(lan)).sin_addr.s_addr ) {
    fprintf(stderr, "mesa: session lane collides with udp lane space\r\n");
    ret_i = 1;
  }
  u3z(lan);

  return ret_i;
}

/* _test_quic4_lane_dispatch(): QUIC hop lanes use address dialing vtable.
*/
static c3_i
_test_quic4_lane_dispatch(void)
{
  u3_mesa sam_u;
  memset(&sam_u, 0, sizeof(sam_u));

  _test_sess_send_state sat_u = {0};
  sam_u.qit_u = (u3_mesa_tran){
    .sen_f = _test_sess_send_cb,
    .adr_f = _test_addr_send_cb,
    .bak_v = &sat_u,
  };

  sockaddr_in qic_u = {0};
  qic_u.sin_family = AF_INET;
  qic_u.sin_addr.s_addr = htonl(0xc0000201);
  qic_u.sin_port = htons(8443);

  u3_pit_addr adr_u = {
    .lan_u = _mesa_lane_quic4(qic_u),
    .nex_p = NULL,
  };
  c3_y buf_y[3] = { 0xab, 0xcd, 0xef };

  _mesa_send_bufs(&sam_u, NULL, buf_y, sizeof(buf_y), &adr_u);

  c3_i ret_i = 0;
  if ( 0 != sat_u.hit_w ) {
    fprintf(stderr, "mesa: QUIC v4 lane used session dispatch\r\n");
    ret_i = 1;
  }
  if ( 1 != sat_u.adr_hit_w ) {
    fprintf(stderr, "mesa: QUIC v4 lane did not dispatch once\r\n");
    ret_i = 1;
  }
  if (  (AF_INET != sat_u.adr_u.sin_family)
     || (qic_u.sin_addr.s_addr != sat_u.adr_u.sin_addr.s_addr)
     || (qic_u.sin_port != sat_u.adr_u.sin_port) )
  {
    fprintf(stderr, "mesa: QUIC v4 lane dispatched wrong endpoint\r\n");
    ret_i = 1;
  }
  if ( (sizeof(buf_y) != sat_u.adr_len_w) || (0xab != sat_u.adr_one_y) ) {
    fprintf(stderr, "mesa: QUIC v4 lane dispatched wrong packet\r\n");
    ret_i = 1;
  }
  if ( c3y != _mesa_pit_lanes_equal(_mesa_lane_quic4(qic_u),
                                    _mesa_lane_quic4(qic_u)) )
  {
    fprintf(stderr, "mesa: QUIC v4 lane did not compare equal\r\n");
    ret_i = 1;
  }

  qic_u.sin_port = htons(8444);
  if ( c3n != _mesa_pit_lanes_equal(adr_u.lan_u, _mesa_lane_quic4(qic_u)) ) {
    fprintf(stderr, "mesa: QUIC v4 lane ignored port in comparison\r\n");
    ret_i = 1;
  }

  return ret_i;
}

/* _test_session_rejects_legacy_packet(): QUIC sessions carry Mesa only.
*/
static c3_i
_test_session_rejects_legacy_packet(void)
{
  u3_mesa sam_u;
  memset(&sam_u, 0, sizeof(sam_u));

  u3_sess ses_u = {
    .sid_d = 42,
    .bak_p = &ses_u,
  };

  c3_y buf_y[4] = { 0, 0, 0, 0 };

  _mesa_hear_lane(&sam_u, _mesa_lane_sess(&ses_u), sizeof(buf_y), buf_y);
  return 0;
}

/* _test_session_bound_modal_send(): peer sends prefer a verified session.
*/
static c3_i
_test_session_bound_modal_send(void)
{
  u3_mesa sam_u;
  u3_pier pir_u;
  u3_peer per_u;
  memset(&sam_u, 0, sizeof(sam_u));
  memset(&pir_u, 0, sizeof(pir_u));
  memset(&per_u, 0, sizeof(per_u));

  _test_sess_send_state sat_u = {0};
  sam_u.pir_u = &pir_u;
  sam_u.sab_u = u3_sess_tab_init();
  sam_u.qit_u = (u3_mesa_tran){
    .sen_f = _test_sess_send_cb,
    .bak_v = &sat_u,
  };

  per_u.sam_u = &sam_u;
  per_u.her_u[0] = 0x1234;
  per_u.her_u[1] = 0x1;
  per_u.imp_y = 0;
  per_u.dan_u.sin_family = AF_INET;
  per_u.dan_u.sin_addr.s_addr = htonl(0x7f000001);
  per_u.dan_u.sin_port = htons(31337);

  u3_sess* ses_u = u3_sess_open(sam_u.sab_u, (void*)0xfeed);
  c3_i ret_i = 0;
  if ( c3y != u3_sess_bind(sam_u.sab_u,
                           per_u.her_u,
                           _test_sess_fresh(0, 1, 1),
                           ses_u) )
  {
    fprintf(stderr, "mesa: modal session bind rejected\r\n");
    ret_i = 1;
  }

  c3_y buf_y[3] = { 0xaa, 0xbb, 0xcc };
  _mesa_send_modal(&per_u, uv_buf_init((c3_c*)buf_y, sizeof(buf_y)), NULL);

  if ( 1 != sat_u.hit_w ) {
    fprintf(stderr, "mesa: bound modal send did not use session\r\n");
    ret_i = 1;
  }
  if ( ses_u != sat_u.ses_u ) {
    fprintf(stderr, "mesa: bound modal send used wrong session\r\n");
    ret_i = 1;
  }
  if ( (sizeof(buf_y) != sat_u.len_w) || (0xaa != sat_u.one_y) ) {
    fprintf(stderr, "mesa: bound modal send sent wrong packet\r\n");
    ret_i = 1;
  }

  u3_sess_tab_free(sam_u.sab_u);
  return ret_i;
}

/* _test_session_hear_without_udp_not_direct(): session hears do not imply UDP.
*/
static c3_i
_test_session_hear_without_udp_not_direct(void)
{
  u3_mesa sam_u;
  u3_peer per_u;
  memset(&sam_u, 0, sizeof(sam_u));
  memset(&per_u, 0, sizeof(per_u));

  _init_peer(&sam_u, &per_u);
  per_u.dir_u.her_d = _get_now_micros();

  c3_i ret_i = 0;
  if ( c3n != _mesa_is_direct_mode(&per_u) ) {
    fprintf(stderr, "mesa: fresh zero UDP lane selected as direct\r\n");
    ret_i = 1;
  }

  per_u.dan_u.sin_family = AF_INET;
  per_u.dan_u.sin_addr.s_addr = htonl(0x7f000001);
  per_u.dan_u.sin_port = htons(31337);

  if ( c3y != _mesa_is_direct_mode(&per_u) ) {
    fprintf(stderr, "mesa: valid fresh UDP lane not selected as direct\r\n");
    ret_i = 1;
  }

  return ret_i;
}

/* _test_session_bound_forward_request(): sponsor relay prefers sessions.
*/
static c3_i
_test_session_bound_forward_request(void)
{
  u3_mesa sam_u;
  u3_pier pir_u;
  u3_peer per_u;
  memset(&sam_u, 0, sizeof(sam_u));
  memset(&pir_u, 0, sizeof(pir_u));
  memset(&per_u, 0, sizeof(per_u));

  vt_init(&sam_u.per_u);
  vt_init(&sam_u.pit_u);

  _test_sess_send_state sat_u = {0};
  sam_u.pir_u = &pir_u;
  sam_u.sab_u = u3_sess_tab_init();
  sam_u.for_o = c3y;
  sam_u.qit_u = (u3_mesa_tran){
    .sen_f = _test_sess_send_cb,
    .bak_v = &sat_u,
  };

  per_u.sam_u = &sam_u;
  per_u.ful_o = c3y;
  per_u.imp_y = 0;
  per_u.her_u[0] = 0x1234;
  per_u.her_u[1] = 0x1;
  _mesa_put_peer(&sam_u, per_u.her_u, &per_u);

  u3_sess* ses_u = u3_sess_open(sam_u.sab_u, (void*)0xbeef);
  c3_i ret_i = 0;
  if ( c3y != u3_sess_bind(sam_u.sab_u,
                           per_u.her_u,
                           _test_sess_fresh(0, 1, 1),
                           ses_u) )
  {
    fprintf(stderr, "mesa: forward session bind rejected\r\n");
    ret_i = 1;
  }

  c3_c* pax_c = "/forward";
  u3_mesa_pict pic_u;
  memset(&pic_u, 0, sizeof(pic_u));
  pic_u.sam_u = &sam_u;
  pic_u.pac_u.hed_u.typ_y = PACT_PEEK;
  pic_u.pac_u.hed_u.pro_y = MESA_VER;
  pic_u.pac_u.hed_u.nex_y = HOP_NONE;
  pic_u.pac_u.pek_u.nam_u.her_u[0] = per_u.her_u[0];
  pic_u.pac_u.pek_u.nam_u.her_u[1] = per_u.her_u[1];
  pic_u.pac_u.pek_u.nam_u.boq_y = 13;
  pic_u.pac_u.pek_u.nam_u.nit_o = c3n;
  pic_u.pac_u.pek_u.nam_u.aut_o = c3n;
  pic_u.pac_u.pek_u.nam_u.pat_c = pax_c;
  pic_u.pac_u.pek_u.nam_u.pat_s = strlen(pax_c);
  pic_u.pac_u.pek_u.nam_u.str_u.str_c = pax_c;
  pic_u.pac_u.pek_u.nam_u.str_u.len_w = strlen(pax_c);

  sockaddr_in req_u;
  memset(&req_u, 0, sizeof(req_u));
  req_u.sin_family = AF_INET;
  req_u.sin_addr.s_addr = htonl(0x7f000001);
  req_u.sin_port = htons(31338);

  _mesa_forward_request(&sam_u, &pic_u, _mesa_lane_udp4(req_u));

  if ( 1 != sat_u.hit_w ) {
    fprintf(stderr, "mesa: bound forward did not use session\r\n");
    ret_i = 1;
  }
  if ( ses_u != sat_u.ses_u ) {
    fprintf(stderr, "mesa: bound forward used wrong session\r\n");
    ret_i = 1;
  }
  if ( 0 == sat_u.len_w ) {
    fprintf(stderr, "mesa: bound forward sent empty packet\r\n");
    ret_i = 1;
  }

  u3_pit_entry* ent_u = _mesa_get_pit(&sam_u, &pic_u.pac_u.pek_u.nam_u);
  if (  (NULL == ent_u)
     || (NULL == ent_u->adr_u)
     || (U3_MESA_LANE_UDP4 != ent_u->adr_u->lan_u.kin_e)
     || (c3y != _mesa_lanes_equal(req_u, ent_u->adr_u->lan_u.adr4_u)) )
  {
    fprintf(stderr, "mesa: bound forward did not record requester lane\r\n");
    ret_i = 1;
  }

  u3_sess_tab_free(sam_u.sab_u);
  vt_cleanup(&sam_u.pit_u);
  vt_cleanup(&sam_u.per_u);
  return ret_i;
}

/* _test_bind_gift_forward_request(): %bind drives relay session egress.
*/
static c3_i
_test_bind_gift_forward_request(void)
{
  u3_mesa sam_u;
  u3_pier pir_u;
  u3_peer per_u;
  memset(&sam_u, 0, sizeof(sam_u));
  memset(&pir_u, 0, sizeof(pir_u));
  memset(&per_u, 0, sizeof(per_u));

  vt_init(&sam_u.per_u);
  vt_init(&sam_u.pit_u);

  _test_sess_send_state sat_u = {0};
  sam_u.pir_u = &pir_u;
  sam_u.sab_u = u3_sess_tab_init();
  sam_u.for_o = c3y;
  sam_u.qit_u = (u3_mesa_tran){
    .sen_f = _test_sess_send_cb,
    .bak_v = &sat_u,
  };

  per_u.sam_u = &sam_u;
  per_u.ful_o = c3y;
  per_u.imp_y = 0;
  per_u.her_u[0] = 0x1234;
  per_u.her_u[1] = 0x1;
  _mesa_put_peer(&sam_u, per_u.her_u, &per_u);

  u3_sess* ses_u = u3_sess_open(sam_u.sab_u, (void*)0xcafe);
  c3_i ret_i = 0;
  if ( c3y != _mesa_kick(&sam_u,
                         c3__bind,
                         _test_bind_dat(per_u.her_u, 0, 1, 1,
                                        u3i_chub(_mesa_encode_session_lane(ses_u->sid_d)))) )
  {
    fprintf(stderr, "mesa: forward bind gift rejected\r\n");
    ret_i = 1;
  }
  if ( ses_u != u3_sess_find(sam_u.sab_u, per_u.her_u) ) {
    fprintf(stderr, "mesa: forward bind gift did not bind session\r\n");
    ret_i = 1;
  }

  c3_c* pax_c = "/bind-forward";
  u3_mesa_pict pic_u;
  memset(&pic_u, 0, sizeof(pic_u));
  pic_u.sam_u = &sam_u;
  pic_u.pac_u.hed_u.typ_y = PACT_PEEK;
  pic_u.pac_u.hed_u.pro_y = MESA_VER;
  pic_u.pac_u.hed_u.nex_y = HOP_NONE;
  pic_u.pac_u.pek_u.nam_u.her_u[0] = per_u.her_u[0];
  pic_u.pac_u.pek_u.nam_u.her_u[1] = per_u.her_u[1];
  pic_u.pac_u.pek_u.nam_u.boq_y = 13;
  pic_u.pac_u.pek_u.nam_u.nit_o = c3n;
  pic_u.pac_u.pek_u.nam_u.aut_o = c3n;
  pic_u.pac_u.pek_u.nam_u.pat_c = pax_c;
  pic_u.pac_u.pek_u.nam_u.pat_s = strlen(pax_c);
  pic_u.pac_u.pek_u.nam_u.str_u.str_c = pax_c;
  pic_u.pac_u.pek_u.nam_u.str_u.len_w = strlen(pax_c);

  sockaddr_in req_u;
  memset(&req_u, 0, sizeof(req_u));
  req_u.sin_family = AF_INET;
  req_u.sin_addr.s_addr = htonl(0x7f000001);
  req_u.sin_port = htons(31339);

  _mesa_forward_request(&sam_u, &pic_u, _mesa_lane_udp4(req_u));

  if ( 1 != sat_u.hit_w ) {
    fprintf(stderr, "mesa: bind-driven forward did not use session\r\n");
    ret_i = 1;
  }
  if ( ses_u != sat_u.ses_u ) {
    fprintf(stderr, "mesa: bind-driven forward used wrong session\r\n");
    ret_i = 1;
  }
  if ( 0 == sat_u.len_w ) {
    fprintf(stderr, "mesa: bind-driven forward sent empty packet\r\n");
    ret_i = 1;
  }

  u3_sess_tab_free(sam_u.sab_u);
  vt_cleanup(&sam_u.pit_u);
  vt_cleanup(&sam_u.per_u);
  return ret_i;
}

/* _test_quic_hop_decoding(): parse advertised QUIC v4 HOP_LONG lanes.
*/
static c3_i
_test_quic_hop_decoding(void)
{
  c3_i ret_i = 0;
  c3_y dat_y[MESA_HOP_LONG_QUIC_UDP4_SIZE] = {0};
  sockaddr_in adr_u;

  dat_y[0] = MESA_HOP_LONG_QUIC_UDP4;
  c3_etch_word(dat_y + 1, 0xc0000201);
  c3_etch_short(dat_y + 5, 8443);

  u3_mesa_hop_once hop_u = {
    .len_w = sizeof(dat_y),
    .dat_y = dat_y,
  };

  if ( c3y != _mesa_decode_quic_udp4_hop(&hop_u, &adr_u) ) {
    fprintf(stderr, "mesa: QUIC v4 hop did not decode\r\n");
    ret_i = 1;
  }
  else if (  (AF_INET != adr_u.sin_family)
          || (0xc0000201 != ntohl(adr_u.sin_addr.s_addr))
          || (8443 != ntohs(adr_u.sin_port)) )
  {
    fprintf(stderr, "mesa: QUIC v4 hop decoded wrong address\r\n");
    ret_i = 1;
  }

  dat_y[0] = 0xff;
  if ( c3n != _mesa_decode_quic_udp4_hop(&hop_u, &adr_u) ) {
    fprintf(stderr, "mesa: unknown HOP_LONG tag accepted\r\n");
    ret_i = 1;
  }

  dat_y[0] = MESA_HOP_LONG_QUIC_UDP4;
  hop_u.len_w--;
  if ( c3n != _mesa_decode_quic_udp4_hop(&hop_u, &adr_u) ) {
    fprintf(stderr, "mesa: short QUIC v4 hop accepted\r\n");
    ret_i = 1;
  }

  hop_u.len_w = sizeof(dat_y);
  c3_etch_short(dat_y + 5, 0);
  if ( c3n != _mesa_decode_quic_udp4_hop(&hop_u, &adr_u) ) {
    fprintf(stderr, "mesa: zero-port QUIC v4 hop accepted\r\n");
    ret_i = 1;
  }

  return ret_i;
}

/* _test_quic_sponsor_lane_policy(): sponsor atoms stay UDP unless requested.
*/
static c3_i
_test_quic_sponsor_lane_policy(void)
{
  u3_mesa sam_u;
  arena   are_u = arena_create(1024);
  memset(&sam_u, 0, sizeof(sam_u));

  c3_o net_o = u3_Host.ops_u.net;
  c3_o qsp_o = u3_Host.ops_u.qsp;
  c3_s qsp_s = u3_Host.ops_u.qsp_s;
  u3_Host.ops_u.net = c3n;
  u3_Host.ops_u.qsp = c3n;
  u3_Host.ops_u.qsp_s = 0;

  c3_i ret_i = 0;

  {
    u3_noun las = u3i_list(u3i_word(0), u3_none);
    u3_pit_addr* adr_u = _mesa_lanes_to_addrs(&sam_u, las, &are_u);

    if (  (NULL == adr_u)
       || (U3_MESA_LANE_UDP4 != adr_u->lan_u.kin_e)
       || (0x7f000001 != ntohl(adr_u->lan_u.adr4_u.sin_addr.s_addr))
       || (31337 != ntohs(adr_u->lan_u.adr4_u.sin_port)) )
    {
      fprintf(stderr, "mesa: default sponsor lane did not resolve to UDP\r\n");
      ret_i = 1;
    }
    u3z(las);
  }

  arena_free(&are_u);
  are_u = arena_create(1024);
  u3_Host.ops_u.qsp = c3y;
  u3_Host.ops_u.qsp_s = 9443;

  {
    u3_noun las = u3i_list(u3i_word(0), u3_none);
    u3_pit_addr* adr_u = _mesa_lanes_to_addrs(&sam_u, las, &are_u);

    if (  (NULL == adr_u)
       || (U3_MESA_LANE_QUIC4 != adr_u->lan_u.kin_e)
       || (0x7f000001 != ntohl(adr_u->lan_u.qic4_u.sin_addr.s_addr))
       || (9443 != ntohs(adr_u->lan_u.qic4_u.sin_port)) )
    {
      fprintf(stderr, "mesa: QUIC sponsor lane did not resolve to QUIC\r\n");
      ret_i = 1;
    }
    u3z(las);
  }

  u3_Host.ops_u.net = net_o;
  u3_Host.ops_u.qsp = qsp_o;
  u3_Host.ops_u.qsp_s = qsp_s;
  arena_free(&are_u);
  return ret_i;
}

/* _test_origin_hop_stamping(): relayed pages advertise the right source lane.
*/
static c3_i
_test_origin_hop_stamping(void)
{
  u3_mesa sam_u;
  u3_pier pir_u;
  arena   are_u = arena_create(1024);
  memset(&sam_u, 0, sizeof(sam_u));
  memset(&pir_u, 0, sizeof(pir_u));

  sam_u.are_u = are_u;
  sam_u.pir_u = &pir_u;
  pir_u.por_s = 31337;
  pir_u.poq_s = 8443;

  c3_o net_o = u3_Host.ops_u.net;
  u3_Host.ops_u.net = c3n;

  c3_i ret_i = 0;

  {
    u3_mesa_head hed_u = {0};
    u3_mesa_page_pact pag_u = {0};
    u3_sess ses_u = {
      .sid_d = 44,
      .bak_p = &ses_u,
    };

    _mesa_add_hop(&sam_u, 0, &hed_u, &pag_u, _mesa_lane_sess(&ses_u));

    if ( HOP_LONG != hed_u.nex_y ) {
      fprintf(stderr, "mesa: session origin did not stamp HOP_LONG\r\n");
      ret_i = 1;
    }
    if ( MESA_HOP_LONG_QUIC_UDP4_SIZE != pag_u.one_u.len_w ) {
      fprintf(stderr, "mesa: QUIC hop payload has wrong length\r\n");
      ret_i = 1;
    }
    else if (  (MESA_HOP_LONG_QUIC_UDP4 != pag_u.one_u.dat_y[0])
            || (0x7f000001 != c3_sift_word(pag_u.one_u.dat_y + 1))
            || (8443 != c3_sift_short(pag_u.one_u.dat_y + 5)) )
    {
      fprintf(stderr, "mesa: QUIC hop payload encoded wrong lane\r\n");
      ret_i = 1;
    }
  }

  {
    u3_mesa_head hed_u = {0};
    u3_mesa_page_pact pag_u = {0};
    sockaddr_in adr_u = {0};
    adr_u.sin_family = AF_INET;
    adr_u.sin_addr.s_addr = htonl(0x7f000001);
    adr_u.sin_port = htons(31338);

    _mesa_add_hop(&sam_u, 0, &hed_u, &pag_u, _mesa_lane_udp4(adr_u));

    if ( HOP_SHORT != hed_u.nex_y ) {
      fprintf(stderr, "mesa: UDP origin did not stamp HOP_SHORT\r\n");
      ret_i = 1;
    }
    if (  (0x7f000001 != c3_sift_word(pag_u.sot_u))
       || (31338 != c3_sift_short(pag_u.sot_u + 4)) )
    {
      fprintf(stderr, "mesa: UDP hop payload encoded wrong lane\r\n");
      ret_i = 1;
    }
  }

  u3_Host.ops_u.net = net_o;
  arena_free(&are_u);
  return ret_i;
}

/* _test_session_close_drops_pit_lane(): closing session clears PIT pointers.
*/
static c3_i
_test_session_close_drops_pit_lane(void)
{
  u3_mesa sam_u;
  memset(&sam_u, 0, sizeof(sam_u));
  vt_init(&sam_u.pit_u);
  sam_u.sab_u = u3_sess_tab_init();

  u3_sess* ses_u = u3_sess_open(sam_u.sab_u, (void*)0x1234);

  c3_c* pax_c = "/pit";
  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.str_u.str_c = pax_c;
  nam_u.str_u.len_w = strlen(pax_c);

  _mesa_add_lane_to_pit(&sam_u, &nam_u, _mesa_lane_sess(ses_u));

  u3_pit_entry* ent_u = _mesa_get_pit(&sam_u, &nam_u);
  c3_i ret_i = 0;
  if ( (NULL == ent_u) || (NULL == ent_u->adr_u) ) {
    fprintf(stderr, "mesa: session PIT lane was not inserted\r\n");
    ret_i = 1;
  }

  u3_mesa_sess_close(&sam_u, ses_u);

  if ( (NULL != ent_u) && (NULL != ent_u->adr_u) ) {
    fprintf(stderr, "mesa: closed session survived in PIT\r\n");
    ret_i = 1;
  }

  u3_sess_tab_free(sam_u.sab_u);
  vt_cleanup(&sam_u.pit_u);
  return ret_i;
}

/* _test_session_lane_next_fragments(): follow-up PEEKs stay on session lane.
*/
static c3_i
_test_session_lane_next_fragments(void)
{
  u3_mesa sam_u;
  memset(&sam_u, 0, sizeof(sam_u));

  _test_sess_send_state sat_u = {0};
  sam_u.qit_u = (u3_mesa_tran){
    .sen_f = _test_sess_send_cb,
    .bak_v = &sat_u,
  };

  u3_peer per_u;
  memset(&per_u, 0, sizeof(per_u));
  per_u.sam_u = &sam_u;

  u3_gage gag_u;
  _init_gage(&gag_u);

  u3_pact_stat wat_u[2];
  memset(wat_u, 0, sizeof(wat_u));

  c3_c* pax_c = "/frag";

  u3_mesa_pict pic_u;
  memset(&pic_u, 0, sizeof(pic_u));
  pic_u.sam_u = &sam_u;
  pic_u.pac_u.hed_u.typ_y = PACT_PEEK;
  pic_u.pac_u.hed_u.pro_y = MESA_VER;
  pic_u.pac_u.hed_u.nex_y = HOP_NONE;
  pic_u.pac_u.pek_u.nam_u.her_u[0] = 0x42;
  pic_u.pac_u.pek_u.nam_u.her_u[1] = 0;
  pic_u.pac_u.pek_u.nam_u.boq_y = 13;
  pic_u.pac_u.pek_u.nam_u.nit_o = c3n;
  pic_u.pac_u.pek_u.nam_u.aut_o = c3n;
  pic_u.pac_u.pek_u.nam_u.pat_c = pax_c;
  pic_u.pac_u.pek_u.nam_u.pat_s = strlen(pax_c);

  c3_w pek_w = mesa_size_pact(&pic_u.pac_u);
  c3_c pek_c[PACT_SIZE];
  memset(pek_c, 0, sizeof(pek_c));

  u3_pend_req req_u;
  memset(&req_u, 0, sizeof(req_u));
  req_u.per_u = &per_u;
  req_u.pic_u = &pic_u;
  req_u.gag_u = &gag_u;
  req_u.wat_u = wat_u;
  req_u.tof_d = 1;
  req_u.pek_w = pek_w;
  req_u.pek_c = pek_c;

  u3_sess ses_u = {
    .sid_d = 43,
    .bak_p = &ses_u,
  };

  _mesa_request_next_fragments(&sam_u, &req_u, _mesa_lane_sess(&ses_u));

  c3_i ret_i = 0;
  if ( 1 != sat_u.hit_w ) {
    fprintf(stderr, "mesa: session follow-up PEEK did not dispatch\r\n");
    ret_i = 1;
  }
  if ( &ses_u != sat_u.ses_u ) {
    fprintf(stderr, "mesa: session follow-up used wrong session\r\n");
    ret_i = 1;
  }
  if ( pek_w != sat_u.len_w ) {
    fprintf(stderr, "mesa: session follow-up sent wrong length\r\n");
    ret_i = 1;
  }
  if ( (1 != req_u.nex_d) || (1 != req_u.out_d) ) {
    fprintf(stderr, "mesa: session follow-up did not mark request sent\r\n");
    ret_i = 1;
  }

  return ret_i;
}

/* _test_quic_send_rule(): RFC section 4.1 packet framing classification.
*/
static c3_i
_test_quic_send_rule(void)
{
  c3_i ret_i = 0;

  if ( U3_MESA_QUIC_SEND_DATAGRAM !=
       u3_mesa_quic_classify_send(1024, 1200) ) {
    fprintf(stderr, "quic: small packet did not use DATAGRAM\r\n");
    ret_i = 1;
  }

  if ( U3_MESA_QUIC_SEND_DATAGRAM !=
       u3_mesa_quic_classify_send(1200, 1200) ) {
    fprintf(stderr, "quic: exact-fit packet did not use DATAGRAM\r\n");
    ret_i = 1;
  }

  if ( U3_MESA_QUIC_SEND_STREAM !=
       u3_mesa_quic_classify_send(1201, 1200) ) {
    fprintf(stderr, "quic: oversize packet did not use stream fallback\r\n");
    ret_i = 1;
  }

  if ( U3_MESA_QUIC_SEND_STREAM !=
       u3_mesa_quic_classify_send(1, 0) ) {
    fprintf(stderr, "quic: disabled datagrams did not use stream fallback\r\n");
    ret_i = 1;
  }

  if ( 0 != strcmp("datagram", u3_mesa_quic_send_kind_string(
                                U3_MESA_QUIC_SEND_DATAGRAM)) ) {
    fprintf(stderr, "quic: DATAGRAM kind string changed\r\n");
    ret_i = 1;
  }

  if ( 0 != strcmp("stream", u3_mesa_quic_send_kind_string(
                              U3_MESA_QUIC_SEND_STREAM)) ) {
    fprintf(stderr, "quic: stream kind string changed\r\n");
    ret_i = 1;
  }

  return ret_i;
}

typedef struct _test_quic_io_state {
  u3_sess_tab* tab_u;
  c3_w         open_w;
  c3_w         close_w;
  c3_w         pkt_w;
  c3_w         len_w;
  c3_y         one_y;
} _test_quic_io_state;

static u3_sess*
_test_quic_io_open_cb(void* ptr_v, void* bak_v)
{
  _test_quic_io_state* sat_u = ptr_v;

  sat_u->open_w++;
  return u3_sess_open(sat_u->tab_u, bak_v);
}

static void
_test_quic_io_close_cb(void* ptr_v, u3_sess* ses_u)
{
  _test_quic_io_state* sat_u = ptr_v;

  sat_u->close_w++;
  u3_sess_close(sat_u->tab_u, ses_u, NULL, NULL);
}

static void
_test_quic_io_pkt_cb(void* ptr_v, u3_sess* ses_u, c3_y* buf_y, c3_w len_w)
{
  _test_quic_io_state* sat_u = ptr_v;
  (void)ses_u;

  sat_u->pkt_w++;
  sat_u->len_w = len_w;
  sat_u->one_y = buf_y[0];
  c3_free(buf_y);
}

/* _test_quic_backend_dial_send(): address dispatch opens QUIC and delivers.
*/
static c3_i
_test_quic_backend_dial_send(void)
{
  uv_loop_t             lup_u;
  u3_mesa_quic*        cli_u = NULL;
  u3_mesa_quic*        srv_u = NULL;
  u3_mesa_tran         cli_tra_u = {0};
  u3_mesa_tran         srv_tra_u = {0};
  _test_quic_io_state  cli_sat_u = {0};
  _test_quic_io_state  srv_sat_u = {0};
  c3_i                 ret_i = 0;
  c3_i                 err_i;

  cli_sat_u.tab_u = u3_sess_tab_init();
  srv_sat_u.tab_u = u3_sess_tab_init();

  err_i = uv_loop_init(&lup_u);
  if ( 0 != err_i ) {
    fprintf(stderr, "quic: uv_loop_init: %s\r\n", uv_strerror(err_i));
    ret_i = 1;
    goto done;
  }

  u3_mesa_quic_config srv_cfg_u = {
    .lup_u = &lup_u,
    .por_s = 0,
    .net_o = c3n,
    .cb_u = {
      .ptr_v = &srv_sat_u,
      .ses_open_f = _test_quic_io_open_cb,
      .ses_close_f = _test_quic_io_close_cb,
      .pkt_f = _test_quic_io_pkt_cb,
    },
  };
  u3_mesa_quic_config cli_cfg_u = {
    .lup_u = &lup_u,
    .por_s = 0,
    .net_o = c3n,
    .cb_u = {
      .ptr_v = &cli_sat_u,
      .ses_open_f = _test_quic_io_open_cb,
      .ses_close_f = _test_quic_io_close_cb,
      .pkt_f = _test_quic_io_pkt_cb,
    },
  };

  if ( c3y != u3_mesa_quic_open(&srv_u, &srv_tra_u, &srv_cfg_u) ) {
    fprintf(stderr, "quic: backend server open failed\r\n");
    ret_i = 1;
    goto close_loop;
  }
  if ( c3y != u3_mesa_quic_open(&cli_u, &cli_tra_u, &cli_cfg_u) ) {
    fprintf(stderr, "quic: backend client open failed\r\n");
    ret_i = 1;
    goto close_loop;
  }

  if ( NULL == cli_tra_u.adr_f ) {
    fprintf(stderr, "quic: backend address vtable missing\r\n");
    ret_i = 1;
    goto close_loop;
  }

  {
    sockaddr_in dst_u = {0};
    c3_y*       buf_y = c3_malloc(3);

    dst_u.sin_family = AF_INET;
    dst_u.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dst_u.sin_port = htons(u3_mesa_quic_port(srv_u));
    buf_y[0] = 0xa1;
    buf_y[1] = 0xb2;
    buf_y[2] = 0xc3;

    cli_tra_u.adr_f(cli_tra_u.bak_v, (const struct sockaddr*)&dst_u,
                    buf_y, 3);
  }

  for ( c3_w i_w = 0; (i_w < 2000) && (0 == srv_sat_u.pkt_w); ++i_w ) {
    uv_run(&lup_u, UV_RUN_NOWAIT);
    uv_sleep(1);
  }

  if ( 0 == cli_sat_u.open_w ) {
    fprintf(stderr, "quic: dialing backend did not open a client session\r\n");
    ret_i = 1;
  }
  if ( 0 == srv_sat_u.open_w ) {
    fprintf(stderr, "quic: dialing backend did not open a server session\r\n");
    ret_i = 1;
  }
  if ( 1 != srv_sat_u.pkt_w ) {
    fprintf(stderr, "quic: dialed packet was not delivered\r\n");
    ret_i = 1;
  }
  if ( (3 != srv_sat_u.len_w) || (0xa1 != srv_sat_u.one_y) ) {
    fprintf(stderr, "quic: dialed packet payload changed\r\n");
    ret_i = 1;
  }

close_loop:
  u3_mesa_quic_close(cli_u);
  u3_mesa_quic_close(srv_u);
  uv_run(&lup_u, UV_RUN_DEFAULT);

  err_i = uv_loop_close(&lup_u);
  if ( 0 != err_i ) {
    fprintf(stderr, "quic: uv_loop_close: %s\r\n", uv_strerror(err_i));
    ret_i = 1;
  }

done:
  u3_sess_tab_free(cli_sat_u.tab_u);
  u3_sess_tab_free(srv_sat_u.tab_u);
  return ret_i;
}

/* _test_quic_open_close(): native backend lifecycle smoke test.
*/
static c3_i
_test_quic_open_close(void)
{
  uv_loop_t       lup_u;
  u3_mesa_quic*  qic_u = NULL;
  u3_mesa_tran   tra_u = {0};
  c3_i           ret_i = 0;
  c3_i           err_i;

  err_i = uv_loop_init(&lup_u);
  if ( 0 != err_i ) {
    fprintf(stderr, "quic: uv_loop_init: %s\r\n", uv_strerror(err_i));
    return 1;
  }

  u3_mesa_quic_config cfg_u = {
    .lup_u = &lup_u,
    .por_s = 0,
    .net_o = c3n,
    .cb_u = {0},
  };

  if ( c3y != u3_mesa_quic_open(&qic_u, &tra_u, &cfg_u) ) {
    fprintf(stderr, "quic: backend open failed\r\n");
    ret_i = 1;
  }
  else {
    if ( 0 == u3_mesa_quic_port(qic_u) ) {
      fprintf(stderr, "quic: backend did not bind a UDP port\r\n");
      ret_i = 1;
    }
    if (  (NULL == tra_u.sen_f)
       || (NULL == tra_u.adr_f)
       || (tra_u.bak_v != qic_u) )
    {
      fprintf(stderr, "quic: backend send vtable not initialized\r\n");
      ret_i = 1;
    }
  }

  u3_mesa_quic_close(qic_u);
  uv_run(&lup_u, UV_RUN_DEFAULT);

  err_i = uv_loop_close(&lup_u);
  if ( 0 != err_i ) {
    fprintf(stderr, "quic: uv_loop_close: %s\r\n", uv_strerror(err_i));
    ret_i = 1;
  }

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  c3_i ret_i = 0;

  ret_i |= _test_frag_heap_overflow();
  ret_i |= _test_frag_boq_overflow();
  ret_i |= _test_frag_valid_accepted();
  ret_i |= _test_page_init_bad_boq();
  ret_i |= _test_wire_page_init_bad_boq();
  ret_i |= _test_sess_binding();
  ret_i |= _test_session_bind_gift();
  ret_i |= _test_session_lane_dispatch();
  ret_i |= _test_quic4_lane_dispatch();
  ret_i |= _test_session_rejects_legacy_packet();
  ret_i |= _test_session_bound_modal_send();
  ret_i |= _test_session_hear_without_udp_not_direct();
  ret_i |= _test_session_bound_forward_request();
  ret_i |= _test_bind_gift_forward_request();
  ret_i |= _test_quic_hop_decoding();
  ret_i |= _test_quic_sponsor_lane_policy();
  ret_i |= _test_origin_hop_stamping();
  ret_i |= _test_session_close_drops_pit_lane();
  ret_i |= _test_session_lane_next_fragments();
  ret_i |= _test_quic_send_rule();
  ret_i |= _test_quic_backend_dial_send();
  ret_i |= _test_quic_open_close();

  if ( ret_i ) {
    fprintf(stderr, "test mesa: failed\r\n");
    exit(1);
  }

  u3m_grab(u3_none);

  fprintf(stderr, "test mesa: ok\r\n");
  return 0;
}
