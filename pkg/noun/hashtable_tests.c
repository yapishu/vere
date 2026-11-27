/// @file

#include "noun.h"
#define TEST_SIZE 100000

// defined in noun/hashtable.c
c3_h _ch_skip_slot(c3_h mug_h, c3_h lef_h);

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 28);
}

/* _test_put_del(): 
*/
static c3_i
_test_put_del()
{
  u3p(u3h_root) har_p = u3h_new();
  c3_i ret_i = 1;

  c3_w i_w;
  for ( i_w = 0; i_w < TEST_SIZE; i_w++ ) {
    u3_noun key = u3i_word(i_w);
    u3_noun val = u3nc(u3_nul, u3k(key));
    u3h_put(har_p, key, val);
    u3z(key);
  }
  // fprintf(stderr, "inserted\r\n");

  for ( i_w = 0; i_w < TEST_SIZE; i_w++ ) {
    u3_noun key = u3i_word(i_w);
    u3_weak val = u3h_get(har_p, key);
    if ( val == u3_none ) {
      fprintf(stderr, "failed insert\r\n");
      ret_i = 0;
    }
    u3z(key);
    u3z(val);
  }
  // fprintf(stderr, "presence\r\n");
  c3_w del_w[4] = {30, 82, 4921, 535};

  for ( i_w = 0; i_w < 4; i_w++ ) {
    u3_noun key = u3i_word(del_w[i_w]);
    u3h_del(har_p, key);
    u3z(key);
  }
  // fprintf(stderr, "deleted\r\n");

  for ( i_w = 0; i_w < 4; i_w++ ) {
    u3_noun key = u3i_word(del_w[i_w]);
    u3_weak val = u3h_get(har_p, key);
    if ( u3_none != val ) {
      fprintf(stderr, "failed delete\r\n");
      ret_i = 0;
      break;
    }
  }
  // fprintf(stderr, "presence two\r\n");
  u3h_free(har_p);
  // fprintf(stderr, "freed\r\n");

  return ret_i;
}

/* _test_bit_manipulation():
*/
static c3_i
_test_bit_manipulation()
{
  c3_i ret_i = 1;

  if ( sizeof(u3_noun) != sizeof(u3h_slot) ) {
    fprintf(stderr, "bit manipulation: wrong size\r\n");
    ret_i = 0;
  }

  u3h_slot a = 0;

  if (u3h_slot_is_null(a) != c3y) {
    fprintf(stderr, "bit manipulation: nullity\r\n");
    ret_i = 0;
  }

  a = u3h_noun_be_warm(a);
  if (u3h_slot_is_warm(a) != c3y) {
    fprintf(stderr, "bit manipulation: warmth\r\n");
    ret_i = 0;
  }

  if (u3h_slot_is_null(a) != c3n) {
    fprintf(stderr, "bit manipulation: nullity 2\r\n");
    ret_i = 0;
  }

  a = u3h_noun_be_cold(a);
  if (u3h_slot_is_warm(a) != c3n) {
    fprintf(stderr, "bit manipulation: coldness\r\n");
    ret_i = 0;
  }

  return ret_i;
}

/* _test_no_cache(): test a hashtable without caching.
*/
static c3_i
_test_no_cache(void)
{
  c3_i ret_i = 1;
  c3_w max_w = 1000;
  c3_w   i_w;

  u3p(u3h_root) har_p = u3h_new();

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3h_put(har_p, i_w, i_w + max_w);
  }

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    if ( (i_w + max_w) != u3h_get(har_p, i_w) ) {
      fprintf(stderr, "bit test_no_cache: get failed\r\n");
      ret_i = 0;
    }
  }

  u3h_free(har_p);
  return ret_i;
}

