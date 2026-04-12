/// @file

#include "noun.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 24);
}

static u3_noun
_nock_fol(u3_noun fol)
{
  return u3n_nock_on(u3_nul, fol);
}

static c3_w _capture_log_count_w;
static c3_c _capture_log_last_c[4096];

static void
_capture_log_sink(c3_c* msg_c)
{
  if ( 0 == strncmp(msg_c, "uridian-capture:", 16) ) {
    _capture_log_count_w += 1;
    strncpy(_capture_log_last_c, msg_c, sizeof(_capture_log_last_c) - 1);
    _capture_log_last_c[sizeof(_capture_log_last_c) - 1] = '\0';
  }
}

static c3_w
_capture_store_file_count(const c3_c* dir_c)
{
  DIR* dir_u = opendir(dir_c);
  struct dirent* ent_u;
  c3_w count_w = 0;

  if ( 0 == dir_u ) {
    return 0;
  }
  while ( 0 != (ent_u = readdir(dir_u)) ) {
    if ( 0 == strcmp(ent_u->d_name, ".") || 0 == strcmp(ent_u->d_name, "..") ) {
      continue;
    }
    count_w += 1;
  }
  closedir(dir_u);
  return count_w;
}

static c3_i
_test_nock_meme(void)
{
  //  (jam !=(=(~ =|(i=@ |-(?:(=(i ^~((bex 32))) ~ [i $(i +(i))]))))))
  //
  const c3_y buf_y[] = {
    0xe1, 0x16, 0x1b,  0x4, 0x1b, 0xe1, 0x20, 0x58, 0x1c, 0x76, 0x4d, 0x96, 0xd8,
    0x31, 0x60,  0x0,  0x0,  0x0,  0x0, 0xd8,  0x8, 0x37, 0xce,  0xd, 0x92, 0x21,
    0x83, 0x68, 0x61, 0x87, 0x39, 0xce, 0x4d,  0xe, 0x92, 0x21, 0x87, 0x19,  0x8
  };
  u3_noun fol = u3s_cue_bytes(sizeof(buf_y), buf_y);
  u3_noun gon;
  c3_w    i_w;
  c3_i  ret_i = 1;

  for ( i_w = 0; i_w < 3; i_w++ ) {
    gon = u3m_soft(0, _nock_fol, u3k(fol));

    if ( c3n == u3r_p(gon, c3__meme, 0) ) {
      u3m_p("nock meme unexpected mote", u3h(gon));
      ret_i = 0;
      u3z(gon);
      break;
    }

    u3z(gon);
  }

  u3z(fol);

  return ret_i;
}

static c3_i
_test_meme(void)
{
  c3_i ret_i = 1;

  if ( !_test_nock_meme() ) {
    fprintf(stderr, "test nock meme: failed\r\n");
    ret_i = 0;
  }

  return ret_i;
}

static c3_i
_test_uridian_capture(void)
{
  u3_noun fol = u3nt(9, 2, u3nc(1, u3nc(u3nc(0, 3), u3nc(123, 456))));
  u3_noun cap = u3n_etch_capture(0, fol);
  c3_c* pre_c = u3m_pretty(cap);
  c3_i ret_i = 1;
  const c3_c* wan_c =
    "[%uridian-capture '{[libl i:0] [ticb 0] halt}' [[[0 3] 123 456] 0] [[2 0] 0] 0 0 [1 0] 0]";

  if ( 0 != strcmp(pre_c, wan_c) ) {
    fprintf(stderr, "test uridian capture: unexpected output\r\n");
    fprintf(stderr, "want: %s\r\n", wan_c);
    fprintf(stderr, "have: %s\r\n", pre_c);
    ret_i = 0;
  }

  c3_free(pre_c);
  u3z(cap);
  u3z(fol);
  return ret_i;
}

static c3_i
_test_uridian_prog_capture(void)
{
  u3_noun fol = u3nt(9, 2, u3nc(1, u3nc(u3nc(0, 3), u3nc(123, 456))));
  u3p(u3n_prog) pog_p = u3n_find(u3_nul, fol);
  u3_noun pro = u3n_burn(pog_p, 0);
  u3_noun cap = u3n_etch_prog_capture(0, pog_p);
  c3_c* pre_c = u3m_pretty(cap);
  c3_i ret_i = 1;

  if ( 0 == strstr(pre_c, "%uridian-callsite") ) {
    fprintf(stderr, "test uridian prog capture: missing rich callsite\r\n");
    fprintf(stderr, "have: %s\r\n", pre_c);
    ret_i = 0;
  }
  else if ( 0 == strstr(pre_c, "%uridian-capture") ) {
    fprintf(stderr, "test uridian prog capture: missing nested capture\r\n");
    fprintf(stderr, "have: %s\r\n", pre_c);
    ret_i = 0;
  }

  c3_free(pre_c);
  u3z(cap);
  u3z(pro);
  u3z(fol);
  return ret_i;
}

