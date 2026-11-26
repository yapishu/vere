/// Differential tests for the Nock compiler. The oracle evaluates the
// terminating subset used by the generator directly on nouns.

#include "noun.h"

static c3_h seed = 0xa2c87f19;
static c3_h cases, exits;

static c3_w
_rand(void)
{
  seed ^= seed << 13;
  seed ^= seed >> 17;
  return seed ^= seed << 5;
}

static u3_noun
_slot(u3_atom axis)
{
  return u3nc(0, axis);
}

static u3_noun
_lit(u3_noun val)
{
  return u3nc(1, val);
}

static u3_noun
_tag(c3_w i)
{
  switch ( i % 8 ) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 255;
    case 3: return 256;
    case 4: return 65535;
    case 5: return 65536 + i;
    case 6: return u3i_chub(0xabc00000000ULL + i);
    default: return u3nc(i, u3i_string("tag"));
  }
}

static u3_noun
_tree(c3_w depth)
{
  if ( !depth || !(_rand() % 3) ) return _tag(_rand() % 32);
  u3_noun left = _tree(depth - 1);
  return u3nc(left, _tree(depth - 1));
}

// Replace at an axis by rebuilding the path from borrowed children.
static u3_weak
_ref_edit(u3_noun old, u3_atom axis, c3_w depth, u3_noun value)
{
  if ( !depth ) return u3k(value);
  if ( c3n == u3du(old) ) return u3_none;
  c3_t tail = u3r_bit(depth - 1, axis);
  u3_weak sub = _ref_edit(tail ? u3t(old) : u3h(old), axis, depth - 1, value);
  if ( sub == u3_none ) return sub;
  return tail ? u3nc(u3k(u3h(old)), sub) : u3nc(sub, u3k(u3t(old)));
}

// Return an owned noun or u3_none for an ordinary Nock crash.
static u3_weak
_eval(u3_noun bus, u3_noun fol)
{
  u3_noun op = u3h(fol), arg = u3t(fol), a, b;
  if ( c3y == u3du(op) ) {
    if ( (a = _eval(bus, op)) == u3_none ) return a;
    b = _eval(bus, arg);
    if ( b != u3_none ) return u3nc(a, b);
    u3z(a);
    return b;
  }
  switch ( op ) {
    case 0: {
      if ( !arg ) return u3_none;
      c3_w bits = u3r_met(0, arg) - 1;
      while ( bits ) {
        if ( c3n == u3du(bus) ) return u3_none;
        --bits;
        bus = u3r_bit(bits, arg) ? u3t(bus) : u3h(bus);
      }
      return u3k(bus);
    }
    case 1:
      return u3k(arg);
    case 2: {
      if ( (a = _eval(bus, u3h(arg))) == u3_none ) return a;
      b = _eval(bus, u3t(arg));
      u3_weak result = (b == u3_none) ? u3_none : _eval(a, b);
      if ( b != u3_none ) u3z(b);
      u3z(a);
      return result;
    }
    case 3:
    case 4:
      if ( (a = _eval(bus, arg)) == u3_none ) return a;
      if ( op == 4 && c3y == u3ud(a) ) return u3i_vint(a);
      b = op == 3 ? u3du(a) : u3_none;
      u3z(a);
      return b;
    case 5:
      if ( (a = _eval(bus, u3h(arg))) == u3_none ) return a;
      b = _eval(bus, u3t(arg));
      if ( b != u3_none ) {
        u3_noun same = u3r_sing(a, b);
        u3z(b);
        b = same;
      }
      u3z(a);
      return b;
    case 6:
      if ( (a = _eval(bus, u3h(arg))) == u3_none ) return a;
      if ( a <= 1 ) return _eval(bus, a ? u3t(u3t(arg)) : u3h(u3t(arg)));
      u3z(a);
      return u3_none;
    case 7:
    case 8:
      if ( (a = _eval(bus, u3h(arg))) == u3_none ) return a;
      if ( op == 8 ) a = u3nc(a, u3k(bus));
      b = _eval(a, u3t(arg));
      u3z(a);
      return b;
    case 10: {
      u3_noun spec = u3h(arg), axis = u3h(spec);
      if ( (a = _eval(bus, u3t(arg))) == u3_none ) return a;
      b = _eval(bus, u3t(spec));
      u3_weak out = u3_none;
      if ( b != u3_none ) {
        if ( axis ) out = _ref_edit(a, axis, u3r_met(0, axis) - 1, b);
        u3z(b);
      }
      u3z(a);
      return out;
    }
    case 11: // All generated hints are %memo with a literal clue.
      return _eval(bus, u3t(arg));
    default: abort();
  }
}

static u3_noun
_memo(u3_noun fol)
{
  return u3nt(11, u3nc(c3__memo, _lit(0)), fol);
}

static u3_noun
_chain(c3_w count, c3_t guard, c3_w axis)
{
  u3_noun rest = _lit(999);
  while ( count ) {
    --count;
    u3_noun val = _lit(_tag(count));
    u3_noun test = count & 1 ? u3nt(5, val, _slot(axis)) : u3nt(5, _slot(axis), val);
    if ( guard ) test = u3nq(6, u3nc(3, _slot(2)), test, _lit(1));
    rest = u3nq(6, test, u3nc(_lit(count), _slot(1)), rest);
  }
  return rest;
}

static u3_noun
_formula(c3_w depth)
{
  if ( !depth ) return (_rand() & 1) ? _slot(_rand() % 9) : _lit(_tree(1));
  unsigned op = _rand() % 12;
  if ( op == 0 ) return _chain(3 + _rand() % 7, depth & 1, 4);
  u3_noun a = _formula(depth - 1);
  switch ( op ) {
    case 1: return _memo(a);
    case 2: return u3nc(3, a);
    case 3: return u3nc(4, a);
    case 4: return u3nc(a, _formula(depth - 1));
    case 5: return u3nt(5, a, _formula(depth - 1));
    case 6: return u3nt(7, a, _formula(depth - 1));
    case 7: return u3nt(8, a, _formula(depth - 1));
    case 8: return u3nq(6, a, _slot(1), _slot(0));
    default: {
      u3_noun test = u3nc(3, _slot(1 + _rand() % 8));
      return u3nq(6, test, a, _formula(depth - 1));
    }
  }
}

static u3_noun
_burn_twice(u3_noun pair)
{
  u3p(u3n_prog) prog = u3n_find(0, u3t(pair));
  u3_noun cold = u3n_burn(prog, u3k(u3h(pair)));
  u3_noun warm = u3n_burn(prog, u3k(u3h(pair)));
  u3_assert(c3y == u3r_sing(cold, warm));
  // The nested road grows in the opposite direction. Re-enter through find
  // so jet and memo sites belong to the active road.
  u3m_hate(0);
  prog = u3n_find(0, u3t(pair));
  u3_noun nested = u3m_love(u3n_burn(prog, u3k(u3h(pair))));
  u3_assert(c3y == u3r_sing(cold, nested));
  u3z(cold);
  u3z(warm);
  return nested;
}

static c3_t
_matches(u3_weak expected, u3_noun result)
{
  if ( expected == u3_none ) return u3h(result) == c3__exit;
  return u3h(result) == 0 && c3y == u3r_sing(expected, u3t(result));
}

static void
_check(u3_noun bus, u3_noun fol)
{
  u3_weak ref = _eval(bus, fol);
  u3_noun pair = u3nc(bus, fol);
  u3_noun gon = u3m_soft(0, _burn_twice, u3k(pair));
  ++cases;
  if ( u3_none == ref ) ++exits;
  if ( !_matches(ref, gon) ) {
    fprintf(stderr, "nock-compiler-test case %u seed %08x\n", cases, seed);
    u3m_p("input", pair);
    u3m_p("actual", gon);
    if ( ref != u3_none ) u3m_p("expected", ref);
    abort();
  }
  if ( ref != u3_none ) u3z(ref);
  u3z(pair);
  u3z(gon);
}

// A shared subtree at each level reaches every axis with modest allocation.
static u3_noun
_full(c3_w depth)
{
  u3_noun tree = 42;
  while ( depth-- ) tree = u3nc(u3k(tree), tree);
  return tree;
}

// Distinct leaves make swapped head/tail traversals visible to the oracle.
static u3_noun
_numbered(c3_w depth, c3_w axis)
{
  if ( !depth ) return axis;
  return u3nc(_numbered(depth - 1, axis * 2), _numbered(depth - 1, axis * 2 + 1));
}

