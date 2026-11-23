// Differential tests for Nock decision compilation. The oracle evaluates the
// terminating subset used by the generator directly on nouns.
#include "noun.h"
#include <time.h>

static c3_h seed = 0xa2c87f19;
static c3_h cases, exits;
static c3_t trace;

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
  if ( c3n == u3r_sing(cold, warm) ) abort();
  // The nested road grows in the opposite direction. Re-enter through find
  // so jet and memo sites belong to the active road.
  u3m_hate(0);
  prog = u3n_find(0, u3t(pair));
  u3_noun nested = u3m_love(u3n_burn(prog, u3k(u3h(pair))));
  if ( c3n == u3r_sing(cold, nested) ) abort();
  u3z(cold);
  u3z(warm);
  return nested;
}

static void
_check(u3_noun bus, u3_noun fol)
{
  u3_weak ref = _eval(bus, fol);
  u3_noun pair = u3nc(bus, fol);
  u3_noun gon = u3m_soft(0, _burn_twice, u3k(pair));
  ++cases;
  if ( u3_none == ref ) ++exits;
  if ( trace ) {
    printf("%u %08x %08x %s %08x\n", cases, seed, (unsigned)u3r_mug(pair),
           ref == u3_none ? "exit" : "product", (unsigned)u3r_mug(gon));
  }
  if ( u3_none == ref ? u3h(gon) != c3__exit
                      : (u3h(gon) != 0 || c3n == u3r_sing(ref, u3t(gon))) ) {
    fprintf(stderr, "decision-test case %u seed %08x\n", cases, seed);
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
    if ( result != expected ) abort();
    u3z(result);
    // An autocons keeps the subject while evaluating its first formula.
    result = u3n_nock_on(u3k(tree), u3nc(_slot(axis), _lit(9)));
    if ( u3h(result) != expected || u3t(result) != 9 ) abort();
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
  if ( logs >= 8 ) abort();
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
  if ( u3h(gon) != 0 || u3t(gon) != 123 || logs != 3 ) abort();
  for ( c3_w i = 0; i < logs; ++i ) if ( logged[i] != 4 ) abort();
  u3z(gon);
  gon = u3m_soft(0, _burn_twice, u3nc(u3nc(6, 123), fol));
  if ( u3h(gon) != c3__exit || logs != 3 ) abort();
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
  if ( u3h(gon) || u3t(gon) != 55 || logs != 3 ) abort();
  for ( unsigned i = 0; i < 3; ++i ) if ( logged[i] != 41 ) abort();
  u3z(gon);
  gon = u3m_soft(0, _burn_twice, u3nc(u3nc(7, 55), fol));
  if ( u3h(gon) != c3__exit || logs != 3 ) abort();
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
  if ( c3n == u3r_sing(gon, expected) ) abort();
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

/* Compare a prepared gate with the oracle on [battery [sample context]].
 * Reuse the prepared site for two calls to exercise sample ownership. */
static u3_noun
_prepared_twice(u3_noun input)
{
  u3_noun gate, sample, specialized;
  u3x_trel(input, &gate, &sample, &specialized);
  u3j_site site;
  u3j_gate_prep(&site, u3k(gate));
#ifdef U3_GATE_SAMPLE
  if ( !!site.sample_y != specialized ) abort();
#endif
  u3_noun a = u3j_gate_slam(&site, u3k(sample));
  u3_noun b = u3j_gate_slam(&site, u3k(sample));
  if ( c3n == u3r_sing(a, b) ) abort();
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
  if ( expected == u3_none ? u3h(result) != c3__exit :
       (u3h(result) != 0 || c3n == u3r_sing(expected, u3t(result))) ) abort();
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
  if ( logs != 2 || logged[0] != 71 || logged[1] != 71 ) abort();
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
  if ( result != child || u3a_use(child) != 2 ) abort();
  u3z(result);
  if ( u3a_use(child) != 1 ) abort();
  u3z(child);
  u3j_gate_lose(&site);
}

#if defined(U3_CPU_DEBUG) && defined(U3_GATE_SAMPLE)
static void
_prepared_profile(void)
{
  u3j_site site;
  u3j_gate_prep(&site, u3nt(_slot(6), 0, 37));
  const c3_w flags[] = {0, u3o_trace, u3o_debug_cpu, 0};
  c3_w saved = u3C.wag_h;
  for ( unsigned i = 0; i < 4; ++i ) {
    u3C.wag_h = saved | flags[i];
    c3_d before = u3R->pro.cel_d;
    if ( 42 != u3j_gate_slam(&site, 42) ) abort();
    if ( u3R->pro.cel_d - before != (flags[i] ? 2 : 0) ) abort();
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
    if ( (i == 0 && product != c3y) || (i && c3n == u3du(product)) ) abort();
    u3z(product);
    if ( u3a_use(leaf) != 1 ) abort();
    u3z(leaf);
    u3z(fol);
  }
}

// Warm the old mug and preserve external references independently. The
// replacement alternates between a literal and the complete original core.
static void
_reuse_cases(void)
{
  unsigned same = 0, eligible = 0;
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
      eligible += !(mode & 4) && (c3y == u3a_is_mutable(u3R, bus));
      u3_noun result = u3n_burn(program, bus);
      // Compare only the encoded offset. A replaced input can already be free.
      same += result == bus;
      if ( u3r_mug(expected) != u3r_mug(result) ||
           c3n == u3r_sing(result, expected) ) abort();
      if ( root != u3_none ) {
        if ( c3n == u3r_sing(root, other) ) abort();
        u3z(root);
      }
      if ( payload != u3_none ) {
        if ( c3n == u3r_sing(payload, u3t(other)) ) abort();
        u3z(payload);
      }
      u3z(other); u3z(expected); u3z(result); u3z(fol);
    }
  }
  if ( getenv("U3_OWNER_IDENTITY") )
    fprintf(stderr, "edit identity: %u eligible, %u reused\n", eligible, same);
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
  if ( c3n == u3r_sing(got, expected) ) abort();
  u3z(got); u3z(expected);

  logs = 0;
  u3C.slog_f = _log;
  u3_noun val = u3nt(11, u3nc(c3__slog, _lit(71)), _lit(7));
  got = u3m_soft_run(0, u3n_nock_on, u3nt(4, 5, 6),
                      u3nt(10, u3nc(6, val), _slot(1)));
  u3C.slog_f = 0;
  expected = u3nc(0, u3nt(4, 7, 6));
  if ( logs != 1 || logged[0] != 71 || c3n == u3r_sing(got, expected) ) abort();
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
  if ( u3h(got) || u3t(got) != 42 || logs != 2 ||
       logged[0] != 81 || logged[1] != 82 ) abort();
  u3z(got);
  got = u3m_soft(0, _burn_twice, u3nc(0, fol));
  if ( u3h(got) != c3__exit || logs != 3 || logged[2] != 81 ) abort();
  u3z(got);
  u3C.slog_f = 0;
}

