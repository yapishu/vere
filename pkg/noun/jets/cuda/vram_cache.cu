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
  if ( budget_bytes > 0 ) g_budget = budget_bytes;
  g_cap = 16;
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
