/// @file

#include "hostfs.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifndef U3_OS_wasm
# include <fcntl.h>
# include <sys/stat.h>
# ifndef U3_OS_windows
#  include <sys/mman.h>
# endif
# include <unistd.h>
#endif

#ifdef U3_OS_wasm
  __attribute__((import_module("env"), import_name("u3_wasm_file_mkdir")))
  extern c3_i
  u3_wasm_file_mkdir(const c3_c* pat_c, c3_w mod_w);

  __attribute__((import_module("env"), import_name("u3_wasm_file_exists")))
  extern c3_i
  u3_wasm_file_exists(const c3_c* pat_c);

  __attribute__((import_module("env"), import_name("u3_wasm_file_open")))
  extern c3_i
  u3_wasm_file_open(const c3_c* pat_c, c3_w flg_w, c3_w mod_w);

  __attribute__((import_module("env"), import_name("u3_wasm_file_close")))
  extern c3_i
  u3_wasm_file_close(c3_i fil_i);

  __attribute__((import_module("env"), import_name("u3_wasm_file_handle_size")))
  extern c3_ds
  u3_wasm_file_handle_size(c3_i fil_i);

  __attribute__((import_module("env"), import_name("u3_wasm_file_resize")))
  extern c3_i
  u3_wasm_file_resize(c3_i fil_i, c3_d len_d);

  __attribute__((import_module("env"), import_name("u3_wasm_file_read_at")))
  extern c3_i
  u3_wasm_file_read_at(c3_i fil_i, c3_d off_d, c3_y* buf_y, c3_d len_d);

  __attribute__((import_module("env"), import_name("u3_wasm_file_write_at")))
  extern c3_i
  u3_wasm_file_write_at(c3_i fil_i, c3_d off_d, const c3_y* buf_y, c3_d len_d);

  __attribute__((import_module("env"), import_name("u3_wasm_file_sync")))
  extern c3_i
  u3_wasm_file_sync(c3_i fil_i);

  __attribute__((import_module("env"), import_name("u3_wasm_file_unlink")))
  extern c3_i
  u3_wasm_file_unlink(const c3_c* pat_c);

  __attribute__((import_module("env"), import_name("u3_wasm_file_size")))
  extern c3_ds
  u3_wasm_file_size(const c3_c* pat_c);

  __attribute__((import_module("env"), import_name("u3_wasm_file_read")))
  extern c3_i
  u3_wasm_file_read(const c3_c* pat_c, c3_y* buf_y, c3_d len_d);

  __attribute__((import_module("env"), import_name("u3_wasm_file_write")))
  extern c3_i
  u3_wasm_file_write(const c3_c* pat_c, const c3_y* buf_y, c3_d len_d);
#endif

#ifndef U3_OS_wasm
static c3_i
_fs_native_flags(c3_w flg_w)
{
  c3_i out_i;

  if ( (flg_w & U3FS_O_READ) && (flg_w & U3FS_O_WRITE) ) {
    out_i = O_RDWR;
  }
  else if ( flg_w & U3FS_O_WRITE ) {
    out_i = O_WRONLY;
  }
  else {
    out_i = O_RDONLY;
  }

  if ( flg_w & U3FS_O_CREATE ) {
    out_i |= O_CREAT;
  }
  if ( flg_w & U3FS_O_TRUNC ) {
    out_i |= O_TRUNC;
  }
  if ( flg_w & U3FS_O_EXCL ) {
    out_i |= O_EXCL;
  }

  return out_i;
}

static c3_o
_fs_off(c3_c* cap_c, c3_d off_d, off_t* out_i)
{
  if ( off_d > (c3_d)LLONG_MAX ) {
    fprintf(stderr, "%s: offset overflow (%" PRIu64 ")\r\n", cap_c, off_d);
    return c3n;
  }

  *out_i = (off_t)off_d;
  if ( (c3_d)*out_i != off_d ) {
    fprintf(stderr, "%s: offset overflow (%" PRIu64 ")\r\n", cap_c, off_d);
    return c3n;
  }

  return c3y;
}
#endif

