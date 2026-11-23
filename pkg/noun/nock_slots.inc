// Axes in the byte and short fragment instructions are positive direct atoms.
// Traverse three direction bits per dispatch. Constant field offsets keep
// the next cell address independent of shifts on the critical load chain.
static inline u3_noun
_n_step(u3_noun b, c3_w bit)
{
  u3a_cell* cell = u3a_to_ptr(_n_kale(b));
  return bit ? cell->tel : cell->hed;
}

static inline u3_noun
_n_path3(u3_noun a, u3_noun b)
{
#ifdef U3_SPINE_BENCH
  // A right-nested subject puts a field at a run of tails followed by a
  // short suffix. Select the entire path once. Each case falls through
  // to the same suffix with one fewer tail left to read.
#define EACH(F, d) F(d,0) F(d,1) F(d,2) F(d,3) F(d,4) F(d,5) F(d,6) F(d,7) \
                   F(d,8) F(d,9) F(d,10) F(d,11) F(d,12) F(d,13) F(d,14) F(d,15)
#define CODE(d,s) (16u + 16u * (d) + (s))
#define AXIS(d,s) ((((2u << (d)) - 1u) << 4) | (s))
#define ID(d,s) [AXIS(d,s)] = CODE(d,s),
  // A zero entry falls back to the ordinary path walker. This byte is the
  // number of leading tails and the four low direction bits.
  static const c3_y recipes[65536] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    EACH(ID,0) EACH(ID,1) EACH(ID,2) EACH(ID,3) EACH(ID,4) EACH(ID,5)
    EACH(ID,6) EACH(ID,7) EACH(ID,8) EACH(ID,9) EACH(ID,10) EACH(ID,11)
  };
#undef ID
#undef AXIS
#undef EACH
#define STEP(bit) do { b = _n_step(b, (bit)); } while (0)
#define LAST2(s) STEP((s) & 2); STEP((s) & 1)
#define LAST4(s) STEP((s) & 8); STEP((s) & 4); LAST2(s)
#define TAIL(n, s) case CODE(n,s): STEP(1); \
                   __attribute__((fallthrough))
#define PATH(s) TAIL(11,s); TAIL(10,s); TAIL(9,s); TAIL(8,s); \
                TAIL(7,s);  TAIL(6,s);  TAIL(5,s); TAIL(4,s); \
                TAIL(3,s);  TAIL(2,s);  TAIL(1,s);            \
                case 16+(s): LAST4(s); return b
#define PAIR(s) case 4+(s): LAST2(s); return b
#define TRIPLE(s) case 8+(s): STEP((s) & 4); LAST2(s); return b
  switch ( a <= 65535 ? recipes[a] : 0 ) {
    case 1: return b;
    case 2: STEP(0); return b;
    case 3: STEP(1); return b;
    PAIR(0); PAIR(1); PAIR(2); PAIR(3);
    TRIPLE(0); TRIPLE(1); TRIPLE(2); TRIPLE(3);
    TRIPLE(4); TRIPLE(5); TRIPLE(6); TRIPLE(7);
    PATH(0);  PATH(1);  PATH(2);  PATH(3);
    PATH(4);  PATH(5);  PATH(6);  PATH(7);
    PATH(8);  PATH(9);  PATH(10); PATH(11);
    PATH(12); PATH(13); PATH(14); PATH(15);
  }
#undef TRIPLE
#undef PAIR
#undef PATH
#undef TAIL
#undef LAST4
#undef LAST2
#undef STEP
#undef CODE
#endif

#define STEP(B, K) _n_step((B), (K))
#define TWO(B, K) STEP(STEP(B, ((K) >> 1) & 1), (K) & 1)
#define THREE(B, K) TWO(STEP(B, ((K) >> 2) & 1), (K))
  c3_w depth = u3x_dep(a);
  c3_w rem = depth % 3;
  depth -= rem;
  switch ( a >> depth ) {
    case 2: b = STEP(b, 0); break;
    case 3: b = STEP(b, 1); break;
    case 4: b = TWO(b, 0); break;
    case 5: b = TWO(b, 1); break;
    case 6: b = TWO(b, 2); break;
    case 7: b = TWO(b, 3); break;
  }
  while ( depth ) {
    depth -= 3;
    switch ( (a >> depth) & 7 ) {
#define PATH(K) case K: b = THREE(b, K); break
      PATH(0); PATH(1); PATH(2); PATH(3);
      PATH(4); PATH(5); PATH(6); PATH(7);
#undef PATH
    }
  }
