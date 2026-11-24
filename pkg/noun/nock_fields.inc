// A split entry consumes one or two owned operands on the ordinary VM stack.
// The bytecode following the projections is shared with the full-core entry.
typedef struct {
  u3_noun first, second;
  c3_w ip;
} _n_field_entry;

enum { _n_field_steps = 8 };

static inline c3_w
_n_field_arg(const c3_y* pc)
{
  switch ( _n_arg(*pc) ) {
    case 1: return pc[1];
    case 2: return pc[1] | ((c3_w)pc[2] << 8);
    default: return 0;
  }
}

static inline c3_t
_n_field_keep(c3_y op)
{
  return op == HEAD || (op >= FABK && op <= FISK) ||
         (op >= LIT0 && op <= LISK);
}

static inline c3_t
_n_field_lose(c3_y op)
{
  return op == HELD || (op >= FABL && op <= FISL) ||
         (op >= LIL0 && op <= LISL);
}

static inline c3_t
_n_field_test(c3_y op)
{
  return op == DEEP || (op >= SAM0 && op <= SANS);
}

static c3_t
_n_field_paths(const c3_y* pc, unsigned depth)
{
  if ( !depth ) return 0;
  if ( _n_field_lose(*pc) ) return 1;
  if ( !_n_field_keep(*pc) ) return 0;
  pc += 1 + _n_arg(*pc);
  if ( *pc == SWAP ) return _n_field_lose(pc[1]);
  if ( !_n_field_test(*pc) ) return 0;
  pc += 1 + _n_arg(*pc);
  if ( *pc != SBIN && *pc != SINS ) return 0;
  c3_w skip = _n_field_arg(pc);
  pc += 1 + _n_arg(*pc);
  return _n_field_paths(pc, depth - 1) && _n_field_paths(pc + skip, depth - 1);
}

static c3_t
_n_field_eligible(const c3_y* pc)
{
  // Reject a prefix if either side of a test keeps the entire subject.
  return _n_field_paths(pc, _n_field_steps);
}

static u3_weak
_n_field_load(const u3n_prog* arm, const c3_y* pc, u3_noun gate, u3_noun sample)
{
  c3_w arg = _n_field_arg(pc);
  u3_noun axis;
  switch ( *pc ) {
    case HEAD: case HELD: return u3h(gate);
    case LIT0: case LIL0: return 0;
    case LIT1: case LIL1: return 1;
    case LITB: case LITS: case LILB: case LILS: return arg;
    case LIBK: case LISK: case LIBL: case LISL: return arm->lit_u.non[arg];
    case FIBK: case FISK: case FIBL: case FISL: axis = arm->lit_u.non[arg]; break;
    default: axis = arg; break;
  }
  if ( axis < 4 || axis > 0xffff ) return u3_none;
  unsigned shift = u3x_dep(axis) - 2;
  c3_w root = 1U << shift;
  if ( (axis >> shift) == 6 ) {
    return axis == 6 ? sample : u3r_at(root | (axis & (root - 1)), sample);
  }
  // The battery and context still belong to the template.
  return u3r_at(axis, gate);
}

static c3_t
_n_field_enter(const u3n_prog* arm, u3_noun gate, u3_noun sample, _n_field_entry* entry)
{
  const c3_y* start = arm->byc_u.ops_y;
  const c3_y* pc = start;
  for ( unsigned steps = 0; steps < _n_field_steps; steps++ ) {
    c3_y op = *pc;
    if ( !_n_field_keep(op) && !_n_field_lose(op) ) return 0;
    u3_weak first = _n_field_load(arm, pc, gate, sample);
    if ( first == u3_none ) return 0;
    pc += 1 + _n_arg(op);
    if ( _n_field_lose(op) ) {
      *entry = (_n_field_entry){u3k(first), u3_none, pc - start};
      return 1;
    }
    if ( *pc == SWAP && _n_field_lose(pc[1]) ) {
      ++pc;
      u3_weak second = _n_field_load(arm, pc, gate, sample);
      if ( second == u3_none ) return 0;
      pc += 1 + _n_arg(*pc);
      *entry = (_n_field_entry){u3k(first), u3k(second), pc - start};
      return 1;
    }
    if ( !_n_field_test(*pc) ) return 0;
    c3_y test = *pc;
    c3_w arg = _n_field_arg(pc);
    pc += 1 + _n_arg(test);
    if ( *pc != SBIN && *pc != SINS ) return 0;
    c3_t yes;
    if ( test == DEEP ) yes = c3y == u3du(first);
    else {
      u3_noun literal = (test == SAM0 || test == SAM1) ? (u3_noun)(test == SAM1)
                      : (test == SANB || test == SANS) ? arm->lit_u.non[arg] : arg;
      // Mutating equality must execute exactly once on the ordinary path.
      if ( first != literal && c3n == u3a_is_cat(first) && c3n == u3a_is_cat(literal) )
        return 0;
      yes = first == literal;
    }
    c3_w skip = _n_field_arg(pc);
    pc += 1 + _n_arg(*pc);
    if ( !yes ) pc += skip;
  }
  return 0;
}
