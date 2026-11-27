// A protected cue can build its result directly on the receiving road. The
// parser numbers constructors as their operands become available. During
// the take, each spent instruction becomes the noun used by later ones.
// The child heap stays on the parent's stack until the take finishes.
typedef union {
  struct { uint32_t head, tail; } cell;
  u3_noun noun;
} _cue_node;

typedef struct {
  c3_d value;  // literal up to 64 bits, otherwise payload offset in the source
  c3_d width;
} _cue_atom;

struct u3_cue_plan {
  _cue_node* nodes;
  _cue_atom* atoms;
  size_t count, capacity;
  size_t atom_count, atom_capacity;
  uint32_t root;
};

// The low half of the code space holds literal atoms on either loom.
// A definition transfers its initial reference. Each backreference gains it.
#define CUE_NODE   UINT32_C(0x80000000)
#define CUE_GAIN   UINT32_C(0x40000000)
#define CUE_INDEX  UINT32_C(0x3fffffff)

static inline uint32_t
_cue_plan_back(uint32_t ref)
{
  return ref & CUE_NODE ? ref | CUE_GAIN : ref;
}

static uint32_t
_cue_plan_cell(u3_cue_plan* plan, uint32_t head, uint32_t tail)
{
  if ( plan->count == plan->capacity ) {
    size_t next = plan->capacity ? 2 * plan->capacity : 128;
    plan->nodes = u3a_realloc(plan->nodes, plan->capacity * sizeof(_cue_node),
                                           next * sizeof(_cue_node));
    plan->capacity = next;
  }
  uint32_t i = plan->count++;
  plan->nodes[i] = (_cue_node){.cell = {head, tail}};
  return CUE_NODE | i;
}

static uint32_t
_cue_plan_atom(u3_cue_plan* plan, ur_bsr_t* reader, const ur_cue_token_t* token)
{
  c3_d width = token->width;
  if ( width <= 64 && token->value < CUE_NODE ) return token->value;
  if ( plan->atom_count == plan->atom_capacity ) {
    size_t next = plan->atom_capacity ? 2 * plan->atom_capacity : 32;
    plan->atoms = u3a_realloc(plan->atoms, plan->atom_capacity * sizeof(_cue_atom),
                                           next * sizeof(_cue_atom));
    plan->atom_capacity = next;
  }
  c3_d value = width <= 64 ? token->value : reader->bits;
  if ( width > 64 ) ur_bsr_skip_any(reader, width);
  uint32_t i = plan->atom_count++;
  plan->atoms[i] = (_cue_atom){value, width};
  return _cue_plan_cell(plan, UINT32_MAX, i);
}

static inline u3_noun
_cue_plan_value(const u3_cue_plan* plan, uint32_t ref)
{
  if ( !(ref & CUE_NODE) ) return ref;
  u3_noun noun = plan->nodes[ref & CUE_INDEX].noun;
  return ref & CUE_GAIN ? u3k(noun) : noun;
}

u3_noun
u3s_cue_plan_take(u3_cue_plan* plan, u3_atom a)
{
  u3a_atom* vat_u = u3a_to_ptr(a);
  const c3_y* bytes = (const c3_y*)vat_u->buf_w;
  c3_w len_w = u3r_met(3, a);
  for ( size_t i = 0; i < plan->count; i++ ) {
    _cue_node* node = &plan->nodes[i];
    if ( node->cell.head != UINT32_MAX ) {
      u3_noun head = _cue_plan_value(plan, node->cell.head);
      u3_noun tail = _cue_plan_value(plan, node->cell.tail);
      node->noun = u3nc(head, tail);
    }
    else {
      _cue_atom atom = plan->atoms[node->cell.tail];
      if ( atom.width <= 64 ) {
        node->noun = u3i_chub(atom.value);
      }
      else {
        c3_d off = atom.value >> 3;
        ur_bsr_t reader = {.left = len_w - off, .bits = atom.value,
                           .off = atom.value & 7, .bytes = bytes + off};
        u3i_slab slab;
        u3i_slab_init(&slab, 0, atom.width);
        ur_bsr_bytes_any(&reader, atom.width, slab.buf_y);
        node->noun = u3i_slab_mint_bytes(&slab);
      }
    }
  }
  return _cue_plan_value(plan, plan->root);
}

#undef CUE_NODE
#undef CUE_GAIN
#undef CUE_INDEX
