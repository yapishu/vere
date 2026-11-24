typedef struct {
  _cue_page* pages;
  _cue_directory* directory;
  u3_noun*   nouns;
  size_t     page_count, page_capacity;
  size_t     noun_count, noun_capacity;
  c3_t       external;
  _cue_page  local_pages[2];
  u3_noun    local_nouns[32];
} _cue_refs;

static void
_cue_refs_init(_cue_refs* refs)
{
  refs->external = 0;
  refs->directory = 0;
  refs->pages = refs->local_pages;
  refs->nouns = refs->local_nouns;
  refs->page_count = refs->noun_count = 0;
  refs->page_capacity = sizeof(refs->local_pages) / sizeof(_cue_page);
  refs->noun_capacity = sizeof(refs->local_nouns) / sizeof(u3_noun);
}

static void
_cue_refs_init_external(_cue_refs* refs, c3_d hint)
{
  _cue_refs_init(refs);
  refs->external = 1;
  if ( hint > refs->noun_capacity && hint <= SIZE_MAX / sizeof(u3_noun) ) {
    refs->nouns = c3_malloc(hint * sizeof(u3_noun));
    refs->noun_capacity = hint;
  }
}

static inline void
_cue_refs_dispose(_cue_refs* refs, void* ptr)
{
  if ( refs->external ) c3_free(ptr);
  else u3a_free(ptr);
}

// A Nock call keeps overflow storage on its road so a bail releases it.
// A pier's reusable cue handle owns its buffers outside the loom.
static void*
_cue_refs_grow(_cue_refs* refs, void* old, void* local, size_t* cap, size_t width)
{
  if ( *cap > ((SIZE_MAX - 256) / width) / 2 ) u3m_bail(c3__meme);
  size_t bytes = 2 * *cap * width;
  if ( !refs->external && bytes / sizeof(c3_w) > (c3_w)-1 - 64 ) u3m_bail(c3__meme);
  void* next = refs->external ? c3_malloc(bytes) : u3a_malloc(bytes);
  memcpy(next, old, bytes / 2);
  if ( old != local ) _cue_refs_dispose(refs, old);
  *cap *= 2;
  return next;
}

static void
_cue_refs_add_page(_cue_refs* refs, c3_d number)
{
  if ( refs->page_count == refs->page_capacity ) {
    refs->pages = _cue_refs_grow(refs, refs->pages, refs->local_pages,
                                 &refs->page_capacity, sizeof(_cue_page));
  }
  _cue_page_init(refs->pages, refs->page_count++, number, refs->noun_count,
                 &refs->directory, refs->external);
}

static inline __attribute__((always_inline)) c3_d
_cue_refs_open(_cue_refs* refs, c3_d bit)
{
  c3_d number = bit >> 9;
  if ( !refs->page_count || refs->pages[refs->page_count - 1].number != number )
    _cue_refs_add_page(refs, number);
  if ( refs->noun_count == refs->noun_capacity ) {
    refs->nouns = _cue_refs_grow(refs, refs->nouns, refs->local_nouns,
                                 &refs->noun_capacity, sizeof(u3_noun));
  }
  _cue_page* page = &refs->pages[refs->page_count - 1];
  size_t slot = refs->noun_count++;
  _cue_page_mark(page, bit, slot);
  // An open cell has a position, but cannot be the target of a backreference.
  refs->nouns[slot] = u3_none;
  return slot;
}

static inline void
_cue_refs_finish(_cue_refs* refs, c3_d slot, u3_noun value)
{
  // Every completed value is owned by the result or a pending decoder frame.
  refs->nouns[slot] = value;
}

static inline u3_weak
_cue_refs_get(_cue_refs* refs, c3_d bit)
{
  size_t slot = _cue_page_find(refs->pages, refs->page_count, refs->directory, bit);
  if ( slot == SIZE_MAX ) return u3_none;
  u3_weak value = refs->nouns[slot];
  return value == u3_none ? u3_none : u3k(value);
}

static void
_cue_refs_free(_cue_refs* refs)
{
  _cue_directory_free(refs->directory, refs->external);
  if ( refs->pages != refs->local_pages ) _cue_refs_dispose(refs, refs->pages);
  if ( refs->nouns != refs->local_nouns ) _cue_refs_dispose(refs, refs->nouns);
}
