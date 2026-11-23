// Guarded-product benchmark. Run each mode in its own process. Each timed
// sample starts on a fresh road with a compiled program and one warmup call.
#include "noun.h"
#include <time.h>

static u3_noun
_slot(c3_w axis)
{
  return u3nc(0, axis);
}

static u3_noun
_memo(u3_noun body)
{
  return u3nt(11, u3nc(c3__memo, u3nc(1, 0)), body);
}

// The recursive battery has subject [battery list] and counts its list's
// cells. Calling it with Nock 2 keeps this benchmark independent of jets.
static u3_noun
_count(c3_w axis)
{
  u3_noun recur = u3nt(2, u3nc(_slot(2), _slot(7)), _slot(2));
  u3_noun arm = u3nq(6, u3nc(3, _slot(3)), u3nc(4, recur), u3nc(1, 0));
  u3_noun core = u3nc(u3nc(1, u3k(arm)), _slot(axis));
  return u3nt(2, core, u3nc(1, arm));
}

enum cache { plain, whole, projected };

static u3_noun
_formula(enum cache cache)
{
  u3_noun count = cache == projected ? u3nt(7, _slot(2), _memo(_count(1)))
                                     : _count(2);
  u3_noun fol = u3nc(count, _slot(3));
  return cache == whole ? _memo(fol) : fol;
}

static uint64_t
_now(void)
{
  struct timespec ts;
  if ( clock_gettime(CLOCK_MONOTONIC, &ts) ) abort();
  return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

enum pattern { varying_hole, identical, alternating, cheap };

static void
_run(enum cache cache, enum pattern pattern, unsigned rep)
{
  const unsigned n = pattern == cheap ? 200000 : 2000;
  u3m_hate(0);
  u3_noun list = 0;
  for ( unsigned i = 0; i < 128; ++i ) list = u3nc(i, list);
  u3_noun other = u3nc(128, u3k(list));
  u3_noun fol = _formula(cache);
  if ( pattern == cheap ) {
    u3z(fol);
    u3_noun work = u3nc(4, u3nc(4, _slot(2)));
    if ( cache == projected ) {
      u3z(work);
      work = u3nt(7, _slot(2), _memo(u3nc(4, u3nc(4, _slot(1)))));
    }
    fol = u3nc(work, _slot(3));
    if ( cache == whole ) fol = _memo(fol);
  }
  u3p(u3n_prog) prog = u3n_find(0, fol);
  uint64_t start = 0;
  for ( unsigned i = 0; i <= n; ++i ) {
    if ( i == 1 ) start = _now();
    c3_t alt = pattern == alternating && (i & 1);
    u3_noun dep = pattern == cheap ? i : (alt ? other : list);
    u3_noun hole = pattern == identical ? 0 : i;
    u3_noun result = u3n_burn(prog, u3nc(u3k(dep), hole));
    u3_noun expect = pattern == cheap ? i + 2 : 128 + alt;
    if ( c3n == u3du(result) || u3h(result) != expect || u3t(result) != hole ) {
      fprintf(stderr, "rep=%u pattern=%u cache=%u i=%u expected=[%u %u]\n",
              rep, pattern, cache, i, (unsigned)expect, (unsigned)hole);
      u3m_p("actual", result);
      abort();
    }
    u3z(result);
  }
  uint64_t elapsed = _now() - start;
  const char* caches[] = { "plain", "whole", "projected" };
  const char* patterns[] = { "varying-hole", "identical", "alternating", "cheap-miss" };
  printf("%u,%s,%s,%u,%.1f\n", rep, caches[cache], patterns[pattern], n,
         (double)elapsed / n);
  u3z(fol); u3z(other); u3z(list);
  u3z(u3m_love(0));
}

// [7 [4 0 2] 1 42] has a discarded increment that can bail. Both
// inputs are in the same soft call, so a spurious hit would mask the bail.
static u3_noun
_strict(u3_noun unused)
{
  u3_noun fol = u3nc(u3nt(7, u3nc(4, _slot(2)), u3nc(1, 42)), _slot(3));
  u3p(u3n_prog) prog = u3n_find(0, fol);
  u3z(fol);
  u3z(u3n_burn(prog, u3nc(4, 0)));
  return u3n_burn(prog, u3nc(u3nc(1, 2), 0));
}

// These reads share axis 2. Keep it fixed while changing a left-hand hole
// and executing the formula inside a larger product (the keep-stack path).
static u3_noun
_nested(u3_noun unused)
{
  u3_noun input = u3nc(11, 11);
  u3_noun inner = u3nc(_slot(3), u3nt(5, _slot(4), _slot(5)));
  u3_noun fol = u3nc(inner, u3nc(1, 19));
  u3p(u3n_prog) prog = u3n_find(0, fol);
  u3z(fol);
  for ( unsigned i = 0; i < 4; ++i ) {
    u3_noun dep = i == 3 ? u3nc(11, 12) : u3k(input);
    u3_noun pro = u3n_burn(prog, u3nc(dep, i));
    u3_noun expected = u3nc(u3nc(i, i == 3), 19);
    if ( c3n == u3r_sing(expected, pro) ) abort();
    u3z(expected);
    u3z(pro);
  }
  u3z(input);
  return 0;
}

static u3_noun
_missing_hole(u3_noun unused)
{
  u3_noun fol = u3nc(u3nc(4, u3nc(4, _slot(2))), _slot(7));
  u3p(u3n_prog) prog = u3n_find(0, fol);
  u3z(fol);
  u3z(u3n_burn(prog, u3nc(4, u3nc(0, 1))));
  return u3n_burn(prog, u3nc(4, 0));
}

int
main(void)
{
  const char* mode = getenv("U3_GUARD");
  c3_t accelerated = mode && (!strcmp(mode, "use") || !strcmp(mode, "shadow"));
#ifndef U3_GUARDED_BENCH
  if ( accelerated ) {
    fputs("U3_GUARD requires -Dcopt=-DU3_GUARDED_BENCH\n", stderr);
    return 1;
  }
#endif
  u3m_boot_lite(1 << 26);
  u3_noun gon = u3m_soft(0, _strict, 0);
  if ( c3n == u3r_p(gon, c3__exit, NULL) ) abort();
  u3z(gon);
  gon = u3m_soft(0, _missing_hole, 0);
  if ( c3n == u3r_p(gon, c3__exit, NULL) ) abort();
  u3z(gon);
  gon = u3m_soft(0, _nested, 0);
  if ( c3n == u3r_p(gon, 0, NULL) ) abort();
  u3z(gon);
  puts("repeat,cache,pattern,calls,ns_per_call");
  // In accelerated runs time the discovered recipe. Stock runs compare it
  // with both placements of a real %memo hint, in their own fresh caches.
  unsigned count = accelerated ? 1 : 3;
  for ( unsigned rep = 0; rep < 5; ++rep ) {
    for ( unsigned j = 0; j < 4; ++j ) {
      for ( unsigned k = 0; k < count; ++k ) {
        _run((rep+k) % count, (rep+j) % 4, rep);
      }
    }
  }
  return 0;
}
