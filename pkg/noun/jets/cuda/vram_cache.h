/*
 * vram_cache.h — content-hashed VRAM residency for weight atoms.
 *
 * Design contract:
 *   - Caller passes host bytes + 32-bit content hash (from u3r_mug, or any
 *     stable hash of the same bytes).
 *   - Cache returns a CUdeviceptr (as uintptr_t to keep this header free of
 *     cuda.h).  On miss, uploads; on hit, returns the existing ptr.
 *   - Collision safety: hash plus a byte-length check plus a sentinel byte
 *     spot-check.  Callers that need stronger guarantees can pass a 64-bit
 *     hash via vram_cache_get_or_upload64.
 *   - LRU eviction when total VRAM budget is exceeded.
 *   - Format-agnostic: just a byte blob keyed by hash.  fp32, fp16, mlx2
 *     packed uint32, q8 int8 — cache doesn't care.
 */

#ifndef URBIT_CUDA_VRAM_CACHE_H
#define URBIT_CUDA_VRAM_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum {
  VRAM_CACHE_OK = 0,
  VRAM_CACHE_NO_CUDA,
  VRAM_CACHE_ALLOC_FAIL,
  VRAM_CACHE_INVALID_ARG,
  VRAM_CACHE_MISS         /* probe returned: not cached */
} vram_cache_status;

/*
 * Initialize the global cache with a VRAM budget in bytes.  Idempotent.
 * Budget of 0 means "use default" (currently 8 GiB).
 */
vram_cache_status vram_cache_init(size_t budget_bytes);

/*
 * Tear down cache, freeing all cached allocations.  Optional.
 */
void vram_cache_shutdown(void);

/*
 * Look up or upload bytes[0..n_bytes-1] identified by hash.  On success,
 * *out_dptr is set to the device pointer (valid until eviction or
 * shutdown).  On miss, performs cudaMalloc + cudaMemcpy; on hit, returns
 * the cached pointer without re-uploading.
 *
 * The 32-bit hash variant is fine for mug-based identification within a
 * single ship.  For long-running caches across many weights, use the 64-bit
 * variant to make collisions statistically impossible.
 */
vram_cache_status
vram_cache_get_or_upload(const void* bytes,
                         size_t      n_bytes,
                         uint32_t    hash,
                         uintptr_t*  out_dptr);

vram_cache_status
vram_cache_get_or_upload64(const void* bytes,
                           size_t      n_bytes,
                           uint64_t    hash,
                           uintptr_t*  out_dptr);

/*
 * Lookup-only variant.  No allocation, no upload — just checks whether
 * a matching entry is already resident.  `sentinel` must point to the
 * first min(16, n_bytes) bytes of the payload; used to guard against
 * 32-bit hash collisions without requiring the full payload in hand.
 *
 * Returns VRAM_CACHE_OK with *out_dptr set on hit.
 * Returns VRAM_CACHE_MISS on miss (no state change).
 */
vram_cache_status
vram_cache_probe(uint32_t       hash,
                 size_t         n_bytes,
                 const uint8_t* sentinel,
                 uintptr_t*     out_dptr);

/*
 * Allocate an uninitialized VRAM buffer keyed by an opaque 64-bit ID.
 * Unlike get_or_upload, this doesn't transfer host bytes — the caller
 * writes into the buffer via kernel or cudaMemcpy after this returns.
 * Intended for per-layer KV cache entries whose content is produced
 * on-device and is a deterministic function of the (seq_hash, layer,
 * kind) key encoded into the ID.
 *
 * Same LRU as weights; separate keyspace convention (e.g. top bit set)
 * keeps weight and KV entries from colliding.
 *
 * On cache hit with matching n_bytes, returns the cached dptr without
 * re-allocating.  On hit with *different* n_bytes, evicts the old entry
 * and allocates fresh (the caller is trusted to regenerate content).
 */
vram_cache_status
vram_cache_alloc64(uint64_t    key,
                   size_t      n_bytes,
                   uintptr_t*  out_dptr);

/*
 * Probe by 64-bit key only, no sentinel check.  Returns OK with dptr +
 * n_bytes set on hit, MISS otherwise.  For caches whose content is
 * written on-device (see vram_cache_alloc64).
 */
vram_cache_status
vram_cache_probe64_keyonly(uint64_t    key,
                           uintptr_t*  out_dptr,
                           size_t*     out_n_bytes);

/*
 * Drop all entries whose key has any of `mask_bits` set.  Used to clear
 * a generation's KV entries when its stream finishes.  Returns the
 * number of entries evicted.
 */
size_t
vram_cache_drop_by_mask(uint64_t mask_bits);

/*
 * Diagnostics: total bytes resident, entry count, hit/miss counters.
 */
typedef struct {
  size_t   resident_bytes;
  size_t   budget_bytes;
  size_t   n_entries;
  uint64_t n_hits;
  uint64_t n_misses;
  uint64_t n_evictions;
} vram_cache_stats;

void vram_cache_get_stats(vram_cache_stats* out);

#ifdef __cplusplus
}
#endif

#endif  /* URBIT_CUDA_VRAM_CACHE_H */
