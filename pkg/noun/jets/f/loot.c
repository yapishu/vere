/// @file

#include "jets/136/w.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/* +look and +loot place a node's value at axis 1 in a leaf and axis 2
** otherwise. A sole child is at 3 and two children are at 6 and 7.
** Carry these bits through both trees and build the axis on a match.
** https://github.com/urbit/urbit/blob/5a187feded/pkg/arvo/sys/hoon.hoon#L7459-L7529
*/
typedef struct _arm_axis {
  const struct _arm_axis* pre_u;
  c3_w axe_w;
} _arm_axis;

typedef struct {
  _arm_axis axis;
  u3_noun dom;
} _arm_frame;

//  Each chunk has its own leading 1. Leave room for two bits and keep
//  the chunk a direct atom on both loom widths.
//
static inline void
_arm_room(_arm_axis* pos_u, u3a_pile* pil_u)
{
  if ( pos_u->axe_w >= (1U << 28) ) {
    _arm_frame* top = u3a_push(pil_u);
    *top = (_arm_frame){*pos_u, u3_nul};
    *pos_u = (_arm_axis){&top->axis, 1};
  }
}

static u3_atom
_arm_axis_atom(_arm_axis pos_u)
{
  if ( !pos_u.pre_u ) return pos_u.axe_w;

  c3_d len_d = 1;
  for ( const _arm_axis* at = &pos_u; at; at = at->pre_u ) {
    len_d += c3_bits_word(at->axe_w) - 1;
  }
  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 0, len_d);
  c3_w off_w = 0;
  for ( const _arm_axis* at = &pos_u; at; at = at->pre_u ) {
    c3_w len_w = c3_bits_word(at->axe_w) - (0 != at->pre_u);
    u3r_chop(0, 0, len_w, off_w, sab_u.buf_w, at->axe_w);
    off_w += len_w;
  }
  return u3i_slab_moot(&sab_u);
}

static u3_noun
_look_at(u3_noun cog, u3_noun dab, c3_h* mug_h, _arm_axis pos_u, u3a_pile* pil_u)
{
  u3_post cap_p = u3R->cap_p;
  u3_noun pro = u3_nul;
  while ( u3_nul != dab ) {
    u3_noun arm, lef, rit;
    u3x_trel(dab, &arm, &lef, &rit);
    if ( c3n == u3du(arm) ) {
      u3l_log("bad look");
      return u3m_bail(c3__exit);
    }
    _arm_room(&pos_u, pil_u);
    u3_noun key = u3h(arm);
    c3_t leaf_t = !lef && !rit;
    c3_t same_t = (cog == key);
    c3_t left_t = 1;
    if ( !same_t ) {
      if ( leaf_t ) same_t = (c3y == u3r_sing(cog, key));
      else {
        if ( !*mug_h ) *mug_h = u3r_mug(cog);
        c3_h piv_h = u3r_mug(key);
        if ( *mug_h != piv_h ) left_t = (*mug_h < piv_h);
        else {
          same_t = (c3y == u3r_sing(cog, key));
          left_t = same_t || c3y == u3qc_dor(cog, key);
        }
      }
    }
    if ( same_t && c3y == u3du(u3t(arm)) ) {
      if ( !leaf_t ) pos_u.axe_w <<= 1;
      pro = u3nt(u3_nul, _arm_axis_atom(pos_u), u3k(u3t(arm)));
      break;
    }
    if ( leaf_t ) break;
    pos_u.axe_w = (lef && rit) ? (pos_u.axe_w << 2) + (left_t ? 2 : 3)
                               : (pos_u.axe_w << 1) + 1;
    dab = left_t ? u3h(u3t(dab)) : u3t(u3t(dab));
  }
  u3R->cap_p = cap_p;
  return pro;
}

u3_noun
u3qf_look(u3_noun cog, u3_noun dab)
{
  u3a_pile pil_u;
  u3a_pile_prep(&pil_u, sizeof(_arm_frame), alignof(_arm_frame));
  c3_h mug_h = 0;
  u3_noun pro = _look_at(cog, dab, &mug_h, (_arm_axis){0, 1}, &pil_u);
  u3R->cap_p = pil_u.beg_p;
  return pro;
}

static u3_noun
_loot(u3_noun cog, u3_noun dom, c3_t note_t)
{
  u3a_pile pil_u;
  u3a_pile_prep(&pil_u, sizeof(_arm_frame), alignof(_arm_frame));
  _arm_axis pos_u = {0, 1};
  c3_h mug_h = 0;
  while ( u3_nul != dom ) {
    u3_noun chapter, lef, rit;
    u3x_trel(dom, &chapter, &lef, &rit);
    if ( c3n == u3du(chapter) ) return u3m_bail(c3__fail);

    _arm_room(&pos_u, &pil_u);
    _arm_axis arm_u = pos_u;
    if ( lef || rit ) arm_u.axe_w <<= 1;
    u3_noun dab = note_t ? u3t(u3t(chapter)) : u3t(chapter);
    u3_noun pro = _look_at(cog, dab, &mug_h, arm_u, &pil_u);
    if ( u3_nul != pro ) {
      u3R->cap_p = pil_u.beg_p;
      return pro;
    }

    if ( lef && rit ) {
      //  Search the left chapter before resuming its sibling.
      //
      _arm_frame* top = u3a_push(&pil_u);
      *top = (_arm_frame){{pos_u.pre_u, (pos_u.axe_w << 2) + 3}, dom};
      pos_u.axe_w = (pos_u.axe_w << 2) + 2;
      dom = u3h(u3t(dom));
    }
    else if ( lef || rit ) {
      pos_u.axe_w = (pos_u.axe_w << 1) + 1;
      dom = lef ? u3h(u3t(dom)) : u3t(u3t(dom));
    }
    else {
      for (;;) {
        if ( c3y == u3a_pile_done(&pil_u) ) return u3_nul;
        _arm_frame top = *(_arm_frame*)u3a_peek(&pil_u);
        u3a_drop(&pil_u);
        if ( top.dom ) {
          dom = u3t(u3t(top.dom));
          pos_u = top.axis;
          break;
        }
      }
    }
  }
  u3R->cap_p = pil_u.beg_p;
  return u3_nul;
}

u3_noun
u3qf_loot(u3_noun cog, u3_noun dom)
{
  return _loot(cog, dom, 0);
}

u3_noun
u3qf_loot_136(u3_noun cog, u3_noun dom)
{
  return _loot(cog, dom, 1);
}

u3_noun
u3wf_look(u3_noun cor)
{
  u3_noun cog, dab;
  if ( c3n == u3r_mean(cor, {u3x_sam_2, &cog}, {u3x_sam_3, &dab}) ) {
    return u3m_bail(c3__fail);
  }
  return u3qf_look(cog, dab);
}

u3_noun
u3wf_loot(u3_noun cor)
{
  u3_noun cog, dom;
  if ( c3n == u3r_mean(cor, {u3x_sam_2, &cog}, {u3x_sam_3, &dom}) ) {
    return u3m_bail(c3__fail);
  }
  return u3qf_loot(cog, dom);
}

u3_noun
u3wf_loot_136(u3_noun cor)
{
  u3_noun cog, dom;
  if ( c3n == u3r_mean(cor, {u3x_sam_2, &cog}, {u3x_sam_3, &dom}) ) {
    return u3m_bail(c3__fail);
  }
  return u3qf_loot_136(cog, dom);
}