/* A different leaf value at every axis distinguishes the two directions.
** Exercise both the consuming slot and a retained subject in autocons.
*/
static void
_check_short_axes(void)
{
  u3_noun tree = _numbered(15, 1);
  unsigned invalid = 0;
  for ( c3_w axis = 1; axis <= 65535; ++axis ) {
    c3_w bit = 1;
    while ( bit <= (axis >> 1) ) bit <<= 1;
    u3_noun expected = tree;
    for ( c3_w step = bit >> 1; step; step >>= 1 ) {
      expected = (axis & step) ? u3t(expected) : u3h(expected);
    }
    u3_noun formula = _slot(axis);
    u3_noun result = u3n_nock_on(u3k(tree), formula);
    u3_assert(result == expected);
    u3z(result);
    // An autocons keeps the subject while evaluating its first formula.
    result = u3n_nock_on(u3k(tree), u3nc(_slot(axis), _lit(9)));
    u3_assert(u3h(result) == expected && u3t(result) == 9);
    u3z(result);
    // A truncated tree fails at each successive hop on specialized axes.
    c3_w prefix = axis >> 4;
    if ( !(prefix & (prefix + 1)) ) {
      for ( c3_w depth = 0, step = bit; step > 1; ++depth, step >>= 1 ) {
        _check(_full(depth), _slot(axis));
        ++invalid;
      }
    }
    if ( (axis & 1023) == 1023 ) u3n_reclaim();
  }
  u3z(tree);
  fprintf(stderr, "short-axis check: 65535 axes, 131070 products, %u expected exits\n", invalid);
}

static c3_w logged[8], logs;

static void
_log(u3_noun msg)
{
  u3_assert(logs < 8);
  logged[logs++] = msg;
  u3z(msg);
}

static void
_effects(void)
{
  u3_noun fol = _slot(0);
  for ( c3_w i = 5; i > 0; --i ) {
    u3_noun test = u3nt(5, _slot(2), _lit(i));
    u3_noun report = u3nt(11, u3nc(c3__slog, _lit(i)), _slot(3));
    fol = u3nq(6, test, report, fol);
  }
  // The winning arm runs on each evaluation. The failing default is lazy.
  u3C.slog_f = _log;
  u3_noun gon = u3m_soft(0, _burn_twice, u3nc(u3nc(4, 123), u3k(fol)));
  u3_assert(u3h(gon) == 0 && u3t(gon) == 123 && logs == 3);
  for ( c3_w i = 0; i < logs; ++i ) {
    u3_assert(logged[i] == 4);
  }
  u3z(gon);
  gon = u3m_soft(0, _burn_twice, u3nc(u3nc(6, 123), fol));
  u3_assert(u3h(gon) == c3__exit && logs == 3);
  u3z(gon);
  u3C.slog_f = 0;

  fol = _lit(0);
  for ( c3_w i = 6; i > 0; --i ) {
    u3_noun test = u3nt(5, _slot(2), _lit(i));
    u3_noun arm = u3nt(11, u3nc(c3__spot, _lit(i)), _slot(0));
    fol = u3nq(6, test, arm, fol);
  }
  gon = u3m_soft_run(0, u3n_nock_on, u3nc(3, 0), fol);
  u3_noun expected = u3nc(2, u3nc(u3nc(c3__spot, 3), 0));
  if ( c3n == u3r_sing(expected, gon) ) {
    u3m_p("expected trace", expected);
    u3m_p("actual trace", gon);
    abort();
  }
  u3z(expected);
  u3z(gon);
  // A Nock hint spelling the internal tag has ordinary user-hint semantics.
  _check(123, u3nt(11, c3_s4('j','m','p','s'), _slot(1)));
}

static void
_indexed(void)
{
  // Duplicate tags, hash collisions, non-direct subjects, failed guards,
  // wide skips and literal-pool indexes. Each arm has a distinct product.
  u3_noun fol = _lit(54321);
  for ( c3_w i = 120; i-- > 0; ) {
    u3_noun test = u3nt(5, _slot(4), _lit(0x120000 + 97 * (i % 101)));
    test = u3nq(6, u3nc(3, _slot(2)), test, _lit(1));
    u3_noun arm = u3nc(_lit(i + 80000), u3nc(_lit(i + 90000), _slot(3)));
    fol = u3nq(6, test, arm, fol);
  }
  fol = u3nc(_memo(fol), _slot(2));
  for ( c3_w i = 0; i < 110; ++i ) {
    _check(u3nc(u3nc(0x120000 + 97*i, i), 0), u3k(fol));
  }
  _check(u3nc(u3nc(u3nc(2, 3), 4), 5), u3k(fol));
  _check(u3nc(u3nc(u3i_string("indirect atom"), 0), 0), u3k(fol));
  _check(u3nc(0, 5), u3k(fol));
  _check(0, fol);
}

// Each pattern is either a literal on the subject fragment or a literal on
// its head guarded by cellhood. Reverse operands to cover both Nock shapes.
static u3_noun
_mixed_test(c3_w base, c3_w axis, c3_t cell, c3_t reverse, u3_noun key)
{
  u3_noun lhs = _slot(axis), rhs = _lit(key);
  u3_noun eq = reverse ? u3nt(5, rhs, lhs) : u3nt(5, lhs, rhs);
  return cell ? u3nq(6, u3nc(3, _slot(base)), eq, _lit(1)) : eq;
}

static u3_noun
_at_axis(c3_w axis, u3_noun val)
{
  while ( axis > 1 ) {
    val = (axis & 1) ? u3nc(axis, val) : u3nc(val, axis);
    axis >>= 1;
  }
  return val;
}

// Retained subjects, consuming slots, overlapping reads, and a failing read
// on either arm. The reference evaluator walks each axis independently.
static void
_projection_cases(void)
{
  const c3_h axes[] = {4, 6, 14, 126, 255, 256, 1023, 16382};
  u3_noun values[] = {0, 1, 42, 256, 65535, 65536, u3i_chub(1ULL << 56),
                      u3i_string("an indirect atom on either loom"), u3nc(42, 42)};
  for ( size_t i = 0; i < sizeof(axes)/sizeof(*axes); ++i ) {
    c3_h base = axes[i];
    u3_noun subject = _at_axis(base, _numbered(2, 1));
    for ( c3_h first = 0; first < 3; ++first ) {
      c3_h axis = first ? 2 * base + first - 1 : base;
      c3_h arms[] = {base, 2 * base, 2 * base + 1, 4 * base + 2, 3, 0};
      for ( size_t j = 0; j < sizeof(arms)/sizeof(*arms); ++j ) {
        c3_h field = arms[j];
        u3_noun arm = _slot(field);
        u3_noun pair = u3nc(_slot(axis), u3k(arm));
        _check(u3k(subject), u3k(pair));
        _check(u3k(subject), u3nc(pair, _slot(1)));
        for ( size_t k = 0; k <= sizeof(values)/sizeof(*values); ++k ) {
          u3_noun test = k ? u3nt(5, _slot(axis), _lit(u3k(values[k - 1])))
                           : u3nc(3, _slot(axis));
          u3_noun nested = u3nq(6, u3nc(3, _slot(base)), u3k(arm), _slot(2));
          u3_noun fol = u3nq(6, test, nested, u3nc(u3k(arm), _slot(1)));
          _check(u3k(subject), u3k(fol));
          _check(_at_axis(axis, k ? u3k(values[k - 1]) : 0), u3k(fol));
          _check(0, fol);
        }
        u3z(arm);
      }
    }
    u3z(subject);
  }
  // Both edges into a wide conditional are reachable. One arm starts
  // with a retained slot and the other with a consuming slot.
  for ( unsigned depth = 8; depth <= 14; depth += 6 ) {
    u3_noun padding = _lit(1024);
    for ( unsigned i = 0; i < depth; ++i ) padding = u3nc(u3k(padding), padding);
    u3_noun test = u3nt(5, _slot(14), _lit(0));
    u3_noun fol = u3nq(6, test, u3nc(_slot(15), padding), _slot(7));
    _check(_at_axis(14, 0), u3k(fol));
    _check(_at_axis(14, 1), fol);
  }
  // The trailing literal pool puts the equality operand above index 255.
  u3_noun padding = _lit(1000000);
  for ( unsigned i = 0; i < 9; ++i ) padding = u3nc(u3k(padding), padding);
  u3_noun test = u3nt(5, _slot(14), _lit(u3k(values[7])));
  u3_noun fol = u3nc(u3nq(6, test, _slot(15), _slot(7)), padding);
  _check(_at_axis(14, u3i_string("an indirect atom on either loom")), u3k(fol));
  _check(_at_axis(14, u3nc(42, 42)), fol);
  for ( size_t i = 0; i < sizeof(values)/sizeof(*values); ++i ) u3z(values[i]);
}

