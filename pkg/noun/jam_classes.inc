// Number the input DAG in postorder. Equal atoms and equal pairs of child
// numbers get one record. The input owns every representative throughout jam.
typedef struct {
  c3_w left, right;  // Two class IDs for a cell; an atom and zero otherwise.
  c3_d first;
} _jam_class;

typedef struct { u3_noun noun; c3_w id; } _jam_physical;
typedef struct { c3_w id; c3_h hash; } _jam_shape;

typedef struct {
  _jam_class* nodes;
  _jam_physical* physical;
  _jam_shape* shapes;
  c3_w count, capacity, used, slots, indexed, shape_slots;
  c3_d bits;
  c3_t deferred;
  c3_w small[256];
  _jam_class local_nodes[32];
  _jam_physical local_physical[64];
  _jam_shape local_shapes[32];
} _jam_classes;

static inline c3_h
_jam_mix(c3_d x)
{
  x ^= x >> 33;
  x *= UINT64_C(0xff51afd7ed558ccd);
  x ^= x >> 33;
  return (c3_h)x;
}

static c3_w
_jam_double(c3_w size, size_t width)
{
  if ( size > c3_w_max / 2 || size > (SIZE_MAX - 64) / width / 2 )
    u3m_bail(c3__meme);
  return size * 2;
}

static void
_jam_dispose(void* array, void* local)
{
  if ( array != local ) u3a_free(array);
}

static _jam_physical*
_jam_address(_jam_classes* c, u3_noun noun)
{
  c3_w mask = c->slots - 1, i = _jam_mix(noun) & mask;
  while ( c->physical[i].id && c->physical[i].noun != noun ) i = (i + 1) & mask;
  return &c->physical[i];
}

static void
_jam_remember(_jam_classes* c, u3_noun noun, c3_w id)
{
  if ( c->used == c->slots - c->slots / 4 ) {
    _jam_physical* old = c->physical;
    c3_w size = c->slots;
    c->slots = _jam_double(size, sizeof(*old));
    c->physical = u3a_calloc(c->slots, sizeof(*old));
    for ( c3_w i = 0; i < size; ++i ) {
      if ( old[i].id ) *_jam_address(c, old[i].noun) = old[i];
    }
    _jam_dispose(old, c->local_physical);
  }
  *_jam_address(c, noun) = (_jam_physical){noun, id};
  ++c->used;
}

static inline c3_h
_jam_hash(c3_w left, c3_w right)
{
  if ( right ) return _jam_mix(((c3_d)left << 32) ^ right);
  return c3y == u3a_is_cat(left) ? _jam_mix(left) : u3r_mug(left);
}

static c3_t
_jam_same(const _jam_class* n, c3_w left, c3_w right)
{
  if ( n->right != right ) return 0;
  if ( n->left == left ) return 1;
  if ( right || c3y == u3a_is_cat(left) || c3y == u3a_is_cat(n->left) ) return 0;
  u3a_atom* a = u3a_to_ptr(left);
  u3a_atom* b = u3a_to_ptr(n->left);
  return a->len_w == b->len_w && a->mug_w == b->mug_w &&
         !memcmp(a->buf_w, b->buf_w, a->len_w * sizeof(c3_w));
}

static void
_jam_grow_shapes(_jam_classes* c)
{
  c3_w size = _jam_double(c->shape_slots, sizeof(*c->shapes));
  _jam_shape* slots = u3a_calloc(size, sizeof(*slots));
  for ( c3_w i = 0; i < c->shape_slots; ++i ) {
    _jam_shape entry = c->shapes[i];
    if ( !entry.id ) continue;
    c3_w j = entry.hash & (size - 1);
    while ( slots[j].id ) j = (j + 1) & (size - 1);
    slots[j] = entry;
  }
  _jam_dispose(c->shapes, c->local_shapes);
  c->shapes = slots;
  c->shape_slots = size;
}

