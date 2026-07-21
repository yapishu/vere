/// @file
///
/// Link probe for the WASM u3_disk backend over hostfs.

#include "mars_boot.h"
#include "hostfs.h"
#include "vere.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define PROBE_DEFAULT_LOOM_EXP  29
#define PROBE_REAL_PILL_PATH    "/brass.pill"
#define PROBE_PIER_PATH         "/pier"
#define PROBE_MAX_HOST_POKES    32
#define PROBE_REACTOR_ARG_BYTES 4096

#if defined(__wasm__)
#define PROBE_EXPORT(name) __attribute__((export_name(name), visibility("default")))
extern void __wasm_call_ctors(void);
#else
#define PROBE_EXPORT(name)
#endif

#if defined(__clang__)
#define PROBE_NO_STACK_PROTECTOR __attribute__((no_stack_protector))
#else
#define PROBE_NO_STACK_PROTECTOR
#endif

typedef struct _probe_host_poke {
  const c3_c* ovum_c;
  const c3_c* effects_c;
} _probe_host_poke;

static c3_o     _probe_reactor_ctors_o = c3n;
static c3_o     _probe_reactor_live_o = c3n;
static u3_disk* _probe_reactor_log_u = 0;
static c3_d     _probe_reactor_boot_len_d = 0;
static c3_c     _probe_reactor_arg0_c[PROBE_REACTOR_ARG_BYTES];
static c3_c     _probe_reactor_arg1_c[PROBE_REACTOR_ARG_BYTES];

static void
_probe_dump_tape(u3_noun tep)
{
  while ( c3y == u3du(tep) ) {
    fputc((c3_i)u3h(tep), stderr);
    tep = u3t(tep);
  }
}

static void
_probe_slog(u3_noun hod)
{
  u3_noun pri, tac;

  if ( c3y == u3r_cell(hod, &pri, &tac) ) {
    (void)pri;

    if ( c3y == u3a_is_atom(tac) ) {
      c3_c* str_c = u3r_string(tac);
      fputs(str_c, stderr);
      c3_free(str_c);
    }
    else if ( c3__leaf == u3h(tac) ) {
      _probe_dump_tape(u3t(tac));
    }
    else {
      fputs("disk-wasm: slog tank\r\n", stderr);
    }

    fputs("\r\n", stderr);
  }

  u3z(hod);
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
_probe_arg_value(int argc, char** argv, const c3_c* arg_c, const c3_c** out_c)
{
  size_t len_i = strlen(arg_c);
  *out_c = 0;

  for ( c3_i i_i = 1; i_i < argc; i_i++ ) {
    if ( 0 == strcmp(argv[i_i], arg_c) ) {
      if ( ++i_i >= argc ) {
        fprintf(stderr, "disk-wasm: missing %s value\r\n", arg_c);
        return c3n;
      }

      *out_c = argv[i_i];
      return c3y;
    }
    else if (  (0 == strncmp(argv[i_i], arg_c, len_i))
            && ('=' == argv[i_i][len_i]) )
    {
      *out_c = argv[i_i] + len_i + 1;
      return c3y;
    }
  }

  return c3y;
}

static c3_o
_probe_arg_match(int argc,
                 char** argv,
                 c3_i* i_i,
                 const c3_c* arg_c,
                 const c3_c** out_c)
{
  size_t len_i = strlen(arg_c);
  const c3_c* inp_c = argv[*i_i];

  if ( 0 == strcmp(inp_c, arg_c) ) {
    if ( ++(*i_i) >= argc ) {
      fprintf(stderr, "disk-wasm: missing %s value\r\n", arg_c);
      return c3n;
    }
    *out_c = argv[*i_i];
    return c3y;
  }
  else if (  (0 == strncmp(inp_c, arg_c, len_i))
          && ('=' == inp_c[len_i]) )
  {
    *out_c = inp_c + len_i + 1;
    return c3y;
  }

  *out_c = 0;
  return c3y;
}

static c3_o
_probe_parse_host_pokes(int argc,
                        char** argv,
                        _probe_host_poke* pok_u,
                        c3_w* pok_w)
{
  *pok_w = 0;

  for ( c3_i i_i = 1; i_i < argc; i_i++ ) {
    const c3_c* val_c;

    if ( c3n == _probe_arg_match(argc, argv, &i_i, "--poke-ovum", &val_c) ) {
      return c3n;
    }
    if ( val_c ) {
      if ( PROBE_MAX_HOST_POKES == *pok_w ) {
        fprintf(stderr, "disk-wasm: too many --poke-ovum arguments\r\n");
        return c3n;
      }

      pok_u[*pok_w].ovum_c = val_c;
      pok_u[*pok_w].effects_c = 0;
      (*pok_w)++;
      continue;
    }

    if ( c3n == _probe_arg_match(argc, argv, &i_i, "--effects-out", &val_c) ) {
      return c3n;
    }
    if ( val_c ) {
      if ( 0 == *pok_w ) {
        continue;
      }
      if ( pok_u[*pok_w - 1].effects_c ) {
        fprintf(stderr, "disk-wasm: duplicate --effects-out for %s\r\n",
                        pok_u[*pok_w - 1].ovum_c);
        return c3n;
      }

      pok_u[*pok_w - 1].effects_c = val_c;
    }
  }

  return c3y;
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
        fprintf(stderr, "disk-wasm: missing --loom value\r\n");
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
        fprintf(stderr, "disk-wasm: invalid loom exponent %s\r\n", val_c);
        return c3n;
      }

      *exp_w = (c3_w)val_l;
    }
  }

  return c3y;
}

