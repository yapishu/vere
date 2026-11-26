/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"

/* +redo carries field names down a pair of types. On a recursive type the
** hypothesis is the pair, scoped to that descent. Restoring the original
** hold requires comparing the entire answer with its expansion.
** The paired traversal follows the product construction for recursive types.
** https://www.cs.cornell.edu/~kozen/Papers/sub.pdf
** https://github.com/urbit/urbit/blob/bd3973dc97c0f4ac8eae5fddfa15c39d522eb8d0/pkg/arvo/sys/hoon.hoon#L10602-L10816
*/
typedef struct _redo_names {
  u3_noun tool;
  c3_w len;
  const struct _redo_names* outer;
} _redo_names;

typedef struct _redo_pair {
  u3_noun sut, ref;
  const struct _redo_pair* parent;
} _redo_pair;

typedef struct {
  u3_noun van;
  c3_w fuel;
  c3_y kel;
} _redo_walk;

static c3_t
_redo_leaf(u3_noun typ)
{
  return typ == c3__noun || typ == c3__void ||
         (c3y == u3du(typ) && (u3h(typ) == c3__atom || u3h(typ) == c3__core));
}

//  The walk borrows fields from both inputs. Defer equal-mug cells,
//  since unifying their children could invalidate a borrowed field.
//
static u3_weak
_redo_same(u3_noun a, u3_noun b)
{
  if ( a == b ) return c3y;
  if ( c3y == u3ud(a) || c3y == u3ud(b) ) return u3r_sing(a, b);
  return u3r_mug(a) != u3r_mug(b) ? c3n : u3_none;
}

static u3_noun
_redo_faces(u3_noun typ, const u3_noun* face, c3_w len_w)
{
  if ( typ != c3__void ) {
    while ( len_w ) typ = u3nt(c3__face, u3k(face[--len_w]), typ);
  }
  return typ;
}

static u3_weak
_redo_plain(u3_noun sut, u3_noun ref, c3_w dep_w, c3_w* rem_w)
{
  u3_noun hos[16], har[16], hed, tel;
  c3_w hos_w = 0, har_w = 0;

  if ( !dep_w || !*rem_w ) return u3_none;
  --*rem_w;

  for (;;) {
    if ( _redo_leaf(ref) ) return _redo_faces(u3k(sut), hos, hos_w);
    u3_weak eql = _redo_same(sut, ref);
    if ( eql == c3y ) return _redo_faces(u3k(sut), hos, hos_w);
    if ( eql == u3_none ) return u3_none;

    if ( c3n == u3du(sut) || u3h(sut) != c3__face ) break;
    if ( hos_w == c3_array_len(hos) ||
         c3n == u3r_cell(u3t(sut), &hos[hos_w], &sut) ) return u3_none;
    ++hos_w;
  }

  c3_t leaf_t = _redo_leaf(sut);
  if ( !leaf_t && c3n == u3r_pq(sut, c3__cell, &hed, &tel) ) return u3_none;

  while ( c3y == u3du(ref) ) {
    u3_noun tag = u3h(ref), tool;
    if ( tag != c3__face && tag != c3__hint ) break;
    if ( !*rem_w || c3n == u3r_cell(u3t(ref), &tool, &ref) ) return u3_none;
    --*rem_w;
    if ( tag == c3__face ) {
      if ( har_w == c3_array_len(har) ) return u3_none;
      har[har_w++] = tool;
    }
  }

  //  Both arrays run outermost first. Drop the longest suffix of har
  //  that matches the prefix of hos, then wrap the subject in both.
  //
  c3_w lip_w = c3_min(har_w, hos_w);
  for ( ; lip_w; --lip_w ) {
    c3_w i_w;
    for ( i_w = 0; i_w < lip_w; ++i_w ) {
      u3_weak eql = _redo_same(hos[i_w], har[har_w - lip_w + i_w]);
      if ( eql == u3_none ) return u3_none;
      if ( eql == c3n ) break;
    }
    if ( i_w == lip_w ) break;
  }

  u3_noun pro;
  if ( leaf_t ) {
    if ( !_redo_leaf(ref) && (c3n == u3du(ref) || u3h(ref) != c3__cell) ) {
      return u3_none;
    }
    pro = u3k(sut);
  }
  else {
    //  On a cell, +peek at axes 2 and 3 returns its two fields.
    //
    u3_noun lef, rit;
    if ( c3n == u3r_pq(ref, c3__cell, &lef, &rit) ) return u3_none;
    u3_weak h = _redo_plain(hed, lef, dep_w - 1, rem_w);
    if ( h == u3_none ) return u3_none;
    u3_weak t = _redo_plain(tel, rit, dep_w - 1, rem_w);
    if ( t == u3_none ) {
      u3z(h);
      return u3_none;
    }
    pro = u3nt(c3__cell, h, t);
  }
  return _redo_faces(_redo_faces(pro, hos, hos_w), har, har_w - lip_w);
}

