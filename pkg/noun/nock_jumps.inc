// Thread the forward edges of the compiler's instruction list before skips
// acquire byte widths. Every redirect preserves the complete operand stack.
typedef struct {
  u3_noun op;
  c3_w    cod, entry, fall, jump, pos;
  c3_t    live, bridge;
} _n_edge;

static c3_t
_n_index_hint(u3_noun op)
{
  if ( c3n == u3du(op) || u3h(op) != HILB ) return 0;
  u3_noun tag = u3h(u3t(op));
#ifdef U3_BORROW_BENCH
  if ( tag == N_BORROW_TAG ) return 1;
#endif
#ifdef U3_MIXED_BENCH
  if ( tag == N_MIXED_TAG ) return 1;
#endif
  return tag == N_CASE_TAG;
}

// Each index consumes an additional literal slot. A large program can use
// all 65536 slots for its ordinary instructions. Four neutral stack ops
// leave the linear dispatch and every instruction-count displacement intact.
static u3_noun
_n_unindex_large(u3_noun ops)
{
  u3_noun cur = ops, front = u3_nul;
  while ( cur != u3_nul ) {
    u3_noun op = u3h(cur);
    if ( _n_index_hint(op) ) break;
    cur = u3t(cur);
  }
  if ( cur == u3_nul ) return ops;
  for ( cur = ops; cur != u3_nul; cur = u3t(cur) ) {
    u3_noun op = u3h(cur);
    if ( _n_index_hint(op) ) {
      u3_assert(u3h(front) == TOSS && u3h(u3t(front)) == SWAP &&
                u3h(u3t(u3t(front))) == TOSS);
      u3_noun rest = u3k(u3t(u3t(u3t(front))));
      u3z(front);
      front = u3nq(COPY, TOSS, COPY, u3nc(TOSS, rest));
    }
    else front = u3nc(u3k(op), front);
  }
  u3z(ops);
  return u3kb_flop(front);
}

static u3_noun
_n_thread(u3_noun ops)
{
  c3_w len = u3qb_lent(ops);
  // Limit the compiler's scratch space independently of formula size.
  if ( len < 4 ) return ops;
  if ( len > 65535 ) return _n_unindex_large(ops);
  _n_edge* edges = u3a_calloc(len + 1, sizeof(*edges));
  u3_noun list = ops;
  edges[len].entry = len;
  c3_t folded = 0;

  // The source list is reversed. All branch destinations are already
  // resolved when we encounter their predecessors.
  for ( c3_w i = len; i-- > 0; list = u3t(list) ) {
    _n_edge* e = &edges[i];
    u3_noun op = e->op = u3h(list);
    e->cod = c3y == u3du(op) ? u3h(op) : op;
    e->entry = i;
    e->fall = edges[i + 1].entry;
    if ( e->cod == SBIP || e->cod == SBIN ) {
      e->jump = edges[i + 1 + u3t(op)].entry;
      if ( e->cod == SBIP ) e->entry = e->jump;
    }
    else if ( e->cod == SKIB || e->cod == SLIB ) {
      e->jump = edges[i + 1 + u3h(u3t(u3t(op)))].entry;
    }
    else if ( (e->cod == LIT0 || e->cod == LIT1) && edges[e->fall].cod == SBIN ) {
      // A pushed literal is consumed immediately by the boolean check.
      // LIL0 and LIL1 replace the subject and cannot use this rule.
      _n_edge* test = &edges[e->fall];
      e->entry = e->cod == LIT0 ? test->fall : test->jump;
      folded = 1;
    }
  }
  if ( !folded ) {
    u3a_free(edges);
    return ops;
  }

  edges[edges[0].entry].live = 1;
  for ( c3_w i = 0; i < len; i++ ) {
    _n_edge* e = &edges[i];
    if ( !e->live ) continue;
    edges[e->fall].live = 1;
    if ( e->cod == SBIN || e->cod == SKIB || e->cod == SLIB ) {
      edges[e->jump].live = 1;
    }
  }

  c3_w next = len;
  for ( c3_w i = len; i-- > 0; ) {
    if ( edges[i].live ) {
      edges[i].bridge = edges[i].fall != next;
      next = i;
    }
  }
  c3_w count = 0;
  for ( c3_w i = 0; i <= len; i++ ) {
    edges[i].pos = count;
    if ( edges[i].live ) count += 1 + edges[i].bridge;
  }

  u3_noun out = u3_nul;
  for ( c3_w i = 0; i < len; i++ ) {
    _n_edge* e = &edges[i];
    if ( !e->live ) continue;
    u3_noun op;
    if ( e->cod == SBIN || e->cod == SKIB || e->cod == SLIB ) {
      c3_w skip = edges[e->jump].pos - e->pos - 1;
      if ( e->cod == SBIN ) op = u3nc(SBIN, skip);
      else {
        u3_noun args = u3t(e->op);
        op = u3nq(e->cod, u3h(args), skip, u3k(u3t(u3t(args))));
      }
    }
    else op = u3k(e->op);
    out = u3nc(op, out);
    if ( e->bridge ) {
      out = u3nc(u3nc(SBIP, edges[e->fall].pos - e->pos - 2), out);
    }
  }
  u3a_free(edges);
  u3z(ops);
  return out;
}
