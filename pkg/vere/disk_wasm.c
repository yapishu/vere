/// @file
///
/// WASM hostfs-backed event log backend for the u3_disk API.

#include "vere.h"

#include "hostfs.h"
#include "version.h"

#include <stdint.h>

#define DISK_WASM_EVENT_PATH  "0i0/events.bin"
#define DISK_WASM_META_PATH   "meta.bin"
#define DISK_WASM_VERSION     1

static const c3_y _disk_event_magic[8] = {
  'u', '3', 'd', 'i', 's', 'k', 'w', 1
};

static const c3_y _disk_meta_magic[8] = {
  'u', '3', 'm', 'e', 't', 'a', 'w', 1
};

struct _u3_disk_walk {
  u3_disk* log_u;
  c3_d     nex_d;
  c3_d     las_d;
  c3_o     liv_o;
};

static void
_disk_put32(c3_y* buf_y, c3_w val_w)
{
  buf_y[0] = val_w & 0xff;
  buf_y[1] = (val_w >> 8) & 0xff;
  buf_y[2] = (val_w >> 16) & 0xff;
  buf_y[3] = (val_w >> 24) & 0xff;
}

static c3_w
_disk_get32(const c3_y* buf_y)
{
  return (c3_w)buf_y[0]
       | ((c3_w)buf_y[1] << 8)
       | ((c3_w)buf_y[2] << 16)
       | ((c3_w)buf_y[3] << 24);
}

static void
_disk_put64(c3_y* buf_y, c3_d val_d)
{
  for ( c3_w i_w = 0; i_w < 8; i_w++ ) {
    buf_y[i_w] = (val_d >> (8 * i_w)) & 0xff;
  }
}

static c3_d
_disk_get64(const c3_y* buf_y)
{
  c3_d val_d = 0;

  for ( c3_w i_w = 0; i_w < 8; i_w++ ) {
    val_d |= ((c3_d)buf_y[i_w]) << (8 * i_w);
  }

  return val_d;
}

static c3_c*
_disk_join(c3_c* dir_c, const c3_c* suf_c)
{
  c3_w len_w = strlen(dir_c) + 1 + strlen(suf_c) + 1;
  c3_c* pax_c = c3_malloc(len_w);
  snprintf(pax_c, len_w, "%s/%s", dir_c, suf_c);
  return pax_c;
}

static c3_c*
_disk_event_path(u3_disk* log_u)
{
  return _disk_join(log_u->com_u->pax_c, DISK_WASM_EVENT_PATH);
}

static c3_c*
_disk_meta_path(c3_c* log_c)
{
  return _disk_join(log_c, DISK_WASM_META_PATH);
}

static c3_o
_disk_ensure_log_dirs(c3_c* pax_c)
{
  c3_c* urb_c = _disk_join(pax_c, ".urb");
  c3_c* put_c = _disk_join(urb_c, "put");
  c3_c* get_c = _disk_join(urb_c, "get");
  c3_c* log_c = _disk_join(urb_c, "log");
  c3_c* epo_c = _disk_join(log_c, "0i0");

  c3_o ret_o = (  (c3y == u3fs_ensure_dir("disk", urb_c, 0700))
               && (c3y == u3fs_ensure_dir("disk", put_c, 0700))
               && (c3y == u3fs_ensure_dir("disk", get_c, 0700))
               && (c3y == u3fs_ensure_dir("disk", log_c, 0700))
               && (c3y == u3fs_ensure_dir("disk", epo_c, 0700)) )
             ? c3y
             : c3n;

  c3_free(urb_c);
  c3_free(put_c);
  c3_free(get_c);
  c3_free(log_c);
  c3_free(epo_c);

  return ret_o;
}

static c3_o
_disk_empty_events(c3_c* pat_c)
{
  c3_y* buf_y;

  if ( c3n == u3fs_mmap("disk: events", pat_c, 20, &buf_y) ) {
    return c3n;
  }

  memcpy(buf_y, _disk_event_magic, sizeof(_disk_event_magic));
  _disk_put32(buf_y + 8, DISK_WASM_VERSION);
  _disk_put64(buf_y + 12, 0);

  c3_o sav_o = u3fs_mmap_save("disk: events", pat_c, 20, buf_y);
  c3_o map_o = u3fs_munmap(20, buf_y);

  return ( (c3y == sav_o) && (c3y == map_o) ) ? c3y : c3n;
}