static void
_mixed(void)
{
  const c3_w axes[] = {1, 3, 14, 126, 510};
  for ( unsigned layout = 0; layout < 6; ++layout ) {
    for ( unsigned a = 0; a < sizeof(axes)/sizeof(*axes); ++a ) {
      c3_w axis = axes[a];
      u3_noun fol = (layout & 1) ? _slot(0) : _lit(777);
      for ( unsigned i = 24; i-- > 0; ) {
        c3_t cell = (layout / 2 == 0) ? (i < 12)
                  : (layout / 2 == 1) ? (i >= 12) : (i & 1);
        c3_w key = 90000 + ((i / (layout / 2 == 2 ? 2 : 1)) % 9);
        u3_noun eq = _mixed_test(axis, cell ? 2*axis : axis, cell, i & 2, key);
        fol = u3nq(6, eq, u3nc(_lit(i), _slot(1)), fol);
      }
      if ( layout & 1 ) fol = u3nc(_memo(fol), _slot(1));
      for ( unsigned i = 0; i <= 10; ++i ) {
        _check(_at_axis(axis, 90000 + i), u3k(fol));
        _check(_at_axis(axis, u3nc(90000 + i, _tag(i))), u3k(fol));
      }
      _check(_at_axis(axis, u3nc(u3nc(90000, 0), 7)), u3k(fol));
      _check(_at_axis(axis, u3i_chub(0xabc00000000ULL)), u3k(fol));
      _check(0, fol);
    }
  }

  // A different guard, a deeper projection and a literal cell form barriers.
  // Each can change the selected arm or the point at which slot evaluation bails.
  for ( unsigned kind = 0; kind < 4; ++kind ) {
    u3_noun barrier = kind == 0 ? _mixed_test(3, 4, 1, 0, 22)
                    : kind == 1 ? _mixed_test(2, 8, 1, 1, 22)
                    : kind == 2 ? _mixed_test(2, 2, 0, 0, u3nc(22, 10))
                                : _mixed_test(2, 2, 0, 1, u3i_chub(1ULL << 34));
    u3_noun fol = u3nq(6, _mixed_test(2, 4, 1, 0, 22), _lit(5), _slot(0));
    fol = u3nq(6, barrier, _lit(4), fol);
    fol = u3nq(6, _mixed_test(2, 4, 1, 1, 11), _lit(3), fol);
    fol = u3nq(6, _mixed_test(2, 2, 0, 0, 11), _lit(2), fol);
    fol = u3nq(6, _mixed_test(2, 2, 0, 0, 33), _lit(1), fol);
    _check(u3nc(11, 0), u3k(fol));
    _check(u3nc(7, u3nc(2, 3)), u3k(fol));
    _check(u3nc(u3nc(22, 10), 0), u3k(fol));
    _check(u3nc(u3nc(u3nc(22, 9), 8), u3nc(2, 3)), u3k(fol));
    _check(u3nc(u3i_chub(1ULL << 34), 0), fol);
  }

  // At each table/axis boundary, use identical atom and cell-head keys.
  // The largest key occupies all 32 hash-key bits in the cell partition.
  const c3_w lengths[] = {3, 4, 127, 128, 129};
  for ( unsigned j = 0; j < sizeof(lengths)/sizeof(*lengths); ++j ) {
    for ( unsigned a = 0; a < 2; ++a ) {
      c3_w axis = 65535 + a;
      u3_noun fol = _lit(999);
      for ( unsigned i = lengths[j]; i-- > 0; ) {
        c3_t cell = i & 1;
        c3_w key = 0x7fffffff - i / 2;
        u3_noun test = _mixed_test(axis, cell ? 2*axis : axis, cell, i & 2, key);
        fol = u3nq(6, test, _lit(i), fol);
      }
      for ( unsigned i = 0; i < lengths[j] + 2; ++i ) {
        u3_noun key = 0x7fffffff - i / 2;
        if ( i & 1 ) key = u3nc(key, i);
        _check(_at_axis(axis, key), u3k(fol));
      }
      u3z(fol);
    }
  }

  // A large arm makes the shape split use a four-byte byte displacement
  // while the instruction count still fits the table builder.
  u3_noun wide = _lit(90000);
  for ( unsigned i = 0; i < 14; ++i ) wide = u3nc(u3k(wide), wide);
  u3_noun chain = u3nq(6, _mixed_test(2, 4, 1, 1, 4), wide, _lit(0));
  chain = u3nq(6, _mixed_test(2, 4, 1, 0, 3), _lit(33), chain);
  chain = u3nq(6, _mixed_test(2, 2, 0, 1, 2), _lit(22), chain);
  _check(u3nc(2, 0), u3k(chain));
  _check(u3nc(u3nc(3, 0), 0), u3k(chain));
  _check(u3nc(u3nc(4, 0), 0), chain);

  // A mixed switch in a program whose existing literals fill the pool.
  u3_noun big = _lit(1000000), small = _lit(42);
  for ( unsigned i = 0; i < 16; ++i ) big = u3nc(u3k(big), big);
  for ( unsigned i = 0; i < 12; ++i ) {
    c3_t cell = i & 1;
    small = u3nq(6, _mixed_test(3, cell ? 6 : 3, cell, 0, i), _lit(i + 1), small);
  }
  _check(u3nc(1, u3nc(7, 8)), u3nq(6, _slot(2), big, small));
}

static void
_mixed_effects(void)
{
  u3_noun fol = _slot(0);
  for ( unsigned i = 6; i-- > 0; ) {
    c3_t cell = i & 1;
    u3_noun eq = _mixed_test(2, cell ? 4 : 2, cell, 0, (i / 2) % 2);
    u3_noun arm = u3nt(11, u3nc(c3__slog, _lit(40+i)), _slot(3));
    fol = u3nq(6, eq, arm, fol);
  }
  logs = 0;
  u3C.slog_f = _log;
  u3_noun gon = u3m_soft(0, _burn_twice, u3nc(u3nc(u3nc(0, 3), 55), u3k(fol)));
  u3_assert(u3h(gon) == 0 && u3t(gon) == 55 && logs == 3);
  for ( unsigned i = 0; i < 3; ++i ) {
    u3_assert(logged[i] == 41);
  }
  u3z(gon);
  gon = u3m_soft(0, _burn_twice, u3nc(u3nc(7, 55), fol));
  u3_assert(u3h(gon) == c3__exit && logs == 3);
  u3z(gon);
  u3C.slog_f = 0;

  fol = _lit(123);
  for ( unsigned i = 8; i-- > 0; ) {
    c3_t cell = i & 1;
    u3_noun test = _mixed_test(2, cell ? 4 : 2, cell, 0, i / 2);
    u3_noun arm = u3nt(11, u3nc(c3__spot, _lit(i)), _slot(0));
    fol = u3nq(6, test, arm, fol);
  }
  gon = u3m_soft_run(0, u3n_nock_on, u3nc(u3nc(3, 0), 9), fol);
  u3_noun expected = u3nc(2, u3nc(u3nc(c3__spot, 7), 0));
  u3_assert(c3y == u3r_sing(gon, expected));
  u3z(gon);
  u3z(expected);
  _check(42, u3nt(11, c3_s4('m','x','d','s'), _slot(1)));
}

static void
_full_literal_pool(void)
{
  // The large branch consumes every literal index. The small branch still
  // dispatches when the compiler has no spare slot for its hash table.
  u3_noun big = _lit(1000000);
  for ( c3_w i = 0; i < 16; ++i ) big = u3nc(u3k(big), big);
  u3_noun arms = _lit(99);
  for ( c3_w i = 4; i > 0; --i ) {
    arms = u3nq(6, u3nt(5, _slot(3), _lit(i)), _lit(i + 10), arms);
  }
  _check(u3nc(1, 3), u3nq(6, _slot(2), big, arms));
}

static void
_project_pair(void)
{
  for ( c3_w axis = 2; axis <= 1024; ++axis ) {
    u3_noun body = u3nc(_slot(2), _slot(axis));
    u3_noun fol = u3nt(8, _slot(3), body);
    if ( axis & 1 ) fol = u3nc(fol, _slot(1));
    _check(_numbered(5, 1), fol);
  }
}

static u3_noun (*const _list_jets[])(u3_noun, u3_noun) = {
  u3qb_turn, u3qb_skim, u3qb_skip, u3qb_murn
};

static u3_noun
_list_call(u3_noun args)
{
  u3_noun kind, list, gate;
  u3x_trel(args, &kind, &list, &gate);
  return _list_jets[kind](list, gate);
}

/* Each input element is [loobean payload]. Its bit in the mask determines
 * whether the jet keeps it, drops it, or replaces it. */
