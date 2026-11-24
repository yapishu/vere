/// @file

#include "serial.h"

#include <errno.h>
#include <fcntl.h>

#include "allocate.h"
#include "cue_pages.h"
#include "hashtable.h"
#include "jets/k.h"
#include "jets/q.h"
#include "retrieve.h"
#include "ur/ur.h"
#include "vortex.h"
#include "xtract.h"

const c3_y u3s_dit_y[64] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '-', '~'
};

/* Cursor in the pre-sized jam slab. */
struct _cs_jam_slab {
  u3i_slab* sab_u;
  c3_w      len_w;
  c3_w      bit_w;
};

static inline void
_cs_jam_slab_advance(struct _cs_jam_slab* slab, c3_w bits)
{
  if ( bits > slab->len_w - slab->bit_w ) u3m_bail(c3__fail);
  slab->bit_w += bits;
}

/* _cs_jam_slab_chop(): chop [met_w] bits of [a] into [slab_u]
*/
static inline void
_cs_jam_slab_chop(struct _cs_jam_slab* slab_u, c3_w met_w, u3_noun a)
{
  c3_w bit_w = slab_u->bit_w;
  _cs_jam_slab_advance(slab_u, met_w);

  {
    c3_w* buf_w = slab_u->sab_u->buf_w;
    u3r_chop(0, 0, met_w, bit_w, buf_w, a);
  }
}

/* _cs_jam_slab_mat(): length-prefixed encode (mat) [a] into [slab_u]
*/
static void
_cs_jam_slab_mat(struct _cs_jam_slab* slab_u, u3_noun a)
{
  if ( 0 == a ) {
    _cs_jam_slab_chop(slab_u, 1, 1);
  }
  else {
    c3_w   a_w = u3r_met(0, a);
    c3_w   b_w = c3_bits_word(a_w);
    c3_w bit_w = slab_u->bit_w;

    //  reserve the prefix and payload together
    //
    {
      c3_w met_w = a_w + (2 * b_w);

      if ( a_w > (c3_w_max - 64) ) {
        u3m_bail(c3__fail);
        return;
      }

      _cs_jam_slab_advance(slab_u, met_w);
    }

    {
#ifndef VERE64
      c3_w  src_w[2];
#else
      c3_w  src_w[1];
#endif

      c3_w* buf_w = slab_u->sab_u->buf_w;

      //  _cs_jam_slab_chop(slab_u, b_w+1, 1 << b_w);
      //
      {
#ifndef VERE64
        c3_d dat_d = (c3_d)1 << b_w;
        src_w[0]   = (c3_w)dat_d;
        src_w[1]   = dat_d >> 32;
        u3r_chop_words(0, 0, b_w + 1, bit_w, buf_w, 2, src_w);
#else
        src_w[0] = (c3_d)1 << b_w;
        u3r_chop_words(0, 0, b_w + 1, bit_w, buf_w, 1, src_w);
#endif

        bit_w += b_w + 1;
      }

      //  _cs_jam_slab_chop(slab_u, b_w-1, a_w);
      //
      {
        src_w[0] = a_w;
        u3r_chop_words(0, 0, b_w - 1, bit_w, buf_w, 1, src_w);
        bit_w += b_w - 1;
      }

      //  _cs_jam_slab_chop(slab_u, a_w, a);
      //
      u3r_chop(0, 0, a_w, bit_w, buf_w, a);
    }
  }
}

/* _cs_coin_chub(): shortcircuit u3i_chubs().
*/
static inline u3_atom
_cs_coin_chub(c3_d a_d)
{
  return ( ((c3_d)u3a_direct_max) >= a_d ) ? a_d : u3i_chubs(1, &a_d);
}

/* _cs_jam_bsw_atom(): encode in/direct atom in bitstream.
*/
static inline void
_cs_jam_bsw_atom(ur_bsw_t* rit_u, c3_w met_w, u3_atom a)
{
  if ( c3y == u3a_is_cat(a) ) {
    //  XX need a ur_bsw_atom32()
    //
    ur_bsw_atom64(rit_u, (c3_y)met_w, (c3_d)a);
  }
  else {
    u3a_atom* vat_u = u3a_to_ptr(a);
    //  XX assumes little-endian
    //  XX need a ur_bsw_atom_words()
    //
    c3_y*     byt_y = (c3_y*)vat_u->buf_w;
    ur_bsw_atom_bytes(rit_u, (c3_d)met_w, byt_y);
  }
}

#include "jam_classes.inc"

/* u3s_jam_fib(): jam into a slab and return the length in bits. */
c3_w
u3s_jam_fib(u3i_slab* sab_u, u3_noun a)
{
  struct _cs_jam_slab slab_u = { .sab_u = sab_u };
  _jam_emit_classes(a, NULL, &slab_u);
  return slab_u.bit_w;
}