static void
_pooled_triples(void)
{
  u3_noun shared = u3nc(32, 64);
  u3_noun held = u3_nul;
  for ( c3_w remaining = 0; remaining <= 2; ++remaining ) {
    if ( u3R->cel.cel_p ) {
      if ( u3R->cel.hav_w < remaining ) {
        while ( u3R->cel.hav_w ) held = u3nc(0, held);
        held = u3nc(0, held);
      }
      while ( u3R->cel.hav_w > remaining ) held = u3nc(0, held);
      if ( u3R->cel.hav_w != remaining ) abort();
    }
    u3_noun triple = u3nt(u3k(shared), remaining, u3k(shared));
    if ( u3h(triple) != shared || u3h(u3t(triple)) != remaining ||
         u3t(u3t(triple)) != shared || u3a_use(shared) != 3 ||
         u3a_use(triple) != 1 || u3a_use(u3t(triple)) != 1 ) abort();
    u3z(triple);
    if ( u3a_use(shared) != 1 ) abort();
  }
  u3z(held);
  u3z(shared);
}

static u3_noun
_factory_formula(c3_w index, u3_noun clue)
{
  u3_noun battery = _lit(0x10000 + index);
  u3_noun core = u3nt(8, _lit(index), u3nc(_lit(battery), _slot(1)));
  return u3nt(7, core, u3nt(11, u3nc(c3__fast, _lit(clue)), _slot(1)));
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
  if ( program->reg_u.len_w != 261 || program->lit_u.len_w <= 256 ) abort();
  u3p(u3j_fink) previous = 0;
  for ( c3_w pass = 0; pass < 4; ++pass ) {
    u3_noun subject = u3nc(77, pass >> 1);
    u3_noun expected = _eval(subject, formula);
    u3_noun result = u3n_burn(ref, subject);
    if ( c3n == u3r_sing(expected, result) ) abort();
    u3z(expected);
    u3z(result);
    c3_w registered = 0;
    for ( c3_w i = 0; i < program->reg_u.len_w; ++i ) {
      u3_weak cached = program->reg_u.rit_u[i].clu;
      if ( cached != u3_none ) {
        if ( c3n == u3r_sing(cached, clue) ) abort();
        u3p(u3j_fink) current = program->reg_u.rit_u[i].fin_p;
        if ( (pass & 1) && current != previous ) abort();
        previous = current;
        ++registered;
      }
    }
    if ( registered != 1 ) abort();
  }
  u3z(formula);
  u3z(clue);
  u3z(u3m_love(0));
}

