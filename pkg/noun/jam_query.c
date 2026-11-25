/// @file
// Compile nested patterns into a bottom-up tree automaton. A state describes
// the subpatterns that match at one root. A cell combines its children's
// states, and a jam backreference reuses the state at its target position.

#include "serial.h"
#include "cue_pages.h"
#include "ur/bitstream.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

enum { RULE_ANY, RULE_ATOM, RULE_LITERAL, RULE_PAIR };
enum { MAX_RULES = 64, STATE_OPEN = 255 };

typedef struct {
  unsigned kind, head, tail;
  uint64_t value;
  const char* text;  // Literals longer than eight bytes.
  size_t size;
} rule;

struct _u3_jam_query {
  rule rules[MAX_RULES];
  unsigned count, states, literals;
  uint8_t literal[MAX_RULES], atom_state[MAX_RULES], small[256];
  uint64_t roots[MAX_RULES], mask[STATE_OPEN], accept[STATE_OPEN];
  uint8_t cell[STATE_OPEN][STATE_OPEN];
};

static uint64_t
_bit(unsigned i)
{
  return UINT64_C(1) << i;
}

static unsigned
_intern(u3_jam_query* q, rule r)
{
  for ( unsigned i = 2; i < q->count; i++ ) {
    const rule* s = &q->rules[i];
    if ( r.kind == s->kind && r.head == s->head && r.tail == s->tail &&
         r.value == s->value && r.size == s->size &&
         (!r.size || !memcmp(r.text, s->text, r.size)) ) return i;
  }
  if ( q->count == MAX_RULES ) return MAX_RULES;
  if ( r.size ) r.text = memcpy(c3_malloc(r.size), r.text, r.size);
  q->rules[q->count] = r;
  return q->count++;
}

static void
_spaces(const char** src)
{
  while ( isspace((unsigned char)**src) ) ++*src;
}

