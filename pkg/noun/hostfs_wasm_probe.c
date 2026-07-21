/// @file
///
/// Link probe for the browser/WASM host file boundary.

#include "hostfs.h"

#include <string.h>

int
main(void)
{
  c3_d  len_d = 0;
  c3_y* red_y = 0;

  if ( c3y != u3fs_mmap_read("hostfs", "/in", &len_d, &red_y) ) {
    return 1;
  }
  if ( (5 != len_d) || (0 != memcmp(red_y, "ames!", 5)) ) {
    return 1;
  }
  if ( c3y != u3fs_munmap(len_d, red_y) ) {
    return 1;
  }

  c3_y* wry_y = 0;
  if ( c3y != u3fs_mmap("hostfs", "/out", 4, &wry_y) ) {
    return 1;
  }
  memcpy(wry_y, "quic", 4);
  if ( c3y != u3fs_mmap_save("hostfs", "/out", 4, wry_y) ) {
    return 1;
  }
  if ( c3y != u3fs_munmap(4, wry_y) ) {
    return 1;
  }

  if ( c3n != u3fs_mmap_read("hostfs", "/missing", &len_d, &red_y) ) {
    return 1;
  }

  if ( c3y != u3fs_ensure_dir("hostfs", "/tmp", 0700) ) {
    return 1;
  }
  if ( c3y == u3fs_exists("/random") ) {
    return 1;
  }

  c3_i fil_i = u3fs_open("hostfs",
                         "/random",
                         U3FS_O_READ | U3FS_O_WRITE |
                         U3FS_O_CREATE | U3FS_O_TRUNC,
                         0600);
  if ( fil_i < 0 ) {
    return 1;
  }

  c3_y buf_y[8] = {0};
  c3_d siz_d = 0;

  if (  (c3y != u3fs_write_at("hostfs", fil_i, 0, 4, "ames"))
     || (c3y != u3fs_write_at("hostfs", fil_i, 4, 4, "quic"))
     || (c3y != u3fs_size("hostfs", fil_i, &siz_d))
     || (8 != siz_d)
     || (c3y != u3fs_read_at("hostfs", fil_i, 4, 4, buf_y))
     || (0 != memcmp(buf_y, "quic", 4))
     || (c3y != u3fs_resize("hostfs", fil_i, 4))
     || (c3y != u3fs_size("hostfs", fil_i, &siz_d))
     || (4 != siz_d)
     || (c3y != u3fs_sync("hostfs", fil_i)) )
  {
    return 1;
  }
  if ( c3y != u3fs_close("hostfs", fil_i) ) {
    return 1;
  }
  if ( c3y != u3fs_exists("/random") ) {
    return 1;
  }
  if ( c3y != u3fs_unlink("hostfs", "/random") ) {
    return 1;
  }
  if ( c3y == u3fs_exists("/random") ) {
    return 1;
  }

  return 0;
}
