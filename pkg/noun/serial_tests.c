/// @file

#include "noun.h"
#include "cue_pages.h"
#include "ur/ur.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 24);
}

static void
_byte_print(c3_d        out_d,
            c3_y*       out_y,
            c3_w        len_w,
            const c3_y* byt_y)
{
  c3_d i_d;

  fprintf(stderr, "  actual: { ");
  for ( i_d = 0; i_d < out_d; i_d++ ) {
    fprintf(stderr, "0x%x, ", out_y[i_d]);
  }
  fprintf(stderr, "}\r\n");
  fprintf(stderr, "  expect: { ");
  for ( i_d = 0; i_d < len_w; i_d++ ) {
    fprintf(stderr, "0x%x, ", byt_y[i_d]);
  }
  fprintf(stderr, "}\r\n");
}

static c3_i
_test_jam_spec(const c3_c* cap_c,
               u3_noun       ref,
               c3_w        len_w,
               const c3_y* byt_y)
{
  c3_i  ret_i = 1;
  c3_d  out_d;
  c3_y* out_y;

  {
    u3s_jam_xeno(ref, &out_d, &out_y);

    if ( out_d != len_w || 0 != memcmp(out_y, byt_y, len_w) ) {
      fprintf(stderr, "\033[31mjam xeno %s fail\033[0m\r\n", cap_c);
      _byte_print(out_d, out_y, len_w, byt_y);
      ret_i = 0;
    }

    free(out_y);
  }

  {
    u3i_slab sab_u;
    c3_w     bit_w = u3s_jam_fib(&sab_u, ref);

    out_d = ((c3_d)bit_w + 0x7) >> 3;
    //  XX assumes little-endian
    //
    out_y = sab_u.buf_y;

    if ( out_d != len_w || 0 != memcmp(out_y, byt_y, len_w) ) {
      fprintf(stderr, "\033[31mjam fib %s fail\033[0m\r\n", cap_c);
      _byte_print(out_d, out_y, len_w, byt_y);
      ret_i = 0;
    }

    u3i_slab_free(&sab_u);
  }

  return ret_i;
}

