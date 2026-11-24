#ifdef U3_DECISION_STATS
static struct {
  uint64_t tables, hits, defaults;
} _n_index_stats;

static void
_n_report_tables(void)
{
  fprintf(stderr, "case tables=%" PRIu64 " matches=%" PRIu64 " defaults=%" PRIu64 "\n",
          _n_index_stats.tables, _n_index_stats.hits, _n_index_stats.defaults);
}
#endif

// This private HILB literal is emitted only by _n_comp_cases. A stock VM
// executes the ordinary hint and its cleanup, then walks the same case arms.
#define N_CASE_TAG c3_s4('j','m','p','s')

static inline c3_h
_n_bucket(c3_h key, c3_h mask)
{
  key ^= key >> 16;
  key *= 0x85ebca6bU;
  return (key ^ (key >> 13)) & mask;
}

static c3_h
_n_operand(const c3_y* p, c3_w width)
{
  c3_h val = 0;
  for ( c3_w i = 0; i < width; ++i ) val |= (c3_h)p[i] << (8 * i);
  return val;
}

// Index the first occurrence of each tag. A verified target starts with
// TOSS, which removes the fallback's selector before the arm or default.
static c3_w
_n_index_rows(u3n_prog* prog, c3_w pc, c3_w count, uint32_t mask,
              uint32_t* entries, uint32_t shape)
{
  c3_y* code = prog->byc_u.ops_y;
  c3_w end = prog->byc_u.len_w;
  if ( pc >= end ) return 0;
  for ( c3_w i = 0; i < count; ++i ) {
    if ( end - pc < 4 || code[pc++] != COPY ) return 0;
    c3_y op = code[pc++];
    u3_noun key;
    switch ( op ) {
      case SAM0: key = 0; break;
      case SAM1: key = 1; break;
      case SAMB: case SAMS: case SANB: case SANS: {
        c3_w width = (op == SAMB || op == SANB) ? 1 : 2;
        c3_h arg = _n_operand(code + pc, width);
        pc += width;
        if ( op == SANB || op == SANS ) {
          if ( arg >= prog->lit_u.len_w ) return 0;
          key = prog->lit_u.non[arg];
        }
        else key = arg;
        if ( key > 0x7fffffff ) return 0;
      } break;
      default: return 0;
    }
    if ( pc >= end ) return 0;
    op = code[pc++];
    if ( op < SBIN || op > SWIN ) return 0;
    c3_w width = 1U << (op - SBIN);
    if ( end - pc <= width ) return 0;
    c3_h delta = _n_operand(code + pc, width);
    pc += width;
    if ( code[pc] != TOSS || delta >= end - pc || pc >= UINT32_MAX ) return 0;
    uint32_t tag = (uint32_t)key | shape;
    uint32_t slot = _n_bucket(tag, mask);
    while ( entries[2*slot + 1] && entries[2*slot] != tag ) slot = (slot + 1) & mask;
    if ( !entries[2*slot + 1] ) {
      entries[2*slot] = tag;
      entries[2*slot + 1] = (uint32_t)pc;
    }
    pc += delta;
  }
  if ( code[pc] != TOSS || pc >= UINT32_MAX ) return 0;
  return pc;
}

// Decode the assembled fallback after widths and jump threading are final.
// An unrecognized shape keeps the ordinary linear code. Entries hold only
// immediate tags and byte offsets inside this same immutable program.
static u3_atom
_n_case_table(u3n_prog* prog, c3_w pc, c3_w count)
{
  c3_y* code = prog->byc_u.ops_y;
  c3_w end = prog->byc_u.len_w;
  if ( count < 4 || count > 128 || end - pc < 4 ||
       code[pc] != TOSS || code[pc+1] != SWAP || code[pc+2] != TOSS ) return 0;
  pc += 3;
  c3_h cap = 8;
  while ( cap < 2 * count ) cap *= 2;
  c3_h words[2 + 2 * 256 + 1] = {cap - 1};
  pc = _n_index_rows(prog, pc, count, cap - 1, words + 2, 0);
  if ( !pc ) return 0;
  words[1] = pc;
  // Preserve the complete zero-filled probe table in the atom.
  words[2 + 2*cap] = 1;
#ifdef U3_DECISION_STATS
  if ( !_n_index_stats.tables++ ) atexit(_n_report_tables);
#endif
  return u3i_halfs(3 + 2*cap, words);
}

static inline c3_w
_n_case_target(u3_atom table, u3_noun key)
{
  const c3_h* words = (const c3_h*)((u3a_atom*)u3a_to_ptr(table))->buf_w;
  if ( key <= 0x7fffffff ) {
    c3_h mask = words[0], slot = _n_bucket(key, mask);
    while ( words[3 + 2*slot] ) {
      if ( words[2 + 2*slot] == key ) {
#ifdef U3_DECISION_STATS
        ++_n_index_stats.hits;
#endif
        return words[3 + 2*slot];
      }
      slot = (slot + 1) & mask;
    }
  }
#ifdef U3_DECISION_STATS
  ++_n_index_stats.defaults;
#endif
  return words[1];
}
