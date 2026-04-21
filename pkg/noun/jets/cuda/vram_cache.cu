/*
 * vram_cache.cu — content-hashed LRU cache of weight buffers in VRAM.
 *
 * Plain-C data structures (no STL) so the static archive can be linked into
 * a musl-static urbit binary without dragging libstdc++.
 *
 * Open-addressed hashmap (linear probe) + doubly-linked list for LRU.
 * Single-threaded: jets run serially inside a vere work process, so no lock.
 *
 * Expected working set: O(100) entries for a 28-layer transformer.
 */

#include "vram_cache.h"

#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Entry {
  uint64_t       key;          /* nonzero = occupied; 0 = empty/tombstone */
  uint64_t       full_hash;
  size_t         n_bytes;
  uintptr_t      dptr;
  uint8_t        sentinel[16];
  uint8_t        untagged;     /* 1 = alloc64/probe64 entry (no sentinel) */
  struct Entry*  prev;
  struct Entry*  next;
} Entry;

static int      g_init = 0;
static size_t   g_budget = (size_t)8 * 1024 * 1024 * 1024;  /* 8 GiB */
static size_t   g_resident = 0;
static uint64_t g_hits = 0, g_misses = 0, g_evictions = 0;

/* Hash table. Capacity is power of two. Grows 2x on load > 0.5. */
static Entry** g_table = NULL;
static size_t  g_cap = 0;
static size_t  g_count = 0;
static Entry*  g_lru_head = NULL;  /* MRU */
static Entry*  g_lru_tail = NULL;  /* LRU */

static size_t
_probe(uint64_t key)
{
  /* splitmix-ish finalizer then mask */
  uint64_t h = key;
  h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;
  return (size_t)(h & (g_cap - 1));
}

static Entry**
_find_slot(uint64_t key)
{
  /* Returns the slot holding key (occupied) or the first empty slot where
   * it would be inserted. Never returns NULL because load stays ≤ 0.5. */
  size_t i = _probe(key);
  for ( ;; ) {
    Entry* e = g_table[i];
    if ( e == NULL || e->key == key ) return &g_table[i];
    i = (i + 1) & (g_cap - 1);
  }
}

static void
_grow(void)
{
  size_t new_cap = g_cap == 0 ? 16 : g_cap * 2;
  Entry** new_table = (Entry**)calloc(new_cap, sizeof(Entry*));
  size_t old_cap = g_cap;
  Entry** old_table = g_table;
  g_cap = new_cap;
  g_table = new_table;
  for ( size_t i = 0; i < old_cap; i++ ) {
    Entry* e = old_table[i];
    if ( e != NULL ) {
      Entry** slot = _find_slot(e->key);
      *slot = e;
    }
  }
  free(old_table);
}

static void
_lru_unlink(Entry* e)
{
  if ( e->prev ) e->prev->next = e->next; else g_lru_head = e->next;
  if ( e->next ) e->next->prev = e->prev; else g_lru_tail = e->prev;
  e->prev = e->next = NULL;
}

static void
_lru_push_front(Entry* e)
{
  e->prev = NULL;
  e->next = g_lru_head;
  if ( g_lru_head ) g_lru_head->prev = e;
  g_lru_head = e;
  if ( !g_lru_tail ) g_lru_tail = e;
}

static void
_lru_touch(Entry* e)
{
  if ( g_lru_head == e ) return;
  _lru_unlink(e);
  _lru_push_front(e);
}

static void
_remove_entry(Entry* e)
{
  /* Linear probe: mark slot empty, then shift subsequent entries that
   * hashed earlier but were displaced by the probe (simple rehash). */
  size_t idx = 0;
  for ( size_t i = 0; i < g_cap; i++ ) {
    if ( g_table[i] == e ) { idx = i; break; }
  }
  g_table[idx] = NULL;
  /* Re-insert entries following idx until an empty slot, to preserve probing. */
  size_t i = (idx + 1) & (g_cap - 1);
  while ( g_table[i] != NULL ) {
    Entry* moved = g_table[i];
    g_table[i] = NULL;
    g_count--;
    Entry** slot = _find_slot(moved->key);
    *slot = moved;
    g_count++;
    i = (i + 1) & (g_cap - 1);
  }
  g_count--;
  _lru_unlink(e);
  cudaFree((void*)(e->dptr));
  g_resident -= e->n_bytes;
  free(e);
}

