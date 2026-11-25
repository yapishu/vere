/// @file

#include "noun.h"
#include "ur/bitstream.h"

static const char cord[] =
  "http-client-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz";

static uint64_t
_expected(u3_noun noun, u3_atom text)
{
  uint64_t mask = 1;  // _
  if ( c3y == u3ud(noun) ) {
    mask |= 2;  // @
    if ( noun == 0 ) mask |= 4;
    if ( noun == 42 ) mask |= 8;
    if ( c3y == u3r_sing(noun, text) ) mask |= 16;
    if ( u3r_met(0, noun) == 64 && u3r_chub(0, noun) == UINT64_MAX ) mask |= 32;
    return mask;
  }
  u3_noun h = u3h(noun), t = u3t(noun);
  if ( h == 0 ) mask |= 64;
  if ( h == 0 && t == 1 ) mask |= 128;
  if ( c3y == u3du(h) && u3h(h) == 0 && t == 0 ) mask |= 256;
  return mask | _expected(h, text) | _expected(t, text);
}

static void
_compare(const u3_jam_query* q, u3_cue_xeno* dec, u3_atom text, size_t len, const c3_y* bytes)
{
  u3_weak noun = len ? u3s_cue_xeno_with(dec, len, bytes) : u3_none;
  uint64_t matches = UINT64_MAX;
  u3_assert((c3y == u3s_jam_query_scan(q, len, bytes, &matches)) == (noun != u3_none));
  u3_assert(matches == (noun == u3_none ? 0 : _expected(noun, text)));
  if ( noun != u3_none ) u3z(noun);
}

static void
_test_decode(void)
{
  const char* error;
  char term[sizeof(cord) + 1] = "%";
  memcpy(term+1, cord, sizeof(cord));
  const char* patterns[] = {"_", "@", "0", "42", term, "18446744073709551615",
                            "[0 _]", "[0 1]", "[[0 _] 0]"};
  u3_jam_query* q = u3s_jam_query_compile(9, patterns, &error);
  u3_assert(q && !error);
  memset(term, 0, sizeof(term));  // The compiled query owns its literals.
  u3_atom text = u3i_string(cord);
  u3_cue_xeno* dec = u3s_cue_xeno_init_with(1, 32);
  _compare(q, dec, text, 0, NULL);
  for ( unsigned len = 1; len <= 2; len++ ) {
    for ( unsigned i = 0; i < (1u << (8*len)); i++ ) {
      uint8_t bytes[] = {i, i >> 8};
      _compare(q, dec, text, len, bytes);
    }
  }

  // Truncate each payload at every byte boundary, with and without a tail.
  // The larger widths include declared high zeros and a multi-chunk cord.
  const unsigned widths[] = {64, 74, 259, 512, 8*(sizeof(cord)-1)-1, 951};
  for ( size_t i = 0; i < sizeof(widths)/sizeof(*widths); i++ ) {
    for ( unsigned cell = 0; cell <= 1; cell++ ) {
      uint8_t data[120] = {0};
      unsigned width = widths[i];
      if ( width == 64 ) memset(data, 0xff, 8);
      else if ( width >= 8*(sizeof(cord)-1)-1 ) memcpy(data, cord, sizeof(cord)-1);
      else data[0] = 42;
      ur_bsw_t w = {0};
      ur_bsw_init(&w, 13, 21);
      if ( cell ) ur_bsw_cell(&w);
      ur_bsw_atom_bytes(&w, width, data);
      if ( cell ) ur_bsw_atom64(&w, 0, 0);
      uint8_t* bytes;
      uint64_t len;
      ur_bsw_done(&w, &len, &bytes);
      for ( size_t cut = 1; cut <= len; cut++ ) _compare(q, dec, text, cut, bytes);
      c3_free(bytes);
    }
  }
  u3s_cue_xeno_done(dec);
  u3z(text);
  u3s_jam_query_free(q);
}

static void
_test_shared(void)
{
  // Each tail refers to its completed head. The expanded tree has 2^depth leaves.
  const unsigned depth = 20000;
  ur_bsw_t w = {0};
  ur_bsw_init(&w, 13, 21);
  for ( unsigned i = 0; i < depth; i++ ) ur_bsw_cell(&w);
  ur_bsw_atom64(&w, 7, 'x');
  for ( unsigned i = depth; i; i-- ) {
    unsigned at = 2*i;
    ur_bsw_back64(&w, 32 - __builtin_clz(at), at);
  }
  uint64_t len, matches;
  uint8_t* bytes;
  ur_bsw_done(&w, &len, &bytes);
  const char* patterns[64] = {"[%x %x]", "[[%x %x] [%x %x]]", "[%y %x]"};
  for ( unsigned i = 3; i < 64; i++ ) patterns[i] = "%x";
  const char* error;
  u3_jam_query* q = u3s_jam_query_compile(64, patterns, &error);
  u3_assert(q && !error);
  u3_assert(c3y == u3s_jam_query_scan(q, len, bytes, &matches) && matches == (UINT64_MAX ^ 4));
  c3_free(bytes);

  // A match in the head is provisional until the tail is valid.
  ur_bsw_init(&w, 13, 21);
  ur_bsw_cell(&w);
  ur_bsw_atom64(&w, 7, 'x');
  ur_bsw_back64(&w, 0, 0);  // still-open root
  ur_bsw_done(&w, &len, &bytes);
  u3_assert(c3n == u3s_jam_query_scan(q, len, bytes, &matches) && matches == 0);
  c3_free(bytes);
  u3s_jam_query_free(q);
}

static void
_test_compile(void)
{
  const char* error;
  const char* invalid[] = {"", "[0]", "[_ @ 1]", "%", "0 _", "18446744073709551616"};
  for ( size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++ ) {
    u3_assert(!u3s_jam_query_compile(1, &invalid[i], &error) && error);
  }
  u3_assert(!u3s_jam_query_compile(0, NULL, &error));
  u3_assert(!u3s_jam_query_compile(65, NULL, &error));

  char numbers[63][4];
  const char* literals[63];
  for ( unsigned i = 0; i < 63; i++ ) {
    snprintf(numbers[i], sizeof(numbers[i]), "%u", i);
    literals[i] = numbers[i];
  }
  u3_jam_query* query = u3s_jam_query_compile(62, literals, &error);
  u3_assert(query && !error);
  u3s_jam_query_free(query);
  u3_assert(!u3s_jam_query_compile(63, literals, &error) && error);

  // Each pattern tests an independent list element. Eight elements require
  // a distinct transition state for each of the 256 subsets of matches.
  const char* patterns[8];
  char indexed[8][64] = {"[%x _]"};
  patterns[0] = indexed[0];
  for ( unsigned i = 1; i < 8; ++i ) {
    snprintf(indexed[i], sizeof(indexed[i]), "[_ %s]", patterns[i - 1]);
    patterns[i] = indexed[i];
  }
  query = u3s_jam_query_compile(7, patterns, &error);
  u3_assert(query && !error);
  u3s_jam_query_free(query);
  u3_assert(!u3s_jam_query_compile(8, patterns, &error) && error);
}

int
main(void)
{
  u3m_boot_lite(1 << 24);
  _test_compile();
  _test_decode();
  _test_shared();
  fputs("jam-search ok\n", stderr);
  return 0;
}