static c3_o
_probe_fake_ship(int argc, char** argv, c3_d who_d[2])
{
  const c3_c* val_c;
  who_d[0] = 0;
  who_d[1] = 0;

  if ( c3n == _probe_arg_value(argc, argv, "--fake-ship", &val_c) ) {
    return c3n;
  }
  if ( !val_c ) {
    return c3y;
  }

  c3_c* end_c;
  int bas_i = (  (0 == strncmp(val_c, "0x", 2))
              || (0 == strncmp(val_c, "0X", 2)) )
            ? 16
            : 10;
  unsigned long long val_d = strtoull(val_c, &end_c, bas_i);

  if ( ('\0' != *end_c) ) {
    fprintf(stderr, "disk-wasm: invalid --fake-ship value %s\r\n", val_c);
    return c3n;
  }

  who_d[0] = (c3_d)val_d;
  return c3y;
}

static c3_o
_probe_read_jam(const c3_c* pat_c, u3_noun* out)
{
  c3_d len_d;
  c3_y* buf_y;

  if ( c3n == u3fs_mmap_read((c3_c*)"disk-wasm: read jam",
                             (c3_c*)pat_c,
                             &len_d,
                             &buf_y) )
  {
    return c3n;
  }

  *out = u3s_cue_xeno(len_d, buf_y);
  u3fs_munmap(len_d, buf_y);

  if ( u3_none == *out ) {
    fprintf(stderr, "disk-wasm: cue failed (%s)\r\n", pat_c);
    return c3n;
  }

  return c3y;
}

static c3_o
_probe_write_jam(const c3_c* pat_c, u3_noun nun, c3_d* len_d)
{
  c3_y* buf_y = 0;

  *len_d = 0;

  if ( 0 == u3s_jam_xeno(nun, len_d, &buf_y) ) {
    fprintf(stderr, "disk-wasm: jam failed (%s)\r\n", pat_c);
    return c3n;
  }

  c3_i fil_i = u3fs_open((c3_c*)"disk-wasm: write jam",
                         (c3_c*)pat_c,
                         U3FS_O_WRITE | U3FS_O_CREATE | U3FS_O_TRUNC,
                         0600);
  if ( fil_i < 0 ) {
    free(buf_y);
    return c3n;
  }

  c3_o ret_o = c3y;
  if (  (c3n == u3fs_write_at((c3_c*)"disk-wasm: write jam",
                              fil_i, 0, *len_d, buf_y))
     || (c3n == u3fs_sync((c3_c*)"disk-wasm: write jam", fil_i)) )
  {
    ret_o = c3n;
  }

  if ( c3n == u3fs_close((c3_c*)"disk-wasm: write jam", fil_i) ) {
    ret_o = c3n;
  }

  free(buf_y);
  return ret_o;
}