/* u3s_jam_xeno(): jam into an off-loom byte buffer. */
c3_d
u3s_jam_xeno(u3_noun a, c3_d* len_d, c3_y** byt_y)
{
  ur_bsw_t writer = {0};
  _jam_emit_classes(a, &writer, NULL);
  return ur_bsw_done(&writer, len_d, byt_y);
}

/* _cs_cue: stack frame for tracking intermediate cell results
*/
typedef struct _cs_cue {
  u3_weak hed;  //  head of a cell or u3_none
  u3_atom wid;  //  bitwidth of [hed] or 0
  u3_atom cur;  //  bit-cursor position
} _cs_cue;

/* _cs_rub: rub, TRANSFER [cur], RETAIN [a]
*/
static inline u3_noun
_cs_rub(u3_atom cur, u3_atom a)
{
  u3_noun pro = u3qe_rub(cur, a);
  u3z(cur);
  return pro;
}

/* _cs_cue_next(): advance into [a], reading next value
**                 TRANSFER [cur], RETAIN [a]
*/
static inline u3_noun
_cs_cue_next(u3a_pile*     pil_u,
             u3p(u3h_root) har_p,
             u3_atom         cur,
             u3_atom           a,
             u3_atom*        wid)
{
  while ( 1 ) {
    //  read tag bit at cur
    //
    c3_y tag_y = u3qc_cut(0, cur, 1, a);

    //  low bit unset, (1 + cur) points to an atom
    //
    //    produce atom and the width we read
    //
    if ( 0 == tag_y ) {
      u3_noun bur = _cs_rub(u3i_vint(cur), a);
      u3_noun pro = u3k(u3t(bur));

      u3h_put(har_p, cur, u3k(pro));
      *wid = u3qa_inc(u3h(bur));

      u3z(bur);
      return pro;
    }
    else {
      //  read tag bit at (1 + cur)
      //
      {
        u3_noun x = u3qa_inc(cur);
        tag_y = u3qc_cut(0, x, 1, a);
        u3z(x);
      }

      //  next bit set, (2 + cur) points to a backref
      //
      //    produce referenced value and the width we read
      //
      if ( 1 == tag_y ) {
        u3_noun bur = _cs_rub(u3ka_add(2, cur), a);
        u3_noun pro = u3x_good(u3h_get(har_p, u3t(bur)));

        *wid = u3qa_add(2, u3h(bur));

        u3z(bur);
        return pro;
      }
      //  next bit unset, (2 + cur) points to the head of a cell
      //
      //    push a head-frame onto the road stack and read the head
      //
      else {
        _cs_cue* fam_u = u3a_push(pil_u);

        //  NB: fam_u->wid unused in head-frame
        //
        fam_u->hed = u3_none;
        fam_u->cur = cur;

        cur = u3qa_add(2, cur);
        continue;
      }
    }
  }
}

u3_noun
u3s_cue(u3_atom a)
{
  //  pro:   cue'd noun product
  //  wid:   bitwidth read to produce [pro]
  //  fam_u: stack frame
  //  har_p: backreference table
  //  pil_u: stack control structure
  //
  u3_noun         pro;
  u3_atom         wid;
  _cs_cue*      fam_u;
  u3p(u3h_root) har_p = u3h_new();
  u3a_pile      pil_u;

  //  initialize stack control
  //
  u3a_pile_prep(&pil_u, sizeof(*fam_u), __alignof__(*fam_u));

  //  commence cueing at bit-position 0
  //
  pro = _cs_cue_next(&pil_u, har_p, 0, a, &wid);

  //  process cell results
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
    fam_u = u3a_peek(&pil_u);

    do {
      //  head-frame: stash [pro] and [wid]; continue into the tail
      //
      if ( u3_none == fam_u->hed ) {
        //  NB: fam_u->wid unused in head-frame
        //
        fam_u->hed = pro;
        fam_u->wid = wid;

        //  continue reading at the bit-position after [pro]
        {
          u3_noun cur = u3ka_add(2, u3qa_add(wid, fam_u->cur));
          pro = _cs_cue_next(&pil_u, har_p, cur, a, &wid);
        }

        fam_u = u3a_peek(&pil_u);
      }
      //  tail-frame: cons cell, recalculate [wid], and pop the stack
      //
      else {
        pro   = u3nc(fam_u->hed, pro);
        u3h_put(har_p, fam_u->cur, u3k(pro));
        u3z(fam_u->cur);
        wid   = u3ka_add(2, u3ka_add(wid, fam_u->wid));
        fam_u = u3a_pop(&pil_u);
      }
    } while ( c3n == u3a_pile_done(&pil_u) );
  }

  u3z(wid);
  u3h_free(har_p);

  return pro;
}

