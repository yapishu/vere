// A pure self-tail call replaces a pair below axis 6. Keep both fields
// borrowed from the original subject until the next bytecode iteration.
typedef struct {
  unsigned field;  // 0 for the left field, 1 for the right
  unsigned axis;   // projection within that field
  unsigned bumps;
} _n_tail_update;

// Return the path from parent to axis, or zero if parent is not a prefix.
// Both inputs are positive, direct axes no larger than 65535.
static unsigned
_n_tail_path(unsigned parent, unsigned axis)
{
  if ( parent > axis ) return 0;
  unsigned depth = u3x_dep(axis) - u3x_dep(parent);
  unsigned root = 1U << depth;
  return (axis >> depth) == parent ? root | (axis & (root - 1)) : 0;
}

static c3_t
_n_tail_update_read(u3_noun expr, unsigned pair, _n_tail_update* out)
{
  u3_noun arg;
  out->bumps = 0;
  while ( c3y == u3r_p(expr, 4, &arg) ) {
    if ( ++out->bumps > 16 ) return 0;
    expr = arg;
  }
  if ( c3n == u3r_p(expr, 0, &arg) || !arg || arg > 65535 ) return 0;
  unsigned path = _n_tail_path(pair, arg);
  if ( path < 2 ) return 0;
  unsigned bit = 1U << (u3x_dep(path) - 1);
  out->field = !!(path & bit);
  out->axis = bit | (path & (bit - 1));
  return 1;
}

static u3_weak
_n_tail_value(const _n_tail_update* op, const u3_noun fields[2])
{
  u3_noun val = fields[op->field];
  if ( op->axis != 1 ) val = u3r_at(op->axis, val);
  if ( op->bumps ) {
    // Only unboxed additions. Bytecode handles larger atoms and invalid bumps.
    if ( val > 0x7fffffffU - op->bumps ) return u3_none;
    val += op->bumps;
  }
  return val;
}

#ifdef U3_TAIL_STATS
static uint64_t _n_tail_batches, _n_tail_steps;
static void
_n_tail_report(void)
{
  fprintf(stderr, "tail batches=%" PRIu64 " steps=%" PRIu64 "\n",
          _n_tail_batches, _n_tail_steps);
}
#endif

typedef struct {
  unsigned pair, field;
  u3_noun compare;
  _n_tail_update left, right;
} _n_tail_plan;

static c3_t
_n_tail_read(u3_noun bat, _n_tail_plan* plan)
{
  u3_noun test, done, call, axis, edit, patch, subject;
  u3_noun pair, update, lhs, rhs, compare, source;
  if ( c3n == u3r_pqr(bat, 6, &test, &done, &call) ||
       c3n == u3r_pq(test, 5, &lhs, &rhs) ||
       c3n == u3r_pq(call, 9, &axis, &edit) || axis != 2 ||
       c3n == u3r_pq(edit, 10, &patch, &subject) ||
       c3n == u3r_p(subject, 0, &axis) || axis != 1 ||
       c3n == u3r_cell(patch, &pair, &update) || !pair || pair > 32767 ||
       !_n_tail_path(6, pair) || c3n == u3du(update) ) return 0;

  // Excluding calls from the exit expression gives this program one site.
  if ( c3n == u3r_p(done, 0, &axis) && c3n == u3r_p(done, 1, &axis) ) return 0;
  if ( c3y == u3r_p(lhs, 0, &axis) && (axis >> 1) == pair ) {
    plan->field = axis & 1;
    compare = rhs;
  }
  else if ( c3y == u3r_p(rhs, 0, &axis) && (axis >> 1) == pair ) {
    plan->field = axis & 1;
    compare = lhs;
  }
  else return 0;

  if ( c3y == u3r_p(compare, 0, &source) ) {
    if ( !source || source > 65535 ||
         _n_tail_path(pair, source) || _n_tail_path(source, pair) ) return 0;
  }
  else if ( c3n == u3r_p(compare, 1, &source) || c3n == u3a_is_cat(source) ) return 0;
  plan->pair = pair;
  plan->compare = compare;
  return _n_tail_update_read(u3h(update), pair, &plan->left) &&
         _n_tail_update_read(u3t(update), pair, &plan->right);
}

// The ordinary kick resolves before entry. Guard misses retain the original
// core or materialize the last complete iteration. Bytecode then handles
// the first unsupported step with its original input and bail behavior.
static __attribute__((noinline)) u3_noun
_n_tail_batch(u3_noun cor, u3j_site* site)
{
  _n_tail_plan plan;
  if ( c3n == u3du(cor) || site->axe != 2 || site->bat != u3h(cor) ||
       !_n_tail_read(u3h(cor), &plan) ) return cor;
  u3_noun source = u3t(plan.compare);
  u3_weak limit = u3h(plan.compare) ? source : u3r_at(source, cor);
  if ( c3n == u3a_is_cat(limit) ) return cor;
  u3_weak start = u3r_at(plan.pair, cor);
  if ( start == u3_none || c3n == u3du(start) ||
       c3n == u3j_site_unregistered(site) ) return cor;

  u3_noun fields[2] = {u3h(start), u3t(start)};
  unsigned steps = 0;
  while ( fields[plan.field] != limit && steps < 4096 ) {
    u3_weak x = _n_tail_value(&plan.left, fields);
    u3_weak y = _n_tail_value(&plan.right, fields);
    if ( x == u3_none || y == u3_none ) break;
    fields[0] = x;
    fields[1] = y;
    ++steps;
  }
  if ( !steps ) return cor;
#ifdef U3_TAIL_STATS
  if ( !_n_tail_batches++ ) atexit(_n_tail_report);
  _n_tail_steps += steps;
#endif
#ifdef U3_CPU_DEBUG
  u3R->pro.nox_d += steps;
#endif
  return u3i_edit(cor, plan.pair, u3nc(u3k(fields[0]), u3k(fields[1])));
}
