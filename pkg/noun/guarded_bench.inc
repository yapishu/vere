// Included only in the guarded-product benchmark build.
// HILB/HILL already implement a guarded skip with a before/after token.
#define GUARD_TAG c3_s4('g','p','r','d')
#define CHECK_TAG c3_s4('g','c','h','k')

static struct {
  int mode;  // 0 uninitialized, 1 off, 2 shadow, 3 use
  unsigned min_nodes;
  uint64_t sites, probes, hits, checked, changed;
} guard_stats;

static void
_guard_report(void)
{
  fprintf(stderr, "guard sites=%llu probes=%llu hits=%llu checked=%llu changed=%llu\n",
          (unsigned long long)guard_stats.sites, (unsigned long long)guard_stats.probes,
          (unsigned long long)guard_stats.hits, (unsigned long long)guard_stats.checked,
          (unsigned long long)guard_stats.changed);
}

static int
_guard_mode(void)
{
  if ( !guard_stats.mode ) {
    const char* mode = getenv("U3_GUARD");
    guard_stats.mode = mode && !strcmp(mode, "shadow") ? 2
                     : mode && !strcmp(mode, "use") ? 3 : 1;
    guard_stats.min_nodes = 3;
    const char* minimum = getenv("U3_GUARD_MIN_NODES");
    if ( minimum && *minimum ) {
      char* end;
      unsigned long n = strtoul(minimum, &end, 10);
      if ( *end || n < 3 || n > 256 ) {
        fprintf(stderr, "U3_GUARD_MIN_NODES must be an integer from 3 through 256\n");
        abort();
      }
      guard_stats.min_nodes = n;
    }
    if ( guard_stats.mode > 1 ) atexit(_guard_report);
  }
  return guard_stats.mode;
}

// Collect a common ancestor of every subject read, including reads whose
// values are discarded.  Inside a closed subcomputation only purity matters.
static c3_t
_guard_reads(u3_noun fol, c3_w* axis, unsigned* budget, c3_t self)
{
  u3_noun op, arg, a, b, c;
  if ( !*budget || c3n == u3r_cell(fol, &op, &arg) ) return 0;
  --*budget;
  if ( c3y == u3du(op) ) {
    return _guard_reads(op, axis, budget, self) && _guard_reads(arg, axis, budget, self);
  }
  switch ( op ) {
    case 0: {
      if ( !arg || arg > 0x7fffffff ) return 0;
      if ( axis ) {
        unsigned x = *axis, y = arg;
        if ( x ) {
          while ( __builtin_clz(x) < __builtin_clz(y) ) x >>= 1;
          while ( __builtin_clz(y) < __builtin_clz(x) ) y >>= 1;
          while ( x != y ) { x >>= 1; y >>= 1; }
        }
        *axis = x ? x : y;
      }
      return 1;
    }
    case 1: return 1;
    case 3: case 4: return _guard_reads(arg, axis, budget, self);
    case 5: return _guard_reads(arg, axis, budget, self); // both operands, as autocons
    case 6:
      return c3y == u3r_trel(arg, &a, &b, &c)
          && _guard_reads(a, axis, budget, self)
          && _guard_reads(b, axis, budget, self)
          && _guard_reads(c, axis, budget, self);
    case 7:
      return c3y == u3r_cell(arg, &a, &b) && _guard_reads(a, axis, budget, self)
          && _guard_reads(b, NULL, budget, 0);
    case 2: {
      // Apply a quoted battery to [that battery, payload]. Within its body
      // a recursive eval may preserve the battery at axis 2 and replace the
      // payload. All other indirect calls (and every jet/hint/scry) stop analysis.
      u3_noun core, target, bat, payload, literal, code;
      if ( c3n == u3r_cell(arg, &core, &target)
        || c3n == u3r_cell(core, &bat, &payload) ) return 0;
      if ( self && c3y == u3r_p(bat, 0, &a) && a == 2
                && c3y == u3r_p(target, 0, &a) && a == 2 ) {
        return _guard_reads(payload, axis, budget, self);
      }
      return c3y == u3r_p(bat, 1, &literal) && c3y == u3r_p(target, 1, &code)
          && literal == code && _guard_reads(payload, axis, budget, self)
          && _guard_reads(code, NULL, budget, 1);
    }
    default: return 0;
  }
}

// key = [private-tag [side guard-axis hole-axis formula]]
static u3_weak
_guard_plan(u3_noun fol)
{
  u3_noun hole, work = u3h(fol);
  u3_noun side = 0;
  if ( c3n == u3r_p(u3t(fol), 0, &hole) ) {
    side = 1;
    if ( c3n == u3r_p(work, 0, &hole) ) return u3_none;
    work = u3t(fol);
  }
  if ( !hole || hole > 0x7fffffff ) return u3_none;
  c3_w axis = 0;
  unsigned budget = 256;
  if ( !_guard_reads(work, &axis, &budget, 0) || axis < 2
    || 256 - budget < guard_stats.min_nodes ) {
    return u3_none;
  }
  ++guard_stats.sites;
  return u3nc(GUARD_TAG, u3nq(side, axis, hole, u3k(fol)));
}

// Borrow the entry from this road's transient cache. A failed slot or an
// address mismatch takes the ordinary path. No mug or deep guard comparison.
static c3_o
_guard_fore(u3_noun key, u3_noun bus, u3_noun* token)
{
  u3_noun side, axis, slot, fol;
  u3x_qual(u3t(key), &side, &axis, &slot, &fol);
  ++guard_stats.probes;
  u3_weak input = u3r_at(axis, bus), hole = u3r_at(slot, bus);
  if ( _guard_mode() == 1 || input == u3_none || hole == u3_none ) {
    u3z(key);
    *token = u3_nul;
    return c3y;
  }
  u3_weak entry = u3h_git(u3R->cax.har_p, key);
  if ( entry != u3_none && input == u3h(entry) ) {
    ++guard_stats.hits;
    u3_noun part = u3k(u3h(u3t(entry)));
    u3_noun result = side ? u3nc(u3k(hole), part) : u3nc(part, u3k(hole));
    if ( guard_stats.mode == 2 ) {
      if ( c3n == u3r_sing(hole, u3t(u3t(entry))) ) ++guard_stats.changed;
      *token = u3nc(CHECK_TAG, result);
      u3z(key);
      return c3y;
    }
    *token = result;
    u3z(key);
    return c3n;
  }
  *token = u3nq(GUARD_TAG, key, u3k(input), guard_stats.mode == 2 ? u3k(hole) : 0);
  return c3y;
}

static void
_guard_save(u3_noun token, u3_noun pro)
{
  u3_noun tag, key, input, hole;
  u3x_qual(token, &tag, &key, &input, &hole);
  u3_noun part = u3h(u3t(key)) ? u3t(pro) : u3h(pro);
  u3h_put(u3R->cax.har_p, key, u3nt(u3k(input), u3k(part), u3k(hole)));
}
