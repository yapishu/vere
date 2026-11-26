/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/* +redo carries subject and reference faces down to the next cell or leaf.
** +dear joins the two stacks at their longest overlap. A fork can produce
** several reference stacks, and a hold can consult the compiler environment.
** These cases run the gate with its original subject.
** https://github.com/urbit/urbit/blob/bd3973dc97c0f4ac8eae5fddfa15c39d522eb8d0/pkg/arvo/sys/hoon.hoon#L10602-L10809
*/
static c3_t
_redo_leaf(u3_noun typ)
{
  return typ == c3__noun || typ == c3__void ||
         (c3y == u3du(typ) && (u3h(typ) == c3__atom || u3h(typ) == c3__core));
}

//  The walk borrows fields from both inputs. Defer equal-mug cells to
//  Hoon, since unifying their children could invalidate a borrowed field.
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
_redo_walk(u3_noun sut, u3_noun ref, c3_w dep_w, c3_w* rem_w)
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
    u3_weak h = _redo_walk(hed, lef, dep_w - 1, rem_w);
    if ( h == u3_none ) return u3_none;
    u3_weak t = _redo_walk(tel, rit, dep_w - 1, rem_w);
    if ( t == u3_none ) {
      u3z(h);
      return u3_none;
    }
    pro = u3nt(c3__cell, h, t);
  }
  return _redo_faces(_redo_faces(pro, hos, hos_w), har, har_w - lip_w);
}

u3_noun
u3wfu_redo(u3_noun cor)
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
            ? u3k(sut) : _redo_walk(sut, ref, 32, &rem_w);
      }
      if ( u3_none == pro ) {
        pro = u3n_nock_on(u3k(cor), u3k(u3h(cor)));
      }
      return u3z_save(u3z_memo_toss, key, pro);
    }
  }
}