static c3_o
_disk_load_events(u3_disk* log_u, c3_d* out_cnt_d, c3_d* out_len_d, c3_y** out_y)
{
  c3_c* pat_c = _disk_event_path(log_u);
  c3_o ret_o = u3fs_mmap_read("disk: events", pat_c, out_len_d, out_y);
  c3_free(pat_c);

  if ( c3n == ret_o ) {
    return c3n;
  }

  if (  (*out_len_d < 20)
     || (0 != memcmp(*out_y, _disk_event_magic, sizeof(_disk_event_magic)))
     || (DISK_WASM_VERSION != _disk_get32(*out_y + 8)) )
  {
    fprintf(stderr, "disk: wasm event log header invalid\r\n");
    u3fs_munmap(*out_len_d, *out_y);
    *out_len_d = 0;
    *out_y = 0;
    return c3n;
  }

  *out_cnt_d = _disk_get64(*out_y + 12);
  return c3y;
}

static c3_o
_disk_find_event(u3_disk* log_u,
                 c3_d     eve_d,
                 c3_d*    out_len_d,
                 c3_y**   out_y)
{
  c3_d len_d, cnt_d;
  c3_y* buf_y;

  if ( c3n == _disk_load_events(log_u, &cnt_d, &len_d, &buf_y) ) {
    return c3n;
  }

  c3_o ret_o = c3n;
  c3_d off_d = 20;

  for ( c3_d i_d = 0; i_d < cnt_d; i_d++ ) {
    if ( (off_d + 12) > len_d ) {
      fprintf(stderr, "disk: wasm event log truncated index\r\n");
      goto done;
    }

    c3_d hav_d = _disk_get64(buf_y + off_d);
    c3_w rec_w = _disk_get32(buf_y + off_d + 8);
    off_d += 12;

    if (  (4 >= rec_w)
       || ((off_d + rec_w) > len_d) )
    {
      fprintf(stderr, "disk: wasm event log truncated record\r\n");
      goto done;
    }

    if ( eve_d == hav_d ) {
      c3_y* dat_y = c3_malloc(rec_w);
      memcpy(dat_y, buf_y + off_d, rec_w);
      *out_len_d = rec_w;
      *out_y = dat_y;
      ret_o = c3y;
      goto done;
    }

    off_d += rec_w;
  }

done:
  u3fs_munmap(len_d, buf_y);
  return ret_o;
}

static c3_o
_disk_commit_batch(u3_disk* log_u)
{
  c3_d old_len_d, old_cnt_d;
  c3_y* old_y;

  if ( c3n == _disk_load_events(log_u, &old_cnt_d, &old_len_d, &old_y) ) {
    return c3n;
  }

  if ( old_cnt_d != (log_u->sav_u.eve_d - 1) ) {
    fprintf(stderr, "disk: wasm event gap: have %" PRIu64 ", saving %" PRIu64 "\r\n",
                    old_cnt_d, log_u->sav_u.eve_d);
    u3fs_munmap(old_len_d, old_y);
    return c3n;
  }

  c3_d add_d = 0;
  for ( c3_w i_w = 0; i_w < log_u->sav_u.len_w; i_w++ ) {
    add_d += 12 + log_u->sav_u.siz_i[i_w];
  }

  c3_c* pat_c = _disk_event_path(log_u);
  c3_y* new_y;
  if ( c3n == u3fs_mmap("disk: events", pat_c, old_len_d + add_d, &new_y) ) {
    c3_free(pat_c);
    u3fs_munmap(old_len_d, old_y);
    return c3n;
  }

  memcpy(new_y, old_y, old_len_d);
  _disk_put64(new_y + 12, old_cnt_d + log_u->sav_u.len_w);

  c3_d off_d = old_len_d;
  for ( c3_w i_w = 0; i_w < log_u->sav_u.len_w; i_w++ ) {
    c3_d eve_d = log_u->sav_u.eve_d + i_w;
    size_t siz_i = log_u->sav_u.siz_i[i_w];

    if ( siz_i > UINT32_MAX ) {
      fprintf(stderr, "disk: wasm event too large\r\n");
      c3_free(pat_c);
      u3fs_munmap(old_len_d, old_y);
      u3fs_munmap(old_len_d + add_d, new_y);
      return c3n;
    }

    _disk_put64(new_y + off_d, eve_d);
    _disk_put32(new_y + off_d + 8, (c3_w)siz_i);
    memcpy(new_y + off_d + 12, log_u->sav_u.byt_y[i_w], siz_i);
    off_d += 12 + siz_i;
  }

  c3_o ret_o = u3fs_mmap_save("disk: events", pat_c, old_len_d + add_d, new_y);

  c3_free(pat_c);
  u3fs_munmap(old_len_d, old_y);
  u3fs_munmap(old_len_d + add_d, new_y);

  return ret_o;
}