/*
**  stack frame for recording head vs tail iteration
**
**    $?  [u3_none bits=@]
**    [hed=* bits=@]
*/
typedef struct _cue_frame_s {
  u3_weak ref;
  c3_d    key_d;  // Dictionary key reserved at the opening cell tag.
} _cue_frame_t;

#include "cue_rank.inc"

/* _cs_cue_xeno_next(): read next value from bitstream, dictionary off-loom.
*/
static inline ur_cue_res_e
_cs_cue_xeno_next(u3a_pile* pil_u, ur_bsr_t* red_u,
                  _cue_refs* dic_u, u3_noun* out)
{
  while ( 1 ) {
    c3_d bit_d = red_u->bits;
    ur_cue_token_t token;
    ur_cue_res_e res_e = ur_bsr_token(red_u, &token);
    if ( res_e != ur_cue_good ) return res_e;

    for ( unsigned i = 0; i < token.cells; i++, bit_d += 2 ) {
      _cue_frame_t* fam_u = u3a_push(pil_u);
      fam_u->ref = u3_none;
      fam_u->key_d = _cue_refs_open(dic_u, bit_d);
    }

    switch ( token.tag ) {
      default: u3_assert(0);
      case ur_jam_cell: continue;

      case ur_jam_back: {
        u3_weak bak = _cue_refs_get(dic_u, token.value);
        if ( bak == u3_none ) return ur_cue_back;
        *out = bak;
        return ur_cue_good;
      }

      case ur_jam_atom: {
        c3_d len_d = token.width;
        if ( len_d <= 64 ) {
          *out = len_d < u3a_word_bits ? (u3_noun)token.value : u3i_chub(token.value);
        }
        else {
          c3_d     byt_d = (len_d + 0x7) >> 3;
          u3i_slab sab_u;

          if ( c3_w_max < byt_d) {
            return ur_cue_meme;
          }
          else {
            u3i_slab_init(&sab_u, 3, byt_d);
            ur_bsr_bytes_any(red_u, len_d, sab_u.buf_y);
            *out = u3i_slab_mint_bytes(&sab_u);
          }
        }

        _cue_refs_finish(dic_u, _cue_refs_open(dic_u, bit_d), *out);
        return ur_cue_good;
      }
    }
  }
}

struct _u3_cue_xeno {
  _cue_refs dic_u;
};

/* _cs_cue_xeno(): cue on-loom, with off-loom dictionary in handle.
*/
static u3_weak
_cs_cue_xeno(u3_cue_xeno* sil_u,
             c3_d         len_d,
             const c3_y*  byt_y)
{
  ur_bsr_t      red_u = {0};
  _cue_refs* dic_u = &sil_u->dic_u;
  u3a_pile      pil_u;
  _cue_frame_t* fam_u;
  ur_cue_res_e  res_e;
  u3_noun         ref;

  //  initialize stack control
  //
  u3a_pile_prep(&pil_u, sizeof(*fam_u), __alignof__(*fam_u));

  //  init bitstream-reader
  //
  if ( ur_cue_good != (res_e = ur_bsr_init(&red_u, len_d, byt_y)) ) {
    return c3n;
  }
  //  bit-cursor (and backreferences) must fit in 62-bit direct atoms
  //
  //  XX: not 63?
  else if ( 0x7ffffffffffffffULL < len_d ) {
    return c3n;
  }

  //  advance into stream
  //
  res_e = _cs_cue_xeno_next(&pil_u, &red_u, dic_u, &ref);

  //  process cell results
  //
  if (  (c3n == u3a_pile_done(&pil_u))
     && (ur_cue_good == res_e) )
  {
    fam_u = u3a_peek(&pil_u);

    do {
      //  f is a head-frame; stash result and read the tail from the stream
      //
      if ( u3_none == fam_u->ref ) {
        fam_u->ref = ref;
        res_e = _cs_cue_xeno_next(&pil_u, &red_u, dic_u, &ref);
        fam_u = u3a_peek(&pil_u);
      }
      //  f is a tail-frame; pop the stack and continue
      //
      else {
        ref   = u3nc(fam_u->ref, ref);
        _cue_refs_finish(dic_u, fam_u->key_d, ref);
        fam_u = u3a_pop(&pil_u);
      }
    }
    while (  (c3n == u3a_pile_done(&pil_u))
          && (ur_cue_good == res_e) );
  }

  if ( ur_cue_good == res_e ) {
    return ref;
  }
  //  on failure, unwind the stack and dispose of intermediate nouns
  //
  while ( c3n == u3a_pile_done(&pil_u) ) {
    _cue_frame_t* pending = u3a_peek(&pil_u);
    if ( u3_none != pending->ref ) u3z(pending->ref);
    u3a_drop(&pil_u);
  }

  return u3_none;
}

