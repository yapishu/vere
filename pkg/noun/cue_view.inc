// An entry is zero while reading a head, then holds its tail's bit offset.
// Completion makes an atom or cell available to subsequent backreferences.
#define CUE_COMPLETE (UINT64_C(1) << 63)

struct _u3_cue_view {
  const c3_y* bytes;
  c3_d len;
  _cue_page* pages;
  _cue_directory* directory;
  size_t page_count, page_capacity;
  c3_d* tails;
  size_t count, capacity;
  ur_dictn_t memo;
  u3_cue_xeno* full;
};

static void*
_cue_view_grow(void* old, size_t* capacity, size_t width)
{
  if ( *capacity > SIZE_MAX / width / 2 ) u3m_bail(c3__meme);
  size_t next = *capacity ? *capacity * 2 : 32;
  void* grown = c3_realloc(old, next * width);
  *capacity = next;
  return grown;
}

static size_t
_cue_view_start(u3_cue_view* v, c3_d bit, c3_d entry)
{
  c3_d number = bit >> 9;
  if ( !v->page_count || v->pages[v->page_count - 1].number != number ) {
    if ( v->page_count == v->page_capacity )
      v->pages = _cue_view_grow(v->pages, &v->page_capacity, sizeof(*v->pages));
    _cue_page_init(v->pages, v->page_count++, number, v->count,
                   &v->directory, 1);
  }
  if ( v->count == v->capacity )
    v->tails = _cue_view_grow(v->tails, &v->capacity, sizeof(*v->tails));
  size_t slot = v->count++;
  _cue_page_mark(&v->pages[v->page_count - 1], bit, slot);
  v->tails[slot] = entry;
  return slot;
}

u3_cue_view*
u3s_cue_view_new(void)
{
  return c3_calloc(sizeof(u3_cue_view));
}

c3_o
u3s_cue_view_open(u3_cue_view* v, c3_d len, const c3_y* bytes)
{
  u3_assert(u3R == &u3H->rod_u);
  v->bytes = 0;
  if ( v->directory ) v->directory->height = 0;
  v->page_count = v->count = 0;
  ur_bsr_t reader = {0};
  if ( !len || len > (CUE_COMPLETE - 1) / 8 ||
       ur_cue_good != ur_bsr_init(&reader, len, bytes) ) return c3n;

  u3a_pile stack;
  u3a_pile_prep(&stack, sizeof(size_t), __alignof__(size_t));
  while ( 1 ) {
    c3_d bit = reader.bits;
    ur_cue_token_t token;
    if ( bit >= len * 8 || ur_cue_good != ur_bsr_token(&reader, &token) ) break;
    for ( unsigned i = 0; i < token.cells; i++, bit += 2 )
      *(size_t*)u3a_push(&stack) = _cue_view_start(v, bit, 0);
    if ( token.tag == ur_jam_cell ) continue;
    if ( token.tag == ur_jam_back ) {
      size_t slot = _cue_page_find(v->pages, v->page_count, v->directory, token.value);
      if ( slot == SIZE_MAX || !(v->tails[slot] & CUE_COMPLETE) ) break;
    }
    else {
      if ( ((token.width + 7) >> 3) > c3_w_max ) break;
      _cue_view_start(v, bit, CUE_COMPLETE);
      if ( token.width > 64 ) ur_bsr_skip_any(&reader, token.width);
    }
    while ( c3n == u3a_pile_done(&stack) ) {
      size_t slot = *(size_t*)u3a_peek(&stack);
      if ( !v->tails[slot] ) {
        v->tails[slot] = reader.bits;
        goto next;
      }
      v->tails[slot] |= CUE_COMPLETE;
      u3a_pop(&stack);
    }
    v->len = len;
    v->bytes = bytes;
    return c3y;
  next: ;
  }
  while ( c3n == u3a_pile_done(&stack) ) u3a_pop(&stack);
  return c3n;
}

