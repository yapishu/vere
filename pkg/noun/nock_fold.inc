// Recognize a tagged tree fold with a direct or curried binary merge.
// All fields are borrowed. The opaque arm can contain arbitrary Nock.
typedef struct {
  u3_noun leaf, branch, merge, base, outer, inner;
} _n_fold_plan;

static c3_t
_n_fold_slot(u3_noun fol, c3_w axis)
{
  u3_noun slot;
  return c3y == u3r_p(fol, 0, &slot) && slot == axis;
}

static c3_t
_n_fold_tag(u3_noun fol, u3_noun* tag)
{
  u3_noun left, right;
  return c3y == u3r_pq(fol, 5, &left, &right) &&
         c3y == u3r_p(left, 1, tag) && c3y == u3a_is_cat(*tag) &&
         _n_fold_slot(right, 12);
}

// [9 axis 10 [6 value] 0 subject]
static c3_t
_n_fold_call(u3_noun fol, u3_noun* axis, u3_noun* value, c3_w subject)
{
  u3_noun edit, patch, slot;
  return c3y == u3r_pq(fol, 9, axis, &edit) &&
         c3y == u3r_pq(edit, 10, &patch, &slot) && _n_fold_slot(slot, subject) &&
         c3y == u3r_p(patch, 6, value);
}

static c3_t
_n_fold_recur(u3_noun kick, c3_w child)
{
  u3_noun axis, value;
  return _n_fold_call(kick, &axis, &value, 1) && axis == 2 && _n_fold_slot(value, child);
}

static c3_t
_n_fold_child(u3_noun fol, c3_w child)
{
  u3_noun pop, kick;
  return c3y == u3r_pq(fol, 7, &pop, &kick) && _n_fold_slot(pop, 3) &&
         _n_fold_recur(kick, child);
}

static c3_t
_n_fold_read(u3_noun fol, _n_fold_plan* plan)
{
  u3_noun test, yes, no, excluded, hint, test2, body, first, right, make, left;
  u3_noun init, arg, axis, slot;
  if ( c3n == u3r_pqr(fol, 6, &test, &yes, &no) || !_n_fold_tag(test, &excluded) ) return 0;
  if ( _n_fold_slot(yes, 13) ) {
    plan->leaf = excluded;
    if ( c3n == u3r_pqr(no, 6, &test, &body, &no) || !_n_fold_tag(test, &plan->branch) ||
         (!_n_fold_slot(no, 0) && c3n == u3r_p(no, 1, NULL)) ) return 0;
  }
  else if ( c3n == u3r_pqr(no, 6, &test, &yes, &no) || !_n_fold_tag(test, &plan->leaf) ||
            !_n_fold_slot(yes, 13) || c3n == u3r_pq(no, 11, &hint, &body) ||
            c3n == u3r_p(hint, c3_s4('t','o','s','s'), &test2) || !_n_fold_tag(test2, &plan->branch) ||
            excluded == plan->leaf || excluded == plan->branch ) return 0;
  if ( plan->leaf == plan->branch ) return 0;
  if ( c3y == u3r_pq(body, 7, &arg, &plan->merge) ) {
    return c3y == u3r_cell(arg, &left, &right) &&
           _n_fold_recur(left, 26) && _n_fold_recur(right, 27);
  }
  plan->merge = u3_none;
  if ( c3n == u3r_pq(body, 8, &first, &right) ||
       !_n_fold_call(right, &axis, &arg, 2) || axis != 2 || !_n_fold_child(arg, 27) ||
       c3n == u3r_pq(first, 8, &make, &left) ||
       !_n_fold_call(left, &plan->inner, &arg, 2) || !_n_fold_child(arg, 26) ||
       c3n == u3r_pq(make, 9, &plan->outer, &init) ||
       c3n == u3r_p(init, 0, &slot) || slot < 7 || slot > 65535 ) return 0;
  // Only axes in the battery compile to kick sites. The remaining five
  // calls are numbered backwards from the end of the curried merge.
  if ( plan->outer < 2 || plan->outer > 65535 || plan->inner < 2 || plan->inner > 65535 ||
       u3qc_cap(plan->inner) != 2 || u3qc_cap(plan->outer) != 2 ) return 0;
  // Every descent changes only axis 6. The factory is under axis 7.
  plan->base = slot;
  while ( slot > 7 ) slot >>= 1;
  return slot == 7;
}