static void
_list_masks(void)
{
  for ( unsigned kind = 0; kind < 4; kind++ ) {
    u3_noun fol = _slot(12);
    if ( kind == 0 ) fol = u3nq(6, fol, _slot(6), _lit(42));
    if ( kind == 3 ) fol = u3nq(6, fol, u3nc(_lit(0), _slot(6)), _lit(0));
    u3_noun gate = u3nt(fol, 0, 37);
    u3_assert(_list_jets[kind](0, 99) == 0);
    for ( unsigned mask = 0; mask < 256; mask++ ) {
      u3_noun list = 0, want = 0, suffix = 0;
      unsigned copied = 0;
      for ( unsigned i = 8; i-- > 0; ) {
        unsigned bit = (mask >> i) & 1;
        u3_noun item = u3nc(bit, u3i_chub(0xffffffffULL + i));
        list = u3nc(item, list);
        if ( kind == 2 ? bit : !bit ) {
          want = u3nc(u3k(item), want);
          if ( u3t(list) == suffix ) suffix = list;
          else copied++;
        }
        else if ( kind == 0 ) {
          want = u3nc(42, want);
          copied++;
        }
      }
      u3_noun out = _list_jets[kind](list, gate), tail = out;
      for ( unsigned i = 0; i < copied; i++ ) tail = u3t(tail);
      u3_assert(tail == suffix);
      u3z(list);
      u3_assert(c3y == u3r_sing(out, want));
      u3z(out);
      u3z(want);
    }
    u3z(gate);
  }
}

static void
_list_failure(unsigned kind, u3_noun list, u3_noun fol, unsigned log_count)
{
  logs = 0;
  u3C.slog_f = _log;
  u3_noun got = u3m_soft(0, _list_call, u3nt(kind, list, u3nt(fol, 0, 37)));
  u3C.slog_f = 0;
  u3_assert(u3h(got) == c3__exit && logs == log_count);
  for ( unsigned i = 0; i < logs; ++i ) {
    u3_assert(logged[i] == i + 1);
  }
  u3z(got);
}

static void
_test_list_weld(void)
{
  for ( c3_w len = 8192; len <= 8193; len++ ) {
    u3_noun list = 0, item = u3nc(23, 45), tail = u3nc(56, 78);
    for ( c3_w i = 0; i < len; i++ ) list = u3nc(u3k(item), list);
    u3_noun product = u3qb_weld(list, tail), node = product;
    u3z(list);
    u3_assert(u3a_use(item) == len + 1 && u3a_use(tail) == 2);
    for ( c3_w i = 0; i < len; i++, node = u3t(node) ) {
      u3_assert(u3h(node) == item);
    }
    u3_assert(node == tail);
    u3z(product);
    u3_assert(u3a_use(item) == 1 && u3a_use(tail) == 1);
    u3z(tail);
    u3z(item);
  }
}

static void
_list_guards(void)
{
  for ( unsigned kind = 0; kind < 4; kind++ ) {
    u3_noun val = (kind == 3) ? u3nc(0, 7) : 0;
    _list_failure(kind, u3nt(1, 2, 9), _lit(u3k(val)), 0);
    _list_failure(kind, u3nt(1, 2, 9), u3nt(11, u3nc(c3__slog, _slot(6)), _lit(val)), 2);
  }
  _list_failure(1, u3nc(0, 0), _lit(2), 0);
  _list_failure(2, u3nc(0, 0), _lit(u3nc(0, 0)), 0);
  _list_failure(3, u3nc(0, 0), _lit(2), 0);
  _list_failure(0, u3nc(0, 0), _slot(12), 0);

  u3j_site site;
  u3_noun equal = u3nt(5, _slot(12), _slot(13));
  u3j_gate_prep(&site, u3nt(u3nq(6, equal, _slot(6), _lit(42)), 0, 37));
  u3_assert(c3n == u3j_gate_read_only(&site));
  u3_noun item = u3nc(u3nc(5, 6), u3nc(5, 6));
  u3_noun list = u3nc(u3k(item), 0);
  u3_noun out = u3qb_turn(list, site.bat);
  u3z(list);
  u3_assert(u3t(out) == 0 && c3y == u3r_sing(u3h(out), item));
  u3z(item);
  u3z(out);
  u3j_gate_lose(&site);

  u3j_gate_prep(&site, u3nt(u3nt(5, _slot(6), _lit(u3i_chub(0xffffffffULL))), 0, 37));
  u3_assert(c3y == u3j_gate_read_only(&site));
  u3j_gate_lose(&site);
}

/* Compare a prepared gate with the oracle on [battery [sample context]].
 * Reuse the prepared site for two calls to exercise sample ownership. */
static u3_noun
_prepared_twice(u3_noun input)
{
  u3_noun gate, sample, specialized;
  u3x_trel(input, &gate, &sample, &specialized);
  u3j_site site;
  u3j_gate_prep(&site, u3k(gate));
  u3_assert(!!site.sample_y == specialized);
  u3_noun a = u3j_gate_slam(&site, u3k(sample));
  u3_noun b = u3j_gate_slam(&site, u3k(sample));
  u3_assert(c3y == u3r_sing(a, b));
  u3z(a);
  u3j_gate_lose(&site);
  return b;
}

static void
_check_prepared(u3_noun formula, u3_noun sample, c3_w specialized)
{
  u3_noun subject = u3nt(u3k(formula), u3k(sample), 37);
  u3_weak expected = _eval(subject, formula);
  u3_noun input = u3nt(u3nt(formula, 0, 37), sample, specialized);
  u3_noun result = u3m_soft(0, _prepared_twice, input);
  u3_assert(_matches(expected, result));
  if ( expected != u3_none ) u3z(expected);
  u3z(result);
  u3z(subject);
}

static u3_noun
_sample_expression(c3_w depth, c3_h key)
{
  if ( !depth ) {
    const c3_w slots[] = {6, 12, 13, 24, 27};
    return key & 1 ? _slot(slots[(key >> 1) % 5]) : _lit(key % 19);
  }
  c3_h left = key * 1664525U + 1013904223U;
  c3_h right = (left ^ (left >> 16)) * 2246822519U;
  u3_noun a = _sample_expression(depth - 1, left);
  switch ( key % 5 ) {
    case 0: return u3nc(3, a);
    case 1: return u3nc(4, a);
    case 2: return u3nt(5, a, _sample_expression(depth - 1, right));
    case 3: return u3nc(a, _sample_expression(depth - 1, right));
    default: return u3nq(6, u3nc(3, _slot(6)), a, _sample_expression(depth - 1, right));
  }
}

/* Evaluate each gate on the full core, then apply the list operation to
 * those products. No prepared site or optimized list jet enters the oracle. */
static u3_weak
_list_reference(unsigned kind, u3_noun list, u3_noun formula)
{
  if ( list == 0 ) return 0;
  u3_noun item = u3h(list), core = u3nt(u3k(formula), u3k(item), 37);
  u3_weak val = _eval(core, formula);
  u3z(core);
  if ( val == u3_none ) return u3_none;
  if ( kind == 1 || kind == 2 ) {
    if ( val > 1 ) { u3z(val); return u3_none; }
    val = (val == (kind == 1 ? c3y : c3n)) ? u3k(item) : u3_none;
  }
  else if ( kind == 3 ) {
    u3_noun unit = val;
    if ( unit && c3n == u3du(unit) ) { u3z(unit); return u3_none; }
    val = unit ? u3k(u3t(unit)) : u3_none;
    u3z(unit);
  }
  u3_weak tail = _list_reference(kind, u3t(list), formula);
  if ( val == u3_none ) return tail;
  if ( tail == u3_none ) { u3z(val); return tail; }
  return u3nc(val, tail);
}

static void
_list_expressions(void)
{
  u3_noun list = u3nq(u3nc(0, 1), u3nc(1, 0), 0,
                      u3nc(u3i_chub(0xffffffffULL), 0));
  for ( unsigned i = 0; i < 256; i++ ) {
    u3_noun fol = _sample_expression(3, i);
    for ( unsigned kind = 0; kind < 4; kind++ ) {
      u3_weak want = _list_reference(kind, list, fol);
      u3_noun args = u3nt(kind, u3k(list), u3nt(u3k(fol), 0, 37));
      u3_noun got = u3m_soft(0, _list_call, args);
      u3_assert(_matches(want, got));
      if ( want != u3_none ) u3z(want);
      u3z(got);
    }
    u3z(fol);
  }
  u3z(list);
}

