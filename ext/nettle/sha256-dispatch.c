#include "sha2-internal.h"
#include <stdatomic.h>

#if defined(__x86_64__)
#include <cpuid.h>
#define SHA_COMPRESS _nettle_sha256_compress_n_sha_ni
#elif defined(__aarch64__)
#define SHA_COMPRESS _nettle_sha256_compress_n_arm64
#if defined(__linux__) || defined(__FreeBSD__)
#include <sys/auxv.h>
#endif
#endif

typedef __typeof__(_nettle_sha256_compress_n) compress_fn;
extern compress_fn SHA_COMPRESS, _nettle_sha256_compress_n_c;

static int
has_sha256(void)
{
#if defined(__x86_64__)
  unsigned int a, b, c, d;
  return __get_cpuid(1, &a, &b, &c, &d) && (c & bit_SSSE3)
      && __get_cpuid_count(7, 0, &a, &b, &c, &d) && (b & bit_SHA);
#elif defined(__ARM_FEATURE_SHA2)
  return 1;
#elif defined(__linux__) || defined(__FreeBSD__)
  const unsigned long required = HWCAP_ASIMD | HWCAP_SHA2;
#if defined(__linux__)
  unsigned long caps = getauxval(AT_HWCAP);
#else
  unsigned long caps = 0;
  if ( elf_aux_info(AT_HWCAP, &caps, sizeof(caps)) ) return 0;
#endif
  return (caps & required) == required;
#else
  return 0;
#endif
}

const uint8_t *
_nettle_sha256_compress_n(uint32_t *state, const uint32_t *k,
                          size_t blocks, const uint8_t *input)
{
  static _Atomic(compress_fn *) impl;
  compress_fn *f = atomic_load_explicit(&impl, memory_order_relaxed);
  if ( !f ) {
    f = has_sha256() ? SHA_COMPRESS : _nettle_sha256_compress_n_c;
    atomic_store_explicit(&impl, f, memory_order_relaxed);
  }
  return f(state, k, blocks, input);
}
