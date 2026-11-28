/// @file

#include "jets/w.h"
#include "noun.h"

/* Outside its type tests and logical connectives, +chip applies +open
** until the expression stops changing, then returns sut. Slots, limbs
** and wings stop at %cnts. Calls and subject pushes stop at %tsgr.
** https://github.com/urbit/urbit/blob/5a187feded/pkg/arvo/sys/hoon.hoon#L9755-L9774
*/
u3_weak
u3wfu_chip_135(u3_noun cor)
{
  u3_noun how, gen, sut, tag, arg;
  if ( c3n == u3r_mean(cor, {u3x_sam_2, &how}, {u3x_sam_3, &gen},
                             {u3x_con_sam, &sut}) || how > 1 ) return u3_none;

  while ( c3y == u3r_cell(gen, &tag, &arg) ) {
    switch ( tag ) {
      case c3__dbug:
        if ( c3n == u3r_cell(arg, 0, &gen) ) return u3_none;
        continue;

      case 0:        case c3__limb: case c3__wing: case c3__cnts:
      case c3__tsgr: case c3__wtcl: case c3__rock: case c3__sand:
      case c3__dtts: case c3__dtwt: case c3__dttr: case c3__fits:
        return u3k(sut);

      case c3__cnhp: case c3__tsgl: case c3__tsls:
        return (c3y == u3du(arg)) ? u3k(sut) : u3_none;

      case c3__cncl:
        if ( c3n == u3r_cell(arg, 0, &arg) ) return u3_none;
        while ( c3y == u3du(arg) ) arg = u3t(arg);
        return arg ? u3_none : u3k(sut);

      default: return u3_none;
    }
  }
  return u3_none;
}
