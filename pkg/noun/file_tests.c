/// @file

#include "noun.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static c3_i
_test_u3m_file(void)
{
  c3_c pax_c[] = "/tmp/urbit-file-test-XXXXXX";
  c3_i fid_i = mkstemp(pax_c);
  if ( fid_i < 0 ) {
    return 1;
  }

  if ( 10 != write(fid_i, "ames-quic!", 10) ) {
    close(fid_i);
    unlink(pax_c);
    return 1;
  }
  close(fid_i);

  u3_atom dat = u3m_file(pax_c);
  c3_w    len_w = u3r_met(3, dat);
  c3_y    out_y[16] = {0};

  u3r_bytes(0, len_w, out_y, dat);
  u3z(dat);
  unlink(pax_c);

  return ( (10 == len_w) && (0 == memcmp(out_y, "ames-quic!", 10)) )
       ? 0
       : 1;
}

int
main(void)
{
  u3m_init(1 << 20);
  u3m_pave(c3y);

  return _test_u3m_file();
}