static c3_t
_redo_seen(const _redo_pair* at, u3_noun sut, u3_noun ref)
{
  for ( ; at; at = at->parent ) {
    if ( c3y == u3r_sing(at->sut, sut) && c3y == u3r_sing(at->ref, ref) ) {
      return 1;
    }
  }
  return 0;
}

//  +dear joins the suffix of the subject stack to the prefix of the
//  reference stack at their longest overlap. Both start innermost first.
//
static u3_noun
_redo_done(u3_noun sut, const _redo_names* hos, const _redo_names* har)
{
  if ( sut == c3__void ) return sut;
  if ( hos && har ) {
    c3_w len = c3_min(hos->len, har->len);
    const _redo_names* start = hos;
    while ( start->len > len ) start = start->outer;
    for ( ; start; start = start->outer ) {
      const _redo_names* a = start;
      const _redo_names* b = har;
      while ( a && c3y == u3r_sing(a->tool, b->tool) ) {
        a = a->outer;
        b = b->outer;
      }
      if ( !a ) { har = b; break; }
    }
  }
  u3_noun pro = u3k(sut);
  for ( ; hos; hos = hos->outer ) pro = u3nt(c3__face, u3k(hos->tool), pro);
  for ( ; har; har = har->outer ) pro = u3nt(c3__face, u3k(har->tool), pro);
  return pro;
}

//  Bounded +tap. The list owns each alternative across unifying lookups.
//
static u3_weak
_redo_tap(_redo_walk* walk, u3_noun set, u3_noun list, c3_w depth)
{
  if ( set == u3_nul ) return list;
  u3_noun typ, lef, rit;
  if ( !depth || !walk->fuel || c3n == u3r_trel(set, &typ, &lef, &rit) ) {
    u3z(list);
    return u3_none;
  }
  --walk->fuel;
  list = _redo_tap(walk, lef, list, depth-1);
  if ( list == u3_none ) return list;
  return _redo_tap(walk, rit, u3nc(u3k(typ), list), depth-1);
}

//  A malformed fan or an active leg leaves the original Hoon call intact.
//
static c3_t
_redo_can_open(_redo_walk* walk, u3_noun sut)
{
  u3_noun leg = u3t(sut);
  u3_weak fan = u3r_at(u3qfu_van_fan, walk->van);
  if ( fan == u3_none || c3n == u3du(leg) ) return 0;
  while ( fan ) {
    u3_noun key, left, right;
    if ( !walk->fuel || c3n == u3r_trel(fan, &key, &left, &right) ||
         _redo_same(key, leg) != c3n ) return 0;
    --walk->fuel;
    fan = (c3y == u3qc_gor(leg, key)) ? left : right;
  }
  return 1;
}

/* +miss on equal types, or on %noun, calls +nest with %void. A completed
** verdict on this road suffices for a non-primitive type. A missing verdict
** sends the whole +redo to Hoon with its original fan and repetition sets.
*/
static u3_weak
_redo_empty(_redo_walk* walk, u3_noun typ)
{
  if ( typ == c3__void ) return c3y;
  if ( typ == c3__noun ) return c3n;
  if ( c3y == u3du(typ) ) {
    u3_noun tag = u3h(typ);
    if ( tag == c3__cell || tag == c3__atom ||
         (tag == c3__core && c3y == u3du(u3t(typ))) ) return c3n;
  }
  return u3qfu_nest_find(walk->van, c3__void, typ);
}

