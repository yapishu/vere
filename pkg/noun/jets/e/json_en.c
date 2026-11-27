/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/*
** structs
*/

typedef struct _json_buffer {
  c3_y*  buf_y;
  size_t len_i;
} json_buffer;

/*
** constants
*/

const c3_y _JSON_NULL[]     = "null";
const c3_y _JSON_TRUE[]     = "true";
const c3_y _JSON_FALSE[]    = "false";

/*
** forward declarations
*/

static c3_d
_measure(u3_noun a);

static void
serialize(json_buffer*, u3_noun);

/*
** core jet logic
*/

static void
_append_char(json_buffer *buf_u, c3_y c_y)
{
  buf_u->buf_y[(buf_u->len_i)++] = c_y;
}

static void
_append_text(json_buffer *buf_u, const c3_y *buf_y, c3_w len_w)
{
  memcpy(&(buf_u->buf_y[buf_u->len_i]), buf_y, len_w);
  buf_u->len_i += len_w;
}

static c3_d
_measure_loobean(u3_noun a)
{
  switch ( a ) {
    default: u3m_bail(c3__exit);
    case c3y: return sizeof(_JSON_TRUE) - 1;
    case c3n: return sizeof(_JSON_FALSE) - 1;
  }
}

static void
_serialize_loobean(json_buffer *buf_u, u3_noun a)
{
  switch ( a ) {
    default: u3_assert(0);
    case c3y: _append_text(buf_u, _JSON_TRUE, sizeof(_JSON_TRUE) - 1);   break;
    case c3n: _append_text(buf_u, _JSON_FALSE, sizeof(_JSON_FALSE) - 1); break;
  }
}

// Small printed numbers fit directly in the noun word.
static inline c3_w
_number_length(u3_atom a)
{
  if ( c3y == u3a_is_cat(a) ) return (c3_bits_word(a) + 7) >> 3;
  return u3r_met(3, a);
}

static c3_d
_measure_number(u3_noun a)
{
  if ( _(u3du(a)) ) {
    u3m_bail(c3__exit);
  }

  return _number_length(a);
}

static const c3_y*
_atom_bytes(const u3_atom* atom)
{
  STATIC_ASSERT(c3_endian == c3_endian_little, "atom byte order");
  if ( c3y == u3a_is_cat(*atom) ) return (const c3_y*)atom;
  const u3a_atom* vat_u = u3a_to_ptr(*atom);
  return (const c3_y*)vat_u->buf_w;
}

static void
_serialize_number(json_buffer* buf_u, u3_noun a)
{
  _append_text(buf_u, _atom_bytes(&a), _number_length(a));
}

static inline uint64_t
_zero_bytes(uint64_t word)
{
  return (word - UINT64_C(0x0101010101010101)) & ~word;
}

// Only the high bits matter. A borrow across lanes can follow a match,
// so this predicate tests a whole word and locates escapes byte by byte.
static inline c3_t
_word_escapes(uint64_t word)
{
  const uint64_t ones = UINT64_C(0x0101010101010101);
  uint64_t control = (word - 32 * ones) & ~word;
  uint64_t hits = control | _zero_bytes(word ^ (34 * ones))
                          | _zero_bytes(word ^ (92 * ones))
                          | _zero_bytes(word ^ (127 * ones));
  return (hits & (128 * ones)) != 0;
}

static inline c3_t
_block_escapes(const c3_y* bytes)
{
  uint64_t word;
  memcpy(&word, bytes, sizeof(word));
  return _word_escapes(word);
}

// Compare a whole block at once. A fixed trip count lets the compiler
// vectorize the scan on x86 and ARM.
// https://lemire.me/blog/2024/05/31/quickly-checking-whether-a-string-needs-escaping/
static inline c3_t
_has_escape16(const c3_y* bytes)
{
  c3_y special = 0;
  for ( size_t i = 0; i < 16; i++ ) {
    c3_y c = bytes[i];
    special |= (c < 32) | (c == 34) | (c == 92) | (c == 127);
  }
  return special;
}

// Return the number of leading bytes known to need no escape.
static inline c3_w
_plain_prefix(const c3_y* bytes, c3_w len)
{
  if ( len < 8 ) {
    // Atom storage ends on a loom word. Set lanes beyond its bytes to 0xff.
    uint64_t word;
    if ( len <= 4 ) {
      uint32_t low;
      memcpy(&low, bytes, sizeof(low));
      word = low;
    }
    else {
      memcpy(&word, bytes, sizeof(word));
    }
    return _word_escapes(word | (UINT64_MAX << (8 * len))) ? 0 : len;
  }
  if ( len >= 16 ) {
    c3_w pos = 0;
    while ( len - pos >= 16 ) {
      if ( _has_escape16(bytes + pos) ) return pos;
      pos += 16;
    }
    // The final load can overlap bytes already checked.
    if ( pos < len && _has_escape16(bytes + len - 16) ) return pos;
    return len;
  }
  if ( _block_escapes(bytes) ) return 0;
  return (len > 8 && _block_escapes(bytes + len - 8)) ? 8 : len;
}