#undef THREE
#undef TWO
#undef STEP
  return b;
}


#ifdef U3_MIXED_BENCH
// The subject on the VM stack owns the borrowed selector. Both the matched
// arm and the default start after the TOSS that discards the fallback's copy.
static inline c3_w
_n_mixed_target(u3_atom table, u3_noun subject)
{
  const uint32_t* words = (const uint32_t*)((u3a_atom*)u3a_to_ptr(table))->buf_w;
  u3_noun val = _n_path3(words[1], subject);
  unsigned cell = c3y == u3du(val);
  u3_noun tag = cell ? u3h(val) : val;
#ifdef U3_MIXED_STATS
  _n_start_mixed_stats();
  if ( cell ) ++_n_mixed_cells;
  else ++_n_mixed_atoms;
#endif
  if ( tag <= 0x7fffffff ) {
    uint32_t key = (uint32_t)tag | ((uint32_t)cell << 31);
    uint32_t mask = words[0], slot = _n_bucket(key, mask);
    while ( words[5 + 2*slot] ) {
      if ( words[4 + 2*slot] == key ) return words[5 + 2*slot] + 1;
      slot = (slot + 1) & mask;
    }
  }
  return words[2 + cell] + 1;
}
#endif

#ifdef U3_BORROW_BENCH
#ifdef U3_BORROW_STATS
static uint64_t _n_borrow_calls[7], _n_borrow_pairs, _n_borrow_heap;
static c3_t _n_borrow_reporting;
static void
_n_borrow_report(void)
{
  fprintf(stderr, "borrow eq=%" PRIu64 " pair=%" PRIu64 " head=%" PRIu64
          " tail=%" PRIu64 " sample=%" PRIu64 " pairs=%" PRIu64 " heap=%" PRIu64 "\n",
          _n_borrow_calls[0], _n_borrow_calls[1], _n_borrow_calls[2],
          _n_borrow_calls[3], _n_borrow_calls[6], _n_borrow_pairs, _n_borrow_heap);
}
static void
_n_borrow_count(u3_noun noun)
{
  ++_n_borrow_pairs;
  if ( c3n == u3a_is_cat(noun) ) ++_n_borrow_heap;
}
#endif

static inline u3_noun
_n_borrow_run(const c3_h* plan, u3_noun subject)
{
  c3_w kind = plan[1] & ~N_BORROW_KEEP;
#ifdef U3_BORROW_STATS
  if ( !_n_borrow_reporting ) { atexit(_n_borrow_report); _n_borrow_reporting = 1; }
  ++_n_borrow_calls[kind];
#endif
  u3_noun base = _n_path3(plan[2], subject);
  u3_noun root = kind == 1 ? base : subject;
  u3_noun left = _n_path3(plan[3], root);
  if ( kind == 0 ) {
    // Structural equality can unify children. Pin both operands across that
    // call, including when one is reachable through the other.
    if ( base != left && c3n == u3a_is_cat(base) && c3n == u3a_is_cat(left) ) {
      u3k(base); u3k(left);
      u3_noun same = u3r_sing(left, base);
      u3z(left); u3z(base);
      return same;
    }
#ifdef U3_BORROW_STATS
    _n_borrow_count(base);
    _n_borrow_count(left);
#endif
    return __(base == left);
  }
#ifdef U3_BORROW_STATS
  _n_borrow_count(base);
#endif
  u3_noun value;
  if ( plan[4] ) {
    u3_noun right = _n_path3(plan[4], root);
    value = u3nc(u3k(left), u3k(right));
  }
  else value = u3k(left);
  switch ( kind ) {
    case 1: return value;
    case 2: return u3nc(value, u3k(u3t(base)));
    case 3: return u3nc(u3k(u3h(base)), value);
    default: return u3nt(u3k(u3h(base)), value, u3k(u3t(u3t(base))));
  }
}
#endif