static u3_weak _redo_miss(_redo_walk*, u3_noun, u3_noun, const _redo_pair*, c3_w);

//  +miss assumes an unordered pair disjoint on revisiting it through a hold.
//  Its cell rule needs a disjoint head or tail. Its fork rule needs every arm.
//
static u3_weak
_redo_disjoint(_redo_walk* walk, u3_noun sut, u3_noun ref,
               const _redo_pair* seen, c3_w depth)
{
  if ( c3y == u3r_sing(sut, ref) ) return _redo_empty(walk, sut);
  if ( sut == c3__void ) return c3y;
  if ( sut == c3__noun ) return _redo_empty(walk, ref);
  if ( c3n == u3du(sut) ) return u3_none;

  u3_noun p, q, r;
  switch ( u3h(sut) ) {
    case c3__face: case c3__hint:
      if ( c3n == u3r_cell(u3t(sut), 0, &q) ) return u3_none;
      return _redo_miss(walk, q, ref, seen, depth);
    case c3__hold: {
      if ( _redo_seen(seen, sut, ref) || _redo_seen(seen, ref, sut) ) return c3y;
      if ( !_redo_can_open(walk, sut) ) return u3_none;
      u3_weak typ = u3qfu_rest_find(walk->van, sut, walk->kel);
      if ( typ == u3_none ) return typ;
      _redo_pair here = {sut, ref, seen};
      u3_weak pro = _redo_miss(walk, typ, ref, &here, depth);
      u3z(typ);
      return pro;
    }
    case c3__fork: {
      u3_weak list = _redo_tap(walk, u3t(sut), u3_nul, depth);
      if ( list == u3_none ) return list;
      u3_weak pro = c3y;
      for ( u3_noun i = list; i && pro == c3y; i = u3t(i) ) {
        pro = _redo_miss(walk, u3h(i), ref, seen, depth);
      }
      u3z(list);
      return pro;
    }
    case c3__core: {
      u3_noun cell = u3nt(c3__cell, c3__noun, c3__noun);
      u3_weak pro = _redo_miss(walk, cell, ref, seen, depth);
      u3z(cell);
      return pro;
    }
    case c3__cell: case c3__atom: break;
    default: return u3_none;
  }

  u3_noun tag = (c3y == u3du(ref)) ? u3h(ref) : ref;
  if ( tag != c3__atom && tag != c3__cell ) {
    return _redo_miss(walk, ref, sut, seen, depth);
  }
  if ( c3n == u3du(ref) ) return u3_none;
  if ( tag != u3h(sut) ) return c3y;
  if ( c3n == u3r_cell(u3t(sut), &p, &q) || c3n == u3r_pq(ref, tag, &r, 0) ) {
    return u3_none;
  }
  if ( tag == c3__atom ) {
    u3_noun val = u3t(u3t(ref));
    return __(c3y == u3du(q) && c3y == u3du(val) && c3n == u3r_sing(q, val));
  }
  u3_weak hed = _redo_miss(walk, p, r, seen, depth);
  return hed != c3n ? hed : _redo_miss(walk, u3t(u3t(sut)), u3t(u3t(ref)), seen, depth);
}

static u3_weak
_redo_miss(_redo_walk* walk, u3_noun sut, u3_noun ref,
           const _redo_pair* seen, c3_w depth)
{
  if ( !depth || !walk->fuel ) return u3_none;
  --walk->fuel;
  u3k(sut); u3k(ref);
  u3_weak pro = _redo_disjoint(walk, sut, ref, seen, depth-1);
  u3z(sut); u3z(ref);
  return pro;
}

static u3_weak _redo_step(_redo_walk*, u3_noun, u3_noun, const _redo_names*,
                          const _redo_names*, const _redo_pair*, c3_w);

