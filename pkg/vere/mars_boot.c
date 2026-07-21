/// @file
///
/// Diskless boot-sequence construction shared by native mars and WASM probes.

#include "mars_boot.h"

#ifndef U3_VERE_PACE
#include "pace.h"
#endif
#ifndef URBIT_VERSION
#include "version.h"
#endif

#include <stdio.h>

#define VERE_ZUSE  408
#define VERE_LULL  320
#define VERE_ARVO  234
#define VERE_HOON  135
#define VERE_NOCK  4

/* _mars_wyrd_card(): construct %wyrd.
*/
static u3_noun
_mars_wyrd_card(c3_m nam_m, c3_w ver_w, c3_l sev_l)
{
  //  XX ghetto (scot %ta)
  //
  u3_noun ver = u3nq(c3__vere,
                     u3i_string(U3_VERE_PACE),
                     u3i_string("~." URBIT_VERSION),
                     u3_nul);
  u3_noun sen = u3i_string("0v1s.vu178");
  u3_noun kel;

  //  special case versions requiring the full stack
  //
  if (  ((c3__zuse == nam_m) && (VERE_ZUSE == ver_w))
     || ((c3__lull == nam_m) && (VERE_LULL == ver_w))
     || ((c3__arvo == nam_m) && (VERE_ARVO == ver_w)) )
  {
    kel = u3nl(u3nc(c3__zuse, VERE_ZUSE),
               u3nc(c3__lull, VERE_LULL),
               u3nc(c3__arvo, VERE_ARVO),
               u3nc(c3__hoon, VERE_HOON),
               u3nc(c3__nock, VERE_NOCK),
               u3_none);
  }
  //  XX speculative!
  //
  else {
    kel = u3nc(nam_m, u3i_word(ver_w));
  }

  return u3nt(c3__wyrd, u3nc(sen, ver), kel);
}

/* _mars_sift_pill(): extract boot formulas and module/userspace ova from pill
*/
static c3_o
_mars_sift_pill(u3_noun  pil,
                u3_noun* bot,
                u3_noun* mod,
                u3_noun* use,
                u3_noun* cax)
{
  u3_noun pil_p, pil_q;
  *cax = u3_nul;

  if ( c3n == u3r_cell(pil, &pil_p, &pil_q) ) {
    return c3n;
  }

  {
    //  XX use faster cue
    //
    u3_noun pro = u3m_soft(0, u3ke_cue, u3k(pil_p));
    u3_noun mot, tag, dat;

    if (  (c3n == u3r_trel(pro, &mot, &tag, &dat))
       || (u3_blip != mot) )
    {
      u3m_p("mot", u3h(pro));
      fprintf(stderr, "boot: failed: unable to parse pill\r\n");
      return c3n;
    }

    if ( c3y == u3r_sing_c("ivory", tag) ) {
      fprintf(stderr, "boot: failed: unable to boot from ivory pill\r\n");
      return c3n;
    }
    else if ( (c3__pill != tag) && (c3__cash != tag) ) {
      if ( c3y == u3a_is_atom(tag) ) {
        u3m_p("pill", tag);
      }
      fprintf(stderr, "boot: failed: unrecognized pill\r\n");
      return c3n;
    }

    {
      u3_noun typ;

      if ( (c3__cash == tag) && (c3y == u3du(dat)) ) {
        *cax = u3t(dat);
        dat = u3h(dat);
      }

      if ( c3n == u3r_qual(dat, &typ, bot, mod, use) ) {
        fprintf(stderr, "boot: failed: unable to extract pill\r\n");
        return c3n;
      }

      if ( c3y == u3a_is_atom(typ) ) {
        c3_c* typ_c = u3r_string(typ);
        fprintf(stderr, "boot: parsing %%%s pill\r\n", typ_c);
        c3_free(typ_c);
      }
    }

    u3k(*bot); u3k(*mod); u3k(*use), u3k(*cax);
    u3z(pro);
  }

  //  optionally replace filesystem in userspace
  //
  if ( u3_nul != pil_q ) {
    c3_w  len_w = 0;
    u3_noun ova = *use;
    u3_noun new = u3_nul;
    u3_noun ovo, tag;

    while ( u3_nul != ova ) {
      ovo = u3h(ova);
      tag = u3h(u3t(ovo));

      if (  (c3__into == tag)
         || (  (c3__park == tag)
            && (c3__base == u3h(u3t(u3t(ovo)))) ) )
      {
        u3_assert( 0 == len_w );
        len_w++;
        ovo = u3t(pil_q);
      }

      new = u3nc(u3k(ovo), new);
      ova = u3t(ova);
    }

    u3_assert( 1 == len_w );

    u3z(*use);
    *use = u3kb_flop(new);
  }

  u3z(pil);

  return c3y;
}