static int
_evict_lru_one(void)
{
  if ( !g_lru_tail ) return 0;
  Entry* victim = g_lru_tail;
  _remove_entry(victim);
  g_evictions++;
  return 1;
}

static void
_ensure_room(size_t need)
{
  while ( g_resident + need > g_budget ) {
    if ( !_evict_lru_one() ) break;
  }
}

extern "C" vram_cache_status
vram_cache_init(size_t budget_bytes)
{
  if ( g_init ) return VRAM_CACHE_OK;
  int dev = 0;
  if ( cudaGetDeviceCount(&dev) != cudaSuccess || dev == 0 )
    return VRAM_CACHE_NO_CUDA;
  if ( cudaSetDevice(0) != cudaSuccess )
    return VRAM_CACHE_NO_CUDA;
  if ( budget_bytes > 0 ) {
    g_budget = budget_bytes;
  } else {
    /* Size the cache to the card's actually-free VRAM, minus a headroom
     * margin for CUDA context, scratch allocs, and other apps.  Keeps
     * the cache dynamic without requiring hardcoded 8-GiB assumptions
     * that over-promise on 4-GB cards and under-use 24-GB cards.
     * Stateless + deterministic: budget affects only when eviction
     * happens, never what is computed. */
    size_t free_b = 0, total_b = 0;
    if ( cudaMemGetInfo(&free_b, &total_b) == cudaSuccess && free_b > 0 ) {
      /* Reserve 1 GiB or 15% (whichever is bigger) for scratch + headroom. */
      size_t pct_margin  = total_b / 7;         /* ~14% of total */
      size_t fixed_margin = (size_t)1 << 30;    /* 1 GiB */
      size_t margin = pct_margin > fixed_margin ? pct_margin : fixed_margin;
      g_budget = free_b > margin ? free_b - margin : free_b / 2;
    } else {
      g_budget = (size_t)4 << 30;  /* safe-ish default */
    }
  }
  /* Pre-size: Qwen3 has 588 weight entries + up to 28×2=56 persistent
   * KV entries per active generation + misc.  Starting at 4096 avoids
   * a dozen power-of-two grows during warmup and keeps load factor low. */
  g_cap = 4096;
  g_table = (Entry**)calloc(g_cap, sizeof(Entry*));
  g_init = 1;
  return VRAM_CACHE_OK;
}

extern "C" void
vram_cache_shutdown(void)
{
  if ( !g_init ) return;
  while ( _evict_lru_one() ) { /* drain */ }
  free(g_table);
  g_table = NULL;
  g_cap = 0;
  g_count = 0;
  g_lru_head = g_lru_tail = NULL;
  g_resident = 0;
  g_init = 0;
}

