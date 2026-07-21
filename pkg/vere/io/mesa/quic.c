/// @file

#include "quic.h"
#include "vere.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_picotls.h>
#include <picotls.h>
#include <picotls/openssl.h>
#include <uv.h>

#define U3_MESA_QUIC_CIDLEN              NGTCP2_MIN_INITIAL_DCIDLEN
#define U3_MESA_QUIC_MAX_UDP             1452
#define U3_MESA_QUIC_RECV_BUF            65535
#define U3_MESA_QUIC_DATAGRAM_FRAME_SIZE 1200
#define U3_MESA_QUIC_DGRAM_OVERHEAD      16
#define U3_MESA_QUIC_MAX_STREAM_PACKET   (1024 * 1024)
#define U3_MESA_QUIC_PENDING_MAX         32

static const uint8_t _ames_alpn[] = "ames/1";
static ptls_iovec_t _ames_client_alpn[] = {
  { .base = (uint8_t*)_ames_alpn, .len = sizeof(_ames_alpn) - 1 },
};

typedef struct _u3_mesa_quic_conn u3_mesa_quic_conn;
typedef struct _u3_mesa_quic_stream u3_mesa_quic_stream;
typedef struct _u3_mesa_quic_out u3_mesa_quic_out;
typedef struct _u3_mesa_quic_pkt u3_mesa_quic_pkt;

struct _u3_mesa_quic_stream {
  int64_t                sid_i;
  c3_y*                  buf_y;
  c3_w                   len_w;
  c3_w                   cap_w;
  u3_mesa_quic_stream*   nex_u;
};

struct _u3_mesa_quic_out {
  int64_t             sid_i;
  c3_y*               buf_y;
  c3_w                len_w;
  c3_w                off_w;
  u3_mesa_quic_out*   nex_u;
};

struct _u3_mesa_quic_pkt {
  c3_y*              buf_y;
  c3_w               len_w;
  u3_mesa_quic_pkt*  nex_u;
};

struct _u3_mesa_quic_conn {
  u3_mesa_quic*              qic_u;
  u3_sess*                   ses_u;
  ngtcp2_conn*               con_u;
  ngtcp2_crypto_conn_ref     ref_u;
  ngtcp2_crypto_picotls_ctx  tls_u;
  ptls_raw_extension_t       ext_u[2];

  ngtcp2_cid                 scid_u;
  ngtcp2_cid                 odcid_u;
  ngtcp2_addr                loc_u;
  ngtcp2_addr                rem_u;
  struct sockaddr_storage    loc_stu;
  struct sockaddr_storage    rem_stu;

  u3_mesa_quic_stream*       str_u;
  u3_mesa_quic_out*          out_u;
  u3_mesa_quic_pkt*          pen_u;
  uint64_t                   dgr_d;
  ngtcp2_tstamp              tms_d;
  c3_w                       pen_w;
  c3_o                       han_o;

  u3_mesa_quic_conn*         nex_u;
};

typedef struct _u3_mesa_quic_send {
  uv_udp_send_t        req_u;
  c3_y*                buf_y;
  c3_w                 len_w;
  u3_mesa_quic_conn*   con_u;
} u3_mesa_quic_send;

struct _u3_mesa_quic {
  uv_udp_t                         udp_u;
  uv_timer_t                       tim_u;
  c3_o                             udp_o;
  c3_o                             tim_o;
  c3_o                             clo_o;
  c3_w                             clo_w;

  c3_s                             por_s;
  c3_y                             rst_y[32];
  u3_mesa_quic_cb                  cb_u;

  ptls_context_t                   tls_u;
  ptls_context_t                   ctl_u;
  ptls_openssl_sign_certificate_t  sig_u;

  u3_mesa_quic_conn*               con_u;
};

static void _quic_flush_conn(u3_mesa_quic_conn* qoc_u);
static void _quic_flush_pending(u3_mesa_quic_conn* qoc_u);
static c3_w _quic_max_datagram_payload(u3_mesa_quic_conn* qoc_u);
static void _quic_send_datagram(u3_mesa_quic_conn* qoc_u,
                                c3_y*              buf_y,
                                c3_w               len_w);
static void _quic_send_conn(u3_mesa_quic_conn* qoc_u,
                            c3_y*              buf_y,
                            c3_w               len_w);
static void _quic_schedule(u3_mesa_quic* qic_u);

u3_mesa_quic_send_kind
u3_mesa_quic_classify_send(c3_w len_w, c3_w max_datagram_payload_w)
{
  if ( (0 != max_datagram_payload_w) && (len_w <= max_datagram_payload_w) ) {
    return U3_MESA_QUIC_SEND_DATAGRAM;
  }

  return U3_MESA_QUIC_SEND_STREAM;
}

const c3_c*
u3_mesa_quic_send_kind_string(u3_mesa_quic_send_kind kin_e)
{
  switch ( kin_e ) {
    case U3_MESA_QUIC_SEND_DATAGRAM: return "datagram";
    case U3_MESA_QUIC_SEND_STREAM:   return "stream";
    default:                         return "unknown";
  }
}

static ngtcp2_tstamp
_quic_now(void)
{
  struct timespec tim_u;

  if ( 0 != clock_gettime(CLOCK_MONOTONIC, &tim_u) ) {
    return 0;
  }

  return (((uint64_t)tim_u.tv_sec) * NGTCP2_SECONDS) +
         ((uint64_t)tim_u.tv_nsec);
}

static ngtcp2_tstamp
_quic_conn_now(u3_mesa_quic_conn* qoc_u)
{
  ngtcp2_tstamp now_d = _quic_now();

  if ( now_d < qoc_u->tms_d ) {
    now_d = qoc_u->tms_d;
  }
  qoc_u->tms_d = now_d;
  return now_d;
}

static void
_quic_log_ngtcp2(const c3_c* cap_c, c3_i err_i)
{
  fprintf(stderr, "mesa: quic: %s: %s\n", cap_c, ngtcp2_strerror(err_i));
}

static void
_quic_log_addr(const c3_c* cap_c, const struct sockaddr* adr_u)
{
  c3_c  ip_c[INET6_ADDRSTRLEN];
  c3_s  por_s = 0;
  const void* src_v = NULL;

  switch ( adr_u->sa_family ) {
    case AF_INET: {
      const struct sockaddr_in* in_u = (const struct sockaddr_in*)adr_u;
      src_v = &in_u->sin_addr;
      por_s = ntohs(in_u->sin_port);
    } break;

    case AF_INET6: {
      const struct sockaddr_in6* in_u = (const struct sockaddr_in6*)adr_u;
      src_v = &in_u->sin6_addr;
      por_s = ntohs(in_u->sin6_port);
    } break;

    default: {
      fprintf(stderr, "mesa: quic: %s: unknown address family %d\n",
              cap_c, adr_u->sa_family);
      return;
    }
  }

  if ( NULL == inet_ntop(adr_u->sa_family, src_v, ip_c, sizeof(ip_c)) ) {
    fprintf(stderr, "mesa: quic: %s: inet_ntop: %s\n",
            cap_c, strerror(errno));
    return;
  }

  fprintf(stderr, "mesa: quic: %s: %s:%u\n", cap_c, ip_c, por_s);
}

static ngtcp2_conn*
_quic_get_conn_from_ref(ngtcp2_crypto_conn_ref* ref_u)
{
  u3_mesa_quic_conn* qoc_u = ref_u->user_data;

  return qoc_u->con_u;
}

static void
_quic_rand_cb(uint8_t* dst_y, size_t len_i, const ngtcp2_rand_ctx* ctx_u)
{
  (void)ctx_u;

  if ( 1 != RAND_bytes(dst_y, (int)len_i) ) {
    abort();
  }
}

