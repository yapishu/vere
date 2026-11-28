/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

//  This version of the jet has a bug, fixing which makes some versions of the
//  compiler too strict, making them fail to compile correct Hoon code.
//  See ut_nest2.c

typedef struct {
  u3_noun cor, bat;
  c3_m    fun_m;
  c3_t    seg_t, reg_t;
  c3_o    tel_o;
} _nest_walk;

/* +dext pushes its verdict and branches on slot 2. Keep the negative
** branch so a strict failure sees [| dext-core] at the point of failure.
** https://github.com/urbit/urbit/blob/5a187feded/pkg/arvo/sys/hoon.hoon#L10410-L10421
*/
static u3_weak
_nest_failure(u3_noun arm)
{
  u3_noun post, test, yes, no, axis, value;
  if ( c3y == u3r_pq(arm, 8, 0, &post) &&
       c3y == u3r_pqr(post, 6, &test, &yes, &no) &&
       c3y == u3r_p(test, 0, &axis) && axis == 2 &&
       c3y == u3r_p(yes, 1, &value) && value == c3y ) return no;
  return u3_none;
}

static u3_noun
_nest_fail(u3_noun cor)
{
  u3_noun fail = u3k(_nest_failure(u3h(cor)));
  return u3n_nock_on(u3nc(c3n, cor), fail);
}

static c3_m
_nest_fun(u3_weak vet)
{
  return 141 + c3__dext + ((!!vet) << 8);
}

u3_weak
u3qfu_nest_find(u3_noun van, u3_noun sut, u3_noun ref)
{
  u3_weak vet = u3r_at(u3qfu_van_vet, van);
  if ( vet > 1 ) return u3_none;
  u3_noun key = u3z_key_3(_nest_fun(vet), sut, ref, u3h(van));
  u3_weak pro = u3z_find(u3z_memo_toss, key);
  u3z(key);
  if ( pro != u3_none && pro > 1 ) {
    u3z(pro);
    return u3_none;
  }
  return pro;
}

static u3_noun
_nest_save(u3_noun key, u3_noun pro, c3_t seg_t, c3_t reg_t)
{
  if ( ((c3y == pro) && !reg_t) || ((c3n == pro) && !seg_t) ) {
    return u3z_save(u3z_memo_toss, key, pro);
  }
  u3z(key);
  return pro;
}

/* +dext carries the two types through structural rules.
** Only a residual call needs a Nock subject. Each child still reads and
** saves its own memo entry, with seg and reg cleared under %cell.
** https://github.com/urbit/urbit/blob/bd3973dc97c0f4ac8eae5fddfa15c39d522eb8d0/pkg/arvo/sys/hoon.hoon#L10418-L10489
*/
static u3_weak
_nest_struct(const _nest_walk*, u3_noun, u3_noun, c3_t, c3_w);

static u3_noun
_nest_child(const _nest_walk* wal_u, u3_noun sut, u3_noun ref,
            c3_t clear_t, c3_w dep_w)
{
  u3_noun key = u3z_key_3(wal_u->fun_m, sut, ref, wal_u->bat);
  u3_weak pro = u3z_find(u3z_memo_toss, key);
  if ( u3_none != pro ) {
    u3z(key);
    return pro;
  }

  //  Equality can unify key fields. Own the current fields across Nock.
  //
  sut = u3k(u3h(u3t(key)));
  ref = u3k(u3h(u3t(u3t(key))));
  pro = _nest_struct(wal_u, sut, ref, clear_t, dep_w);
  if ( u3_none == pro || (pro == c3n && wal_u->tel_o == c3y) ) {
    u3_noun in = u3t(wal_u->cor), nest = u3t(u3t(in));
    u3_noun van = u3i_edit(u3k(u3t(u3t(nest))), u3x_sam, u3k(sut));
    u3_noun sam = u3h(u3t(in));
    u3_noun set = clear_t ? u3nt(0, 0, u3k(u3t(u3t(sam)))) : u3k(sam);
    u3_noun sub = u3nt(u3k(u3h(nest)), u3nc(wal_u->tel_o, u3k(ref)), van);
    u3_noun arm = u3h(wal_u->cor);
    sub = u3nc(u3k(arm), u3nt(u3k(u3h(in)), set, sub));
    pro = (pro == c3n) ? _nest_fail(sub) : u3n_nock_on(sub, u3k(arm));
  }
  u3z(sut); u3z(ref);
  return _nest_save(key, pro, !clear_t && wal_u->seg_t, !clear_t && wal_u->reg_t);
}