static void
_prepared_cases(void)
{
  u3_noun tree = _numbered(5, 1);
  for ( c3_w axis = 1; axis < 128; ++axis ) {
    c3_w root = 1;
    while ( root <= axis / 2 ) root <<= 1;
    u3_noun slot = _slot(6 * root + axis - root);
    _check_prepared(u3k(slot), u3k(tree), 1);
    _check_prepared(u3nc(slot, _slot(6)), u3k(tree), 1);
  }
  for ( c3_h i = 0; i < 256; ++i ) {
    u3_noun expression = _sample_expression(3, i);
    _check_prepared(u3k(expression), u3k(tree), 1);
    _check_prepared(expression, i, 1);
  }
  _check_prepared(_slot(0), u3k(tree), 0);
  _check_prepared(_slot(1), u3k(tree), 0);
  _check_prepared(_slot(3), u3k(tree), 0);
  _check_prepared(_slot(7), u3k(tree), 0);
  _check_prepared(_lit(u3k(tree)), u3k(tree), 1);
  _check_prepared(u3nc(4, _slot(6)), u3k(tree), 1);
  _check_prepared(u3nc(4, _slot(6)), u3i_chub(0xffffffffULL), 1);
  _check_prepared(u3nq(6, _slot(6), _lit(7), _lit(8)), 19, 1);
  u3_noun cond = u3nq(6, u3nc(3, _slot(6)), _slot(12), _slot(6));
  _check_prepared(u3k(cond), u3k(tree), 1);
  _check_prepared(cond, 41, 1);
  _check_prepared(u3nt(5, _slot(12), _slot(13)), u3nc(u3k(tree), u3k(tree)), 1);
  _check_prepared(u3nt(7, _slot(6), _slot(1)), u3k(tree), 0);
  _check_prepared(u3nt(5, _slot(6), _slot(7)), 37, 0);
  _check_prepared(u3nq(6, _slot(6), _lit(3), _slot(7)), 1, 0);

  u3_noun wide = _slot(6);
  for ( unsigned i = 0; i < 5; ++i ) wide = u3nc(u3k(wide), wide);
  _check_prepared(wide, 9, 0);

  // A 43-bit slot is direct on a 64-bit loom and indirect on a 32-bit loom.
  // Its suffix selects the final leaf after forty rightward steps.
  u3_noun deep = 79;
  for ( unsigned i = 0; i < 40; ++i ) deep = u3nc(0, deep);
  _check_prepared(_slot(u3i_chub((7ULL << 40) - 1)), deep, sizeof(u3_noun) == 8);

  logs = 0;
  u3C.slog_f = _log;
  _check_prepared(u3nt(11, u3nc(c3__slog, _lit(71)), _slot(6)), 19, 0);
  u3C.slog_f = 0;
  u3_assert(logs == 2 && logged[0] == 71 && logged[1] == 71);
  u3z(tree);
}

// A projection consumes a newly allocated sample and returns an owned child.
// Both references to that child in the sample must disappear when it does.
static void
_prepared_ownership(void)
{
  u3j_site site;
  u3j_gate_prep(&site, u3nt(_slot(12), 0, 37));
  u3_noun child = u3nc(9, 8);
  u3_noun result = u3j_gate_slam(&site, u3nc(u3k(child), u3k(child)));
  u3_assert(result == child && u3a_use(child) == 2);
  u3z(result);
  u3_assert(u3a_use(child) == 1);
  u3z(child);
  u3j_gate_lose(&site);
}

#if defined(U3_CPU_DEBUG)
static void
_prepared_profile(void)
{
  u3j_site site;
  u3j_gate_prep(&site, u3nt(_slot(6), 0, 37));
  const c3_w flags[] = {0, u3o_trace, u3o_debug_cpu, 0};
  c3_w saved = u3C.wag_h;
  for ( unsigned i = 0; i < 4; ++i ) {
    u3C.wag_h = saved | flags[i];
    u3_assert(u3j_gate_read_only(&site) == __(flags[i] == 0));
    c3_d before = u3R->pro.cel_d;
    u3_assert(42 == u3j_gate_slam(&site, 42));
    u3_assert(u3R->pro.cel_d - before == (flags[i] ? 2 : 0));
  }
  u3C.wag_h = saved;
  u3j_gate_lose(&site);
}
#endif

static u3_noun
_borrow_formula(c3_w op, c3_w base, c3_w left, c3_w right)
{
  if ( !op ) return u3nt(5, _slot(base), _slot(left));
  u3_noun fields = _slot(left);
  if ( right ) fields = u3nc(fields, _slot(right));
  return op == 1 ? u3nt(7, _slot(base), fields)
                 : u3nt(10, u3nc(op, fields), _slot(base));
}

static void
_borrow_cases(void)
{
  const c3_w kinds[] = {0, 1, 2, 3, 6};
  u3_noun tree = _numbered(4, 1);
  for ( unsigned k = 0; k < 5; ++k ) {
    for ( unsigned base = 1; base <= 8; ++base ) {
      for ( unsigned left = 1; left <= 8; ++left ) {
        unsigned right = (base + left) % 5;
        u3_noun f = _borrow_formula(kinds[k], base, left, right);
        _check(u3k(tree), u3k(f));
        _check(u3nc(3, u3nc(4, 5)), u3k(f));
        _check(u3k(tree), u3nc(f, _slot(1)));
      }
    }
  }
  u3z(tree);
  for ( c3_w axis = 65535; axis <= 65536; ++axis ) {
    _check(_at_axis(axis, u3nc(3, 4)), _borrow_formula(1, axis, 2, 3));
    _check(_at_axis(axis, 7), _borrow_formula(0, axis, axis, 0));
  }
  // An equality can unify equal children before a later mismatch.
  u3_noun a = u3nc(u3nc(9, 8), 7), b = u3nc(u3nc(9, 8), 6);
  _check(u3nc(a, b), _borrow_formula(0, 2, 3, 0));
  _check(u3nc(u3nc(1, 2), u3i_chub(1ULL << 40)), _borrow_formula(0, 1, 2, 0));
  _check(42, u3nt(11, c3_s4('b','r','o','w'), _slot(1)));
  u3_noun big = _lit(80000);
  for ( unsigned i = 0; i < 16; ++i ) big = u3nc(u3k(big), big);
  _check(u3nc(1, 7), u3nq(6, _slot(2), big, _borrow_formula(0, 2, 3, 0)));
}

// Both subject and its aliased child belong to this road. The VM consumes
// the only reference to the parent. Every child reference escaping into
// a product must be released when that product is freed.
static void
_borrow_ownership(void)
{
  const c3_w kinds[] = {0, 1, 2, 3, 6};
  for ( unsigned i = 0; i < 5; ++i ) {
    u3_noun leaf = u3nc(12, u3i_chub(1ULL << 40));
    u3_noun left = u3nt(u3k(leaf), u3k(leaf), u3k(leaf));
    u3_noun right = u3nt(u3k(leaf), u3k(leaf), u3k(leaf));
    u3_noun bus = u3nc(left, right);
    u3_noun fol = _borrow_formula(kinds[i], 2, 2, 3);
    u3p(u3n_prog) prog = u3n_find(0, fol);
    u3_noun product = u3n_burn(prog, bus);
    u3_assert(i == 0 ? product == c3y : c3y == u3du(product));
    u3z(product);
    u3_assert(u3a_use(leaf) == 1);
    u3z(leaf);
    u3z(fol);
  }
}

// Warm the old mug and preserve external references independently. The
// replacement alternates between a literal and the complete original core.
static void
_reuse_cases(void)
{
  const c3_w axes[] = {2, 3, 6};
  for ( unsigned k = 0; k < 3; ++k ) {
    for ( unsigned mode = 0; mode < 8; ++mode ) {
      u3_noun bus = u3nt(11, 22, 33), other = u3nt(11, 22, 33);
      u3_noun root = (mode & 1) ? u3k(bus) : u3_none;
      u3_noun payload = (mode & 2) ? u3k(u3t(bus)) : u3_none;
      u3_noun rhs = (mode & 4) ? _slot(1) : _lit(987);
      u3_noun fol = u3nt(10, u3nc(axes[k], rhs), _slot(1));
      u3_noun expected = _eval(other, fol);
      u3p(u3n_prog) program = u3n_find(0, fol);
      u3r_mug(bus);
      u3r_mug(u3t(bus));
      u3_noun result = u3n_burn(program, bus);
      u3_assert(u3r_mug(expected) == u3r_mug(result));
      u3_assert(c3y == u3r_sing(result, expected));
      if ( root != u3_none ) {
        u3_assert(c3y == u3r_sing(root, other));
        u3z(root);
      }
      if ( payload != u3_none ) {
        u3_assert(c3y == u3r_sing(payload, u3t(other)));
        u3z(payload);
      }
      u3z(other); u3z(expected); u3z(result); u3z(fol);
    }
  }
}