static vram_cache_status
_get_or_upload(const void* bytes, size_t n_bytes,
               uint64_t full_hash, uint64_t key,
               uintptr_t* out_dptr)
{
  if ( !out_dptr || !bytes || n_bytes == 0 || key == 0 )
    return VRAM_CACHE_INVALID_ARG;
  if ( !g_init ) {
    vram_cache_status s = vram_cache_init(0);
    if ( s != VRAM_CACHE_OK ) return s;
  }

  Entry** slot = _find_slot(key);
  Entry* e = *slot;
  if ( e != NULL ) {
    size_t spot = n_bytes < 16 ? n_bytes : 16;
    if ( e->n_bytes == n_bytes && e->full_hash == full_hash &&
         memcmp(e->sentinel, bytes, spot) == 0 ) {
      g_hits++;
      _lru_touch(e);
      *out_dptr = e->dptr;
      return VRAM_CACHE_OK;
    }
    /* collision → evict and reinsert */
    _remove_entry(e);
    slot = _find_slot(key);  /* table may have shifted */
  }

  /* miss: allocate, upload, insert */
  _ensure_room(n_bytes);
  void* dptr = NULL;
  if ( cudaMalloc(&dptr, n_bytes) != cudaSuccess )
    return VRAM_CACHE_ALLOC_FAIL;
  if ( cudaMemcpy(dptr, bytes, n_bytes, cudaMemcpyHostToDevice) != cudaSuccess ) {
    cudaFree(dptr);
    return VRAM_CACHE_ALLOC_FAIL;
  }
  Entry* ne = (Entry*)calloc(1, sizeof(Entry));
  ne->key = key;
  ne->full_hash = full_hash;
  ne->n_bytes = n_bytes;
  ne->dptr = (uintptr_t)dptr;
  size_t spot = n_bytes < 16 ? n_bytes : 16;
  memcpy(ne->sentinel, bytes, spot);
  _lru_push_front(ne);
  *slot = ne;
  g_count++;
  g_resident += n_bytes;
  g_misses++;
  /* Keep load factor ≤ 0.5 for clean linear probing. */
  if ( g_count * 2 >= g_cap ) _grow();
  *out_dptr = ne->dptr;
  return VRAM_CACHE_OK;
}

extern "C" vram_cache_status
vram_cache_probe(uint32_t hash, size_t n_bytes, const uint8_t* sentinel,
                 uintptr_t* out_dptr)
{
  if ( !out_dptr || !sentinel || n_bytes == 0 ) return VRAM_CACHE_INVALID_ARG;
  if ( !g_init ) {
    vram_cache_status s = vram_cache_init(0);
    if ( s != VRAM_CACHE_OK ) return s;
  }
  uint64_t key = ((uint64_t)hash ? (uint64_t)hash : 1)
               ^ ((uint64_t)n_bytes * 0x9e3779b97f4a7c15ULL);
  if ( key == 0 ) key = 1;
  Entry** slot = _find_slot(key);
  Entry* e = *slot;
  if ( e == NULL ) return VRAM_CACHE_MISS;
  size_t spot = n_bytes < 16 ? n_bytes : 16;
  if ( e->n_bytes != n_bytes ||
       e->full_hash != (uint64_t)hash ||
       memcmp(e->sentinel, sentinel, spot) != 0 ) {
    return VRAM_CACHE_MISS;
  }
  g_hits++;
  _lru_touch(e);
  *out_dptr = e->dptr;
  return VRAM_CACHE_OK;
}

extern "C" vram_cache_status
vram_cache_get_or_upload(const void* bytes, size_t n_bytes,
                         uint32_t hash, uintptr_t* out_dptr)
{
  /* 32-bit hash into 64-bit key; mix n_bytes for length-collision guard. */
  uint64_t key = ((uint64_t)hash ? (uint64_t)hash : 1)
               ^ ((uint64_t)n_bytes * 0x9e3779b97f4a7c15ULL);
  if ( key == 0 ) key = 1;
  return _get_or_upload(bytes, n_bytes, (uint64_t)hash, key, out_dptr);
}

extern "C" vram_cache_status
vram_cache_get_or_upload64(const void* bytes, size_t n_bytes,
                           uint64_t hash, uintptr_t* out_dptr)
{
  uint64_t key = (hash ? hash : 1) ^ ((uint64_t)n_bytes * 0x9e3779b97f4a7c15ULL);
  if ( key == 0 ) key = 1;
  return _get_or_upload(bytes, n_bytes, hash, key, out_dptr);
}

