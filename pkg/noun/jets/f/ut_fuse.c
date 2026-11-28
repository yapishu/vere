/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/* +fuse carries the [hold ref] pairs it visits in the trap sample, bix.
** On a residual branch, restore [battery bix fuse-core] and run that trap.
** https://github.com/urbit/urbit/blob/5a187feded/pkg/arvo/sys/hoon.hoon#L9621-L9658
*/
typedef struct {
  u3_noun cor, arm;
} _fuse_walk;

static u3_weak
_fuse_body(u3_noun bat)
{
  u3_noun seed, next, zero, fun, kick, arm, axe, sam;
  if ( c3y == u3r_pq(bat, 8, &seed, &next) &&
       c3y == u3r_p(seed, 1, &zero) && zero == 0 &&
       c3y == u3r_pq(next, 8, &fun, &kick) &&
       c3y == u3r_p(fun, 1, &arm) &&
       c3y == u3r_pq(kick, 9, &axe, &sam) && axe == 2 &&
       c3y == u3r_p(sam, 0, &axe) && axe == 1 ) return arm;
  return u3_none;
}

static u3_noun
_fuse_resume(const _fuse_walk* w, u3_noun sut, u3_noun ref, u3_noun bix)
{
  u3_noun cor = u3i_molt(u3k(w->cor), {6, u3k(ref)}, {30, u3k(sut)});
  return u3n_nock_on(u3nt(u3k(w->arm), u3k(bix), cor), u3k(w->arm));
}

static u3_noun
_fuse_step(const _fuse_walk*, u3_noun, u3_noun, u3_noun, c3_w);

static u3_noun
_fuse_fork(const _fuse_walk* w, u3_noun tree, u3_noun ref, u3_noun bix, c3_w dep)
{
  u3_noun list = u3qdi_tap(tree), out = 0, *end = &out;
  for ( u3_noun i = list; i; i = u3t(i) ) {
    u3_noun pro = _fuse_step(w, u3h(i), ref, bix, dep);
    if ( pro != c3__void ) {
      u3_noun *head, *tail;
      *end = u3i_defcons(&head, &tail);
      *head = pro;
      end = tail;
    }
  }
  *end = 0;
  u3z(list);
  if ( !out ) return c3__void;
  u3_noun item = u3h(out);
  u3_noun pro = (!u3t(out) && (c3n == u3du(item) || u3h(item) != c3__fork))
              ? u3k(item) : u3qf_fork(out);
  u3z(out);
  return pro;
}

