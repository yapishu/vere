/// @file

#include "ivory.h"
#include "noun.h"
#include "ur/ur.h"
#include "vere.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  c3_d          len_d = u3_Ivory_pill_len;
  c3_y*         byt_y = u3_Ivory_pill;
  u3_cue_xeno*  sil_u;
  u3_weak       pil;

  u3C.wag_h |= u3o_hashless;
  u3m_boot_lite(1 << 27);
  sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, len_d, byt_y)) ) {
    printf("*** fail _setup 1\n");
    exit(1);
  }
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) {
    printf("*** fail _setup 2\n");
    exit(1);
  }
}

/* _test_lily(): test small noun parsing.
*/
static void
_test_lily()
{
  c3_l lit_l;

  //  1: value too large should fail
  {
    c3_w big_w[] = {(c3_w)0, (c3_w)0, (c3_w)1};
    u3_noun big = u3i_words(3, big_w);
    u3_noun cod = u3dc("scot", c3__uv, big);

    if ( c3y == u3v_lily(c3__uv, cod, &lit_l) ) {
      printf("*** fail _test_lily-1\n");
      exit(1);
    }
  }

  //  2a: maximum direct atom value should succeed
  {
    u3_noun cod = u3dc("scot", c3__ud, u3a_direct_max);
    if ( (c3n == u3v_lily(c3__ud, cod, &lit_l)) ||
         (u3a_direct_max != lit_l) ) {
      printf("*** fail _test_lily-2a\n");
      exit(1);
    }
  }

  //  2b: value with indirect flag set should fail: "strange lily"
  {
    u3_noun cod = u3dc("scot", c3__ux, u3i_word(u3a_indirect_flag));
    if ( c3y == u3v_lily(c3__ux, cod, &lit_l) ) {
      printf("*** fail _test_lily-2b\n");
      exit(1);
    }
  }

  //  3: small values should work
  {
    u3_noun cod = u3dc("scot", c3__ud, 42);
    if ( (c3n == u3v_lily(c3__ud, cod, &lit_l)) ||
         (42 != lit_l) ) {
      printf("*** fail _test_lily-3\n");
      exit(1);
    }
  }

  //  4: zero should work
  {
    u3_noun cod = u3dc("scot", c3__ud, 0);
    if ( (c3n == u3v_lily(c3__ud, cod, &lit_l)) ||
         (0 != lit_l) ) {
      printf("*** fail _test_lily-4\n");
      exit(1);
    }
  }
}

/* Equal cells with distinct children and different mug cache states
** acquire one representative. Swapping the children gives another.
*/
static void
_test_melt_cells(void)
{
  u3_noun a = u3nc(u3nc(u3i_string("melt test atom"), 7), 42);
  u3_noun b = u3nc(u3nc(u3i_string("melt test atom"), 7), 42);
  u3_noun c = u3nc(42, u3k(u3h(b)));
  u3_noun d;

  (void)u3r_mug(a);
  u3h_put(u3R->cax.per_p, c3__melt, u3nq(a, u3k(b), b, c));
  (void)u3_melt_all(0, 0);

  u3_weak val = u3h_git(u3R->cax.per_p, c3__melt);
  u3_assert(u3_none != val && c3y == u3r_qual(val, &a, &b, &c, &d));
  u3_assert(a == b && b == c && a != d);
  u3_assert(u3h(a) == u3t(d) && u3t(a) == 42 && u3h(d) == 42);
  u3h_del(u3R->cax.per_p, c3__melt);
}

#ifndef U3_MEMORY_DEBUG
/* A rock write can fail after evacuation. The rebuilt core still owns
** one shared copy of a multi-page atom with all its bytes intact.
*/
static void
_test_cram_write_failure(void)
{
  c3_y buf_y[3 * 16384 + 17];
  for ( size_t i = 0; i < sizeof(buf_y); i++ ) {
    buf_y[i] = 1 + i % 251;
  }
  u3_atom a = u3i_bytes(sizeof(buf_y), buf_y);
  u3_atom b = u3i_bytes(sizeof(buf_y), buf_y);
  u3_assert(a != b);
  u3A->roc = u3nt(a, b, u3A->roc);

  // An empty directory has no .urb parent for the rock.
  uv_fs_t req_u;
  u3_assert(0 == uv_fs_mkdtemp(NULL, &req_u, "cram-test-XXXXXX", NULL));
  c3_o ret_o = u3u_cram((c3_c*)req_u.path, u3A->eve_d);
  u3_assert(0 == c3_rmdir(req_u.path));
  uv_fs_req_cleanup(&req_u);

  u3_assert(c3n == ret_o);
  a = u3h(u3A->roc);
  b = u3h(u3t(u3A->roc));
  u3_atom exp = u3i_bytes(sizeof(buf_y), buf_y);
  u3_assert(a == b && c3y == u3r_sing(a, exp));
  u3z(exp);
}
#endif

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_lily();
  _test_melt_cells();
#ifndef U3_MEMORY_DEBUG
  _test_cram_write_failure();
#endif

  fprintf(stderr, "test boot: ok\n");
  return 0;
}