/* u3_mars_boot_make(): construct timestamped boot event list.
*/
c3_o
u3_mars_boot_make(u3_mars_boot_opts* inp_u,
                  u3_noun           com,
                  u3_noun*          ova,
                  u3_noun*          xac,
                  u3_mars_boot_meta* met_u)
{
  //  set the disk version
  //
  met_u->ver_w = U3D_VERLAT;

  u3_noun pil, ven, mor, who;

  //  parse boot command
  //
  if ( c3n == u3r_trel(com, &pil, &ven, &mor) ) {
    fprintf(stderr, "boot: invalid command\r\n");
    return c3n;
  }

  //  parse boot event
  //
  {
    u3_noun tag, dat;

    if ( c3n == u3r_cell(ven, &tag, &dat) ) {
      return c3n;
    }

    switch ( tag ) {
      default: {
        fprintf(stderr, "boot: unknown boot event\r\n");
        u3m_p("tag", tag);
        return c3n;
      }

      case c3__fake: {
        met_u->fak_o = c3y;
        who          = dat;
      } break;

      case c3__dawn: {
        met_u->fak_o = c3n;
        who          = u3h(u3t(u3h(dat)));
      } break;
    }
  }

  //  validate and extract identity
  //
  if (  (c3n == u3a_is_atom(who))
     || (1 < u3r_met(7, who)) )
  {
    fprintf(stderr, "boot: invalid identity\r\n");
    u3m_p("who", who);
    return c3n;
  }

  u3r_chubs(0, 2, met_u->who_d, who);

  {
    u3_noun bot, mod, use, cax;

    //  parse pill
    //
    if ( c3n == _mars_sift_pill(u3k(pil), &bot, &mod, &use, &cax) ) {
      return c3n;
    }

    met_u->lif_w = u3qb_lent(bot);

    //  break symmetry in the module sequence
    //
    //    version negotation, verbose, identity, entropy
    //
    {
      u3_noun cad, wir = u3nt(u3_blip, c3__arvo, u3_nul);

      cad = u3nc(c3__wack, u3i_words(16, inp_u->eny_w));
      mod = u3nc(u3nc(u3k(wir), cad), mod);

      cad = u3nc(c3__whom, u3k(who));
      mod = u3nc(u3nc(u3k(wir), cad), mod);

      cad = u3nt(c3__verb, u3_nul, !inp_u->veb_o);
      mod = u3nc(u3nc(u3k(wir), cad), mod);

      cad = _mars_wyrd_card(inp_u->ver_u.nam_m,
                            inp_u->ver_u.ver_w,
                            inp_u->sev_l);
      mod = u3nc(u3nc(wir, cad), mod);  //  transfer [wir]
    }

    //  prepend legacy boot event to the userspace sequence
    //
    //    XX do something about this wire
    //
    {
      u3_noun wir = u3nq(c3__d, c3__term, '1', u3_nul);
      u3_noun cad = u3nt(c3__boot, inp_u->lit_o, u3k(ven));
      use = u3nc(u3nc(wir, cad), use);
    }

    //  add props before/after the userspace sequence
    //
    {
      u3_noun pre = u3_nul;
      u3_noun aft = u3_nul;

      while ( u3_nul != mor ) {
        u3_noun mot = u3h(mor);

        switch ( u3h(mot) ) {
          case c3__prop: {
            u3_noun ter, met, ves;

            if ( c3n == u3r_trel(u3t(mot), &met, &ter, &ves) ) {
              //  XX fatal error?
              //
              u3m_p("invalid prop", u3t(mot));
              break;
            }

            if ( c3__fore == ter ) {
              u3m_p("prop: fore", met);
              pre = u3kb_weld(pre, u3k(ves));
            }
            else if ( c3__hind == ter ) {
              u3m_p("prop: hind", met);
              aft = u3kb_weld(aft, u3k(ves));
            }
            else {
              //  XX fatal error?
              //
              u3m_p("unrecognized prop tier", ter);
            }
          } break;

          //  XX fatal error?
          //
          default: u3m_p("unrecognized boot sequence enhancement", u3h(mot));
        }

        mor = u3t(mor);
      }

      use = u3kb_weld(pre, u3kb_weld(use, aft));
    }

    //  timestamp events, cons list
    //
    {
      u3_noun now = u3m_time_in_tv(&inp_u->tim_u);
      u3_noun bit = u3qc_bex(48);       //  1/2^16 seconds
      u3_noun eve = u3kb_flop(bot);

      {
        u3_noun  lit = u3kb_weld(mod, use);
        u3_noun i, t = lit;

        while ( u3_nul != t ) {
          u3x_cell(t, &i, &t);
          now = u3ka_add(now, u3k(bit));
          eve = u3nc(u3nc(u3k(now), u3k(i)), eve);
        }

        u3z(lit);
      }

      *ova = u3kb_flop(eve);
      u3z(now); u3z(bit);
    }

    //  cache
    //
    {
      u3_noun tmp = cax;
      c3_o gud_o = c3y;
      while ( u3_nul != tmp ) {
        if ( (c3n == u3a_is_cell(tmp)) ||
             (c3n == u3a_is_cell(u3h(tmp))) ||
             (c3n == u3a_is_cell(u3h(u3h(tmp)))) )
        {
          gud_o = c3n;
        }
        tmp = u3t(tmp);
      }

      if ( c3n == gud_o ) {
        u3l_log("mars: got bad cache");
        u3z(cax);
        *xac = u3_nul;
      }
      else {
        *xac = cax;
      }
    }
  }

  u3z(com);

  return c3y;
}