static double
_seconds(void)
{
  struct timespec ts;
  if ( clock_gettime(CLOCK_MONOTONIC, &ts) ) abort();
  return (double)ts.tv_sec + 1e-9 * ts.tv_nsec;
}

static void
_borrow_bench(void)
{
  const char* names[] = {"equal", "equal-heap", "pair", "edit", "sample", "owner-edit"};
  u3_noun formulas[] = {
    _borrow_formula(0, 4, 6, 0), _borrow_formula(0, 2, 3, 0),
    _borrow_formula(1, 2, 2, 3), _borrow_formula(3, 2, 6, 0),
    _borrow_formula(6, 2, 3, 2), u3nt(10, u3nc(6, u3nc(4, _slot(4))), _slot(1)),
  };
  u3m_hate(0);
  for ( unsigned k = 0; k < 6; ++k ) {
    u3p(u3n_prog) pg = u3n_find(0, formulas[k]);
    u3_noun inputs[16], expected[16];
    for ( unsigned j = 0; j < 16; ++j ) {
      inputs[j] = u3nc(u3nt(j, j+1, j+2), u3nt(j, j + (j&1), j+2));
      expected[j] = _eval(inputs[j], formulas[k]);
      if ( expected[j] == u3_none ) abort();
    }
    unsigned n = 750000;
    double start = _seconds();
    for ( unsigned j = 0; j < n; ++j ) {
      unsigned ix = j & 15;
      u3_noun out = u3n_burn(pg, u3k(inputs[ix]));
      if ( c3n == u3r_sing(out, expected[ix]) ) abort();
      u3z(out);
    }
    printf("%s,%.3f\n", names[k], 1e9 * (_seconds()-start)/n);
    for ( unsigned j = 0; j < 16; ++j ) { u3z(expected[j]); u3z(inputs[j]); }
  }
  // Carry the only owning reference to the next iteration. The other
  // measurements retain their prepared subjects while each call runs.
  u3_noun update = u3nt(10, u3nc(6, u3nc(4, _slot(6))), _slot(1));
  u3p(u3n_prog) edit = u3n_find(0, update);
  u3_noun core = u3nt(41, 0, u3nc(97, 98));
  unsigned n = 750000;
  double start = _seconds();
  for ( unsigned j = 0; j < n; ++j ) {
    core = u3n_burn(edit, core);
    if ( u3h(u3t(core)) != j + 1 ) abort();
  }
  printf("unique-sample,%.3f\n", 1e9 * (_seconds()-start)/n);
  if ( u3h(core) != 41 || u3h(u3t(u3t(core))) != 97 ||
       u3t(u3t(u3t(core))) != 98 ) abort();
  u3z(core);
  u3z(update);
  u3z(u3m_love(0));
  for ( unsigned k = 0; k < 6; ++k ) u3z(formulas[k]);
}

