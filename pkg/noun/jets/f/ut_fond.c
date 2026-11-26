/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"

/* +fond resolves a wing from right to left. A natural match carries a vein
** (the path through the subject) and either a leg type or a set of arms.
** An unresolved step returns u3_none so Hoon evaluates the whole wing.
** https://github.com/urbit/urbit/blob/bd3973dc97c0f4ac8eae5fddfa15c39d522eb8d0/pkg/arvo/sys/hoon.hoon#L9376-L9535
*/

static u3_noun
_fond_miss(void)
{
  return u3nt(c3n, c3y, 0);
}

typedef struct _fond_seen {
  u3_noun hold;
  const struct _fond_seen* parent;
} _fond_seen;

typedef struct {
  u3_noun van;
  const _fond_seen* seen;
  c3_w fuel;
  c3_y kel_y;
} _fond_walk;

static u3_weak
_fond_name(u3_noun sut, u3_noun name, u3_noun way, u3_noun vein,
           c3_w axe_w, c3_w dep_w, _fond_walk* walk);

/* +twin joins natural matches at the same vein. A punt lets Hoon
** produce its own trace for conflicting matches.
*/
static u3_weak
_fond_twin(u3_noun a, u3_noun b)
{
  if ( a == u3_nul ) return u3k(b);
  if ( b == u3_nul || c3y == u3r_sing(a, b) ) return u3k(a);
  if ( u3h(a) != c3y || u3h(b) != c3y ) return u3_none;

  u3_noun path = u3h(u3t(a));
  u3_noun x = u3t(u3t(a)), y = u3t(u3t(b));
  if ( c3n == u3r_sing(path, u3h(u3t(b))) || u3h(x) != u3h(y) ) {
    return u3_none;
  }
  if ( u3h(x) == c3y ) {
    u3_noun typ = u3kf_fork(u3nt(u3k(u3t(x)), u3k(u3t(y)), u3_nul));
    return u3nq(c3y, u3k(path), c3y, typ);
  }
  x = u3t(x);
  y = u3t(y);
  if ( c3n == u3r_sing(u3h(x), u3h(y)) ) return u3_none;
  return u3nq(c3y, u3k(path), c3n,
              u3nc(u3k(u3h(x)), u3qdi_uni(u3t(x), u3t(y))));
}

/* Fold +twin from the end of ~(tap in set), without allocating its list.
** On a punt the caller still owns the accumulated port.
*/
static c3_o
_fond_fork(u3_noun set, u3_noun name, u3_noun way, u3_noun vein,
           c3_w axe_w, c3_w dep_w, _fond_walk* walk, u3_noun* port)
{
  if ( set == u3_nul ) return c3y;
  u3_noun typ, lef, rit;
  if ( !dep_w || !walk->fuel || c3n == u3r_trel(set, &typ, &lef, &rit) ||
       c3n == _fond_fork(lef, name, way, vein, axe_w, dep_w-1, walk, port) ) {
    return c3n;
  }
  u3_weak hit = _fond_name(typ, name, way, vein, axe_w, dep_w-1, walk);
  if ( hit == u3_none ) return c3n;
  u3_weak pro = _fond_twin(hit, *port);
  u3z(hit);
  if ( pro == u3_none ) return c3n;
  u3z(*port);
  *port = pro;
  return _fond_fork(rit, name, way, vein, axe_w, dep_w-1, walk, port);
}

/* +repo uses a leg at Kelvin 135 and a list of legs at Kelvin 137.
** Reuse a completed expansion
** on this road. Unknown expansions and fan cycles go through Hoon.
*/
static u3_weak
_fond_hold(u3_noun sut, u3_noun name, u3_noun way, u3_noun vein,
           c3_w axe_w, c3_w dep_w, _fond_walk* walk)
{
  u3_noun item = u3t(sut);
  u3_weak fan = u3r_at(u3qfu_van_fan, walk->van);
  if ( c3n == u3du(item) || fan == u3_none ||
       u3r_at(u3qfu_van_vet, walk->van) > 1 ) return u3_none;

  for ( const _fond_seen* at = walk->seen; at; at = at->parent ) {
    if ( c3y == u3r_sing(at->hold, sut) ) return u3_nul;
  }
  if ( c3y == u3qdi_has(fan, item) ) return u3_none;

  u3_weak typ = u3qfu_rest_find(walk->van, sut, walk->kel_y);
  if ( typ == u3_none ) return u3_none;

  _fond_seen here = {sut, walk->seen};
  walk->seen = &here;
  u3_weak pro = _fond_name(typ, name, way, vein, axe_w, dep_w-1, walk);
  walk->seen = here.parent;
  u3z(typ);
  return pro;
}

