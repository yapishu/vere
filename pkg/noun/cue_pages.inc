// Leaf pages cover 512 input bits. A word's rank within its page fits in
// 16 bits. The only full-width noun ordinal is the one at the page start.
typedef struct {
  c3_d   number, bits[8];
  size_t base;
  c3_s   before[8];
} _cue_page;

typedef struct {
  c3_d   bits;
  size_t base;
} _cue_group;

typedef struct {
  _cue_group* groups;
  size_t count, capacity;
} _cue_level;

// Pages preceding the first gap occupy their own numbered slots. After the
// gap, each set bit at an interior level names one occupied child. Starts
// arrive in order, so siblings occupy consecutive slots at the next level.
typedef struct {
  c3_d first;    // Page number immediately after the first gap.
  size_t dense;  // Number of pages preceding that gap.
  unsigned height;
  _cue_level levels[10];  // At most 55 page-number bits, six per level.
} _cue_directory;

static void
_cue_group_append(_cue_level* level, c3_d bits, size_t base, c3_t external)
{
  if ( level->count == level->capacity ) {
    if ( level->capacity > (SIZE_MAX / sizeof(_cue_group)) / 2 )
      u3m_bail(c3__meme);
    size_t next = level->capacity ? 2 * level->capacity : 4;
    size_t bytes = next * sizeof(_cue_group);
    if ( !external && bytes / sizeof(c3_w) > (c3_w)-1 - 64 )
      u3m_bail(c3__meme);
    level->groups = external ? c3_realloc(level->groups, bytes)
                  : u3a_realloc(level->groups, level->capacity * sizeof(_cue_group), bytes);
    level->capacity = next;
  }
  level->groups[level->count++] = (_cue_group){bits, base};
}

static __attribute__((noinline)) void
_cue_sparse_add(_cue_directory** handle, c3_d page, c3_d previous, size_t slot,
                c3_t external)
{
  _cue_directory* dir = *handle;
  if ( !dir ) {
    dir = external ? c3_calloc(sizeof(*dir)) : u3a_calloc(1, sizeof(*dir));
    *handle = dir;
  }
  if ( !dir->height ) {
    dir->first = page;
    dir->dense = slot;
    dir->height = 1;
    dir->levels[0].count = 0;
    _cue_group_append(&dir->levels[0], 1, slot, external);
    return;
  }

  c3_d key = page - dir->first;
  c3_d last = previous - dir->first;
  while ( key >> (6 * dir->height) ) {
    _cue_level* top = &dir->levels[dir->height++];
    top->count = 0;
    _cue_group_append(top, 1, 0, external);  // The existing root is child zero.
  }
  // Append the missing groups up to the first prefix shared with the last page.
  for ( unsigned i = 0; ; ++i ) {
    _cue_level* level = &dir->levels[i];
    c3_d mask = UINT64_C(1) << (key & 63);
    key >>= 6;
    last >>= 6;
    if ( key == last ) {
      level->groups[level->count - 1].bits |= mask;
      return;
    }
    size_t parent = level->count;
    _cue_group_append(level, mask, slot, external);
    slot = parent;
  }
}

static inline void
_cue_page_init(_cue_page* pages, size_t slot, c3_d page, size_t first_noun,
               _cue_directory** handle, c3_t external)
{
  pages[slot] = (_cue_page){ .number = page, .base = first_noun };
  if ( page != slot ) {
    // The root of a valid stream always starts on page zero.
    _cue_sparse_add(handle, page, pages[slot - 1].number, slot, external);
  }
}

static inline void
_cue_page_mark(_cue_page* page, c3_d bit, size_t slot)
{
  unsigned word = (bit >> 6) & 7;
  if ( !page->bits[word] ) page->before[word] = slot - page->base;
  page->bits[word] |= UINT64_C(1) << (bit & 63);
}

static size_t
_cue_sparse_find(const _cue_directory* dir, c3_d key)
{
  size_t i = 0;
  for ( unsigned h = dir->height; h; --h ) {
    const _cue_group* group = &dir->levels[h - 1].groups[i];
    c3_d mask = UINT64_C(1) << ((key >> (6 * (h - 1))) & 63);
    if ( !(group->bits & mask) ) return SIZE_MAX;
    i = group->base + __builtin_popcountll(group->bits & (mask - 1));
  }
  return i;
}

static inline size_t
_cue_page_find(const _cue_page* pages, size_t count, const _cue_directory* dir, c3_d bit)
{
  c3_d number = bit >> 9;
  size_t i;
  if ( !dir || !dir->height || number < dir->dense ) {
    if ( number >= count ) return SIZE_MAX;
    i = number;
  }
  else {
    if ( number < dir->first || number > pages[count - 1].number ) return SIZE_MAX;
    i = _cue_sparse_find(dir, number - dir->first);
    if ( i == SIZE_MAX ) return SIZE_MAX;
  }
  const _cue_page* page = &pages[i];
  unsigned word = (bit >> 6) & 7;
  c3_d mask = UINT64_C(1) << (bit & 63);
  if ( !(page->bits[word] & mask) ) return SIZE_MAX;
  return page->base + page->before[word] + __builtin_popcountll(page->bits[word] & (mask - 1));
}

static void
_cue_directory_free(_cue_directory* dir, c3_t external)
{
  if ( !dir ) return;
  for ( size_t i = 0; i < sizeof(dir->levels) / sizeof(dir->levels[0]); ++i ) {
    if ( !dir->levels[i].groups ) break;
    if ( external ) c3_free(dir->levels[i].groups);
    else u3a_free(dir->levels[i].groups);
  }
  if ( external ) c3_free(dir);
  else u3a_free(dir);
}