static void
_mixed_bench(c3_w axis)
{
  const c3_w tags[] = {c3__void, c3__noun, c3__atom, c3__cell,
                       c3__core, c3__face, c3__fork, c3__hold};
  u3m_hate(0);
  u3_noun fol = _lit(8);
  for ( unsigned i = 8; i-- > 0; ) {
    c3_t cell = i >= 2;
    u3_noun test = _mixed_test(axis, cell ? 2*axis : axis, cell, i & 1, tags[i]);
    fol = u3nq(6, test, _lit(i), fol);
  }
  u3p(u3n_prog) prog = u3n_find(0, fol);
  u3_noun subjects[10];
  for ( unsigned i = 0; i < 10; ++i ) {
    u3_noun key = i < 8 ? tags[i] : 77;
    if ( i >= 2 && i != 8 ) key = u3nc(key, u3nc(0, i));
    subjects[i] = _at_axis(axis, key);
    if ( u3n_burn(prog, u3k(subjects[i])) != (i < 8 ? i : 8) ) abort();
  }
  // Fixed inputs expose the first-arm cost. The cycle covers all eight
  // winners plus the default on both atom and cell subjects.
  const char* names[] = {"first-atom", "last-cell", "miss-atom", "miss-cell", "cycle"};
  const unsigned inputs[] = {0, 7, 8, 9};
  const unsigned n = 2000000;
  for ( unsigned which = 0; which < 5; ++which ) {
    double start = _seconds();
    for ( unsigned i = 0; i < n; ++i ) {
      unsigned k = which == 4 ? i % 10 : inputs[which];
      if ( u3n_burn(prog, u3k(subjects[k])) != (k < 8 ? k : 8) ) abort();
    }
    printf("%u,%s,%.2f\n", (unsigned)axis, names[which], 1e9 * (_seconds()-start) / n);
  }
  for ( unsigned i = 0; i < 10; ++i ) u3z(subjects[i]);
  u3z(fol);
  u3z(u3m_love(0));
}

static void
_bench(c3_w arms, const char* mode)
{
  u3m_hate(0);
  u3_noun fol = _lit(arms);
  for ( c3_w i = arms; i-- > 0; ) {
    u3_noun test = u3nt(5, _slot(12), _lit(0x80000 + i));
    test = u3nq(6, u3nc(3, _slot(6)), test, _lit(1));
    fol = u3nq(6, test, _lit(i), fol);
  }
  u3p(u3n_prog) prog = u3n_find(0, fol);
  u3_noun subjects[129];
  for ( c3_w i = 0; i <= arms; ++i ) {
    subjects[i] = u3nc(7, u3nc(u3nc(0x80000 + i, 4), 5));
    if ( i != u3n_burn(prog, u3k(subjects[i])) ) abort();
  }
  const c3_w n = 500000;
  c3_t first = !strcmp(mode, "first");
  c3_t miss = !strcmp(mode, "miss");
  double start = _seconds();
  for ( c3_w i = 0; i < n; ++i ) {
    c3_w k = first ? 0 : miss ? arms : (i * 37) % (arms + 1);
    if ( k != u3n_burn(prog, u3k(subjects[k])) ) abort();
  }
  printf("%u,%.1f\n", (unsigned)arms, 1e9 * (_seconds() - start) / n);
  for ( c3_w i = 0; i <= arms; ++i ) u3z(subjects[i]);
  u3z(fol);
  u3z(u3m_love(0));
}

int
main(void)
{
  u3m_boot_lite(1 << 27);
  if ( getenv("U3_SHORT_AXIS_CHECK") ) {
    if ( getenv("U3_CASE_GC") ) u3C.wag_h |= u3o_debug_ram;
    _check_short_axes();
    return 0;
  }
  if ( getenv("U3_BORROW_MICRO") ) { _borrow_bench(); return 0; }
  if ( getenv("U3_MIXED_MICRO") ) {
    _mixed_bench(126);
    _mixed_bench(510);
    return 0;
  }
  const char* mode = getenv("U3_CASE_BENCH");
  if ( mode ) {
    for ( c3_w rep = 0; rep < 5; ++rep ) {
      for ( c3_w arms = 4; arms <= 128; arms *= 2 ) _bench(arms, mode);
    }
    return 0;
  }
  trace = getenv("U3_CASE_TRACE") != 0;
  if ( getenv("U3_CASE_GC") ) u3C.wag_h |= u3o_debug_ram;
  _effects();
  _prepared_cases();
#if defined(U3_CPU_DEBUG) && defined(U3_GATE_SAMPLE)
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
  if ( u3r_met(0, long_axis) != 65 ) abort();
  _check(deep, _slot(long_axis));
  for ( c3_w i = 0; i < 5000; ++i ) {
    u3_noun fol = _formula(1 + _rand() % 5);
    _check(_tree(4), fol);
  }
  _bind_cases();
  _factory_cases();
  _pooled_triples();
  u3m_hate(0);
  _pooled_triples();
  u3z(u3m_love(0));
  fprintf(stderr, "decision-test: %u cases, %u products, %u exits\n",
          cases, cases - exits, exits);
  return 0;
}
