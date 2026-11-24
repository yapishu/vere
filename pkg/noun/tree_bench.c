/// @file Measure the loom heap high-water mark for one tree operation.
#include "noun.h"

int
main(int argc, char** argv)
{
  if ( argc == 1 ) return 0;
  const char* cases[] = {"set", "map", "duplicates", "keys", "equal", "records"};
  enum { SET, MAP, DUPLICATES, KEYS, EQUAL, RECORDS, COUNT };
  size_t kind = 0;
  while ( kind < COUNT && strcmp(argv[1], cases[kind]) ) ++kind;
  if ( argc != 2 || kind == COUNT ) {
    fprintf(stderr, "usage: tree-bench set|map|duplicates|keys|equal|records\n");
    return 1;
  }
#ifndef U3_CPU_DEBUG
  fprintf(stderr, "tree-bench requires -Dcpu-dbg=true for heap counts\n");
  return 1;
#else
  u3m_boot_lite(1 << 27);
  u3_noun a = u3_nul, b = u3_nul;
  c3_w count = (kind >= EQUAL) ? 120000 : 20000;
  for ( c3_w i = count; i; --i ) {
    switch ( kind ) {
      case SET: a = u3nc(i, a); break;
      case MAP: a = u3nc(u3nc(i, i + 1), a); break;
      case DUPLICATES: a = u3nc(u3nc(i % 257, i), a); break;
      case KEYS: {
        u3_noun put = u3qdb_put(a, i, i + 1);
        u3z(a);
        a = put;
      } break;
      case EQUAL: a = u3nc(i, a); b = u3nc(i, b); break;
      case RECORDS:
        a = u3nc(u3nc(i, i + 1), a);
        b = u3nc(u3nc(i, i + 1), b);
        break;
    }
  }

  // Keep both equal inputs on the home road. Comparing one pair cannot
  // unify its children as a side effect of an earlier measurement.
  u3m_hate(0);
  c3_w start = u3a_heap(u3R) - u3R->all.fre_w;
  u3R->all.max_w = start;
  u3_noun result;
  switch ( kind ) {
    case SET: result = u3qdi_gas(0, a); break;
    case MAP: case DUPLICATES: result = u3qdb_gas(0, a); break;
    case KEYS: result = u3qdb_key(a); break;
    default: result = u3r_sing(a, b); u3_assert(result == c3y); break;
  }
  size_t peak = (size_t)(u3R->all.max_w - start) * sizeof(c3_w);
  printf("{\"case\":\"%s\",\"heap_peak_bytes\":%zu,\"product_mug\":%u}\n",
         cases[kind], peak, (unsigned)u3r_mug(result));
  u3z(result);
  u3z(u3m_love(0));
  u3z(a); u3z(b);
  return 0;
#endif
}