//  +sint selects the reference alternatives that intersect the subject. With
//  exactly one survivor its tool stack and normalized type are the answer.
//  All other cases use Hoon's set of tool stacks and its diagnostics.
//
static u3_weak
_redo_select(_redo_walk* walk, u3_noun sut, u3_noun set, c3_w depth)
{
  u3_weak list = _redo_tap(walk, set, u3_nul, depth), only = u3_none;
  if ( list == u3_none ) return list;
  for ( u3_noun i = list; i; i = u3t(i) ) {
    u3_weak miss = _redo_miss(walk, sut, u3h(i), 0, depth);
    if ( miss == u3_none || (miss == c3n && only != u3_none) ) {
      if ( only != u3_none ) u3z(only);
      only = u3_none;
      break;
    }
    if ( miss == c3n ) only = u3k(u3h(i));
  }
  u3z(list);
  return only;
}

static u3_weak
_redo_sint(_redo_walk* walk, u3_noun sut, u3_noun ref, const _redo_names* hos,
           const _redo_names* har, const _redo_pair* seen, c3_w depth)
{
  if ( !depth || !walk->fuel ) return u3_none;
  --walk->fuel;
  c3_t hold = c3y == u3du(sut) && u3h(sut) == c3__hold;
  u3_noun tag = (c3y == u3du(ref)) ? u3h(ref) : ref;
  u3_noun p, q;
  if ( tag == c3__face || tag == c3__hint ) {
    if ( c3n == u3r_pq(ref, tag, &p, &q) ) return u3_none;
    _redo_names name = {u3k(p), har ? har->len+1 : 1, har};
    u3k(q);
    u3_weak pro = _redo_sint(walk, sut, q, hos, tag == c3__face ? &name : har,
                             seen, depth-1);
    u3z(q); u3z(name.tool);
    return pro;
  }
  if ( tag == c3__fork || (tag == c3__hold && !hold) ) {
    if ( c3n == u3du(ref) || (tag == c3__hold && !_redo_can_open(walk, ref)) ) {
      return u3_none;
    }
    u3_weak typ = (tag == c3__fork) ? _redo_select(walk, sut, u3t(ref), depth-1)
                                    : u3qfu_rest_find(walk->van, ref, walk->kel);
    if ( typ == u3_none ) return typ;
    u3_weak pro = _redo_sint(walk, sut, typ, hos, har, seen, depth-1);
    u3z(typ);
    return pro;
  }

  if ( hold ) {
    if ( !_redo_can_open(walk, sut) ) return u3_none;
    if ( _redo_seen(seen, sut, ref) ) return _redo_done(sut, hos, har);
    u3_weak typ = u3qfu_rest_find(walk->van, sut, walk->kel);
    if ( typ == u3_none ) return typ;
    _redo_pair here = {sut, ref, seen};
    u3_weak pro = _redo_step(walk, typ, ref, hos, har, &here, depth-1);
    if ( pro != u3_none && c3y == u3r_sing(typ, pro) ) {
      u3z(pro);
      pro = u3k(sut);
    }
    u3z(typ);
    return pro;
  }
  if ( _redo_leaf(sut) ) {
    return (_redo_leaf(ref) || tag == c3__cell) ? _redo_done(sut, hos, har) : u3_none;
  }

  //  +peek %free at axes 2 and 3 of the normalized reference.
  //
  u3_noun lef, rit;
  if ( c3n == u3r_pq(sut, c3__cell, &p, &q) ) return u3_none;
  if ( c3n == u3r_pq(ref, c3__cell, &lef, &rit) ) {
    if ( ref == c3__noun || ref == c3__void ) lef = rit = ref;
    else if ( c3y == u3du(ref) && tag == c3__atom ) lef = rit = c3__void;
    else if ( c3y == u3r_pq(ref, c3__core, &rit, 0) && u3r_at(59, ref) != u3_none ) {
      lef = c3__noun;
    }
    else return u3_none;
  }
  u3k(q); u3k(rit);
  u3_weak h = _redo_step(walk, p, lef, 0, 0, seen, depth-1);
  u3_weak pro = u3_none;
  if ( h != u3_none ) {
    u3_weak t = _redo_step(walk, q, rit, 0, 0, seen, depth-1);
    if ( t != u3_none ) {
      u3_noun cell = u3nt(c3__cell, h, t);
      pro = _redo_done(cell, hos, har);
      u3z(cell);
    }
    else u3z(h);
  }
  u3z(q); u3z(rit);
  return pro;
}