c3_o
u3fs_ensure_dir(c3_c* cap_c, c3_c* pat_c, c3_w mod_w)
{
#ifdef U3_OS_wasm
  if ( 0 != u3_wasm_file_mkdir(pat_c, mod_w) ) {
    fprintf(stderr, "%s: host mkdir failed (%s)\r\n", cap_c, pat_c);
    return c3n;
  }
  return c3y;
#else
  if ( (0 != c3_mkdir(pat_c, mod_w)) && (EEXIST != errno) ) {
    fprintf(stderr, "%s: mkdir failed (%s): %s\r\n",
                    cap_c, pat_c, strerror(errno));
    return c3n;
  }
  return c3y;
#endif
}

c3_o
u3fs_mkdir(c3_c* cap_c, c3_c* pat_c, c3_w mod_w)
{
#ifdef U3_OS_wasm
  if ( 0 == u3_wasm_file_exists(pat_c) ) {
    return c3n;
  }
  if ( 0 != u3_wasm_file_mkdir(pat_c, mod_w) ) {
    fprintf(stderr, "%s: host mkdir failed (%s)\r\n", cap_c, pat_c);
    return c3n;
  }
  return c3y;
#else
  if ( 0 != c3_mkdir(pat_c, mod_w) ) {
    if ( EEXIST != errno ) {
      fprintf(stderr, "%s: mkdir failed (%s): %s\r\n",
                      cap_c, pat_c, strerror(errno));
    }
    return c3n;
  }
  return c3y;
#endif
}

c3_o
u3fs_exists(c3_c* pat_c)
{
#ifdef U3_OS_wasm
  return (0 == u3_wasm_file_exists(pat_c)) ? c3y : c3n;
#else
  return (0 == access(pat_c, F_OK)) ? c3y : c3n;
#endif
}

c3_i
u3fs_open(c3_c* cap_c, c3_c* pat_c, c3_w flg_w, c3_w mod_w)
{
#ifdef U3_OS_wasm
  c3_i fil_i = u3_wasm_file_open(pat_c, flg_w, mod_w);
  if ( fil_i < 0 ) {
    fprintf(stderr, "%s: host open failed (%s)\r\n", cap_c, pat_c);
  }
  return fil_i;
#else
  c3_i fil_i = c3_open(pat_c, _fs_native_flags(flg_w), mod_w);
  if ( fil_i < 0 ) {
    fprintf(stderr, "%s: c3_open failed (%s): %s\r\n",
                    cap_c, pat_c, strerror(errno));
  }
  return fil_i;
#endif
}

c3_o
u3fs_close(c3_c* cap_c, c3_i fil_i)
{
#ifdef U3_OS_wasm
  if ( 0 != u3_wasm_file_close(fil_i) ) {
    fprintf(stderr, "%s: host close failed (%d)\r\n", cap_c, fil_i);
    return c3n;
  }
  return c3y;
#else
  if ( 0 != close(fil_i) ) {
    fprintf(stderr, "%s: close failed (%d): %s\r\n",
                    cap_c, fil_i, strerror(errno));
    return c3n;
  }
  return c3y;
#endif
}

c3_o
u3fs_size(c3_c* cap_c, c3_i fil_i, c3_d* siz_d)
{
#ifdef U3_OS_wasm
  c3_ds siz_s = u3_wasm_file_handle_size(fil_i);
  if ( siz_s < 0 ) {
    fprintf(stderr, "%s: host file size failed (%d)\r\n", cap_c, fil_i);
    return c3n;
  }
  *siz_d = (c3_d)siz_s;
  return c3y;
#else
  struct stat buf_b;

  if ( -1 == fstat(fil_i, &buf_b) ) {
    fprintf(stderr, "%s: stat failed (%d): %s\r\n",
                    cap_c, fil_i, strerror(errno));
    return c3n;
  }
  if ( buf_b.st_size < 0 ) {
    fprintf(stderr, "%s: negative size (%d)\r\n", cap_c, fil_i);
    return c3n;
  }

  *siz_d = (c3_d)buf_b.st_size;
  return c3y;
#endif
}

c3_o
u3fs_resize(c3_c* cap_c, c3_i fil_i, c3_d siz_d)
{
#ifdef U3_OS_wasm
  if ( 0 != u3_wasm_file_resize(fil_i, siz_d) ) {
    fprintf(stderr, "%s: host resize failed (%d, %" PRIu64 ")\r\n",
                    cap_c, fil_i, siz_d);
    return c3n;
  }
  return c3y;
#else
  off_t off_i;

  if ( c3n == _fs_off(cap_c, siz_d, &off_i) ) {
    return c3n;
  }

  if ( 0 != ftruncate(fil_i, off_i) ) {
    fprintf(stderr, "%s: ftruncate failed (%d, %" PRIu64 "): %s\r\n",
                    cap_c, fil_i, siz_d, strerror(errno));
    return c3n;
  }
  return c3y;
#endif
}

