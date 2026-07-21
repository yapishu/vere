/// @file
///
/// Link probe for diskless fake-ship boot construction under wasm32-wasi.

#include "mars_boot.h"
#include "hostfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PROBE_DEFAULT_LOOM_EXP  24
#define PROBE_REAL_PILL_PATH    "/brass.pill"
#define PROBE_EVENT_LOG_PATH    "/pier/.urb/log/events.bin"
#define PROBE_EVENT_LOG_VERSION 1

static const c3_y PROBE_EVENT_LOG_MAGIC[8] = {
  'm', 'a', 'r', 's', 'l', 'o', 'g', 1
};

static void
_probe_put32(c3_y* buf_y, c3_w val_w)
{
  buf_y[0] = val_w & 0xff;
  buf_y[1] = (val_w >> 8) & 0xff;
  buf_y[2] = (val_w >> 16) & 0xff;
  buf_y[3] = (val_w >> 24) & 0xff;
}

static c3_w
_probe_get32(const c3_y* buf_y)
{
  return (c3_w)buf_y[0]
       | ((c3_w)buf_y[1] << 8)
       | ((c3_w)buf_y[2] << 16)
       | ((c3_w)buf_y[3] << 24);
}

static u3_noun
_probe_ovum(u3_noun tag)
{
  return u3nc(u3nt(u3_blip, c3__arvo, u3_nul),
              u3nc(tag, u3_nul));
}

static u3_noun
_probe_pill(void)
{
  u3_noun bot = u3nc(_probe_ovum(c3__boot), u3_nul);
  u3_noun mod = u3nc(_probe_ovum(c3__test), u3_nul);
  u3_noun use = u3_nul;
  u3_noun dat = u3nq(c3__test, bot, mod, use);
  u3_noun dec = u3nc(c3__pill, dat);
  u3_atom jam = u3ke_jam(dec);

  return u3nc(jam, u3_nul);
}

static c3_o
_probe_list_len(u3_noun lit, c3_w* len_w)
{
  u3_noun len = u3qb_lent(lit);
  c3_o ret_o = u3r_safe_word(len, len_w);
  u3z(len);
  return ret_o;
}

static u3_noun
_probe_card(u3_noun ovo)
{
  return u3t(ovo);
}

static c3_o
_probe_has_arg(int argc, char** argv, const c3_c* arg_c)
{
  for ( c3_i i_i = 1; i_i < argc; i_i++ ) {
    if ( 0 == strcmp(argv[i_i], arg_c) ) {
      return c3y;
    }
  }

  return c3n;
}

static c3_o
_probe_loom_exp(int argc, char** argv, c3_w* exp_w)
{
  *exp_w = PROBE_DEFAULT_LOOM_EXP;

  for ( c3_i i_i = 1; i_i < argc; i_i++ ) {
    const c3_c* arg_c = argv[i_i];
    const c3_c* val_c = 0;

    if ( 0 == strncmp(arg_c, "--loom=", 7) ) {
      val_c = arg_c + 7;
    }
    else if ( 0 == strcmp(arg_c, "--loom") ) {
      if ( ++i_i >= argc ) {
        fprintf(stderr, "mars-boot: missing --loom value\r\n");
        return c3n;
      }
      val_c = argv[i_i];
    }

    if ( val_c ) {
      c3_c* end_c;
      unsigned long val_l = strtoul(val_c, &end_c, 10);

      if (  ('\0' != *end_c)
         || (val_l < 20)
         || (val_l > 31) )
      {
        fprintf(stderr, "mars-boot: invalid loom exponent %s\r\n", val_c);
        return c3n;
      }

      *exp_w = (c3_w)val_l;
    }
  }

  return c3y;
}

static c3_w
_probe_event_jam_len(u3_noun eve)
{
  u3_atom mat = u3qe_jam(eve);
  c3_w len_w = u3r_met(3, mat);
  u3z(mat);
  return len_w;
}

static c3_w
_probe_event_write(c3_y* buf_y, u3_noun eve)
{
  u3_atom mat = u3qe_jam(eve);
  c3_w len_w = u3r_met(3, mat);

  //  Boot event records use the same bytes as disk.c: mug then jam(event).
  //
  _probe_put32(buf_y, 0);
  u3r_bytes(0, len_w, buf_y + 4, mat);
  u3z(mat);

  return 4 + len_w;
}