/* A tagged record has a singleton atom type at the head of a cell. */
static u3_weak
_nest_tag(u3_noun typ, u3_noun* hed)
{
  u3_noun atm, aura, unit, tag;
  if ( c3n == u3r_pq(typ, c3__cell, &atm, 0) ||
       c3n == u3r_pq(atm, c3__atom, &aura, &unit) || c3n == u3ud(aura) ||
       c3n == u3r_p(unit, 0, &tag) || c3n == u3a_is_cat(tag) ) return u3_none;
  if ( hed ) *hed = atm;
  return tag;
}

/* Each alternative uses the same [ref bat] suffix. Only the two fresh
** cells need mugs. A miss leaves the complete, owning key for the save.
*/
static u3_weak
_nest_probe(c3_m fun_m, u3_noun sut, u3_noun suffix, u3_noun* key)
{
  *key = u3z_key_2(fun_m, sut, suffix);
  u3a_cell* root = u3a_to_ptr(*key);
  u3a_cell* pair = u3a_to_ptr(root->tel);
  pair->mug_w = u3r_mug_cell(sut, suffix);
  root->mug_w = u3r_mug_both(u3r_mug_word(fun_m), pair->mug_w);
  u3_weak pro = u3z_find(u3z_memo_toss, *key);
  if ( pro != u3_none ) u3z(*key);
  return pro;
}

/* Distinct tags settle a fresh head check. Honor an existing verdict
** for either the record or its head, and save both misses in Hoon order.
** The cleared recursion sets under a cell allow caching a negative head.
*/
static u3_noun
_nest_clash(const _nest_walk* wal_u, u3_noun sut, u3_noun record,
            u3_noun head, c3_t clear_t, c3_w dep_w)
{
  u3_noun key, inner;
  u3_weak pro = _nest_probe(wal_u->fun_m, sut, record, &key);
  if ( pro != u3_none ) return pro;

  // A lookup can unify fields. Read the current subject through its key.
  sut = u3h(u3t(key));
  pro = _nest_probe(wal_u->fun_m, u3h(u3t(sut)), head, &inner);
  if ( pro == u3_none ) pro = u3z_save(u3z_memo_toss, inner, c3n);
  if ( c3y == u3x_loob(pro) ) {
    u3_noun args = u3t(key), ref = u3h(u3t(args));
    pro = _nest_child(wal_u, u3t(u3t(u3h(args))), u3t(u3t(ref)), 1, dep_w-1);
  }
  return _nest_save(key, pro, !clear_t && wal_u->seg_t, !clear_t && wal_u->reg_t);
}

static u3_noun
_nest_fork(const _nest_walk* wal_u, u3_noun sut, u3_noun ref, c3_t some_t,
           c3_t clear_t, c3_w dep_w)
{
  //  +lien and +levy visit alternatives in +tap order and stop at the
  //  first decisive answer. +tap also checks the whole tree before either.
  //
  u3_noun list = u3qdi_tap(u3t(some_t ? sut : ref));
  u3_noun pro = some_t ? c3n : c3y;
  // +lien tries each subject alternative with tel=|.
  _nest_walk child = *wal_u;
  u3_weak tag = u3_none;
  u3_noun record = 0, head = 0;
  if ( some_t ) {
    child.tel_o = c3n;
    u3_noun atm;
    if ( dep_w >= 2 && (tag = _nest_tag(ref, &atm)) != u3_none ) {
      record = u3nc(u3k(ref), u3k(child.bat));
      head = u3nc(u3k(atm), u3k(child.bat));
    }
  }
  while ( list ) {
    u3_noun typ = u3h(list);
    u3_weak alt = (tag == u3_none) ? u3_none : _nest_tag(typ, 0);
    if ( alt != u3_none && alt != tag ) {
      pro = _nest_clash(&child, typ, record, head, clear_t, dep_w);
    }
    else {
      pro = _nest_child(&child, some_t ? typ : sut, some_t ? ref : typ,
                        clear_t, dep_w);
    }
    if ( u3x_loob(pro) == (some_t ? c3y : c3n) ) break;
    u3_noun next = u3k(u3t(list));
    u3z(list);
    list = next;
  }
  u3z(list); u3z(record); u3z(head);
  return pro;
}