static void
_owner_cases(void)
{
  for ( unsigned axis = 1; axis <= 16; ++axis ) {
    u3_noun subject = _numbered(3, 1);
    u3_noun right = u3nt(7, _slot(axis), _slot(1));
    u3_noun reads[] = {
      u3nc(_slot(1), u3k(right)),
      u3nt(5, _slot(1), u3k(right)),
      u3nt(2, _slot(1), _lit(u3k(right))),
      u3nt(10, u3nc(6, _memo(right)), _slot(1)),
    };
    for ( unsigned j = 0; j < 4; ++j ) {
      _check(u3k(subject), u3k(reads[j]));
      _check(u3k(subject), u3nc(reads[j], _slot(1)));
    }
    u3z(subject);
  }
  u3_noun axes[] = {0, 1, 7, 31, 32768, 65536, u3i_chub((1ULL << 40) + 37)};
  for ( unsigned i = 0; i < sizeof(axes)/sizeof(*axes); ++i ) {
    _check(_full(45), u3nt(10, u3nc(axes[i], _lit(987)), _slot(1)));
  }
  // The scry gate returns both its arguments inside a successful unit.
  u3_noun gate = u3nt(u3nc(_lit(0), u3nc(_lit(0), _slot(6))), 0, 0);
  u3_noun scr = u3nt(12, _slot(1), _slot(2));
  u3_noun got = u3m_soft_run(gate, u3n_nock_on, u3nc(4, 5), scr);
  u3_noun expected = u3nt(0, u3nc(4, 5), 4);
  u3_assert(c3y == u3r_sing(got, expected));
  u3z(got); u3z(expected);

  logs = 0;
  u3C.slog_f = _log;
  u3_noun val = u3nt(11, u3nc(c3__slog, _lit(71)), _lit(7));
  got = u3m_soft_run(0, u3n_nock_on, u3nt(4, 5, 6),
                      u3nt(10, u3nc(6, val), _slot(1)));
  u3C.slog_f = 0;
  expected = u3nc(0, u3nt(4, 7, 6));
  u3_assert(logs == 1 && logged[0] == 71 && c3y == u3r_sing(got, expected));
  u3z(got); u3z(expected);
}

// Check a lexical subject inside a result, a kept operand, and a tail call.
static void
_bind_cases(void)
{
  for ( c3_w i = 0; i < 256; ++i ) {
    u3_noun subject = _tree(4);
    u3_noun value = (i & 1) ? _slot(1 + i % 7) : _lit(_tree(3));
    u3_noun read = _slot(2 + 2 * (i % 2));
    u3_noun field = _slot(6 + i % 2);
    u3_noun body;
    switch ( i % 8 ) {
      case 0: body = u3nc(read, field); break;
      case 1: body = u3nt(10, u3nc(2, field), read); break;
      case 2: body = u3nt(7, read, u3nc(_slot(1), field)); break;
      case 3: body = u3nc(u3nc(3, read), u3nc(4, field)); break;
      case 4: body = u3nc(field, read); break;
      case 5: body = u3nc(read, u3nc(_slot(2), field)); break;
      case 6: body = u3nc(read, u3nc(_slot(1), field)); break;
      default: body = u3nc(read, _memo(field)); break;
    }
    u3_noun fol = u3nt(8, value, body);
    if ( i & 2 ) fol = u3nc(fol, _slot(1));
    _check(subject, fol);
  }

  u3_noun deep = _slot(3);
  for ( c3_w i = 0; i < 40; ++i ) deep = u3nc(3, deep);
  _check(12, u3nt(8, _lit(42), u3nc(_slot(2), deep)));
  _check(12, u3nt(8, _lit(42), u3nc(_slot(2), _slot(u3i_chub(1ULL << 36)))));
  _check(12, u3nt(8, _slot(2), _lit(42)));

  // Discarding the boolean still unifies equal children of the two roots.
  u3m_hate(0);
  u3_noun a = u3nt(23, 7, 9), b = u3nt(23, 7, 9);
  u3_assert(u3t(a) != u3t(b));
  u3_noun eq = u3nt(5, _slot(7), _slot(2));
  u3_noun norm = u3nt(8, _slot(2), u3nt(8, eq, _slot(6)));
  u3_noun normalized = u3n_nock_on(u3nc(u3k(a), u3k(b)), norm);
  u3_assert(u3t(a) == u3t(b) && c3y == u3r_sing(normalized, a));
  u3z(normalized); u3z(a); u3z(b);
  u3z(u3m_love(0));

  c3_w fields[] = { 7, 14, 770 };
  for ( c3_w i = 0; i < c3_array_len(fields); i++ ) {
    u3_noun compare = u3nt(5, _slot(fields[i]), _slot(2));
    u3_noun f = u3nt(8, _lit(42), u3nt(8, compare, _slot(6)));
    _check(_numbered(10, 1), u3k(f));
    _check(_numbered(10, 1), u3nc(u3k(f), _slot(1)));
    _check(42, f);
  }

  // A gate receives its new sample. The binding's slog precedes the slog
  // in the arm, including when selecting the replacement later bails.
  u3_noun arm = u3nt(11, u3nc(c3__slog, _lit(82)), _slot(6));
  u3_noun gate = u3nt(arm, 0, 5);
  u3_noun bound = u3nt(11, u3nc(c3__slog, _lit(81)), _lit(gate));
  u3_noun call = u3nt(9, 2, u3nt(10, u3nc(6, _slot(6)), _slot(2)));
  u3_noun fol = u3nt(8, bound, call);
  logs = 0;
  u3C.slog_f = _log;
  u3_noun got = u3m_soft_run(0, u3n_nock_on, u3nc(42, 99), u3k(fol));
  u3_assert(u3h(got) == 0 && u3t(got) == 42);
  u3_assert(logs == 2 && logged[0] == 81 && logged[1] == 82);
  u3z(got);
  got = u3m_soft(0, _burn_twice, u3nc(0, fol));
  u3_assert(u3h(got) == c3__exit && logs == 3 && logged[2] == 81);
  u3z(got);
  u3C.slog_f = 0;
}

static u3_noun
_factory_formula(c3_w index, u3_noun clue)
{
  u3_noun battery = _lit(0x10000 + index);
  u3_noun core = u3nt(8, _lit(index), u3nc(_lit(battery), _slot(1)));
  return u3nt(7, core, u3nt(11, u3nc(c3__fast, _lit(clue)), _slot(1)));
}

static c3_o
_registered_gate(u3_noun gate)
{
  u3j_site site;
  u3j_gate_prep(&site, u3k(gate));
  c3_o found = __(site.loc != u3_none);
  u3j_gate_lose(&site);
  return found;
}

static void
_late_registration(void)
{
  u3m_hate(0);
  u3_noun gate = u3nt(_lit(4723), 0, 0);
  u3_assert(c3n == _registered_gate(gate));

  u3m_hate(0);
  u3j_site site = {.axe = 2, .bat = u3_none, .bas = u3_none, .loc = u3_none};
  u3_assert(u3_none == u3j_site_kick(gate, &site));
  u3_assert(site.pog_p && site.bat == u3h(gate));
  u3_assert(u3_none == u3j_site_kick(gate, &site));
  u3_assert(site.loc == u3_none);

  // The same compiled call site must observe the new registration.
  u3j_mine(u3nt(u3i_string("late"), _lit(0), 0), u3k(gate));
  u3_noun other = u3nt(u3k(u3h(gate)), 42, 0);
  u3_assert(u3_none == u3j_site_kick(other, &site));
  u3_assert(site.loc == u3_none);
  u3z(other);
  u3_assert(u3_none == u3j_site_kick(gate, &site));
  u3_assert(site.loc != u3_none);
  u3j_site_lose(&site);
  u3z(u3m_love(0));
  u3z(gate);
  u3z(u3m_love(0));
}

