/// @file

#ifndef U3_LOOM_H
#define U3_LOOM_H

#include "c3/c3.h"

#ifdef U3_OS_wasm
  extern c3_w* u3m_Loom;
# define u3_Loom u3m_Loom
#else
# define u3_Loom ((c3_w *)(void *)U3_OS_LoomBase)
#endif

  /* u3m_loom_min_bytes(): minimum legal loom byte length.
  */
    size_t
    u3m_loom_min_bytes(void);

  /* u3m_loom_max_bytes(): maximum representable loom byte length.
  */
    c3_d
    u3m_loom_max_bytes(void);

  /* u3m_loom_init(): claim and zero the loom, setting u3C.wor_i on success.
  */
    c3_o
    u3m_loom_init(size_t len_i);

#endif /* ifndef U3_LOOM_H */
