// A projection returns a view owned by the original subject. A plan ends
// before any call, hint or branch that could observe an intermediate value.
#define N_BORROW_TAG c3_s4('b','r','o','w')
#define N_BORROW_KEEP 8

static c3_w
_n_borrow_axis(u3_noun formula)
{
  u3_noun axis;
  return c3y == u3r_p(formula, 0, &axis) && axis && axis <= 65535 ? axis : 0;
}

static u3_weak
_n_borrow_plan(u3_noun op, u3_noun arg, c3_o lose)
{
  u3_noun a, b, lhs, rhs, edit;
  if ( op != 5 && op != 7 && op != 10 ) return u3_none;
  if ( c3n == u3r_cell(arg, &a, &b) ) return u3_none;

  c3_w base, left, right = 0, kind;
  if ( op == 5 ) {
    kind = 0;
    base = _n_borrow_axis(a);
    left = _n_borrow_axis(b);
  }
  else {
    u3_noun fields;
    if ( op == 7 ) {
      kind = 1;
      base = _n_borrow_axis(a);
      fields = b;
    }
    else {
      if ( c3n == u3r_cell(a, &edit, &fields) ||
           (edit != 2 && edit != 3 && edit != 6) ) return u3_none;
      kind = edit;
      base = _n_borrow_axis(b);
    }
    if ( !base ) return u3_none;
    left = _n_borrow_axis(fields);
    if ( !left ) {
      if ( c3n == u3r_cell(fields, &lhs, &rhs) ) return u3_none;
      left = _n_borrow_axis(lhs);
      right = _n_borrow_axis(rhs);
      if ( !right ) return u3_none;
    }
    else if ( kind == 1 ) return u3_none;
  }
  if ( !base || !left ) return u3_none;
  // A losing identity already transfers the original reference to the result.
  if ( kind == 1 && (right == 1 || (base == 1 && lose == c3y)) ) return u3_none;
  if ( kind != 1 && lose == c3y && (right ? right : left) == 1 ) return u3_none;
  return u3nq(kind | (lose == c3n ? N_BORROW_KEEP : 0), base, left, right);
}

// Packing happens after branch threading and operand sizing. This region
// contains only straight-line slot reads, stack operations and the consumer.
static u3_atom
_n_borrow_pack(u3n_prog* prog, c3_w pc, u3_noun descriptor)
{
  const c3_y* code = prog->byc_u.ops_y;
  c3_w end = prog->byc_u.len_w, n = u3h(descriptor);
  if ( pc >= end || end - pc < 4 || code[pc] != TOSS ||
       code[pc+1] != SWAP || code[pc+2] != TOSS ) return 0;
  pc += 3;
  for ( c3_w i = 0; i < n; ++i ) {
    if ( pc >= end ) return 0;
    c3_y op = code[pc];
    switch ( op ) {
      case COPY: case SWAP: case AUTO: case AULT:
      case HEAD: case HELD: case TAIL: case TALL:
      case FABK: case FASK: case FABL: case FASL:
      case SAME: case SALM: case MUTH: case KUTH:
      case MUTT: case KUTT: case MUSM: case KUSM: break;
      default: return 0;
    }
    pc += 1 + _n_arg(op);
  }
  if ( pc >= end ) return 0;
  u3_noun kind, base, left, right;
  u3x_qual(u3t(descriptor), &kind, &base, &left, &right);
  c3_h words[] = {pc, kind, base, left, right, 1};
  return u3i_halfs(6, words);
}