static void
_parent_registration(void)
{
  u3m_hate(0);
  u3_noun par = u3nc(_lit(4721), 0);
  u3_noun gat = u3nt(_lit(4722), 0, u3k(par));
  u3j_mine(u3nt(u3i_string("outer"), _lit(0), 0), u3k(par));
  u3j_mine(u3nt(u3i_string("inner"), _slot(7), 0), u3k(gat));
  u3_noun bat = u3h(par);
  u3_weak bar = u3h_get(u3R->jed.cod_p, bat);
  u3_assert(bar != u3_none);

  u3_noun empty = u3nt(0, 0, 0);
  u3a_road* outer = u3R;
  u3m_hate(1 << 16);
  u3_assert(c3y == _registered_gate(gat));

  // Profiling can run on an ancestor while the evaluating road is suspended.
  u3a_road* inner = u3R;
  u3R = outer;
  u3j_load(u3nc(u3nc(u3k(bat), u3k(empty)), 0));
  u3R = inner;
  u3_assert(c3n == _registered_gate(gat));
  u3R = outer;
  u3j_load(u3nc(u3nc(u3k(bat), u3k(bar)), 0));
  u3R = inner;
  u3_assert(c3y == _registered_gate(gat));

  // A local registry for this battery shadows the ancestor registration.
  u3j_load(u3nc(u3nc(u3k(bat), u3k(empty)), 0));
  u3_assert(c3n == _registered_gate(gat));
  u3j_load(u3nc(u3nc(u3k(bat), u3k(bar)), 0));
  u3_assert(c3y == _registered_gate(gat));

  // The same change reaches this road when a child returns.
  u3m_hate(0);
  u3j_load(u3nc(u3nc(u3k(bat), u3k(empty)), 0));
  u3z(u3m_love(0));
  u3_assert(c3n == _registered_gate(gat));
  u3z(u3m_love(0));

  u3z(empty); u3z(bar); u3z(gat); u3z(par);
  u3z(u3m_love(0));
}

static void
_factory_cases(void)
{
  // A single program crosses both byte-sized pools, including one valid
  // registration. Other clues exercise the jet dashboard's rejection path.
  u3m_hate(0);
  u3_noun clue = u3nt(u3i_string("shape"), _lit(0), 0);
  u3_noun formula = _factory_formula(260, u3k(clue));
  for ( c3_w i = 260; i-- > 0; ) {
    formula = u3nc(_factory_formula(i, u3nc(i, 0)), formula);
  }
  u3p(u3n_prog) ref = u3n_find(0, formula);
  u3n_prog* program = u3to(u3n_prog, ref);
  u3_assert(program->reg_u.len_w == 261 && program->lit_u.len_w > 256);
  u3p(u3j_fink) previous = 0;
  for ( c3_w pass = 0; pass < 4; ++pass ) {
    u3_noun subject = u3nc(77, pass >> 1);
    u3_noun expected = _eval(subject, formula);
    u3_noun result = u3n_burn(ref, subject);
    u3_assert(c3y == u3r_sing(expected, result));
    u3z(expected);
    u3z(result);
    c3_w registered = 0;
    for ( c3_w i = 0; i < program->reg_u.len_w; ++i ) {
      u3_weak cached = program->reg_u.rit_u[i].clu;
      if ( cached != u3_none ) {
        u3_assert(c3y == u3r_sing(cached, clue));
        u3p(u3j_fink) current = program->reg_u.rit_u[i].fin_p;
        if ( pass & 1 ) u3_assert(current == previous);
        previous = current;
        ++registered;
      }
    }
    u3_assert(registered == 1);
  }
  u3z(formula);
  u3z(clue);
  u3z(u3m_love(0));
}

// A tagged binary grammar with interpreted merges independent of the kernel.
static u3_noun
_fold_kick(c3_w axis, u3_noun value, c3_w base)
{
  return u3nt(9, axis, u3nt(10, u3nc(6, value), _slot(base)));
}

static u3_noun
_fold_child(c3_w axis)
{
  return u3nt(7, _slot(3), _fold_kick(2, _slot(axis), 1));
}

static u3_noun
_fold_tag(c3_w tag)
{
  return u3nt(5, _lit(tag), _slot(12));
}

static u3_noun
_fold_battery(void)
{
  u3_noun outer = u3nt(9, 2, _slot(7));
  u3_noun inner = u3nt(8, outer, _fold_kick(2, _fold_child(26), 2));
  u3_noun body = u3nt(8, inner, _fold_kick(2, _fold_child(27), 2));
  return u3nq(6, _fold_tag(42), _slot(13),
              u3nq(6, _fold_tag(43), body, _slot(0)));
}

static u3_noun
_fold_maker(u3_noun bat, u3_noun sample)
{
  return u3nt(8, sample, u3nc(_lit(bat), _slot(1)));
}

static u3_noun
_fold_context(u3_noun merge)
{
  u3_noun inner = _fold_maker(merge, _slot(6));
  return u3nc(_fold_maker(inner, _lit(0)), 0);
}

static u3_noun
_fold_input(c3_w depth, c3_w leaf, c3_w branch)
{
  u3_noun tree = u3nt(branch, u3nc(leaf, 0), u3nc(leaf, 73));
  while ( depth-- ) tree = u3nt(branch, u3k(tree), tree);
  return tree;
}

static u3_noun
_fold_bit(unsigned table, c3_w axis, unsigned shift)
{
  return u3nq(6, _slot(axis), _lit((table >> shift) & 1),
                               _lit((table >> (shift + 1)) & 1));
}

static void
_fold_tables(u3_noun bat, u3_noun fire)
{
  // Compute the reference alongside construction of a Fibonacci DAG.
  for ( unsigned table = 0; table < 16; table++ ) {
    u3_noun prev = u3nc(42, 0), tree = u3nc(42, 1);
    unsigned a = 0, b = 1;
    for ( unsigned depth = 0; depth < 14; depth++ ) {
      u3_noun next = u3nt(43, prev, u3k(tree));
      prev = tree;
      tree = next;
      unsigned bit = (table >> (2 * a + b)) & 1;
      a = b;
      b = bit;
    }
    u3z(prev);
    for ( unsigned direct = 0; direct < 2; direct++ ) {
      c3_w lhs = direct ? 2 : 30, rhs = direct ? 3 : 6;
      u3_noun merge = u3nq(6, _slot(lhs), _fold_bit(table, rhs, 0), _fold_bit(table, rhs, 2));
      u3_noun gate;
      if ( direct ) {
        u3_noun kids = u3nc(_fold_kick(2, _slot(26), 1), _fold_kick(2, _slot(27), 1));
        u3_noun body = u3nt(7, kids, merge);
        u3_noun arm = u3nq(6, _fold_tag(42), _slot(13),
                            u3nq(6, _fold_tag(43), body, _slot(0)));
        gate = u3nt(arm, u3k(tree), 0);
      }
      else gate = u3nt(u3k(bat), u3k(tree), _fold_context(merge));
      u3_assert(u3n_nock_on(gate, u3k(fire)) == b);
    }
    u3z(tree);
  }
}

static c3_w _fold_logs;

static void
_fold_log(u3_noun message)
{
  u3_assert(message == 88);
  ++_fold_logs;
  u3z(message);
}

static c3_w _fold_jets;

static u3_noun
_fold_sample_jet(u3_noun core)
{
  ++_fold_jets;
  return 88;
}

// One recursive site resolves to a jet only for the empty leaf sample.
static void
_fold_sample_dispatch(u3_noun bat, u3_noun context, u3_noun fire)
{
  u3n_prog* prog = u3to(u3n_prog, u3n_find(0, bat));
  u3j_site* site = prog->cal_u.sit_u + 3;
  u3_assert(site->axe == 2 && site->loc == u3_none);
  u3j_site saved = *site;
  u3j_core driver = { .cos_c = "fold" };
  u3j_harm jet = { .fun_f = _fold_sample_jet, .liv = c3y, .ice = c3y };
  u3j_fink* fine = u3a_malloc(sizeof(*fine) + sizeof(u3j_fist));
  fine->len_w = 1;
  fine->sat = u3nc(42, 0);
  fine->fis_u[0] = (u3j_fist){bat, 6};
  site->loc = 0;
  site->fon_o = c3n;
  site->fin_p = u3of(u3j_fink, fine);
  site->jet_o = c3y;
  site->cop_u = &driver;
  site->ham_u = &jet;
  _fold_jets = 0;
  u3_noun gate = u3nt(u3k(bat), _fold_input(4, 42, 43), u3k(context));
  u3_assert(u3n_nock_on(gate, u3k(fire)) == 88 && _fold_jets == 16);
  *site = saved;
  u3z(fine->sat);
  u3a_free(fine);
}

