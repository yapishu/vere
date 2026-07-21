/// @file
///
/// Raw-QUIC transport backend boundary for mesa.
///
/// This header is intentionally ngtcp2-free.  mesa.c owns session binding and
/// packet dispatch; the backend owns QUIC connections and packet framing.

#ifndef U3_VERE_IO_MESA_QUIC_H
#define U3_VERE_IO_MESA_QUIC_H

#include "c3/c3.h"
#include "session.h"

typedef struct uv_loop_s uv_loop_t;
struct sockaddr;
typedef struct _u3_mesa_quic u3_mesa_quic;

#ifndef U3_MESA_QUIC_DEFAULT_PORT
#define U3_MESA_QUIC_DEFAULT_PORT 8443
#endif

/* u3_mesa_tran: send side consumed by mesa.c.
**
**   [sen_f] takes ownership of [buf_y] and sends over an existing session.
**   [adr_f] takes ownership of [buf_y] and sends to an advertised raw-QUIC
**   endpoint, opening a client connection if needed.  The backend must free
**   buffers after successful queueing or on drop.
*/
typedef struct _u3_mesa_tran {
  void  (*sen_f)(void* bak_v, u3_sess* ses_u, c3_y* buf_y, c3_w len_w);
  void  (*adr_f)(void* bak_v, const struct sockaddr* adr_u,
                 c3_y* buf_y, c3_w len_w);
  void* bak_v;
} u3_mesa_tran;

/* u3_mesa_quic_cb: callbacks the backend invokes back into mesa.c.
*/
typedef struct _u3_mesa_quic_cb {
  void* ptr_v;

  /* A QUIC session opened.  [bak_v] is the backend's per-connection handle. */
  u3_sess* (*ses_open_f)(void* ptr_v, void* bak_v);

  /* A QUIC session closed. */
  void (*ses_close_f)(void* ptr_v, u3_sess* ses_u);

  /* A mesa packet arrived on [ses_u].  Ownership of [buf_y] is transferred. */
  void (*pkt_f)(void* ptr_v, u3_sess* ses_u, c3_y* buf_y, c3_w len_w);
} u3_mesa_quic_cb;

typedef struct _u3_mesa_quic_config {
  uv_loop_t*        lup_u;              //  event loop for backend handles
  c3_s             por_s;              //  UDP port to bind; 0 = ephemeral
  c3_o             net_o;              //  c3y = public, c3n = loopback only
  u3_mesa_quic_cb  cb_u;
} u3_mesa_quic_config;

typedef enum _u3_mesa_quic_send_kind {
  U3_MESA_QUIC_SEND_DATAGRAM = 0,
  U3_MESA_QUIC_SEND_STREAM   = 1,
} u3_mesa_quic_send_kind;

/* u3_mesa_quic_classify_send(): classify RFC section 4.1 packet framing.
**
**   [max_datagram_payload_w] is the current QUIC DATAGRAM payload capacity,
**   already adjusted for frame overhead by the backend.
*/
u3_mesa_quic_send_kind
u3_mesa_quic_classify_send(c3_w len_w, c3_w max_datagram_payload_w);

const c3_c*
u3_mesa_quic_send_kind_string(u3_mesa_quic_send_kind kin_e);

/* u3_mesa_quic_open(): start the native raw-QUIC backend.
**
**   On success, [*qic_u] owns the backend and [tran_u] is initialized with
**   the mesa send vtable.  Returns c3y on success, c3n on failure.
*/
c3_o
u3_mesa_quic_open(u3_mesa_quic**              qic_u,
                  u3_mesa_tran*              tran_u,
                  const u3_mesa_quic_config* cfg_u);

/* u3_mesa_quic_close(): stop the backend and release its resources.
*/
void
u3_mesa_quic_close(u3_mesa_quic* qic_u);

/* u3_mesa_quic_port(): return the bound UDP port.
*/
c3_s
u3_mesa_quic_port(const u3_mesa_quic* qic_u);

#endif /* U3_VERE_IO_MESA_QUIC_H */
