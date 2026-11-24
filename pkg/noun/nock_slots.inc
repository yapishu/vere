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
