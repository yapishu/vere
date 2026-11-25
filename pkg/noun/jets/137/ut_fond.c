/// @file

#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"

/* +fond resolves a wing from right to left. A natural match carries a vein
** (the path through the subject) and either a leg type or a set of arms.
** The direct walk only produces singleton arm sets. Any unresolved step
** returns u3_none so the interpreter evaluates the wing from the beginning.
** https://github.com/urbit/urbit/blob/bd3973dc97c0f4ac8eae5fddfa15c39d522eb8d0/pkg/arvo/sys/hoon.hoon#L9376-L9535
*/

static u3_noun
_fond_miss(void)
{
  return u3nt(c3n, c3y, 0);
}

static u3_weak
_fond_name(u3_noun sut, u3_noun name, u3_noun way, u3_noun vein,
           c3_w axe_w, c3_w dep_w, c3_w* fuel_w)
{
  u3_noun tag, p, q;
  if ( !dep_w || !*fuel_w || axe_w >= (1U << 29) ) return u3_none;
  (*fuel_w)--;

  if ( sut == c3__void ) return u3_nul;
  if ( sut == c3__noun ) return _fond_miss();
  if ( c3n == u3r_trel(sut, &tag, &p, &q) ) return u3_none;

  switch ( tag ) {
    case c3__atom: return _fond_miss();
    case c3__hint: return _fond_name(q, name, way, vein, axe_w, dep_w-1, fuel_w);
    case c3__face: {
      if ( c3n == u3ud(p) ) return u3_none;
      if ( c3n == u3r_sing(p, name) ) return _fond_miss();
      u3_noun path = u3nt(u3_nul, u3nc(u3_nul, axe_w), u3k(vein));
      return u3nq(c3y, path, c3y, u3k(q));
    }
    case c3__cell: {
      u3_weak hed = _fond_name(p, name, way, vein, 2*axe_w, dep_w-1, fuel_w);
      if ( hed == u3_none || hed == u3_nul || u3h(hed) == c3y ) return hed;
      u3z(hed);
      return _fond_name(q, name, way, vein, 2*axe_w+1, dep_w-1, fuel_w);
    }
    case c3__core: {
      u3_noun pol, met, dom;
      if ( c3n == u3r_mean(sut, {58, &pol}, {59, &met}, {63, &dom}) ||
           (pol != c3__wet && pol != c3__dry) ) return u3_none;

      u3_noun arm = u3qf_loot_136(name, dom);
      if ( arm != u3_nul ) {
        u3_noun hit = u3t(arm);
        u3_noun axe = u3qc_peg(2, u3h(hit));
        u3_noun leg = u3nc(u3k(sut), u3nc(pol, u3k(u3t(hit))));
        u3_noun set = u3nt(leg, u3_nul, u3_nul);
        u3_noun path = u3nc(u3nc(u3_nul, axe_w), u3k(vein));
        u3z(arm);
        return u3nq(c3y, path, c3n, u3nc(axe, set));
      }
      if ( met == c3__gold || way == c3__free ) {
        return _fond_name(p, name, way, vein, 2*axe_w+1, dep_w-1, fuel_w);
      }
      return u3_none;
    }
    default: return u3_none;
  }
}

static u3_weak
_fond_axis(u3_noun sut, u3_noun way, c3_w axe_w)
{
  if ( !axe_w ) return u3_none;
  c3_w bit_w = ((c3_w)1 << (c3_bits_word(axe_w) - 1)) >> 1;
  for ( c3_w dep_w = 0; bit_w && dep_w < 80; dep_w++ ) {
    if ( sut == c3__noun || sut == c3__void ) return sut;
    u3_noun tag, p, q;
    if ( c3n == u3r_trel(sut, &tag, &p, &q) ) return u3_none;
    switch ( tag ) {
      case c3__hint: case c3__face: sut = q; continue;
      case c3__atom: return c3__void;
      case c3__cell: sut = (axe_w & bit_w) ? q : p; break;
      case c3__core:
        if ( !(axe_w & bit_w) ) return c3__noun;
        if ( way != c3__free && u3r_at(59, sut) != c3__gold ) return u3_none;
        sut = p;
        break;
      default: return u3_none;
    }
    bit_w >>= 1;
  }
  return bit_w ? u3_none : u3k(sut);
}

u3_weak
u3wfu_fond_137(u3_noun cor)
{
  u3_noun way, hyp, sut;
  if ( c3n == u3r_mean(cor, {u3x_sam_2, &way}, {u3x_sam_3, &hyp},
                            {u3x_con_sam, &sut}) ||
       (way != c3__read && way != c3__rite && way != c3__free && way != c3__both) ) {
    return u3_none;
  }

  u3_noun limbs[16];
  c3_w len_w = 0, fuel_w = 256;
  while ( hyp != u3_nul ) {
    if ( len_w == 16 || c3n == u3du(hyp) ) return u3_none;
    limbs[len_w++] = u3h(hyp);
    hyp = u3t(hyp);
  }

  u3_noun port = u3nq(c3y, u3_nul, c3y, u3k(sut));
  while ( len_w ) {
    if ( port == u3_nul ) return u3_none;
    if ( u3h(port) == c3n ) return port;
    u3_noun vein = u3h(u3t(port)), match = u3t(u3t(port));
    if ( u3h(match) == c3y ) sut = u3t(match);
    else {
      u3_noun set = u3t(u3t(match));
      sut = u3h(u3h(set));
    }

    u3_noun limb = limbs[--len_w];
    u3_weak next = u3_none;
    if ( c3y == u3ud(limb) ) {
      next = _fond_name(sut, limb, way, vein, 1, 40, &fuel_w);
    }
    else if ( u3h(limb) == c3y && c3y == u3a_is_cat(u3t(limb)) ) {
      c3_w axe_w = u3t(limb);
      u3_weak typ = _fond_axis(sut, way, axe_w);
      if ( typ != u3_none ) {
        next = u3nq(c3y, u3nc(u3nc(u3_nul, axe_w), u3k(vein)), c3y, typ);
      }
    }
    else {
      u3_noun tag, skip, unit, mark, name;
      if ( c3y == u3r_trel(limb, &tag, &skip, &unit) && tag == c3n &&
           skip == 0 && c3y == u3r_cell(unit, &mark, &name) &&
           mark == u3_nul && c3y == u3ud(name) ) {
        next = _fond_name(sut, name, way, vein, 1, 40, &fuel_w);
      }
    }
    u3z(port);
    if ( next == u3_none ) return u3_none;
    port = next;
  }
  return port;
}
