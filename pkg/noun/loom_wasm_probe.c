/// @file
///
/// Link probe for claiming a small Vere loom under wasm32-wasi.

#include "loom.h"

#include "options.h"

int
main(void)
{
  if ( ((c3_d)1 << 31) != u3m_loom_max_bytes() ) {
    return 1;
  }

  if ( c3y != u3m_loom_init(u3m_loom_min_bytes()) ) {
    return 1;
  }
  if ( NULL == u3_Loom ) {
    return 1;
  }
  if ( (u3m_loom_min_bytes() >> 2) != u3C.wor_i ) {
    return 1;
  }

  u3_Loom[0] = 0xfeedface;
  if ( 0xfeedface != u3_Loom[0] ) {
    return 1;
  }

  if ( c3n != u3m_loom_init(u3m_loom_min_bytes() + 1) ) {
    return 1;
  }
  return 0;
}