static c3_w
_jam_intern(_jam_classes* c, c3_w left, c3_w right)
{
  c3_t small = !right && left < 256;
  c3_w child = right ? c3_max(left, right) : 0;
  // A cell consuming the newest class gets the very next ID.
  c3_t indexed = !small && (!right || child < c->count);
  c3_h hash = 0;
  c3_w slot = 0;
  if ( small ) {
    if ( c->small[left] ) return c->small[left];
  }
  else if ( indexed ) {
    // In postorder a parent can immediately follow its newer child. Its ID
    // is then child + 1, so the class record itself serves as its index.
    if ( right ) {
      const _jam_class* n = &c->nodes[child];
      if ( n->left == left && n->right == right ) return child + 1;
    }
    c3_w mask = c->shape_slots - 1;
    hash = _jam_hash(left, right);
    slot = hash & mask;
    while ( c->shapes[slot].id ) {
      _jam_shape entry = c->shapes[slot];
      if ( entry.hash == hash && _jam_same(&c->nodes[entry.id - 1], left, right) )
        return entry.id;
      slot = (slot + 1) & mask;
    }
  }
  if ( c->count == c->capacity ) {
    c3_w size = _jam_double(c->capacity, sizeof(*c->nodes));
    _jam_class* nodes = u3a_malloc(size * sizeof(*nodes));
    memcpy(nodes, c->nodes, c->count * sizeof(*nodes));
    _jam_dispose(c->nodes, c->local_nodes);
    c->nodes = nodes;
    c->capacity = size;
  }
  c3_w id = ++c->count;
  c->nodes[id - 1] = (_jam_class){left, right, UINT64_MAX};
  if ( small ) c->small[left] = id;
  else if ( indexed ) {
    c->shapes[slot] = (_jam_shape){id, hash};
    if ( ++c->indexed == c->shape_slots - c->shape_slots / 4 ) _jam_grow_shapes(c);
  }
  return id;
}

static inline c3_d
_jam_token_end(c3_d start, c3_d tag, c3_d width)
{
  c3_d prefix = tag + (width ? 2 * c3_bits_chub(width) : 1);
  if ( start > UINT64_MAX - prefix || width > UINT64_MAX - prefix - start )
    u3m_bail(c3__fail);
  return start + prefix + width;
}

static c3_w
_jam_number(_jam_classes* c, u3_noun noun)
{
  typedef struct { u3_noun noun; c3_w head; c3_d start; c3_t memo; } frame;
  u3a_pile stack;
  u3a_pile_prep(&stack, sizeof(frame), alignof(frame));
  for (;;) {
    c3_w id;
    if ( c3y == u3du(noun) ) {
      // Only current-road cells with exact counts can prove a single incoming
      // edge. Other roads can borrow references, and home gains can be pending.
      c3_t memo = c->deferred || c3y == u3a_is_senior(u3R, noun) ||
                  c3y == u3a_is_junior(u3R, noun) || u3a_use(noun) != 1;
      id = memo ? _jam_address(c, noun)->id : 0;
      if ( !id ) {
        *(frame*)u3a_push(&stack) = (frame){noun, 0, c->bits, memo};
        if ( c->bits > UINT64_MAX - 2 ) u3m_bail(c3__fail);
        c->bits += 2;
        noun = u3h(noun);
        continue;
      }
    }
    else id = _jam_intern(c, noun, 0);
    _jam_class* n = &c->nodes[id - 1];
    c3_w width = n->right ? 0 : u3r_met(0, noun);
    if ( n->first != UINT64_MAX && (n->right || width > c3_bits_chub(n->first)) )
      c->bits = _jam_token_end(c->bits, 2, c3_bits_chub(n->first));
    else {
      if ( n->first == UINT64_MAX ) n->first = c->bits;
      c->bits = _jam_token_end(c->bits, 1, width);
    }
    for (;;) {
      if ( c3y == u3a_pile_done(&stack) ) return id;
      frame* f = u3a_peek(&stack);
      if ( !f->head ) {
        f->head = id;
        noun = u3t(f->noun);
        break;
      }
      id = _jam_intern(c, f->head, id);
      if ( f->memo ) _jam_remember(c, f->noun, id);
      n = &c->nodes[id - 1];
      if ( n->first == UINT64_MAX ) n->first = f->start;
      // An equal completed cell already contains both child classes and
      // all their descendants. No new first positions need to be undone.
      else c->bits = _jam_token_end(f->start, 2, c3_bits_chub(n->first));
      u3a_pop(&stack);
    }
  }
}

// Pack low bits first, keeping a partial word until a full word can be stored.
// See the Zstandard bit accumulator.
// https://github.com/facebook/zstd/blob/dev/lib/common/bitstream.h
typedef struct {
  c3_y* next;
  c3_y* end;
  c3_d word;
  unsigned used;
} _jam_output;

static inline void
_jam_put(_jam_output* out, unsigned count, c3_d value)
{
  unsigned used = out->used;
  out->word |= value << used;
  unsigned total = used + count;
  if ( total >= 64 ) {
    u3_assert(out->end - out->next >= 8);
    memcpy(out->next, &out->word, 8);
    out->next += 8;
    out->word = used ? value >> (64 - used) : 0;
  }
  out->used = total & 63;
}