static c3_i
_test_cue_spec(const c3_c* cap_c,
               u3_noun       ref,
               c3_w        len_w,
               const c3_y* byt_y)
{
  c3_i ret_i = 1;

  {
    u3_noun pro = u3m_soft(0, u3s_cue_atom, u3i_bytes(len_w, byt_y));
    u3_noun tag, out;

    u3x_cell(pro, &tag, &out);

    if ( u3_blip != tag ) {
      fprintf(stderr, "\033[31mcue %s fail 1\033[0m\r\n", cap_c);
      ret_i = 0;
    }
    else if ( c3n == u3r_sing(ref, out) ) {
      fprintf(stderr, "\033[31mcue %s fail 2\033[0m\r\n", cap_c);
      u3m_p("ref", ref);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_noun out;

    if ( u3_none == (out = u3s_cue_xeno(len_w, byt_y)) ) {
      fprintf(stderr, "\033[31mcue %s fail 3\033[0m\r\n", cap_c);
      ret_i = 0;
    }
    else if ( c3n == u3r_sing(ref, out) ) {
      fprintf(stderr, "\033[31mcue %s fail 4\033[0m\r\n", cap_c);
      u3m_p("ref", ref);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
  }

  return ret_i;
}

// The independent ur arena interns its own nodes and runs its own jam traversal.
static void
_check_canonical(ur_root_t* arena, ur_nref ref, u3_noun noun)
{
  c3_d len, size;
  c3_y *gold, *bytes;
  c3_d bits = ur_jam(arena, ref, &len, &gold);
  u3_assert(bits == u3s_jam_xeno(noun, &size, &bytes));
  u3_assert(size == len && !memcmp(gold, bytes, len));
  c3_free(bytes);
  u3i_slab slab;
  u3_assert(bits == u3s_jam_fib(&slab, noun));
  u3_assert(!memcmp(gold, slab.buf_y, len));
  u3i_slab_free(&slab);
  c3_free(gold);
}

static void
_test_shared_jam(ur_root_t* arena)
{
  c3_d value = UINT64_C(0xc001d00d89abcdef);
  ur_nref ref = ur_coin64(arena, value);
  u3_noun a = u3i_chub(value), b = u3i_chub(value);
  // Both towers share within themselves, but are allocated independently.
  for ( unsigned i = 0; i < 72; ++i ) {
    a = u3nc(u3k(a), a);
    b = u3nc(u3k(b), b);
    ref = ur_cons(arena, ref, ref);
  }
  u3_assert(a != b);
  u3_noun root = u3nt(a, b, 173);
  ref = ur_cons(arena, ref, ur_cons(arena, ref, 173));
  _check_canonical(arena, ref, root);
  u3a_defer_open();
  u3_noun twice = u3nc(u3k(root), u3k(root));
  _check_canonical(arena, ur_cons(arena, ref, ref), twice);
  u3z(twice);
  u3a_defer_close();
  u3m_hate(0);
  _check_canonical(arena, ref, root);
  u3z(u3m_love(0));
  u3z(root);

  // Distinct equal-length heap atoms have the same 31-bit mug on both looms.
  c3_d x = UINT64_C(0xc001d00d0000b33f), y = UINT64_C(0xc001d00d00012a3e);
  a = u3i_chub(x); b = u3i_chub(y);
  u3_assert(u3r_mug(a) == u3r_mug(b) && c3n == u3r_sing(a, b));
  root = u3nc(u3nc(u3k(a), u3k(b)), u3nc(b, a));
  ur_nref ax = ur_coin64(arena, x), ay = ur_coin64(arena, y);
  ref = ur_cons(arena, ur_cons(arena, ax, ay), ur_cons(arena, ay, ax));
  // Deep input also crosses successive class-table growth boundaries.
  for ( unsigned i = 0; i < 4096; ++i ) {
    root = u3nc(root, i);
    ref = ur_cons(arena, ref, i);
  }
  _check_canonical(arena, ref, root);
  u3z(root);
}

/* Check both cue entry points against a noun built independently of the
** bitstream. Reuse the external dictionary across successes and exits.
*/
typedef struct {
  u3_cue_xeno* decoder;
  ur_root_t* arena;
  u3_cue_view* view;
} _cue_test;

static void
_check_stream(_cue_test* test, ur_bsw_t* writer, u3_weak expected)
{
  c3_d len;
  c3_y* bytes;
  ur_bsw_done(writer, &len, &bytes);
  if ( expected != u3_none ) {
    ur_nref ref;
    u3_assert(ur_cue_good == ur_cue(test->arena, len, bytes, &ref));
    _check_canonical(test->arena, ref, expected);
  }
  u3_noun result = u3m_soft(0, u3s_cue_atom, u3i_bytes(len, bytes));
  u3_weak external = u3s_cue_xeno_with(test->decoder, len, bytes);
  u3_cue_view* view = test->view;
  c3_o opened = u3s_cue_view_open(view, len, bytes);
  u3_assert((opened == c3y) == (expected != u3_none));
  for ( c3_w axis = 0; axis < 32; axis++ ) {
    u3_weak want = (axis && expected != u3_none) ? u3r_at(axis, expected) : u3_none;
    u3_weak got = u3s_cue_view_at(view, axis);
    u3_assert((got == u3_none) == (want == u3_none));
    if ( got != u3_none ) {
      u3_assert(c3y == u3r_sing(want, got));
      u3z(got);
    }
  }
  if ( expected == u3_none ) {
    u3_assert(u3h(result) == c3__exit && external == u3_none);
  }
  else {
    u3_assert(u3h(result) == 0 && external != u3_none);
    u3_assert(c3y == u3r_sing(expected, u3t(result)));
    u3_assert(c3y == u3r_sing(expected, external));
    u3z(external);
  }
  u3z(result);
  c3_free(bytes);
}

typedef struct {
  uint32_t rng;
  unsigned used;
  c3_d starts[64];
  u3_noun values[64];
} _cue_generator;

static uint32_t
_sample(_cue_generator* gen)
{
  uint32_t r = gen->rng;
  r ^= r << 13; r ^= r >> 17; r ^= r << 5;
  return gen->rng = r;
}

// Build the expected noun while writing a prefix stream. Backreferences pick
// completed values by index in this generator, independently of cue's lookup.
static u3_noun
_write_tree(ur_bsw_t* writer, _cue_generator* gen, unsigned depth)
{
  c3_d start = writer->bits;
  uint32_t draw = _sample(gen);
  if ( gen->used && draw % 7 == 0 ) {
    unsigned i = _sample(gen) % c3_min(gen->used, 64);
    c3_d bit = gen->starts[i];
    ur_bsw_back64(writer, bit ? 64 - __builtin_clzll(bit) : 0, bit);
    return u3k(gen->values[i]);
  }

  u3_noun value;
  if ( depth && draw % 4 != 0 ) {
    ur_bsw_cell(writer);
    u3_noun head = _write_tree(writer, gen, depth - 1);
    value = u3nc(head, _write_tree(writer, gen, depth - 1));
  }
  else {
    c3_d atom = draw % 3 ? draw % 257 : ((c3_d)draw << 32) | _sample(gen);
    ur_bsw_atom64(writer, atom ? 64 - __builtin_clzll(atom) : 0, atom);
    value = u3i_chub(atom);
  }
  unsigned i = gen->used++ % 64;
  gen->starts[i] = start;
  gen->values[i] = value;
  return value;
}

// Compare complete results and rejection against the independent ur arena
// for every input small enough to exhaust in a unit test.
static void
_test_short_cue(_cue_test* test)
{
  for ( unsigned len = 1; len <= 2; ++len ) {
    for ( unsigned word = 0; word < (1U << (8 * len)); ++word ) {
      c3_y bytes[2] = {word, word >> 8};
      ur_nref ref;
      ur_cue_res_e status = ur_cue(test->arena, len, bytes, &ref);
      u3_weak noun = u3s_cue_xeno_with(test->decoder, len, bytes);
      u3_assert((status == ur_cue_good) == (noun != u3_none));
      if ( noun != u3_none ) _check_canonical(test->arena, ref, noun);
      c3_o open = u3s_cue_view_open(test->view, len, bytes);
      u3_assert((open == c3y) == (noun != u3_none));
      if ( open == c3y ) {
        u3_noun copy = u3s_cue_view_at(test->view, 1);
        u3_assert(c3y == u3r_sing(noun, copy));
        u3z(copy);
      }
      // The atom entry point obtains its byte length from its highest bit.
      if ( bytes[len - 1] ) {
        u3_noun result = u3m_soft(0, u3s_cue_atom, word);
        c3_m error = (status == ur_cue_good) ? 0
                   : (status == ur_cue_meme) ? c3__meme : c3__exit;
        u3_assert(u3h(result) == error);
        if ( noun != u3_none ) u3_assert(c3y == u3r_sing(noun, u3t(result)));
        u3z(result);
      }
      if ( noun != u3_none ) u3z(noun);
    }
  }
}

// Exercise all directory heights without allocating the intervening jam bytes.
// Each page has two starts. Missing pages, unset bits and future pages have no rank.
static void
_test_cue_page_ranks(void)
{
  const c3_d numbers[] = {
    0, 1, 4, 5, 67, 68, 69, 130, 4099, 4100, 4101,
    (UINT64_C(1) << 30) + 4, (UINT64_C(1) << 54) + 4
  };
  size_t count = sizeof(numbers) / sizeof(numbers[0]);
  _cue_directory* dir = 0;
  _cue_page pages[sizeof(numbers) / sizeof(numbers[0])];
  for ( unsigned pass = 0; pass < 2; ++pass ) {
    if ( dir ) dir->height = 0;
    for ( size_t i = 0; i < count; ++i ) {
      c3_d bit = numbers[i] << 9;
      _cue_page_init(pages, i, numbers[i], 2 * i, &dir, 1);
      _cue_page_mark(&pages[i], bit, 2 * i);
      _cue_page_mark(&pages[i], bit + 511, 2 * i + 1);
      for ( size_t j = 0; j <= i; ++j ) {
        c3_d at = numbers[j] << 9;
        u3_assert(_cue_page_find(pages, i + 1, dir, at) == 2 * j);
        u3_assert(_cue_page_find(pages, i + 1, dir, at + 511) == 2 * j + 1);
        u3_assert(_cue_page_find(pages, i + 1, dir, at + 510) == SIZE_MAX);
        if ( j && numbers[j] > numbers[j - 1] + 1 )
          u3_assert(_cue_page_find(pages, i + 1, dir, at - 1) == SIZE_MAX);
      }
      u3_assert(_cue_page_find(pages, i + 1, dir, bit + 512) == SIZE_MAX);
    }
  }
  _cue_directory_free(dir, 1);
}

static void
_test_cue_positions(void)
{
  _cue_test test = { .decoder = u3s_cue_xeno_init_with(1, 2), .arena = ur_root_init() };
  _test_shared_jam(test.arena);
  test.view = u3s_cue_view_new();
  _test_cue_page_ranks();
  _test_short_cue(&test);
  // A wide encoded payload can represent a direct atom on either loom.
  {
    c3_d values[] = {0, (UINT64_C(1) << 57) | 9, UINT64_C(1) << 62};
    for ( size_t i = 0; i < sizeof(values) / sizeof(*values); ++i ) {
      c3_y payload[12] = {0};
      for ( unsigned j = 0; j < 8; ++j ) payload[j] = values[i] >> (8 * j);
      ur_bsw_t bits = {0};
      ur_bsw_init(&bits, 13, 21);
      ur_bsw_atom_bytes(&bits, 96, payload);
      u3_noun expected = u3i_chub(values[i]);
      _check_stream(&test, &bits, expected);
      u3z(expected);
    }
  }

  // The prefix is [ [42 7] [ back-to-42 ... ] ]. An offset may name a
  // completed atom or cell. Open cells, payload bits and the backreference
  // token itself have no dictionary value.
  for ( c3_d target = 0; target < 80; target++ ) {
    ur_bsw_t writer = {0};
    ur_bsw_init(&writer, 13, 21);
    ur_bsw_cell(&writer);
    c3_d pair = writer.bits;
    ur_bsw_cell(&writer);
    c3_d first = writer.bits;
    ur_bsw_atom64(&writer, 6, 42);
    c3_d second = writer.bits;
    ur_bsw_atom64(&writer, 3, 7);
    ur_bsw_cell(&writer);
    ur_bsw_back64(&writer, c3_bits_word(first), first);
    ur_bsw_back64(&writer, c3_bits_word(target), target);
    u3_weak value = target == pair ? u3nc(42, 7)
                   : target == first ? 42 : target == second ? 7 : u3_none;
    u3_weak expected = value == u3_none ? u3_none : u3nt(u3nc(42, 7), 42, value);
    _check_stream(&test, &writer, expected);
    if ( expected != u3_none ) u3z(expected);
  }

  // Skip many empty pages with a single atom, then refer to completed
  // entries on both sides of the gap in a nonmonotonic order.
  {
    ur_bsw_t writer = {0};
    c3_d starts[128];
    u3_noun values[128];
    c3_y payload[4096];
    memset(payload, 0xa5, sizeof(payload));
    ur_bsw_init(&writer, 13, 21);
    for ( unsigned i = 0; i < 128; i++ ) {
      ur_bsw_cell(&writer);
      starts[i] = writer.bits;
      if ( i == 64 ) {
        ur_bsw_atom_bytes(&writer, sizeof(payload) * 8, payload);
        values[i] = u3i_bytes(sizeof(payload), payload);
      }
      else {
        ur_bsw_atom64(&writer, c3_bits_word(i), i);
        values[i] = i;
      }
    }
    for ( unsigned i = 0; i < 128; i++ ) {
      c3_d bit = starts[(i * 73) % 128];
      ur_bsw_cell(&writer);
      ur_bsw_back64(&writer, c3_bits_word(bit), bit);
    }
    ur_bsw_atom64(&writer, 0, 0);
    u3_noun expected = 0;
    for ( unsigned i = 128; i-- > 0; ) {
      expected = u3nc(u3k(values[(i * 73) % 128]), expected);
    }
    for ( unsigned i = 128; i-- > 0; ) expected = u3nc(values[i], expected);
    _check_stream(&test, &writer, expected);
    u3z(expected);
  }

  // A backreference into the previous input must fail on a reused handle.
  ur_bsw_t writer = {0};
  ur_bsw_init(&writer, 13, 21);
  ur_bsw_back64(&writer, 2, 2);
  _check_stream(&test, &writer, u3_none);
  for ( uint32_t seed = 1; seed <= 512; seed++ ) {
    _cue_generator gen = { .rng = seed };
    ur_bsw_t bits = {0};
    ur_bsw_init(&bits, 13, 21);
    u3_noun expected = _write_tree(&bits, &gen, 9);
    _check_stream(&test, &bits, expected);
    u3z(expected);
  }
  ur_root_free(test.arena);
  u3s_cue_xeno_done(test.decoder);
  // Cue supplies missing high bits of a final atom as zeros.
  {
    ur_bsw_t writer = {0};
    c3_y payload[64] = { 123 };
    ur_bsw_init(&writer, 13, 21);
    ur_bsw_atom_bytes(&writer, 512, payload);
    c3_d len;
    c3_y* bytes;
    ur_bsw_done(&writer, &len, &bytes);
    u3_assert(len > 10);
    u3_assert(c3y == u3s_cue_view_open(test.view, 10, bytes));
    u3_assert(123 == u3s_cue_view_at(test.view, 1));
    u3_assert(123 == u3s_cue_xeno(10, bytes));
    c3_free(bytes);
  }
  // A 96-bit axis selects a shared diamond whose expanded tree has over
  // a trillion leaves. The result owns its cells after invalidation.
  u3_noun diamond = u3nc(1001, 1002);
  for ( unsigned i = 0; i < 40; i++ ) diamond = u3nc(u3k(diamond), diamond);
  u3_noun root = u3k(diamond);
  for ( unsigned i = 0; i < 95; i++ ) root = u3nc(i, root);
  c3_d len;
  c3_y* bytes;
  u3s_jam_xeno(root, &len, &bytes);
  u3_assert(c3y == u3s_cue_view_open(test.view, len, bytes));
  c3_y path[12];
  memset(path, 0xff, sizeof(path));
  u3_noun axis = u3i_bytes(sizeof(path), path);
  u3_weak product = u3s_cue_view_at(test.view, axis);
  u3_assert(c3n == u3s_cue_view_open(test.view, 0, 0));
  u3_assert(u3_none == u3s_cue_view_at(test.view, 1));
  u3s_cue_view_free(test.view);
  c3_free(bytes);
  u3z(axis); u3z(root); u3z(diamond);
  u3_assert(product != u3_none);
  u3_noun at = product;
  for ( unsigned i = 0; i < 40; i++ ) {
    u3_assert(u3h(at) == u3t(at));
    at = u3h(at);
  }
  u3_assert(u3h(at) == 1001 && u3t(at) == 1002);
  u3z(product);
}

static c3_i
_test_jam_roundtrip(void)
{
  c3_i ret_i = 1;

#     define TEST_CASE(a, b)                                        \
        const c3_c* cap_c = a;                                      \
        u3_noun       ref = b;                                      \
        ret_i &= _test_jam_spec(cap_c, ref, sizeof(res_y), res_y);  \
        ret_i &= _test_cue_spec(cap_c, ref, sizeof(res_y), res_y);  \
        u3z(ref);

//  {
//    c3_y res_y[1] = { 0x2 };
//    TEST_CASE("0", 0);
//  }
//
//  {
//    c3_y res_y[1] = { 0xc };
//    TEST_CASE("1", 1);
//  }
//
//  {
//    c3_y res_y[1] = { 0x48 };
//    TEST_CASE("2", 2);
//  }
//
//  {
//    c3_y res_y[6] = { 0xc0, 0x37, 0xb, 0x9b, 0xa3, 0x3 };
//    TEST_CASE("%fast", c3__fast);
//  }
//
//  {
//    c3_y res_y[6] = { 0xc0, 0x37, 0xab, 0x63, 0x63, 0x3 };
//    TEST_CASE("%full", c3__full);
//  }
//
//  {
//    c3_y res_y[1] = { 0x29 };
//    TEST_CASE("[0 0]", u3nc(0, 0));
//  }
//
//  {
//    c3_y res_y[2] = { 0x31, 0x3 };
//    TEST_CASE("[1 1]", u3nc(1, 1));
//  }
//
//  {
//    c3_y res_y[2] = { 0x31, 0x12 };
//    TEST_CASE("[1 2]", u3nc(1, 2));
//  }
//
//  {
//    c3_y res_y[2] = { 0x21, 0xd1 };
//    TEST_CASE("[2 3]", u3nc(2, 3));
//  }
//
//  {
//    c3_y res_y[11] = { 0x1, 0xdf, 0x2c, 0x6c, 0x8e, 0xe, 0x7c, 0xb3, 0x3a, 0x36, 0x36 };
//    TEST_CASE("[%fast %full]", u3nc(c3__fast, c3__full));
//  }
//
//  {
//    c3_y res_y[2] = { 0x71, 0xcc };
//    TEST_CASE("[1 1 1]", u3nc(1, u3nc(1, 1)));
//  }
//
//  {
//    c3_y res_y[3] = { 0x71, 0x48, 0x34 };
//    TEST_CASE("[1 2 3]", u3nt(1, 2, 3));
//  }

  {
    c3_y res_y[12] = { 0x1, 0xdf, 0x2c, 0x6c, 0x8e, 0x1e, 0xf0, 0xcd, 0xea, 0xd8, 0xd8, 0x93 };
    TEST_CASE("[%fast %full %fast]", u3nc(c3__fast, u3nc(c3__full, c3__fast)));
  }

  {
    c3_y res_y[3] = { 0xc5, 0x48, 0x34 };
    TEST_CASE("[[1 2] 3]", u3nc(u3nc(1, 2), 3));
  }

  {
    c3_y res_y[5] = { 0xc5, 0xc8, 0x26, 0x27, 0x1 };
    TEST_CASE("[[1 2] [1 2] 1 2]", u3nt(u3nc(1, 2), u3nc(1, 2), u3nc(1, 2)));
  }

  {
    c3_y res_y[6] = { 0xa5, 0x35, 0x19, 0xf3, 0x18, 0x5 };
    TEST_CASE("[[0 0] [[0 0] 1 1] 1 1]", u3nc(u3nc(0, 0), u3nc(u3nc(u3nc(0, 0), u3nc(1, 1)), u3nc(1, 1))));
  }

  {
    c3_y res_y[14] = { 0x15, 0x17, 0xb2, 0xd0, 0x85, 0x59, 0xb8, 0x61, 0x87, 0x5f, 0x10, 0x54, 0x55, 0x5 };
    TEST_CASE("deep", u3nc(u3nc(u3nc(1, u3nc(u3nc(2, u3nc(u3nc(3, u3nc(u3nc(4, u3nc(u3nt(5, 6, u3nc(7, u3nc(u3nc(8, 0), 0))), 0)), 0)), 0)), 0)), 0), 0));
  }

  {
    c3_y inp_y[33] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    c3_y res_y[35] = { 0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8 };
    TEST_CASE("wide", u3i_bytes(sizeof(inp_y), inp_y));
  }

  {
    c3_y inp_y[16] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xc, 0xa8, 0xab, 0x60, 0xef, 0x2d, 0xd, 0x0, 0x0, 0x80 };
    c3_y res_y[19] = { 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x18, 0x50, 0x57, 0xc1, 0xde, 0x5b, 0x1a, 0x0, 0x0, 0x0, 0x1 };
    TEST_CASE("date", u3i_bytes(sizeof(inp_y), inp_y));
  }

  {
    u3_noun a = u3i_string("abcdefjhijklmnopqrstuvwxyz");
    c3_y res_y[32] = {
       0x1, 0xf8,  0xc, 0x13, 0x1b, 0x23, 0x2b, 0x33, 0x53, 0x43, 0x4b,
      0x53, 0x5b, 0x63, 0x6b, 0x73, 0x7b, 0x83, 0x8b, 0x93, 0x9b, 0xa3,
      0xab, 0xb3, 0xbb, 0xc3, 0xcb, 0xd3, 0x87,  0xc, 0x3d,  0x9
    };
    TEST_CASE("alpha", u3nq(u3k(a), 2, 3, a));
  }

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_jam_roundtrip() ) {
    fprintf(stderr, "test jam: failed\r\n");
    exit(1);
  }

  _test_cue_positions();

  //  GC, failing on a leak
  //
  u3C.wag_h |= u3o_leak_crash;
  u3m_grab();

  fprintf(stderr, "test jam: ok\r\n");
  return 0;
}