#ifdef U3_FOLD_STATS
c3_d u3n_fold_hits;
static void
_n_fold_report(void)
{
  fprintf(stderr, "fold reductions=%" PRIu64 "\n", u3n_fold_hits);
}
#endif

// A speculative subject may lack a parent. Check each step before following
// it. A changing sample also requires a first parent independent of axis 6.
static c3_t
_n_fold_fine(u3p(u3j_fink) pos, u3_noun core, c3_t variant)
{
  if ( !pos ) return 0;
  const u3j_fink* fine = u3to(u3j_fink, pos);
  if ( variant && (!fine->len_w || fine->fis_u[0].pax != 7) ) return 0;
  for ( c3_w i = 0; i < fine->len_w; i++ ) {
    if ( c3n == u3du(core) || c3n == u3r_sing(u3h(core), fine->fis_u[i].bat) ) return 0;
    core = u3r_at(fine->fis_u[i].pax, core);
    if ( core == u3_none ) return 0;
  }
  return c3y == u3r_sing(core, fine->sat);
}

static c3_t
_n_fold_soft(const u3j_site* site, u3_noun core, c3_t variant)
{
  if ( !site->pog_p || c3n == u3du(core) || site->bat == u3_none ||
       c3n == u3r_sing(site->bat, u3h(core)) ) return 0;
  if ( site->loc == u3_none ) return c3y == u3j_site_unregistered(site);
  return site->jet_o == c3n && _n_fold_fine(site->fin_p, core, variant);
}

// Evaluate just a literal gate constructor and check that its optional
// registration is the no-op already established at that exact program site.
static u3_weak
_n_fold_factory(const u3j_site* site, u3_noun core)
{
  if ( !_n_fold_soft(site, core, 0) ) return u3_none;
  u3_noun fol = u3r_at(site->axe, core), make, tail, hint, clue = u3_none;
  if ( fol == u3_none ) return u3_none;
  if ( c3y == u3r_pq(fol, 7, &make, &tail) ) {
    u3_noun body, literal;
    if ( c3n == u3r_pq(tail, 11, &hint, &body) || !_n_fold_slot(body, 1) ||
         c3n == u3r_p(hint, c3__fast, &literal) || c3n == u3r_p(literal, 1, &clue) ) return u3_none;
    fol = make;
  }
  u3_noun sample, pair, lit, slot, bat, val;
  if ( c3n == u3r_pq(fol, 8, &sample, &pair) ||
       c3n == u3r_cell(pair, &lit, &slot) || !_n_fold_slot(slot, 1) ||
       c3n == u3r_p(lit, 1, &bat) ) return u3_none;
  if ( c3n == u3r_p(sample, 1, &val) ) {
    if ( !_n_fold_slot(sample, 6) || (val = u3r_at(6, core)) == u3_none ) return u3_none;
  }
  u3_noun gate = u3nt(u3k(bat), u3k(val), u3k(core));
  if ( clue != u3_none ) {
    const u3n_prog* arm = u3to(u3n_prog, site->pog_p);
    if ( arm->reg_u.len_w != 1 || arm->reg_u.rit_u->clu == u3_none ||
         c3n == u3r_sing(arm->reg_u.rit_u->clu, clue) ||
         !_n_fold_fine(arm->reg_u.rit_u->fin_p, gate, 0) ) {
      u3z(gate);
      return u3_none;
    }
  }
  return gate;
}

// Scratch borrows from a single live input for the duration of a scan.
// Incrementing the generation makes all references from older scans inert.
#define FOLD_SLOTS 8192
static struct { u3_noun node; c3_w generation; uint16_t choices; } _n_fold_seen[FOLD_SLOTS];
static c3_w _n_fold_generation;

// A rejected merge can recur at every node. These integer identities only
// throttle probes. Reuse of a loom address can postpone a fresh probe.
static struct {
  u3_noun battery, context;
  unsigned remaining;
} _n_fold_retry;

typedef struct {
  const _n_fold_plan* plan;
  u3_noun witness;
  c3_w count;
} _n_fold_scan;