/* _test_skip_slot():
*/
static c3_i
_test_skip_slot(void)
{
  c3_i ret_i = 1;

  //  root table
  {
    c3_h mug_h = 0x17 << 25;
    c3_h res_h = _ch_skip_slot(mug_h, 25);

    if ( (0x18 << 25) != res_h ) {
      fprintf(stderr, "bit skip_slot (a): failed\r\n");
      ret_i = 0;
    }
  }

  {
    c3_h mug_h = 63 << 25; //  6 bits, all ones
    c3_h res_h = _ch_skip_slot(mug_h, 25);

    if ( 0 != res_h ) {
      fprintf(stderr, "bit skip_slot (b): failed\r\n");
      ret_i = 0;
    }
  }

  //  child nodes
  {
    c3_h mug_h = 17 << 20;
    c3_h res_h = _ch_skip_slot(mug_h, 20);

    if ( (18 << 20) != res_h ) {
      fprintf(stderr, "bit skip_slot (c): failed\r\n");
      ret_i = 0;
    }
  }

  {
    c3_h mug_h = 31 << 20; //  5 bits, all ones
    c3_h res_h = _ch_skip_slot(mug_h, 20);
    u3_assert((1 << 25) == res_h);

    if ( (1 << 25) != res_h ) {
      fprintf(stderr, "bit skip_slot (d): failed\r\n");
      ret_i = 0;
    }
  }

  return ret_i;
}

/* _test_cache_trimming(): ensure a caching hashtable removes stale items.
*/
static c3_i
_test_cache_trimming(void)
{
  c3_i ret_i = 1;
  c3_w max_w = 2000000; // big number
  //c3_w max_w = 348000; // caused a leak before
  c3_w i_w, fil_w = max_w / 10;

  u3p(u3h_root) har_p = u3h_new_cache(fil_w);
  u3h_root*     har_u = u3to(u3h_root, har_p);

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3_noun cel = u3nc(i_w, i_w);
    u3h_put(har_p, cel, cel);
  }

  {
    // last thing we put in is still there
    c3_w  las_w = max_w - 1;
    u3_noun key = u3nc(las_w, las_w);
    u3_noun val = u3h_get(har_p, key);
    u3z(key);

    if ( las_w != u3t(val) ) {
      fprintf(stderr, "cache_trimming (a): fail\r\n");
      ret_i = 0;
    }

    if ( fil_w != har_u->use_w ) {
      fprintf(stderr, "cache_trimming (b): fail %"PRIc3_w" != %"PRIc3_w"\r\n",
              fil_w, har_u->use_w );
      ret_i = 0;
    }

    u3z(val);
  }

  u3h_free(har_p);
  return ret_i;
}

/* _test_clock_order(): move through sparse children and wrap at the root.
*/
static void
_test_clock_order(void)
{
  u3p(u3h_root) har_p = u3h_new();
  u3h_root*     har_u = u3to(u3h_root, har_p);
  // 4749 has root slot 0. The others have root slot 63 and child slots
  // 0, 6, 19 and 31, respectively.
  c3_h keys_h[] = {4749, 969, 720, 266, 8096};
  c3_h evict_h[] = {720, 266, 8096, 969, 4749};
  c3_h after_h[] = {0x7e700000, 0x7f400000, 0, 0, 0x02000000};

  for ( c3_h i_h = 0; i_h < 5; i_h++ ) {
    u3h_put(har_p, keys_h[i_h], keys_h[i_h]);
  }
  har_u->arm_u.mug_h = (63U << 25) | (3U << 20) | 5U;

  for ( c3_h i_h = 0; i_h < 5; i_h++ ) {
    u3h_trim_to(har_p, 4 - i_h);
    u3_assert(u3_none == u3h_git(har_p, evict_h[i_h]));
    u3_assert(after_h[i_h] == har_u->arm_u.mug_h);
    if ( 0 == i_h ) {
      u3_assert(4749 == u3h_git(har_p, 4749));
    }
  }
  u3h_free(har_p);
}

/* Check both the clock and ownership when a discarded value shares a
** child with its key and with values that stay in the table.
*/
static void
_test_batched_put(void)
{
  const c3_w max_w = 4096;
  u3p(u3h_root) ref_p = u3h_new_cache(max_w);
  u3p(u3h_root) har_p = u3h_new_cache(max_w);
  u3_noun       lit = u3nc(42, 17);

  for ( c3_w i_w = 0; i_w < max_w + 103; i_w++ ) {
    u3_noun key = u3nc(i_w, u3k(lit));
    u3_noun val = u3nc(u3k(key), u3k(lit));
    u3_weak del = u3h_put_get(ref_p, key, u3k(val));
    if ( u3_none != del ) {
      u3z(del);
      u3h_trim_to(ref_p, max_w - 15);
    }
    u3h_put_batched(har_p, key, val);
    u3z(key);
  }
  u3_assert(u3h_wyt(ref_p) == u3h_wyt(har_p));
  for ( c3_w i_w = 0; i_w < max_w + 103; i_w++ ) {
    u3_noun key = u3nc(i_w, u3k(lit));
    u3_assert(u3h_git(ref_p, key) == u3h_git(har_p, key));
    u3z(key);
  }
  u3h_free(ref_p);
  u3h_free(har_p);
  u3_assert(1 == u3a_use(lit));
  u3z(lit);
}

