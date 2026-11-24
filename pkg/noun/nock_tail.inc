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

#ifdef U3_TAIL_STATS
static uint64_t _n_tail_batches, _n_tail_steps;
static void
_n_tail_report(void)
{
  fprintf(stderr, "tail batches=%" PRIu64 " steps=%" PRIu64 "\n",
          _n_tail_batches, _n_tail_steps);
}
#endif

static inline void
_n_tail_count(c3_d steps)
{
#ifdef U3_TAIL_STATS
  if ( !_n_tail_batches++ ) atexit(_n_tail_report);
  _n_tail_steps += steps;
#endif
#ifdef U3_CPU_DEBUG
  u3R->pro.nox_d += steps;
#endif
}

typedef struct {
  unsigned axis;
  u3_noun compare;
  c3_t successor;
} _n_tail_plan;

// Match a slot or its successor without traversing or unifying the formula.
static c3_t
_n_tail_counter_test(u3_noun test, unsigned axis, _n_tail_plan* plan)
{
  u3_noun slot;
  c3_t successor = c3y == u3r_p(test, 4, &slot);
  u3_noun source;
  if ( c3n == u3r_p(successor ? slot : test, 0, &source) || source != axis ) {
    return 0;
  }
  plan->successor = successor;
  return 1;
}

static c3_t
_n_tail_read(u3_noun bat, _n_tail_plan* plan)
{
  u3_noun test, done, call, axis, edit, patch, subject;
  u3_noun edited, update, lhs, rhs, compare, source;
  if ( c3n == u3r_pqr(bat, 6, &test, &done, &call) ||
       c3n == u3r_pq(test, 5, &lhs, &rhs) ||
       c3n == u3r_pq(call, 9, &axis, &edit) || axis != 2 ||
       c3n == u3r_pq(edit, 10, &patch, &subject) ||
       c3n == u3r_p(subject, 0, &axis) || axis != 1 ||
       c3n == u3r_cell(patch, &edited, &update) || !edited || edited > 32767 ||
       !_n_tail_path(6, edited) || c3n == u3du(update) ) return 0;

  // Excluding calls from the exit expression gives this program one site.
  if ( c3n == u3r_p(done, 0, &axis) && c3n == u3r_p(done, 1, &axis) ) return 0;

  // Increment one counter until it or its successor equals a fixed bound.
  u3_noun slot, counter;
  if ( c3n == u3r_p(update, 4, &slot) ||
       c3n == u3r_p(slot, 0, &counter) || counter != edited ) return 0;
  if ( _n_tail_counter_test(lhs, edited, plan) ) compare = rhs;
  else if ( _n_tail_counter_test(rhs, edited, plan) ) compare = lhs;
  else return 0;

  if ( c3y == u3r_p(compare, 0, &source) ) {
    if ( !source || source > 65535 ||
         _n_tail_path(edited, source) || _n_tail_path(source, edited) ) return 0;
  }
  else if ( c3n == u3r_p(compare, 1, &source) || c3n == u3a_is_cat(source) ) return 0;
  plan->axis = edited;
  plan->compare = compare;
  return 1;
}

// The ordinary kick resolves before entry. Guard misses return the original core.
static __attribute__((noinline)) u3_noun
_n_tail_batch(u3_noun cor, u3j_site* site)
{
  _n_tail_plan plan;
  if ( c3n == u3du(cor) || site->axe != 2 || site->bat != u3h(cor) ||
       !_n_tail_read(u3h(cor), &plan) ) return cor;
  u3_noun source = u3t(plan.compare);
  u3_weak limit = u3h(plan.compare) ? source : u3r_at(source, cor);
  if ( c3n == u3a_is_cat(limit) ) return cor;
  u3_weak start = u3r_at(plan.axis, cor);
  if ( c3n == u3a_is_cat(start) || limit <= start ) return cor;
  u3_noun stop = limit - plan.successor;
  if ( stop == start || c3n == u3j_site_unregistered(site) ) return cor;
  _n_tail_count(stop - start);
  return u3i_edit(cor, plan.axis, stop);
}
