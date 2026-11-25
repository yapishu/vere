/// @file

#include "noun.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 20);
  u3m_pave(c3y);
}

static inline c3_i
_ud_etch(c3_d num_d, const c3_c* num_c)
{
  u3_atom  num = u3i_chub(num_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_ud_c(num, &out_c);
  c3_i   ret_i = 1;

  if ( 0 != strcmp(num_c, out_c) ) {
    fprintf(stderr, "etch_ud: %" PRIu64 " fail; expected %s, got '%s'\r\n",
                    num_d, num_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_ud(num);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_ud: %" PRIu64 " mismatch; expected %s\r\n", num_d, num_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(num);

  return ret_i;
}

static c3_i
_test_etch_ud(void)
{
  c3_i ret_i = 1;

  ret_i &= _ud_etch(0, "0");
  ret_i &= _ud_etch(1, "1");
  ret_i &= _ud_etch(12, "12");
  ret_i &= _ud_etch(123, "123");
  ret_i &= _ud_etch(1234, "1.234");
  ret_i &= _ud_etch(12345, "12.345");
  ret_i &= _ud_etch(123456, "123.456");
  ret_i &= _ud_etch(1234567, "1.234.567");
  ret_i &= _ud_etch(12345678, "12.345.678");
  ret_i &= _ud_etch(123456789, "123.456.789");
  ret_i &= _ud_etch(100000000, "100.000.000");
  ret_i &= _ud_etch(101101101, "101.101.101");
  ret_i &= _ud_etch(201201201, "201.201.201");
  ret_i &= _ud_etch(302201100, "302.201.100");

  ret_i &= _ud_etch(8589934592ULL, "8.589.934.592");
  ret_i &= _ud_etch(2305843009213693952ULL, "2.305.843.009.213.693.952");
  ret_i &= _ud_etch(18446744073709551615ULL, "18.446.744.073.709.551.615");

  {
    c3_c* num_c = "340.282.366.920.938.463.463.374.607.431.768.211.456";
    u3_atom num = u3qc_bex(128);
    c3_c*  out_c;
    size_t len_i = u3s_etch_ud_c(num, &out_c);

    if ( 0 != strncmp(num_c, out_c, len_i) ) {
      fprintf(stderr, "etch_ud: (bex 128) fail; expected %s, got '%s'\r\n",
                      num_c, out_c);
      ret_i = 0;
    }
    else {
      u3_noun out = u3s_etch_ud(num);
      u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

      if ( c3n == u3r_sing(tou, out) ) {
        fprintf(stderr, "etch_ud: (bex 128) mismatch; expected %s\r\n", num_c);
        u3m_p("out", out);
        ret_i = 0;
      }

      u3z(out);
      u3z(tou);
    }

    c3_free(out_c);
    u3z(num);
  }

  return ret_i;
}

static inline c3_i
_ux_etch(c3_d num_d, const c3_c* num_c)
{
  u3_atom  num = u3i_chub(num_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_ux_c(num, &out_c);
  c3_i   ret_i = 1;

  if ( 0 != strcmp(num_c, out_c) ) {
    fprintf(stderr, "etch_ux: 0x%" PRIx64 " fail; expected %s, got '%s'\r\n",
                    num_d, num_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_ux(num);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_ux: 0x%" PRIx64 " mismatch; expected %s\r\n", num_d, num_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(num);

  return ret_i;
}

static c3_i
_test_etch_ux(void)
{
  c3_i ret_i = 1;

  ret_i &= _ux_etch(0x0, "0x0");
  ret_i &= _ux_etch(0x1, "0x1");
  ret_i &= _ux_etch(0x12, "0x12");
  ret_i &= _ux_etch(0x123, "0x123");
  ret_i &= _ux_etch(0x1234, "0x1234");
  ret_i &= _ux_etch(0x12345, "0x1.2345");
  ret_i &= _ux_etch(0x123456, "0x12.3456");
  ret_i &= _ux_etch(0x1234567, "0x123.4567");
  ret_i &= _ux_etch(0x12345678, "0x1234.5678");
  ret_i &= _ux_etch(0x123456789, "0x1.2345.6789");
  ret_i &= _ux_etch(0x100000000, "0x1.0000.0000");
  ret_i &= _ux_etch(0x101101101, "0x1.0110.1101");
  ret_i &= _ux_etch(0x201201201, "0x2.0120.1201");
  ret_i &= _ux_etch(0x302201100, "0x3.0220.1100");

  ret_i &= _ux_etch(0x123456789abcdefULL, "0x123.4567.89ab.cdef");
  ret_i &= _ux_etch(0x8589934592ULL, "0x85.8993.4592");
  ret_i &= _ux_etch(0x5843009213693952ULL, "0x5843.0092.1369.3952");
  ret_i &= _ux_etch(0x6744073709551615ULL, "0x6744.0737.0955.1615");

  {
    c3_c* num_c = "0x1.0000.0000.0000.0000.0000.0000.0000.0000";
    u3_atom num = u3qc_bex(128);
    c3_c*  out_c;
    size_t len_i = u3s_etch_ux_c(num, &out_c);

    if ( 0 != strncmp(num_c, out_c, len_i) ) {
      fprintf(stderr, "etch_ux: (bex 128) fail; expected %s, got '%s'\r\n",
                      num_c, out_c);
      ret_i = 0;
    }
    else {
      u3_noun out = u3s_etch_ux(num);
      u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

      if ( c3n == u3r_sing(tou, out) ) {
        fprintf(stderr, "etch_ux: (bex 128) mismatch; expected %s\r\n", num_c);
        u3m_p("out", out);
        ret_i = 0;
      }

      u3z(out);
      u3z(tou);
    }

    c3_free(out_c);
    u3z(num);
  }

  return ret_i;
}

static inline c3_i
_uv_etch(c3_d num_d, const c3_c* num_c)
{
  u3_atom  num = u3i_chub(num_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_uv_c(num, &out_c);
  c3_i   ret_i = 1;

  if ( 0 != strcmp(num_c, out_c) ) {
    fprintf(stderr, "etch_uv: 0x%" PRIx64 " fail; expected %s, got '%s'\r\n",
                    num_d, num_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_uv(num);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_uv: 0x%" PRIx64 " mismatch; expected %s\r\n", num_d, num_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(num);

  return ret_i;
}

static c3_i
_test_etch_uv(void)
{
  c3_i ret_i = 1;

  ret_i &= _uv_etch(0x0, "0v0");
  ret_i &= _uv_etch(0x1, "0v1");
  ret_i &= _uv_etch(0x10, "0vg");
  ret_i &= _uv_etch(0x12, "0vi");
  ret_i &= _uv_etch(0x123, "0v93");
  ret_i &= _uv_etch(0x1234, "0v4hk");
  ret_i &= _uv_etch(0x12345, "0v28q5");
  ret_i &= _uv_etch(0x123456, "0v14d2m");
  ret_i &= _uv_etch(0x1234567, "0vi6hb7");
  ret_i &= _uv_etch(0x12345678, "0v9.38ljo");
  ret_i &= _uv_etch(0x123456789, "0v4h.kaps9");
  ret_i &= _uv_etch(0x100000000, "0v40.00000");
  ret_i &= _uv_etch(0x101101101, "0v40.h0481");
  ret_i &= _uv_etch(0x201201201, "0v80.i04g1");
  ret_i &= _uv_etch(0x302201100, "0vc1.20480");

  ret_i &= _uv_etch(0x123456789abcdefULL, "0v28.q5cu4.qnjff");
  ret_i &= _uv_etch(0x8589934592ULL, "0vgm4.p6hci");
  ret_i &= _uv_etch(0x5843009213693952ULL, "0v5gg.o0i89.mieai");
  ret_i &= _uv_etch(0x6744073709551615ULL, "0v6eh.076s4.la5gl");

  {
    c3_c* num_c = "0v8.00000.00000.00000.00000.00000";
    u3_atom num = u3qc_bex(128);
    c3_c*  out_c;
    size_t len_i = u3s_etch_uv_c(num, &out_c);

    if ( 0 != strncmp(num_c, out_c, len_i) ) {
      fprintf(stderr, "etch_uv: (bex 128) fail; expected %s, got '%s'\r\n",
                      num_c, out_c);
      ret_i = 0;
    }
    else {
      u3_noun out = u3s_etch_uv(num);
      u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

      if ( c3n == u3r_sing(tou, out) ) {
      //   fprintf(stderr, "etch_uv: (bex 128) mismatch; expected %s\r\n", num_c);
        u3m_p("out", out);
        ret_i = 0;
      }

      u3z(out);
      u3z(tou);
    }

    c3_free(out_c);
    u3z(num);
  }

  return ret_i;
}

static inline c3_i
_uw_etch(c3_d num_d, const c3_c* num_c)
{
  u3_atom  num = u3i_chub(num_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_uw_c(num, &out_c);
  c3_i   ret_i = 1;

  if ( 0 != strcmp(num_c, out_c) ) {
    fprintf(stderr, "etch_uw: 0x%" PRIx64 " fail; expected %s, got '%s'\r\n",
                    num_d, num_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_uw(num);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_uw: 0x%" PRIx64 " mismatch; expected %s\r\n", num_d, num_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(num);

  return ret_i;
}

static c3_i
_test_etch_uw(void)
{
  c3_i ret_i = 1;

  ret_i &= _uw_etch(0x0, "0w0");
  ret_i &= _uw_etch(0x1, "0w1");
  ret_i &= _uw_etch(0x10, "0wg");
  ret_i &= _uw_etch(0x12, "0wi");
  ret_i &= _uw_etch(0x123, "0w4z");
  ret_i &= _uw_etch(0x1234, "0w18Q");
  ret_i &= _uw_etch(0x12345, "0wid5");
  ret_i &= _uw_etch(0x123456, "0w4zhm");
  ret_i &= _uw_etch(0x1234567, "0w18QlD");
  ret_i &= _uw_etch(0x12345678, "0wid5pU");
  ret_i &= _uw_etch(0x123456789, "0w4.zhmu9");
  ret_i &= _uw_etch(0x100000000, "0w4.00000");
  ret_i &= _uw_etch(0x101101101, "0w4.14141");
  ret_i &= _uw_etch(0x201201201, "0w8.18181");
  ret_i &= _uw_etch(0x302201100, "0wc.28140");

  ret_i &= _uw_etch(0x123456789abcdefULL, "0w4zhmu.9GYTL");
  ret_i &= _uw_etch(0x8589934592ULL, "0w8m.9AQmi");
  ret_i &= _uw_etch(0x5843009213693952ULL, "0w5.x3098.jqjBi");
  ret_i &= _uw_etch(0x6744073709551615ULL, "0w6.t41Ps.9lhol");

  {
    c3_c* num_c = "0w40.00000.00000.00000.00000";
    u3_atom num = u3qc_bex(128);
    c3_c*  out_c;
    size_t len_i = u3s_etch_uw_c(num, &out_c);

    if ( 0 != strncmp(num_c, out_c, len_i) ) {
      fprintf(stderr, "etch_uw: (bex 128) fail; expected %s, got '%s'\r\n",
                      num_c, out_c);
      ret_i = 0;
    }
    else {
      u3_noun out = u3s_etch_uw(num);
      u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

      if ( c3n == u3r_sing(tou, out) ) {
        fprintf(stderr, "etch_uw: (bex 128) mismatch; expected %s\r\n", num_c);
        u3m_p("out", out);
        ret_i = 0;
      }

      u3z(out);
      u3z(tou);
    }

    c3_free(out_c);
    u3z(num);
  }

  return ret_i;
}

static inline c3_i
_ud_good(c3_w num_w, const c3_c* num_c)
{
  u3_weak out;
  if ( num_w != (out = u3s_sift_ud_bytes(strlen(num_c), (c3_y*)num_c)) ) {
    if ( u3_none == out ) {
      fprintf(stderr, "sift_ud: %s fail; expected %"PRIc3_w"\r\n", num_c, num_w);
    }
    else {
      fprintf(stderr, "sift_ud: %s wrong; expected %"PRIc3_w": actual %"PRIc3_w"\r\n", num_c, num_w, out);
    }
    return 0;
  }

  return 1;
}

static inline c3_i
_ud_fail(const c3_c* num_c)
{
  u3_weak out;
  if ( u3_none != (out = u3s_sift_ud_bytes(strlen(num_c), (c3_y*)num_c)) ) {
    u3m_p("out", out);
    fprintf(stderr, "sift_ud: %s expected fail\r\n", num_c);
    return 0;
  }

  return 1;
}

static c3_i
_test_sift_ud(void)
{
  c3_i ret_i = 1;

  ret_i &= _ud_good(0, "0");
  ret_i &= _ud_good(1, "1");
  ret_i &= _ud_good(12, "12");
  ret_i &= _ud_good(123, "123");
  ret_i &= _ud_good(1234, "1.234");
  ret_i &= _ud_good(12345, "12.345");
  ret_i &= _ud_good(123456, "123.456");
  ret_i &= _ud_good(1234567, "1.234.567");
  ret_i &= _ud_good(12345678, "12.345.678");
  ret_i &= _ud_good(123456789, "123.456.789");
  ret_i &= _ud_good(100000000, "100.000.000");
  ret_i &= _ud_good(101101101, "101.101.101");
  ret_i &= _ud_good(201201201, "201.201.201");
  ret_i &= _ud_good(302201100, "302.201.100");

  ret_i &= _ud_fail("01");
  ret_i &= _ud_fail("02");
  ret_i &= _ud_fail("003");
  ret_i &= _ud_fail("1234");
  ret_i &= _ud_fail("1234.5");
  ret_i &= _ud_fail("1234.567.8");
  ret_i &= _ud_fail("1234.56..78.");
  ret_i &= _ud_fail("123.45a");
  ret_i &= _ud_fail(".123.456");

  {
    c3_c* num_c = "4.294.967.296";
    u3_weak out = u3s_sift_ud_bytes(strlen(num_c), (c3_y*)num_c);
    u3_atom pro = u3qc_bex(32);

    if ( u3_none == out ) {
      fprintf(stderr, "sift_ud: (bex 32) fail\r\n");
      ret_i = 0;
    }

    if ( c3n == u3r_sing(pro, out) ) {
      u3m_p("out", out);
      fprintf(stderr, "sift_ud: (bex 32) wrong\r\n");
      ret_i = 0;
    }

    u3z(out); u3z(pro);
  }


  {
    c3_c* num_c = "340.282.366.920.938.463.463.374.607.431.768.211.456";
    u3_weak out = u3s_sift_ud_bytes(strlen(num_c), (c3_y*)num_c);
    u3_atom pro = u3qc_bex(128);

    if ( u3_none == out ) {
      fprintf(stderr, "sift_ud: (bex 128) fail\r\n");
      ret_i = 0;
    }

    if ( c3n == u3r_sing(pro, out) ) {
      u3m_p("out", out);
      fprintf(stderr, "sift_ud: (bex 128) wrong\r\n");
      ret_i = 0;
    }

    u3z(out); u3z(pro);
  }

  return ret_i;
}

static c3_i
_test_en_base16(void)
{
  c3_i ret_i = 1;

  {
    u3_atom dat = 0xaa;
    u3_atom pro = u3qe_en_base16(u3r_met(3, dat), dat);

    if ( c3n == u3r_sing_c("aa", pro) ) {
      fprintf(stderr, "en_base16: fail (a)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom dat = 0x1234;
    u3_atom pro = u3qe_en_base16(u3r_met(3, dat), dat);

    if ( c3n == u3r_sing_c("1234", pro) ) {
      fprintf(stderr, "en_base16: fail (b)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom dat = 0xf012;
    u3_atom pro = u3qe_en_base16(u3r_met(3, dat), dat);

    if ( c3n == u3r_sing_c("f012", pro) ) {
      fprintf(stderr, "en_base16: fail (c)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom dat = 0x10b;
    u3_atom pro = u3qe_en_base16(u3r_met(3, dat), dat);

    if ( c3n == u3r_sing_c("010b", pro) ) {
      fprintf(stderr, "en_base16: fail (d)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom pro = u3qe_en_base16(3, 0x1234);

    if ( c3n == u3r_sing_c("001234", pro) ) {
      fprintf(stderr, "en_base16: fail (e)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom pro = u3qe_en_base16(1, 0x1234);

    if ( c3n == u3r_sing_c("34", pro) ) {
      fprintf(stderr, "en_base16: fail (f)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  return ret_i;
}


static c3_i
_test_de_base16(void)
{
  c3_i ret_i = 1;

  {
    u3_noun inp = u3i_string("aa");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (a)\r\n");
      ret_i = 0;
    }

    if ( 1 != len ) {
      fprintf(stderr, "de_base16: fail len (a)\r\n");
      ret_i = 0;
    }

    if ( 0xaa != dat ) {
      fprintf(stderr, "de_base16: fail dat (a)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("1234");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (b)\r\n");
      ret_i = 0;
    }

    if ( 2 != len ) {
      fprintf(stderr, "de_base16: fail len (b)\r\n");
      ret_i = 0;
    }

    if ( 0x1234 != dat ) {
      fprintf(stderr, "de_base16: fail dat (b)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("f012");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (c)\r\n");
      ret_i = 0;
    }

    if ( 2 != len ) {
      fprintf(stderr, "de_base16: fail len (c)\r\n");
      ret_i = 0;
    }

    if ( 0xf012 != dat ) {
      fprintf(stderr, "de_base16: fail dat (c)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("010b");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (d)\r\n");
      ret_i = 0;
    }

    if ( 2 != len ) {
      fprintf(stderr, "de_base16: fail len (d)\r\n");
      ret_i = 0;
    }

    if ( 0x10b != dat ) {
      fprintf(stderr, "de_base16: fail dat (d)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("10b");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (e)\r\n");
      ret_i = 0;
    }

    if ( 2 != len ) {
      fprintf(stderr, "de_base16: fail len (e)\r\n");
      ret_i = 0;
    }

    if ( 0x10b != dat ) {
      fprintf(stderr, "de_base16: fail dat (e)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("001234");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (f)\r\n");
      ret_i = 0;
    }

    if ( 3 != len ) {
      fprintf(stderr, "de_base16: fail len (f)\r\n");
      ret_i = 0;
    }

    if ( 0x1234 != dat ) {
      fprintf(stderr, "de_base16: fail dat (f)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  return ret_i;
}

static c3_i
_test_base16(void)
{
  c3_i ret_i = 1;

  ret_i &= _test_en_base16();
  ret_i &= _test_de_base16();

  return ret_i;
}

static c3_w
_fein_ob_w(c3_w inp_w)
{
  u3_atom inp = u3i_word(inp_w);
  u3_atom act = u3qe_fein_ob(inp);
  c3_w  act_w = u3r_word(0, act);
  u3z(inp); u3z(act);
  return act_w;
}

static c3_i
_expect_fein_ob_w(c3_w inp_w, c3_w exp_w)
{
  c3_w act_w = _fein_ob_w(inp_w);

  if ( act_w != exp_w ) {
    fprintf(stderr, "fein: inp=0x%08"PRIxc3_w" exp=0x%08"PRIxc3_w" act=0x%08"PRIxc3_w"\n",
                    inp_w, exp_w, act_w);
    return 0;
  }

  return 1;
}

static c3_i
_test_fein_ob(void)
{
  c3_i ret_i = 1;

  ret_i &= _expect_fein_ob_w(0, 0);
  ret_i &= _expect_fein_ob_w(0xffff, 0xffff);
  ret_i &= _expect_fein_ob_w(0x1b08f, 0x76b920e5);
  ret_i &= _expect_fein_ob_w(0x10000, 0x423e60bf);
  ret_i &= _expect_fein_ob_w(0x10001, 0xd4400acb);
  ret_i &= _expect_fein_ob_w(0x10002, 0xf429043);
  ret_i &= _expect_fein_ob_w(0x10000000, 0xa04bc7fa);
  ret_i &= _expect_fein_ob_w(0x1234abcd, 0x686f6c25);
  ret_i &= _expect_fein_ob_w(0xabcd1234, 0x4a220c8);
  ret_i &= _expect_fein_ob_w(0xdeadbeef, 0x909bc4a9);
  ret_i &= _expect_fein_ob_w(0xfffff, 0x6746b96b);
  ret_i &= _expect_fein_ob_w(0xffffffff, 0xbba4dcce);

  return ret_i;
}

static c3_w
_fynd_ob_w(c3_w inp_w)
{
  u3_atom inp = u3i_word(inp_w);
  u3_atom act = u3qe_fynd_ob(inp);
  c3_w  act_w = u3r_word(0, act);
  u3z(inp); u3z(act);
  return act_w;
}

static c3_i
_expect_fynd_ob_w(c3_w exp_w, c3_w inp_w)
{
  c3_w act_w = _fynd_ob_w(inp_w);

  if ( act_w != exp_w ) {
    fprintf(stderr, "fynd: inp=0x%08"PRIxc3_w" exp=0x%08"PRIxc3_w" act=0x%08"PRIxc3_w"\n",
                    inp_w, exp_w, act_w);
    return 0;
  }

  return 1;
}

static c3_i
_test_fynd_ob(void)
{
  c3_i ret_i = 1;

  ret_i &= _expect_fynd_ob_w(0, 0);
  ret_i &= _expect_fynd_ob_w(0xffff, 0xffff);
  ret_i &= _expect_fynd_ob_w(0x10000, 0x423e60bf);
  ret_i &= _expect_fynd_ob_w(0x10001, 0xd4400acb);
  ret_i &= _expect_fynd_ob_w(0x10002, 0xf429043);
  ret_i &= _expect_fynd_ob_w(0x10000000, 0xa04bc7fa);
  ret_i &= _expect_fynd_ob_w(0x1234abcd, 0x686f6c25);
  ret_i &= _expect_fynd_ob_w(0xabcd1234, 0x4a220c8);
  ret_i &= _expect_fynd_ob_w(0xdeadbeef, 0x909bc4a9);
  ret_i &= _expect_fynd_ob_w(0xfffff, 0x6746b96b);
  ret_i &= _expect_fynd_ob_w(0xffffffff, 0xbba4dcce);

  return ret_i;
}

static c3_i
_exhaust_roundtrip_fein_fynd_ob(void)
{
  c3_i ret_i = 1;
  c3_w fyn_w, i_w;

  {
    u3_atom fen, fyn;

    for ( i_w = 0x10000; i_w < 0x80000000; i_w++ ) {
      fen   = u3qe_fein_ob(i_w);
      fyn   = u3qe_fynd_ob(fen);
      fyn_w = u3r_word(0, fyn);

      if ( i_w != fyn_w ) {
        fprintf(stderr, "fein/fynd: inp=0x%08"PRIxc3_w" fein=0x%08"PRIxc3_w" fynd=0x%08"PRIxc3_w"\n",
                        i_w, u3r_word(0, fen), fyn_w);
        ret_i = 0;
      }
      u3z(fen); u3z(fyn);

      if ( !(i_w % 0x1000000) ) {
        fprintf(stderr, "fein/fynd: 0x%"PRIxc3_w" done\n", i_w);
      }
    }
  }

  {
    c3_w fen_w;

    do {
      fen_w = _fein_ob_w(i_w);
      fyn_w = _fynd_ob_w(fen_w);
      if ( i_w != fyn_w ) {
        fprintf(stderr, "fein/fynd: inp=0x%08"PRIxc3_w" fein=0x%08"PRIxc3_w" fynd=0x%08"PRIxc3_w"\n",
                        i_w, fen_w, fyn_w);
        ret_i = 0;
      }

      if ( !(i_w % 0x1000000) ) {
        fprintf(stderr, "fein/fynd: 0x%"PRIxc3_w" done\n", i_w);
      }
    }
    while ( ++i_w );
  }

  return ret_i;
}

static c3_i
_test_ob(void)
{
  c3_i ret_i = 1;
  ret_i &= _test_fein_ob();
  ret_i &= _test_fynd_ob();
  //  disabled, takes almost ~m15
  //
  // ret_i &= _exhaust_roundtrip_fein_fynd_ob();
  return ret_i;
}

static c3_i
_test_mas(void)
{
  c3_i  ret_i = 1;
  u3_atom res;

  if ( 0x4000 != (res = u3qc_mas(0x8000)) ) {
    fprintf(stderr, "test mas fail: (mas 0x8000) != 0x4000: 0x'%"PRIxc3_w"'\r\n", res);
    ret_i = 0;
  }

  if ( 0x20000000 != (res = u3qc_mas(0x40000000)) ) {
    fprintf(stderr, "test mas fail: (mas 0x4000.0000) != 0x2000.0000: 0x%"PRIxc3_w"\r\n", res);
    ret_i = 0;
  }

  {
    u3_atom sam, pro;

    sam = u3qc_bex(36);
    pro = u3qc_bex(35);
    res = u3qc_mas(sam);

    if ( c3n == u3r_sing(pro, res) ) {
      c3_c* out_c;
      u3s_etch_ux_c(res, &out_c);
      fprintf(stderr, "test mas fail: (mas (bex 36)) != (bex 35): %s\r\n", out_c);
      c3_free(out_c);
      ret_i = 0;
    }

    u3z(res); u3z(sam); u3z(pro);

    sam = u3qc_bex(64);
    pro = u3qc_bex(63);
    res = u3qc_mas(sam);

    if ( c3n == u3r_sing(pro, res) ) {
      c3_c* out_c;
      u3s_etch_ux_c(res, &out_c);
      fprintf(stderr, "test mas fail: (mas (bex 64)) != (bex 63): %s\r\n", out_c);
      c3_free(out_c);
      ret_i = 0;
    }

    u3z(res); u3z(sam); u3z(pro);

    sam = u3qc_bex(65);
    pro = u3qc_bex(64);
    res = u3qc_mas(sam);

    if ( c3n == u3r_sing(pro, res) ) {
      c3_c* out_c;
      u3s_etch_ux_c(res, &out_c);
      fprintf(stderr, "test mas fail: (mas (bex 65)) != (bex 64): %s\r\n", out_c);
      c3_free(out_c);
      ret_i = 0;
    }

    u3z(res); u3z(sam); u3z(pro);
  }

  return ret_i;
}

static c3_i
_cut_check(c3_g a_g, u3_atom b, u3_atom c, u3_atom d,
           u3_atom exp, const c3_c* cas_c)
{
  u3_atom pro = u3qc_cut(a_g, b, c, d);
  c3_i  ret_i = 1;

  if ( c3n == u3r_sing(exp, pro) ) {
    fprintf(stderr, "test cut fail: %s\r\n", cas_c);
    u3m_p("expected", exp);
    u3m_p("got", pro);
    ret_i = 0;
  }

  u3z(b); u3z(c); u3z(exp); u3z(pro);
  return ret_i;
}

static c3_i
_test_cut(void)
{
  c3_i    ret_i = 1;
  u3_atom d     = u3qc_bex(100);   //  101 bits

  ret_i &= _cut_check(0, 0, 0, d, 0, "zero width");
  ret_i &= _cut_check(0, 100, 1, d, 1, "top bit");
  ret_i &= _cut_check(0, 101, 1, d, 0, "past the end");
  ret_i &= _cut_check(0, 0, 101, d, u3k(d), "whole atom");
  ret_i &= _cut_check(0, 64, 1000, d, u3qc_bex(36), "clamped width");

  //  an offset or width wider than a word used to bail %fail;
  //  hoon's +cut gives 0 or the rest of the atom
  //
  ret_i &= _cut_check(0, 0, u3qc_bex(40), d, u3k(d), "huge width");
  ret_i &= _cut_check(0, 64, u3qc_bex(40), d, u3qc_bex(36),
                      "huge width at offset");
  ret_i &= _cut_check(0, u3qc_bex(40), 1, d, 0, "huge offset");
  ret_i &= _cut_check(0, u3qc_bex(40), u3qc_bex(40), d, 0,
                      "huge offset and width");

  //  .b + .c used to wrap around a word and skip the clamp
  //
  ret_i &= _cut_check(0, 64, u3i_word((c3_w)0 - 16), d, u3qc_bex(36),
                      "width that wraps");

  u3z(d);
  return ret_i;
}

static c3_i
_test_rub(void)
{
  c3_i ret_i = 1;

  //  40 leading zeros claim a 2^39-bit value: the jet used to bail
  //  %fail, which +mole can't catch.  hoon gives [(add 80 (bex 39)) 0]
  //
  {
    u3_atom b   = u3qc_bex(40);
    u3_atom e   = u3qc_bex(39);
    u3_noun pro = u3qe_rub(0, b);
    u3_noun exp = u3nc(u3qa_add(80, e), 0);

    if ( c3n == u3r_sing(exp, pro) ) {
      fprintf(stderr, "test rub fail: (rub 0 (bex 40))\r\n");
      u3m_p("got", pro);
      ret_i = 0;
    }

    u3z(b); u3z(e); u3z(pro); u3z(exp);
  }

  //  a well-formed +mat still decodes
  //
  {
    u3_noun mat = u3qe_mat(c3__ames);
    u3_noun pro = u3qe_rub(0, u3t(mat));
    u3_noun exp = u3nc(u3k(u3h(mat)), c3__ames);

    if ( c3n == u3r_sing(exp, pro) ) {
      fprintf(stderr, "test rub fail: (rub 0 q:(mat %%ames))\r\n");
      u3m_p("got", pro);
      ret_i = 0;
    }

    u3z(mat); u3z(pro); u3z(exp);
  }

  return ret_i;
}

/* _lug_check(): run +lug on [e a] with f32 options, compare to [exp].
**   p=24, v=-149, w=253, so emx is --104 and lfn is [--104 0xff.ffff].
**   @s atoms: --n is 2n, -n is 2n-1.
*/
static c3_i
_lug_check(c3_m t_m, c3_w e_w, c3_w a_w, u3_noun exp, const c3_c* cas_c)
{
  u3_noun a   = u3nc(e_w, a_w);
  u3_noun b   = u3nt(u3nt(24, 297, 253), c3__z, c3__d);
  u3_noun pro = u3qef_lug(a, b, t_m, c3y);
  c3_i  ret_i = 1;

  if ( c3n == u3r_sing(exp, pro) ) {
    fprintf(stderr, "test lug fail: %s\r\n", cas_c);
    u3m_p("expected", exp);
    u3m_p("got", pro);
    ret_i = 0;
  }

  u3z(a); u3z(b); u3z(exp); u3z(pro);
  return ret_i;
}

static c3_i
_test_lug(void)
{
  c3_i ret_i = 1;

  //  2 * MAX (exponent --105) overflows: toward zero saturates,
  //  every other direction gives infinity (urbit/urbit#7426)
  //
  ret_i &= _lug_check(c3__fl, 210, 0xffffff,
                      u3nq(c3__f, c3y, 208, 0xffffff), "%fl overflow");
  ret_i &= _lug_check(c3__sm, 210, 0xffffff,
                      u3nq(c3__f, c3y, 208, 0xffffff), "%sm overflow");
  ret_i &= _lug_check(c3__ce, 210, 0xffffff, u3nc(c3__i, c3y), "%ce overflow");
  ret_i &= _lug_check(c3__lg, 210, 0xffffff, u3nc(c3__i, c3y), "%lg overflow");
  ret_i &= _lug_check(c3__ne, 210, 0xffffff, u3nc(c3__i, c3y), "%ne overflow");

  //  MAX itself is in range in every direction
  //
  ret_i &= _lug_check(c3__fl, 208, 0xffffff,
                      u3nq(c3__f, c3y, 208, 0xffffff), "%fl max");
  ret_i &= _lug_check(c3__ce, 208, 0xffffff,
                      u3nq(c3__f, c3y, 208, 0xffffff), "%ce max");

  return ret_i;
}

// Individual puts independently specify the entire canonical tree and the
// winning map value for every key, including distinct keys with equal mugs.
static void
_test_bulk_trees(void)
{
  const unsigned sizes[] = {0, 1, 2, 31, 32, 33, 255, 1024};
  for ( size_t i = 0; i < sizeof(sizes) / sizeof(*sizes); i++ ) {
    u3_noun keys = u3_nul, pairs = u3_nul, set = u3_nul, map = u3_nul;
    for ( unsigned j = sizes[i]; j > 0; --j ) {
      u3_noun key;
      switch ( j % 7 ) {
        case 0: key = u3i_chub(UINT64_C(0xc001d00d0000b33f)); break;
        case 1: key = u3i_chub(UINT64_C(0xc001d00d00012a3e)); break;
        case 2: key = u3nc(j % 11, j % 5); break;
        case 3: case 4: key = 0; break;
        default: key = j; break;
      }
      keys = u3nc(u3k(key), keys);
      pairs = u3nc(u3nc(key, j), pairs);
    }
    for ( u3_noun rest = pairs; rest; rest = u3t(rest) ) {
      u3_noun pair = u3h(rest), key = u3h(pair);
      u3_noun next_set = u3qdi_put(set, key);
      u3_noun next_map = u3qdb_put(map, key, u3t(pair));
      u3z(set); u3z(map);
      set = next_set; map = next_map;
    }
    u3_noun built_set = u3qdi_gas(u3_nul, keys);
    u3_noun built_map = u3qdb_gas(u3_nul, pairs);
    u3_assert(c3y == u3r_sing(set, built_set));
    u3_assert(c3y == u3r_sing(map, built_map));
    u3_noun edited = u3qdb_put(map, map ? u3h(u3h(map)) : 0, 99);
    u3_noun merged = u3qdb_uni(map, edited);
    u3_assert(merged == edited);
    u3z(merged); u3z(edited);
    u3_noun projected = u3qdb_key(map);
    u3_assert(c3y == u3r_sing(projected, set));
    u3z(projected);
    u3z(built_set); u3z(built_map);
    u3z(set); u3z(map); u3z(keys); u3z(pairs);
  }
  // Equal heap priorities for different mugs. gor places 136618 first,
  // while the mor tie is decided by dor in favor of 114115.
  u3_noun x = 114115, y = 136618;
  u3_assert(u3r_mug(x) > u3r_mug(y));
  u3_assert(u3r_mug_word(u3r_mug(x)) == u3r_mug_word(u3r_mug(y)));
  u3_noun tie_keys = u3nc(y, u3nc(x, 0));
  u3_noun tie_set = u3qdi_gas(0, tie_keys);
  u3_noun tie_want = u3nt(x, u3nt(y, 0, 0), 0);
  u3_assert(c3y == u3r_sing(tie_set, tie_want));
  u3z(tie_keys); u3z(tie_set); u3z(tie_want);

  // A noncanonical map can place the same key on either side.
  u3_noun list = u3nc(4, u3nc(7, 0));
  u3_noun expected = u3qdi_gas(0, list);
  u3_noun tree = u3nt(u3nc(7, 0), u3nt(u3nc(7, 1), 0, 0),
                                    u3nt(u3nc(4, 2), 0, 0));
  u3_noun actual = u3qdb_key(tree);
  u3_assert(c3y == u3r_sing(expected, actual));
  u3z(list); u3z(expected); u3z(tree); u3z(actual);

}

static void
_test_map_union(void)
{
  u3_noun a = 0, b = 0, want = 0;
  for ( c3_w i = 0; i < 2048; ++i ) {
    u3_noun key = i;
    switch ( i ) {
      case 0: key = u3i_chub(UINT64_C(0xc001d00d0000b33f)); break;
      case 1: key = u3i_chub(UINT64_C(0xc001d00d00012a3e)); break;
      case 2: key = 114115; break;
      case 3: key = 136618; break;
    }
    if ( !(i & 1) || !(i % 5) ) a = u3kdb_put(a, u3k(key), 11);
    if ( (i & 1) || !(i % 5) ) b = u3kdb_put(b, u3k(key), 22);
    want = u3kdb_put(want, key, ((i & 1) || !(i % 5)) ? 22 : 11);
  }
  u3_noun got = u3qdb_uni(a, b);
  u3_assert(c3y == u3r_sing(got, want));
  u3z(got);

  // Independently allocated equal keys may exceed the streaming tie budget.
  u3_noun ka = 0, kb = 0;
  for ( c3_w i = 0; i < 96; ++i ) {
    ka = u3nc(i, ka);
    kb = u3nc(i, kb);
  }
  a = u3kdb_put(a, ka, 33);
  b = u3kdb_put(b, u3k(kb), 44);
  want = u3kdb_put(want, kb, 44);
  got = u3kdb_uni(a, b);
  u3_assert(c3y == u3r_sing(got, want));
  u3z(got); u3z(want);
}

static c3_i
_test_jets(void)
{
  c3_i ret_i = 1;

  _test_bulk_trees();
  _test_map_union();

  // Both alternating triples are longest. The earlier right endpoint wins.
  u3_noun left = u3nc(1, u3nq(2, 1, 2, 0));
  u3_noun right = u3nc(2, u3nq(1, 2, 1, 0));
  u3_noun common = u3qe_loss(left, right);
  u3_noun expected = u3nq(2, 1, 2, 0);
  u3_assert(c3y == u3r_sing(common, expected));
  u3z(left); u3z(right); u3z(common); u3z(expected);

  // Shifted alternating lists have two optimal answers across several
  // machine words. The right list without its last element ends earlier.
  left = right = expected = u3_nul;
  for ( c3_w i = 193; i > 0; --i ) {
    left = u3nc(i & 1, left);
    right = u3nc((i + 1) & 1, right);
    if ( i < 193 ) expected = u3nc((i + 1) & 1, expected);
  }
  common = u3qe_loss(left, right);
  u3_assert(c3y == u3r_sing(common, expected));
  u3z(left); u3z(right); u3z(common); u3z(expected);

  // Escapes on either side of a word boundary, including zero and DEL.
  const c3_y bytes[] = {'a','b','c','d','e','f','g','"',0,'\\',10,127,'z'};
  u3_noun json = u3nc(c3__s, u3i_bytes(sizeof(bytes), bytes));
  u3_noun text = u3qe_json_en(json);
  u3_assert(c3y == u3r_sing_c("\"abcdefg\\\"\\u0000\\\\\\n\\u007fz\"", text));
  u3z(json); u3z(text);

  if ( !_test_cut() ) {
    fprintf(stderr, "test jets: cut: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_rub() ) {
    fprintf(stderr, "test jets: rub: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_ud() ) {
    fprintf(stderr, "test jets: etch_ud: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_ux() ) {
    fprintf(stderr, "test jets: etch_ux: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_uv() ) {
    fprintf(stderr, "test jets: etch_uv: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_uw() ) {
    fprintf(stderr, "test jets: etch_uw: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sift_ud() ) {
    fprintf(stderr, "test jets: sift_ud: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_base16() ) {
    fprintf(stderr, "test jets: base16: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_ob() ) {
    fprintf(stderr, "test jets: ob: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_mas() ) {
    fprintf(stderr, "test jets: mas: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_lug() ) {
    fprintf(stderr, "test jets: lug: failed\r\n");
    ret_i = 0;
  }

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_jets() ) {
    fprintf(stderr, "test jets: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab();

  fprintf(stderr, "test jets: ok\r\n");
  return 0;
}
