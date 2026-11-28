/// @file

#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"

/* +open:ap expands a gate call in two steps. %cnsg lays out arguments
** along the sample's right spine. %cntr evaluates each edit under the
** original subject, which is at slot 3 after the gate is pushed.
** https://github.com/urbit/urbit/blob/5a187feded/pkg/arvo/sys/hoon.hoon#L8507-L8527
*/
static u3_weak
_open_cnsg(u3_noun wing, u3_noun gate, u3_noun args)
{
  u3_noun edits, *end = &edits;
  u3_atom axe = 6;
  u3_noun anon = u3nt(c3n, 0, u3_nul);
  while ( c3y == u3du(args) ) {
    u3_noun arg = u3h(args), *head, *next;
    args = u3t(args);
    u3_atom slot = args ? u3qc_peg(axe, 2) : u3k(axe);
    u3_noun path = u3nt(u3k(anon), u3nc(c3y, slot), u3_nul);
    *end = u3i_defcons(&head, &next);
    end = next;
    *head = u3nc(path, u3k(arg));
    if ( args ) {
      u3_atom sibling = u3qc_peg(axe, 3);
      u3z(axe);
      axe = sibling;
    }
  }
  *end = u3_nul;
  u3z(anon);
  u3z(axe);
  if ( args ) { u3z(edits); return u3_none; }
  return u3nq(c3__cntr, u3k(wing), u3k(gate), edits);
}

static u3_weak
_open_cntr(u3_noun wing, u3_noun gate, u3_noun edits)
{
  if ( !edits ) {
    return u3nt(c3__tsgr, u3k(gate), u3nc(c3__wing, u3k(wing)));
  }
  u3_noun at = wing;
  while ( c3y == u3du(at) ) at = u3t(at);
  if ( at ) return u3_none;

  u3_noun changes, *end = &changes;
  u3_noun tail = u3nc(0, 3), path, gen;
  while ( c3y == u3du(edits) && c3y == u3r_cell(u3h(edits), &path, &gen) ) {
    u3_noun *head, *next;
    *end = u3i_defcons(&head, &next);
    end = next;
    *head = u3nc(u3k(path), u3nt(c3__tsgr, u3k(tail), u3k(gen)));
    edits = u3t(edits);
  }
  *end = u3_nul;
  u3z(tail);
  if ( edits ) { u3z(changes); return u3_none; }

  u3_noun way = u3nc(u3nc(c3y, 2), u3_nul);
  u3_noun arm = u3qb_weld(wing, way);
  u3z(way);
  return u3nt(c3__tsls, u3k(gate), u3nt(c3__cnts, arm, changes));
}

u3_noun
u3wfp_open(u3_noun cor)
{
  u3_noun gen, tag, arg, wing, gate, edits;
  if ( u3_none == (gen = u3r_at(u3x_sam, cor)) ||
       c3n == u3r_cell(gen, &tag, &arg) ||
       (tag != c3__cnsg && tag != c3__cntr) ||
       c3n == u3r_trel(arg, &wing, &gate, &edits) ) return u3_none;
  return tag == c3__cnsg ? _open_cnsg(wing, gate, edits)
                         : _open_cntr(wing, gate, edits);
}