c3_o
u3fs_read_at(c3_c* cap_c, c3_i fil_i, c3_d off_d, c3_d len_d, void* buf_v)
{
#ifdef U3_OS_wasm
  if ( 0 != u3_wasm_file_read_at(fil_i, off_d, buf_v, len_d) ) {
    fprintf(stderr, "%s: host read failed (%d, %" PRIu64 ", %" PRIu64 ")\r\n",
                    cap_c, fil_i, off_d, len_d);
    return c3n;
  }
  return c3y;
#else
  off_t   off_i;
  ssize_t ret_i;

  if ( (len_d > (c3_d)SSIZE_MAX) || (c3n == _fs_off(cap_c, off_d, &off_i)) ) {
    return c3n;
  }

  ret_i = pread(fil_i, buf_v, len_d, off_i);
  if ( (ssize_t)len_d != ret_i ) {
    if ( 0 < ret_i ) {
      fprintf(stderr, "%s: partial read (%zu/%" PRIu64 ")\r\n",
                      cap_c, (size_t)ret_i, len_d);
    }
    else {
      fprintf(stderr, "%s: read failed: %s\r\n", cap_c, strerror(errno));
    }
    return c3n;
  }
  return c3y;
#endif
}

c3_o
u3fs_write_at(c3_c* cap_c, c3_i fil_i, c3_d off_d, c3_d len_d, const void* buf_v)
{
#ifdef U3_OS_wasm
  if ( 0 != u3_wasm_file_write_at(fil_i, off_d, buf_v, len_d) ) {
    fprintf(stderr, "%s: host write failed (%d, %" PRIu64 ", %" PRIu64 ")\r\n",
                    cap_c, fil_i, off_d, len_d);
    return c3n;
  }
  return c3y;
#else
  off_t   off_i;
  ssize_t ret_i;

  if ( (len_d > (c3_d)SSIZE_MAX) || (c3n == _fs_off(cap_c, off_d, &off_i)) ) {
    return c3n;
  }

  ret_i = pwrite(fil_i, buf_v, len_d, off_i);
  if ( (ssize_t)len_d != ret_i ) {
    if ( 0 < ret_i ) {
      fprintf(stderr, "%s: partial write (%zu/%" PRIu64 ")\r\n",
                      cap_c, (size_t)ret_i, len_d);
    }
    else {
      fprintf(stderr, "%s: write failed: %s\r\n", cap_c, strerror(errno));
    }
    return c3n;
  }
  return c3y;
#endif
}

c3_o
u3fs_sync(c3_c* cap_c, c3_i fil_i)
{
#ifdef U3_OS_wasm
  if ( 0 != u3_wasm_file_sync(fil_i) ) {
    fprintf(stderr, "%s: host sync failed (%d)\r\n", cap_c, fil_i);
    return c3n;
  }
  return c3y;
#else
  if ( -1 == c3_sync(fil_i) ) {
    fprintf(stderr, "%s: sync failed (%d): %s\r\n",
                    cap_c, fil_i, strerror(errno));
    return c3n;
  }
  return c3y;
#endif
}

c3_o
u3fs_unlink(c3_c* cap_c, c3_c* pat_c)
{
#ifdef U3_OS_wasm
  if ( 0 != u3_wasm_file_unlink(pat_c) ) {
    fprintf(stderr, "%s: host unlink failed (%s)\r\n", cap_c, pat_c);
    return c3n;
  }
  return c3y;
#else
  if ( 0 != c3_unlink(pat_c) ) {
    fprintf(stderr, "%s: unlink failed (%s): %s\r\n",
                    cap_c, pat_c, strerror(errno));
    return c3n;
  }
  return c3y;
#endif
}