static u3_weak
_fond_name(u3_noun sut, u3_noun name, u3_noun way, u3_noun vein,
           c3_w axe_w, c3_w dep_w, _fond_walk* walk)
{
  u3_noun tag, p, q;
  if ( !dep_w || !walk->fuel || axe_w >= (1U << 29) ) return u3_none;
  walk->fuel--;

  if ( sut == c3__void ) return u3_nul;
  if ( sut == c3__noun ) return _fond_miss();
  if ( c3n == u3du(sut) ) return u3_none;
  if ( u3h(sut) == c3__hold ) {
    return _fond_hold(sut, name, way, vein, axe_w, dep_w, walk);
  }
  if ( u3h(sut) == c3__fork ) {
    u3_noun port = u3_nul;
    if ( c3y == _fond_fork(u3t(sut), name, way, vein, axe_w, dep_w, walk, &port) ) {
      return port;
    }
    u3z(port);
    return u3_none;
  }
  if ( c3n == u3r_trel(sut, &tag, &p, &q) ) return u3_none;

  switch ( tag ) {
    case c3__atom: return _fond_miss();
    case c3__hint: return _fond_name(q, name, way, vein, axe_w, dep_w-1, walk);
    case c3__face: {
      if ( c3n == u3ud(p) ) return u3_none;
      if ( c3n == u3r_sing(p, name) ) return _fond_miss();
      u3_noun path = u3nt(u3_nul, u3nc(u3_nul, axe_w), u3k(vein));
      return u3nq(c3y, path, c3y, u3k(q));
    }
    case c3__cell: {
      u3_weak hed = _fond_name(p, name, way, vein, 2*axe_w, dep_w-1, walk);
      if ( hed == u3_none || hed == u3_nul || u3h(hed) == c3y ) return hed;
      u3z(hed);
      return _fond_name(q, name, way, vein, 2*axe_w+1, dep_w-1, walk);
    }
    case c3__core: {
      u3_noun pol, met, dom;
      if ( c3n == u3r_mean(sut, {58, &pol}, {59, &met}, {63, &dom}) ||
           (pol != c3__wet && pol != c3__dry) ) return u3_none;

      //  Kelvin 135 stores the arm map directly in each chapter.
      //
      u3_noun arm = (135 == walk->kel_y) ? u3qf_loot(name, dom)
                                         : u3qf_loot_136(name, dom);
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
        return _fond_name(p, name, way, vein, 2*axe_w+1, dep_w-1, walk);
      }
      return u3_none;
    }
    default: return u3_none;
  }
}

/* An anonymous limb selects the current type through one name. Hints
** are transparent. A fork or hold defers to the full wing evaluator.
*/
static u3_weak
_fond_anon(u3_noun sut, u3_noun skip, u3_noun vein)
{
  for ( c3_w i = 0; i < 40; i++ ) {
    if ( sut == c3__void ) return u3_nul;
    if ( sut != c3__noun ) {
      u3_noun tag, p, q;
      if ( c3n == u3r_trel(sut, &tag, &p, &q) ) return u3_none;
      if ( tag == c3__hint ) { sut = q; continue; }
      if ( tag == c3__face ) sut = q;
      else if ( tag != c3__atom && tag != c3__cell && tag != c3__core ) return u3_none;
    }
    if ( skip ) return u3nt(c3n, c3y, skip - 1);
    return u3nq(c3y, u3nt(u3_nul, u3nc(u3_nul, 1), u3k(vein)), c3y, u3k(sut));
  }
  return u3_none;
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
u3qfu_fond(u3_noun van, u3_noun sut, u3_noun way, u3_noun hyp, c3_y kel_y)
{
  u3_noun limbs[16];
  c3_w len_w = 0;
  _fond_walk walk = {.van = van, .fuel = 256, .kel_y = kel_y};
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
      if ( u3h(u3t(set)) != u3_nul || u3t(u3t(set)) != u3_nul ) {
        u3z(port);
        return u3_none;
      }
      sut = u3h(u3h(set));
    }

    u3_noun limb = limbs[--len_w];
    u3_weak next = u3_none;
    if ( c3y == u3ud(limb) ) {
      next = _fond_name(sut, limb, way, vein, 1, 40, &walk);
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
           c3y == u3a_is_cat(skip) ) {
        if ( unit == u3_nul ) {
          next = _fond_anon(sut, skip, vein);
        }
        else if ( !skip && c3y == u3r_cell(unit, &mark, &name) &&
                  mark == u3_nul && c3y == u3ud(name) ) {
          next = _fond_name(sut, name, way, vein, 1, 40, &walk);
        }
      }
    }
    u3z(port);
    if ( next == u3_none ) return u3_none;
    port = next;
  }
  return port;
}

static u3_weak
_fond_gate(u3_noun cor, c3_y kel_y)
{
  u3_noun way, hyp, sut;
  if ( c3n == u3r_mean(cor, {u3x_sam_2, &way}, {u3x_sam_3, &hyp},
                            {u3x_con_sam, &sut}) ||
       (way != c3__read && way != c3__rite && way != c3__free && way != c3__both) ) {
    return u3_none;
  }

  return u3qfu_fond(u3x_at(u3x_con, cor), sut, way, hyp, kel_y);
}

u3_weak
u3wfu_fond_135(u3_noun cor)
{
  return _fond_gate(cor, 135);
}

u3_weak
u3wfu_fond_137(u3_noun cor)
{
  return _fond_gate(cor, 137);
}
