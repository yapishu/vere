/// @file
///
/// transport-session table for connection-shaped lanes (quic/webtransport).
///
/// a session is a held, dialed-in connection over which a remote peer is
/// reachable; unlike a udp lane it fails explicitly when the connection
/// closes. this module tracks ship -> session bindings per the
/// ames-over-quic spec: a binding is created only from authenticated Ames
/// metadata with a strictly-increasing freshness tuple, a newer verified
/// binding supersedes an older one, and bindings die with their session.
///
/// portable core: c3 types and verstable only -- no libuv, no nouns --
/// so the same logic hosts under the native quic backend and a wasm/
/// webtransport build. signature verification is the caller's business;
/// by the time u3_sess_bind is called the packet must already be verified.

#ifndef U3_VERE_IO_MESA_SESSION_H
#define U3_VERE_IO_MESA_SESSION_H

#include "c3/c3.h"

  /* u3_sess: one transport session.
  */
    typedef struct _u3_sess {
      c3_d              sid_d;              //  local session id
      void*             bak_p;              //  backend connection handle
      struct _u3_sess*  nex_u;              //  next in table's session list
    } u3_sess;

  /* u3_sess_fresh: authenticated freshness domain for a binding beacon.
  */
    typedef struct _u3_sess_fresh {
      c3_w  rif_w;
      c3_d  bon_d;
      c3_d  seq_d;
    } u3_sess_fresh;

  /* u3_sess_tab: session table (opaque).
  */
    typedef struct _u3_sess_tab u3_sess_tab;

  /* u3_sess_fell_f: callback on unbinding, once per formerly-bound ship.
  */
    typedef void (*u3_sess_fell_f)(void* ptr_v, const c3_d her_d[2]);

  /* u3_sess_tab_init(): create an empty session table.
  */
    u3_sess_tab*
    u3_sess_tab_init(void);

  /* u3_sess_tab_free(): free table, all sessions, and all bindings.
  **
  **   does not invoke fell callbacks; for orderly teardown close each
  **   session first.
  */
    void
    u3_sess_tab_free(u3_sess_tab* tab_u);

  /* u3_sess_open(): create a session for backend handle [bak_p].
  */
    u3_sess*
    u3_sess_open(u3_sess_tab* tab_u, void* bak_p);

  /* u3_sess_bind(): bind [her_d] to [ses_u] on verified metadata.
  **
  **   [fre_u] is selected by the authenticated packet layer. It is not a Mesa
  **   fragment number or raw path text. The binding is created or moved only
  **   if [fre_u] is strictly greater than the last bound value for this ship,
  **   defeating replayed bindings. Returns c3y if the binding was created or
  **   refreshed.
  */
    c3_o
    u3_sess_bind(u3_sess_tab* tab_u,
                 const c3_d   her_d[2],
                 u3_sess_fresh fre_u,
                 u3_sess*     ses_u);

  /* u3_sess_find_sid(): live session with local id [sid_d], or NULL.
  */
    u3_sess*
    u3_sess_find_sid(u3_sess_tab* tab_u, c3_d sid_d);

  /* u3_sess_find(): live session bound to [her_d], or NULL.
  */
    u3_sess*
    u3_sess_find(u3_sess_tab* tab_u, const c3_d her_d[2]);

  /* u3_sess_close(): close [ses_u], dropping its bindings.
  **
  **   invokes [fel_f] with [ptr_v] once per ship that was bound to the
  **   session (so the caller can inject %fell), then frees the session.
  **   [fel_f] may be NULL.
  */
    void
    u3_sess_close(u3_sess_tab*   tab_u,
                  u3_sess*       ses_u,
                  u3_sess_fell_f fel_f,
                  void*          ptr_v);

#endif /* ifndef U3_VERE_IO_MESA_SESSION_H */