static c3_o
_probe_ensure_event_log_dirs(void)
{
  return (  (c3y == u3fs_ensure_dir("mars event log", "/pier", 0700))
         && (c3y == u3fs_ensure_dir("mars event log", "/pier/.urb", 0700))
         && (c3y == u3fs_ensure_dir("mars event log", "/pier/.urb/log", 0700)) )
       ? c3y
       : c3n;
}

static c3_o
_probe_event_log_save(u3_noun ova, c3_w len_w)
{
  c3_d tot_d = 16;

  for ( u3_noun lit = ova; u3_nul != lit; lit = u3t(lit) ) {
    tot_d += 4 + 4 + _probe_event_jam_len(u3h(lit));
  }

  if ( tot_d > UINT32_MAX ) {
    fprintf(stderr, "mars-boot: event log too large\r\n");
    return c3n;
  }

  if ( c3n == _probe_ensure_event_log_dirs() ) {
    return c3n;
  }

  c3_y* buf_y;
  if ( c3n == u3fs_mmap("mars event log",
                        (c3_c*)PROBE_EVENT_LOG_PATH,
                        tot_d,
                        &buf_y) )
  {
    return c3n;
  }

  memcpy(buf_y, PROBE_EVENT_LOG_MAGIC, sizeof(PROBE_EVENT_LOG_MAGIC));
  _probe_put32(buf_y + 8, PROBE_EVENT_LOG_VERSION);
  _probe_put32(buf_y + 12, len_w);

  c3_w off_w = 16;
  for ( u3_noun lit = ova; u3_nul != lit; lit = u3t(lit) ) {
    c3_w rec_w = _probe_event_write(buf_y + off_w + 4, u3h(lit));
    _probe_put32(buf_y + off_w, rec_w);
    off_w += 4 + rec_w;
  }

  if (  (c3n == u3fs_mmap_save("mars event log",
                               (c3_c*)PROBE_EVENT_LOG_PATH,
                               tot_d,
                               buf_y))
     || (c3n == u3fs_munmap(tot_d, buf_y)) )
  {
    return c3n;
  }

  u3l_log("mars-boot: saved event log %s events=%u bytes=%" PRIc3_d,
          PROBE_EVENT_LOG_PATH, len_w, tot_d);
  return c3y;
}

static c3_o
_probe_event_log_load(u3_noun* out_ova, c3_w* out_len_w)
{
  c3_d len_d;
  c3_y* buf_y;

  if ( c3n == u3fs_mmap_read("mars event log",
                             (c3_c*)PROBE_EVENT_LOG_PATH,
                             &len_d,
                             &buf_y) )
  {
    return c3n;
  }

  c3_o ret_o = c3n;
  u3_noun ova = u3_nul;

  if (  (len_d < 16)
     || (0 != memcmp(buf_y, PROBE_EVENT_LOG_MAGIC,
                     sizeof(PROBE_EVENT_LOG_MAGIC)))
     || (PROBE_EVENT_LOG_VERSION != _probe_get32(buf_y + 8)) )
  {
    fprintf(stderr, "mars-boot: invalid event log header\r\n");
    goto done;
  }

  c3_w cnt_w = _probe_get32(buf_y + 12);
  c3_d off_d = 16;

  for ( c3_w i_w = 0; i_w < cnt_w; i_w++ ) {
    if ( (off_d + 4) > len_d ) {
      fprintf(stderr, "mars-boot: truncated event log index\r\n");
      goto done;
    }

    c3_w rec_w = _probe_get32(buf_y + off_d);
    off_d += 4;

    if (  (4 >= rec_w)
       || ((off_d + rec_w) > len_d) )
    {
      fprintf(stderr, "mars-boot: truncated event log record\r\n");
      goto done;
    }

    u3_noun job = u3ke_cue(u3i_bytes(rec_w - 4, buf_y + off_d + 4));
    ova = u3nc(job, ova);
    off_d += rec_w;
  }

  if ( off_d != len_d ) {
    fprintf(stderr, "mars-boot: trailing event log bytes\r\n");
    goto done;
  }

  *out_ova = u3kb_flop(ova);
  *out_len_w = cnt_w;
  ova = u3_nul;
  ret_o = c3y;

done:
  u3z(ova);
  u3fs_munmap(len_d, buf_y);
  if ( c3y == ret_o ) {
    u3l_log("mars-boot: loaded event log %s events=%u bytes=%" PRIc3_d,
            PROBE_EVENT_LOG_PATH, *out_len_w, len_d);
  }
  return ret_o;
}