static u3_noun
_fuse_type(const _fuse_walk* w, u3_noun sut, u3_noun ref, u3_noun bix, c3_w dep)
{
  if ( !dep ) return _fuse_resume(w, sut, ref, bix);
  if ( ref == c3__noun || c3y == u3r_sing(sut, ref) ) return u3k(sut);
  if ( sut == c3__noun ) return u3k(ref);
  if ( sut == c3__void ) return c3__void;

  u3_noun p, q, a, b, pro;
  u3_noun tag = c3y == u3du(sut) ? u3h(sut) : 0;
  u3_noun rit = c3y == u3du(ref) ? u3h(ref) : 0;
  switch ( tag ) {
    case c3__atom:
      if ( rit == c3__cell ) return c3__void;
      if ( rit != c3__atom ) return _fuse_step(w, ref, sut, bix, dep-1);
      if ( c3n == u3r_trel(sut, 0, &p, &q) || c3n == u3r_trel(ref, 0, &a, &b) ||
           c3n == u3ud(p) || c3n == u3ud(a) ) break;
      if ( c3y == u3du(q) && c3y == u3du(b) && c3n == u3r_sing(q, b) ) {
        return c3__void;
      }
      return u3nt(c3__atom, u3k(c3y == u3qf_fitz(a, p) ? p : a),
                            u3k(c3y == u3du(q) ? q : b));

    // +cell receives both intersections, including the tail after a void head.
    case c3__cell: {
      if ( rit != c3__cell ) return _fuse_step(w, ref, sut, bix, dep-1);
      if ( c3n == u3r_trel(sut, 0, &p, 0) || c3n == u3r_trel(ref, 0, &a, 0) ) break;
      u3_noun hed = _fuse_step(w, p, a, bix, dep-1);
      u3_noun tel = _fuse_step(w, u3t(u3t(sut)), u3t(u3t(ref)), bix, dep-1);
      pro = u3qf_cell(hed, tel);
      u3z(hed); u3z(tel);
      return pro;
    }
    case c3__face: case c3__hint:
      if ( c3n == u3r_trel(sut, 0, 0, &q) ) break;
      q = _fuse_step(w, q, ref, bix, dep-1);
      p = u3h(u3t(sut));
      pro = tag == c3__face ? u3qf_face(p, q) : u3qf_hint(p, q);
      u3z(q);
      return pro;

    case c3__fork:
      return _fuse_fork(w, u3t(sut), ref, bix, dep-1);

    case c3__core: case c3__hold: {
      u3_noun set;
      if ( tag == c3__hold ) {
        u3_noun pair = u3nc(u3k(sut), u3k(ref));
        if ( c3y == u3qdi_has(bix, pair) ) { u3z(pair); break; }
        set = u3qdi_put(bix, pair);
        u3z(pair);
      }
      else {
        set = u3k(bix);
      }
      u3_noun van = u3i_edit(u3k(u3t(u3t(w->cor))), u3x_sam, u3k(sut));
      q = u3j_kink(van, u3qfu_van_repo_135);
      pro = _fuse_step(w, q, ref, set, dep-1);
      u3z(q); u3z(set);
      return pro;
    }
  }
  return _fuse_resume(w, sut, ref, bix);
}

/* A nested Nock call can unify fields of its caller. Own the operands
** across both halves of a cell and across visits to union alternatives.
*/
static u3_noun
_fuse_step(const _fuse_walk* w, u3_noun sut, u3_noun ref, u3_noun bix, c3_w dep)
{
  u3k(sut); u3k(ref);
  u3_noun pro = _fuse_type(w, sut, ref, bix, dep);
  u3z(sut); u3z(ref);
  return pro;
}

static u3_noun
_fuse(u3_noun cor, c3_t structural)
{
  u3_noun bat, sut, ref, van;

  if (  (c3n == u3r_mean(cor, {u3x_sam, &ref}, {u3x_con, &van}))
     || (u3_none == (bat = u3r_at(u3x_bat, van)))
     || (u3_none == (sut = u3r_at(u3x_sam, van))) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    u3_weak vet = u3r_at(u3qfu_van_vet, van);
    c3_m  fun_m = 141 + c3__fuse + ((!!vet) << 8);
    u3_noun key = u3z_key_3(fun_m, sut, ref, bat);
    u3_weak pro = u3z_find(u3z_memo_toss, key);

    if ( u3_none != pro ) {
      u3z(key);
      return pro;
    }
    else {
      u3_weak arm = structural && vet <= 1 ? _fuse_body(u3h(cor)) : u3_none;
      if ( arm != u3_none && !(u3C.wag_h & (u3o_trace | u3o_debug_cpu)) ) {
        _fuse_walk walk = { cor, u3k(arm) };
        u3_noun args = u3t(key);
        // Bound C recursion. Deeper calls resume on the Nock stack.
        pro = _fuse_step(&walk, u3h(args), u3h(u3t(args)), 0, 96);
        u3z(walk.arm);
      }
      else {
        pro = u3n_nock_on(u3k(cor), u3k(u3h(cor)));
      }
      return u3z_save(u3z_memo_toss, key, pro);
    }
  }
}

u3_noun
u3wfu_fuse_135(u3_noun cor)
{
  return _fuse(cor, 1);
}

u3_noun
u3wfu_fuse(u3_noun cor)
{
  return _fuse(cor, 0);
}