// Extra bytes beyond the literal input byte in a quoted cord.
static const c3_y _escape_extra[256] = {
  [0 ... 9] = 5, [10] = 1, [11 ... 31] = 5,
  [34] = 1, [92] = 1, [127] = 5
};

static inline c3_d
_quoted_size(const c3_y* bytes, c3_w len, c3_w pos)
{
  c3_d size = (c3_d)len + 2;
  while ( pos < len ) {
    c3_w end = pos + c3_min(16, len - pos);
    for ( ; pos < end; pos++ ) size += _escape_extra[bytes[pos]];
    if ( pos < len ) pos += _plain_prefix(bytes + pos, len - pos);
  }
  return size;
}

static inline c3_y*
_escape_byte(c3_y* out, c3_y c)
{
  static const c3_y hex[] = "0123456789abcdef";
  c3_y extra = _escape_extra[c];
  if ( !extra ) {
    *out++ = c;
  }
  else if ( extra == 1 ) {
    *out++ = '\\';
    *out++ = (c == '\n') ? 'n' : c;
  }
  else {
    memcpy(out, "\\u00", 4);
    out[4] = hex[c >> 4];
    out[5] = hex[c & 15];
    out += 6;
  }
  return out;
}

static void
_write_escaped(json_buffer* buf_u, const c3_y* bytes, c3_w len, c3_w pos)
{
  c3_y* out = buf_u->buf_y + buf_u->len_i;
  *out++ = '"';
  for (;;) {
    if ( pos ) {
      memcpy(out, bytes, pos);
      out += pos;
    }
    bytes += pos;
    len -= pos;
    if ( !len ) break;
    c3_w width = c3_min(16, len);
    for ( c3_w i = 0; i < width; ++i ) out = _escape_byte(out, bytes[i]);
    bytes += width;
    len -= width;
    if ( !len ) break;
    pos = _plain_prefix(bytes, len);
  }
  *out++ = '"';
  buf_u->len_i = out - buf_u->buf_y;
}

static c3_d
_measure_string(u3_noun a)
{
  u3x_atom(a);
  const c3_y* bytes = _atom_bytes(&a);
  c3_w len = u3r_met(3, a);
  // Short strings cost less to quote directly than to classify first.
  if ( len <= 4 ) {
    c3_w extra = 0;
    for ( c3_w i = 0; i < len; i++ ) extra += _escape_extra[bytes[i]];
    return len + 2 + extra;
  }
  c3_w pos = _plain_prefix(bytes, len);
  return pos < len ? _quoted_size(bytes, len, pos) : (c3_d)len + 2;
}

static void
_serialize_string(json_buffer* buf_u, u3_noun a)
{
  const c3_y* bytes = _atom_bytes(&a);
  c3_w len = u3r_met(3, a);
  if ( len <= 4 ) {
    c3_y* out = buf_u->buf_y + buf_u->len_i;
    *out++ = '"';
    for ( c3_w i = 0; i < len; ++i ) out = _escape_byte(out, bytes[i]);
    *out++ = '"';
    buf_u->len_i = out - buf_u->buf_y;
    return;
  }
  c3_w pos = _plain_prefix(bytes, len);
  if ( pos < len ) _write_escaped(buf_u, bytes, len, pos);
  else {
    _append_char(buf_u, '"');
    _append_text(buf_u, bytes, len);
    _append_char(buf_u, '"');
  }
}

// Bound each subtotal so expanded shared subtrees cannot wrap the slab size.
static inline c3_d
_checked_size(c3_d len_d)
{
  if ( len_d > SIZE_MAX || len_d > u3a_maximum * sizeof(c3_w) ) {
    return u3m_bail(c3__fail);
  }
  return len_d;
}

static c3_d
_measure_array(u3_noun a)
{
  if ( u3_nul != a ) {
    u3_noun i, t = a;
    // array open brace
    c3_d    siz_d = 1;

    while ( u3_nul != t ) {
      u3x_cell(t, &i, &t);
      // comma or array close brace
      siz_d = _checked_size(siz_d + _measure(i) + 1);
    }

    return siz_d;
  }
  else {
    // empty array
    return 2;
  }
}

