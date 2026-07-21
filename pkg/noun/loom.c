/// @file

#include "loom.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "options.h"

#define U3M_LOOM_PAGE_BITS 16
#define U3M_LOOM_PAGE_BYTES ((size_t)1 << U3M_LOOM_PAGE_BITS)

#ifdef U3_OS_wasm
c3_w* u3m_Loom;
static c3_o _loom_wasm_fresh_o = c3y;
#endif

size_t
u3m_loom_min_bytes(void)
{
  return U3M_LOOM_PAGE_BYTES;
}

c3_d
u3m_loom_max_bytes(void)
{
  return (c3_d)1 << (U3_OS_LoomBits + 4);
}

static c3_o
_loom_size_ok(size_t len_i)
{
  return (  !len_i
         || (len_i & (len_i - 1))
         || (len_i < u3m_loom_min_bytes())
         || ((c3_d)len_i > u3m_loom_max_bytes()) )
       ? c3n
       : c3y;
}

#ifdef U3_OS_wasm
static c3_o
_loom_init_wasm(size_t len_i)
{
  uintptr_t mem_i = ((uintptr_t)__builtin_wasm_memory_size(0))
                  << U3M_LOOM_PAGE_BITS;
  uintptr_t lom_i = mem_i - len_i;

  if ( (mem_i < len_i) || (lom_i & (U3M_LOOM_PAGE_BYTES - 1)) ) {
    u3l_log("boot: wasm loom reservation %zuMB failed in %zuMB memory",
            len_i >> 20, (size_t)(mem_i >> 20));
    return c3n;
  }

  if ( c3n == _loom_wasm_fresh_o ) {
    memset((void*)lom_i, 0, len_i);
  }
  _loom_wasm_fresh_o = c3n;
  u3m_Loom = (c3_w*)lom_i;
  u3C.wor_i = len_i >> 2;
  u3l_log("loom: mapped %zuMB", len_i >> 20);
  return c3y;
}
#else
static c3_o
_loom_init_native(size_t len_i)
{
  void* map_v = mmap((void *)u3_Loom,
                     len_i,
                     (PROT_READ | PROT_WRITE),
                     (MAP_ANON | MAP_FIXED | MAP_PRIVATE),
                     -1, 0);

  if ( MAP_FAILED == map_v ) {
    map_v = mmap((void *)0,
                 len_i,
                 (PROT_READ | PROT_WRITE),
                 (MAP_ANON | MAP_PRIVATE),
                 -1, 0);

    u3l_log("boot: mapping %zuMB failed", len_i >> 20);
    u3l_log("see https://docs.urbit.org/user-manual/running/cloud-hosting"
            " for adding swap space");
    if ( MAP_FAILED != map_v ) {
      u3l_log("if porting to a new platform, try U3_OS_LoomBase %p",
              map_v);
      munmap(map_v, len_i);
    }
    return c3n;
  }

  u3C.wor_i = len_i >> 2;
  u3l_log("loom: mapped %zuMB", len_i >> 20);
  return c3y;
}
#endif

c3_o
u3m_loom_init(size_t len_i)
{
  if ( c3n == _loom_size_ok(len_i) ) {
    u3l_log("loom: bad size: %zu", len_i);
    return c3n;
  }

#ifdef U3_OS_wasm
  return _loom_init_wasm(len_i);
#else
  return _loom_init_native(len_i);
#endif
}