static u3_weak
_nest_struct(const _nest_walk* wal_u, u3_noun sut, u3_noun ref,
             c3_t clear_t, c3_w dep_w)
{
  if ( !dep_w ) return u3_none;
  u3_noun s = (c3y == u3du(sut)) ? u3h(sut) : 0;
  u3_noun r = (c3y == u3du(ref)) ? u3h(ref) : 0;
  if ( sut == ref || sut == c3__noun || (s == r && c3y == u3r_sing(sut, ref)) ) {
    return c3y;
  }

  u3_noun a, b, c, d;
  switch ( s ) {
    case c3__face: case c3__hint:
      if ( c3n == u3r_trel(sut, 0, 0, &a) ) return u3_none;
      return _nest_child(wal_u, a, ref, clear_t, dep_w-1);

    case c3__cell:
      if ( r != c3__cell ) break;
      if ( c3n == u3r_trel(sut, 0, &a, 0) || c3n == u3r_trel(ref, 0, &b, 0) ) {
        return u3_none;
      }
      if ( c3n == u3x_loob(_nest_child(wal_u, a, b, 1, dep_w-1)) ) return c3n;
      return _nest_child(wal_u, u3t(u3t(sut)), u3t(u3t(ref)), 1, dep_w-1);

    case c3__atom:
      if ( r != c3__atom ) break;
      if ( c3n == u3r_trel(sut, 0, &a, &b) || c3n == u3r_trel(ref, 0, &c, &d) ||
           c3n == u3ud(a) || c3n == u3ud(c) ) return u3_none;
      return c3a(u3qf_fitz(a, c), __(0 == b || c3y == u3r_sing(b, d)));

    case c3__fork:
      if ( ref == c3__noun || r == c3__atom || r == c3__cell || r == c3__core ) {
        return _nest_fork(wal_u, sut, ref, 1, clear_t, dep_w-1);
      }
      break;

    case c3__core: break;
    default: if ( sut != c3__void ) return u3_none;
  }

  if ( ref == c3__void ) return c3y;
  if ( ref == c3__noun || r == c3__atom || r == c3__cell ) return c3n;
  if ( r == c3__fork ) return _nest_fork(wal_u, sut, ref, 0, clear_t, dep_w-1);
  if ( (r == c3__hint || r == c3__face) && c3y == u3r_trel(ref, 0, 0, &a) ) {
    return _nest_child(wal_u, sut, a, clear_t, dep_w-1);
  }
  return u3_none;
}

u3_noun
u3wfu_nest_dext1(u3_noun dext_core)
{
  u3_noun nest_in_core, nest_core;
  u3_noun bat, sut, ref, van, seg, reg, gil;

  if (  (u3_none == (nest_in_core = u3r_at(3, dext_core)))
     || (c3n == u3r_mean(nest_in_core, {u3x_sam_2, &seg},
                                       {u3x_sam_6, &reg},
                                       {u3x_sam_7, &gil},
                                       {u3x_con, &nest_core}))
     || (c3n == u3r_mean(nest_core, {u3x_sam_3, &ref},
                                    {u3x_con, &van}))
     || (u3_none == (bat = u3r_at(u3x_bat, van)))
     || (u3_none == (sut = u3r_at(u3x_sam, van))) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    u3_weak vet = u3r_at(u3qfu_van_vet, van);
    c3_m  fun_m = _nest_fun(vet);
    u3_noun key = u3z_key_3(fun_m, sut, ref, bat);
    u3_weak pro = u3z_find(u3z_memo_toss, key);

    if ( u3_none != pro ) {
      u3z(key);
      return pro;
    }
    else {
      c3_t seg_t = (u3_nul != seg), reg_t = (u3_nul != reg);
      u3_weak tel = u3r_at(u3x_sam_2, nest_core);
      if ( !(u3C.wag_h & (u3o_trace | u3o_debug_cpu)) &&
           (tel == c3n || (tel == c3y && _nest_failure(u3h(dext_core)) != u3_none)) ) {
        u3_noun args = u3t(key), rest = u3t(args);
        _nest_walk wal_u = { dext_core, u3k(u3t(rest)), fun_m, seg_t, reg_t, tel };
        sut = u3k(u3h(args)); ref = u3k(u3h(rest));
        // Bound native recursion. Deeper branches continue in Nock.
        pro = _nest_struct(&wal_u, sut, ref, 0, 24);
        u3z(sut); u3z(ref); u3z(wal_u.bat);
        if ( pro == c3n && tel == c3y ) pro = _nest_fail(u3k(dext_core));
      }
      if ( u3_none == pro ) {
        pro = u3n_nock_on(u3k(dext_core), u3k(u3x_at(u3x_bat, dext_core)));
      }
      return _nest_save(key, pro, seg_t, reg_t);
    }
  }
}
