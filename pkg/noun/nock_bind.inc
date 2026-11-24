// A Nock 8 subject is [binding subject]. Substitute the binding at the
// first evaluated slot and rebase later slots onto the original subject.
// Refuse a second use of the binding, an observation of the whole pair,
// or an instruction whose order or subject this pass does not model.
static u3_weak
_n_bind_body(u3_noun formula, u3_noun binding, c3_t* first, c3_w* budget)
{
  u3_noun op, arg, left, right, axis = 0;
  if ( !*budget || c3n == u3r_cell(formula, &op, &arg) ) return u3_none;
  --*budget;

  if ( op == 0 ) {
    if ( arg < 2 || arg > 0x7fffffffU ) return u3_none;
    c3_w direction = 1U << (u3x_dep((c3_w)arg) - 1);
    c3_w suffix = direction | (arg & (direction - 1));
    if ( arg & direction ) return *first ? u3_none : u3nc(0, suffix);
    if ( !*first ) return u3_none;
    *first = 0;
    return suffix == 1 ? u3k(binding) : u3nt(7, u3k(binding), u3nc(0, suffix));
  }
  if ( op == 1 ) return *first ? u3_none : u3k(formula);

  if ( op == 3 || op == 4 ) {
    u3_weak child = _n_bind_body(arg, binding, first, budget);
    return child == u3_none ? child : u3nc(op, child);
  }

  if ( op == 7 || op == 9 ) {
    if ( c3n == u3r_cell(arg, &left, &right) ) return u3_none;
    u3_weak subject = _n_bind_body(op == 7 ? left : right, binding, first, budget);
    if ( subject == u3_none ) return u3_none;
    return op == 7 ? u3nt(7, subject, u3k(right)) : u3nt(9, u3k(left), subject);
  }

  if ( c3y == u3du(op) ) {
    left = op;
    right = arg;
  }
  else if ( op == 10 ) {
    u3_noun edit;
    // The VM evaluates the target before the replacement.
    if ( c3n == u3r_cell(arg, &edit, &left) ||
         c3n == u3r_cell(edit, &axis, &right) || c3n == u3ud(axis) ) return u3_none;
  }
  else return u3_none;

  u3_weak a = _n_bind_body(left, binding, first, budget);
  if ( a == u3_none ) return u3_none;
  u3_weak b = _n_bind_body(right, binding, first, budget);
  if ( b == u3_none ) {
    u3z(a);
    return u3_none;
  }
  return op == 10 ? u3nt(10, u3nc(u3k(axis), b), a) : u3nc(a, b);
}