static c3_o
_disk_batch(u3_disk* log_u)
{
  u3_feat* fet_u = log_u->put_u.ext_u;
  c3_w     len_w = log_u->sen_d - log_u->dun_d;

  if ( !len_w || (c3y == log_u->sav_u.ted_o) ) {
    return c3n;
  }

  len_w = c3_min(len_w, 100);

  u3_assert( fet_u );
  u3_assert( (1ULL + log_u->dun_d) == fet_u->eve_d );

  log_u->sav_u.ret_o = c3n;
  log_u->sav_u.eve_d = fet_u->eve_d;
  log_u->sav_u.len_w = len_w;

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    u3_assert( fet_u );
    u3_assert( (log_u->sav_u.eve_d + i_w) == fet_u->eve_d );

    log_u->sav_u.byt_y[i_w] = fet_u->hun_y;
    log_u->sav_u.siz_i[i_w] = fet_u->len_i;

    fet_u = fet_u->nex_u;
  }

  log_u->hit_w[len_w]++;
  return c3y;
}

static void
_disk_commit_done(u3_disk* log_u)
{
  c3_d eve_d = log_u->sav_u.eve_d;
  c3_w len_w = log_u->sav_u.len_w;
  c3_o ret_o = log_u->sav_u.ret_o;

  if ( c3y == ret_o ) {
    log_u->dun_d += len_w;
  }

  if ( log_u->sav_u.don_f ) {
    log_u->sav_u.don_f(log_u->sav_u.ptr_v, eve_d + (len_w - 1), ret_o);
  }

  {
    u3_feat* fet_u = log_u->put_u.ext_u;

    while ( fet_u && (fet_u->eve_d <= log_u->dun_d) ) {
      log_u->put_u.ext_u = fet_u->nex_u;
      c3_free(fet_u->hun_y);
      c3_free(fet_u);
      fet_u = log_u->put_u.ext_u;
    }
  }

  if ( !log_u->put_u.ext_u ) {
    log_u->put_u.ent_u = 0;
  }
}

static void
_disk_plan(u3_disk* log_u, c3_l mug_l, u3_noun job)
{
  u3_feat* fet_u = c3_malloc(sizeof(*fet_u));
  fet_u->eve_d = ++log_u->sen_d;
  fet_u->len_i = u3_disk_etch(log_u, job, mug_l, &fet_u->hun_y);
  fet_u->nex_u = 0;

  if ( !log_u->put_u.ent_u ) {
    u3_assert( !log_u->put_u.ext_u );
    log_u->put_u.ent_u = log_u->put_u.ext_u = fet_u;
  }
  else {
    log_u->put_u.ent_u->nex_u = fet_u;
    log_u->put_u.ent_u = fet_u;
  }
}

static c3_o
_disk_read_count(u3_disk* log_u, c3_d* cnt_d)
{
  c3_d len_d;
  c3_y* buf_y;
  c3_o ret_o = _disk_load_events(log_u, cnt_d, &len_d, &buf_y);

  if ( c3y == ret_o ) {
    u3fs_munmap(len_d, buf_y);
  }

  return ret_o;
}

static u3_noun
_disk_mass(u3_atom cod, u3_noun lit)
{
  return u3nt(cod, c3n, lit);
}

static u3_noun
_disk_mase(c3_c* cod_c, u3_noun dat)
{
  return u3nt(u3i_string(cod_c), c3y, dat);
}