static int
_quic_get_new_cid_cb(ngtcp2_conn*                 con_u,
                     ngtcp2_cid*                  cid_u,
                     ngtcp2_stateless_reset_token* tok_u,
                     size_t                       len_i,
                     void*                        dat_v)
{
  u3_mesa_quic_conn* qoc_u = dat_v;
  (void)con_u;

  if ( (NULL == qoc_u) || (NULL == qoc_u->qic_u) ) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  if ( 1 != RAND_bytes(cid_u->data, (int)len_i) ) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  cid_u->datalen = len_i;

  if ( 0 != ngtcp2_crypto_generate_stateless_reset_token(
              tok_u->data,
              qoc_u->qic_u->rst_y,
              sizeof(qoc_u->qic_u->rst_y),
              cid_u) )
  {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int
_quic_handshake_completed_cb(ngtcp2_conn* con_u, void* dat_v)
{
  u3_mesa_quic_conn* qoc_u = dat_v;
  u3_mesa_quic*      qic_u = qoc_u->qic_u;
  (void)con_u;

  qoc_u->han_o = c3y;
  _quic_log_addr("handshake complete", (const struct sockaddr*)&qoc_u->rem_stu);

  if ( (NULL == qoc_u->ses_u) && (NULL != qic_u->cb_u.ses_open_f) ) {
    qoc_u->ses_u = qic_u->cb_u.ses_open_f(qic_u->cb_u.ptr_v, qoc_u);
  }

  return 0;
}

static c3_o
_quic_deliver_packet(u3_mesa_quic_conn* qoc_u, c3_y* buf_y, c3_w len_w)
{
  u3_mesa_quic* qic_u = qoc_u->qic_u;

  if ( (0 == len_w) ||
       (NULL == qoc_u->ses_u) ||
       (NULL == qic_u->cb_u.pkt_f) )
  {
    c3_free(buf_y);
    return c3n;
  }

  qic_u->cb_u.pkt_f(qic_u->cb_u.ptr_v, qoc_u->ses_u, buf_y, len_w);
  return c3y;
}

static int
_quic_recv_datagram_cb(ngtcp2_conn*  con_u,
                       uint32_t      flg_w,
                       const uint8_t*dat_y,
                       size_t        len_i,
                       void*         dat_v)
{
  u3_mesa_quic_conn* qoc_u = dat_v;
  c3_y*              buf_y;
  (void)con_u;
  (void)flg_w;

  if ( len_i > UINT32_MAX ) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  if ( 0 == len_i ) {
    return 0;
  }

  buf_y = c3_malloc(len_i);
  memcpy(buf_y, dat_y, len_i);
  _quic_deliver_packet(qoc_u, buf_y, (c3_w)len_i);

  return 0;
}

static u3_mesa_quic_stream*
_quic_stream_find(u3_mesa_quic_conn* qoc_u, int64_t sid_i)
{
  u3_mesa_quic_stream* str_u;

  for ( str_u = qoc_u->str_u; NULL != str_u; str_u = str_u->nex_u ) {
    if ( sid_i == str_u->sid_i ) {
      return str_u;
    }
  }

  return NULL;
}

static u3_mesa_quic_stream*
_quic_stream_get(u3_mesa_quic_conn* qoc_u, int64_t sid_i)
{
  u3_mesa_quic_stream* str_u = _quic_stream_find(qoc_u, sid_i);

  if ( NULL != str_u ) {
    return str_u;
  }

  str_u = c3_calloc(sizeof(*str_u));
  str_u->sid_i = sid_i;
  str_u->nex_u = qoc_u->str_u;
  qoc_u->str_u = str_u;

  return str_u;
}

static void
_quic_stream_drop(u3_mesa_quic_conn* qoc_u, u3_mesa_quic_stream* str_u)
{
  u3_mesa_quic_stream** cur_u = &qoc_u->str_u;

  while ( NULL != *cur_u ) {
    if ( str_u == *cur_u ) {
      *cur_u = str_u->nex_u;
      c3_free(str_u->buf_y);
      c3_free(str_u);
      return;
    }
    cur_u = &(*cur_u)->nex_u;
  }
}

static int
_quic_recv_stream_cb(ngtcp2_conn*  con_u,
                     uint32_t      flg_w,
                     int64_t       sid_i,
                     uint64_t      off_d,
                     const uint8_t*dat_y,
                     size_t        len_i,
                     void*         dat_v,
                     void*         sud_v)
{
  u3_mesa_quic_conn*   qoc_u = dat_v;
  u3_mesa_quic_stream* str_u;
  uint64_t             nex_d;
  (void)sud_v;

  if ( len_i > UINT32_MAX ) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  str_u = _quic_stream_get(qoc_u, sid_i);
  nex_d = ((uint64_t)str_u->len_w) + len_i;

  if ( (off_d != str_u->len_w) ||
       (nex_d > U3_MESA_QUIC_MAX_STREAM_PACKET) ) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  if ( nex_d > str_u->cap_w ) {
    c3_w cap_w = (0 == str_u->cap_w) ? 1024 : str_u->cap_w;
    c3_y* nex_y;

    while ( cap_w < nex_d ) {
      cap_w *= 2;
    }

    nex_y = c3_malloc(cap_w);
    if ( NULL != str_u->buf_y ) {
      memcpy(nex_y, str_u->buf_y, str_u->len_w);
      c3_free(str_u->buf_y);
    }
    str_u->buf_y = nex_y;
    str_u->cap_w = cap_w;
  }

  if ( 0 != len_i ) {
    memcpy(str_u->buf_y + str_u->len_w, dat_y, len_i);
    str_u->len_w += (c3_w)len_i;
    ngtcp2_conn_extend_max_stream_offset(con_u, sid_i, len_i);
    ngtcp2_conn_extend_max_offset(con_u, len_i);
  }

  if ( 0 != (flg_w & NGTCP2_STREAM_DATA_FLAG_FIN) ) {
    c3_y* buf_y = str_u->buf_y;
    c3_w  len_w = str_u->len_w;

    str_u->buf_y = NULL;
    _quic_stream_drop(qoc_u, str_u);
    _quic_deliver_packet(qoc_u, buf_y, len_w);
  }

  return 0;
}

static int
_quic_stream_close_cb(ngtcp2_conn* con_u,
                      uint32_t     flg_w,
                      int64_t      sid_i,
                      uint64_t     app_d,
                      void*        dat_v,
                      void*        sud_v)
{
  u3_mesa_quic_conn*   qoc_u = dat_v;
  u3_mesa_quic_stream* str_u = _quic_stream_find(qoc_u, sid_i);
  (void)flg_w;
  (void)app_d;
  (void)sud_v;

  if ( NULL != str_u ) {
    _quic_stream_drop(qoc_u, str_u);
  }

  ngtcp2_conn_extend_max_streams_uni(con_u, 1);
  return 0;
}

static int
_quic_select_alpn_cb(ptls_on_client_hello_t*             self_u,
                     ptls_t*                             tls_u,
                     ptls_on_client_hello_parameters_t*  par_u)
{
  size_t i;
  (void)self_u;

  for ( i = 0; i < par_u->negotiated_protocols.count; ++i ) {
    ptls_iovec_t pro_u = par_u->negotiated_protocols.list[i];

    if ( (pro_u.len == (sizeof(_ames_alpn) - 1)) &&
         (0 == memcmp(pro_u.base, _ames_alpn, sizeof(_ames_alpn) - 1)) ) {
      return ptls_set_negotiated_protocol(tls_u, (const char*)pro_u.base,
                                          pro_u.len);
    }
  }

  return PTLS_ALERT_NO_APPLICATION_PROTOCOL;
}

static ptls_on_client_hello_t _quic_select_alpn = { _quic_select_alpn_cb };

static ptls_key_exchange_algorithm_t* _quic_key_exchanges[] = {
#if PTLS_OPENSSL_HAVE_X25519
  &ptls_openssl_x25519,
#endif
  &ptls_openssl_secp256r1,
#ifdef PTLS_OPENSSL_HAVE_SECP384R1
  &ptls_openssl_secp384r1,
#endif
#ifdef PTLS_OPENSSL_HAVE_SECP521R1
  &ptls_openssl_secp521r1,
#endif
  NULL
};

static ptls_cipher_suite_t* _quic_cipher_suites[] = {
  &ptls_openssl_aes128gcmsha256,
  &ptls_openssl_aes256gcmsha384,
#if PTLS_OPENSSL_HAVE_CHACHA20_POLY1305
  &ptls_openssl_chacha20poly1305sha256,
#endif
  NULL
};

static int
_quic_make_cert(u3_mesa_quic* qic_u)
{
  EVP_PKEY_CTX* ctx_u = NULL;
  EVP_PKEY*     key_u = NULL;
  X509*         cer_u = NULL;
  X509_NAME*    nam_u;
  int           ret_i = -1;

  ctx_u = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  if ( NULL == ctx_u ) {
    goto done;
  }
  if ( 1 != EVP_PKEY_keygen_init(ctx_u) ) {
    goto done;
  }
  if ( 1 != EVP_PKEY_CTX_set_ec_paramgen_curve_nid(
              ctx_u, NID_X9_62_prime256v1) )
  {
    goto done;
  }
  if ( 1 != EVP_PKEY_keygen(ctx_u, &key_u) ) {
    goto done;
  }

  cer_u = X509_new();
  if ( NULL == cer_u ) {
    goto done;
  }

  if ( (1 != X509_set_version(cer_u, 2)) ||
       (1 != ASN1_INTEGER_set(X509_get_serialNumber(cer_u),
                              (long)(time(NULL) & LONG_MAX))) ||
       (NULL == X509_gmtime_adj(X509_get_notBefore(cer_u), 0)) ||
       (NULL == X509_gmtime_adj(X509_get_notAfter(cer_u),
                                30L * 24L * 60L * 60L)) ||
       (1 != X509_set_pubkey(cer_u, key_u)) )
  {
    goto done;
  }

  nam_u = X509_get_subject_name(cer_u);
  if ( (NULL == nam_u) ||
       (1 != X509_NAME_add_entry_by_txt(nam_u, "CN", MBSTRING_ASC,
                                        (const unsigned char*)"localhost",
                                        -1, -1, 0)) ||
       (1 != X509_set_issuer_name(cer_u, nam_u)) ||
       (0 >= X509_sign(cer_u, key_u, EVP_sha256())) )
  {
    goto done;
  }

  if ( 0 != ptls_openssl_load_certificates(&qic_u->tls_u, cer_u, NULL) ) {
    goto done;
  }
  if ( 0 != ptls_openssl_init_sign_certificate(&qic_u->sig_u, key_u) ) {
    goto done;
  }

  qic_u->tls_u.sign_certificate = &qic_u->sig_u.super;
  ret_i = 0;

done:
  if ( NULL != cer_u ) {
    X509_free(cer_u);
  }
  if ( NULL != key_u ) {
    EVP_PKEY_free(key_u);
  }
  if ( NULL != ctx_u ) {
    EVP_PKEY_CTX_free(ctx_u);
  }
  return ret_i;
}

static int
_quic_init_tls(u3_mesa_quic* qic_u)
{
  qic_u->ctl_u = (ptls_context_t){
    .random_bytes = ptls_openssl_random_bytes,
    .get_time = &ptls_get_time,
    .key_exchanges = _quic_key_exchanges,
    .cipher_suites = _quic_cipher_suites,
    .require_dhe_on_psk = 1,
  };

  if ( 0 != ngtcp2_crypto_picotls_configure_client_context(&qic_u->ctl_u) ) {
    return -1;
  }

  qic_u->tls_u = (ptls_context_t){
    .random_bytes = ptls_openssl_random_bytes,
    .get_time = &ptls_get_time,
    .key_exchanges = _quic_key_exchanges,
    .cipher_suites = _quic_cipher_suites,
    .require_dhe_on_psk = 1,
    .server_cipher_preference = 1,
    .on_client_hello = &_quic_select_alpn,
  };

  if ( 0 != ngtcp2_crypto_picotls_configure_server_context(&qic_u->tls_u) ) {
    return -1;
  }

  return _quic_make_cert(qic_u);
}

static void
_quic_free_tls(u3_mesa_quic* qic_u)
{
  size_t i;

  if ( NULL != qic_u->sig_u.key ) {
    ptls_openssl_dispose_sign_certificate(&qic_u->sig_u);
    qic_u->sig_u.key = NULL;
  }

  for ( i = 0; i < qic_u->tls_u.certificates.count; ++i ) {
    free(qic_u->tls_u.certificates.list[i].base);
  }
  free(qic_u->tls_u.certificates.list);
  qic_u->tls_u.certificates.list = NULL;
  qic_u->tls_u.certificates.count = 0;
}

static void
_quic_init_transport_params(ngtcp2_transport_params* par_u)
{
  ngtcp2_transport_params_default(par_u);

  par_u->initial_max_data = 1024 * 1024;
  par_u->initial_max_stream_data_bidi_local = 128 * 1024;
  par_u->initial_max_stream_data_bidi_remote = 128 * 1024;
  par_u->initial_max_stream_data_uni = 1024 * 1024;
  par_u->initial_max_streams_bidi = 4;
  par_u->initial_max_streams_uni = 16;
  par_u->max_datagram_frame_size = U3_MESA_QUIC_DATAGRAM_FRAME_SIZE;
}

static void
_quic_init_settings(ngtcp2_settings* set_u, ngtcp2_tstamp now_d)
{
  ngtcp2_settings_default(set_u);
  set_u->initial_ts = now_d;
}

static ngtcp2_callbacks
_quic_server_callbacks(void)
{
  return (ngtcp2_callbacks){
    .recv_client_initial = ngtcp2_crypto_recv_client_initial_cb,
    .recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb,
    .handshake_completed = _quic_handshake_completed_cb,
    .encrypt = ngtcp2_crypto_encrypt_cb,
    .decrypt = ngtcp2_crypto_decrypt_cb,
    .hp_mask = ngtcp2_crypto_hp_mask_cb,
    .rand = _quic_rand_cb,
    .update_key = ngtcp2_crypto_update_key_cb,
    .delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    .delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    .recv_datagram = _quic_recv_datagram_cb,
    .recv_stream_data = _quic_recv_stream_cb,
    .stream_close = _quic_stream_close_cb,
    .version_negotiation = ngtcp2_crypto_version_negotiation_cb,
    .get_new_connection_id2 = _quic_get_new_cid_cb,
    .get_path_challenge_data2 = ngtcp2_crypto_get_path_challenge_data2_cb,
  };
}

static ngtcp2_callbacks
_quic_client_callbacks(void)
{
  return (ngtcp2_callbacks){
    .client_initial = ngtcp2_crypto_client_initial_cb,
    .recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb,
    .handshake_completed = _quic_handshake_completed_cb,
    .encrypt = ngtcp2_crypto_encrypt_cb,
    .decrypt = ngtcp2_crypto_decrypt_cb,
    .hp_mask = ngtcp2_crypto_hp_mask_cb,
    .recv_retry = ngtcp2_crypto_recv_retry_cb,
    .rand = _quic_rand_cb,
    .update_key = ngtcp2_crypto_update_key_cb,
    .delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    .delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    .recv_datagram = _quic_recv_datagram_cb,
    .recv_stream_data = _quic_recv_stream_cb,
    .stream_close = _quic_stream_close_cb,
    .version_negotiation = ngtcp2_crypto_version_negotiation_cb,
    .get_new_connection_id2 = _quic_get_new_cid_cb,
    .get_path_challenge_data2 = ngtcp2_crypto_get_path_challenge_data2_cb,
  };
}

static ngtcp2_socklen
_quic_sockaddr_len(const struct sockaddr* adr_u)
{
  switch ( adr_u->sa_family ) {
    case AF_INET:  return sizeof(struct sockaddr_in);
    case AF_INET6: return sizeof(struct sockaddr_in6);
    default:       return 0;
  }
}

static void
_quic_conn_unlink(u3_mesa_quic_conn* qoc_u)
{
  u3_mesa_quic_conn** cur_u = &qoc_u->qic_u->con_u;

  while ( NULL != *cur_u ) {
    if ( qoc_u == *cur_u ) {
      *cur_u = qoc_u->nex_u;
      return;
    }
    cur_u = &(*cur_u)->nex_u;
  }
}

static void
_quic_free_out(u3_mesa_quic_out* out_u)
{
  c3_free(out_u->buf_y);
  c3_free(out_u);
}

static void
_quic_free_pkt(u3_mesa_quic_pkt* pkt_u)
{
  c3_free(pkt_u->buf_y);
  c3_free(pkt_u);
}

static void
_quic_conn_free(u3_mesa_quic_conn* qoc_u)
{
  u3_mesa_quic_stream* str_u;
  u3_mesa_quic_out*    out_u;
  u3_mesa_quic_pkt*    pkt_u;

  while ( NULL != qoc_u->str_u ) {
    str_u = qoc_u->str_u;
    qoc_u->str_u = str_u->nex_u;
    c3_free(str_u->buf_y);
    c3_free(str_u);
  }

  while ( NULL != qoc_u->out_u ) {
    out_u = qoc_u->out_u;
    qoc_u->out_u = out_u->nex_u;
    _quic_free_out(out_u);
  }

  while ( NULL != qoc_u->pen_u ) {
    pkt_u = qoc_u->pen_u;
    qoc_u->pen_u = pkt_u->nex_u;
    _quic_free_pkt(pkt_u);
  }

  if ( NULL != qoc_u->con_u ) {
    ngtcp2_conn_del(qoc_u->con_u);
    qoc_u->con_u = NULL;
  }

  ngtcp2_crypto_picotls_deconfigure_session(&qoc_u->tls_u);

  if ( NULL != qoc_u->tls_u.ptls ) {
    ptls_free(qoc_u->tls_u.ptls);
    qoc_u->tls_u.ptls = NULL;
  }

  c3_free(qoc_u);
}

static void
_quic_conn_close(u3_mesa_quic_conn* qoc_u)
{
  u3_mesa_quic* qic_u = qoc_u->qic_u;

  if ( (NULL != qoc_u->ses_u) && (NULL != qic_u->cb_u.ses_close_f) ) {
    qic_u->cb_u.ses_close_f(qic_u->cb_u.ptr_v, qoc_u->ses_u);
    qoc_u->ses_u = NULL;
  }

  _quic_conn_unlink(qoc_u);
  _quic_conn_free(qoc_u);
}

static c3_o
_quic_cid_eq(const ngtcp2_cid* cid_u, const uint8_t* dat_y, size_t len_i)
{
  if ( cid_u->datalen != len_i ) {
    return c3n;
  }

  return __(0 == memcmp(cid_u->data, dat_y, len_i));
}

static c3_o
_quic_sockaddr_eq(const struct sockaddr_storage* a_u,
                  const struct sockaddr*         b_u)
{
  const struct sockaddr* aa_u = (const struct sockaddr*)a_u;

  if ( aa_u->sa_family != b_u->sa_family ) {
    return c3n;
  }

  switch ( b_u->sa_family ) {
    case AF_INET: {
      const struct sockaddr_in* ai_u = (const struct sockaddr_in*)aa_u;
      const struct sockaddr_in* bi_u = (const struct sockaddr_in*)b_u;

      return __( (ai_u->sin_port == bi_u->sin_port) &&
                 (ai_u->sin_addr.s_addr == bi_u->sin_addr.s_addr) );
    }

    case AF_INET6: {
      const struct sockaddr_in6* ai_u = (const struct sockaddr_in6*)aa_u;
      const struct sockaddr_in6* bi_u = (const struct sockaddr_in6*)b_u;

      return __( (ai_u->sin6_port == bi_u->sin6_port) &&
                 (ai_u->sin6_scope_id == bi_u->sin6_scope_id) &&
                 (0 == memcmp(&ai_u->sin6_addr,
                              &bi_u->sin6_addr,
                              sizeof(ai_u->sin6_addr))) );
    }

    default: return c3n;
  }
}

static u3_mesa_quic_conn*
_quic_find_conn(u3_mesa_quic* qic_u, const uint8_t* cid_y, size_t len_i)
{
  u3_mesa_quic_conn* qoc_u;

  for ( qoc_u = qic_u->con_u; NULL != qoc_u; qoc_u = qoc_u->nex_u ) {
    if ( _( _quic_cid_eq(&qoc_u->scid_u, cid_y, len_i) ) ||
         _( _quic_cid_eq(&qoc_u->odcid_u, cid_y, len_i) ) )
    {
      return qoc_u;
    }
  }

  return NULL;
}

static u3_mesa_quic_conn*
_quic_find_conn_addr(u3_mesa_quic* qic_u, const struct sockaddr* adr_u)
{
  u3_mesa_quic_conn* qoc_u;

  for ( qoc_u = qic_u->con_u; NULL != qoc_u; qoc_u = qoc_u->nex_u ) {
    if ( c3y == _quic_sockaddr_eq(&qoc_u->rem_stu, adr_u) ) {
      return qoc_u;
    }
  }

  return NULL;
}

static int
_quic_conn_init_tls(u3_mesa_quic_conn* qoc_u)
{
  int ret_i;

  ngtcp2_crypto_picotls_ctx_init(&qoc_u->tls_u);
  qoc_u->tls_u.ptls = ptls_server_new(&qoc_u->qic_u->tls_u);
  if ( NULL == qoc_u->tls_u.ptls ) {
    return -1;
  }

  *ptls_get_data_ptr(qoc_u->tls_u.ptls) = &qoc_u->ref_u;
  qoc_u->tls_u.handshake_properties.additional_extensions = qoc_u->ext_u;
  qoc_u->ext_u[0] = (ptls_raw_extension_t){ .type = UINT16_MAX };
  qoc_u->ext_u[1] = (ptls_raw_extension_t){ .type = UINT16_MAX };

  ret_i = ngtcp2_crypto_picotls_configure_server_session(&qoc_u->tls_u);
  if ( 0 != ret_i ) {
    return -1;
  }

  ngtcp2_conn_set_tls_native_handle(qoc_u->con_u, &qoc_u->tls_u);
  return 0;
}

static int
_quic_conn_init_client_tls(u3_mesa_quic_conn* qoc_u)
{
  int ret_i;

  ngtcp2_crypto_picotls_ctx_init(&qoc_u->tls_u);
  qoc_u->tls_u.ptls = ptls_client_new(&qoc_u->qic_u->ctl_u);
  if ( NULL == qoc_u->tls_u.ptls ) {
    return -1;
  }

  *ptls_get_data_ptr(qoc_u->tls_u.ptls) = &qoc_u->ref_u;
  qoc_u->tls_u.handshake_properties.additional_extensions = qoc_u->ext_u;
  qoc_u->ext_u[0] = (ptls_raw_extension_t){ .type = UINT16_MAX };
  qoc_u->ext_u[1] = (ptls_raw_extension_t){ .type = UINT16_MAX };

  ret_i = ngtcp2_crypto_picotls_configure_client_session(&qoc_u->tls_u,
                                                         qoc_u->con_u);
  if ( 0 != ret_i ) {
    return -1;
  }

  qoc_u->tls_u.handshake_properties.client.negotiated_protocols.list =
    _ames_client_alpn;
  qoc_u->tls_u.handshake_properties.client.negotiated_protocols.count = 1;
  ptls_set_server_name(qoc_u->tls_u.ptls, "localhost", strlen("localhost"));

  ngtcp2_conn_set_tls_native_handle(qoc_u->con_u, &qoc_u->tls_u);
  return 0;
}

static u3_mesa_quic_conn*
_quic_conn_new(u3_mesa_quic*            qic_u,
               const struct sockaddr*   rem_u,
               const ngtcp2_version_cid*vcid_u,
               uint32_t                 ver_w)
{
  u3_mesa_quic_conn*       qoc_u;
  ngtcp2_cid               dcid_u;
  ngtcp2_path              pat_u;
  ngtcp2_callbacks         cal_u;
  ngtcp2_settings          set_u;
  ngtcp2_transport_params  par_u;
  ngtcp2_tstamp            now_d;
  c3_i                     loc_i;
  int                      ret_i;

  if ( (vcid_u->dcidlen > NGTCP2_MAX_CIDLEN) ||
       (vcid_u->scidlen > NGTCP2_MAX_CIDLEN) ||
       (vcid_u->dcidlen < NGTCP2_MIN_INITIAL_DCIDLEN) )
  {
    return NULL;
  }

  qoc_u = c3_calloc(sizeof(*qoc_u));
  qoc_u->qic_u = qic_u;
  qoc_u->han_o = c3n;
  qoc_u->ref_u.get_conn = _quic_get_conn_from_ref;
  qoc_u->ref_u.user_data = qoc_u;
  now_d = _quic_conn_now(qoc_u);

  loc_i = sizeof(qoc_u->loc_stu);
  if ( 0 != uv_udp_getsockname(&qic_u->udp_u,
                               (struct sockaddr*)&qoc_u->loc_stu, &loc_i) )
  {
    c3_free(qoc_u);
    return NULL;
  }

  memcpy(&qoc_u->rem_stu, rem_u, _quic_sockaddr_len(rem_u));
  qoc_u->loc_u.addr = (ngtcp2_sockaddr*)&qoc_u->loc_stu;
  qoc_u->loc_u.addrlen = loc_i;
  qoc_u->rem_u.addr = (ngtcp2_sockaddr*)&qoc_u->rem_stu;
  qoc_u->rem_u.addrlen = _quic_sockaddr_len(rem_u);

  ngtcp2_cid_init(&dcid_u, vcid_u->scid, vcid_u->scidlen);
  ngtcp2_cid_init(&qoc_u->odcid_u, vcid_u->dcid, vcid_u->dcidlen);
  qoc_u->scid_u.datalen = U3_MESA_QUIC_CIDLEN;

  if ( 1 != RAND_bytes(qoc_u->scid_u.data, (int)qoc_u->scid_u.datalen) ) {
    c3_free(qoc_u);
    return NULL;
  }

  pat_u = (ngtcp2_path){
    .local = qoc_u->loc_u,
    .remote = qoc_u->rem_u,
  };
  cal_u = _quic_server_callbacks();
  _quic_init_settings(&set_u, now_d);
  _quic_init_transport_params(&par_u);
  par_u.original_dcid = qoc_u->odcid_u;
  par_u.original_dcid_present = 1;

  if ( 0 != ngtcp2_crypto_generate_stateless_reset_token(
              par_u.stateless_reset_token,
              qic_u->rst_y,
              sizeof(qic_u->rst_y),
              &qoc_u->scid_u) )
  {
    c3_free(qoc_u);
    return NULL;
  }

  ret_i = ngtcp2_conn_server_new(&qoc_u->con_u,
                                 &dcid_u,
                                 &qoc_u->scid_u,
                                 &pat_u,
                                 ver_w,
                                 &cal_u,
                                 &set_u,
                                 &par_u,
                                 NULL,
                                 qoc_u);
  if ( 0 != ret_i ) {
    _quic_log_ngtcp2("ngtcp2_conn_server_new", ret_i);
    c3_free(qoc_u);
    return NULL;
  }

  if ( 0 != _quic_conn_init_tls(qoc_u) ) {
    _quic_conn_free(qoc_u);
    return NULL;
  }

  qoc_u->nex_u = qic_u->con_u;
  qic_u->con_u = qoc_u;
  _quic_log_addr("accepted connection", rem_u);
  return qoc_u;
}

static u3_mesa_quic_conn*
_quic_conn_dial(u3_mesa_quic* qic_u, const struct sockaddr* rem_u)
{
  u3_mesa_quic_conn*       qoc_u;
  ngtcp2_path              pat_u;
  ngtcp2_callbacks         cal_u;
  ngtcp2_settings          set_u;
  ngtcp2_transport_params  par_u;
  ngtcp2_tstamp            now_d;
  ngtcp2_socklen           rem_i;
  c3_i                     loc_i;
  int                      ret_i;

  rem_i = _quic_sockaddr_len(rem_u);
  if ( 0 == rem_i ) {
    return NULL;
  }

  qoc_u = c3_calloc(sizeof(*qoc_u));
  qoc_u->qic_u = qic_u;
  qoc_u->han_o = c3n;
  qoc_u->ref_u.get_conn = _quic_get_conn_from_ref;
  qoc_u->ref_u.user_data = qoc_u;
  now_d = _quic_conn_now(qoc_u);

  loc_i = sizeof(qoc_u->loc_stu);
  if ( 0 != uv_udp_getsockname(&qic_u->udp_u,
                               (struct sockaddr*)&qoc_u->loc_stu, &loc_i) )
  {
    c3_free(qoc_u);
    return NULL;
  }

  memcpy(&qoc_u->rem_stu, rem_u, rem_i);
  qoc_u->loc_u.addr = (ngtcp2_sockaddr*)&qoc_u->loc_stu;
  qoc_u->loc_u.addrlen = loc_i;
  qoc_u->rem_u.addr = (ngtcp2_sockaddr*)&qoc_u->rem_stu;
  qoc_u->rem_u.addrlen = rem_i;

  qoc_u->odcid_u.datalen = U3_MESA_QUIC_CIDLEN;
  qoc_u->scid_u.datalen = U3_MESA_QUIC_CIDLEN;
  if (  (1 != RAND_bytes(qoc_u->odcid_u.data,
                         (int)qoc_u->odcid_u.datalen))
     || (1 != RAND_bytes(qoc_u->scid_u.data,
                         (int)qoc_u->scid_u.datalen)) )
  {
    c3_free(qoc_u);
    return NULL;
  }

  pat_u = (ngtcp2_path){
    .local = qoc_u->loc_u,
    .remote = qoc_u->rem_u,
  };
  cal_u = _quic_client_callbacks();
  _quic_init_settings(&set_u, now_d);
  _quic_init_transport_params(&par_u);

  ret_i = ngtcp2_conn_client_new(&qoc_u->con_u,
                                 &qoc_u->odcid_u,
                                 &qoc_u->scid_u,
                                 &pat_u,
                                 NGTCP2_PROTO_VER_V1,
                                 &cal_u,
                                 &set_u,
                                 &par_u,
                                 NULL,
                                 qoc_u);
  if ( 0 != ret_i ) {
    _quic_log_ngtcp2("ngtcp2_conn_client_new", ret_i);
    c3_free(qoc_u);
    return NULL;
  }

  if ( 0 != _quic_conn_init_client_tls(qoc_u) ) {
    _quic_conn_free(qoc_u);
    return NULL;
  }

  qoc_u->nex_u = qic_u->con_u;
  qic_u->con_u = qoc_u;
  _quic_log_addr("dialing connection", rem_u);
  _quic_flush_conn(qoc_u);
  _quic_schedule(qic_u);
  return qoc_u;
}

static void
_quic_send_cb(uv_udp_send_t* req_u, c3_i sas_i)
{
  u3_mesa_quic_send* snd_u = (u3_mesa_quic_send*)req_u;

  if ( 0 != sas_i ) {
    fprintf(stderr, "mesa: quic: send: %s\n", uv_strerror(sas_i));
  }

  c3_free(snd_u->buf_y);
  c3_free(snd_u);
}

static c3_o
_quic_udp_send(u3_mesa_quic_conn* qoc_u,
               const ngtcp2_path* pat_u,
               const uint8_t*     dat_y,
               size_t             len_i)
{
  u3_mesa_quic_send* snd_u;
  uv_buf_t           buf_u;
  const struct sockaddr* dst_u;
  c3_i               ret_i;

  if ( (0 == len_i) || (len_i > UINT32_MAX) || _(qoc_u->qic_u->clo_o) ) {
    return c3n;
  }

  if ( (NULL != pat_u) && (NULL != pat_u->remote.addr) ) {
    dst_u = (const struct sockaddr*)pat_u->remote.addr;
  }
  else {
    dst_u = (const struct sockaddr*)qoc_u->rem_u.addr;
  }

  snd_u = c3_calloc(sizeof(*snd_u));
  snd_u->buf_y = c3_malloc(len_i);
  snd_u->len_w = (c3_w)len_i;
  snd_u->con_u = qoc_u;
  memcpy(snd_u->buf_y, dat_y, len_i);

  buf_u = uv_buf_init((c3_c*)snd_u->buf_y, (unsigned int)snd_u->len_w);
  ret_i = uv_udp_send(&snd_u->req_u,
                      &qoc_u->qic_u->udp_u,
                      &buf_u,
                      1,
                      dst_u,
                      _quic_send_cb);
  if ( 0 != ret_i ) {
    fprintf(stderr, "mesa: quic: send: %s\n", uv_strerror(ret_i));
    c3_free(snd_u->buf_y);
    c3_free(snd_u);
    return c3n;
  }

  return c3y;
}

static void
_quic_drain_streams(u3_mesa_quic_conn* qoc_u)
{
  size_t lim_i;

  for ( lim_i = 0; (lim_i < 32) && (NULL != qoc_u->out_u); ++lim_i ) {
    c3_y              dat_y[U3_MESA_QUIC_MAX_UDP];
    ngtcp2_path_storage ps_u;
    ngtcp2_pkt_info  pin_u = { 0 };
    ngtcp2_ssize     wrt_i;
    ngtcp2_ssize     dat_i = 0;
    u3_mesa_quic_out*out_u = qoc_u->out_u;
    ngtcp2_tstamp    now_d = _quic_conn_now(qoc_u);
    uint32_t          flg_w = NGTCP2_WRITE_STREAM_FLAG_NONE;

    if ( out_u->off_w == out_u->len_w ) {
      flg_w |= NGTCP2_WRITE_STREAM_FLAG_FIN;
    }
    else if ( (out_u->len_w - out_u->off_w) <= U3_MESA_QUIC_MAX_UDP ) {
      flg_w |= NGTCP2_WRITE_STREAM_FLAG_FIN;
    }

    ngtcp2_path_storage_zero(&ps_u);
    wrt_i = ngtcp2_conn_write_stream(qoc_u->con_u,
                                     &ps_u.path,
                                     &pin_u,
                                     dat_y,
                                     sizeof(dat_y),
                                     &dat_i,
                                     flg_w,
                                     out_u->sid_i,
                                     out_u->buf_y + out_u->off_w,
                                     out_u->len_w - out_u->off_w,
                                     now_d);
    if ( wrt_i < 0 ) {
      if ( (NGTCP2_ERR_STREAM_DATA_BLOCKED == wrt_i) ||
           (NGTCP2_ERR_STREAM_ID_BLOCKED == wrt_i) ) {
        return;
      }

      _quic_log_ngtcp2("ngtcp2_conn_write_stream", (c3_i)wrt_i);
      qoc_u->out_u = out_u->nex_u;
      _quic_free_out(out_u);
      continue;
    }

    if ( 0 == wrt_i ) {
      return;
    }

    ngtcp2_conn_update_pkt_tx_time(qoc_u->con_u, now_d);
    _quic_udp_send(qoc_u, &ps_u.path, dat_y, (size_t)wrt_i);

    if ( dat_i > 0 ) {
      out_u->off_w += (c3_w)dat_i;
    }

    if ( (out_u->off_w == out_u->len_w) &&
         (0 != (flg_w & NGTCP2_WRITE_STREAM_FLAG_FIN)) ) {
      qoc_u->out_u = out_u->nex_u;
      _quic_free_out(out_u);
    }
  }
}

static void
_quic_flush_conn(u3_mesa_quic_conn* qoc_u)
{
  size_t lim_i;

  _quic_drain_streams(qoc_u);

  for ( lim_i = 0; lim_i < 32; ++lim_i ) {
    c3_y                 dat_y[U3_MESA_QUIC_MAX_UDP];
    ngtcp2_path_storage  ps_u;
    ngtcp2_pkt_info      pin_u = { 0 };
    ngtcp2_ssize         wrt_i;
    ngtcp2_tstamp        now_d = _quic_conn_now(qoc_u);

    ngtcp2_path_storage_zero(&ps_u);
    wrt_i = ngtcp2_conn_write_pkt(qoc_u->con_u, &ps_u.path, &pin_u,
                                  dat_y, sizeof(dat_y), now_d);
    if ( wrt_i < 0 ) {
      if ( (NGTCP2_ERR_CLOSING == wrt_i) ||
           (NGTCP2_ERR_DRAINING == wrt_i) ) {
        return;
      }
      _quic_log_ngtcp2("ngtcp2_conn_write_pkt", (c3_i)wrt_i);
      return;
    }

    if ( 0 == wrt_i ) {
      return;
    }

    ngtcp2_conn_update_pkt_tx_time(qoc_u->con_u, now_d);
    _quic_udp_send(qoc_u, &ps_u.path, dat_y, (size_t)wrt_i);
  }
}

static void
_quic_read_conn(u3_mesa_quic_conn* qoc_u,
                const uint8_t*     dat_y,
                size_t             len_i,
                const struct sockaddr* rem_u)
{
  ngtcp2_path      pat_u;
  ngtcp2_pkt_info  pin_u = { 0 };
  ngtcp2_tstamp    now_d = _quic_conn_now(qoc_u);
  int              ret_i;

  memcpy(&qoc_u->rem_stu, rem_u, _quic_sockaddr_len(rem_u));
  qoc_u->rem_u.addr = (ngtcp2_sockaddr*)&qoc_u->rem_stu;
  qoc_u->rem_u.addrlen = _quic_sockaddr_len(rem_u);

  pat_u = (ngtcp2_path){
    .local = qoc_u->loc_u,
    .remote = qoc_u->rem_u,
  };

  ret_i = ngtcp2_conn_read_pkt(qoc_u->con_u, &pat_u, &pin_u,
                               dat_y, len_i, now_d);
  if ( 0 != ret_i ) {
    if ( (NGTCP2_ERR_CLOSING == ret_i) ||
         (NGTCP2_ERR_DRAINING == ret_i) ) {
      return;
    }

    if ( NGTCP2_ERR_CRYPTO == ret_i ) {
      fprintf(stderr,
              "mesa: quic: read packet: %s (tls alert=%u)\n",
              ngtcp2_strerror(ret_i),
              ngtcp2_conn_get_tls_alert2(qoc_u->con_u));
    }
    else {
      _quic_log_ngtcp2("ngtcp2_conn_read_pkt", ret_i);
    }

    _quic_conn_close(qoc_u);
    return;
  }

  _quic_flush_pending(qoc_u);
  _quic_flush_conn(qoc_u);
}

static void
_quic_alloc(uv_handle_t* had_u, size_t len_i, uv_buf_t* buf_u)
{
  (void)had_u;
  (void)len_i;

  buf_u->base = c3_malloc(U3_MESA_QUIC_RECV_BUF);
  buf_u->len = U3_MESA_QUIC_RECV_BUF;
}

static void
_quic_recv_cb(uv_udp_t*              udp_u,
              ssize_t                nrd_i,
              const uv_buf_t*        buf_u,
              const struct sockaddr* adr_u,
              unsigned               flg_i)
{
  u3_mesa_quic*       qic_u = udp_u->data;
  ngtcp2_version_cid  vcid_u;
  u3_mesa_quic_conn*  qoc_u;
  int                 ret_i;

  if ( 0 > nrd_i ) {
    fprintf(stderr, "mesa: quic: recv: %s\n", uv_strerror((int)nrd_i));
    goto done;
  }

  if ( (0 == nrd_i) || (NULL == adr_u) ) {
    goto done;
  }

  if ( 0 != (flg_i & UV_UDP_PARTIAL) ) {
    fprintf(stderr, "mesa: quic: recv: message truncated\n");
    goto done;
  }

  ret_i = ngtcp2_pkt_decode_version_cid(&vcid_u,
                                        (const uint8_t*)buf_u->base,
                                        (size_t)nrd_i,
                                        U3_MESA_QUIC_CIDLEN);
  if ( NGTCP2_ERR_VERSION_NEGOTIATION == ret_i ) {
    fprintf(stderr, "mesa: quic: unsupported version 0x%08x\n",
            vcid_u.version);
    goto done;
  }
  if ( 0 != ret_i ) {
    _quic_log_ngtcp2("decode packet header", ret_i);
    goto done;
  }

  qoc_u = _quic_find_conn(qic_u, vcid_u.dcid, vcid_u.dcidlen);
  if ( (NULL == qoc_u) &&
       ((0 == vcid_u.version) || (0 == vcid_u.scidlen)) )
  {
    qoc_u = _quic_find_conn_addr(qic_u, adr_u);
  }
  if ( NULL == qoc_u ) {
    ngtcp2_pkt_hd hd_u;

    ret_i = ngtcp2_accept(&hd_u, (const uint8_t*)buf_u->base,
                          (size_t)nrd_i);
    if ( 0 != ret_i ) {
      if ( NGTCP2_ERR_INVALID_ARGUMENT != ret_i ) {
        _quic_log_ngtcp2("accept initial", ret_i);
      }
      goto done;
    }

    if ( NGTCP2_PKT_INITIAL != hd_u.type ) {
      fprintf(stderr, "mesa: quic: dropped first non-initial packet type %u\n",
              hd_u.type);
      goto done;
    }

    qoc_u = _quic_conn_new(qic_u, adr_u, &vcid_u, hd_u.version);
    if ( NULL == qoc_u ) {
      fprintf(stderr,
              "mesa: quic: failed to create connection "
              "(version=0x%08x, dcidlen=%zu, scidlen=%zu)\n",
              hd_u.version, vcid_u.dcidlen, vcid_u.scidlen);
      goto done;
    }
  }

  _quic_read_conn(qoc_u, (const uint8_t*)buf_u->base, (size_t)nrd_i, adr_u);
  _quic_schedule(qic_u);

done:
  c3_free(buf_u->base);
}

static c3_w
_quic_max_datagram_payload(u3_mesa_quic_conn* qoc_u)
{
  const ngtcp2_transport_params* par_u;
  uint64_t                       siz_d;

  par_u = ngtcp2_conn_get_remote_transport_params2(qoc_u->con_u);
  siz_d = (NULL != par_u) ? par_u->max_datagram_frame_size : 0;

  if ( 0 == siz_d ) {
    siz_d = U3_MESA_QUIC_DATAGRAM_FRAME_SIZE;
  }
  if ( siz_d > U3_MESA_QUIC_DATAGRAM_FRAME_SIZE ) {
    siz_d = U3_MESA_QUIC_DATAGRAM_FRAME_SIZE;
  }
  if ( siz_d <= U3_MESA_QUIC_DGRAM_OVERHEAD ) {
    return 0;
  }

  return (c3_w)(siz_d - U3_MESA_QUIC_DGRAM_OVERHEAD);
}

static void
_quic_send_datagram(u3_mesa_quic_conn* qoc_u,
                    c3_y*              buf_y,
                    c3_w               len_w)
{
  c3_y                 dat_y[U3_MESA_QUIC_MAX_UDP];
  ngtcp2_path_storage  ps_u;
  ngtcp2_pkt_info      pin_u = { 0 };
  ngtcp2_ssize         wrt_i;
  ngtcp2_tstamp        now_d = _quic_conn_now(qoc_u);
  int                  acc_i = 0;

  ngtcp2_path_storage_zero(&ps_u);
  wrt_i = ngtcp2_conn_write_datagram(qoc_u->con_u,
                                     &ps_u.path,
                                     &pin_u,
                                     dat_y,
                                     sizeof(dat_y),
                                     &acc_i,
                                     NGTCP2_WRITE_DATAGRAM_FLAG_NONE,
                                     ++qoc_u->dgr_d,
                                     buf_y,
                                     len_w,
                                     now_d);
  if ( wrt_i < 0 ) {
    _quic_log_ngtcp2("ngtcp2_conn_write_datagram", (c3_i)wrt_i);
    c3_free(buf_y);
    return;
  }

  if ( (0 == wrt_i) || !acc_i ) {
    c3_free(buf_y);
    return;
  }

  ngtcp2_conn_update_pkt_tx_time(qoc_u->con_u, now_d);
  _quic_udp_send(qoc_u, &ps_u.path, dat_y, (size_t)wrt_i);
  c3_free(buf_y);
}

static void
_quic_queue_stream(u3_mesa_quic_conn* qoc_u, c3_y* buf_y, c3_w len_w)
{
  u3_mesa_quic_out* out_u;
  u3_mesa_quic_out**cur_u;
  int64_t           sid_i;
  int               ret_i;

  ret_i = ngtcp2_conn_open_uni_stream(qoc_u->con_u, &sid_i, NULL);
  if ( 0 != ret_i ) {
    _quic_log_ngtcp2("ngtcp2_conn_open_uni_stream", ret_i);
    c3_free(buf_y);
    return;
  }

  out_u = c3_calloc(sizeof(*out_u));
  out_u->sid_i = sid_i;
  out_u->buf_y = buf_y;
  out_u->len_w = len_w;

  cur_u = &qoc_u->out_u;
  while ( NULL != *cur_u ) {
    cur_u = &(*cur_u)->nex_u;
  }
  *cur_u = out_u;
}

static void
_quic_queue_pending(u3_mesa_quic_conn* qoc_u, c3_y* buf_y, c3_w len_w)
{
  u3_mesa_quic_pkt*  pkt_u;
  u3_mesa_quic_pkt**cur_u;

  while ( qoc_u->pen_w >= U3_MESA_QUIC_PENDING_MAX ) {
    pkt_u = qoc_u->pen_u;
    if ( NULL == pkt_u ) {
      qoc_u->pen_w = 0;
      break;
    }

    qoc_u->pen_u = pkt_u->nex_u;
    qoc_u->pen_w--;
    _quic_free_pkt(pkt_u);
  }

  pkt_u = c3_calloc(sizeof(*pkt_u));
  pkt_u->buf_y = buf_y;
  pkt_u->len_w = len_w;

  cur_u = &qoc_u->pen_u;
  while ( NULL != *cur_u ) {
    cur_u = &(*cur_u)->nex_u;
  }
  *cur_u = pkt_u;
  qoc_u->pen_w++;
}

static void
_quic_flush_pending(u3_mesa_quic_conn* qoc_u)
{
  while (  _(qoc_u->han_o)
        && (NULL != qoc_u->pen_u) )
  {
    u3_mesa_quic_pkt* pkt_u = qoc_u->pen_u;
    c3_y*             buf_y = pkt_u->buf_y;
    c3_w              len_w = pkt_u->len_w;

    qoc_u->pen_u = pkt_u->nex_u;
    qoc_u->pen_w--;
    pkt_u->buf_y = NULL;
    c3_free(pkt_u);

    _quic_send_conn(qoc_u, buf_y, len_w);
  }
}

static void
_quic_send_conn(u3_mesa_quic_conn* qoc_u,
                c3_y*              buf_y,
                c3_w               len_w)
{
  u3_mesa_quic*           qic_u;
  u3_mesa_quic_send_kind  kin_e;
  c3_w                    max_w;

  if ( (NULL == qoc_u) ||
       (NULL == qoc_u->con_u) ||
       (0 == len_w) )
  {
    c3_free(buf_y);
    return;
  }

  qic_u = qoc_u->qic_u;
  if ( c3n == qoc_u->han_o ) {
    _quic_queue_pending(qoc_u, buf_y, len_w);
    _quic_flush_conn(qoc_u);
    _quic_schedule(qic_u);
    return;
  }

  max_w = _quic_max_datagram_payload(qoc_u);
  kin_e = u3_mesa_quic_classify_send(len_w, max_w);

  switch ( kin_e ) {
    case U3_MESA_QUIC_SEND_DATAGRAM: {
      _quic_send_datagram(qoc_u, buf_y, len_w);
    } break;

    case U3_MESA_QUIC_SEND_STREAM: {
      _quic_queue_stream(qoc_u, buf_y, len_w);
      _quic_flush_conn(qoc_u);
    } break;
  }

  _quic_schedule(qic_u);
}

static void
_quic_send_backend(void* bak_v, u3_sess* ses_u, c3_y* buf_y, c3_w len_w)
{
  u3_mesa_quic*       qic_u = bak_v;
  u3_mesa_quic_conn*  qoc_u;

  if ( (NULL == qic_u) ||
       (NULL == ses_u) ||
       (NULL == ses_u->bak_p) ||
       (0 == len_w) )
  {
    c3_free(buf_y);
    return;
  }

  qoc_u = ses_u->bak_p;
  if ( qic_u != qoc_u->qic_u ) {
    c3_free(buf_y);
    return;
  }

  _quic_send_conn(qoc_u, buf_y, len_w);
}

static void
_quic_send_addr_backend(void*                  bak_v,
                        const struct sockaddr* adr_u,
                        c3_y*                  buf_y,
                        c3_w                   len_w)
{
  u3_mesa_quic*       qic_u = bak_v;
  u3_mesa_quic_conn*  qoc_u;
  const struct sockaddr_in* sin_u;

  if ( (NULL == qic_u) ||
       (NULL == adr_u) ||
       (AF_INET != adr_u->sa_family) ||
       (0 == len_w) ||
       _(qic_u->clo_o) )
  {
    c3_free(buf_y);
    return;
  }

  sin_u = (const struct sockaddr_in*)adr_u;
  if ( 0 == sin_u->sin_port ) {
    c3_free(buf_y);
    return;
  }

  qoc_u = _quic_find_conn_addr(qic_u, adr_u);
  if ( NULL == qoc_u ) {
    qoc_u = _quic_conn_dial(qic_u, adr_u);
  }
  if ( NULL == qoc_u ) {
    c3_free(buf_y);
    return;
  }

  _quic_send_conn(qoc_u, buf_y, len_w);
}

static void
_quic_timer_cb(uv_timer_t* tim_u)
{
  u3_mesa_quic*       qic_u = tim_u->data;
  u3_mesa_quic_conn*  qoc_u = qic_u->con_u;
  ngtcp2_tstamp       now_d = _quic_now();

  while ( NULL != qoc_u ) {
    u3_mesa_quic_conn* nex_u = qoc_u->nex_u;
    ngtcp2_tstamp      exp_d = ngtcp2_conn_get_expiry2(qoc_u->con_u);

    if ( exp_d <= now_d ) {
      int ret_i = ngtcp2_conn_handle_expiry(qoc_u->con_u,
                                            _quic_conn_now(qoc_u));

      if ( 0 != ret_i ) {
        _quic_log_ngtcp2("ngtcp2_conn_handle_expiry", ret_i);
        _quic_conn_close(qoc_u);
      }
      else {
        _quic_flush_conn(qoc_u);
      }
    }

    qoc_u = nex_u;
  }

  _quic_schedule(qic_u);
}

static void
_quic_schedule(u3_mesa_quic* qic_u)
{
  u3_mesa_quic_conn* qoc_u;
  ngtcp2_tstamp      now_d;
  ngtcp2_tstamp      nex_d = UINT64_MAX;
  uint64_t           gap_d;

  if ( _(qic_u->clo_o) || (c3n == qic_u->tim_o) ) {
    return;
  }

  for ( qoc_u = qic_u->con_u; NULL != qoc_u; qoc_u = qoc_u->nex_u ) {
    ngtcp2_tstamp exp_d = ngtcp2_conn_get_expiry2(qoc_u->con_u);

    if ( exp_d < nex_d ) {
      nex_d = exp_d;
    }
  }

  if ( UINT64_MAX == nex_d ) {
    uv_timer_stop(&qic_u->tim_u);
    return;
  }

  now_d = _quic_now();
  gap_d = (nex_d <= now_d) ? 0 : ((nex_d - now_d) / NGTCP2_MILLISECONDS);

  uv_timer_start(&qic_u->tim_u, _quic_timer_cb, gap_d, 0);
}

static void
_quic_handle_close_cb(uv_handle_t* had_u)
{
  u3_mesa_quic* qic_u = had_u->data;

  if ( NULL != qic_u ) {
    qic_u->clo_w--;

    if ( (0 == qic_u->clo_w) && _(qic_u->clo_o) ) {
      c3_free(qic_u);
    }
  }
}

void
u3_mesa_quic_close(u3_mesa_quic* qic_u)
{
  u3_mesa_quic_conn* qoc_u;

  if ( (NULL == qic_u) || _(qic_u->clo_o) ) {
    return;
  }

  qic_u->clo_o = c3y;

  while ( NULL != qic_u->con_u ) {
    qoc_u = qic_u->con_u;
    _quic_conn_close(qoc_u);
  }

  _quic_free_tls(qic_u);

  if ( _(qic_u->udp_o) ) {
    uv_udp_recv_stop(&qic_u->udp_u);
    if ( !uv_is_closing((uv_handle_t*)&qic_u->udp_u) ) {
      qic_u->clo_w++;
      uv_close((uv_handle_t*)&qic_u->udp_u, _quic_handle_close_cb);
    }
  }

  if ( _(qic_u->tim_o) ) {
    uv_timer_stop(&qic_u->tim_u);
    if ( !uv_is_closing((uv_handle_t*)&qic_u->tim_u) ) {
      qic_u->clo_w++;
      uv_close((uv_handle_t*)&qic_u->tim_u, _quic_handle_close_cb);
    }
  }

  if ( 0 == qic_u->clo_w ) {
    c3_free(qic_u);
  }
}

c3_o
u3_mesa_quic_open(u3_mesa_quic**              out_u,
                  u3_mesa_tran*              tran_u,
                  const u3_mesa_quic_config* cfg_u)
{
  u3_mesa_quic*      qic_u;
  struct sockaddr_in add_u;
  c3_i               len_i;
  c3_i               ret_i;

  if ( (NULL == out_u) || (NULL == cfg_u) || (NULL == cfg_u->lup_u) ) {
    return c3n;
  }

  *out_u = NULL;
  qic_u = c3_calloc(sizeof(*qic_u));
  qic_u->udp_o = c3n;
  qic_u->tim_o = c3n;
  qic_u->clo_o = c3n;
  qic_u->cb_u = cfg_u->cb_u;

  if ( 1 != RAND_bytes(qic_u->rst_y, (int)sizeof(qic_u->rst_y)) ) {
    fprintf(stderr, "mesa: quic: reset secret init failed\n");
    c3_free(qic_u);
    return c3n;
  }

  if ( 0 != _quic_init_tls(qic_u) ) {
    fprintf(stderr, "mesa: quic: tls init failed\n");
    c3_free(qic_u);
    return c3n;
  }

  ret_i = uv_udp_init(cfg_u->lup_u, &qic_u->udp_u);
  if ( 0 != ret_i ) {
    fprintf(stderr, "mesa: quic: udp init: %s\n", uv_strerror(ret_i));
    _quic_free_tls(qic_u);
    c3_free(qic_u);
    return c3n;
  }
  qic_u->udp_o = c3y;
  qic_u->udp_u.data = qic_u;

  ret_i = uv_timer_init(cfg_u->lup_u, &qic_u->tim_u);
  if ( 0 != ret_i ) {
    fprintf(stderr, "mesa: quic: timer init: %s\n", uv_strerror(ret_i));
    u3_mesa_quic_close(qic_u);
    return c3n;
  }
  qic_u->tim_o = c3y;
  qic_u->tim_u.data = qic_u;

  memset(&add_u, 0, sizeof(add_u));
  add_u.sin_family = AF_INET;
  add_u.sin_addr.s_addr = _(cfg_u->net_o) ? htonl(INADDR_ANY)
                                          : htonl(INADDR_LOOPBACK);
  add_u.sin_port = htons(cfg_u->por_s);

  ret_i = uv_udp_bind(&qic_u->udp_u, (const struct sockaddr*)&add_u, 0);
  if ( 0 != ret_i ) {
    fprintf(stderr, "mesa: quic: bind: %s\n", uv_strerror(ret_i));
    u3_mesa_quic_close(qic_u);
    return c3n;
  }

  len_i = sizeof(add_u);
  ret_i = uv_udp_getsockname(&qic_u->udp_u, (struct sockaddr*)&add_u, &len_i);
  if ( 0 != ret_i ) {
    fprintf(stderr, "mesa: quic: getsockname: %s\n", uv_strerror(ret_i));
    u3_mesa_quic_close(qic_u);
    return c3n;
  }
  qic_u->por_s = ntohs(add_u.sin_port);

  ret_i = uv_udp_recv_start(&qic_u->udp_u, _quic_alloc, _quic_recv_cb);
  if ( 0 != ret_i ) {
    fprintf(stderr, "mesa: quic: recv start: %s\n", uv_strerror(ret_i));
    u3_mesa_quic_close(qic_u);
    return c3n;
  }

  if ( NULL != tran_u ) {
    *tran_u = (u3_mesa_tran){
      .sen_f = _quic_send_backend,
      .adr_f = _quic_send_addr_backend,
      .bak_v = qic_u,
    };
  }

  *out_u = qic_u;
  return c3y;
}

c3_s
u3_mesa_quic_port(const u3_mesa_quic* qic_u)
{
  return (NULL == qic_u) ? 0 : qic_u->por_s;
}
