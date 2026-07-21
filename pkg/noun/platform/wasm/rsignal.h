/// @file

#ifndef NOUN_PLATFORM_WASM_RSIGNAL_H
#define NOUN_PLATFORM_WASM_RSIGNAL_H

#include <sys/time.h>

#ifndef __wasm_exception_handling__
#define __wasm_exception_handling__ 1
#endif

#include <setjmp.h>

#ifndef SIGINT
#define SIGINT 2
#endif
#ifndef SIGTERM
#define SIGTERM 15
#endif
#ifndef SIGVTALRM
#define SIGVTALRM 26
#endif
#ifndef SIGSTK
#define SIGSTK 31
#endif

#ifndef ITIMER_VIRTUAL
#define ITIMER_VIRTUAL 1
#endif

struct itimerval {
  struct timeval it_interval;
  struct timeval it_value;
};

#define rsignal_jmpbuf      jmp_buf
#define rsignal_setjmp(buf) setjmp(buf)
#define rsignal_longjmp     longjmp

typedef void (*rsignal_handler_f)(int);

static inline rsignal_handler_f
rsignal_install_handler(int sig_i, rsignal_handler_f han_f)
{
  (void)sig_i;
  return han_f;
}

static inline rsignal_handler_f
rsignal_deinstall_handler(int sig_i)
{
  (void)sig_i;
  return 0;
}

static inline int
rsignal_setitimer(int typ_i, struct itimerval* in_u, struct itimerval* out_u)
{
  (void)typ_i;
  (void)in_u;
  (void)out_u;
  return 0;
}

#endif /* ifndef NOUN_PLATFORM_WASM_RSIGNAL_H */
