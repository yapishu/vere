/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"

/* +take follows a vein from its tail. Expand its axes into directions in
** leaf-to-root order. Zero marks a face to preserve at the selected slot.
** https://github.com/urbit/urbit/blob/bd3973dc97c0f4ac8eae5fddfa15c39d522eb8d0/pkg/arvo/sys/hoon.hoon#L10875-L10938
*/
typedef struct {
  c3_y steps[64];
  c3_w len_w;
  c3_t slot;
} _take_path;

static u3_weak
_take_axis(u3_noun vein, _take_path* path)
{
  while ( u3_nul != vein ) {
    u3_noun unit = u3h(vein);
    vein = u3t(vein);
    if ( u3_nul == unit ) {
      if ( path->len_w == sizeof(path->steps) ) return u3_none;
      path->steps[path->len_w++] = 0;
    }
    else {
      u3_noun axe = u3t(unit);
      if ( c3n == u3a_is_cat(axe) || !axe ) return u3_none;
      while ( axe > 1 ) {
        if ( path->len_w == sizeof(path->steps) ) return u3_none;
        path->steps[path->len_w++] = 2 + (axe & 1);
        axe >>= 1;
      }
    }
  }

  c3_w axe_w = 1;
  for ( c3_w i = path->len_w; i; i-- ) {
    c3_y step = path->steps[i-1];
    if ( step ) {
      if ( axe_w >= (1U << 30) ) return u3_none;
      axe_w = 2 * axe_w + (step & 1);
    }
  }
  return axe_w;
}

/* +cool uses +take to intersect or subtract an atom singleton at a vein.
** +toss uses the same walk with a constant replacement.
** https://github.com/urbit/urbit/blob/5a187feded/pkg/arvo/sys/hoon.hoon#L9181-L9191
*/
typedef struct _take_hold {
  u3_noun sut;
  const struct _take_hold* prev;
} _take_hold;

typedef struct {
  u3_noun van;
  c3_o how;
  c3_w fuel;
  const _take_hold* holds;
} _take_refine;

static u3_weak
_take(u3_noun sut, u3_noun ref, const _take_path* path, c3_w len,
      c3_w depth, _take_refine* walk);

/* Prepending in left-root-right order gives ~(tap in set) directly. */
static c3_o
_take_fork(u3_noun set, u3_noun ref, const _take_path* path, c3_w len,
           c3_w depth, _take_refine* walk, u3_noun* types)
{
  if ( !set ) return c3y;
  u3_noun typ, lef, rit;
  if ( !depth || !walk->fuel || c3n == u3r_trel(set, &typ, &lef, &rit) ) {
    return c3n;
  }
  --walk->fuel;
  //  Descending into a type can unify siblings in this set.
  //
  u3k(typ); u3k(rit);
  c3_o ok = _take_fork(lef, ref, path, len, depth-1, walk, types);
  if ( c3y == ok ) {
    u3_weak pro = _take(typ, ref, path, len, depth-1, walk);
    if ( pro == u3_none ) ok = c3n;
    else {
      *types = u3nc(pro, *types);
      ok = _take_fork(rit, ref, path, len, depth-1, walk, types);
    }
  }
  u3z(rit); u3z(typ);
  return ok;
}

/* +repo calls +rest on the leg. Leave recursion and unknown expansions
** to Hoon, including its original trace on a loop.
*/
static u3_weak
_take_hold_type(u3_noun sut, u3_noun ref, const _take_path* path, c3_w len,
                c3_w depth, _take_refine* walk)
{
  for ( const _take_hold* at = walk->holds; at; at = at->prev ) {
    if ( c3y == u3r_sing(at->sut, sut) ) return u3_none;
  }
  u3_weak fan = u3r_at(u3qfu_van_fan, walk->van);
  if ( fan == u3_none || c3n == u3du(u3t(sut)) ||
       c3y == u3qdi_has(fan, u3t(sut)) ) return u3_none;
  u3_weak typ = u3qfu_rest_find(walk->van, sut, 135);
  if ( typ == u3_none ) return u3_none;
  _take_hold here = {u3k(sut), walk->holds};
  walk->holds = &here;
  u3_weak pro = _take(typ, ref, path, len, depth, walk);
  walk->holds = here.prev;
  u3z(here.sut); u3z(typ);
  return pro;
}