// Bit t is the answer for binary truth table t. In that table, bits
// 0..3 give the products for (0,0), (0,1), (1,0), and (1,1).
static int
_n_fold_walk(_n_fold_scan* scan, u3_noun node, c3_w depth)
{
  u3_noun tag, data, left, right;
  if ( depth > 192 || c3n == u3r_cell(node, &tag, &data) ) return -1;
  if ( tag == scan->plan->leaf ) {
    if ( !data ) return 0;
    if ( scan->witness && scan->witness != data ) return -1;
    scan->witness = data;
    return UINT16_MAX;
  }
  if ( tag != scan->plan->branch || c3n == u3r_cell(data, &left, &right) ) return -1;
  c3_w i = (c3_w)vt_hash_integer(node) & (FOLD_SLOTS - 1);
  while ( _n_fold_seen[i].generation == _n_fold_generation ) {
    if ( _n_fold_seen[i].node == node ) return _n_fold_seen[i].choices;
    i = (i + 1) & (FOLD_SLOTS - 1);
  }
  if ( ++scan->count > FOLD_SLOTS / 2 ) return -1;
  _n_fold_seen[i].node = node;
  _n_fold_seen[i].generation = _n_fold_generation;
  int l = _n_fold_walk(scan, left, depth + 1);
  if ( l < 0 ) return -1;
  int r = _n_fold_walk(scan, right, depth + 1);
  if ( r < 0 ) return -1;
  uint16_t choices = (~l & ~r & 0xaaaa) | (~l & r & 0xcccc) |
                     ( l & ~r & 0xf0f0) | ( l & r & 0xff00);
  _n_fold_seen[i].choices = choices;
  return choices;
}

// Evaluate a bounded, effect-free path in an interpreted merge. Unequal
// indirect references can denote equal nouns, so that comparison defers.
static u3_weak
_n_fold_value(u3_noun fol, u3_noun core, unsigned* budget)
{
  u3_noun op, arg, lhs, rhs;
  if ( !*budget || c3n == u3r_cell(fol, &op, &arg) ) return u3_none;
  --*budget;
  switch ( op ) {
    case 0: return c3y == u3a_is_cat(arg) && arg ? u3r_at(arg, core) : u3_none;
    case 1: return arg;
    case 5: {
      if ( c3n == u3r_cell(arg, &lhs, &rhs) ) return u3_none;
      u3_weak a = _n_fold_value(lhs, core, budget);
      u3_weak b = _n_fold_value(rhs, core, budget);
      if ( a == u3_none || b == u3_none ) return u3_none;
      if ( a == b ) return 0;
      return c3y == u3a_is_cat(a) || c3y == u3a_is_cat(b) ? 1 : u3_none;
    }
    case 6: {
      u3_noun test;
      if ( c3n == u3r_trel(arg, &test, &lhs, &rhs) ) return u3_none;
      u3_weak b = _n_fold_value(test, core, budget);
      return b <= 1 ? _n_fold_value(b ? rhs : lhs, core, budget) : u3_none;
    }
    default: return u3_none;
  }
}

static u3_weak
_n_fold_merge(const u3j_site* site, u3_noun core)
{
  if ( site->loc != u3_none && site->jet_o == c3y && site->ham_u &&
       site->ham_u->fun_f == u3wdi_uni && site->ham_u->liv == c3y &&
       site->ham_u->ice == c3y && _n_fold_fine(site->fin_p, core, 0) ) {
    u3_weak a = u3r_at(30, core), b = u3r_at(6, core);
    if ( a == u3_none || b == u3_none ) return u3_none;
    return (!a || a == b) ? b : (!b ? a : u3_none);
  }
  unsigned budget = 32;
  return _n_fold_soft(site, core, 0) ? _n_fold_value(u3h(core), core, &budget) : u3_none;
}

static int
_n_fold_prove(const u3j_site* sites, u3_noun seed, u3_noun witness)
{
  u3_weak outer = _n_fold_factory(sites + 4, seed);
  if ( outer == u3_none ) return -1;
  c3_t ok = 1;
  unsigned table = 0;
  for ( unsigned i = 0; ok && i < 2; i++ ) {
    u3_noun a = i ? witness : 0;
    u3_noun context = u3nt(u3k(u3h(outer)), u3k(a), u3k(u3t(u3t(outer))));
    u3_weak inner = _n_fold_factory(sites + 2, context);
    u3z(context);
    if ( inner == u3_none ) { ok = 0; break; }
    for ( unsigned j = 0; ok && j < 2; j++ ) {
      u3_noun b = j ? witness : 0;
      u3_noun gate = u3nt(u3k(u3h(inner)), u3k(b), u3k(u3t(u3t(inner))));
      u3_weak result = _n_fold_merge(sites, gate);
      if ( result == witness ) table |= 1U << (2 * i + j);
      else if ( result != 0 ) ok = 0;
      u3z(gate);
    }
    u3z(inner);
  }
  u3z(outer);
  return ok ? (int)table : -1;
}