static c3_o
_probe_list_len(u3_noun lit, c3_w* len_w)
{
  u3_noun len = u3qb_lent(lit);
  c3_o ret_o = u3r_safe_word(len, len_w);
  u3z(len);
  return ret_o;
}

static c3_o
_probe_make_boot(u3_noun* out_ova,
                 u3_noun* out_cax,
                 u3_meta* out_met,
                 c3_d who_d[2])
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

  if ( c3n == u3fs_exists((c3_c*)PROBE_REAL_PILL_PATH) ) {
    fprintf(stderr, "disk-wasm: missing %s\r\n", PROBE_REAL_PILL_PATH);
    return c3n;
  }

  u3_noun pill = u3nc(u3m_file((c3_c*)PROBE_REAL_PILL_PATH), u3_nul);
  u3_noun com = u3nt(pill, u3nc(c3__fake, u3i_chubs(2, who_d)), u3_nul);
  u3_mars_boot_meta met_u = {0};
  u3_noun ova = u3_nul;
  u3_noun cax = u3_nul;

  if ( c3n == u3_mars_boot_make(&inp_u, com, &ova, &cax, &met_u) ) {
    fprintf(stderr, "disk-wasm: boot constructor failed\r\n");
    return c3n;
  }

  out_met->ver_w = met_u.ver_w;
  out_met->who_d[0] = met_u.who_d[0];
  out_met->who_d[1] = met_u.who_d[1];
  out_met->fak_o = met_u.fak_o;
  out_met->lif_w = met_u.lif_w;

  *out_ova = ova;
  *out_cax = cax;
  return c3y;
}

static c3_o
_probe_meta_match(const u3_meta* a_u, const u3_meta* b_u)
{
  return (  (a_u->ver_w == b_u->ver_w)
         && (a_u->who_d[0] == b_u->who_d[0])
         && (a_u->who_d[1] == b_u->who_d[1])
         && (a_u->fak_o == b_u->fak_o)
         && (a_u->lif_w == b_u->lif_w) )
       ? c3y
       : c3n;
}

static c3_o
_probe_walk_count(u3_disk* log_u, c3_w len_w)
{
  u3_disk_walk* wok_u = u3_disk_walk_init(log_u, 1, len_w);
  c3_w got_w = 0;

  if ( !wok_u ) {
    fprintf(stderr, "disk-wasm: walk init failed\r\n");
    return c3n;
  }

  while ( c3y == u3_disk_walk_live(wok_u) ) {
    u3_fact tac_u;

    if ( c3n == u3_disk_walk_step(wok_u, &tac_u) ) {
      u3_disk_walk_done(wok_u);
      return c3n;
    }

    if ( tac_u.eve_d != (1 + got_w) ) {
      fprintf(stderr, "disk-wasm: walk event mismatch\r\n");
      u3z(tac_u.job);
      u3_disk_walk_done(wok_u);
      return c3n;
    }

    got_w++;
    u3z(tac_u.job);
  }

  u3_disk_walk_done(wok_u);

  if ( got_w != len_w ) {
    fprintf(stderr, "disk-wasm: walk count mismatch: %u/%u\r\n", got_w, len_w);
    return c3n;
  }

  return c3y;
}

static c3_o
_probe_run_boot(u3_noun ova, u3_noun cax)
{
  u3m_hate(1 << 18);
  u3_noun xev = u3m_love(u3ke_cue(u3ke_jam(u3nc(cax, ova))));
  u3z(cax);
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

  u3l_log("disk-wasm: bootstrap starting");

  u3C.slog_f = _probe_slog;
  if ( c3n == u3v_boot(ova) ) {
    u3C.slog_f = 0;
    return c3n;
  }
  u3C.slog_f = 0;

  u3l_log("disk-wasm: bootstrap complete core=%x event=%" PRIu64,
          u3r_mug(u3A->roc), u3A->eve_d);
  return c3y;
}