static u3_weak
_redo_dext(_redo_walk* walk, u3_noun sut, u3_noun ref, const _redo_names* hos,
           const _redo_names* har, const _redo_pair* seen, c3_w depth)
{
  if ( _redo_leaf(ref) || c3y == u3r_sing(sut, ref) ) return _redo_done(sut, hos, har);
  u3_noun tag, p, q;
  if ( c3y == u3r_trel(sut, &tag, &p, &q) && (tag == c3__face || tag == c3__hint) ) {
    _redo_names name = {u3k(p), hos ? hos->len+1 : 1, hos};
    u3_weak pro = _redo_step(walk, q, ref, tag == c3__face ? &name : hos, har, seen, depth);
    if ( pro != u3_none && tag == c3__hint ) {
      u3_noun hint = u3qf_hint(name.tool, pro);
      u3z(pro);
      pro = hint;
    }
    u3z(name.tool);
    return pro;
  }
  if ( c3y == u3r_p(sut, c3__fork, &p) ) {
    u3_weak list = _redo_tap(walk, p, u3_nul, depth);
    if ( list == u3_none ) return list;
    u3_noun out = u3_nul;
    for ( u3_noun i = list; i; i = u3t(i) ) {
      u3_weak pro = _redo_step(walk, u3h(i), ref, hos, har, seen, depth);
      if ( pro == u3_none ) { u3z(list); u3z(out); return pro; }
      out = u3nc(pro, out);
    }
    u3z(list);
    return u3kf_fork(out);
  }
  return _redo_sint(walk, sut, ref, hos, har, seen, depth);
}

static u3_weak
_redo_step(_redo_walk* walk, u3_noun sut, u3_noun ref, const _redo_names* hos,
           const _redo_names* har, const _redo_pair* seen, c3_w depth)
{
  if ( !depth || !walk->fuel ) return u3_none;
  --walk->fuel;
  //  Memo lookups and equality can unify descendants of either input.
  u3k(sut); u3k(ref);
  u3_weak pro = _redo_dext(walk, sut, ref, hos, har, seen, depth-1);
  u3z(sut); u3z(ref);
  return pro;
}

static u3_noun
_redo(u3_noun cor, c3_y kel)
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
    c3_m  fun_m = 141 + c3__redo + ((!!vet) << 8);
    u3_noun key = u3z_key_3(fun_m, sut, ref, bat);
    u3_weak pro = u3z_find(u3z_memo_toss, key);

    if ( u3_none != pro ) {
      u3z(key);
      return pro;
    }
    else {
      if ( !(u3C.wag_h & (u3o_trace | u3o_debug_cpu)) ) {
        //  The memo lookup can unify fields of the key.
        //
        sut = u3h(u3t(key));
        ref = u3h(u3t(u3t(key)));
        c3_w rem_w = 256;
        pro = (_redo_leaf(ref) || c3y == u3r_sing(sut, ref))
            ? u3k(sut) : _redo_plain(sut, ref, 32, &rem_w);
        if ( pro == u3_none ) {
          _redo_walk walk = {u3x_at(u3x_con, cor), 1024, kel};
          pro = _redo_step(&walk, sut, ref, 0, 0, 0, 80);
        }
      }
      if ( u3_none == pro ) {
        pro = u3n_nock_on(u3k(cor), u3k(u3h(cor)));
      }
      return u3z_save(u3z_memo_toss, key, pro);
    }
  }
}

u3_noun
u3wfu_redo(u3_noun cor)
{
  return _redo(cor, 137);
}

u3_noun
u3wfu_redo_135(u3_noun cor)
{
  return _redo(cor, 135);
}
