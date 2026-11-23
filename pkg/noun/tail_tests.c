// Checked products, crashes and effects for self-recursive Nock batteries.
#include "noun.h"

static unsigned cases, exits, messages;

static u3_noun
_slot(unsigned axis)
{
  return u3nc(0, axis);
}

static u3_noun
_inc(unsigned axis, unsigned count)
{
  u3_noun fol = _slot(axis);
  while ( count-- ) fol = u3nc(4, fol);
  return fol;
}

static u3_noun
_battery(unsigned pair, u3_noun lhs, u3_noun rhs, u3_noun done,
         u3_noun left, u3_noun right)
{
  u3_noun edit = u3nt(10, u3nc(pair, u3nc(left, right)), _slot(1));
  return u3nq(6, u3nt(5, lhs, rhs), done, u3nt(9, 2, edit));
}

static u3_noun
_run(u3_noun core)
{
  return u3n_nock_on(u3k(core), u3nt(9, 2, _slot(1)));
}

static u3_noun
_run_three(u3_noun core)
{
  u3_noun first = _run(core);
  u3_noun warm = _run(core);
  u3m_hate(0);
  u3_noun nested = u3m_love(_run(core));
  if ( c3n == u3r_sing(first, warm) || c3n == u3r_sing(first, nested) ) abort();
  u3z(first);
  u3z(warm);
  return nested;
}

static void
_check(u3_noun battery, u3_noun sample, u3_weak expected)
{
  u3_noun core = u3nt(battery, sample, 0);
  u3_noun outcome = u3m_soft(0, _run_three, u3k(core));
  ++cases;
  if ( expected == u3_none ) ++exits;
  if ( expected == u3_none ? u3h(outcome) != c3__exit
                           : u3h(outcome) || c3n == u3r_sing(u3t(outcome), expected) ) {
    fprintf(stderr, "tail-test case %u\n", cases);
    u3m_p("input", core);
    u3m_p("outcome", outcome);
    if ( expected != u3_none ) u3m_p("expected", expected);
    abort();
  }
  if ( expected != u3_none ) u3z(expected);
  u3z(core);
  u3z(outcome);
}

static void
_counters(void)
{
  const unsigned lengths[] = {0, 1, 2, 63, 64, 4095, 4096, 4097, 4098, 8195};
  for ( unsigned stride = 1; stride <= 17; ++stride ) {
    unsigned bumps = 18 - stride;
    for ( unsigned k = 0; k < sizeof(lengths) / sizeof(*lengths); ++k ) {
      unsigned steps = lengths[k], start = k + 11, end = start + steps * stride;
      unsigned index = (k & 1) ? 27 : 26;
      unsigned acc = index ^ 1;
      u3_noun lhs = _slot((k & 2) ? 12 : index);
      u3_noun rhs = _slot((k & 2) ? index : 12);
      u3_noun left = _inc(26, index == 26 ? stride : bumps);
      u3_noun right = _inc(27, index == 27 ? stride : bumps);
      u3_noun bat = _battery(13, lhs, rhs, _slot(acc), left, right);
      u3_noun sample = u3nt(end, index == 26 ? start : 42, index == 27 ? start : 42);
      _check(bat, sample, 42 + steps * bumps);
    }
  }
  // Cross the direct-atom boundary in the middle of a batch.
  _check(_battery(13, _slot(26), _slot(12), _slot(27), _inc(26, 1), _inc(27, 2)),
         u3nt(100, 0, 0x7fffffcb), u3i_chub(0x80000093ULL));
  // An indirect invariant and an invariant inside the edited subtree.
  _check(_battery(13, _slot(26), _slot(12), _slot(27), _inc(26, 1), _inc(27, 3)),
         u3nt(u3i_chub(0x10000000008ULL), u3i_chub(0x10000000000ULL), 7), 31);
  _check(_battery(13, _slot(26), _slot(27), _slot(26), _inc(26, 1), _slot(27)),
         u3nt(0, 0, 137), 137);
  // Both right-hand sides read the old pair. The right field lags one step.
  _check(_battery(13, _slot(26), _slot(12), _slot(27), _inc(26, 1), _slot(26)),
         u3nt(4300, 0, 999), 4299);
  // The ordinary exit arm sees the materialized sample, including its limit.
  _check(_battery(13, _slot(12), _slot(26), _slot(6), _inc(26, 1), _inc(27, 2)),
         u3nt(4500, 0, 7), u3nt(4500, 4500, 9007));
  _check(_battery(13, _slot(26), _slot(12), _slot(54), _inc(26, 1), _inc(27, 1)),
         u3nt(78, 1, 0), u3_none);
}

// Build the sample bottom-up so the sentinel at each sibling is distinct.
static u3_noun
_wrap(unsigned path, u3_noun value)
{
  while ( path > 1 ) {
    value = (path & 1) ? u3nc(path, value) : u3nc(value, path);
    path >>= 1;
  }
  return value;
}

