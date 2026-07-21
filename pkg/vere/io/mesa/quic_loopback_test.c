#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_picotls.h>
#include <picotls.h>
#include <picotls/openssl.h>

#define CIDLEN NGTCP2_MIN_INITIAL_DCIDLEN
#define MAX_UDP_PAYLOAD_SIZE 1452
#define DATAGRAM_FRAME_SIZE 1200

static const uint8_t ames_alpn[] = "ames/1";
static ptls_iovec_t client_alpn[] = {
  { .base = (uint8_t*)ames_alpn, .len = sizeof(ames_alpn) - 1 },
};
static const uint8_t datagram_payload[] = {
  0x61, 0x6d, 0x65, 0x73, 0x2d, 0x71, 0x75, 0x69,
  0x63, 0x2d, 0x6c, 0x6f, 0x6f, 0x70, 0x62, 0x61,
  0x63, 0x6b
};
static const uint8_t reset_token_secret[] = "ames-quic-loopback-reset-secret";

static const char tls_key_pem[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgwEvkGGgXAcRaG7Z8\n"
"gA7C6+W2RsW9gcjV9e5ybr0ikaahRANCAASCo35bDi+Q/q/CzHI1e5QaBrbqbFhW\n"
"G20QbVAeMK8l0oC8OGD3PSpZK1HXwALwzhMuwhxDos3ANb5naa5y17fQ\n"
"-----END PRIVATE KEY-----\n";

static const char tls_cert_pem[] =
"-----BEGIN CERTIFICATE-----\n"
"MIICBzCCAa2gAwIBAgIUd2l6Pce3S0QH3dQC0Q/CjHbmggowCgYIKoZIzj0EAwIw\n"
"WTELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGElu\n"
"dGVybmV0IFdpZGdpdHMgUHR5IEx0ZDESMBAGA1UEAwwJbG9jYWxob3N0MB4XDTI1\n"
"MTExNDExNTcwMFoXDTI1MTIxNDExNTcwMFowWTELMAkGA1UEBhMCQVUxEzARBgNV\n"
"BAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMgUHR5IEx0\n"
"ZDESMBAGA1UEAwwJbG9jYWxob3N0MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE\n"
"gqN+Ww4vkP6vwsxyNXuUGga26mxYVhttEG1QHjCvJdKAvDhg9z0qWStR18AC8M4T\n"
"LsIcQ6LNwDW+Z2mucte30KNTMFEwHQYDVR0OBBYEFFVgXLoLwzpf6+twP5z8Ujr2\n"
"5mxnMB8GA1UdIwQYMBaAFFVgXLoLwzpf6+twP5z8Ujr25mxnMA8GA1UdEwEB/wQF\n"
"MAMBAf8wCgYIKoZIzj0EAwIDSAAwRQIhAO4tnDNRAcooz62vf2m7vTyDqFCjcaIv\n"
"SJ9Gq0lvEXEcAiBwWBNUASBqLaje3hmtgwxcF7EIqqiGo5j8f9Ufgu6SRg==\n"
"-----END CERTIFICATE-----\n";

typedef struct quic_endpoint quic_endpoint;

static ngtcp2_conn*
get_conn_from_ref(ngtcp2_crypto_conn_ref* conn_ref);

typedef struct {
  ngtcp2_tstamp ts;
  quic_endpoint* client;
  quic_endpoint* server;
} loopback_state;

struct quic_endpoint {
  const char* name;
  int is_server;
  ngtcp2_conn* conn;
  ngtcp2_crypto_conn_ref conn_ref;
  ngtcp2_crypto_picotls_ctx cptls;
  ptls_raw_extension_t tls_exts[2];
  ptls_context_t tls_ctx;
  ptls_openssl_sign_certificate_t sign_cert;
  struct sockaddr_storage addr_storage;
  ngtcp2_addr addr;
  int handshake_completed;
  int handshake_confirmed;
  int datagram_seen;
  int echo_seen;
};

static ngtcp2_tstamp
timestamp_now(void)
{
  struct timespec tp;

  if ( 0 != clock_gettime(CLOCK_MONOTONIC, &tp) ) {
    fprintf(stderr, "clock_gettime: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  return ((uint64_t)tp.tv_sec * NGTCP2_SECONDS) + (uint64_t)tp.tv_nsec;
}

static void
init_ipv4_endpoint(quic_endpoint* ep, const char* name, int is_server,
                   uint16_t port)
{
  struct sockaddr_in* sin = (struct sockaddr_in*)&ep->addr_storage;

  memset(ep, 0, sizeof(*ep));
  ep->name = name;
  ep->is_server = is_server;
  ep->conn_ref.get_conn = get_conn_from_ref;
  ep->conn_ref.user_data = ep;

  sin->sin_family = AF_INET;
  sin->sin_port = htons(port);
  sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  ep->addr.addr = (struct sockaddr*)sin;
  ep->addr.addrlen = sizeof(*sin);
}

static ngtcp2_conn*
get_conn_from_ref(ngtcp2_crypto_conn_ref* conn_ref)
{
  quic_endpoint* ep = conn_ref->user_data;
  return ep->conn;
}

static void
rand_cb(uint8_t* dest, size_t destlen, const ngtcp2_rand_ctx* rand_ctx)
{
  (void)rand_ctx;

  if ( 1 != RAND_bytes(dest, (int)destlen) ) {
    abort();
  }
}

static int
get_new_connection_id_cb(ngtcp2_conn* conn, ngtcp2_cid* cid,
                         ngtcp2_stateless_reset_token* token, size_t cidlen,
                         void* user_data)
{
  (void)conn;
  (void)user_data;

  if ( 1 != RAND_bytes(cid->data, (int)cidlen) ) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  cid->datalen = cidlen;

  if ( 1 != RAND_bytes(token->data, sizeof(token->data)) ) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int
handshake_completed_cb(ngtcp2_conn* conn, void* user_data)
{
  quic_endpoint* ep = user_data;
  (void)conn;

  ep->handshake_completed = 1;
  return 0;
}

static int
handshake_confirmed_cb(ngtcp2_conn* conn, void* user_data)
{
  quic_endpoint* ep = user_data;
  (void)conn;

  ep->handshake_confirmed = 1;
  return 0;
}

static int
recv_datagram_cb(ngtcp2_conn* conn, uint32_t flags, const uint8_t* data,
                 size_t datalen, void* user_data)
{
  quic_endpoint* ep = user_data;
  (void)conn;
  (void)flags;

  if ( (datalen != sizeof(datagram_payload)) ||
       (0 != memcmp(data, datagram_payload, sizeof(datagram_payload))) ) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  if ( ep->is_server ) {
    ep->datagram_seen = 1;
  }
  else {
    ep->echo_seen = 1;
  }

  return 0;
}

static int
select_alpn_cb(ptls_on_client_hello_t* self, ptls_t* ptls,
               ptls_on_client_hello_parameters_t* params)
{
  size_t i;
  (void)self;

  for ( i = 0; i < params->negotiated_protocols.count; ++i ) {
    ptls_iovec_t proto = params->negotiated_protocols.list[i];

    if ( (proto.len == sizeof(ames_alpn) - 1) &&
         (0 == memcmp(proto.base, ames_alpn, sizeof(ames_alpn) - 1)) ) {
      return ptls_set_negotiated_protocol(ptls, (const char*)proto.base,
                                          proto.len);
    }
  }

  return PTLS_ALERT_NO_APPLICATION_PROTOCOL;
}

static ptls_on_client_hello_t select_alpn = { select_alpn_cb };

static ptls_key_exchange_algorithm_t* key_exchanges[] = {
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

static ptls_cipher_suite_t* cipher_suites[] = {
  &ptls_openssl_aes128gcmsha256,
  &ptls_openssl_aes256gcmsha384,
#if PTLS_OPENSSL_HAVE_CHACHA20_POLY1305
  &ptls_openssl_chacha20poly1305sha256,
#endif
  NULL
};

static int
init_client_tls_context(quic_endpoint* ep)
{
  ep->tls_ctx = (ptls_context_t){
    .random_bytes = ptls_openssl_random_bytes,
    .get_time = &ptls_get_time,
    .key_exchanges = key_exchanges,
    .cipher_suites = cipher_suites,
    .require_dhe_on_psk = 1,
  };

  if ( 0 != ngtcp2_crypto_picotls_configure_client_context(&ep->tls_ctx) ) {
    fprintf(stderr, "quic-loopback: configure client picotls context failed\n");
    return -1;
  }

  return 0;
}

static EVP_PKEY*
read_private_key_from_memory(const char* pem)
{
  BIO* bio;
  EVP_PKEY* pkey;

  bio = BIO_new_mem_buf(pem, -1);
  if ( NULL == bio ) {
    return NULL;
  }

  pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
  BIO_free(bio);

  return pkey;
}

static X509*
read_cert_from_memory(const char* pem)
{
  BIO* bio;
  X509* cert;

  bio = BIO_new_mem_buf(pem, -1);
  if ( NULL == bio ) {
    return NULL;
  }

  cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
  BIO_free(bio);

  return cert;
}

static int
init_server_tls_context(quic_endpoint* ep)
{
  EVP_PKEY* pkey;
  X509* cert;

  ep->tls_ctx = (ptls_context_t){
    .random_bytes = ptls_openssl_random_bytes,
    .get_time = &ptls_get_time,
    .key_exchanges = key_exchanges,
    .cipher_suites = cipher_suites,
    .require_dhe_on_psk = 1,
    .server_cipher_preference = 1,
    .on_client_hello = &select_alpn,
  };

  if ( 0 != ngtcp2_crypto_picotls_configure_server_context(&ep->tls_ctx) ) {
    fprintf(stderr, "quic-loopback: configure server picotls context failed\n");
    return -1;
  }

  pkey = read_private_key_from_memory(tls_key_pem);
  if ( NULL == pkey ) {
    fprintf(stderr, "quic-loopback: could not parse test private key\n");
    return -1;
  }

  cert = read_cert_from_memory(tls_cert_pem);
  if ( NULL == cert ) {
    EVP_PKEY_free(pkey);
    fprintf(stderr, "quic-loopback: could not parse test certificate\n");
    return -1;
  }

  if ( 0 != ptls_openssl_load_certificates(&ep->tls_ctx, cert, NULL) ) {
    X509_free(cert);
    EVP_PKEY_free(pkey);
    fprintf(stderr, "quic-loopback: could not load test certificate\n");
    return -1;
  }

  if ( 0 != ptls_openssl_init_sign_certificate(&ep->sign_cert, pkey) ) {
    X509_free(cert);
    EVP_PKEY_free(pkey);
    fprintf(stderr, "quic-loopback: could not load test signing key\n");
    return -1;
  }

  ep->tls_ctx.sign_certificate = &ep->sign_cert.super;

  X509_free(cert);
  EVP_PKEY_free(pkey);

  return 0;
}

static void
init_transport_params(ngtcp2_transport_params* params)
{
  ngtcp2_transport_params_default(params);

  params->initial_max_data = 1024 * 1024;
  params->initial_max_stream_data_bidi_local = 128 * 1024;
  params->initial_max_stream_data_bidi_remote = 128 * 1024;
  params->initial_max_stream_data_uni = 128 * 1024;
  params->initial_max_streams_bidi = 4;
  params->initial_max_streams_uni = 4;
  params->max_datagram_frame_size = DATAGRAM_FRAME_SIZE;
}

static void
init_settings(ngtcp2_settings* settings, ngtcp2_tstamp ts)
{
  ngtcp2_settings_default(settings);
  settings->initial_ts = ts;
}

static ngtcp2_callbacks
client_callbacks(void)
{
  return (ngtcp2_callbacks){
    .client_initial = ngtcp2_crypto_client_initial_cb,
    .recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb,
    .handshake_completed = handshake_completed_cb,
    .encrypt = ngtcp2_crypto_encrypt_cb,
    .decrypt = ngtcp2_crypto_decrypt_cb,
    .hp_mask = ngtcp2_crypto_hp_mask_cb,
    .recv_retry = ngtcp2_crypto_recv_retry_cb,
    .rand = rand_cb,
    .update_key = ngtcp2_crypto_update_key_cb,
    .delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    .delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    .recv_datagram = recv_datagram_cb,
    .handshake_confirmed = handshake_confirmed_cb,
    .version_negotiation = ngtcp2_crypto_version_negotiation_cb,
    .get_new_connection_id2 = get_new_connection_id_cb,
    .get_path_challenge_data2 = ngtcp2_crypto_get_path_challenge_data2_cb,
  };
}

static ngtcp2_callbacks
server_callbacks(void)
{
  return (ngtcp2_callbacks){
    .recv_client_initial = ngtcp2_crypto_recv_client_initial_cb,
    .recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb,
    .handshake_completed = handshake_completed_cb,
    .encrypt = ngtcp2_crypto_encrypt_cb,
    .decrypt = ngtcp2_crypto_decrypt_cb,
    .hp_mask = ngtcp2_crypto_hp_mask_cb,
    .rand = rand_cb,
    .update_key = ngtcp2_crypto_update_key_cb,
    .delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    .delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    .recv_datagram = recv_datagram_cb,
    .version_negotiation = ngtcp2_crypto_version_negotiation_cb,
    .get_new_connection_id2 = get_new_connection_id_cb,
    .get_path_challenge_data2 = ngtcp2_crypto_get_path_challenge_data2_cb,
  };
}

static int
init_client_conn(quic_endpoint* client, const quic_endpoint* server,
                 ngtcp2_cid* odcid, ngtcp2_cid* client_scid,
                 ngtcp2_tstamp ts)
{
  ngtcp2_path path = {
    .local = client->addr,
    .remote = server->addr,
  };
  ngtcp2_callbacks callbacks = client_callbacks();
  ngtcp2_settings settings;
  ngtcp2_transport_params params;
  int rv;

  odcid->datalen = CIDLEN;
  client_scid->datalen = 0;

  if ( 1 != RAND_bytes(odcid->data, (int)odcid->datalen) ) {
    fprintf(stderr, "quic-loopback: RAND_bytes odcid failed\n");
    return -1;
  }

  if ( (0 != client_scid->datalen) &&
       (1 != RAND_bytes(client_scid->data, (int)client_scid->datalen)) ) {
    fprintf(stderr, "quic-loopback: RAND_bytes client scid failed\n");
    return -1;
  }

  init_settings(&settings, ts);
  init_transport_params(&params);

  rv = ngtcp2_conn_client_new(&client->conn, odcid, client_scid, &path,
                              NGTCP2_PROTO_VER_V1, &callbacks, &settings,
                              &params, NULL, client);
  if ( 0 != rv ) {
    fprintf(stderr, "quic-loopback: ngtcp2_conn_client_new: %s\n",
            ngtcp2_strerror(rv));
    return -1;
  }

  ngtcp2_crypto_picotls_ctx_init(&client->cptls);
  client->cptls.ptls = ptls_client_new(&client->tls_ctx);
  if ( NULL == client->cptls.ptls ) {
    fprintf(stderr, "quic-loopback: ptls_client_new failed\n");
    return -1;
  }

  *ptls_get_data_ptr(client->cptls.ptls) = &client->conn_ref;
  client->cptls.handshake_properties.additional_extensions =
    client->tls_exts;
  client->tls_exts[0] = (ptls_raw_extension_t){ .type = UINT16_MAX };
  client->tls_exts[1] = (ptls_raw_extension_t){ .type = UINT16_MAX };

  rv = ngtcp2_crypto_picotls_configure_client_session(&client->cptls,
                                                      client->conn);
  if ( 0 != rv ) {
    fprintf(stderr, "quic-loopback: configure client picotls session failed\n");
    return -1;
  }

  client->cptls.handshake_properties.client.negotiated_protocols.list =
    client_alpn;
  client->cptls.handshake_properties.client.negotiated_protocols.count = 1;
  ptls_set_server_name(client->cptls.ptls, "localhost",
                       strlen("localhost"));

  ngtcp2_conn_set_tls_native_handle(client->conn, &client->cptls);

  return 0;
}

static int
init_server_conn(quic_endpoint* server, const quic_endpoint* client,
                 const ngtcp2_cid* odcid, const ngtcp2_cid* client_scid,
                 ngtcp2_tstamp ts)
{
  ngtcp2_cid dcid;
  ngtcp2_cid scid;
  ngtcp2_path path = {
    .local = server->addr,
    .remote = client->addr,
  };
  ngtcp2_callbacks callbacks = server_callbacks();
  ngtcp2_settings settings;
  ngtcp2_transport_params params;
  int rv;

  ngtcp2_cid_init(&dcid, client_scid->data, client_scid->datalen);

  scid.datalen = CIDLEN;
  if ( 1 != RAND_bytes(scid.data, (int)scid.datalen) ) {
    fprintf(stderr, "quic-loopback: RAND_bytes server scid failed\n");
    return -1;
  }

  init_settings(&settings, ts);
  init_transport_params(&params);
  params.original_dcid = *odcid;
  params.original_dcid_present = 1;

  if ( 0 != ngtcp2_crypto_generate_stateless_reset_token(
              params.stateless_reset_token, reset_token_secret,
              sizeof(reset_token_secret) - 1, &scid) ) {
    fprintf(stderr, "quic-loopback: stateless reset token failed\n");
    return -1;
  }

  rv = ngtcp2_conn_server_new(&server->conn, &dcid, &scid, &path,
                              NGTCP2_PROTO_VER_V1, &callbacks, &settings,
                              &params, NULL, server);
  if ( 0 != rv ) {
    fprintf(stderr, "quic-loopback: ngtcp2_conn_server_new: %s\n",
            ngtcp2_strerror(rv));
    return -1;
  }

  ngtcp2_crypto_picotls_ctx_init(&server->cptls);
  server->cptls.ptls = ptls_server_new(&server->tls_ctx);
  if ( NULL == server->cptls.ptls ) {
    fprintf(stderr, "quic-loopback: ptls_server_new failed\n");
    return -1;
  }

  *ptls_get_data_ptr(server->cptls.ptls) = &server->conn_ref;
  server->cptls.handshake_properties.additional_extensions =
    server->tls_exts;
  server->tls_exts[0] = (ptls_raw_extension_t){ .type = UINT16_MAX };
  server->tls_exts[1] = (ptls_raw_extension_t){ .type = UINT16_MAX };

  rv = ngtcp2_crypto_picotls_configure_server_session(&server->cptls);
  if ( 0 != rv ) {
    fprintf(stderr, "quic-loopback: configure server picotls session failed\n");
    return -1;
  }

  ngtcp2_conn_set_tls_native_handle(server->conn, &server->cptls);

  return 0;
}

static int
handle_expiry(quic_endpoint* ep, ngtcp2_tstamp ts)
{
  int rv = ngtcp2_conn_handle_expiry(ep->conn, ts);

  if ( 0 != rv ) {
    fprintf(stderr, "quic-loopback: %s handle expiry: %s\n", ep->name,
            ngtcp2_strerror(rv));
    return -1;
  }

  return 0;
}

static int
deliver_packet(quic_endpoint* sender, quic_endpoint* receiver,
               const uint8_t* packet, size_t packet_len, ngtcp2_tstamp ts)
{
  ngtcp2_path path = {
    .local = receiver->addr,
    .remote = sender->addr,
  };
  ngtcp2_pkt_info pi = { 0 };
  int rv;

  rv = ngtcp2_conn_read_pkt(receiver->conn, &path, &pi, packet, packet_len, ts);
  if ( 0 != rv ) {
    fprintf(stderr, "quic-loopback: %s read packet from %s: %s",
            receiver->name, sender->name, ngtcp2_strerror(rv));
    if ( NGTCP2_ERR_CRYPTO == rv ) {
      fprintf(stderr, " (tls alert=%u)",
              ngtcp2_conn_get_tls_alert2(receiver->conn));
    }
    fputc('\n', stderr);
    return -1;
  }

  return 0;
}

static int
pump_one(quic_endpoint* sender, quic_endpoint* receiver, loopback_state* state,
         int* progressed)
{
  uint8_t buf[MAX_UDP_PAYLOAD_SIZE];
  ngtcp2_path_storage ps;
  ngtcp2_pkt_info pi = { 0 };
  ngtcp2_ssize nwrite;

  ngtcp2_path_storage_zero(&ps);
  nwrite = ngtcp2_conn_write_pkt(sender->conn, &ps.path, &pi, buf,
                                 sizeof(buf), state->ts);
  if ( nwrite < 0 ) {
    fprintf(stderr, "quic-loopback: %s write packet: %s\n",
            sender->name, ngtcp2_strerror((int)nwrite));
    return -1;
  }

  if ( 0 == nwrite ) {
    return 0;
  }

  *progressed = 1;

  if ( 0 != deliver_packet(sender, receiver, buf, (size_t)nwrite,
                           state->ts) ) {
    return -1;
  }

  state->ts += NGTCP2_MILLISECONDS;
  return 0;
}

static int
pump_packets(loopback_state* state)
{
  int progressed = 0;

  if ( 0 != pump_one(state->client, state->server, state, &progressed) ) {
    return -1;
  }

  if ( 0 != pump_one(state->server, state->client, state, &progressed) ) {
    return -1;
  }

  if ( !progressed ) {
    ngtcp2_tstamp client_expiry =
      ngtcp2_conn_get_expiry2(state->client->conn);
    ngtcp2_tstamp server_expiry =
      ngtcp2_conn_get_expiry2(state->server->conn);
    ngtcp2_tstamp next_expiry =
      client_expiry < server_expiry ? client_expiry : server_expiry;

    if ( UINT64_MAX == next_expiry ) {
      return 0;
    }

    if ( state->ts < next_expiry ) {
      state->ts = next_expiry;
    }

    if ( client_expiry <= state->ts ) {
      if ( 0 != handle_expiry(state->client, state->ts) ) {
        return -1;
      }
      progressed = 1;
    }

    if ( server_expiry <= state->ts ) {
      if ( 0 != handle_expiry(state->server, state->ts) ) {
        return -1;
      }
      progressed = 1;
    }
  }

  return progressed;
}

static int
run_handshake(loopback_state* state)
{
  size_t i;

  for ( i = 0; i < 1000; ++i ) {
    int rv = pump_packets(state);
    if ( rv < 0 ) {
      return -1;
    }

    if ( ngtcp2_conn_get_handshake_completed2(state->client->conn) &&
         ngtcp2_conn_get_handshake_completed2(state->server->conn) ) {
      return 0;
    }

    if ( 0 == rv ) {
      fprintf(stderr, "quic-loopback: handshake stalled\n");
      return -1;
    }
  }

  fprintf(stderr, "quic-loopback: handshake timed out\n");
  return -1;
}

static int
write_datagram(quic_endpoint* sender, quic_endpoint* receiver,
               loopback_state* state, uint64_t dgram_id)
{
  uint8_t buf[MAX_UDP_PAYLOAD_SIZE];
  ngtcp2_path_storage ps;
  ngtcp2_pkt_info pi = { 0 };
  ngtcp2_ssize nwrite;
  int accepted = 0;

  ngtcp2_path_storage_zero(&ps);
  nwrite = ngtcp2_conn_write_datagram(sender->conn, &ps.path, &pi, buf,
                                      sizeof(buf), &accepted,
                                      NGTCP2_WRITE_DATAGRAM_FLAG_NONE,
                                      dgram_id, datagram_payload,
                                      sizeof(datagram_payload), state->ts);
  if ( nwrite < 0 ) {
    fprintf(stderr, "quic-loopback: %s write datagram: %s\n",
            sender->name, ngtcp2_strerror((int)nwrite));
    return -1;
  }

  if ( (0 == nwrite) || !accepted ) {
    fprintf(stderr, "quic-loopback: %s datagram was not accepted\n",
            sender->name);
    return -1;
  }

  if ( 0 != deliver_packet(sender, receiver, buf, (size_t)nwrite,
                           state->ts) ) {
    return -1;
  }

  state->ts += NGTCP2_MILLISECONDS;
  return 0;
}

static int
drain_packets(loopback_state* state, size_t limit)
{
  size_t i;

  for ( i = 0; i < limit; ++i ) {
    int rv = pump_packets(state);
    if ( rv < 0 ) {
      return -1;
    }
    if ( 0 == rv ) {
      return 0;
    }
  }

  return 0;
}

static void
free_endpoint(quic_endpoint* ep)
{
  size_t i;

  if ( NULL != ep->conn ) {
    ngtcp2_conn_del(ep->conn);
    ep->conn = NULL;
  }

  ngtcp2_crypto_picotls_deconfigure_session(&ep->cptls);

  if ( NULL != ep->cptls.ptls ) {
    ptls_free(ep->cptls.ptls);
    ep->cptls.ptls = NULL;
  }

  if ( NULL != ep->sign_cert.key ) {
    ptls_openssl_dispose_sign_certificate(&ep->sign_cert);
    ep->sign_cert.key = NULL;
  }

  for ( i = 0; i < ep->tls_ctx.certificates.count; ++i ) {
    free(ep->tls_ctx.certificates.list[i].base);
  }
  free(ep->tls_ctx.certificates.list);
  ep->tls_ctx.certificates.list = NULL;
  ep->tls_ctx.certificates.count = 0;
}

int
main(void)
{
  const ngtcp2_info* qif = ngtcp2_version(0);
  const nghttp3_info* hif = nghttp3_version(0);
  quic_endpoint client;
  quic_endpoint server;
  ngtcp2_cid odcid;
  ngtcp2_cid client_scid;
  loopback_state state;
  int rc = 1;

  if ( (NULL == qif) || (NULL == hif) ) {
    fprintf(stderr, "quic-loopback: version API failed\n");
    return 1;
  }

  if ( strcmp(qif->version_str, NGTCP2_VERSION) ||
       strcmp(hif->version_str, NGHTTP3_VERSION) ) {
    fprintf(stderr, "quic-loopback: linked headers/library mismatch\n");
    return 1;
  }

  init_ipv4_endpoint(&client, "client", 0, 12345);
  init_ipv4_endpoint(&server, "server", 1, 4433);

  if ( (0 != init_client_tls_context(&client)) ||
       (0 != init_server_tls_context(&server)) ) {
    goto cleanup;
  }

  state = (loopback_state){
    .ts = timestamp_now(),
    .client = &client,
    .server = &server,
  };

  if ( 0 != init_client_conn(&client, &server, &odcid, &client_scid,
                             state.ts) ) {
    goto cleanup;
  }

  if ( 0 != init_server_conn(&server, &client, &odcid, &client_scid,
                             state.ts) ) {
    goto cleanup;
  }

  if ( 0 != run_handshake(&state) ) {
    goto cleanup;
  }

  if ( 0 != drain_packets(&state, 16) ) {
    goto cleanup;
  }

  if ( 0 != write_datagram(&client, &server, &state, 1) ) {
    goto cleanup;
  }

  if ( !server.datagram_seen ) {
    fprintf(stderr, "quic-loopback: server did not receive datagram\n");
    goto cleanup;
  }

  if ( 0 != write_datagram(&server, &client, &state, 2) ) {
    goto cleanup;
  }

  if ( !client.echo_seen ) {
    fprintf(stderr, "quic-loopback: client did not receive datagram echo\n");
    goto cleanup;
  }

  printf("quic-loopback: ngtcp2 %s, nghttp3 %s, picotls handshake + DATAGRAM echo ok\n",
         qif->version_str, hif->version_str);
  rc = 0;

cleanup:
  free_endpoint(&server);
  free_endpoint(&client);
  return rc;
}