size_t
u3_disk_etch(u3_disk* log_u, u3_noun eve, c3_l mug_l, c3_y** out_y)
{
  (void)log_u;

  u3_atom mat = u3qe_jam(eve);
  c3_w len_w = u3r_met(3, mat);
  size_t len_i = 4 + len_w;
  c3_y* dat_y = c3_malloc(len_i);

  dat_y[0] = mug_l & 0xff;
  dat_y[1] = (mug_l >> 8) & 0xff;
  dat_y[2] = (mug_l >> 16) & 0xff;
  dat_y[3] = (mug_l >> 24) & 0xff;
  u3r_bytes(0, len_w, dat_y + 4, mat);

  u3z(mat);
  *out_y = dat_y;
  return len_i;
}

c3_o
u3_disk_sift(u3_disk* log_u,
             size_t   len_i,
             c3_y*    dat_y,
             c3_l*    mug_l,
             u3_noun* job)
{
  (void)log_u;

  if ( 4 >= len_i ) {
    return c3n;
  }

  *mug_l = dat_y[0]
         ^ (dat_y[1] << 8)
         ^ (dat_y[2] << 16)
         ^ (dat_y[3] << 24);
  *job = u3ke_cue(u3i_bytes(len_i - 4, dat_y + 4));

  return c3y;
}

void
u3_disk_plan(u3_disk* log_u, u3_fact* tac_u)
{
  if ( u3C.wag_w & u3o_dryrun ) {
    log_u->sen_d++;
    log_u->dun_d++;
    return;
  }

  u3_assert( (1ULL + log_u->sen_d) == tac_u->eve_d );

  _disk_plan(log_u, tac_u->mug_l, tac_u->job);
  u3_disk_sync(log_u);
}

void
u3_disk_plan_list(u3_disk* log_u, u3_noun lit)
{
  u3_noun i, t = lit;

  while ( u3_nul != t ) {
    u3x_cell(t, &i, &t);
    _disk_plan(log_u, 0, i);
  }

  u3z(lit);
}

c3_o
u3_disk_sync(u3_disk* log_u)
{
  c3_o ret_o = c3n;

  while ( c3y == _disk_batch(log_u) ) {
    ret_o = _disk_commit_batch(log_u);
    log_u->sav_u.ret_o = ret_o;
    _disk_commit_done(log_u);

    if ( c3n == ret_o ) {
      return c3n;
    }
  }

  return ret_o;
}

void
u3_disk_async(u3_disk* log_u, void* ptr_v, u3_disk_news don_f)
{
  log_u->sav_u.ptr_v = ptr_v;
  log_u->sav_u.don_f = don_f;
}

u3_weak
u3_disk_read_list(u3_disk* log_u, c3_d eve_d, c3_d len_d, c3_l* mug_l)
{
  u3_noun eve = u3_nul;

  for ( c3_d i_d = 0; i_d < len_d; i_d++ ) {
    c3_d dat_d;
    c3_y* dat_y;
    c3_l mug;
    u3_noun job;

    if ( c3n == _disk_find_event(log_u, eve_d + i_d, &dat_d, &dat_y) ) {
      u3z(eve);
      return u3_none;
    }
    if ( c3n == u3_disk_sift(log_u, dat_d, dat_y, &mug, &job) ) {
      c3_free(dat_y);
      u3z(eve);
      return u3_none;
    }

    *mug_l = mug;
    eve = u3nc(job, eve);
    c3_free(dat_y);
  }

  return u3kb_flop(eve);
}

u3_disk_walk*
u3_disk_walk_init(u3_disk* log_u, c3_d eve_d, c3_d len_d)
{
  u3_disk_walk* wok_u = c3_malloc(sizeof(*wok_u));
  c3_d las_d = eve_d + len_d - 1;

  wok_u->log_u = log_u;
  wok_u->nex_d = eve_d;
  wok_u->las_d = c3_min(las_d, log_u->dun_d);
  wok_u->liv_o = (wok_u->nex_d <= wok_u->las_d) ? c3y : c3n;
  return wok_u;
}

c3_o
u3_disk_walk_live(u3_disk_walk* wok_u)
{
  if ( wok_u->nex_d > wok_u->las_d ) {
    wok_u->liv_o = c3n;
  }

  return wok_u->liv_o;
}