static unsigned
_parse(u3_jam_query* q, const char** src, unsigned depth)
{
  _spaces(src);
  const char* p = *src;
  if ( depth >= MAX_RULES ) return MAX_RULES;
  switch ( *p ) {
    case '_':
    case '@':
      *src = p + 1;
      return *p == '_' ? RULE_ANY : RULE_ATOM;
    case '[': {
      *src = p + 1;
      unsigned h = _parse(q, src, depth + 1);
      if ( h == MAX_RULES ) return MAX_RULES;
      unsigned t = _parse(q, src, depth + 1);
      _spaces(src);
      if ( t == MAX_RULES || **src != ']' ) return MAX_RULES;
      ++*src;
      return _intern(q, (rule){.kind = RULE_PAIR, .head = h, .tail = t});
    }
    case '%': {
      const char* start = ++p;
      while ( (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' ) p++;
      size_t n = p - start;
      if ( !n ) return MAX_RULES;
      *src = p;
      rule r = {.kind = RULE_LITERAL};
      if ( n > 8 ) {
        r.text = start;
        r.size = n;
      }
      else {
        for ( size_t i = 0; i < n; i++ )
          r.value |= (uint64_t)(uint8_t)start[i] << (8 * i);
      }
      return _intern(q, r);
    }
    default: {
      uint64_t v = 0;
      if ( *p < '0' || *p > '9' ) return MAX_RULES;
      do {
        unsigned digit = *p++ - '0';
        if ( v > (UINT64_MAX - digit) / 10 ) return MAX_RULES;
        v = 10 * v + digit;
      } while ( *p >= '0' && *p <= '9' );
      *src = p;
      return _intern(q, (rule){.kind = RULE_LITERAL, .value = v});
    }
  }
}

// Each state names the set of subpatterns that match the current root.
static unsigned
_state(u3_jam_query* q, uint64_t mask)
{
  for ( unsigned i = 0; i < q->states; i++ )
    if ( q->mask[i] == mask ) return i;
  if ( q->states == STATE_OPEN ) return STATE_OPEN;
  unsigned id = q->states++;
  q->mask[id] = mask;
  while ( mask ) {
    q->accept[id] |= q->roots[__builtin_ctzll(mask)];
    mask &= mask - 1;
  }
  return id;
}

static bool
_transition(u3_jam_query* q, unsigned h, unsigned t)
{
  uint64_t mask = _bit(RULE_ANY);
  for ( unsigned i = 2; i < q->count; i++ ) {
    const rule* r = &q->rules[i];
    if ( r->kind == RULE_PAIR && (q->mask[h] & _bit(r->head)) &&
                            (q->mask[t] & _bit(r->tail)) ) {
      mask |= _bit(i);
    }
  }
  unsigned id = _state(q, mask);
  q->cell[h][t] = id;
  return id != STATE_OPEN;
}

u3_jam_query*
u3s_jam_query_compile(size_t count, const char* const* patterns, const char** error)
{
  *error = "expected 1 to 64 patterns";
  if ( !count || count > 64 ) return NULL;
  u3_jam_query* q = c3_calloc(sizeof(*q));
  q->count = 2;
  for ( unsigned i = 0; i < count; i++ ) {
    const char* src = patterns[i];
    unsigned root = _parse(q, &src, 0);
    _spaces(&src);
    if ( root == MAX_RULES || *src ) {
      *error = "invalid pattern or more than 62 distinct literals and pairs";
      goto fail;
    }
    q->roots[root] |= _bit(i);
  }
  // State zero covers every atom that is absent from the literal set.
  _state(q, _bit(RULE_ANY) | _bit(RULE_ATOM));
  for ( unsigned i = 2; i < q->count; i++ ) {
    const rule* r = &q->rules[i];
    if ( r->kind != RULE_LITERAL ) continue;
    unsigned id = _state(q, _bit(RULE_ANY) | _bit(RULE_ATOM) | _bit(i));
    q->atom_state[i] = id;
    if ( !r->size && r->value < 256 ) q->small[r->value] = id;
    else q->literal[q->literals++] = i;
  }
  // New states extend the loop. Each ordered pair is compiled exactly once.
  for ( unsigned h = 0; h < q->states; h++ ) {
    for ( unsigned t = 0; t <= h; t++ ) {
      if ( !_transition(q, h, t) || (t != h && !_transition(q, t, h)) ) {
        *error = "patterns require more than 255 distinct subtree states";
        goto fail;
      }
    }
  }
  *error = NULL;
  return q;
fail:
  u3s_jam_query_free(q);
  return NULL;
}

void
u3s_jam_query_free(u3_jam_query* q)
{
  if ( !q ) return;
  for ( unsigned i = 2; i < q->count; i++ ) {
    c3_free((void*)q->rules[i].text);
  }
  c3_free(q);
}

// Ordinary cue supplies missing high bits of the final atom as zeros.
// Find the significant width in the physical payload consumed by cue.
static uint64_t
_payload_width(const ur_bsr_t* r, uint64_t width)
{
  uint64_t end = r->off + width;
  while ( end > r->off ) {
    uint64_t ix = (end - 1) >> 3;
    unsigned top = 1 + ((end - 1) & 7);
    unsigned v = r->bytes[ix] & ((1U << top) - 1);
    if ( !ix ) v &= 0xffU << r->off;
    if ( v ) return ix * 8 + (32u - __builtin_clz(v)) - r->off;
    end = ix * 8;
  }
  return 0;
}

static bool
_text_equal(ur_bsr_t r, uint64_t bits, const rule* literal)
{
  uint8_t buf[64];
  const char* text = literal->text;
  while ( bits ) {
    unsigned take = c3_min(bits, 8 * sizeof(buf));
    // The last byte may end inside the literal.
    memset(buf, 0, sizeof(buf));
    ur_bsr_bytes_any(&r, take, buf);
    unsigned bytes = (take + 7) / 8;
    if ( memcmp(buf, text, bytes) ) return false;
    text += bytes;
    bits -= take;
  }
  return true;
}

static uint8_t
_atom(const u3_jam_query* q, ur_bsr_t* r, const ur_cue_token_t* token)
{
  uint64_t width = token->width;
  uint64_t value = token->value;
  ur_bsr_t payload = *r;
  if ( width > 64 ) {
    uint64_t available = r->left * 8 - r->off;
    if ( width > available && !r->off ) {
      // The aligned byte reader consumes the entire remaining buffer.
      *r = (ur_bsr_t){.bits = r->bits + width};
      width = available;
    }
    else {
      ur_bsr_skip_any(r, width);
      if ( width > available ) width = available - (r->left * 8 - r->off);
    }
    width = _payload_width(&payload, width);
    if ( width <= 64 ) {
      value = ur_bsr64_any(&payload, width);
    }
  }
  if ( width <= 64 && value < 256 ) return q->small[value];
  for ( unsigned j = 0; j < q->literals; j++ ) {
    unsigned i = q->literal[j];
    const rule* lit = &q->rules[i];
    if ( width <= 64 ) {
      if ( !lit->size && lit->value == value ) return q->atom_state[i];
    }
    else if ( lit->size == (width + 7) / 8 && _text_equal(payload, width, lit) ) {
      return q->atom_state[i];
    }
  }
  return 0;
}

typedef struct {
  size_t slot;
  uint8_t head;
} frame;

typedef struct {
  _cue_page* pages;
  _cue_directory* directory;
  uint8_t* state;
  frame* stack;
  size_t count, page_count, depth, states_cap, pages_cap, stack_cap;
  _cue_page local_pages[2];
  uint8_t local_state[64];
  frame local_stack[16];
} scan;

static void*
_grow(void* old, void* local, size_t* capacity, size_t width)
{
  if ( *capacity > SIZE_MAX / width / 2 ) u3m_bail(c3__meme);
  size_t bytes = *capacity * width;
  *capacity *= 2;
  if ( old == local ) return memcpy(c3_malloc(2 * bytes), old, bytes);
  return c3_realloc(old, *capacity * width);
}

// Positions arrive in order. Each valid backreference names a completed node.
static size_t
_record_node(scan* s, uint64_t bit, uint8_t state)
{
  uint64_t page = bit >> 9;
  if ( !s->page_count || s->pages[s->page_count - 1].number != page ) {
    if ( s->page_count == s->pages_cap )
      s->pages = _grow(s->pages, s->local_pages, &s->pages_cap, sizeof(*s->pages));
    _cue_page_init(s->pages, s->page_count++, page, s->count, &s->directory, 1);
  }
  if ( s->count == s->states_cap )
    s->state = _grow(s->state, s->local_state, &s->states_cap, sizeof(*s->state));
  size_t slot = s->count++;
  _cue_page_mark(s->pages + s->page_count - 1, bit, slot);
  s->state[slot] = state;
  return slot;
}

c3_o
u3s_jam_query_scan(const u3_jam_query* q, size_t len, const uint8_t* bytes, uint64_t* matches)
{
  scan s = {
    .pages = s.local_pages, .pages_cap = sizeof(s.local_pages) / sizeof(*s.pages),
    .state = s.local_state, .states_cap = sizeof(s.local_state),
    .stack = s.local_stack, .stack_cap = sizeof(s.local_stack) / sizeof(*s.stack)
  };
  ur_bsr_t r = {0};
  uint64_t found = 0;
  c3_o ok = c3n;
  *matches = 0;
  if ( !len || len > UINT64_C(0x7ffffffffffffff) ||
       ur_bsr_init(&r, len, bytes) != ur_cue_good )
    return c3n;
  for (;;) {
    uint64_t bit = r.bits;
    ur_cue_token_t token = {0};
    if ( ur_bsr_token(&r, &token) != ur_cue_good ) break;
    for ( unsigned i = 0; i < token.cells; i++, bit += 2 ) {
      if ( s.depth == s.stack_cap )
        s.stack = _grow(s.stack, s.local_stack, &s.stack_cap, sizeof(*s.stack));
      s.stack[s.depth++] = (frame){_record_node(&s, bit, STATE_OPEN), STATE_OPEN};
    }
    if ( token.tag == ur_jam_cell ) continue;
    uint8_t state;
    if ( token.tag == ur_jam_back ) {
      size_t ix = _cue_page_find(s.pages, s.page_count, s.directory, token.value);
      if ( ix == SIZE_MAX || s.state[ix] == STATE_OPEN ) break;
      state = s.state[ix];
    }
    else {
      if ( (token.width + 7) / 8 > c3_w_max ) break;
      state = _atom(q, &r, &token);
      _record_node(&s, bit, state);
    }
    for (;;) {
      // Every subtree reached via a backreference occurs earlier in this
      // record. Its interior matches have already contributed to found.
      found |= q->accept[state];
      if ( !s.depth ) {
        *matches = found;
        ok = c3y;
        goto done;
      }
      frame* f = &s.stack[s.depth - 1];
      if ( f->head == STATE_OPEN ) {
        f->head = state;
        break;
      }
      state = q->cell[f->head][state];
      s.state[f->slot] = state;
      s.depth--;
    }
  }
done:
  _cue_directory_free(s.directory, 1);
  if ( s.pages != s.local_pages ) c3_free(s.pages);
  if ( s.state != s.local_state ) c3_free(s.state);
  if ( s.stack != s.local_stack ) c3_free(s.stack);
  return ok;
}
