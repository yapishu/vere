// Time the registered Nock gate or the pier decoder on supplied jam files.
#include "noun.h"
#include "urcrypt.h"
#include <time.h>

static c3_y*
_read_file(const char* path, c3_d* size)
{
  FILE* in = fopen(path, "rb");
  if ( !in || fseek(in, 0, SEEK_END) ) { perror(path); exit(1); }
  long end = ftell(in);
  if ( end < 0 || fseek(in, 0, SEEK_SET) ) { perror(path); exit(1); }
  *size = (c3_d)end;
  if ( *size > (c3_w)-1 ) { fprintf(stderr, "atom too large: %s\n", path); exit(1); }
  c3_y* bytes = c3_malloc(*size ? *size : 1);
  if ( *size != fread(bytes, 1, *size, in) ) { fprintf(stderr, "short read: %s\n", path); exit(1); }
  fclose(in);
  return bytes;
}

static c3_d
_now(void)
{
  struct timespec now;
  if ( clock_gettime(CLOCK_MONOTONIC, &now) ) { perror("clock_gettime"); exit(1); }
  return (c3_d)now.tv_sec * 1000000000 + now.tv_nsec;
}

static u3_noun
_slam(u3_noun pair)
{
  u3_noun gate = u3k(u3h(pair)), sample = u3k(u3t(pair));
  u3z(pair);
  return u3n_slam_on(gate, sample);
}

/* Read a batch of axes from each input. Units distinguish a missing axis
 * from the atom zero. Include decoding and releasing the full root, or
 * opening the index, inside the interval. Hash only the extracted output. */
static u3_noun
_project(u3_cue_xeno* decoder,
         u3_cue_view* view,
         const c3_y* bytes, c3_d len, const c3_w* axes, size_t count)
{
  u3_noun root = u3_none, list = u3_nul;
  if ( view ) {
    if ( c3n == u3s_cue_view_open(view, len, bytes) ) return u3_none;
  }
  else {
    root = u3s_cue_xeno_with(decoder, len, bytes);
    if ( root == u3_none ) return u3_none;
  }
  while ( count ) {
    c3_w axis = axes[--count];
    u3_weak value;
    if ( view ) value = u3s_cue_view_at(view, axis);
    else {
      value = u3r_at(axis, root);
      if ( value != u3_none ) value = u3k(value);
    }
    list = u3nc(value == u3_none ? 0 : u3nc(0, value), list);
  }
  if ( root != u3_none ) u3z(root);
  return list;
}

int
main(int argc, char** argv)
{
  if ( argc == 1 ) return 0;
  c3_t project = !strcmp(argv[1], "project");
  c3_t indexed = !strcmp(argv[1], "view");
  project |= indexed;
  if ( argc < 4 || (!project && strcmp(argv[1], "nock") && strcmp(argv[1], "xeno")) ) {
    fprintf(stderr, "usage: cue-bench nock|xeno|project|view IVORY.jam [AXIS,...] INPUT.jam...\n");
    return 1;
  }
  size_t count = 0;
  c3_w axes[32];
  if ( project ) {
    if ( argc < 5 ) return 1;
    const char* arg = argv[3];
    do {
      char* end;
      unsigned long axis = strtoul(arg, &end, 10);
      if ( arg == end || !axis || axis > 0x7fffffff || count == 32 ) return 1;
      axes[count++] = axis;
      if ( !*end ) break;
      if ( *end != ',' ) return 1;
      arg = end + 1;
    } while ( 1 );
  }

  c3_d len;
  c3_y* bytes = _read_file(argv[2], &len);
  u3C.wag_h |= u3o_hashless;
  u3m_boot_lite((size_t)1 << 31);
  u3_weak pill = u3s_cue_xeno(len, bytes);
  if ( pill == u3_none || c3n == u3v_boot_lite(pill) ) return 1;
  c3_free(bytes);
  u3_noun gate = u3v_wish("cue");
  u3_cue_xeno* decoder = u3s_cue_xeno_init();
  u3_cue_view* view = indexed ? u3s_cue_view_new() : 0;
  for ( int i = project ? 4 : 3; i < argc; ++i ) {
    bytes = _read_file(argv[i], &len);
    u3_noun product;
    c3_d start, elapsed;
    if ( project ) {
      start = _now();
      product = _project(decoder, view, bytes, len, axes, count);
      elapsed = _now() - start;
      if ( product == u3_none ) { fprintf(stderr, "invalid jam: %s\n", argv[i]); return 1; }
    }
    else if ( !strcmp(argv[1], "nock") ) {
      u3_noun pair = u3nc(u3k(gate), u3i_bytes(len, bytes));
      start = _now();
      u3_noun result = u3m_soft(0, _slam, pair);
      elapsed = _now() - start;
      if ( u3h(result) != 0 ) { fprintf(stderr, "cue bailed: %s\n", argv[i]); return 1; }
      product = u3k(u3t(result));
      u3z(result);
    }
    else {
      start = _now();
      product = u3s_cue_xeno_with(decoder, len, bytes);
      elapsed = _now() - start;
      if ( product == u3_none ) { fprintf(stderr, "cue failed: %s\n", argv[i]); return 1; }
    }
    c3_free(bytes);
    c3_d size;
    c3_y* jam;
    c3_y digest[32];
    u3s_jam_xeno(product, &size, &jam);
    urcrypt_shay(jam, size, digest);
    printf("%s\t%" PRIu64 "\t%" PRIu64 "\t", argv[i], elapsed, size);
    for ( size_t j = 0; j < sizeof(digest); ++j ) printf("%02x", digest[j]);
    putchar('\n');
    fflush(stdout);
    c3_free(jam);
    u3z(product);
  }
  if ( view ) u3s_cue_view_free(view);
  u3s_cue_xeno_done(decoder);
  u3z(gate);
  return 0;
}
