/// @file

#include "events.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static c3_i
_write_all(c3_c* pax_c, c3_y* byt_y, size_t len_i)
{
  c3_i fid_i = open(pax_c, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if ( fid_i < 0 ) {
    return 1;
  }

  ssize_t ret_i = write(fid_i, byt_y, len_i);
  c3_i    err_i = (ret_i == (ssize_t)len_i) ? 0 : 1;
  close(fid_i);
  return err_i;
}

static c3_i
_read_all(c3_c* pax_c, c3_y* byt_y, size_t len_i)
{
  c3_i fid_i = open(pax_c, O_RDONLY, 0600);
  if ( fid_i < 0 ) {
    return 1;
  }

  ssize_t ret_i = read(fid_i, byt_y, len_i);
  c3_i    err_i = (ret_i == (ssize_t)len_i) ? 0 : 1;
  close(fid_i);
  return err_i;
}

int
main(void)
{
  c3_c src_c[] = "/tmp/urbit-events-src-XXXXXX";
  c3_c dst_c[] = "/tmp/urbit-events-dst-XXXXXX";
  c3_c sim_c[8192];
  c3_c dim_c[8192];
  size_t len_i = (size_t)2 << (u3a_page + 2);
  c3_y*  inp_y = malloc(len_i);
  c3_y*  out_y = calloc(1, len_i);

  if ( (NULL == inp_y) || (NULL == out_y) ) {
    return 1;
  }
  if ( (NULL == mkdtemp(src_c)) || (NULL == mkdtemp(dst_c)) ) {
    return 1;
  }
  rmdir(dst_c);

  for ( size_t i_i = 0; i_i < len_i; i_i++ ) {
    inp_y[i_i] = (c3_y)(i_i * 131);
  }

  snprintf(sim_c, 8192, "%s/image.bin", src_c);
  snprintf(dim_c, 8192, "%s/image.bin", dst_c);

  if ( 0 != _write_all(sim_c, inp_y, len_i) ) {
    return 1;
  }
  if ( c3y != u3e_backup(src_c, dst_c, c3n) ) {
    return 1;
  }
  if ( 0 != _read_all(dim_c, out_y, len_i) ) {
    return 1;
  }
  if ( 0 != memcmp(inp_y, out_y, len_i) ) {
    return 1;
  }
  if ( c3n != u3e_backup(src_c, dst_c, c3n) ) {
    return 1;
  }

  unlink(sim_c);
  unlink(dim_c);
  rmdir(src_c);
  rmdir(dst_c);
  free(inp_y);
  free(out_y);
  return 0;
}
