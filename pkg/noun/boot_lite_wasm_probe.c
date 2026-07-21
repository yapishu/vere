/// @file
///
/// Linked boot-lite probe for the browser/WASM noun runtime boundary.

#include "noun.h"

#include <stdlib.h>
#include <string.h>

static c3_i
_test_basic_nouns(void)
{
  u3_noun msg = u3i_string("ames-quic");
  u3_noun cel = u3nc(u3k(msg), 0x42);
  u3_noun ref = u3nc(u3i_string("ames-quic"), 0x42);
  c3_i ret_i = 0;

  if ( c3y != u3r_sing(cel, ref) ) {
    goto done;
  }

  c3_d  len_d = 0;
  c3_y* byt_y = 0;
  if ( 0 != u3s_jam_xeno(cel, &len_d, &byt_y) ) {
    u3_noun out = u3s_cue_bytes(len_d, byt_y);
    if ( c3y == u3r_sing(cel, out) ) {
      ret_i = 1;
    }
    u3z(out);
    free(byt_y);
  }

done:
  u3z(msg);
  u3z(cel);
  u3z(ref);
  return ret_i;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  if ( 0 == _test_basic_nouns() ) {
    return 1;
  }

  u3m_stop();
  return 0;
}