/* u3s_cue_xeno_init_with(): initialize a cue_xeno handle as specified.
*/
u3_cue_xeno*
u3s_cue_xeno_init_with(c3_d pre_d, c3_d siz_d)
{
  u3_cue_xeno* sil_u;

  sil_u = c3_calloc(sizeof(*sil_u));
  _cue_refs_init_external(&sil_u->dic_u, siz_d);

  return sil_u;
}

/* u3s_cue_xeno_init(): initialize a cue_xeno handle.
*/
u3_cue_xeno*
u3s_cue_xeno_init(void)
{
  return u3s_cue_xeno_init_with(ur_fib10, ur_fib11);
}

/* u3s_cue_xeno_init(): cue on-loom, with off-loom dictionary in handle.
*/
u3_weak
u3s_cue_xeno_with(u3_cue_xeno* sil_u,
                  c3_d         len_d,
                  const c3_y*  byt_y)
{
  u3_weak som;

  u3_assert( &(u3H->rod_u) == u3R );

  som = _cs_cue_xeno(sil_u, len_d, byt_y);
  sil_u->dic_u.noun_count = sil_u->dic_u.page_count = 0;
  if ( sil_u->dic_u.directory ) sil_u->dic_u.directory->height = 0;
  return som;
}

/* u3s_cue_xeno_init(): dispose cue_xeno handle.
*/
void
u3s_cue_xeno_done(u3_cue_xeno* sil_u)
{
  _cue_refs_free(&sil_u->dic_u);
  c3_free(sil_u);
}

/* u3s_cue_xeno(): cue on-loom, with off-loom dictionary.
*/
u3_weak
u3s_cue_xeno(c3_d        len_d,
             const c3_y* byt_y)
{
  u3_cue_xeno* sil_u;
  u3_weak        som;

  u3_assert( &(u3H->rod_u) == u3R );

  sil_u = u3s_cue_xeno_init();
  som   = _cs_cue_xeno(sil_u, len_d, byt_y);
  u3s_cue_xeno_done(sil_u);
  return som;
}

/* _cs_cue_need(): bail on ur_cue_* read failures.
*/
static inline void
_cs_cue_need(ur_cue_res_e res_e)
{
  if ( ur_cue_good == res_e ) {
    return;
  }
  else {
    c3_m mot_m = (ur_cue_meme == res_e) ? c3__meme : c3__exit;
    u3m_bail(mot_m);
  }
}

/* _cs_cue_bytes_next(): read next value from bitstream.
*/
static inline u3_noun
_cs_cue_bytes_next(u3a_pile*     pil_u,
                   _cue_refs*    refs,
                   ur_bsr_t*     red_u)
{
  while ( 1 ) {
    c3_d bit_d = red_u->bits;
    ur_cue_token_t token;
    _cs_cue_need(ur_bsr_token(red_u, &token));

    for ( unsigned i = 0; i < token.cells; i++, bit_d += 2 ) {
      _cue_frame_t* fam_u = u3a_push(pil_u);
      fam_u->ref = u3_none;
      fam_u->key_d = _cue_refs_open(refs, bit_d);
    }

    switch ( token.tag ) {
      default: u3_assert(0);
      case ur_jam_cell: continue;

      case ur_jam_back: {
        return u3x_good(_cue_refs_get(refs, token.value));
      }

      case ur_jam_atom: {
        c3_d len_d = token.width;
        u3_atom vat;
        if ( len_d <= 64 ) {
          vat = len_d < u3a_word_bits ? (u3_noun)token.value : u3i_chub(token.value);
        }
        else {
          u3i_slab sab_u;
          u3i_slab_init(&sab_u, 0, len_d);

          ur_bsr_bytes_any(red_u, len_d, sab_u.buf_y);
          vat = u3i_slab_mint_bytes(&sab_u);
        }

        _cue_refs_finish(refs, _cue_refs_open(refs, bit_d), vat);
        return vat;
      }
    }
  }
}