static c3_i
_test_uridian_runtime_capture(void)
{
  u3_noun fol = u3nc(4, u3nc(0, 1));
  u3_noun gat = u3nt(9, 2, u3nc(1, u3nc(u3nc(0, 3), u3nc(123, 456))));
  c3_c tem_c[] = "/tmp/uridian-store-XXXXXX";
  c3_c* sto_c = mkdtemp(tem_c);
  void (*old_log_f)(c3_c*) = u3C.stderr_log_f;
  c3_i ret_i = 1;
  u3_noun pro;

  if ( 0 == sto_c ) {
    fprintf(stderr, "test uridian runtime capture: could not make temp dir\r\n");
    return 0;
  }

  setenv("URIDIAN_CAPTURE", "1", 1);
  setenv("URIDIAN_CAPTURE_RATE", "1", 1);
  unsetenv("URIDIAN_CAPTURE_SUBJECT");

  _capture_log_count_w = 0;
  _capture_log_last_c[0] = '\0';
  u3C.stderr_log_f = _capture_log_sink;

  pro = u3n_nock_on(42, u3k(fol));
  if ( 43 != pro ) {
    fprintf(stderr, "test uridian runtime capture: unexpected product (no subject)\r\n");
    ret_i = 0;
  }
  else if ( 0 == _capture_log_count_w ) {
    fprintf(stderr, "test uridian runtime capture: no sampled capture logged\r\n");
    ret_i = 0;
  }
  else if ( 0 == strstr(_capture_log_last_c, "%uridian-capture") ) {
    fprintf(stderr, "test uridian runtime capture: bad capture line\r\n");
    fprintf(stderr, "have: %s\r\n", _capture_log_last_c);
    ret_i = 0;
  }
  else if ( 0 != strstr(_capture_log_last_c, "[1 42]") ) {
    fprintf(stderr, "test uridian runtime capture: subject should be omitted by default\r\n");
    fprintf(stderr, "have: %s\r\n", _capture_log_last_c);
    ret_i = 0;
  }
  u3z(pro);

  _capture_log_count_w = 0;
  _capture_log_last_c[0] = '\0';
  pro = u3n_nock_on(0, u3k(gat));
  {
    u3_noun wan = u3nc(123, 456);
    if ( c3n == u3r_sing(pro, wan) ) {
      fprintf(stderr, "test uridian runtime capture: unexpected product (kick fallback)\r\n");
      ret_i = 0;
    }
    u3z(wan);
  }
  if ( _capture_log_count_w < 2 ) {
    fprintf(stderr, "test uridian runtime capture: kick fallback should log multiple captures\r\n");
    ret_i = 0;
  }
  u3z(pro);

  setenv("URIDIAN_CAPTURE_STORE", sto_c, 1);
  unsetenv("URIDIAN_CAPTURE_SUBJECT");
  _capture_log_count_w = 0;
  _capture_log_last_c[0] = '\0';

  pro = u3n_nock_on(42, u3k(fol));
  if ( 43 != pro ) {
    fprintf(stderr, "test uridian runtime capture: unexpected product (with subject store)\r\n");
    ret_i = 0;
  }
  else if ( 0 == _capture_log_count_w ) {
    fprintf(stderr, "test uridian runtime capture: no sampled capture logged with subject store\r\n");
    ret_i = 0;
  }
  else if ( 0 == strstr(_capture_log_last_c, "textref-") ) {
    fprintf(stderr, "test uridian runtime capture: subject ref missing from capture line\r\n");
    fprintf(stderr, "have: %s\r\n", _capture_log_last_c);
    ret_i = 0;
  }
  else if ( 0 == _capture_store_file_count(sto_c) ) {
    fprintf(stderr, "test uridian runtime capture: subject store should contain at least one file\r\n");
    ret_i = 0;
  }
  else if ( 0 != strstr(_capture_log_last_c, "[1 42]") ) {
    fprintf(stderr, "test uridian runtime capture: inline subject should still be omitted by default with store\r\n");
    fprintf(stderr, "have: %s\r\n", _capture_log_last_c);
    ret_i = 0;
  }
  u3z(pro);

  setenv("URIDIAN_CAPTURE_SUBJECT", "1", 1);
  _capture_log_count_w = 0;
  _capture_log_last_c[0] = '\0';

  pro = u3n_nock_on(42, u3k(fol));
  if ( 43 != pro ) {
    fprintf(stderr, "test uridian runtime capture: unexpected product (with subject)\r\n");
    ret_i = 0;
  }
  else if ( 0 == _capture_log_count_w ) {
    fprintf(stderr, "test uridian runtime capture: no sampled capture logged with subject\r\n");
    ret_i = 0;
  }
  else if ( 0 == strstr(_capture_log_last_c, "[1 42]") ) {
    fprintf(stderr, "test uridian runtime capture: subject missing when requested\r\n");
    fprintf(stderr, "have: %s\r\n", _capture_log_last_c);
    ret_i = 0;
  }
  u3z(pro);

  unsetenv("URIDIAN_CAPTURE");
  unsetenv("URIDIAN_CAPTURE_RATE");
  unsetenv("URIDIAN_CAPTURE_SUBJECT");
  unsetenv("URIDIAN_CAPTURE_STORE");
  u3C.stderr_log_f = old_log_f;
  u3z(fol);
  u3z(gat);
  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_uridian_runtime_capture() ) {
    fprintf(stderr, "test uridian runtime capture: failed\r\n");
    exit(1);
  }

  if ( !_test_meme() ) {
    fprintf(stderr, "test meme: failed\r\n");
    exit(1);
  }

  if ( !_test_uridian_capture() ) {
    fprintf(stderr, "test uridian capture: failed\r\n");
    exit(1);
  }

  if ( !_test_uridian_prog_capture() ) {
    fprintf(stderr, "test uridian prog capture: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test nock: ok\r\n");
  return 0;
}
