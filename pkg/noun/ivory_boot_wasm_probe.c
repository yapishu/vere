/// @file
///
/// Linked Ivory-pill boot probe for the browser/WASM noun runtime boundary.

#include "ivory.h"
#include "noun.h"
#include "ur/ur.h"

#include <stdio.h>

int
main(void)
{
  c3_d          len_d = u3_Ivory_pill_len;
  c3_y*         byt_y = u3_Ivory_pill;
  u3_cue_xeno*  sil_u;
  u3_weak       pil;
  c3_l          lit_l;

  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);

  sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, len_d, byt_y)) ) {
    fprintf(stderr, "ivory: unable to cue pill\r\n");
    u3s_cue_xeno_done(sil_u);
    return 1;
  }
  u3s_cue_xeno_done(sil_u);

  if ( c3n == u3v_boot_lite(pil) ) {
    fprintf(stderr, "ivory: boot failed\r\n");
    return 1;
  }

  if ( c3n == u3v_lily(c3__ud, u3dc("scot", c3__ud, 42), &lit_l) ) {
    fprintf(stderr, "ivory: lily parse failed\r\n");
    return 1;
  }
  if ( 42 != lit_l ) {
    fprintf(stderr, "ivory: lily parse mismatch: %u\r\n", lit_l);
    return 1;
  }

  u3l_log("ivory: booted core %x", u3r_mug(u3A->roc));
  u3m_stop();
  return 0;
}