/* +fuse selects the more specific aura. +crop can remove a singleton
** from a constant atom, but represents a general atom with its original type.
*/
static u3_weak
_take_literal(u3_noun sut, u3_noun tag, u3_noun p, u3_noun q,
              u3_noun ref, c3_o how)
{
  if ( tag == c3__void ) return c3__void;
  if ( tag == c3__cell || tag == c3__core ) return how == c3y ? c3__void : u3k(sut);
  if ( tag == c3__noun ) {
    if ( how == c3y ) return u3k(ref);
    return u3kf_fork(u3nt(u3nt(c3__atom, 0, 0),
                          u3nt(c3__cell, c3__noun, c3__noun), u3_nul));
  }
  if ( tag != c3__atom || c3n == u3ud(p) ) return u3_none;
  u3_noun aura = u3h(u3t(ref)), unit = u3t(u3t(ref));
  if ( q ) {
    if ( c3n == u3du(q) || u3h(q) || c3n == u3ud(u3t(q)) ) return u3_none;
    if ( c3n == u3r_sing(q, unit) ) return how == c3y ? c3__void : u3k(sut);
  }
  if ( how == c3n ) return q ? c3__void : u3k(sut);
  return u3nt(c3__atom, u3k(c3y == u3qf_fitz(aura, p) ? p : aura), u3k(unit));
}

static u3_weak
_take(u3_noun sut, u3_noun ref, const _take_path* path, c3_w len,
      c3_w depth, _take_refine* walk)
{
  if ( !len && !walk ) return u3k(ref);
  if ( !depth || (walk && !walk->fuel) ) return u3_none;
  if ( walk ) --walk->fuel;

  c3_y step = len ? path->steps[len-1] : 0;
  u3_noun tag = sut, p = c3__noun, q = c3__noun;
  if ( sut != c3__noun && sut != c3__void ) {
    if ( c3n == u3r_cell(sut, &tag, &q) ||
         tag == c3__noun || tag == c3__void ) return u3_none;
    if ( tag == c3__hold ) {
      return walk ? _take_hold_type(sut, ref, path, len, depth-1, walk) : u3_none;
    }
    if ( tag == c3__fork ) {
      u3_noun types = u3_nul;
      if ( !walk || !q || c3n == _take_fork(q, ref, path, len, depth-1, walk, &types) ) {
        u3z(types);
        return u3_none;
      }
      return u3kf_fork(types);
    }
    if ( c3n == u3r_cell(q, &p, &q) ) return u3_none;
  }
  if ( tag == c3__face || tag == c3__hint ) {
    c3_w next = len - (len && !step && tag == c3__face);
    u3k(p);
    u3_weak child = _take(q, ref, path, next, depth-1, walk);
    u3_weak pro = child;
    if ( child != u3_none ) {
      pro = tag == c3__face ? u3qf_face(p, child) : u3qf_hint(p, child);
      u3z(child);
    }
    u3z(p);
    return pro;
  }
  if ( !len ) return _take_literal(sut, tag, p, q, ref, walk->how);
  if ( !step ) {
    if ( path->slot && tag != c3__atom && tag != c3__cell &&
         tag != c3__core && tag != c3__noun ) return u3_none;
    return _take(sut, ref, path, len-1, depth-1, walk);
  }
  if ( tag == c3__atom || tag == c3__void ) return path->slot ? u3_none : c3__void;
  if ( path->slot && tag == c3__core &&
       (step == 2 || u3r_at(59, sut) != c3__gold) ) return u3_none;
  if ( tag == c3__core && step == 2 ) {
    tag = c3__cell;
    q = p;
    p = c3__noun;
  }
  if ( tag != c3__core && tag != c3__cell && tag != c3__noun ) return u3_none;

  c3_t tail = tag != c3__core && step == 3;
  u3_noun other = u3k(tail ? p : q);
  u3_weak child = _take(tail ? q : p, ref, path, len-1, depth-1, walk);
  u3_weak pro = child;
  if ( child != u3_none && child != c3__void ) {
    if ( tag == c3__core ) return u3nt(c3__core, child, other);
    pro = tail ? u3qf_cell(other, child) : u3qf_cell(child, other);
    u3z(child);
  }
  u3z(other);
  return pro;
}