static int
_n_fold_direct(u3_noun merge, u3_noun witness)
{
  unsigned table = 0;
  for ( unsigned i = 0; i < 4; i++ ) {
    u3_noun pair = u3nc(i & 2 ? u3k(witness) : 0, i & 1 ? u3k(witness) : 0);
    unsigned budget = 32;
    u3_weak result = _n_fold_value(merge, pair, &budget);
    c3_t valid = result == 0 || result == witness;
    if ( result == witness ) table |= 1U << i;
    u3z(pair);
    if ( !valid ) return -1;
  }
  return (int)table;
}

static u3_weak
_n_fold_try(u3_noun core, const u3n_prog* arm)
{
  _n_fold_plan plan;
  u3_noun bat, tree, context, tag, kids, head, tail;
  if ( (u3C.wag_h & (u3o_debug_cpu | u3o_trace | u3o_debug_ram)) ||
       c3n == u3r_trel(core, &bat, &tree, &context) ||
       c3n == u3r_cell(tree, &tag, &kids) || c3n == u3r_cell(kids, &head, &tail) ||
       c3n == u3du(head) || c3n == u3du(tail) || (u3h(head) != tag && u3h(tail) != tag) ) return u3_none;
  if ( _n_fold_retry.remaining && _n_fold_retry.battery == bat && _n_fold_retry.context == context ) {
    --_n_fold_retry.remaining;
    return u3_none;
  }
  if ( !_n_fold_read(bat, &plan) || tag != plan.branch ) return u3_none;

  if ( !++_n_fold_generation ) {
    memset(_n_fold_seen, 0, sizeof(_n_fold_seen));
    ++_n_fold_generation;
  }
  _n_fold_scan scan = { .plan = &plan };
  int choices = _n_fold_walk(&scan, tree, 0);
  if ( choices < 0 || scan.count < 3 ) return u3_none;
  const u3j_site* sites = arm->cal_u.sit_u;
  const u3j_site *left, *right;
  if ( plan.merge != u3_none ) {
    if ( arm->cal_u.len_w != 2 ) return u3_none;
    right = sites;
    left = sites + 1;
  }
  else {
    if ( arm->cal_u.len_w < 5 || sites[0].axe != 2 ||
         sites[2].axe != plan.inner || sites[4].axe != plan.outer ) return u3_none;
    right = sites + 1;
    left = sites + 3;
  }
  if ( left->axe != 2 || right->axe != 2 ||
       left->pog_p != u3of(u3n_prog, arm) || right->pog_p != left->pog_p ) return u3_none;
  // Fine checks can unify equal subnouns. Pin both borrowed values across
  // those checks independently of their positions in the tree and battery.
  u3_noun witness = u3k(scan.witness ? scan.witness : 1);
  if ( plan.merge != u3_none ) u3k(plan.merge);
  int table = -1;
  if ( _n_fold_soft(left, core, 1) && _n_fold_soft(right, core, 1) ) {
    if ( plan.merge != u3_none ) table = _n_fold_direct(plan.merge, witness);
    else {
      u3_weak seed = u3r_at(plan.base, core);
      if ( seed != u3_none ) table = _n_fold_prove(sites, seed, witness);
    }
  }
  if ( plan.merge != u3_none ) u3z(plan.merge);
  if ( table < 0 ) {
    _n_fold_retry.battery = bat;
    _n_fold_retry.context = context;
    _n_fold_retry.remaining = 64;
    u3z(witness);
    return u3_none;
  }
#ifdef U3_FOLD_STATS
  if ( !u3n_fold_hits++ ) atexit(_n_fold_report);
#endif
  if ( choices & (1U << table) ) return witness;
  u3z(witness);
  return 0;
}