static c3_o
_probe_commit_job(u3_disk* log_u,
                  u3_noun  job,
                  const c3_c* cap_c,
                  const c3_c* effects_c)
{
  c3_d pre_d = u3A->eve_d;
  u3_noun pro;

  if ( c3n == u3v_poke_sure(0, u3k(job), &pro) ) {
    fprintf(stderr, "disk-wasm: %s failed\r\n", cap_c);
    u3m_p("disk-wasm: poke dud", pro);
    u3z(pro);
    u3z(job);
    return c3n;
  }

  if ( u3A->eve_d != (pre_d + 1) ) {
    fprintf(stderr, "disk-wasm: %s event mismatch: before=%" PRIu64
                    " after=%" PRIu64 "\r\n",
                    cap_c, pre_d, u3A->eve_d);
    u3z(pro);
    u3z(job);
    return c3n;
  }

  {
    u3_fact tac_u = {
      .job   = job,
      .mug_l = u3r_mug(u3A->roc),
      .eve_d = u3A->eve_d
    };

    u3_disk_plan(log_u, &tac_u);
  }

  if ( log_u->dun_d != u3A->eve_d ) {
    fprintf(stderr, "disk-wasm: %s commit mismatch: disk=%" PRIu64
                    " arvo=%" PRIu64 "\r\n",
                    cap_c, log_u->dun_d, u3A->eve_d);
    u3z(pro);
    u3z(job);
    return c3n;
  }

  c3_w eff_w = 0;
  if ( c3n == _probe_list_len(pro, &eff_w) ) {
    fprintf(stderr, "disk-wasm: %s produced non-list effects\r\n", cap_c);
    u3z(pro);
    u3z(job);
    return c3n;
  }

  if ( effects_c ) {
    c3_d effects_len_d;
    if ( c3n == _probe_write_jam(effects_c, pro, &effects_len_d) ) {
      fprintf(stderr, "disk-wasm: %s effects write failed (%s)\r\n",
                      cap_c, effects_c);
      u3z(pro);
      u3z(job);
      return c3n;
    }

    u3l_log("disk-wasm: wrote effects %s bytes=%" PRIu64,
            effects_c, effects_len_d);
  }

  u3l_log("disk-wasm: %s committed event=%" PRIu64
          " core=%x effects=%u",
          cap_c, u3A->eve_d, u3r_mug(u3A->roc), eff_w);

  u3z(pro);
  u3z(job);
  return c3y;
}

static u3_weak
_probe_replay_event(const u3_fact* tac_u)
{
  u3_noun gon = u3m_soft(0, u3v_poke, tac_u->job);
  u3_noun tag, dat;
  u3x_cell(gon, &tag, &dat);

  if ( u3_blip != tag ) {
    return gon;
  }

  {
    u3_noun cor = u3t(dat);
    c3_l mug_l = u3r_mug(cor);

    if ( tac_u->mug_l && (tac_u->mug_l != mug_l) ) {
      fprintf(stderr, "disk-wasm: replay mug mismatch at event %" PRIu64
                      ": expected %08x actual %08x\r\n",
                      tac_u->eve_d, tac_u->mug_l, mug_l);
      u3z(gon);
      return u3nc(c3__awry, u3_nul);
    }

    u3z(u3A->roc);
    u3A->roc = u3k(cor);
    u3A->eve_d++;
  }

  u3z(gon);
  return u3_none;
}