static void
_jam_put_atom(_jam_output* out, u3_atom a, c3_w width)
{
  if ( c3y == u3a_is_cat(a) ) {
    _jam_put(out, width, a);
    return;
  }
  u3a_atom* atom = u3a_to_ptr(a);
  const c3_y* src = (const c3_y*)atom->buf_w;
  size_t words = width >> 6;
  if ( words ) {
    u3_assert(words <= (size_t)(out->end - out->next) / 8);
    if ( !out->used ) {
      memcpy(out->next, src, words * 8);
    }
    else {
      unsigned lo = out->used, hi = 64 - lo;
      c3_d carry = out->word;
      for ( size_t i = 0; i < words; i++ ) {
        c3_d x;
        memcpy(&x, src + 8 * i, 8);
        c3_d part = carry | (x << lo);
        memcpy(out->next + 8 * i, &part, 8);
        carry = x >> hi;
      }
      out->word = carry;
    }
    out->next += words * 8;
    src += words * 8;
  }
  unsigned rest = width & 63;
  if ( rest ) {
    c3_d tail = 0;
    memcpy(&tail, src, (rest + 7) >> 3);
    _jam_put(out, rest, tail);
  }
}

// A mat of nonzero width has k zero bits, then a one, then the low k-1
// bits of the width. Short headers and their tags fit in one word.
static inline void
_jam_put_header(_jam_output* out, c3_d width, unsigned tagbits)
{
  c3_d tag = tagbits == 2 ? 3 : 0;
  if ( !width ) {
    _jam_put(out, tagbits + 1, tag | (UINT64_C(1) << tagbits));
    return;
  }
  unsigned k = c3_bits_chub(width);
  c3_d suffix = ((width ^ (UINT64_C(1) << (k - 1))) << 1) | 1;
  if ( 2 * k + tagbits <= 64 ) {
    _jam_put(out, 2 * k + tagbits, tag | (suffix << (k + tagbits)));
  }
  else {
    _jam_put(out, tagbits, tag);
    _jam_put(out, k, 0);
    _jam_put(out, k, suffix);
  }
}

static c3_d
_jam_emit_classes(u3_noun root, u3i_slab* slab, c3_y prefix, c3_y** bytes)
{
  _jam_classes c = {0};
  c.deferred = u3a_defer_active();
  c.nodes = c.local_nodes;
  c.physical = c.local_physical;
  c.shapes = c.local_shapes;
  c.capacity = sizeof(c.local_nodes) / sizeof(*c.nodes);
  c.slots = sizeof(c.local_physical) / sizeof(*c.physical);
  c.shape_slots = sizeof(c.local_shapes) / sizeof(*c.shapes);
  c3_w id = _jam_number(&c, root);
  _jam_dispose(c.physical, c.local_physical);
  _jam_dispose(c.shapes, c.local_shapes);

  c3_d payload = (c.bits >> 3) + !!(c.bits & 7);
  if ( payload > SIZE_MAX - prefix ) u3m_bail(c3__meme);
  c3_y* start;
  if ( slab ) {
    if ( c.bits > c3_w_max || c.bits > UINT64_MAX - (u3a_word_bits - 1) ) u3m_bail(c3__fail);
    u3i_slab_bare(slab, 0, c.bits);
    // The emitter initializes each payload byte. Clear the final word for padding.
    slab->buf_w[slab->len_w - 1] = 0;
    start = slab->buf_y;
  }
  else {
    *bytes = c3_malloc(prefix + payload);
    start = *bytes + prefix;
  }
  STATIC_ASSERT(c3_endian == c3_endian_little, "");
  _jam_output out = { .next = start, .end = start + payload };
  u3a_pile stack;
  u3a_pile_prep(&stack, sizeof(c3_w), alignof(c3_w));
  for (;;) {
    _jam_class* n = &c.nodes[id - 1];
    c3_d pos = ((c3_d)(out.next - start) << 3) + out.used;
    c3_d first = n->first;
    c3_w width = n->right ? 0 : u3r_met(0, n->left);
    if ( pos == first && n->right ) {
      _jam_put(&out, 2, 1);
      *(c3_w*)u3a_push(&stack) = n->right;
      id = n->left;
      continue;
    }
    if ( first != pos && (n->right || width > c3_bits_chub(first)) ) {
      unsigned k = c3_bits_chub(first);
      _jam_put_header(&out, k, 2);
      _jam_put(&out, k, first);
    }
    else {
      _jam_put_header(&out, width, 1);
      _jam_put_atom(&out, n->left, width);
    }
    if ( c3y == u3a_pile_done(&stack) ) break;
    id = *(c3_w*)u3a_peek(&stack);
    u3a_pop(&stack);
  }
  u3_assert(c.bits == ((c3_d)(out.next - start) << 3) + out.used);
  memcpy(out.next, &out.word, (out.used + 7) >> 3);
  _jam_dispose(c.nodes, c.local_nodes);
  return c.bits;
}
