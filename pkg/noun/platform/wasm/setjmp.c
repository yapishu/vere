/// @file

#ifdef U3_OS_wasm

#include <stdint.h>

struct _wasm_longjmp_args {
  void* env;
  int val;
};

struct _wasm_jmp_buf {
  void* invocation;
  uint32_t label;
  struct _wasm_longjmp_args args;
};

void
__wasm_setjmp(void* env, uint32_t label, void* invocation)
{
  struct _wasm_jmp_buf* buf = env;

  buf->invocation = invocation;
  buf->label = label;
}

uint32_t
__wasm_setjmp_test(void* env, void* invocation)
{
  struct _wasm_jmp_buf* buf = env;

  return (buf->invocation == invocation) ? buf->label : 0;
}

__attribute__((noreturn)) void
__wasm_longjmp(void* env, int val)
{
  struct _wasm_jmp_buf* buf = env;

  if ( 0 == val ) {
    val = 1;
  }

  buf->args.env = env;
  buf->args.val = val;
  __builtin_wasm_throw(1, &buf->args);
  __builtin_unreachable();
}

#endif
