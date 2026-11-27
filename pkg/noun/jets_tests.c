/// @file

#include "noun.h"
#include "jets/w.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 24);
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

/* _test_json_de_array(): a parse error inside an array returns ~, and the
** happy path still parses. The fix guards the error-path free of the
** array frame's borrowed tail pointer (#1142); this is a smoke test of
** the error path (the loom corruption it guards against is
** layout-dependent and only reliably bails on a large, populated loom).
*/
static c3_i
_test_json_de_array(void)
{
  c3_i ret_i = 1;

  {
    u3_noun inp = u3i_string("[1,2");
    u3_noun pro = u3qe_json_de(inp);

    if ( u3_nul != pro ) {
      fprintf(stderr, "json_de: unterminated array should be ~\r\n");
      ret_i = 0;
    }
    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("[1,2]");
    u3_noun pro = u3qe_json_de(inp);

    if ( (c3n == u3du(pro)) || (u3_nul != u3h(pro)) ) {
      fprintf(stderr, "json_de: happy path should parse to [~ json]\r\n");
      ret_i = 0;
    }
    u3z(inp); u3z(pro);
  }

  return ret_i;
}

static c3_i
_test_json_de_string(void)
{
  const c3_w lengths[] = {7, 8, 1023, 1024, 1025};
  const c3_c suffix[] = "\\\"\\n\xc3\xa9\"";
  const c3_c decoded[] = "\"\n\xc3\xa9";
  c3_y text[1040], value[1040];
  c3_i ret_i = 1;

  for ( c3_w i = 0; i < c3_array_len(lengths); i++ ) {
    c3_w len = lengths[i];
    text[0] = '"';
    memset(text + 1, 'a', len);
    memcpy(text + 1 + len, suffix, sizeof(suffix) - 1);
    memset(value, 'a', len);
    memcpy(value + len, decoded, sizeof(decoded) - 1);

    u3_noun got = u3ke_json_de(u3i_bytes(len + sizeof(suffix), text));
    u3_noun want = u3nt(u3_nul, c3__s,
                        u3i_bytes(len + sizeof(decoded) - 1, value));
    ret_i &= c3y == u3r_sing(want, got);
    u3z(want); u3z(got);
  }
  return ret_i;
}

