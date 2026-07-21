#include "whereami.h"

int
wai_getExecutablePath(char* out, int capacity, int* dirname_length)
{
  (void)out;
  (void)capacity;
  if ( dirname_length ) {
    *dirname_length = 0;
  }
  return -1;
}

int
wai_getModulePath(char* out, int capacity, int* dirname_length)
{
  return wai_getExecutablePath(out, capacity, dirname_length);
}