static void
_serialize_array(json_buffer *buf_u, u3_noun a)
{
  _append_char(buf_u, '[');

  if ( u3_nul != a ) {
    u3_noun i, t = a;

    while ( u3_nul != t ) {
      u3x_cell(t, &i, &t);
      serialize(buf_u, i);
      _append_char(buf_u, ',');
    }

    // Remove trailing comma from array contents
    --buf_u->len_i;
  }

  _append_char(buf_u, ']');
}

static c3_d
_measure_object_helper(u3_noun a)
{
  c3_d siz_d = 0;

  if ( u3_nul != a ) {
    u3_noun n_a, l_a, r_a;
    u3_noun pn_a, qn_a;
    u3x_trel(a, &n_a, &l_a, &r_a);
    u3x_cell(n_a, &pn_a, &qn_a);

    siz_d += _measure_object_helper(r_a);
    siz_d += _measure_object_helper(l_a);

    siz_d += _measure_string(pn_a);
    siz_d += _measure(qn_a);

    // colon and comma (or closing brace)
    siz_d += 2;
  }

  return _checked_size(siz_d);
}

static void
_serialize_object_helper(json_buffer *buf_u, u3_noun a)
{
  if ( u3_nul != a ) {
    u3_noun n_a, l_a, r_a;
    u3_noun pn_a, qn_a;
    u3x_trel(a, &n_a, &l_a, &r_a);
    u3x_cell(n_a, &pn_a, &qn_a);

    // order is important to match unjetted tree traversal
    _serialize_object_helper(buf_u, r_a);

    _serialize_string(buf_u, pn_a);
    _append_char(buf_u, ':');
    serialize(buf_u, qn_a);
    _append_char(buf_u, ',');

    _serialize_object_helper(buf_u, l_a);
  }
}

static c3_d
_measure_object(u3_noun a)
{
  if ( u3_nul != a ) {
    // opening brace
    return 1 + _measure_object_helper(a);
  }
  else {
    // empty object
    return 2;
  }
}

static void
_serialize_object(json_buffer *buf_u, u3_noun a)
{
  _append_char(buf_u, '{');

  if ( u3_nul != a ) {
    _serialize_object_helper(buf_u, a);

    // Remove trailing comma from object contents
    --buf_u->len_i;
  }

  _append_char(buf_u, '}');
}

static c3_d
_measure(u3_noun a)
{
  if ( u3_nul == a ) {
    return sizeof(_JSON_NULL) - 1;
  }
  else {
    u3_noun s, p;
    u3x_cell(a, &s, &p);

    switch ( s ) {
      default: u3m_bail(c3__fail);
      case c3__a: return _measure_array(p);
      case c3__o: return _measure_object(p);
      case c3__b: return _measure_loobean(p);
      case c3__n: return _measure_number(p);
      case c3__s: return _measure_string(p);
    }
  }
}

static void
serialize(json_buffer *buf_u, u3_noun a)
{
  if ( u3_nul == a ) {
    _append_text(buf_u, _JSON_NULL, sizeof(_JSON_NULL) - 1);
  }
  else {
    u3_noun s, p;
    u3x_cell(a, &s, &p);

    switch ( s ) {
      default: u3_assert(0);
      case c3__a: _serialize_array(buf_u, p);   break;
      case c3__o: _serialize_object(buf_u, p);  break;
      case c3__b: _serialize_loobean(buf_u, p); break;
      case c3__n: _serialize_number(buf_u, p);  break;
      case c3__s: _serialize_string(buf_u, p);  break;
    }
  }
}

/*
** jet interface functions
*/

u3_atom
u3qe_json_en(u3_noun a)
{
  u3i_slab     sab_u;
  json_buffer  bof_u;
  json_buffer *buf_u = &bof_u;
  c3_d         siz_d = _checked_size(_measure(a));

  u3i_slab_bare(&sab_u, 3, siz_d);
  // Serialization fills the payload. Clear the padding for atom minting.
  sab_u.buf_w[sab_u.len_w - 1] = 0;
  buf_u->buf_y = sab_u.buf_y;
  buf_u->len_i = 0;

  serialize(buf_u, a);
  u3_assert(buf_u->len_i == siz_d);

  return u3i_slab_mint_bytes(&sab_u);
}

u3_atom
u3ke_json_en(u3_noun a)
{
  u3_atom res = u3qe_json_en(a);
  u3z(a);
  return res;
}

u3_atom
u3we_json_en(u3_noun cor)
{
  return u3qe_json_en(u3x_at(u3x_sam, cor));
}
