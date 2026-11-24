// A run of arms compares one slot with successive literal tags. An optional
// shape check guards that comparison in every arm.
typedef struct {
  u3_noun guard, slot, tag, arm, rest;
} _n_case;

static c3_t
_n_read_case(u3_noun fol, _n_case* row)
{
  u3_noun test, left, right, axis;
  if ( c3n == u3r_pqr(fol, 6, &test, &row->arm, &row->rest) ) return 0;
  row->guard = u3_none;
  if ( c3y == u3r_pqr(test, 6, &row->guard, &left, &right) ) {
    u3_noun shape, val;
    if ( c3n == u3r_p(row->guard, 3, &shape)
      || c3n == u3r_p(shape, 0, &axis) || c3n == u3ud(axis)
      || c3n == u3r_p(right, 1, &val) || val != 1 ) return 0;
    test = left;
  }
  if ( c3n == u3r_pq(test, 5, &left, &right) ) return 0;
  if ( c3y == u3r_p(left, 1, &row->tag) ) row->slot = right;
  else if ( c3y == u3r_p(right, 1, &row->tag) ) row->slot = left;
  else return 0;
  return c3y == u3r_p(row->slot, 0, &axis) && c3y == u3ud(axis) && axis;
}

// Emit one comparison block. Every entry owns a selector above the subject.
// A match drops it and skips the rest of this decision after its arm returns.
// A miss drops it and goes to the shared default.
static c3_w
_n_case_block(u3_noun* ops, u3_noun rows, c3_w len, c3_t direct,
              c3_o los_o, c3_o tel_o, c3_w miss_skip, c3_w after)
{
  u3_noun miss = u3nc(TOSS, u3_nul);
  if ( miss_skip ) _n_emit(&miss, u3nc(SBIP, miss_skip));
  u3_noun parts = u3nc(miss, u3_nul);
  c3_w size = miss_skip ? 2 : 1;
  while ( u3_nul != rows ) {
    u3_noun item = u3h(rows), test = u3nt(5, u3nc(0, 1), u3nc(1, u3k(u3h(item))));
    u3_noun part = u3_nul, arm = u3nc(TOSS, u3_nul);
    c3_w arm_len = 1 + _n_comp(&arm, u3t(item), los_o, tel_o);
    _n_emit(&arm, u3nc(SBIP, size + after));
    c3_w cmp = _n_comp(&part, test, c3n, c3n);
    u3z(test);
    _n_emit(&part, u3nc(SBIN, arm_len + 1));
    _n_apen(&part, arm);
    // Hold the reversed fragments in execution order until all offsets are known.
    u3_noun old = rows;
    rows = u3k(u3t(rows));
    u3z(old);
    parts = u3nc(part, parts);
    size += cmp + 2 + arm_len;
  }
  if ( direct && len >= 4 && len <= 128 ) {
    _n_emit(ops, u3nc(HILB, u3nc(N_CASE_TAG, len)));
    _n_emit(ops, TOSS);
    _n_emit(ops, SWAP);
    _n_emit(ops, TOSS);
    size += 4;
  }
  for ( u3_noun cur = parts; cur != u3_nul; cur = u3t(cur) ) {
    _n_apen(ops, u3k(u3h(cur)));
  }
  u3z(parts);
  return size;
}

static c3_w
_n_comp_cases(u3_noun* ops, u3_noun fol, c3_o los_o, c3_o tel_o)
{
  _n_case first, row;
  if ( !_n_read_case(fol, &first) ) return 0;
  row = first;
  u3_noun rows = u3_nul, end = fol;
  c3_w len = 0;
  c3_t direct = 1, more = 0;
  do {
    if ( !_n_formulaic(row.arm) ) { more = 0; break; }
    rows = u3nc(u3nc(u3k(row.tag), u3k(row.arm)), rows);
    ++len;
    if ( row.tag > 0x7fffffff ) direct = 0;
    end = row.rest;
    more = _n_read_case(end, &row);
  } while ( more && c3y == u3r_sing(first.slot, row.slot)
         && (first.guard == row.guard ||
             (first.guard != u3_none && row.guard != u3_none &&
              c3y == u3r_sing(first.guard, row.guard))) );
  if ( len < 3 || !_n_formulaic(end) ) {
    u3z(rows);
    return 0;
  }

  u3_noun def = u3_nul, body = u3_nul;
  c3_w def_len = _n_comp(&def, end, los_o, tel_o);
  c3_w body_len = _n_comp(&body, first.slot, c3n, c3n);
  body_len += _n_case_block(&body, rows, len, direct, los_o, tel_o, 0, def_len);
  c3_w pre = 0;
  if ( first.guard != u3_none ) {
    pre = 1 + _n_comp(ops, first.guard, c3n, c3n);
    _n_emit(ops, u3nc(SBIN, body_len));
  }
  _n_apen(ops, body);
  _n_apen(ops, def);
  return pre + body_len + def_len;
}
