// The assembler measures jumps after this pass. Each replacement still
// occupies one instruction and every branch retains its original target.
typedef struct {
  u3_noun op;
  c3_h    plan;
  c3_y    code;
} _n_projection;

static c3_h
_n_proj_axis(const _n_projection* at)
{
  switch ( at->code ) {
    case HEAD: case HELD: return 2;
    case TAIL: case TALL: return 3;
    case FABK: case FABL: case FASK: case FASL: return u3t(at->op);
    default: return 0;
  }
}

static c3_h
_n_proj_prefix(c3_h a, c3_h b)
{
  if ( !b ) return 1;
  unsigned x = u3x_dep(a), y = u3x_dep(b);
  if ( x > y ) a >>= x - y;
  if ( y > x ) b >>= y - x;
  return a == b ? a : a >> c3_bits_word(a ^ b);
}

static c3_h
_n_proj_suffix(c3_h base, c3_h axis)
{
  c3_h root = 1U << (u3x_dep(axis) - u3x_dep(base));
  return root | (axis & (root - 1));
}

// An edge has three suffix bits, one ownership bit and two size bits.
// A zero size skips a five-byte projection. Its original slot remains
// available to the planner when two neighboring plans share an instruction.
static c3_h
_n_proj_edge(const _n_projection* at, c3_h base, c3_h common)
{
  if ( common < base ) return 0;
  c3_h suffix = _n_proj_suffix(base, _n_proj_axis(at));
  if ( suffix > 7 ) return 0;
  c3_y code = at->code;
  c3_t keep = code == HEAD || code == TAIL || code == FABK || code == FASK;
  c3_h size = at->plan ? 0 : 1 + _n_arg(code);
  return suffix | (keep << 3) | (size << 4);
}

// Bits 0..15 select the anchor. Bits 16..18 select the first field below
// it. The next two six-bit groups describe the yes and no continuations.
// The complete operand fits a direct atom on either loom.
static c3_h
_n_proj_plan(const _n_projection* at)
{
  const _n_projection* yes;
  const _n_projection* no;
  switch ( at[1].code ) {
    case SWAP: yes = no = at + 2; break;
    case DEEP: case SAM0: case SAM1: case SAMB: case SAMS:
      if ( at[2].code != SBIN ) return 0;
      yes = at + 3;
      no = yes + u3t(at[2].op);
      break;
    default: return 0;
  }
  c3_h axis = _n_proj_axis(at);
  c3_h left = _n_proj_prefix(axis, _n_proj_axis(yes));
  c3_h right = _n_proj_prefix(axis, _n_proj_axis(no));
  c3_h base = left == 1 ? right : right == 1 ? left : c3_min(left, right);
  if ( base < 2 ) return 0;
  c3_h first = _n_proj_suffix(base, axis);
  if ( first > 7 ) return 0;
  c3_h on_yes = _n_proj_edge(yes, base, left);
  c3_h on_no = _n_proj_edge(no, base, right);
  if ( !on_yes && !on_no ) return 0;
  return base | (first << 16) | (on_yes << 19) | (on_no << 25);
}

static u3_noun
_n_proj_compile(u3_noun ops)
{
  c3_w len = u3qb_lent(ops);
  if ( len < 3 || len > 65535 ) return ops;
  _n_projection* slots = u3a_calloc(len + 1, sizeof(*slots));
  u3_noun list = ops;
  c3_t changed = 0;
  for ( c3_w i = len; i-- > 0; list = u3t(list) ) {
    _n_projection* at = &slots[i];
    at->op = u3h(list);
    at->code = c3y == u3du(at->op) ? u3h(at->op) : at->op;
    if ( (at->code == FABK || at->code == FASK) &&
         (at->plan = _n_proj_plan(at)) ) {
      changed = 1;
    }
  }
  if ( changed ) {
    u3_noun out = u3_nul;
    for ( c3_w i = 0; i < len; ++i ) {
      _n_projection* at = &slots[i];
      out = u3nc(at->plan ? u3nc(PFRD, at->plan) : u3k(at->op), out);
    }
    u3z(ops);
    ops = out;
  }
  u3a_free(slots);
  return ops;
}
