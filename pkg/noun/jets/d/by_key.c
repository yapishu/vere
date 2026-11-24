/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

//  [a] is RETAINED, [set] is TRANSFERRED
//
static u3_noun
_by_key(u3_noun a, u3_noun set)
{
  if ( u3_nul == a ) {
    return set;
  }
  else {
    u3_noun n_a, l_a, r_a;
    u3x_trel(a, &n_a, &l_a, &r_a);

    {
      u3_noun new = u3qdi_put(set, u3h(n_a));
      u3z(set);
      set = new;
    }

    set = _by_key(l_a, set);

    return _by_key(r_a, set);
  }
}

// A map satisfying the same gor/mor invariants as a set already has
// its canonical key-set topology. Project it in postorder.
static u3_noun
_by_key_project(u3_noun map)
{
  typedef struct {
    u3_noun key, right, left;
  } frame;
  u3a_pile stack;
  u3a_pile_prep(&stack, sizeof(frame), alignof(frame));
  u3_noun set = u3_nul;
  for (;;) {
    while ( map ) {
      u3_noun key = u3h(u3h(map)), branches = u3t(map);
      u3_noun left = u3h(branches), right = u3t(branches);
      if ( !left && !right ) {
        set = u3nc(u3k(key), u3k(branches));
        break;
      }
      *(frame*)u3a_push(&stack) = (frame){ key, right, u3_none };
      map = left;
    }
    for (;;) {
      if ( c3y == u3a_pile_done(&stack) ) return set;
      frame* top = u3a_peek(&stack);
      if ( u3_none == top->left ) {
        top->left = set;
        map = top->right;
        set = u3_nul;
        break;
      }
      set = u3nt(u3k(top->key), top->left, set);
      u3a_drop(&stack);
    }
  }
}

u3_noun
u3qdb_key(u3_noun a)
{
  if ( c3y == u3qdb_apt(a) ) return _by_key_project(a);
  return _by_key(a, u3_nul);
}

u3_noun
u3wdb_key(u3_noun cor)
{
  return u3qdb_key(u3x_at(u3x_con_sam, cor));
}