extern "C" vram_cache_status
vram_cache_alloc64(uint64_t key, size_t n_bytes, uintptr_t* out_dptr)
{
  if ( !out_dptr || n_bytes == 0 || key == 0 ) return VRAM_CACHE_INVALID_ARG;
  if ( !g_init ) {
    vram_cache_status s = vram_cache_init(0);
    if ( s != VRAM_CACHE_OK ) return s;
  }
  Entry** slot = _find_slot(key);
  Entry* e = *slot;
  if ( e != NULL ) {
    if ( e->n_bytes == n_bytes ) {
      g_hits++;
      _lru_touch(e);
      *out_dptr = e->dptr;
      return VRAM_CACHE_OK;
    }
    /* size changed — evict and re-alloc. */
    _remove_entry(e);
    slot = _find_slot(key);
  }
  _ensure_room(n_bytes);
  void* dptr = NULL;
  if ( cudaMalloc(&dptr, n_bytes) != cudaSuccess )
    return VRAM_CACHE_ALLOC_FAIL;
  Entry* ne = (Entry*)calloc(1, sizeof(Entry));
  ne->key       = key;
  ne->full_hash = key;
  ne->n_bytes   = n_bytes;
  ne->dptr      = (uintptr_t)dptr;
  ne->untagged  = 1;
  _lru_push_front(ne);
  *slot = ne;
  g_count++;
  g_resident += n_bytes;
  g_misses++;
  if ( g_count * 2 >= g_cap ) _grow();
  *out_dptr = ne->dptr;
  return VRAM_CACHE_OK;
}

extern "C" vram_cache_status
vram_cache_probe64_keyonly(uint64_t key, uintptr_t* out_dptr, size_t* out_n_bytes)
{
  if ( !out_dptr || key == 0 ) return VRAM_CACHE_INVALID_ARG;
  if ( !g_init ) return VRAM_CACHE_MISS;
  Entry** slot = _find_slot(key);
  Entry* e = *slot;
  if ( e == NULL ) return VRAM_CACHE_MISS;
  g_hits++;
  _lru_touch(e);
  *out_dptr = e->dptr;
  if ( out_n_bytes ) *out_n_bytes = e->n_bytes;
  return VRAM_CACHE_OK;
}

extern "C" int
vram_cache_drop(uint64_t key)
{
  if ( !g_init || key == 0 ) return 0;
  Entry** slot = _find_slot(key);
  Entry* e = *slot;
  if ( e == NULL ) return 0;
  _remove_entry(e);
  return 1;
}

extern "C" size_t
vram_cache_drop_if(uint64_t mask, uint64_t expect)
{
  if ( !g_init || mask == 0 ) return 0;
  size_t dropped = 0;
  size_t cap = g_cap;
  Entry** victims = (Entry**)calloc(cap, sizeof(Entry*));
  size_t nv = 0;
  for ( size_t i = 0; i < cap; i++ ) {
    Entry* e = g_table[i];
    if ( e && (e->key & mask) == (expect & mask) ) {
      victims[nv++] = e;
    }
  }
  for ( size_t i = 0; i < nv; i++ ) {
    _remove_entry(victims[i]);
    dropped++;
  }
  free(victims);
  return dropped;
}

extern "C" size_t
vram_cache_drop_by_mask(uint64_t mask_bits)
{
  if ( !g_init || mask_bits == 0 ) return 0;
  size_t dropped = 0;
  /* Collect victims first so _remove_entry doesn't trip us mid-scan. */
  size_t cap = g_cap;
  Entry** victims = (Entry**)calloc(cap, sizeof(Entry*));
  size_t nv = 0;
  for ( size_t i = 0; i < cap; i++ ) {
    Entry* e = g_table[i];
    if ( e && (e->key & mask_bits) ) {
      victims[nv++] = e;
    }
  }
  for ( size_t i = 0; i < nv; i++ ) {
    _remove_entry(victims[i]);
    dropped++;
  }
  free(victims);
  return dropped;
}

extern "C" void
vram_cache_get_stats(vram_cache_stats* out)
{
  if ( !out ) return;
  out->resident_bytes = g_resident;
  out->budget_bytes = g_budget;
  out->n_entries = g_count;
  out->n_hits = g_hits;
  out->n_misses = g_misses;
  out->n_evictions = g_evictions;
}