static void
_fold_check(void)
{
  u3_noun bat = _fold_battery();
  u3_noun merge = u3nq(6, u3nt(5, _lit(0), _slot(30)), _slot(6), _slot(30));
  u3_noun context = _fold_context(merge);
  u3_noun tree = _fold_input(11, 42, 43);
  u3_noun fire = u3nt(9, 2, _slot(1));
  for ( unsigned i = 0; i < 4; i++ ) {
    u3_noun seed = i & 1 ? _fold_context(_lit(99)) : u3k(context);
    u3_noun core = u3nt(u3k(bat), u3k(tree), seed);
    u3_noun result = u3n_nock_on(core, u3k(fire));
    u3_assert(result == (i & 1 ? 99 : 73));
  }
  _fold_sample_dispatch(bat, context, fire);
  _fold_tables(bat, fire);
  u3_noun effect = u3nt(11, u3nc(c3__slog, _lit(88)), _lit(1));
  u3_noun small = _fold_input(4, 42, 43);
  u3C.slog_f = _fold_log;
  _fold_logs = 0;
  u3_noun result = u3n_nock_on(u3nt(u3k(bat), small, _fold_context(effect)), u3k(fire));
  u3C.slog_f = 0;
  u3_assert(result == 1 && _fold_logs == 31);
  // A constructor hint must run even if its returned gate is a pure selector.
  u3_noun factory = u3nt(11, u3nc(c3__slog, _lit(88)), u3k(u3h(context)));
  u3_noun gate = u3nt(u3k(bat), _fold_input(4, 42, 43), u3nc(factory, 0));
  u3C.slog_f = _fold_log;
  _fold_logs = 0;
  result = u3n_nock_on(gate, u3k(fire));
  u3C.slog_f = 0;
  u3_assert(result == 73 && _fold_logs == 31);
  if ( u3R == &u3H->rod_u ) {
    u3_noun bad = u3nt(43, u3k(tree), u3nc(41, 0));
    u3_noun failure = u3m_soft(0, _burn_twice,
                         u3nc(u3nt(u3k(bat), bad, u3k(context)), u3k(fire)));
    u3_assert(u3h(failure) == c3__exit);
    u3z(failure);
  }
  u3z(context);
  u3z(tree);
  u3z(fire);
  u3z(bat);
}

static c3_w _arm_punts;

static u3_weak
_arm_punt(u3_noun core)
{
  ++_arm_punts;
  return u3_none;
}

// Each pushed value calls an interpreted leaf. The last call supplies the
// product, so changing the leaf can change both the value and its effects.
static void
_arm_result_cases(void)
{
  u3m_hate(0);
  u3_noun arm = u3nt(9, 2, _slot(2047));
  for ( c3_w i = 8; i-- > 0; ) {
    arm = u3nt(8, u3nt(9, 2, _slot((8u << i) - 1)), arm);
  }
  u3_noun caller = u3nc(u3nt(9, 4, _slot(1)), _lit(0));
  u3p(u3n_prog) ref = u3n_find(0, caller);
  u3_noun battery = u3nc(arm, 0);
  u3_noun leaf = u3nc(_slot(3), _slot(3));
  for ( c3_w value = 41; value < 43; ++value ) {
    u3_weak previous = u3_none;
    for ( c3_w pass = 0; pass < 4; ++pass ) {
      u3_noun core = u3nt(u3k(battery), 0, u3nc(u3k(leaf), value));
      u3_noun product = u3n_burn(ref, core);
      u3_assert(u3t(product) == 0 && u3h(u3h(product)) == value &&
                u3t(u3h(product)) == value);
      // Keep the earlier cell alive to distinguish reuse from reallocation.
      if ( previous != u3_none ) {
#if !defined(U3_CPU_DEBUG) && !defined(U3_MEMORY_DEBUG) && !defined(VERBOSE_BYTECODE)
        if ( pass == 3 ) u3_assert(previous == u3h(product));
#endif
        u3z(previous);
      }
      previous = u3k(u3h(product));
      u3z(product);
    }
    u3z(previous);
  }
  // Relocking any inner call site invalidates the recorded execution.
  u3j_site* outer = u3to(u3n_prog, ref)->cal_u.sit_u;
  u3n_prog* builder = u3to(u3n_prog, outer->pog_p);
  u3j_site* site = &builder->cal_u.sit_u[0];
  u3j_site_ream(site);

  // A native punt still executes, even when fallback yields the same noun.
  u3j_site saved = *site;
  u3j_harm jet = { .fun_f = _arm_punt, .liv = c3y, .ice = c3y };
  u3j_core driver = { .cos_c = "arm-punt" };
  u3j_fink* fine = u3a_malloc(sizeof(*fine));
  fine->len_w = 0;
  fine->sat = u3nc(u3k(leaf), 42);
  site->loc = 0;
  site->fin_p = u3of(u3j_fink, fine);
  site->jet_o = c3y;
  site->cop_u = &driver;
  site->ham_u = &jet;
  _arm_punts = 0;
  for ( c3_w pass = 0; pass < 3; ++pass ) {
    u3_noun product = u3n_burn(ref, u3nt(u3k(battery), 0, u3k(fine->sat)));
    u3_assert(u3h(u3h(product)) == 42 && _arm_punts == pass + 1);
    u3z(product);
  }
  *site = saved;
  u3z(fine->sat);
  u3a_free(fine);

  u3_noun logging = u3nt(11, u3nc(c3__slog, _lit(88)), leaf);
  u3_noun core = u3nt(battery, 0, u3nc(logging, 43));
  _fold_logs = 0;
  u3C.slog_f = _fold_log;
  for ( c3_w pass = 0; pass < 3; ++pass ) {
    u3_noun product = u3n_burn(ref, u3k(core));
    u3_assert(u3h(u3h(product)) == 43 && u3t(u3h(product)) == 43);
    u3_assert(_fold_logs == 9 * (pass + 1));
    u3z(product);
  }
  u3C.slog_f = 0;
  u3z(core);
  u3z(caller);
  u3z(u3m_love(0));
}

int
main(void)
{
  u3m_boot_lite(1 << 27);
  _parent_registration();
  _late_registration();
  _arm_result_cases();
  _list_masks();
  _test_list_weld();
  u3m_hate(0);
  _list_masks();
  _test_list_weld();
  u3z(u3m_love(0));
  _list_guards();
  _list_expressions();
  _fold_check();
  u3m_hate(0);
  _fold_check();
  u3z(u3m_love(0));
  _effects();
  _prepared_cases();
#if defined(U3_CPU_DEBUG)
  _prepared_profile();
#endif
  _prepared_ownership();
  u3m_hate(0);
  _prepared_ownership();
  u3z(u3m_love(0));
  _indexed();
  _mixed();
  _mixed_effects();
  _full_literal_pool();
  _project_pair();
  _borrow_cases();
  _owner_cases();
  _borrow_ownership();
  _reuse_cases();
  u3m_hate(0);
  _borrow_ownership();
  _reuse_cases();
  u3z(u3m_love(0));
  for ( c3_w keep = 0; keep < 2; ++keep ) {
    for ( c3_w guard = 0; guard < 2; ++guard ) {
      u3_noun f = _chain(40, guard, 4);
      if ( keep ) f = u3nc(_memo(f), _slot(1));
      for ( c3_w i = 0; i < 42; ++i ) {
        _check(u3nc(u3nc(_tag(i), i), 123), u3k(f));
      }
      _check(55, u3k(f));
      _check(u3nc(55, 2), f);
    }
  }
  // Three-byte literals make the byte displacement exceed 65535 while
  // the instruction count stays inside the threading pass.
  u3_noun wide = _lit(1024);
  for ( c3_w d = 0; d < 14; ++d ) wide = u3nc(u3k(wide), wide);
  for ( c3_w i = 0; i < 2; ++i ) {
    u3_noun test = u3nq(6, u3nc(3, _slot(1)), _slot(2), _lit(1));
    _check(u3nc(i, 3), u3nq(6, test, u3k(wide), _slot(3)));
  }
  u3z(wide);
  u3_noun numbered = _numbered(16, 1);
  for ( c3_w i = 0; i <= 256; ++i ) {
    _check(u3k(numbered), u3nc(_slot(i), _lit(0)));
  }
  for ( c3_w i = 0; i < 1000; ++i ) {
    c3_w axis = _rand() & 65535;
    _check(u3k(numbered), u3nc(_slot(axis), _slot(axis)));
  }
  u3z(numbered);
  u3_noun deep = _full(70);
  c3_h words[] = {0x87654321, 0x12345678, 1};
  u3_noun long_axis = u3i_halfs(3, words);
  u3_assert(u3r_met(0, long_axis) == 65);
  _check(deep, _slot(long_axis));
  for ( c3_w i = 0; i < 5000; ++i ) {
    u3_noun fol = _formula(1 + _rand() % 5);
    _check(_tree(4), fol);
  }
  _bind_cases();
  _factory_cases();
  _projection_cases();
  _check_short_axes();
  u3C.wag_h |= u3o_debug_ram;
  u3m_grab();
  fprintf(stderr, "nock-compiler-test: %u cases, %u products, %u exits\n",
          cases, cases - exits, exits);
  return 0;
}
