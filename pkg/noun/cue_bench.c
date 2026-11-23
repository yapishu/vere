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

int
main(int argc, char** argv)
{
  if ( argc == 1 ) return 0;
  if ( argc < 4 || (strcmp(argv[1], "nock") && strcmp(argv[1], "xeno")) ) {
    fprintf(stderr, "usage: cue-bench nock|xeno IVORY.jam INPUT.jam...\n");
    return 1;
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
  for ( int i = 3; i < argc; ++i ) {
    bytes = _read_file(argv[i], &len);
    u3_noun product;
    c3_d start, elapsed;
    if ( !strcmp(argv[1], "nock") ) {
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
  u3s_cue_xeno_done(decoder);
  u3z(gate);
  return 0;
}