c3_o
u3_disk_walk_step(u3_disk_walk* wok_u, u3_fact* tac_u)
{
  c3_d dat_d;
  c3_y* dat_y;

  tac_u->eve_d = wok_u->nex_d;

  if ( c3n == _disk_find_event(wok_u->log_u, tac_u->eve_d, &dat_d, &dat_y) ) {
    fprintf(stderr, "disk: (%" PRIu64 "): wasm read fail\r\n", tac_u->eve_d);
    return wok_u->liv_o = c3n;
  }

  if ( c3n == u3_disk_sift(wok_u->log_u,
                           dat_d,
                           dat_y,
                           &tac_u->mug_l,
                           &tac_u->job) )
  {
    c3_free(dat_y);
    fprintf(stderr, "disk: (%" PRIu64 "): wasm sift fail\r\n", tac_u->eve_d);
    return wok_u->liv_o = c3n;
  }

  c3_free(dat_y);
  wok_u->nex_d++;
  return c3y;
}

void
u3_disk_walk_done(u3_disk_walk* wok_u)
{
  c3_free(wok_u);
}

c3_o
u3_disk_save_meta(MDB_env* mdb_u, const u3_meta* met_u)
{
  u3_disk* log_u = (u3_disk*)mdb_u;
  c3_c* pat_c = _disk_meta_path(log_u->com_u->pax_c);
  c3_y buf_y[33];

  memcpy(buf_y, _disk_meta_magic, sizeof(_disk_meta_magic));
  _disk_put32(buf_y + 8, DISK_WASM_VERSION);
  _disk_put32(buf_y + 12, met_u->ver_w);
  _disk_put64(buf_y + 16, met_u->who_d[0]);
  _disk_put64(buf_y + 24, met_u->who_d[1]);
  buf_y[32] = met_u->fak_o;

  c3_y* out_y;
  if ( c3n == u3fs_mmap("disk: meta", pat_c, 37, &out_y) ) {
    c3_free(pat_c);
    return c3n;
  }

  memcpy(out_y, buf_y, 33);
  _disk_put32(out_y + 33, met_u->lif_w);

  c3_o ret_o = u3fs_mmap_save("disk: meta", pat_c, 37, out_y);
  u3fs_munmap(37, out_y);
  c3_free(pat_c);

  return ret_o;
}

c3_o
u3_disk_save_meta_meta(c3_c* log_c, const u3_meta* met_u)
{
  u3_disk log_u = {0};
  u3_dire dir_u = {0};

  dir_u.pax_c = log_c;
  log_u.com_u = &dir_u;

  return u3_disk_save_meta((MDB_env*)&log_u, met_u);
}

c3_o
u3_disk_read_meta(MDB_env* mdb_u, u3_meta* met_u)
{
  u3_disk* log_u = (u3_disk*)mdb_u;
  c3_c* pat_c = _disk_meta_path(log_u->com_u->pax_c);
  c3_d len_d;
  c3_y* buf_y;

  if ( c3n == u3fs_mmap_read("disk: meta", pat_c, &len_d, &buf_y) ) {
    c3_free(pat_c);
    return c3n;
  }
  c3_free(pat_c);

  if (  (37 != len_d)
     || (0 != memcmp(buf_y, _disk_meta_magic, sizeof(_disk_meta_magic)))
     || (DISK_WASM_VERSION != _disk_get32(buf_y + 8)) )
  {
    fprintf(stderr, "disk: wasm metadata invalid\r\n");
    u3fs_munmap(len_d, buf_y);
    return c3n;
  }

  if ( met_u ) {
    met_u->ver_w = _disk_get32(buf_y + 12);
    met_u->who_d[0] = _disk_get64(buf_y + 16);
    met_u->who_d[1] = _disk_get64(buf_y + 24);
    met_u->fak_o = buf_y[32];
    met_u->lif_w = _disk_get32(buf_y + 33);
  }

  u3fs_munmap(len_d, buf_y);
  return c3y;
}

c3_o
u3_disk_make(c3_c* pax_c)
{
  if ( c3n == u3fs_mkdir("disk", pax_c, 0700) ) {
    return c3n;
  }
  if ( c3n == _disk_ensure_log_dirs(pax_c) ) {
    return c3n;
  }

  c3_c* urb_c = _disk_join(pax_c, ".urb");
  c3_c* log_c = _disk_join(urb_c, "log");
  c3_c* pat_c = _disk_join(log_c, DISK_WASM_EVENT_PATH);
  c3_o ret_o = _disk_empty_events(pat_c);

  c3_free(urb_c);
  c3_free(log_c);
  c3_free(pat_c);

  return ret_o;
}