static c3_o
_probe_boot_pill(u3_noun    pill,
                 const c3_c* lab_c,
                 c3_w       min_w,
                 c3_w       len_want,
                 c3_w       lif_want,
                 c3_o       noc_o,
                 u3_noun*   out_ova,
                 u3_noun*   out_cax)
{
  u3_mars_boot_opts inp_u = {0};
  for ( c3_w i_w = 0; i_w < 16; i_w++ ) {
    inp_u.eny_w[i_w] = 0x1000 + i_w;
  }
  inp_u.veb_o = c3n;
  inp_u.lit_o = c3n;
  inp_u.sev_l = 7;
  inp_u.tim_u.tv_sec = 1;
  inp_u.tim_u.tv_usec = 250000;
  inp_u.ver_u.nam_m = c3__zuse;
  inp_u.ver_u.ver_w = 408;

  u3_noun com = u3nt(pill,
                     u3nc(c3__fake, 0),
                     u3_nul);
  u3_noun ova = u3_nul;
  u3_noun cax = u3_nul;
  u3_mars_boot_meta met_u = {0};

  if ( c3n == u3_mars_boot_make(&inp_u, com, &ova, &cax, &met_u) ) {
    fprintf(stderr, "mars-boot: constructor failed\r\n");
    return c3n;
  }

  c3_w len_w = 0;
  if (  (c3n == _probe_list_len(ova, &len_w))
     || (len_w < min_w)
     || (len_want && (len_want != len_w)) )
  {
    fprintf(stderr, "mars-boot: event count mismatch: %u\r\n", len_w);
    return c3n;
  }

  if (  (c3y != met_u.fak_o)
     || (0 != met_u.who_d[0])
     || (0 != met_u.who_d[1])
     || (0 == met_u.lif_w)
     || (lif_want && (lif_want != met_u.lif_w)) )
  {
    fprintf(stderr, "mars-boot: metadata mismatch\r\n");
    return c3n;
  }

  if ( len_want ) {
    u3_noun first = u3h(ova);
    if ( c3__boot != u3h(_probe_card(first)) ) {
      fprintf(stderr, "mars-boot: first boot formula mismatch\r\n");
      return c3n;
    }

    u3_noun second = u3h(u3t(ova));
    if ( c3__wyrd != u3h(_probe_card(u3t(second))) ) {
      fprintf(stderr, "mars-boot: wyrd card missing\r\n");
      return c3n;
    }

    u3_noun last = u3h(u3t(u3t(u3t(u3t(u3t(u3t(ova)))))));
    if ( c3__boot != u3h(_probe_card(u3t(last))) ) {
      fprintf(stderr, "mars-boot: legacy boot card missing\r\n");
      return c3n;
    }
  }

  if ( (c3y == noc_o) && (u3_nul != cax) ) {
    fprintf(stderr, "mars-boot: unexpected cache\r\n");
    return c3n;
  }
  else if ( (0 == out_cax) && (u3_nul != cax) ) {
    u3z(cax);
  }

  if ( 0 == strcmp(lab_c, "synthetic") ) {
    u3l_log("mars-boot: fake zod events=%u life=%u", len_w, met_u.lif_w);
  }
  else {
    u3l_log("mars-boot: %s fake zod events=%u life=%u",
            lab_c, len_w, met_u.lif_w);
  }

  if ( out_ova ) {
    *out_ova = ova;
  }
  else {
    u3z(ova);
  }

  if ( out_cax ) {
    *out_cax = cax;
  }
  return c3y;
}