static c3_i
_test_json_de_object(void)
{
  // The first two keys share a mug. The last two share only a heap rank.
  const c3_c* keys[] = {"field-41314", "field-104476",
                        "field-116410", "field-149074"};
  u3_noun map = u3_nul;
  for ( c3_w i = 0; i < c3_array_len(keys); i++ ) {
    map = u3kdb_put(map, u3i_string(keys[i]), u3nc(c3__n, '1' + i));
  }
  u3_noun want = u3nc(u3_nul, u3nc(c3__o, map));
  u3_noun got = u3ke_json_de(u3i_string(
    "{\"field-149074\":4,\"field-104476\":[{\"x\":1}],"
    "\"field-41314\":1,\"field-116410\":3,\"field-104476\":2}"));
  c3_i ret_i = c3y == u3r_sing(want, got);
  u3z(want); u3z(got);
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
_test_hike(void)
{
  u3_noun edits = u3nl(u3nt(1, 1, 9), u3nt(4, 1, 11), u3nt(5, 1, 12),
                       u3nt(4, 1, 13), u3nt(3, 0, 3), u3nt(7, 1, 14));
  u3_noun merged = u3nc(u3nt(1, 11, 12), u3nc(0, 3));
  u3_noun root = u3nt(10, u3nc(1, merged), u3nc(0, 6));
  u3_noun want = u3nt(10, u3nt(7, 1, 14), root);
  u3_noun got = u3qf_hike(6, edits);
  c3_i ret_i = c3y == u3r_sing(want, got);
  u3z(edits); u3z(want); u3z(got);

  u3_atom left = u3qc_bex(71);
  u3_atom right = u3qa_inc(left);
  edits = u3nl(u3nt(right, 1, 36), u3nt(left, 1, 35));
  want = u3nt(10, u3nc(u3qc_bex(70), u3nt(1, 35, 36)), u3nc(0, 6));
  got = u3qf_hike(6, edits);
  ret_i &= c3y == u3r_sing(want, got);
  u3z(edits); u3z(want); u3z(got);
  return ret_i;
}

static c3_i
_test_noun_order(void)
{
  u3_noun a = u3i_string("abcdefghijkl");
  u3_noun b = u3i_string("abcdefghijkl");
  u3_noun x = u3nc(u3k(a), 256);
  u3_noun y = u3nc(u3k(b), 1);
  u3_noun z = u3nc(u3k(b), 256);

  c3_i ret_i = (c3y == u3qc_aor(0, 256)) && (c3n == u3qc_aor(256, 0))
            && (c3y == u3qc_aor(a, b)) && (c3y == u3qc_dor(a, b))
            && (c3y == u3qc_aor(x, z)) && (c3y == u3qc_dor(x, z))
            && (c3y == u3qc_aor(x, y)) && (c3n == u3qc_dor(x, y))
            && (c3n == u3qc_aor(y, x)) && (c3y == u3qc_dor(y, x))
            && (c3y == u3qc_dor(a, x)) && (c3n == u3qc_aor(x, a));

  u3z(x); u3z(y); u3z(z); u3z(a); u3z(b);
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
  const unsigned sizes[] = {0, 1, 2, 31, 32, 33, 63, 64, 65, 255, 1024};
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
    u3_assert(c3y == u3qdi_apt(built_set));
    u3_assert(c3y == u3qdb_apt(built_map));
    u3_assert(c3y == u3r_sing(set, built_set));
    u3_assert(c3y == u3r_sing(map, built_map));
    // Retain a key absent from the batch and replace the value at key zero.
    u3_noun seed_set = u3qdi_put(0, 8192);
    u3_noun seed_map = u3kdb_put(u3qdb_put(0, 8192, 5), 0, 7);
    u3_noun want_set = u3qdi_put(set, 8192);
    u3_noun want_map = u3k(seed_map);
    for ( u3_noun rest = pairs; rest; rest = u3t(rest) ) {
      u3_noun pair = u3h(rest);
      u3_noun next = u3qdb_put(want_map, u3h(pair), u3t(pair));
      u3z(want_map);
      want_map = next;
    }
    u3_noun updated = u3qdb_gas(seed_map, pairs);
    u3_assert(c3y == u3r_sing(want_map, updated));
    u3_assert(7 == u3qdb_git(seed_map, 0));
    u3z(updated);
    updated = u3qdi_gas(seed_set, keys);
    u3_assert(c3y == u3r_sing(want_set, updated));
    u3z(updated); u3z(want_set); u3z(want_map); u3z(seed_set); u3z(seed_map);
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
  // Set put can find the root without inspecting its malformed branches.
  u3_noun malformed = u3nc(42, 99);
  u3_noun batch = u3qb_reap(100, 42);
  u3_noun found = u3qdi_gas(malformed, batch);
  u3_assert(found == malformed);
  u3z(found); u3z(batch); u3z(malformed);

  // The ordered cursor can reject an out-of-order node after producing a
  // prefix. Replay the original puts on the supplied tree in that case.
  u3_noun unordered = u3nt(42, u3nt(7, 0, 0), u3nt(7, 0, 0));
  u3_noun updates = 0, sequential = u3k(unordered);
  for ( c3_w i = 128; i; --i ) {
    updates = u3nc(i, updates);
  }
  for ( u3_noun rest = updates; rest; rest = u3t(rest) ) {
    sequential = u3kdi_put(sequential, u3k(u3h(rest)));
  }
  found = u3qdi_gas(unordered, updates);
  u3_assert(c3y == u3r_sing(sequential, found));
  u3z(found); u3z(sequential); u3z(updates); u3z(unordered);

  // Equal heap priorities for different mugs. gor places 136618 first,
  // while the mor tie is decided by dor in favor of 114115.
  u3_noun x = 114115, y = 136618;
  u3_assert(u3r_mug(x) > u3r_mug(y));
  u3_assert(u3r_mug_word(u3r_mug(x)) == u3r_mug_word(u3r_mug(y)));
  u3_noun tie_keys = u3nc(y, u3nc(x, 0));
  u3_noun tie_set = u3qdi_gas(0, tie_keys);
  u3_noun tie_want = u3nt(x, u3nt(y, 0, 0), 0);
  u3_assert(c3y == u3r_sing(tie_set, tie_want));
  u3_assert(c3y == u3qdi_apt(tie_set));
  u3z(tie_keys); u3z(tie_set); u3z(tie_want);

  // Kernel 140 accepts equal keys. The current set validator rejects them.
  u3_noun a = 0, b = 0;
  for ( c3_w i = 0; i < 96; ++i ) {
    a = u3nc(i, a);
    b = u3nc(i, b);
  }
  u3_noun duplicates = u3nt(a, u3nt(b, 0, 0), 0);
  u3_noun core = u3nq(0, 0, 0, u3nc(duplicates, 0));
  u3_assert(c3n == u3qdi_apt(duplicates));
  u3_assert(c3y == u3wdi_apt_140(core));
  u3z(core);

  // A heap-order violation decides the result before the child's shape.
  u3_noun bad = u3nt(y, u3nc(x, 123), 456);
  u3_assert(c3n == u3qdi_apt(bad));
  u3z(bad);

  // A noncanonical map can place the same key on either side.
  u3_noun list = u3nc(4, u3nc(7, 0));
  u3_noun expected = u3qdi_gas(0, list);
  u3_noun tree = u3nt(u3nc(7, 0), u3nt(u3nc(7, 1), 0, 0),
                                    u3nt(u3nc(4, 2), 0, 0));
  u3_noun actual = u3qdb_key(tree);
  u3_assert(c3n == u3qdb_apt(tree));
  u3_assert(c3y == u3r_sing(expected, actual));
  u3z(list); u3z(expected); u3z(tree); u3z(actual);

  // Distinct keys with a long common prefix can tie in both tree orders.
  a = u3i_chub(UINT64_C(0xc001d00d0000b33f));
  b = u3i_chub(UINT64_C(0xc001d00d00012a3e));
  for ( c3_w i = 0; i < 96; ++i ) {
    a = u3nc(i, a);
    b = u3nc(i, b);
  }
  u3_assert(u3r_mug(a) == u3r_mug(b));
  tree = u3kdb_put(u3kdb_put(0, u3k(a), 8), u3k(b), 9);
  expected = u3kdi_put(u3kdi_put(0, a), b);
  actual = u3qdb_key(tree);
  u3_assert(c3y == u3r_sing(expected, actual));
  u3z(tree); u3z(expected); u3z(actual);
}

// Shared input subtrees appear at both positions in the output. On a
// child road the key is senior; on its own road each list head owns it.
static void
_test_tree_tap(void)
{
  u3_assert(u3_nul == u3qdi_tap(u3_nul));
  u3_noun key = u3nc(42, 7);
  u3_noun tree = u3nt(u3k(key), 0, 0);
  for ( c3_w i = 1; i <= 10; ++i ) tree = u3nt(i, u3k(tree), tree);
  u3m_hate(0);
  u3_noun senior = u3m_love(u3qdi_tap(tree));
  u3_noun list = u3kdi_tap(tree);
  u3_noun at = list, other = senior;
  for ( c3_w i = 1; i < 2048; ++i ) {
    u3_noun want = (i & 1) ? key : __builtin_ctz(i);
    u3_assert(u3h(at) == want && u3h(other) == want);
    at = u3t(at);
    other = u3t(other);
  }
  u3_assert(!at && !other);
  u3z(senior); u3z(list);
  u3_assert(u3a_use(key) == 1);
  u3z(key);

  // Reverse in-order includes every node of an uneven tree, without
  // requiring a native stack frame per level.
  tree = u3_nul;
  for ( c3_w i = 1; i <= 8192; ++i ) tree = u3nt(i, tree, u3nt(i + 8192, 0, 0));
  list = u3kdi_tap(tree);
  at = list;
  for ( c3_w i = 8192; i; --i ) {
    u3_assert(u3h(at) == i + 8192 && u3h(u3t(at)) == i);
    at = u3t(u3t(at));
  }
  u3_assert(!at);
  u3z(list);

  u3_noun bad = u3nt(7, u3nt(8, 0, u3nc(9, 10)), u3nt(11, 0, 0));
  u3_noun err = u3m_soft(0, u3qdi_tap, bad);
  u3_assert(c3__exit == u3h(err));
  u3z(err);
}

static void
_test_tree_find(void)
{
  u3_noun a = u3i_chub(UINT64_C(0xc001d00d0000b33f));
  u3_noun b = u3i_chub(UINT64_C(0xc001d00d00012a3e));
  u3_assert(u3r_mug(a) == u3r_mug(b));
  u3_noun map = u3kdb_put(u3kdb_put(0, u3k(a), 0), u3k(b), u3k(b));
  u3_assert(0 == u3qdb_git(map, a));
  u3_assert(u3_none == u3qdb_git(map, 0));
  u3_assert(c3y == u3qdb_has(map, b));

  // The result owns its value after both the map and the key are released.
  u3_noun got = u3kdb_get(map, b);
  u3_assert(u3r_chub(0, got) == UINT64_C(0xc001d00d00012a3e));
  u3z(got);

  // An equal root returns before its branches are inspected.
  u3_noun set = u3nc(a, 17);
  u3_noun key = u3i_chub(UINT64_C(0xc001d00d0000b33f));
  u3_assert(c3y == u3qdi_has(set, key));
  u3z(set); u3z(key);
}

static void
_test_put_rotation(void)
{
  // An equal-key put still rotates an out-of-order ancestor.
  u3_noun top = 1, key = 2;
  if ( c3y == u3qc_mor(top, key) ) {
    top = 2;
    key = 1;
  }
  c3_o left_o = u3qc_gor(key, top);
  for ( c3_t map_t = 0; map_t < 2; map_t++ ) {
    u3_noun node = map_t ? u3nc(top, 10) : top;
    u3_noun item = map_t ? u3nc(key, 20) : key;
    u3_noun child = u3nt(u3k(item), 0, 0);
    u3_noun tree = (c3y == left_o) ? u3nt(u3k(node), child, 0)
                                   : u3nt(u3k(node), 0, child);
    u3_noun leaf = u3nt(node, 0, 0);
    u3_noun want = (c3y == left_o) ? u3nt(item, 0, leaf)
                                   : u3nt(item, leaf, 0);
    u3_noun got = map_t ? u3qdb_put(tree, key, 20) : u3qdi_put(tree, key);
    u3_assert(c3y == u3r_sing(got, want));
    u3z(got); u3z(want); u3z(tree);
  }
}

static void
_test_bif(void)
{
  u3_noun map = 0, set = 0;
  for ( c3_w i = 0; i < 64; i++ ) {
    map = u3kdb_put(map, i, i + 1);
    set = u3kdi_put(set, i);
  }

  for ( c3_w key = 0; key <= 64; key++ ) {
    u3_noun maps[2] = {0}, sets[2] = {0};
    for ( c3_w i = 0; i < 64; i++ ) {
      if ( i != key ) {
        c3_o side = u3qc_gor(i, key);
        maps[side] = u3kdb_put(maps[side], i, i + 1);
        sets[side] = u3kdi_put(sets[side], i);
      }
    }
    u3_noun m = u3qdb_bif(map, key);
    u3_noun t = u3qdi_bif(set, key);
    u3_assert(c3y == u3r_sing(u3h(m), maps[0]));
    u3_assert(c3y == u3r_sing(u3t(m), maps[1]));
    u3_assert(c3y == u3r_sing(u3h(t), sets[0]));
    u3_assert(c3y == u3r_sing(u3t(t), sets[1]));
    u3z(m); u3z(t);
    u3z(maps[0]); u3z(maps[1]); u3z(sets[0]); u3z(sets[1]);
  }
  u3z(map); u3z(set);
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

static void
_test_tree_intersection(void)
{
  u3_noun maps[2] = {0}, sets[2] = {0};
  u3_noun common = 0, values[2] = {0};
  for ( c3_w i = 0; i < 1024; ++i ) {
    for ( c3_w side = 0; side < 2; ++side ) {
      if ( (i & 1) == side || !(i % 3) ) {
        maps[side] = u3kdb_put(maps[side], i, i + side);
        sets[side] = u3kdi_put(sets[side], i);
      }
      if ( !(i % 3) ) values[side] = u3kdb_put(values[side], i, i + side);
    }
    if ( !(i % 3) ) common = u3kdi_put(common, i);
  }
  for ( c3_w side = 0; side < 2; ++side ) {
    u3_noun map = u3qdb_int(maps[1 - side], maps[side]);
    u3_noun set = u3qdi_int(sets[1 - side], sets[side]);
    u3_assert(c3y == u3r_sing(map, values[side]));
    u3_assert(c3y == u3r_sing(set, common));
    u3z(map); u3z(set);
  }
  u3_noun leaf = u3kdb_put(0, 27, 42);
  u3_noun from_tree = u3qdb_int(leaf, maps[0]);
  u3_noun from_leaf = u3qdb_int(maps[0], leaf);
  u3_assert(27 == u3t(u3h(from_tree)));
  u3_assert(c3y == u3r_sing(from_leaf, leaf));
  u3z(from_tree); u3z(from_leaf); u3z(leaf); u3z(common);
  for ( c3_w side = 0; side < 2; ++side ) {
    u3z(maps[side]); u3z(sets[side]); u3z(values[side]);
  }
}

static void
_test_arm_axes(void)
{
  u3_noun gen = u3nc(c3__dtls, 0);
  u3_noun hit = u3nt(u3nc('x', u3k(gen)), 0, 0);
  u3_noun miss = u3nt(u3nc('z', u3k(gen)), 0, 0);
  u3_atom axe = 1;
  u3_noun dab = u3k(hit);

  //  Single-child and two-child steps span several axis chunks.
  //
  for ( c3_w i = 0; i < 100; ++i ) {
    c3_w key = i + 256;
    c3_t left_t = (c3y == u3qc_gor('x', key));
    u3_noun other = (i % 3) ? u3k(miss) : 0;
    u3_noun step = other ? (left_t ? 6 : 7) : 3;
    dab = u3nt(u3nc(key, u3k(gen)), left_t ? dab : other, left_t ? other : dab);
    u3_atom next = u3qc_peg(step, axe);
    u3z(axe); axe = next;
  }
  u3_post cap = u3R->cap_p;
  u3_noun want = u3nt(0, u3k(axe), u3k(gen));
  u3_noun got = u3qf_look('x', dab);
  u3_assert(c3y == u3r_sing(want, got) && cap == u3R->cap_p);
  u3z(want); u3z(got);

  for ( c3_t note_t = 0; note_t < 2; ++note_t ) {
    u3_noun empty = note_t ? u3nc(0, 0) : 0;
    u3_noun arm = note_t ? u3nc(0, u3k(dab)) : u3k(dab);
    u3_noun dom = u3nt(u3nc('a', arm), 0, 0);
    u3_noun absent = u3nt(u3nc('b', u3k(empty)), 0, 0);
    u3_atom path = u3k(axe);
    for ( c3_w i = 0; i < 83; ++i ) {
      u3_noun entry = u3nc(i, u3k(empty));
      dom = (i & 1) ? u3nt(entry, dom, u3k(absent))
                    : u3nt(entry, u3k(absent), dom);
      u3_atom next = u3qc_peg((i & 1) ? 6 : 7, path);
      u3z(path); path = next;
    }
    want = u3nt(0, path, u3k(gen));
    got = note_t ? u3qf_loot_136('x', dom) : u3qf_loot('x', dom);
    u3_assert(c3y == u3r_sing(want, got) && cap == u3R->cap_p);
    u3_assert(0 == (note_t ? u3qf_loot_136('q', dom) : u3qf_loot('q', dom)));
    u3_assert(cap == u3R->cap_p);
    u3z(got); u3z(want); u3z(dom); u3z(absent); u3z(empty);
  }

  //  A chapter's own match precedes matches in its children.
  //
  u3_noun root = u3nt(u3nc('a', u3k(hit)), u3nt(u3nc('b', u3k(hit)), 0, 0), 0);
  got = u3qf_loot('x', root);
  want = u3nt(0, 2, u3k(gen));
  u3_assert(c3y == u3r_sing(want, got));
  u3z(want); u3z(got); u3z(root); u3z(axe); u3z(dab); u3z(hit); u3z(miss); u3z(gen);
}

static void
_check_fond_van(c3_y kel_y, u3_noun van, u3_noun wing, u3_weak want)
{
  u3_noun cor = u3nt(0, u3nc(c3__read, wing), van);
  u3_weak got = (135 == kel_y) ? u3wfu_fond_135(cor) : u3wfu_fond_137(cor);
  u3_weak hit = (135 == kel_y) ? u3wfu_find_135(cor) : u3wfu_find_137(cor);
  if ( want != u3_none && c3y == u3du(want) && c3y == u3h(want) ) {
    u3_assert(hit != u3_none && c3y == u3r_sing(hit, want));
    u3z(hit);
  }
  else u3_assert(hit == u3_none);
  if ( want == u3_none ) u3_assert(got == u3_none);
  else {
    u3_assert(got != u3_none && c3y == u3r_sing(got, want));
    u3z(got);
    u3z(want);
  }
  u3z(cor);
}

static void
_check_fond(u3_noun sut, u3_noun wing, u3_weak want)
{
  _check_fond_van(137, u3nt(0, sut, 0), wing, want);
}

static void
_check_peek(u3_noun sut, u3_noun way, u3_atom axe, u3_weak want)
{
  u3_noun cor = u3nt(0, u3nc(way, axe), u3nt(0, sut, 0));
  u3_weak got = u3wfu_peek(cor);
  if ( want == u3_none ) u3_assert(got == u3_none);
  else {
    u3_assert(got != u3_none && c3y == u3r_sing(want, got));
    u3z(got); u3z(want);
  }
  u3z(cor);
}

static void
_test_peek(void)
{
  u3_noun atom = u3nt(c3__atom, c3__ud, 0);
  u3_noun pair = u3nt(c3__cell, u3k(atom), c3__noun);
  u3_noun face = u3nt(c3__face, 'x', u3k(pair));
  u3_noun alts = u3qf_forq(face, pair);
  _check_peek(u3k(alts), c3__read, 1, u3k(alts));
  _check_peek(u3k(alts), c3__read, 2, u3k(atom));
  _check_peek(u3qf_forq(alts, atom), c3__rite, 3, c3__noun);
  u3_noun other = u3nt(c3__cell, c3__noun, u3k(atom));
  _check_peek(u3qf_forq(other, alts), c3__read, 2, u3qf_forq(c3__noun, atom));
  u3z(other);
  u3_noun hold = u3nt(c3__hold, c3__noun, u3nc(c3__tune, 'x'));
  _check_peek(u3qf_forq(hold, alts), c3__both, 2, u3_none);

  u3_noun coil = u3nq(u3nt(0, c3__dry, c3__lead), c3__noun, 0, 0);
  u3_noun core = u3nt(c3__core, u3k(pair), coil);
  _check_peek(u3k(core), c3__read, 2, c3__noun);
  _check_peek(u3k(core), c3__rite, 6, u3_none);
  _check_peek(core, c3__free, 6, u3k(atom));
  _check_peek(alts, c3__read, 0, u3_none);
  u3z(hold); u3z(face); u3z(pair); u3z(atom);
}

static void
_check_toss(u3_noun sut, u3_noun wing, u3_noun mur, u3_weak want)
{
  u3_noun foot = u3nt(c3__dry, 0, 1);
  u3_noun men = u3nc(u3nc(sut, u3k(foot)), 0);
  u3_noun cor = u3nt(0, u3nt(wing, mur, men), u3nt(0, c3__noun, 0));
  u3_weak got = u3wfu_toss_137(cor);
  if ( want == u3_none ) u3_assert(got == u3_none);
  else {
    u3_noun out = u3nt(u3k(u3h(want)), u3nc(u3k(u3t(want)), u3k(foot)), 0);
    u3_assert(got != u3_none && c3y == u3r_sing(got, out));
    u3z(out); u3z(want); u3z(got);
  }
  u3z(cor); u3z(foot);
}

static void
_test_toss(void)
{
  u3_noun anon = u3nt(c3n, 0, 0);
  u3_noun wing = u3nt(u3k(anon), u3nc(0, 3), 0);
  u3_noun atom = u3nt(c3__atom, c3__ud, 0);
  u3_noun note = u3nc(c3__noun, 0);
  u3_noun name = u3nt(c3__face, 'x', c3__noun);
  u3_noun hint = u3nt(c3__hint, u3k(note), name);
  u3_noun pair = u3nt(c3__cell, c3__noun, hint);

  //  A hint preserves the anonymous step for the named field below it.
  //
  name = u3nt(c3__face, 'x', u3k(atom));
  hint = u3nt(c3__hint, u3k(note), name);
  _check_toss(u3k(pair), u3k(wing), u3k(atom),
              u3nc(3, u3nt(c3__cell, c3__noun, hint)));
  _check_toss(pair, wing, c3__void, u3nc(3, c3__void));
  _check_toss(c3__noun, u3nc(u3nc(0, 2), 0), u3k(atom),
              u3nc(2, u3nt(c3__cell, u3k(atom), c3__noun)));
  _check_toss(u3nc(c3__hold, 0), u3nc(u3k(anon), 0), u3k(atom), u3_none);
  _check_fond(u3k(atom), u3nc(u3nt(c3n, 3, 0), 0), u3nt(c3n, c3y, 2));
  _check_fond(c3__void, u3nc(anon, 0), u3_nul);
  //  Hoon checks axis agreement across arms and requires a nonempty list.
  //
  u3_noun leg = u3nc(c3__noun, u3nt(c3__dry, 0, 1));
  u3_noun men = u3nt(u3k(leg), leg, 0);
  u3_noun cor = u3nt(0, u3nt(0, u3k(atom), men), u3nt(0, c3__noun, 0));
  u3_assert(u3_none == u3wfu_toss_135(cor));
  cor = u3i_edit(cor, u3x_sam_7, u3_nul);
  u3_assert(u3_none == u3wfu_toss_137(cor));
  u3z(cor); u3z(note); u3z(atom);
}

static void
_test_fond(void)
{
  u3_noun pair = u3nt(c3__cell, c3__noun, u3nt(c3__face, 'x', c3__noun));
  u3_noun axis = u3nc(0, 3);
  u3_noun path = u3nt(0, u3nc(0, 1), u3nc(u3k(axis), 0));

  // Resolve axis 3 first, then x at the root of that leg.
  _check_fond(u3k(pair), u3nt('x', u3k(axis), 0), u3nq(0, path, 0, c3__noun));
  _check_fond(pair, u3nc('y', 0), u3nt(1, 0, 0));
  // A later step can require the interpreter, including after reaching void.
  _check_fond(u3nt(c3__cell, c3__noun, u3nt(c3__hold, 0, 0)),
              u3nt('x', u3k(axis), 0), u3_none);
  _check_fond(c3__void, u3nt('x', 'y', 0), u3_none);
  u3z(axis);

  u3_noun deep = c3__noun;
  for ( c3_w i = 0; i < 32; ++i ) deep = u3nt(c3__cell, deep, c3__void);
  axis = u3nc(0, u3i_chub(1ULL << 32));
  u3_weak want = (c3y == u3a_is_cat(u3t(axis)))
               ? u3nq(0, u3nc(u3k(axis), 0), 0, c3__noun)
               : u3_none;
  _check_fond(deep, u3nc(axis, 0), want);

  // Branches must agree on the vein. Compatible legs form a union.
  u3_noun atom = u3nt(c3__atom, 0, 0);
  u3_noun fat = u3nt(c3__face, 'x', u3k(atom));
  u3_noun fac = u3nt(c3__face, 'x', c3__noun);
  path = u3nt(0, u3nc(0, 1), 0);
  _check_fond(u3qf_forq(fac, fat), u3nc('x', 0),
              u3nq(0, u3k(path), 0, u3qf_forq(c3__noun, atom)));
  pair = u3nt(c3__cell, c3__noun, u3k(fac));
  _check_fond(u3qf_forq(fac, pair), u3nc('x', 0), u3_none);
  u3z(pair);

  // Same-axis arms from different cores survive as separate alternatives.
  u3_noun gen = u3nc(c3__dtls, 0);
  u3_noun arms = u3nt(u3nc('x', u3k(gen)), 0, 0);
  u3_noun dom = u3nt(u3nt(0, 0, arms), 0, 0);
  u3_noun coil = u3nq(u3nt(0, c3__dry, c3__gold), c3__noun, 0, dom);
  u3_noun ca = u3nt(c3__core, c3__noun, u3k(coil));
  u3_noun cb = u3nt(c3__core, u3k(atom), coil);
  u3_noun cores = u3qf_forq(ca, cb);
  u3_noun foot = u3nc(c3__dry, gen);
  u3_noun set = u3kdi_put(0, u3nc(ca, u3k(foot)));
  set = u3kdi_put(set, u3nc(cb, foot));
  _check_fond(u3k(cores), u3nc('x', 0),
              u3nq(0, u3nc(u3nc(0, 1), 0), 1, u3nc(2, set)));
  _check_fond(cores, u3nt('y', 'x', 0), u3_none);

  // A completed recursive expansion can revisit its hold on one branch.
  // The other branch still supplies the match.
  u3_noun hol = u3nt(c3__hold, c3__noun, u3nc(c3__tune, 'x'));
  u3_noun van = u3nt(0, u3k(hol), u3nc(u3nt(0, 0, c3y), 0));
  u3_noun leg = u3nc(u3k(u3t(hol)), 0);
  _check_fond_van(137, u3k(van), u3nc('x', 0), u3_none);
  u3_noun key = u3qfu_rest_key(van, hol, leg);
  u3z(u3z_save(u3z_memo_toss, key, u3qf_forq(hol, fac)));
  _check_fond_van(137, u3k(van), u3nc('x', 0), u3nq(0, path, 0, c3__noun));
  _check_fond_van(137, u3i_edit(u3k(van), u3qfu_van_vet, c3n), u3nc('x', 0), u3_none);
  _check_fond_van(137, u3i_edit(van, u3qfu_van_fan, u3qdi_put(0, u3t(hol))),
                  u3nc('x', 0), u3_none);
  // Kelvin 135 resolves a raw arm map and looks up the hold by its leg.
  //
  arms = u3nt(u3nc('x', u3nc(c3__dtls, 0)), 0, 0);
  dom = u3nt(u3nc(0, arms), 0, 0);
  coil = u3nq(u3nt(0, c3__dry, c3__gold), c3__noun, 0, dom);
  ca = u3nt(c3__core, c3__noun, coil);
  foot = u3nc(c3__dry, u3nc(c3__dtls, 0));
  set = u3nt(u3nc(u3k(ca), foot), 0, 0);
  _check_fond_van(135, u3nt(0, ca, 0), u3nc('x', 0),
                  u3nq(0, u3nc(u3nc(0, 1), 0), 1, u3nc(2, set)));
  van = u3nt(0, u3k(hol), u3nc(u3nt(0, 0, c3y), 0));
  _check_fond_van(135, u3k(van), u3nc('x', 0), u3_none);
  key = u3qfu_rest_key(van, hol, u3t(hol));
  u3z(u3z_save(u3z_memo_toss, key, u3k(fac)));
  _check_fond_van(135, u3k(van), u3nc('x', 0),
                  u3nq(0, u3nt(0, u3nc(0, 1), 0), 0, c3__noun));
  _check_fond_van(135, u3i_edit(van, u3qfu_van_fan, u3qdi_put(0, u3t(hol))),
                  u3nc('x', 0), u3_none);
  u3z(leg); u3z(hol); u3z(fac); u3z(fat); u3z(atom);
}

static void
_check_nest(u3_noun arm, u3_noun sut, u3_noun ref,
            u3_noun sets, c3_o tel_o, c3_o want_o)
{
  u3_noun van = u3nt(u3k(arm), sut, u3nc(u3nt(0, 0, c3y), 0));
  u3_noun nest = u3nt(0, u3nc(tel_o, ref), van);
  u3_noun cor = u3nc(arm, u3nt(0, sets, nest));
  u3_noun pro = u3wfu_nest_dext1(cor);
  u3_assert(pro == want_o);
  u3z(pro); u3z(cor);
}

static void
_test_nest(void)
{
  u3_noun atom = u3nt(c3__atom, 0, 0);
  u3_noun pair = u3nt(c3__cell, u3k(atom), c3__noun);
  u3_noun fork = u3qf_forq(pair, atom);
  _check_nest(u3nc(1, c3n), u3k(fork), u3k(pair), u3nt(0, 0, 0), c3n, c3y);
  _check_nest(u3nc(1, c3y), u3k(atom), fork, u3nt(0, 0, 0), c3n, c3n);

  // A residual check sees the child types, the cleared seg and reg,
  // and the enclosing gil after descending through the face.
  //
  u3_noun hold = u3nt(c3__hold, 7, 8);
  u3_noun want = u3nt(u3nt(0, 0, 97), u3k(hold), u3k(atom));
  u3_noun fields = u3nt(u3nc(0, 14), u3nc(0, 254), u3nc(0, 125));
  u3_noun arm = u3nt(5, fields, u3nc(1, want));
  _check_nest(arm, u3nt(c3__cell, c3__noun, u3nt(c3__face, 'x', hold)),
              u3nt(c3__cell, u3k(pair), u3k(atom)), u3nt(12, 13, 97), c3n, c3y);

  // A structural child still consumes a verdict already on this road.
  //
  arm = u3nc(0, 2);
  u3_noun key = u3z_key_3(141 + c3__dext, c3__noun, atom, arm);
  u3z(u3z_save(u3z_memo_toss, key, c3n));
  _check_nest(arm, u3nt(c3__face, 'x', c3__noun), u3k(atom),
              u3nt(0, 0, 0), c3n, c3n);

  // The strict gate runs its own continuation.
  //
  _check_nest(u3nc(1, c3n), c3__noun, pair, u3nt(0, 0, 0), c3y, c3n);
  u3z(atom);
}

static void
_check_redo(u3_noun sut, u3_noun ref, u3_noun want)
{
  u3_noun arm = u3nc(1, 97);
  u3_noun van = u3nt(u3k(arm), sut, u3nc(u3nt(0, 0, c3y), 0));
  u3_noun cor = u3nt(arm, ref, van);
  u3_noun pro = u3wfu_redo(cor);
  u3_assert(c3y == u3r_sing(want, pro));
  u3z(pro); u3z(cor); u3z(want);
}

static void
_save_redo_leg(u3_noun van, u3_noun sut, u3_noun typ, c3_y kel)
{
  u3_noun leg = u3t(sut);
  u3_noun arg = kel == 135 ? u3k(leg) : u3nc(u3k(leg), 0);
  u3_noun key = u3qfu_rest_key(van, sut, arg);
  u3z(u3z_save(u3z_memo_toss, key, u3k(typ)));
  u3z(arg);
}

static void
_test_redo_hold(c3_y kel, c3_t warm, c3_t loop, c3_t rename)
{
  u3_noun atom = u3nt(c3__atom, 'u', 0);
  u3_noun typ = u3nt(c3__cell, u3k(atom), c3__noun);
  u3_noun leg = u3nc(c3__noun, u3nt(c3__hand, u3k(typ), u3nc(1, 0)));
  u3_noun sut = u3nc(c3__hold, u3k(leg));
  u3_noun ref = rename ? u3nt(c3__cell, u3nt(c3__face, 'x', u3k(atom)), c3__noun)
                       : u3k(typ);
  u3_noun fan = loop ? u3nt(u3k(leg), 0, 0) : 0;
  u3_noun arm = u3nt(7, u3nc(1, 8*kel + 4*warm + 2*loop + rename), u3nc(1, 97));
  u3_noun van = u3nt(u3k(arm), u3k(sut), u3nc(u3nt(fan, 0, c3y), 0));
  if ( warm ) _save_redo_leg(van, sut, typ, kel);
  u3_noun cor = u3nt(arm, u3k(ref), van);
  u3_noun pro = kel == 135 ? u3wfu_redo_135(cor) : u3wfu_redo(cor);
  u3_noun want = !warm || loop ? 97 : (rename ? ref : sut);
  u3_assert(c3y == u3r_sing(pro, want));
  u3z(pro); u3z(cor); u3z(ref); u3z(sut); u3z(leg); u3z(typ); u3z(atom);
}

//  Both holds expand to cells with their respective holds in the tail.
//  Revisiting the ordered pair ends that descent. The renamed head still
//  changes the result of the enclosing hold.
//
static void
_test_redo_recursion(void)
{
  u3_noun sut = u3nt(c3__hold, c3__noun, 's');
  u3_noun ref = u3nt(c3__hold, c3__noun, 'r');
  u3_noun name = u3nt(c3__face, 'x', c3__noun);
  u3_noun a = u3nt(c3__cell, c3__noun, u3k(sut));
  u3_noun b = u3nt(c3__cell, u3k(name), u3k(ref));
  u3_noun arm = u3nc(1, 97);
  u3_noun van = u3nt(u3k(arm), u3k(sut), u3nc(u3nt(0, 0, c3y), 0));
  _save_redo_leg(van, sut, a, 137);
  _save_redo_leg(van, ref, b, 137);
  u3_noun cor = u3nt(arm, ref, van);
  u3_noun pro = u3wfu_redo(cor);
  u3_noun want = u3nt(c3__cell, name, sut);
  u3_assert(c3y == u3r_sing(pro, want));
  u3z(want); u3z(pro); u3z(cor); u3z(a); u3z(b);
}

// A recursive reference labels both fields. Independent runs of head types
// get their own products, and a disjoint tail leaves the decision to Hoon.
//
static void
_test_redo_list(void)
{
  u3_noun hold = u3nt(c3__hold, c3__noun, 'l');
  u3_noun nil = u3nt(c3__atom, 'u', u3nc(0, 0));
  u3_noun any = u3nt(c3__atom, 'u', 0);
  u3_noun cons = u3nt(c3__cell, u3nt(c3__face, 'i', u3k(any)),
                       u3nt(c3__face, 't', u3k(hold)));
  u3_noun exp = u3qf_forq(nil, cons);
  u3_noun arm = u3nc(1, 97);
  for ( c3_w bad = 0; bad <= 1; ++bad ) {
    u3_noun sut = bad ? u3nt(c3__atom, 'u', u3nc(0, 7)) : u3k(nil);
    u3_noun want = u3nt(c3__face, 't', u3k(sut));
    for ( c3_w j = 0; j < 160; ++j ) {
      u3_noun item = u3nt(c3__atom, 'u', u3nc(0, j / 16));
      sut = u3nt(c3__cell, u3k(item), sut);
      want = u3nt(c3__face, j == 159 ? 'l' : 't',
                  u3nt(c3__cell, u3nt(c3__face, 'i', item), want));
    }
    u3_noun van = u3nt(u3k(arm), sut, u3nc(u3nt(0, 0, c3y), 0));
    _save_redo_leg(van, hold, exp, 135);
    u3_noun cor = u3nt(u3k(arm), u3nt(c3__face, 'l', u3k(hold)), van);
    u3_noun pro = u3wfu_redo_135(cor);
    u3_assert(c3y == u3r_sing(pro, bad ? 97 : want));
    // Even an exact entry subject from an unrelated gate must punt.
    u3_noun fol = u3nt(11, u3nt(c3__mean, 1, 0), u3nt(9, 22, u3nc(0, 1)));
    for ( c3_w i = 4; i; --i ) fol = u3nt(8, u3nc(1, i), fol);
    u3_noun worker = u3nc(fol, u3k(u3t(cor)));
    for ( c3_w i = 1; i <= 4; ++i ) worker = u3nc(i, worker);
    u3_assert(u3_none == u3wfu_redo_list_135(worker));
    u3z(worker);
    u3z(pro); u3z(cor); u3z(want);
  }
  u3z(arm); u3z(exp); u3z(cons); u3z(any); u3z(nil); u3z(hold);
}

static void
_test_redo(void)
{
  u3_noun atom = u3nt(c3__atom, 'u', 0);
  u3_noun x = u3nt(c3__face, 'x', u3k(atom));
  u3_noun yx = u3nt(c3__face, 'y', u3k(x));
  u3_noun xyx = u3nt(c3__face, 'x', u3k(yx));
  u3_noun ref = u3nt(c3__face, 'z',
                      u3nt(c3__hint, 0, u3nt(c3__face, 'x',
                        u3nt(c3__face, 'y', u3nt(c3__face, 'x', c3__noun)))));

  //  Both x and xyx overlap. The longest overlap leaves just z to add.
  //
  _check_redo(u3k(xyx), u3k(ref), u3nt(c3__face, 'z', u3k(xyx)));
  _check_redo(u3nt(c3__cell, u3k(x), c3__void),
              u3nt(c3__cell, u3nt(c3__face, 'y', c3__noun), u3k(ref)),
              u3nt(c3__cell, u3k(yx), c3__void));
  _check_redo(u3k(x), u3nt(c3__face, 'x', u3nt(c3__atom, 'u', 0)), u3k(x));

  // Exactly one alternative intersects the subject. Two matching names
  // need +dear's tool-stack set and go through the original gate.
  //
  u3_noun one = u3nt(c3__atom, 'u', u3nc(0, 1));
  u3_noun two = u3nt(c3__atom, 'u', u3nc(0, 2));
  u3_noun named = u3nt(c3__face, 'a', u3k(one));
  u3_noun other = u3nt(c3__face, 'b', two);
  _check_redo(u3k(one), u3qf_forq(named, other), u3k(named));
  _check_redo(u3k(atom), u3qf_forq(named, other), 97);
  u3z(named); u3z(other); u3z(one);

  _test_redo_recursion();
  _test_redo_list();
  for ( c3_y kel = 135; kel <= 137; kel += 2 ) {
    _test_redo_hold(kel, 1, 0, 0);
    _test_redo_hold(kel, 1, 0, 1);
    _test_redo_hold(kel, 0, 0, 1);
    _test_redo_hold(kel, 1, 1, 1);
  }

  //  A residual tail runs the whole arm after releasing the completed head.
  //
  _check_redo(u3nt(c3__cell, u3k(x), u3nt(c3__hold, 7, 8)),
              u3nt(c3__cell, u3k(ref), u3k(x)), 97);
  u3_noun deep = u3k(x);
  for ( c3_w i_w = 0; i_w < 128; ++i_w ) deep = u3nt(c3__face, 'q', deep);
  _check_redo(deep, u3k(ref), 97);
  u3z(ref); u3z(xyx); u3z(yx); u3z(x); u3z(atom);
}

static void
_test_byte_text(void)
{
  const c3_y raw_y[] = {10, 'a', 0, 'b', 0, 10, 10, 'c', 13};
  u3_atom txt = u3i_bytes(sizeof(raw_y), raw_y);
  u3_noun lis = u3qe_leer(txt);
  u3_noun exp = u3nc(0, u3nq(0x620061, 0, 0x0d63, 0));
  u3_assert(c3y == u3r_sing(lis, exp));
  u3z(txt); u3z(lis); u3z(exp);

  const c3_y eol_y[] = {10, 'a', 0, 'b', 13, 10, 10};
  txt = u3i_bytes(sizeof(eol_y), eol_y);
  lis = u3qe_lune(txt);
  exp = u3nq(0, 0x0d620061, 0, 0);
  u3_assert(c3y == u3r_sing(lis, exp));
  u3z(txt); u3z(lis); u3z(exp);
  u3_assert(u3_nul == u3qe_lune(0));

  u3_noun err = u3m_soft(0, u3qe_lune, 'a');
  u3_assert(c3__exit == u3h(err));
  u3z(err);

  const c3_y dat_y[] = {0xff, 0x00, 0x01, 0x02, 0, 3, 0, 4, 5, 6, 7, 8};
  u3_atom big = u3i_bytes(sizeof(dat_y) - 3, dat_y + 3);
  lis = u3nc(0xff, u3nq(0, 0x0100, big, 0));
  txt = u3qc_rap(3, lis);
  exp = u3i_bytes(sizeof(dat_y), dat_y);
  u3_assert(c3y == u3r_sing(txt, exp));
  u3z(lis); u3z(txt); u3z(exp);
}

static void
_test_byte_reverse(void)
{
  const c3_y bytes[] = {0, 1, 0x80, 2, 0xff, 0, 3, 0xfe, 4, 5, 0, 6, 7, 8, 9, 10, 11};
  for ( c3_w len_w = 0; len_w <= sizeof(bytes); len_w++ ) {
    u3_atom src = u3i_bytes(len_w, bytes);
    for ( c3_w wid_w = 0; wid_w <= sizeof(bytes) + 1; wid_w++ ) {
      c3_y expected[sizeof(bytes) + 1] = {0};
      for ( c3_w i_w = 0; i_w < c3_min(len_w, wid_w); i_w++ ) {
        expected[wid_w - 1 - i_w] = bytes[i_w];
      }
      u3_atom want = u3i_bytes(wid_w, expected);
      u3_atom got = u3qc_rev(3, wid_w, src);
      u3_assert(c3y == u3r_sing(want, got));
      u3z(got);
      if ( wid_w == u3r_met(3, src) ) {
        got = u3qc_swp(3, src);
        u3_assert(c3y == u3r_sing(want, got));
        u3z(got);
      }
      u3z(want);
    }
    u3z(src);
  }
}

static void
_test_byte_list(void)
{
  c3_y bytes[4097];
  for ( size_t i = 0; i < sizeof(bytes); i++ ) {
    bytes[i] = (c3_y)i;
  }

  // The nonempty tail stays shared across a cell-cache refill.
  u3m_hate(0);
  u3_noun tail = u3nc(42, 7);
  u3_noun list = u3i_byte_list(sizeof(bytes), bytes, u3k(tail));
  u3_noun next = list;
  for ( size_t i = 0; i < sizeof(bytes); i++ ) {
    u3_assert(bytes[i] == u3h(next));
    next = u3t(next);
  }
  u3_assert(tail == next);
  u3z(list);
  u3_assert(42 == u3h(tail));
  u3z(tail);
  u3z(u3m_love(0));
}

static c3_i
_test_jets(void)
{
  c3_i ret_i = 1;

  _test_arm_axes();
  _test_fond();
  _test_peek();
  _test_toss();
  _test_nest();
  _test_redo();

  _test_bulk_trees();
  _test_byte_text();
  _test_byte_reverse();
  _test_byte_list();
  _test_map_union();
  _test_tree_intersection();
  _test_bif();
  _test_put_rotation();
  _test_tree_find();
  _test_tree_tap();

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

  if ( !_test_json_de_array() ) {
    fprintf(stderr, "test jets: json_de_array: failed\r\n");
    ret_i = 0;
  }
  if ( !_test_json_de_object() ) {
    fprintf(stderr, "test jets: json_de_object: failed\r\n");
    ret_i = 0;
  }
  if ( !_test_json_de_string() ) {
    fprintf(stderr, "test jets: json_de_string: failed\r\n");
    ret_i = 0;
  }
  if ( !_test_hike() ) {
    fprintf(stderr, "test jets: hike: failed\r\n");
    ret_i = 0;
  }
  if ( !_test_noun_order() ) {
    fprintf(stderr, "test jets: noun order: failed\r\n");
    ret_i = 0;
  }
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

  //  GC, failing on a leak
  //
  u3C.wag_h |= u3o_leak_crash;
  u3m_grab();

  fprintf(stderr, "test jets: ok\r\n");
  return 0;
}
