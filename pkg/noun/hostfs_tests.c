/// @file

#include "hostfs.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static c3_i
_expect_bytes(c3_d len_d, c3_y* byt_y, const c3_c* exp_c)
{
  c3_d exp_d = strlen(exp_c);
  return ( (len_d == exp_d) && (0 == memcmp(byt_y, exp_c, exp_d)) )
       ? 0
       : 1;
}

static c3_i
_test_random_access(void)
{
  c3_c pax_c[] = "/tmp/urbit-hostfs-random-XXXXXX";
  c3_i tmp_i = mkstemp(pax_c);
  if ( tmp_i < 0 ) {
    return 1;
  }
  close(tmp_i);
  unlink(pax_c);

  if ( c3y == u3fs_exists(pax_c) ) {
    return 1;
  }

  c3_i fil_i = u3fs_open("hostfs-test",
                         pax_c,
                         U3FS_O_READ | U3FS_O_WRITE |
                         U3FS_O_CREATE | U3FS_O_TRUNC,
                         0600);
  if ( fil_i < 0 ) {
    return 1;
  }

  c3_y buf_y[8] = {0};
  c3_d siz_d = 0;

  if (  (c3y != u3fs_write_at("hostfs-test", fil_i, 0, 4, "ames"))
     || (c3y != u3fs_write_at("hostfs-test", fil_i, 4, 4, "quic"))
     || (c3y != u3fs_size("hostfs-test", fil_i, &siz_d))
     || (8 != siz_d)
     || (c3y != u3fs_read_at("hostfs-test", fil_i, 4, 4, buf_y))
     || (0 != memcmp(buf_y, "quic", 4))
     || (c3y != u3fs_resize("hostfs-test", fil_i, 4))
     || (c3y != u3fs_size("hostfs-test", fil_i, &siz_d))
     || (4 != siz_d)
     || (c3y != u3fs_sync("hostfs-test", fil_i)) )
  {
    u3fs_close("hostfs-test", fil_i);
    unlink(pax_c);
    return 1;
  }

  if ( c3y != u3fs_close("hostfs-test", fil_i) ) {
    unlink(pax_c);
    return 1;
  }
  if ( c3y != u3fs_exists(pax_c) ) {
    unlink(pax_c);
    return 1;
  }
  if ( c3y != u3fs_unlink("hostfs-test", pax_c) ) {
    unlink(pax_c);
    return 1;
  }
  if ( c3y == u3fs_exists(pax_c) ) {
    return 1;
  }

  return 0;
}

int
main(void)
{
  c3_c pax_c[] = "/tmp/urbit-hostfs-test-XXXXXX";
  c3_i fid_i = mkstemp(pax_c);
  if ( fid_i < 0 ) {
    return 1;
  }

  if ( 5 != write(fid_i, "ames!", 5) ) {
    close(fid_i);
    unlink(pax_c);
    return 1;
  }
  close(fid_i);

  c3_d  len_d = 0;
  c3_y* byt_y = 0;
  if ( c3y != u3fs_mmap_read("hostfs-test", pax_c, &len_d, &byt_y) ) {
    unlink(pax_c);
    return 1;
  }
  if ( 0 != _expect_bytes(len_d, byt_y, "ames!") ) {
    u3fs_munmap(len_d, byt_y);
    unlink(pax_c);
    return 1;
  }
  if ( c3y != u3fs_munmap(len_d, byt_y) ) {
    unlink(pax_c);
    return 1;
  }

  if ( c3y != u3fs_mmap("hostfs-test", pax_c, 4, &byt_y) ) {
    unlink(pax_c);
    return 1;
  }
  memcpy(byt_y, "quic", 4);
  if ( c3y != u3fs_mmap_save("hostfs-test", pax_c, 4, byt_y) ) {
    u3fs_munmap(4, byt_y);
    unlink(pax_c);
    return 1;
  }
  if ( c3y != u3fs_munmap(4, byt_y) ) {
    unlink(pax_c);
    return 1;
  }

  if ( c3y != u3fs_mmap_read("hostfs-test", pax_c, &len_d, &byt_y) ) {
    unlink(pax_c);
    return 1;
  }
  if ( 0 != _expect_bytes(len_d, byt_y, "quic") ) {
    u3fs_munmap(len_d, byt_y);
    unlink(pax_c);
    return 1;
  }
  u3fs_munmap(len_d, byt_y);

  unlink(pax_c);
  return _test_random_access();
}