/* Reserve one root slot so the clock reaches the incoming key after all
** the warm residents. A hit makes that key the first victim. The next
** insertion gets a second chance when no lookup hits after the trim.
*/
static void
_test_memo_clock(void)
{
  u3m_hate(0);
  u3h_free(u3R->cax.har_p);
  u3p(u3h_root) har_p = u3R->cax.har_p = u3h_new_cache(4096);
  u3h_root*     har_u = u3to(u3h_root, har_p);
  u3_noun       key = 0, hit = 0, fresh = 0;

  while ( har_u->use_w < har_u->max_w ) {
    if ( 63 == (u3r_mug(key) >> 25) ) {
      fresh = key;
    }
    else {
      hit = key;
      u3z_save(u3z_memo_toss, key, key);
    }
    ++key;
  }
  u3_assert(63 == (u3r_mug(fresh) >> 25));
  u3_assert(hit == u3z_find(u3z_memo_toss, hit));
  u3z_save(u3z_memo_toss, fresh, fresh);
  u3_assert(u3_none == u3z_find(u3z_memo_toss, fresh));
  u3z_save(u3z_memo_toss, fresh, fresh);
  u3_assert(c3y == u3h_slot_is_warm(har_u->sot_w[63]));
  u3_assert(fresh == u3z_find(u3z_memo_toss, fresh));
  u3_assert(u3_none == u3z_find(u3z_memo_toss, key));
  u3z_save(u3z_memo_toss, fresh, fresh);
  u3_assert(c3y == u3h_slot_is_warm(har_u->sot_w[63]));
  u3z(u3m_love(0));
}

/* A full release queue hands the decrement and allocator trace to u3z().
*/
static void
_test_release_spill(void)
{
  u3_noun lit = u3nc(42, 17), som[256];
  c3_h    wag_h = u3C.wag_h;

  for ( c3_w i_w = 0; i_w < c3_array_len(som); i_w++ ) {
    som[i_w] = u3k(lit);
  }
  u3C.wag_h |= u3o_debug_cpu;
  u3a_lose_many(c3_array_len(som), som);
  u3C.wag_h = wag_h;
  u3_assert(1 == u3a_use(lit));
  u3z(lit);
}

/* _test_cache_replace_value():
*/
static c3_i
_test_cache_replace_value(void)
{
  c3_i ret_i = 1;
  c3_w max_w = 100;
  c3_w   i_w;

  u3p(u3h_root) har_p = u3h_new_cache(max_w);
  u3h_root*     har_u = u3to(u3h_root, har_p);

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3h_put(har_p, i_w, i_w + max_w);
  }

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3h_put(har_p, i_w, i_w + max_w + 1);
  }

  if ( (2 * max_w) != u3h_get(har_p, max_w - 1) ) {
    fprintf(stderr, "cache_replace (a): fail\r\n");
    ret_i = 0;
  }
  if ( max_w != har_u->use_w ) {
    fprintf(stderr, "cache_replace (b): fail\r\n");
    fprintf(stderr, "cache_replace (b): fail %"PRIc3_w" %"PRIc3_w"\r\n",
            max_w, har_u->use_w );
    ret_i = 0;
  }

  u3h_free(har_p);
  return ret_i;
}

static c3_i
_test_hashtable(void)
{
  c3_i ret_i = 1;

  ret_i &= _test_bit_manipulation();
  ret_i &= _test_no_cache();
  ret_i &= _test_skip_slot();
  ret_i &= _test_cache_trimming();
  _test_clock_order();
  _test_memo_clock();
  u3m_hate(0);
  _test_batched_put();
  _test_release_spill();
  u3m_hate(0);
  _test_batched_put();
  u3z(u3m_love(0));
  u3z(u3m_love(0));
  ret_i &= _test_cache_replace_value();
  ret_i &= _test_put_del();

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_hashtable() ) {
    fprintf(stderr, "test_hashtable: failed\r\n");
    exit(1);
  }

  //  GC, failing on a leak
  //
  u3C.wag_h |= u3o_leak_crash;
  u3m_grab();

  fprintf(stderr, "test_hashtable: ok\r\n");

  return 0;
}