static void
_nested_edits(void)
{
  for ( unsigned depth = 0; depth <= 12; ++depth ) {
    unsigned root = 1U << depth;
    for ( unsigned j = 0; j < 8; ++j ) {
      unsigned bits = (j * 1237) & (root - 1);
      unsigned pair = (6U << depth) | bits;
      unsigned field = 2 * pair;
      u3_noun bat = _battery(pair, _slot(field), u3nc(1, 99), _slot(6),
                             _inc(field, 1), _inc(field | 1, 1));
      _check(bat, _wrap(root | bits, u3nc(3, 17)),
                  _wrap(root | bits, u3nc(99, 113)));
    }
  }
}

// A formula can run directly on a core with another battery. Its final kick
// must enter that battery, even when this formula has an eligible tail site.
static u3_noun
_other_arm(u3_noun bat)
{
  u3_noun cor = u3nt(u3nc(1, 192), u3nt(7000, 0, 4), 0);
  return u3n_nock_on(cor, u3k(bat));
}

static void
_indirect_call(void)
{
  u3_noun battery = _battery(13, _slot(26), _slot(12), _slot(27),
                             _inc(26, 1), _inc(27, 1));
  u3_noun result = u3m_soft(0, _other_arm, battery);
  if ( u3h(result) || u3t(result) != 192 ) abort();
  ++cases;
  u3z(result);
}

static u3_noun
_walk(unsigned result, u3_noun update)
{
  return _battery(6, u3nc(1, 0), _slot(12), _slot(result), _slot(25), update);
}

static void
_lists(void)
{
  for ( unsigned kind = 0; kind < 3; ++kind ) {
    u3_noun list = kind == 1 ? 77 : 0;
    u3_noun last = u3nc(u3i_string("indirect atom"), 431);
    for ( unsigned i = 0; i < 8200; ++i ) {
      list = u3nc(i ? i : u3k(last), list);
    }
    if ( kind == 2 ) {
      _check(_walk(13, _inc(13, 1)), u3nc(list, 3), 8203);
    }
    else {
      _check(_walk(13, _slot(24)), u3nc(list, 0), kind ? u3_none : u3k(last));
    }
    u3z(last);
  }
  // The projected head becomes a cell after several valid bumps.
  u3_noun list = u3nc(u3nc(7, 8), 0);
  for ( unsigned i = 0; i < 65; ++i ) list = u3nc(7, list);
  _check(_walk(13, _inc(24, 1)), u3nc(list, 0), u3_none);
}

static void
_log(u3_noun msg)
{
  if ( msg != 101 ) abort();
  ++messages;
  u3z(msg);
}

static void
_effects(void)
{
  u3_noun hinted = u3nt(11, u3nc(c3__slog, u3nc(1, 101)), _inc(27, 1));
  u3C.slog_f = _log;
  _check(_battery(13, _slot(26), _slot(12), _slot(27), _inc(26, 1), hinted),
         u3nt(17, 0, 0), 17);
  if ( messages != 3 * 17 ) abort();
  u3C.slog_f = 0;
}

#ifdef U3_TAIL_BENCH
static void
_registry(void)
{
  u3j_site site = {0};
  site.bat = u3i_string("unregistered battery");
  site.bas = u3i_string("unregistered digest");
  site.loc = u3_none;
  if ( c3y != u3j_site_unregistered(&site) ) abort();
  site.loc = 0;
  if ( c3n != u3j_site_unregistered(&site) ) abort();
  site.loc = u3_none;
  u3h_put(u3H->rod_u.jed.hot_p, site.bas, 0);
  if ( c3n != u3j_site_unregistered(&site) ) abort();
  u3h_del(u3H->rod_u.jed.hot_p, site.bas);
  u3m_hate(0);
  u3h_put(u3R->jed.cod_p, site.bat, 0);
  if ( c3n != u3j_site_unregistered(&site) ) abort();
  u3m_hate(0);
  if ( c3n != u3j_site_unregistered(&site) ) abort();
  u3z(u3m_love(0));
  u3h_del(u3R->jed.cod_p, site.bat);
  u3z(u3m_love(0));
  if ( c3y != u3j_site_unregistered(&site) ) abort();
  u3C.wag_h |= u3o_trace;
  if ( c3n != u3j_site_unregistered(&site) ) abort();
  u3C.wag_h &= ~u3o_trace;
  u3z(site.bat);
  u3z(site.bas);
}
#endif

#ifdef U3_CPU_DEBUG
static void
_count_kicks(void)
{
  u3m_hate(0);
  u3_noun bat = _battery(13, _slot(26), _slot(12), _slot(27), _inc(26, 1), _inc(27, 1));
  u3p(u3n_prog) prog = u3n_find(0, bat);
  u3_noun core = u3nt(bat, u3nt(8701, 0, 0), 0);
  c3_d before = u3R->pro.nox_d;
  if ( u3n_burn(prog, core) != 8701 || u3R->pro.nox_d - before != 8702 ) abort();
  u3z(u3m_love(0));
}
#endif

int
main(void)
{
  u3m_boot_lite(1 << 27);
  if ( getenv("U3_CASE_GC") ) u3C.wag_h |= u3o_debug_ram;
#ifdef U3_TAIL_BENCH
  _registry();
#endif
  _counters();
  _nested_edits();
  _indirect_call();
  _lists();
  _effects();
#ifdef U3_CPU_DEBUG
  _count_kicks();
#endif
  fprintf(stderr, "tail-test: %u cases, %u products, %u exits, %u hints\n",
          cases, cases - exits, exits, messages);
  return 0;
}