/* u3s_cue_bytes(): cue bytes onto the loom.
*/
u3_noun
u3s_cue_bytes(c3_d len_d, const c3_y* byt_y)
{
  ur_bsr_t      red_u = {0};
  u3a_pile      pil_u;
  _cue_frame_t* fam_u;
  _cue_refs     refs;
  u3_noun         ref;

  //  initialize stack control
  //
  u3a_pile_prep(&pil_u, sizeof(*fam_u), __alignof__(*fam_u));

  _cue_refs_init(&refs);

  //  init bitstream-reader
  //
  _cs_cue_need(ur_bsr_init(&red_u, len_d, byt_y));

  //  bit-cursor (and backreferences) must fit in 62-bit direct atoms
  //  XX: not 63?
  if ( 0x7ffffffffffffffULL < len_d ) {
    return u3m_bail(c3__meme);
  }

  //  advance into stream
  //
  ref = _cs_cue_bytes_next(&pil_u, &refs, &red_u);

  //  process cell results
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
    fam_u = u3a_peek(&pil_u);

    do {
      //  f is a head-frame; stash result and read the tail from the stream
      //
      if ( u3_none == fam_u->ref ) {
        fam_u->ref = ref;
        ref        = _cs_cue_bytes_next(&pil_u, &refs, &red_u);
        fam_u      = u3a_peek(&pil_u);
      }
      //  f is a tail-frame; pop the stack and continue
      //
      else {
        ref   = u3nc(fam_u->ref, ref);
        _cue_refs_finish(&refs, fam_u->key_d, ref);
        fam_u = u3a_pop(&pil_u);
      }
    }
    while ( c3n == u3a_pile_done(&pil_u) );
  }

  _cue_refs_free(&refs);

  return ref;
}

/* u3s_cue_atom(): cue atom.
*/
u3_noun
u3s_cue_atom(u3_atom a)
{
  c3_w  len_w = u3r_met(3, a);
  c3_y* byt_y;

  // XX assumes little-endian
  //
  if ( c3y == u3a_is_cat(a) ) {
     byt_y = (c3_y*)&a;
   }
   else {
    u3a_atom* vat_u = u3a_to_ptr(a);
    byt_y = (c3_y*)vat_u->buf_w;
  }

  return u3s_cue_bytes((c3_d)len_w, byt_y);
}

/* _cs_etch_ud_size(): output length in @ud for given mpz_t.
*/
static inline size_t
_cs_etch_ud_size(mpz_t a_mp)
{
  size_t len_i = mpz_sizeinbase(a_mp, 10);
  return len_i + (len_i / 3); // separators
}

/* _cs_etch_ud_bytes(): atom to @ud impl.
*/
static size_t
_cs_etch_ud_bytes(mpz_t a_mp, size_t len_i, c3_y* hun_y)
{
  c3_y*   buf_y = hun_y + (len_i - 1);
  mpz_t   b_mp;
  c3_w     b_w;

  mpz_init2(b_mp, 10);

  if ( !mpz_size(a_mp) ) {
    *buf_y-- = '0';
  }
  else {
    while ( 1 ) {
      b_w = mpz_tdiv_qr_ui(a_mp, b_mp, a_mp, 1000);
      u3_assert( mpz_get_ui(b_mp) == b_w ); // XX

      if ( !mpz_size(a_mp) ) {
        while ( b_w ) {
          *buf_y-- = '0' + (b_w % 10);
          b_w /= 10;
        }
        break;
      }

      *buf_y-- = '0' + (b_w % 10);
      b_w /= 10;
      *buf_y-- = '0' + (b_w % 10);
      b_w /= 10;
      *buf_y-- = '0' + (b_w % 10);
      *buf_y-- = '.';
    }
  }

  buf_y++;

  u3_assert( buf_y >= hun_y ); // XX

  //  mpz_sizeinbase may overestimate by 1
  //
  {
    size_t dif_i = buf_y - hun_y;

    if ( dif_i ) {
      len_i -= dif_i;
      memmove(hun_y, buf_y, len_i);
      memset(hun_y + len_i, 0, dif_i);
    }
  }

  mpz_clear(b_mp);

  return len_i;
}

/* u3s_etch_ud_smol(): c3_d to @ud
**
**   =(26 (met 3 (scot %ud (dec (bex 64)))))
*/
c3_y*
u3s_etch_ud_smol(c3_d a_d, c3_y hun_y[26])
{
  c3_y*  buf_y = hun_y + 25;
  c3_w     b_w;

  if ( !a_d ) {
    *buf_y-- = '0';
  }
  else {
    while ( 1 ) {
      b_w  = a_d % 1000;
      a_d /= 1000;

      if ( !a_d ) {
        while ( b_w ) {
          *buf_y-- = '0' + (b_w % 10);
          b_w /= 10;
        }
        break;
      }

      *buf_y-- = '0' + (b_w % 10);
      b_w /= 10;
      *buf_y-- = '0' + (b_w % 10);
      b_w /= 10;
      *buf_y-- = '0' + (b_w % 10);
      *buf_y-- = '.';
    }
  }

  return buf_y + 1;
}

