// Number the input DAG in postorder. Equal atoms and equal pairs of child
// numbers get one record. The input owns every representative throughout jam.
typedef struct {
  c3_w left, right;  // Two class IDs for a cell; an atom and zero otherwise.
  c3_d first;
} _jam_class;

typedef struct { u3_noun noun; c3_w id; } _jam_physical;

typedef struct {
  _jam_class* nodes;
  _jam_physical* physical;
  c3_w* shapes;
  c3_w count, capacity, used, slots, shape_slots;
  c3_d bits;
  c3_t deferred;
  c3_w small[256];
  _jam_class local_nodes[32];
  _jam_physical local_physical[64];
  c3_w local_shapes[64];
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

static c3_w
_jam_intern(_jam_classes* c, c3_w left, c3_w right)
{
  c3_t small = !right && left < 256;
  c3_w slot = 0;
  if ( small ) {
    if ( c->small[left] ) return c->small[left];
  }
  else {
    c3_w mask = c->shape_slots - 1;
    slot = _jam_hash(left, right) & mask;
    while ( c->shapes[slot] ) {
      c3_w id = c->shapes[slot];
      if ( _jam_same(&c->nodes[id - 1], left, right) ) return id;
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
  else c->shapes[slot] = id;
  if ( id == c->shape_slots - c->shape_slots / 4 ) {
    c3_w size = _jam_double(c->shape_slots, sizeof(c3_w));
    c3_w* slots = u3a_calloc(size, sizeof(*slots));
    for ( c3_w i = 0; i < id; ++i ) {
      const _jam_class* n = &c->nodes[i];
      if ( !n->right && n->left < 256 ) continue;
      c3_w j = _jam_hash(n->left, n->right) & (size - 1);
      while ( slots[j] ) j = (j + 1) & (size - 1);
      slots[j] = i + 1;
    }
    _jam_dispose(c->shapes, c->local_shapes);
    c->shapes = slots;
    c->shape_slots = size;
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

static void
_jam_emit_classes(u3_noun root, ur_bsw_t* writer, struct _cs_jam_fib* fib)
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

  if ( writer ) {
    c3_d bytes = (c.bits >> 3) + !!(c.bits & 7);
    if ( bytes >= SIZE_MAX ) u3m_bail(c3__meme);
    ur_bsw_init(writer, ur_fib11, bytes + 1);
  }
  else {
    if ( c.bits > c3_w_max ) u3m_bail(c3__fail);
    fib->a_w = c.bits;
    u3i_slab_init(fib->sab_u, 0, c.bits);
  }
  u3a_pile stack;
  u3a_pile_prep(&stack, sizeof(c3_w), alignof(c3_w));
  for (;;) {
    _jam_class* n = &c.nodes[id - 1];
    c3_d pos = writer ? writer->bits : fib->bit_w;
    c3_d first = n->first;
    c3_w width = n->right ? 0 : u3r_met(0, n->left);
    if ( pos == first && n->right ) {
      if ( writer ) ur_bsw_cell(writer);
      else _cs_jam_fib_chop(fib, 2, 1);
      *(c3_w*)u3a_push(&stack) = n->right;
      id = n->left;
      continue;
    }
    if ( first != pos && (n->right || width > c3_bits_chub(first)) ) {
      if ( writer ) ur_bsw_back64(writer, c3_bits_chub(first), first);
      else {
        u3_atom at = _cs_coin_chub(first);
        _cs_jam_fib_chop(fib, 2, 3);
        _cs_jam_fib_mat(fib, at);
        u3z(at);
      }
    }
    else if ( writer ) _cs_jam_bsw_atom(writer, width, n->left);
    else {
      _cs_jam_fib_chop(fib, 1, 0);
      _cs_jam_fib_mat(fib, n->left);
    }
    if ( c3y == u3a_pile_done(&stack) ) break;
    id = *(c3_w*)u3a_peek(&stack);
    u3a_pop(&stack);
  }
  u3_assert(c.bits == (writer ? writer->bits : fib->bit_w));
  _jam_dispose(c.nodes, c.local_nodes);
}
