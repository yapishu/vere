/// @file

#ifndef U3_JETS_B_LIST_WALK_H
#define U3_JETS_B_LIST_WALK_H

#include "noun.h"

typedef enum {
  _walk_turn, _walk_skim, _walk_skip, _walk_murn
} _walk_kind;

/* The caller owns the input and the prepared gate cannot rewrite its
 * interior pointers. Copy a run only when a later edit needs its prefix. */
static inline u3_noun
_walk_list(u3_noun list, u3j_site* site, _walk_kind kind)
{
  u3_noun result, *end = &result, run = list;
  for ( u3_noun node = list; node != u3_nul; node = u3t(node) ) {
    u3_noun head = u3h(node);
    u3_noun value = u3j_gate_slam(site, u3k(head));
    if ( kind == _walk_skim || kind == _walk_skip ) {
      if ( u3x_loob(value) == (kind == _walk_skim ? c3y : c3n) ) continue;
      value = u3_none;
    }
    else {
      if ( kind == _walk_murn ) {
        u3_noun unit = value;
        value = (unit == u3_nul) ? u3_none : u3k(u3t(unit));
        u3z(unit);
      }
      if ( value == head ) {
        u3z(value);
        continue;
      }
    }
    if ( run != node ) end = u3i_list_prefix(end, run, node);
    if ( value != u3_none ) {
      u3_noun *hed, *tel;
      *end = u3i_defcons(&hed, &tel);
      *hed = value;
      end = tel;
    }
    run = u3t(node);
  }
  *end = u3k(run);
  return result;
}

#endif