/* u3s_etch_ud(): atom to @ud.
*/
u3_atom
u3s_etch_ud(u3_atom a)
{
  c3_d a_d;

  if ( c3y == u3r_safe_chub(a, &a_d) ) {
    c3_y  hun_y[26];
    c3_y* buf_y = u3s_etch_ud_smol(a_d, hun_y);
    c3_w  dif_w = (c3_p)buf_y - (c3_p)hun_y;
    return u3i_bytes(26 - dif_w, buf_y);
  }

  u3i_slab sab_u;
  size_t   len_i;
  mpz_t     a_mp;
  u3r_mp(a_mp, a);

  len_i = _cs_etch_ud_size(a_mp);
  u3i_slab_bare(&sab_u, 3, len_i);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_ud_bytes(a_mp, len_i, sab_u.buf_y);

  mpz_clear(a_mp);
  return u3i_slab_mint_bytes(&sab_u);
}

/* u3s_etch_ud_c(): atom to @ud, as a malloc'd c string.
*/
size_t
u3s_etch_ud_c(u3_atom a, c3_c** out_c)
{
  c3_d     a_d;
  size_t len_i;
  c3_y*  buf_y;

  if ( c3y == u3r_safe_chub(a, &a_d) ) {
    c3_y  hun_y[26];

    buf_y = u3s_etch_ud_smol(a_d, hun_y);
    len_i = 26 - ((c3_p)buf_y - (c3_p)hun_y);

    *out_c = c3_malloc(len_i + 1);
    (*out_c)[len_i] = 0;
    memcpy(*out_c, buf_y, len_i);

    return len_i;
  }

  mpz_t   a_mp;
  u3r_mp(a_mp, a);

  len_i = _cs_etch_ud_size(a_mp);
  buf_y = c3_malloc(len_i + 1);
  buf_y[len_i] = 0;

  len_i = _cs_etch_ud_bytes(a_mp, len_i, buf_y);

  mpz_clear(a_mp);

  *out_c = (c3_c*)buf_y;
  return len_i;
}

/* _cs_etch_ux_bytes(): atom to @ux impl.
*/
static void
_cs_etch_ux_bytes(u3_atom a, c3_w len_w, c3_y* buf_y)
{
  c3_w   i_w;
  c3_s inp_s;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    inp_s = u3r_short(i_w, a);

    *buf_y-- = u3s_dit_y[(inp_s >>  0) & 0xf];
    *buf_y-- = u3s_dit_y[(inp_s >>  4) & 0xf];
    *buf_y-- = u3s_dit_y[(inp_s >>  8) & 0xf];
    *buf_y-- = u3s_dit_y[(inp_s >> 12) & 0xf];
    *buf_y-- = '.';
  }

  inp_s = u3r_short(len_w, a);

  while ( inp_s ) {
    *buf_y-- = u3s_dit_y[inp_s & 0xf];
    inp_s >>= 4;
  }

  *buf_y-- = 'x';
  *buf_y   = '0';
}

/* u3s_etch_ux(): atom to @ux.
*/
u3_atom
u3s_etch_ux(u3_atom a)
{
  if ( u3_blip == a ) {
    return c3_s3('0', 'x', '0');
  }

  c3_w     sep_w = u3r_met(4, a) - 1;                //  number of separators
  c3_w     las_w = u3r_met(2, u3r_short(sep_w, a));  //  digits before separator
  c3_w     len_w = 2 + las_w + (sep_w * 5);          //  output bytes
  u3i_slab sab_u;
  u3i_slab_bare(&sab_u, 3, len_w);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_ux_bytes(a, sep_w, sab_u.buf_y + len_w - 1);

  return u3i_slab_moot_bytes(&sab_u);
}

/* u3s_etch_ux_c(): atom to @ux, as a malloc'd c string.
*/
size_t
u3s_etch_ux_c(u3_atom a, c3_c** out_c)
{
  if ( u3_blip == a ) {
    *out_c = strdup("0x0");
    return 3;
  }

  c3_y*  buf_y;
  c3_w   sep_w = u3r_met(4, a) - 1;
  c3_w   las_w = u3r_met(2, u3r_short(sep_w, a));
  size_t len_i = 2 + las_w + (sep_w * 5);

  buf_y = c3_malloc(1 + len_i);
  buf_y[len_i] = 0;
  _cs_etch_ux_bytes(a, sep_w, buf_y + len_i - 1);

  *out_c = (c3_c*)buf_y;
  return len_i;
}

//  uint div+ceil non-zero
//
#define _divc_nz(x, y)  (((x) + ((y) - 1)) / (y))

/* _cs_etch_uv_size(): output length in @uv (and aligned bits).
*/
static inline size_t
_cs_etch_uv_size(u3_atom a, c3_w* out_w)
{
  c3_w met_w = u3r_met(0, a);
  c3_w sep_w = _divc_nz(met_w, 25) - 1;  //  number of separators
  c3_w max_w = sep_w * 25;
  c3_w end_w = 0;
  u3r_chop(0, max_w, 25, 0, &end_w, a);

  // XX: 64 what do 
  c3_w bit_w = c3_bits_word(end_w);
  c3_w las_w = _divc_nz(bit_w, 5);       //  digits before separator

  *out_w = max_w;
  return 2 + las_w + (sep_w * 6);
}