static c3_o
_probe_replay_extra(u3_disk* log_u, c3_d eve_d, c3_d len_d)
{
  if ( 0 == len_d ) {
    return c3y;
  }

  u3_disk_walk* wok_u = u3_disk_walk_init(log_u, eve_d, len_d);
  c3_d got_d = 0;

  if ( !wok_u ) {
    fprintf(stderr, "disk-wasm: replay walk init failed\r\n");
    return c3n;
  }

  while ( c3y == u3_disk_walk_live(wok_u) ) {
    u3_fact tac_u;

    if ( c3n == u3_disk_walk_step(wok_u, &tac_u) ) {
      u3_disk_walk_done(wok_u);
      return c3n;
    }

    if ( (eve_d + got_d) != tac_u.eve_d ) {
      fprintf(stderr, "disk-wasm: replay event mismatch\r\n");
      u3z(tac_u.job);
      u3_disk_walk_done(wok_u);
      return c3n;
    }

    u3_weak dud = _probe_replay_event(&tac_u);

    if ( u3_none != dud ) {
      fprintf(stderr, "disk-wasm: replay failed at event %" PRIu64 "\r\n",
              tac_u.eve_d);
      u3m_p("disk-wasm: replay dud", dud);
      u3z(dud);
      u3_disk_walk_done(wok_u);
      return c3n;
    }

    got_d++;
  }

  u3_disk_walk_done(wok_u);

  if ( got_d != len_d ) {
    fprintf(stderr, "disk-wasm: replay count mismatch: %" PRIu64 "/%" PRIu64 "\r\n",
                    got_d, len_d);
    return c3n;
  }

  u3l_log("disk-wasm: replayed extra events=%" PRIu64 " final=%" PRIu64,
          got_d, u3A->eve_d);
  return c3y;
}

static c3_o
_probe_poke_load_mesa(u3_disk* log_u,
                      c3_d    boot_len_d,
                      const c3_c* effects_c)
{
  if ( u3A->eve_d != boot_len_d ) {
    fprintf(stderr, "disk-wasm: load-mesa requires boot event=%" PRIu64
                    ", have event=%" PRIu64 "\r\n",
                    boot_len_d, u3A->eve_d);
    return c3n;
  }
  if ( log_u->dun_d != boot_len_d ) {
    fprintf(stderr, "disk-wasm: load-mesa requires disk event=%" PRIu64
                    ", have event=%" PRIu64 "\r\n",
                    boot_len_d, log_u->dun_d);
    return c3n;
  }

  struct timeval tim_u;
  gettimeofday(&tim_u, 0);

  u3_noun now = u3m_time_in_tv(&tim_u);
  u3_noun wir = u3nc(c3__ames, u3_nul);
  u3_noun cad = u3nc(c3__load, c3__mesa);
  u3_noun ovo = u3nc(u3nc(c3__a, wir), cad);
  u3_noun job = u3nc(now, ovo);
  return _probe_commit_job(log_u, job, "runtime poke %load %mesa", effects_c);
}

static c3_o
_probe_poke_ovum(u3_disk* log_u,
                 const c3_c* ovum_c,
                 const c3_c* effects_c)
{
  u3_noun ovo;
  if ( c3n == _probe_read_jam(ovum_c, &ovo) ) {
    return c3n;
  }

  struct timeval tim_u;
  gettimeofday(&tim_u, 0);

  u3_noun now = u3m_time_in_tv(&tim_u);
  u3_noun job = u3nc(now, ovo);
  c3_c cap_c[256];

  snprintf(cap_c, sizeof(cap_c), "host ovum %s", ovum_c);
  return _probe_commit_job(log_u, job, cap_c, effects_c);
}

static void PROBE_NO_STACK_PROTECTOR
_probe_reactor_call_ctors(void)
{
#if defined(__wasm__)
  if ( c3n == _probe_reactor_ctors_o ) {
    __wasm_call_ctors();
    _probe_reactor_ctors_o = c3y;
  }
#endif
}

static const c3_c*
_probe_reactor_arg(c3_c* arg_c)
{
  return ('\0' == arg_c[0]) ? 0 : arg_c;
}