static u3_disk*
_disk_alloc(c3_c* pax_c)
{
  u3_disk* log_u = c3_calloc(sizeof(*log_u));
  c3_c* urb_c = _disk_join(pax_c, ".urb");
  c3_c* log_c = _disk_join(urb_c, "log");

  log_u->lok_i = -1;
  log_u->liv_o = c3n;
  log_u->ver_w = U3D_VERLAT;
  log_u->sav_u.ted_o = c3n;
  log_u->sav_u.ted_u.data = log_u;
  log_u->dir_u = u3_dire_init(pax_c);
  log_u->urb_u = u3_dire_init(urb_c);
  log_u->com_u = u3_dire_init(log_c);
  log_u->mdb_u = (MDB_env*)log_u;

  c3_free(urb_c);
  c3_free(log_c);

  return log_u;
}

u3_disk*
u3_disk_load(c3_c* pax_c, u3_disk_load_e lod_e)
{
  (void)lod_e;

  if ( c3n == u3fs_exists(pax_c) ) {
    return 0;
  }
  if ( c3n == _disk_ensure_log_dirs(pax_c) ) {
    return 0;
  }

  u3_disk* log_u = _disk_alloc(pax_c);
  c3_d cnt_d;
  c3_c* pat_c = _disk_event_path(log_u);

  if ( c3n == u3fs_exists(pat_c) ) {
    if ( c3n == _disk_empty_events(pat_c) ) {
      c3_free(pat_c);
      u3_disk_exit(log_u);
      return 0;
    }
  }
  c3_free(pat_c);

  if ( c3n == _disk_read_count(log_u, &cnt_d) ) {
    u3_disk_exit(log_u);
    return 0;
  }

  log_u->sen_d = log_u->dun_d = cnt_d;
  log_u->epo_d = 0;
  log_u->liv_o = c3y;

  return log_u;
}

void
u3_disk_exit(u3_disk* log_u)
{
  u3_feat* fet_u = log_u->put_u.ext_u;

  while ( fet_u ) {
    u3_feat* nex_u = fet_u->nex_u;
    c3_free(fet_u->hun_y);
    c3_free(fet_u);
    fet_u = nex_u;
  }

  u3_dire_free(log_u->dir_u);
  u3_dire_free(log_u->urb_u);
  u3_dire_free(log_u->com_u);
  c3_free(log_u);
}

u3_noun
u3_disk_info(u3_disk* log_u)
{
  u3_noun lit = u3i_list(
    _disk_mase("live",        log_u->liv_o),
    _disk_mase("event", u3i_chub(log_u->dun_d)),
    u3_none);

  if ( log_u->put_u.ext_u ) {
    lit = u3nc(
      _disk_mass(
        c3__save,
        u3i_list(
          _disk_mase("save-start", u3i_chub(log_u->put_u.ext_u->eve_d)),
          _disk_mase("save-final", u3i_chub(log_u->put_u.ent_u->eve_d)),
          u3_none)),
      lit);
  }

  return _disk_mass(c3__disk, lit);
}

void
u3_disk_slog(u3_disk* log_u)
{
  u3l_log("  disk: wasm live=%s, event=%" PRIu64,
          (c3y == log_u->liv_o) ? "&" : "|",
          log_u->dun_d);
}

c3_o
u3_disk_epoc_last(u3_disk* log_u, c3_d* lat_d)
{
  (void)log_u;
  *lat_d = 0;
  return c3y;
}

c3_z
u3_disk_epoc_list(u3_disk* log_u, c3_d* sot_d)
{
  (void)log_u;
  if ( sot_d ) {
    sot_d[0] = 0;
  }
  return 1;
}

void
u3_disk_chop(u3_disk* log_u, c3_d eve_d)
{
  (void)log_u;
  (void)eve_d;
  u3l_log("disk: wasm chop unsupported");
}

void
u3_disk_roll(u3_disk* log_u, c3_d eve_d)
{
  (void)log_u;
  (void)eve_d;
  u3l_log("disk: wasm roll unsupported");
}