// Backreferences in an indexed stream always name an atom or cell directly.
static size_t
_cue_view_node(u3_cue_view* v, c3_d* bit)
{
  size_t slot = _cue_page_find(v->pages, v->page_count, v->directory, *bit);
  if ( slot == SIZE_MAX ) {
    ur_bsr_t r = { .bits = *bit + 2, .left = v->len - ((*bit + 2) >> 3),
                   .off = (*bit + 2) & 7, .bytes = v->bytes + ((*bit + 2) >> 3) };
    c3_d width;
    u3_assert(ur_cue_good == ur_bsr_rub_len(&r, &width));
    *bit = ur_bsr64_any(&r, width);
    slot = _cue_page_find(v->pages, v->page_count, v->directory, *bit);
    u3_assert(slot != SIZE_MAX);
  }
  return slot;
}

static u3_atom
_cue_view_atom(u3_cue_view* v, c3_d bit)
{
  ++bit; // Skip the atom tag.
  ur_bsr_t r = { .bits = bit, .left = v->len - (bit >> 3),
                 .off = bit & 7, .bytes = v->bytes + (bit >> 3) };
  c3_d width;
  u3_assert(ur_cue_good == ur_bsr_rub_len(&r, &width));
  if ( width < u3a_word_bits ) return (u3_noun)ur_bsrn_any(&r, width);
  u3i_slab slab;
  u3i_slab_init(&slab, 3, (width + 7) >> 3);
  ur_bsr_bytes_any(&r, width, slab.buf_y);
  return u3i_slab_mint_bytes(&slab);
}

// Borrow completed nouns from pending frames and the growing result. Wipe
// previous entries before lookup, including after an interrupted extraction.
static u3_noun
_cue_view_tree(u3_cue_view* v, c3_d bit, size_t slot)
{
  typedef struct { c3_d bit, tail; u3_noun head; } frame;
  u3a_pile stack;
  u3a_pile_prep(&stack, sizeof(frame), __alignof__(frame));
  if ( v->memo.size ) ur_dictn_wipe(&v->memo);
  else ur_dictn_grow(0, &v->memo, ur_fib10, ur_fib11);
  while ( 1 ) {
    c3_d tail = v->tails[slot] ^ CUE_COMPLETE;
    u3_noun noun;
    if ( ur_dictn_get(0, &v->memo, bit, &noun) ) noun = u3k(noun);
    else if ( tail ) {
      *(frame*)u3a_push(&stack) = (frame){bit, tail, u3_none};
      bit += 2;
      slot = _cue_view_node(v, &bit);
      continue;
    }
    else {
      noun = _cue_view_atom(v, bit);
      ur_dictn_put(0, &v->memo, bit, noun);
    }
    while ( c3n == u3a_pile_done(&stack) ) {
      frame* f = u3a_peek(&stack);
      if ( f->head == u3_none ) {
        f->head = noun;
        bit = f->tail;
        slot = _cue_view_node(v, &bit);
        goto next;
      }
      noun = u3nc(f->head, noun);
      ur_dictn_put(0, &v->memo, f->bit, noun);
      u3a_pop(&stack);
    }
    return noun;
  next: ;
  }
}

u3_weak
u3s_cue_view_at(u3_cue_view* v, u3_atom axis)
{
  u3_assert(u3R == &u3H->rod_u);
  if ( !v->bytes || !axis ) return u3_none;
  c3_d bit = 0;
  size_t slot = 0;
  for ( c3_w depth = u3r_met(0, axis) - 1; depth; --depth ) {
    c3_d tail = v->tails[slot] ^ CUE_COMPLETE;
    if ( !tail ) return u3_none;
    bit = u3r_bit(depth - 1, axis) ? tail : bit + 2;
    slot = _cue_view_node(v, &bit);
  }
  if ( v->tails[slot] == CUE_COMPLETE ) return _cue_view_atom(v, bit);
  // The root has no external backreferences. Sequential cue can fill it
  // without seeking to each child and still leaves the index reusable.
  if ( axis == 1 ) {
    if ( !v->full ) v->full = u3s_cue_xeno_init();
    return u3s_cue_xeno_with(v->full, v->len, v->bytes);
  }
  return _cue_view_tree(v, bit, slot);
}

void
u3s_cue_view_free(u3_cue_view* v)
{
  _cue_directory_free(v->directory, 1);
  c3_free(v->pages);
  c3_free(v->tails);
  if ( v->full ) u3s_cue_xeno_done(v->full);
  if ( v->memo.size ) ur_dict_free((ur_dict_t*)&v->memo);
  c3_free(v);
}

#undef CUE_COMPLETE
