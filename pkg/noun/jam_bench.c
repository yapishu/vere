// Time either production encoder on independently decoded, canonical jams.
#include "noun.h"
#include <time.h>

static c3_d
_now(void)
{
  struct timespec ts;
  if ( clock_gettime(CLOCK_MONOTONIC, &ts) ) abort();
  return (c3_d)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

int
main(int argc, char** argv)
{
  if ( argc == 1 ) return 0;
  if ( argc < 3 || (strcmp(argv[1], "xeno") && strcmp(argv[1], "fib")) ) {
    fprintf(stderr, "usage: jam-bench xeno|fib CANONICAL.jam ...\n");
    return 1;
  }
  u3m_boot_lite((size_t)1 << 31);
  u3_cue_xeno* decoder = u3s_cue_xeno_init();
  for ( int i = 2; i < argc; ++i ) {
    FILE* file = fopen(argv[i], "rb");
    if ( !file || fseek(file, 0, SEEK_END) ) { perror(argv[i]); return 1; }
    long size = ftell(file);
    if ( size <= 0 || fseek(file, 0, SEEK_SET) ) return 1;
    c3_y* input = c3_malloc(size);
    if ( fread(input, 1, size, file) != (size_t)size ) return 1;
    fclose(file);
    u3_weak root = u3s_cue_xeno_with(decoder, size, input);
    if ( root == u3_none ) return 1;
    c3_y* output;
    c3_d len, bits, elapsed;
    u3i_slab slab;
    c3_t fib = !strcmp(argv[1], "fib");
    c3_d start = _now();
    if ( fib ) {
      bits = u3s_jam_fib(&slab, root);
      len = (bits + 7) >> 3;
      output = slab.buf_y;
    }
    else bits = u3s_jam_xeno(root, &len, &output);
    elapsed = _now() - start;
    c3_d want_bits = ((c3_d)size-1)*8 + c3_bits_word(input[size-1]);
    if ( len != (c3_d)size || bits != want_bits || memcmp(output, input, size) ) {
      fprintf(stderr, "%s: different canonical jam\n", argv[i]);
      return 1;
    }
    printf("%s\t%" PRIu64 "\t%" PRIu64 "\n", argv[i], elapsed, bits);
    if ( fib ) u3i_slab_free(&slab);
    else c3_free(output);
    c3_free(input);
    u3z(root);
  }
  u3s_cue_xeno_done(decoder);
  return 0;
}