static c3_o
_probe_reactor_load(c3_w exp_w, c3_d who_d[2])
{
  u3_noun ova = u3_nul;
  u3_noun cax = u3_nul;
  u3_meta met_u = {0};
  c3_w len_w = 0;
  u3_disk* log_u = 0;
  u3_noun lova = u3_none;

  if (  (exp_w < 20)
     || (exp_w > 31) )
  {
    fprintf(stderr, "disk-wasm: invalid reactor loom exponent %u\r\n", exp_w);
    return c3n;
  }
  if ( c3y == _probe_reactor_live_o ) {
    fprintf(stderr, "disk-wasm: reactor already initialized\r\n");
    return c3n;
  }

  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite((size_t)1 << exp_w);

  if (  (c3n == _probe_make_boot(&ova, &cax, &met_u, who_d))
     || (c3n == _probe_list_len(ova, &len_w)) )
  {
    goto fail;
  }

  if ( c3n == u3fs_exists((c3_c*)PROBE_PIER_PATH) ) {
    if ( c3n == u3_disk_make((c3_c*)PROBE_PIER_PATH) ) {
      fprintf(stderr, "disk-wasm: reactor make failed\r\n");
      goto fail;
    }

    log_u = u3_disk_load((c3_c*)PROBE_PIER_PATH, u3_dlod_boot);
    if ( !log_u ) {
      fprintf(stderr, "disk-wasm: reactor boot load failed\r\n");
      goto fail;
    }

    if (  (c3n == u3_disk_save_meta_meta(log_u->com_u->pax_c, &met_u))
       || (c3n == u3_disk_save_meta(log_u->mdb_u, &met_u)) )
    {
      fprintf(stderr, "disk-wasm: reactor save meta failed\r\n");
      goto fail;
    }

    u3_disk_plan_list(log_u, u3k(ova));
    if ( c3n == u3_disk_sync(log_u) ) {
      fprintf(stderr, "disk-wasm: reactor sync failed\r\n");
      goto fail;
    }

    u3l_log("disk-wasm: reactor saved events=%u committed=%" PRIu64,
            len_w, log_u->dun_d);
    u3_disk_exit(log_u);
    log_u = 0;
  }

  log_u = u3_disk_load((c3_c*)PROBE_PIER_PATH, u3_dlod_last);
  if ( !log_u ) {
    fprintf(stderr, "disk-wasm: reactor reload failed\r\n");
    goto fail;
  }

  {
    u3_meta rem_u = {0};
    if (  (c3n == u3_disk_read_meta(log_u->mdb_u, &rem_u))
       || (c3n == _probe_meta_match(&met_u, &rem_u)) )
    {
      fprintf(stderr, "disk-wasm: reactor reload meta mismatch\r\n");
      goto fail;
    }
  }

  {
    c3_l mug_l = 0;
    lova = u3_disk_read_list(log_u, 1, len_w, &mug_l);
  }
  if ( u3_none == lova ) {
    fprintf(stderr, "disk-wasm: reactor read list failed\r\n");
    goto fail;
  }
  if ( c3n == u3r_sing(ova, lova) ) {
    fprintf(stderr, "disk-wasm: reactor read list mismatch\r\n");
    goto fail;
  }
  if ( c3n == _probe_walk_count(log_u, len_w) ) {
    goto fail;
  }

  u3l_log("disk-wasm: reactor loaded events=%u committed=%" PRIu64,
          len_w, log_u->dun_d);

  u3z(ova);
  ova = u3_nul;

  if ( c3n == _probe_run_boot(lova, cax) ) {
    fprintf(stderr, "disk-wasm: reactor boot from log failed\r\n");
    lova = u3_none;
    cax = u3_nul;
    goto fail;
  }
  lova = u3_none;
  cax = u3_nul;

  if ( log_u->dun_d > len_w ) {
    if ( c3n == _probe_replay_extra(log_u,
                                    1ULL + len_w,
                                    log_u->dun_d - len_w) )
    {
      fprintf(stderr, "disk-wasm: reactor replay extra events failed\r\n");
      goto fail;
    }
  }

  _probe_reactor_log_u = log_u;
  _probe_reactor_boot_len_d = len_w;
  _probe_reactor_live_o = c3y;
  u3z(ova);
  return c3y;

fail:
  if ( log_u ) {
    u3_disk_exit(log_u);
  }
  if ( u3_none != lova ) {
    u3z(lova);
  }
  if ( u3_nul != ova ) {
    u3z(ova);
  }
  if ( u3_nul != cax ) {
    u3z(cax);
  }
  return c3n;
}

PROBE_EXPORT("u3_disk_wasm_arg_bytes")
c3_w
u3_disk_wasm_arg_bytes(void)
{
  return PROBE_REACTOR_ARG_BYTES;
}

