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
_fold_tables(u3_noun bat, u3_noun fire, unsigned depth_limit, c3_t timing)
{
  // Compute the reference alongside construction of a Fibonacci DAG.
  for ( unsigned table = 0; table < 16; table++ ) {
    u3_noun prev = u3nc(42, 0), tree = u3nc(42, 1);
    unsigned a = 0, b = 1;
    for ( unsigned depth = 0; depth < depth_limit; depth++ ) {
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
#if defined(U3_FOLD_STATS) && !defined(U3_MEMORY_DEBUG) && !defined(U3_CPU_DEBUG)
      extern c3_d u3n_fold_hits;
      c3_d before = u3n_fold_hits;
#endif
      double start = _seconds();
      u3_assert(u3n_nock_on(gate, u3k(fire)) == b);
      if ( timing ) printf("%u,%u,%.9f\n", table, direct, _seconds() - start);
#if defined(U3_FOLD_STATS) && !defined(U3_MEMORY_DEBUG) && !defined(U3_CPU_DEBUG)
      u3_assert(u3n_fold_hits > before);
#endif
    }
    u3z(tree);
  }
}

// This merge leaves the two-state domain on its first application.
static void
_fold_miss_bench(u3_noun bat, u3_noun fire)
{
  u3_noun gate = u3nt(u3k(bat), _fold_input(17, 42, 43), _fold_context(_lit(99)));
  double start = _seconds();
  u3_assert(u3n_nock_on(gate, u3k(fire)) == 99);
  fprintf(stderr, "unverified merge seconds=%.9f\n", _seconds() - start);
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
  _fold_tables(bat, fire, 14, 0);
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
