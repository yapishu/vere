/// @file

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
} _toss_path;

static u3_weak
_toss_axis(u3_noun vein, _toss_path* path)
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

/* Rebuild the enclosing types on return. A face consumes a zero step,
** while a hint preserves the step for the type underneath it.
*/
static u3_weak
_toss_take(u3_noun sut, u3_noun mur, const c3_y* steps, c3_w len_w, c3_w fuel)
{
  if ( !len_w ) return u3k(mur);
  if ( !fuel ) return u3_none;

  c3_y step = steps[len_w-1];
  u3_noun tag = sut, p = c3__noun, q = c3__noun;
  if ( sut != c3__noun && sut != c3__void ) {
    if ( c3n == u3r_trel(sut, &tag, &p, &q) ) return u3_none;
  }

  if ( c3__face == tag || c3__hint == tag ) {
    c3_w len = len_w - (c3__face == tag && !step);
    u3_weak child = _toss_take(q, mur, steps, len, fuel-1);
    if ( child == u3_none ) return u3_none;
    u3_noun pro = (c3__face == tag) ? u3qf_face(p, child) : u3qf_hint(p, child);
    u3z(child);
    return pro;
  }

  if ( c3__hold == tag || c3__fork == tag ) return u3_none;
  if ( !step ) return _toss_take(sut, mur, steps, len_w-1, fuel-1);
  if ( c3__atom == tag || c3__void == tag ) return c3__void;

  //  +repo of a core exposes a cell with an unrestricted battery.
  //
  if ( tag == c3__core && step == 2 ) {
    tag = c3__cell;
    q = p;
    p = c3__noun;
  }
  if ( tag != c3__core && tag != c3__cell && tag != c3__noun ) return u3_none;

  c3_t tail = (tag != c3__core && step == 3);
  u3_weak child = _toss_take(tail ? q : p, mur, steps, len_w-1, fuel-1);
  if ( child == u3_none ) return u3_none;
  if ( tag == c3__core ) {
    return (child == c3__void) ? c3__void : u3nt(c3__core, child, u3k(q));
  }
  u3_noun pro = tail ? u3qf_cell(p, child) : u3qf_cell(child, q);
  u3z(child);
  return pro;
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

  u3k(sut); u3k(foot); u3k(mur);
  u3_weak port = u3qfu_fond(van, sut, c3__rite, wing, kel_y);
  u3_weak pro = u3_none;
  if ( port != u3_none ) {
    if ( port != u3_nul && u3h(port) == c3y ) {
      _toss_path path = {0};
      u3_weak axe = _toss_axis(u3h(u3t(port)), &path);
      if ( axe != u3_none ) {
        u3_weak typ = _toss_take(sut, mur, path.steps, path.len_w, 128);
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