c3_o
u3fs_mmap_read(c3_c* cap_c, c3_c* pat_c, c3_d* out_d, c3_y** out_y)
{
#ifdef U3_OS_wasm
  c3_ds len_s = u3_wasm_file_size(pat_c);
  if ( len_s < 0 ) {
    fprintf(stderr, "%s: host file missing (%s)\r\n", cap_c, pat_c);
    return c3n;
  }

  c3_d  len_d = (c3_d)len_s;
  c3_y* byt_y = malloc(len_d ? len_d : 1);
  if ( NULL == byt_y ) {
    fprintf(stderr, "%s: host file alloc failed (%s, %" PRIu64 ")\r\n",
                    cap_c, pat_c, len_d);
    return c3n;
  }

  if ( 0 != u3_wasm_file_read(pat_c, byt_y, len_d) ) {
    fprintf(stderr, "%s: host file read failed (%s)\r\n", cap_c, pat_c);
    free(byt_y);
    return c3n;
  }

  *out_d = len_d;
  *out_y = byt_y;
  return c3y;
#else
  c3_i fid_i;
  c3_d len_d;

  if ( -1 == (fid_i = c3_open(pat_c, O_RDONLY, 0644)) ) {
    fprintf(stderr, "%s: c3_open failed (%s): %s\r\n",
                    cap_c, pat_c, strerror(errno));
    return c3n;
  }

  {
    struct stat buf_b;

    if ( -1 == fstat(fid_i, &buf_b) ) {
      fprintf(stderr, "%s: stat failed (%s): %s\r\n",
                      cap_c, pat_c, strerror(errno));
      close(fid_i);
      return c3n;
    }

    len_d = buf_b.st_size;
  }

  {
    void* ptr_v;

    if ( MAP_FAILED == (ptr_v = mmap(0, len_d, PROT_READ, MAP_SHARED, fid_i, 0)) ) {
      fprintf(stderr, "%s: mmap failed (%s): %s\r\n",
                      cap_c, pat_c, strerror(errno));
      close(fid_i);
      return c3n;
    }

    *out_d = len_d;
    *out_y = (c3_y*)ptr_v;
  }

  close(fid_i);
  return c3y;
#endif
}

c3_o
u3fs_mmap(c3_c* cap_c, c3_c* pat_c, c3_d len_d, c3_y** out_y)
{
#ifdef U3_OS_wasm
  c3_y* byt_y = calloc(1, len_d ? len_d : 1);
  if ( NULL == byt_y ) {
    fprintf(stderr, "%s: host file alloc failed (%s, %" PRIu64 ")\r\n",
                    cap_c, pat_c, len_d);
    return c3n;
  }

  *out_y = byt_y;
  return c3y;
#else
  c3_i fid_i;

  if ( -1 == (fid_i = c3_open(pat_c, O_RDWR | O_CREAT | O_TRUNC, 0644)) ) {
    fprintf(stderr, "%s: c3_open failed (%s): %s\r\n",
                    cap_c, pat_c, strerror(errno));
    return c3n;
  }

  if ( 0 != ftruncate(fid_i, len_d) ) {
    fprintf(stderr, "%s: ftruncate grow %s: %s\r\n",
                    cap_c, pat_c, strerror(errno));
    close(fid_i);
    return c3n;
  }

  {
    void* ptr_v;

    if ( MAP_FAILED == (ptr_v = mmap(0, len_d, PROT_READ|PROT_WRITE, MAP_SHARED, fid_i, 0)) ) {
      fprintf(stderr, "%s: mmap failed (%s): %s\r\n",
                      cap_c, pat_c, strerror(errno));
      close(fid_i);
      return c3n;
    }

    *out_y = (c3_y*)ptr_v;
  }

  close(fid_i);
  return c3y;
#endif
}

c3_o
u3fs_mmap_save(c3_c* cap_c, c3_c* pat_c, c3_d len_d, c3_y* byt_y)
{
#ifdef U3_OS_wasm
  if ( 0 != u3_wasm_file_write(pat_c, byt_y, len_d) ) {
    fprintf(stderr, "%s: host file write failed (%s)\r\n", cap_c, pat_c);
    return c3n;
  }
  return c3y;
#else
  if ( 0 != msync(byt_y, len_d, MS_SYNC) ) {
    fprintf(stderr, "%s: msync %s: %s\r\n", cap_c, pat_c, strerror(errno));
    return c3n;
  }

  return c3y;
#endif
}

c3_o
u3fs_munmap(c3_d len_d, c3_y* byt_y)
{
#ifdef U3_OS_wasm
  (void)len_d;
  free(byt_y);
  return c3y;
#else
  if ( 0 != munmap(byt_y, len_d) ) {
    return c3n;
  }

  return c3y;
#endif
}