/* _cs_etch_uv_bytes(): atom to @uv impl.
*/
static void
_cs_etch_uv_bytes(u3_atom a, c3_w max_w, c3_y* buf_y)
{
  c3_w   i_w;
  c3_w inp_w;

  for ( i_w = 0; i_w < max_w; i_w += 25 ) {
    inp_w = 0;
    u3r_chop(0, i_w, 25, 0, &inp_w, a);

    *buf_y-- = u3s_dit_y[(inp_w >>  0) & 0x1f];
    *buf_y-- = u3s_dit_y[(inp_w >>  5) & 0x1f];
    *buf_y-- = u3s_dit_y[(inp_w >> 10) & 0x1f];
    *buf_y-- = u3s_dit_y[(inp_w >> 15) & 0x1f];
    *buf_y-- = u3s_dit_y[(inp_w >> 20) & 0x1f];
    *buf_y-- = '.';
  }

  inp_w = 0;
  u3r_chop(0, max_w, 25, 0, &inp_w, a);

  while ( inp_w ) {
    *buf_y-- = u3s_dit_y[inp_w & 0x1f];
    inp_w >>= 5;
  }

  *buf_y-- = 'v';
  *buf_y   = '0';
}

/* u3s_etch_uv(): atom to @uv.
*/
u3_atom
u3s_etch_uv(u3_atom a)
{
  if ( u3_blip == a ) {
    return c3_s3('0', 'v', '0');
  }

  u3i_slab sab_u;
  c3_w     max_w;
  size_t   len_i = _cs_etch_uv_size(a, &max_w);

  u3i_slab_bare(&sab_u, 3, len_i);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_uv_bytes(a, max_w, sab_u.buf_y + len_i - 1);

  return u3i_slab_moot_bytes(&sab_u);
}

/* u3s_etch_uv_c(): atom to @uv, as a malloc'd c string.
*/
size_t
u3s_etch_uv_c(u3_atom a, c3_c** out_c)
{
  if ( u3_blip == a ) {
    *out_c = strdup("0v0");
    return 3;
  }

  c3_y*  buf_y;
  c3_w   max_w;
  size_t len_i = _cs_etch_uv_size(a, &max_w);

  buf_y = c3_malloc(1 + len_i);
  buf_y[len_i] = 0;
  _cs_etch_uv_bytes(a, max_w, buf_y + len_i - 1);

  *out_c = (c3_c*)buf_y;
  return len_i;
}

/* _cs_etch_uw_size(): output length in @uw (and aligned bits).
*/
static inline size_t
_cs_etch_uw_size(u3_atom a, c3_w* out_w)
{
  c3_w met_w = u3r_met(0, a);
  c3_w sep_w = _divc_nz(met_w, 30) - 1;  //  number of separators
  c3_w max_w = sep_w * 30;
  c3_w end_w = 0;
  u3r_chop(0, max_w, 30, 0, &end_w, a);

  // XX: 64 what do
  c3_w bit_w = c3_bits_word(end_w);
  c3_w las_w = _divc_nz(bit_w, 6);       //  digits before separator

  *out_w = max_w;
  return 2 + las_w + (sep_w * 6);
}

/* _cs_etch_uw_bytes(): atom to @uw impl.
*/
static void
_cs_etch_uw_bytes(u3_atom a, c3_w max_w, c3_y* buf_y)
{
  c3_w   i_w;
  c3_w inp_w;

  for ( i_w = 0; i_w < max_w; i_w += 30 ) {
    inp_w = 0;
    u3r_chop(0, i_w, 30, 0, &inp_w, a);

    *buf_y-- = u3s_dit_y[(inp_w >>  0) & 0x3f];
    *buf_y-- = u3s_dit_y[(inp_w >>  6) & 0x3f];
    *buf_y-- = u3s_dit_y[(inp_w >> 12) & 0x3f];
    *buf_y-- = u3s_dit_y[(inp_w >> 18) & 0x3f];
    *buf_y-- = u3s_dit_y[(inp_w >> 24) & 0x3f];
    *buf_y-- = '.';
  }

  inp_w = 0;
  u3r_chop(0, max_w, 30, 0, &inp_w, a);

  while ( inp_w ) {
    *buf_y-- = u3s_dit_y[inp_w & 0x3f];
    inp_w >>= 6;
  }

  *buf_y-- = 'w';
  *buf_y   = '0';
}

