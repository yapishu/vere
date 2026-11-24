// A page covers 512 bit positions in the input. Only pages that contain an
// atom or cell start exist. The rank of a start bit gives its noun slot.
typedef struct {
  c3_d   number;
  c3_d   bits[8];
  size_t base[8];
} _cue_page;

static inline void
_cue_page_mark(_cue_page* page, c3_d bit, size_t slot)
{
  unsigned word = (bit >> 6) & 7;
  if ( !page->bits[word] ) page->base[word] = slot;
  page->bits[word] |= UINT64_C(1) << (bit & 63);
}

static inline size_t
_cue_page_find(_cue_page* pages, size_t count, c3_d bit)
{
  c3_d number = bit >> 9;
  size_t i;
  if ( number < count && pages[number].number == number ) {
    i = number;
  }
  else {
    size_t lo = 0, hi = count;
    while ( lo < hi ) {
      size_t mid = lo + (hi - lo) / 2;
      if ( pages[mid].number < number ) lo = mid + 1;
      else hi = mid;
    }
    if ( lo == count || pages[lo].number != number ) return SIZE_MAX;
    i = lo;
  }
  _cue_page* page = &pages[i];
  unsigned word = (bit >> 6) & 7;
  c3_d mask = UINT64_C(1) << (bit & 63);
  if ( !(page->bits[word] & mask) ) return SIZE_MAX;
  return page->base[word] + __builtin_popcountll(page->bits[word] & (mask - 1));
}