PROBE_EXPORT("u3_disk_wasm_arg0_ptr")
c3_w
u3_disk_wasm_arg0_ptr(void)
{
  return (c3_w)(uintptr_t)_probe_reactor_arg0_c;
}

PROBE_EXPORT("u3_disk_wasm_arg1_ptr")
c3_w
u3_disk_wasm_arg1_ptr(void)
{
  return (c3_w)(uintptr_t)_probe_reactor_arg1_c;
}

PROBE_EXPORT("u3_disk_wasm_init")
c3_i PROBE_NO_STACK_PROTECTOR
u3_disk_wasm_init(c3_w exp_w, c3_d who_l, c3_d who_h)
{
  c3_d who_d[2] = { who_l, who_h };
  _probe_reactor_call_ctors();
  return (c3y == _probe_reactor_load(exp_w, who_d)) ? 0 : 1;
}

PROBE_EXPORT("u3_disk_wasm_poke_load_mesa")
c3_i
u3_disk_wasm_poke_load_mesa(void)
{
  if ( c3n == _probe_reactor_live_o ) {
    fprintf(stderr, "disk-wasm: reactor not initialized\r\n");
    return 1;
  }

  return (c3y == _probe_poke_load_mesa(
                    _probe_reactor_log_u,
                    _probe_reactor_boot_len_d,
                    _probe_reactor_arg(_probe_reactor_arg0_c)) )
       ? 0
       : 1;
}

PROBE_EXPORT("u3_disk_wasm_poke_ovum")
c3_i
u3_disk_wasm_poke_ovum(void)
{
  if ( c3n == _probe_reactor_live_o ) {
    fprintf(stderr, "disk-wasm: reactor not initialized\r\n");
    return 1;
  }

  return (c3y == _probe_poke_ovum(
                    _probe_reactor_log_u,
                    _probe_reactor_arg(_probe_reactor_arg0_c),
                    _probe_reactor_arg(_probe_reactor_arg1_c)) )
       ? 0
       : 1;
}

PROBE_EXPORT("u3_disk_wasm_event")
c3_d
u3_disk_wasm_event(void)
{
  return (c3y == _probe_reactor_live_o) ? u3A->eve_d : 0;
}

PROBE_EXPORT("u3_disk_wasm_shutdown")
c3_i
u3_disk_wasm_shutdown(void)
{
  if ( c3y == _probe_reactor_live_o ) {
    u3_disk_exit(_probe_reactor_log_u);
    _probe_reactor_log_u = 0;
    _probe_reactor_live_o = c3n;
  }

  u3m_stop();
  return 0;
}