/* u3s_etch_uw(): atom to @uw.
*/
u3_atom
u3s_etch_uw(u3_atom a)
{
  if ( u3_blip == a ) {
    return c3_s3('0', 'w', '0');
  }

  u3i_slab sab_u;
  c3_w     max_w;
  size_t   len_i = _cs_etch_uw_size(a, &max_w);

  u3i_slab_bare(&sab_u, 3, len_i);
  sab_u.buf_w[sab_u.len_w - 1] = 0;

  _cs_etch_uw_bytes(a, max_w, sab_u.buf_y + len_i - 1);

  return u3i_slab_moot_bytes(&sab_u);
}

/* u3s_etch_uw_c(): atom to @uw, as a malloc'd c string.
*/
size_t
u3s_etch_uw_c(u3_atom a, c3_c** out_c)
{
  if ( u3_blip == a ) {
    *out_c = strdup("0w0");
    return 3;
  }

  c3_y*  buf_y;
  c3_w   max_w;
  size_t len_i = _cs_etch_uw_size(a, &max_w);

  buf_y = c3_malloc(1 + len_i);
  buf_y[len_i] = 0;
  _cs_etch_uw_bytes(a, max_w, buf_y + len_i - 1);

  *out_c = (c3_c*)buf_y;
  return len_i;
}

#undef _divc_nz

#define DIGIT(a) ( ((a) >= '0') && ((a) <= '9') )
#define BLOCK(a) (  ('.' == (a)[0]) \
                 && DIGIT(a[1])     \
                 && DIGIT(a[2])     \
                 && DIGIT(a[3])     )

/* u3s_sift_ud_bytes: parse @ud
*/
u3_weak
u3s_sift_ud_bytes(c3_w len_w, c3_y* byt_y)
{
  c3_y num_y = len_w % 4;  //  leading digits length
  c3_s val_s = 0;          //  leading digits value

  //  +ape:ag: just 0
  //
  if ( !len_w )        return u3_none;
  if ( '0' == *byt_y ) return ( 1 == len_w ) ? (u3_noun)0 : u3_none;

  //  +ted:ab: leading nonzero (checked above), plus up to 2 digits
  //
#define NEXT()  do {                          \
    if ( !DIGIT(*byt_y) ) return u3_none;     \
    val_s *= 10;                              \
    val_s += *byt_y++ - '0';                  \
  } while (0)

  switch ( num_y ) {
    case 3: NEXT(); [[fallthrough]];
    case 2: NEXT(); [[fallthrough]];
    case 1: NEXT(); break;
    case 0: return u3_none;
  }

#undef NEXT

  len_w -= num_y;

  //  +tid:ab: dot-prefixed 3-digit blocks
  //
  //    avoid gmp allocation if possible
  //      - 19 decimal digits fit in 64 bits
  //      - 18 digits is 24 bytes with separators
  //
  if (  ((1 == num_y) && (24 >= len_w))
     || (20 >= len_w) )
  {
    c3_d val_d = val_s;

    while ( len_w ) {
      if ( !BLOCK(byt_y) ) return u3_none;

      byt_y++;

      val_d *= 10;
      val_d += *byt_y++ - '0';
      val_d *= 10;
      val_d += *byt_y++ - '0';
      val_d *= 10;
      val_d += *byt_y++ - '0';

      len_w -= 4;
    }

    return u3i_chub(val_d);
  }

  {
    //  avoid gmp realloc if possible
    //
    mpz_t a_mp;
    {
      c3_d bit_d = (c3_d)(len_w / sizeof(c3_w)) * 10;
      mpz_init2(a_mp, (c3_w)c3_min(bit_d, c3_w_max));
      mpz_set_ui(a_mp, val_s);
    }

    while ( len_w ) {
      if ( !BLOCK(byt_y) ) {
        mpz_clear(a_mp);
        return u3_none;
      }

      byt_y++;

      val_s  = *byt_y++ - '0';
      val_s *= 10;
      val_s += *byt_y++ - '0';
      val_s *= 10;
      val_s += *byt_y++ - '0';

      mpz_mul_ui(a_mp, a_mp, 1000);
      mpz_add_ui(a_mp, a_mp, val_s);

      len_w -= 4;
    }

    return u3i_mp(a_mp);
  }
}

#undef BLOCK
#undef DIGIT

/* u3s_sift_ud: parse @ud.
*/
u3_weak
u3s_sift_ud(u3_atom a)
{
  c3_w  len_w = u3r_met(3, a);
  c3_y* byt_y;

  // XX assumes little-endian
  //
  if ( c3y == u3a_is_cat(a) ) {
     byt_y = (c3_y*)&a;
   }
   else {
    u3a_atom* vat_u = u3a_to_ptr(a);
    byt_y = (c3_y*)vat_u->buf_w;
  }

  return u3s_sift_ud_bytes(len_w, byt_y);
}
