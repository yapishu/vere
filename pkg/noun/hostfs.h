/// @file

#ifndef U3_HOSTFS_H
#define U3_HOSTFS_H

#include "c3/c3.h"

#define U3FS_O_READ    0x01
#define U3FS_O_WRITE   0x02
#define U3FS_O_CREATE  0x04
#define U3FS_O_TRUNC   0x08
#define U3FS_O_EXCL    0x10

  /* u3fs_ensure_dir(): create a directory if it is missing.
  */
    c3_o
    u3fs_ensure_dir(c3_c* cap_c, c3_c* pat_c, c3_w mod_w);

  /* u3fs_mkdir(): create a directory, failing if it already exists.
  */
    c3_o
    u3fs_mkdir(c3_c* cap_c, c3_c* pat_c, c3_w mod_w);

  /* u3fs_exists(): test whether a path exists.
  */
    c3_o
    u3fs_exists(c3_c* pat_c);

  /* u3fs_open(): open a file-like object.
  */
    c3_i
    u3fs_open(c3_c* cap_c, c3_c* pat_c, c3_w flg_w, c3_w mod_w);

  /* u3fs_close(): close a file-like object.
  */
    c3_o
    u3fs_close(c3_c* cap_c, c3_i fil_i);

  /* u3fs_size(): measure a file-like object.
  */
    c3_o
    u3fs_size(c3_c* cap_c, c3_i fil_i, c3_d* siz_d);

  /* u3fs_resize(): resize a file-like object.
  */
    c3_o
    u3fs_resize(c3_c* cap_c, c3_i fil_i, c3_d siz_d);

  /* u3fs_read_at(): read exactly [len_d] bytes at [off_d].
  */
    c3_o
    u3fs_read_at(c3_c* cap_c, c3_i fil_i, c3_d off_d, c3_d len_d, void* buf_v);

  /* u3fs_write_at(): write exactly [len_d] bytes at [off_d].
  */
    c3_o
    u3fs_write_at(c3_c* cap_c, c3_i fil_i, c3_d off_d, c3_d len_d, const void* buf_v);

  /* u3fs_sync(): sync a file-like object.
  */
    c3_o
    u3fs_sync(c3_c* cap_c, c3_i fil_i);

  /* u3fs_unlink(): unlink a path.
  */
    c3_o
    u3fs_unlink(c3_c* cap_c, c3_c* pat_c);

  /* u3fs_mmap_read(): map a read-only file-like blob.
  */
    c3_o
    u3fs_mmap_read(c3_c* cap_c, c3_c* pat_c, c3_d* out_d, c3_y** out_y);

  /* u3fs_mmap(): create a writeable file-like blob.
  */
    c3_o
    u3fs_mmap(c3_c* cap_c, c3_c* pat_c, c3_d len_d, c3_y** out_y);

  /* u3fs_mmap_save(): flush a writeable file-like blob.
  */
    c3_o
    u3fs_mmap_save(c3_c* cap_c, c3_c* pat_c, c3_d len_d, c3_y* byt_y);

  /* u3fs_munmap(): release a mapped file-like blob.
  */
    c3_o
    u3fs_munmap(c3_d len_d, c3_y* byt_y);

#endif /* ifndef U3_HOSTFS_H */