int
main(int argc, char** argv)
{
  c3_w exp_w;
  if ( c3n == _probe_loom_exp(argc, argv, &exp_w) ) {
    return 1;
  }
  c3_d who_d[2];
  if ( c3n == _probe_fake_ship(argc, argv, who_d) ) {
    return 1;
  }
  c3_o run_o = _probe_has_arg(argc, argv, "--run-boot");
  c3_o load_only_o = _probe_has_arg(argc, argv, "--load-only");
  c3_o poke_load_mesa_o = _probe_has_arg(argc, argv, "--poke-load-mesa");
  _probe_host_poke pok_u[PROBE_MAX_HOST_POKES] = {0};
  c3_w pok_w = 0;
  const c3_c* effects_c;

  if (  (c3n == _probe_parse_host_pokes(argc, argv, pok_u, &pok_w))
     || (c3n == _probe_arg_value(argc, argv, "--effects-out", &effects_c)) )
  {
    return 1;
  }

  if (  (c3y == poke_load_mesa_o)
     && (0 != pok_w) )
  {
    fprintf(stderr, "disk-wasm: choose either --poke-load-mesa or --poke-ovum\r\n");
    return 1;
  }
  if (  (0 != effects_c)
     && (c3n == poke_load_mesa_o)
     && (0 == pok_w) )
  {
    fprintf(stderr, "disk-wasm: --effects-out requires a poke\r\n");
    return 1;
  }

  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite((size_t)1 << exp_w);

  u3_noun ova = u3_nul;
  u3_noun cax = u3_nul;
  u3_meta met_u = {0};
  c3_w len_w = 0;

  if (  (c3n == _probe_make_boot(&ova, &cax, &met_u, who_d))
     || (c3n == _probe_list_len(ova, &len_w)) )
  {
    return 1;
  }

  u3_disk* log_u;

  if ( c3n == load_only_o ) {
    if ( c3n == u3_disk_make((c3_c*)PROBE_PIER_PATH) ) {
      fprintf(stderr, "disk-wasm: make failed\r\n");
      return 1;
    }

    log_u = u3_disk_load((c3_c*)PROBE_PIER_PATH, u3_dlod_boot);
    if ( !log_u ) {
      fprintf(stderr, "disk-wasm: boot load failed\r\n");
      return 1;
    }

    if (  (c3n == u3_disk_save_meta_meta(log_u->com_u->pax_c, &met_u))
       || (c3n == u3_disk_save_meta(log_u->mdb_u, &met_u)) )
    {
      fprintf(stderr, "disk-wasm: save meta failed\r\n");
      return 1;
    }

    u3_disk_plan_list(log_u, u3k(ova));

    if ( c3n == u3_disk_sync(log_u) ) {
      fprintf(stderr, "disk-wasm: sync failed\r\n");
      return 1;
    }

    u3l_log("disk-wasm: saved events=%u committed=%" PRIu64,
            len_w, log_u->dun_d);
    u3_disk_exit(log_u);
  }

  log_u = u3_disk_load((c3_c*)PROBE_PIER_PATH, u3_dlod_last);
  if ( !log_u ) {
    fprintf(stderr, "disk-wasm: reload failed\r\n");
    return 1;
  }

  u3_meta rem_u = {0};
  if (  (c3n == u3_disk_read_meta(log_u->mdb_u, &rem_u))
     || (c3n == _probe_meta_match(&met_u, &rem_u)) )
  {
    fprintf(stderr, "disk-wasm: reload meta mismatch\r\n");
    return 1;
  }

  c3_l mug_l = 0;
  u3_weak lova = u3_disk_read_list(log_u, 1, len_w, &mug_l);
  if ( u3_none == lova ) {
    fprintf(stderr, "disk-wasm: read list failed\r\n");
    return 1;
  }
  if ( c3n == u3r_sing(ova, lova) ) {
    fprintf(stderr, "disk-wasm: read list mismatch\r\n");
    u3z(lova);
    return 1;
  }

  if ( c3n == _probe_walk_count(log_u, len_w) ) {
    u3z(lova);
    return 1;
  }

  u3l_log("disk-wasm: %s events=%u committed=%" PRIu64,
          (c3y == load_only_o) ? "loaded existing" : "reloaded",
          len_w, log_u->dun_d);

  if ( c3y == run_o ) {
    u3z(ova);
    ova = u3_nul;

    if ( c3n == _probe_run_boot(lova, cax) ) {
      fprintf(stderr, "disk-wasm: boot from log failed\r\n");
      return 1;
    }
    if ( log_u->dun_d > len_w ) {
      if ( c3n == _probe_replay_extra(log_u,
                                      1ULL + len_w,
                                      log_u->dun_d - len_w) )
      {
        fprintf(stderr, "disk-wasm: replay extra events failed\r\n");
        return 1;
      }
    }
    if ( c3y == poke_load_mesa_o ) {
      if ( c3n == _probe_poke_load_mesa(log_u, len_w, effects_c) ) {
        return 1;
      }
    }
    else {
      for ( c3_w i_w = 0; i_w < pok_w; i_w++ ) {
        if ( c3n == _probe_poke_ovum(log_u,
                                     pok_u[i_w].ovum_c,
                                     pok_u[i_w].effects_c) )
        {
          return 1;
        }
      }
    }
  }
  else {
    if ( (c3y == poke_load_mesa_o) || pok_w ) {
      fprintf(stderr, "disk-wasm: pokes require --run-boot\r\n");
      u3z(lova);
      return 1;
    }
    u3z(lova);
  }

  u3z(ova);
  if ( c3y != run_o ) {
    u3z(cax);
  }
  u3_disk_exit(log_u);
  u3m_stop();
  return 0;
}