u3_weak
u3qfu_cool_literal(u3_noun van, u3_noun sut, c3_o how, u3_noun wing, u3_noun ref)
{
  if ( u3r_at(u3qfu_van_vet, van) > 1 ) return u3_none;
  u3k(sut); u3k(van);
  u3_weak port = u3qfu_fond(van, sut, c3__both, wing, 135);
  u3_weak pro = u3_none;
  if ( port != u3_none ) {
    if ( c3y == u3du(port) && c3y == u3h(port) ) {
      _take_path path = {0};
      if ( _take_axis(u3h(u3t(port)), &path) != u3_none ) {
        _take_refine walk = {.van = van, .how = how, .fuel = 512};
        pro = _take(sut, ref, &path, path.len_w, 128, &walk);
      }
    }
    u3z(port);
  }
  u3z(van); u3z(sut);
  return pro;
}

/* +open:ap addresses a gate argument by an axis and an anonymous limb.
** +fond first projects the axis with %rite access. The anonymous limb
** keeps the face at its tip. +take checks that path while rebuilding it.
** https://github.com/urbit/urbit/blob/5a187feded/pkg/arvo/sys/hoon.hoon#L8507-L8519
*/
static u3_weak
_toss_slot(u3_noun sut, u3_noun wing, u3_noun mur, u3_noun foot)
{
  u3_noun anon, limb, end, skip, name, axe;
  if ( c3n == u3r_trel(wing, &anon, &limb, &end) || end ||
       c3n == u3r_pq(anon, c3n, &skip, &name) || skip || name ||
       c3n == u3r_p(limb, c3y, &axe) || !axe || c3n == u3a_is_cat(axe) ) {
    return u3_none;
  }

  _take_path path = {.len_w = 1, .slot = 1};
  for ( c3_w at = axe; at > 1; at >>= 1 ) {
    path.steps[path.len_w++] = 2 + (at & 1);
  }
  u3_weak typ = _take(sut, mur, &path, path.len_w, 128, 0);
  return typ == u3_none ? u3_none : u3nt(axe, u3nc(typ, u3k(foot)), u3_nul);
}

/* +toss checks that all candidate arms agree on the edit axis. With one
** arm the axis follows directly from +fond. Multiple arms use Hoon.
*/
static u3_weak
_toss(u3_noun cor, c3_y kel_y)
{
  u3_noun wing, mur, men, van, sut, foot;
  if ( c3n == u3r_mean(cor, {u3x_sam_2, &wing}, {u3x_sam_6, &mur},
                            {u3x_sam_7, &men}, {u3x_con, &van}) ||
       c3n == u3du(men) || u3_nul != u3t(men) ||
       c3n == u3r_cell(u3h(men), &sut, &foot) ) return u3_none;

  u3_weak slot = _toss_slot(sut, wing, mur, foot);
  if ( slot != u3_none ) return slot;

  u3k(sut); u3k(foot); u3k(mur);
  u3_weak port = u3qfu_fond(van, sut, c3__rite, wing, kel_y);
  u3_weak pro = u3_none;
  if ( port != u3_none ) {
    if ( port != u3_nul && u3h(port) == c3y ) {
      _take_path path = {0};
      u3_weak axe = _take_axis(u3h(u3t(port)), &path);
      if ( axe != u3_none ) {
        u3_weak typ = _take(sut, mur, &path, path.len_w, 128, 0);
        if ( typ != u3_none ) pro = u3nt(axe, u3nc(typ, u3k(foot)), u3_nul);
      }
    }
    u3z(port);
  }
  u3z(mur); u3z(foot); u3z(sut);
  return pro;
}

u3_weak
u3wfu_toss_135(u3_noun cor)
{
  return _toss(cor, 135);
}

u3_weak
u3wfu_toss_137(u3_noun cor)
{
  return _toss(cor, 137);
}