static c3_o
_probe_run_boot(u3_noun ova, u3_noun cax)
{
  //  Mirror native mars' boot-time sharing recovery and memo priming.
  //
  u3m_hate(1 << 18);
  u3_noun xev = u3m_love(u3ke_cue(u3ke_jam(u3nc(cax, ova))));
  u3x_cell(xev, &cax, &ova);
  u3k(cax); u3k(ova);
  u3z(xev);

  xev = cax;
  while ( u3_nul != cax ) {
    u3z_save_m(u3z_memo_keep, 144 + c3__nock, u3h(u3h(cax)),
               u3t(u3h(cax)));
    cax = u3t(cax);
  }
  u3z(xev);

  u3l_log("--------------- bootstrap starting ----------------");
  u3l_log("boot: 1-%u", u3qb_lent(ova));

  if ( c3n == u3v_boot(ova) ) {
    return c3n;
  }

  u3l_log("--------------- bootstrap complete ----------------");
  u3l_log("mars-boot: booted fake zod core %x", u3r_mug(u3A->roc));
  return c3y;
}

int
main(int argc, char** argv)
{
  c3_w exp_w;
  if ( c3n == _probe_loom_exp(argc, argv, &exp_w) ) {
    return 1;
  }

  c3_o real_o = _probe_has_arg(argc, argv, "--real-pill");
  c3_o run_o = _probe_has_arg(argc, argv, "--run-boot");
  c3_o save_o = _probe_has_arg(argc, argv, "--save-events");
  c3_o load_o = _probe_has_arg(argc, argv, "--load-events");

  if ( (c3y == run_o) && (c3n == real_o) ) {
    fprintf(stderr, "mars-boot: --run-boot requires --real-pill\r\n");
    return 1;
  }
  if ( (c3y == save_o) && (c3n == real_o) ) {
    fprintf(stderr, "mars-boot: --save-events requires --real-pill\r\n");
    return 1;
  }
  if ( (c3y == load_o) && (c3n == real_o) ) {
    fprintf(stderr, "mars-boot: --load-events requires --real-pill\r\n");
    return 1;
  }

  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite((size_t)1 << exp_w);

  if ( c3n == _probe_boot_pill(_probe_pill(),
                               "synthetic",
                               7,
                               7,
                               1,
                               c3y,
                               0,
                               0) )
  {
    return 1;
  }

  if ( c3y == real_o ) {
    if ( c3n == u3fs_exists((c3_c*)PROBE_REAL_PILL_PATH) ) {
      fprintf(stderr, "mars-boot: missing %s\r\n", PROBE_REAL_PILL_PATH);
      return 1;
    }

    u3_noun pill = u3nc(u3m_file((c3_c*)PROBE_REAL_PILL_PATH), u3_nul);
    u3_noun ova = u3_nul;
    u3_noun cax = u3_nul;
    c3_w len_w = 0;
    c3_o keep_o = __(  (c3y == run_o)
                     || (c3y == save_o)
                     || (c3y == load_o) );

    if ( c3n == _probe_boot_pill(pill,
                                 "real brass",
                                 8,
                                 0,
                                 0,
                                 c3n,
                                 (c3y == keep_o) ? &ova : 0,
                                 (c3y == run_o) ? &cax : 0) )
    {
      return 1;
    }

    if ( c3y == keep_o ) {
      if ( c3n == _probe_list_len(ova, &len_w) ) {
        fprintf(stderr, "mars-boot: unable to measure event list\r\n");
        return 1;
      }
    }

    if ( c3y == save_o ) {
      if ( c3n == _probe_event_log_save(ova, len_w) ) {
        return 1;
      }
    }

    if ( c3y == load_o ) {
      u3_noun lova = u3_nul;
      c3_w llen_w = 0;
      if ( c3n == _probe_event_log_load(&lova, &llen_w) ) {
        return 1;
      }
      if (  (llen_w != len_w)
         || (c3n == u3r_sing(ova, lova)) )
      {
        fprintf(stderr, "mars-boot: loaded event log mismatch\r\n");
        u3z(lova);
        return 1;
      }
      u3z(ova);
      ova = lova;
    }

    if ( c3y == run_o ) {
      if ( c3n == _probe_run_boot(ova, cax) ) {
        fprintf(stderr, "mars-boot: Arvo bootstrap failed\r\n");
        return 1;
      }
    }
    else if ( c3y == keep_o ) {
      u3z(ova);
    }
  }

  u3m_stop();
  return 0;
}
