/* rope_apply_test.c — bit-exactness for rope_apply_fp32 vs CPU ref.
 * Ref uses identical two-fmaf form so rounding matches GPU's exactly. */

#include "rope_apply.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static uint32_t
xs(uint32_t* s) { uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }

static float
small(uint32_t* s) { int32_t v=(int32_t)xs(s); return (float)v/(float)0x7FFFFFFF; }

static float
adv(uint32_t* s)
{
  uint32_t u=xs(s);
  uint32_t roll=u&0xF, sign=(u>>31)&1, mant=u&0x7FFFFF;
  uint32_t exp_ = roll<2 ? 0
               : roll==2 ? 240+(u>>28)
               : 100 + ((u>>20) & 0x3F);
  uint32_t bits=(sign<<31)|(exp_<<23)|mant;
  if (exp_==255 && mant!=0) bits=(sign<<31)|(128<<23);
  float f; memcpy(&f,&bits,4); return f;
}

static void
ref(const float* x, const float* c, const float* s, float* y,
    size_t S, size_t H, size_t Dh)
{
  size_t half = Dh / 2;
  /* Mirror Hoon: (add (mul xj cp) (mul rot sp)).  Two separate muls,
   * each with one rounding, then one add rounding. */
  for ( size_t p = 0; p < S; p++ ) {
    for ( size_t h = 0; h < H; h++ ) {
      for ( size_t j = 0; j < Dh; j++ ) {
        size_t base = p * H * Dh + h * Dh;
        float xj = x[base + j];
        float xr = (j < half) ? -x[base + j + half] : x[base + j - half];
        size_t cs = p * Dh + j;
        y[base + j] = (xj * c[cs]) + (xr * s[cs]);
      }
    }
  }
}

static int
cmp(const float* a, const float* b, size_t n, const char* lbl)
{
  size_t diff=0, first=(size_t)-1;
  for (size_t i=0;i<n;i++){
    uint32_t ua,ub;
    memcpy(&ua,&a[i],4); memcpy(&ub,&b[i],4);
    if (ua!=ub) { if (first==(size_t)-1) first=i; diff++; }
  }
  if (!diff) { fprintf(stderr,"  %-40s BIT-EXACT (%zu)\n",lbl,n); return 1; }
  uint32_t ua,ub;
  memcpy(&ua,&a[first],4); memcpy(&ub,&b[first],4);
  fprintf(stderr,"  %-40s MISMATCH %zu/%zu @%zu a=%08x b=%08x\n",lbl,diff,n,first,ua,ub);
  return 0;
}

static int
run(size_t S, size_t H, size_t Dh, uint32_t seed, int advi)
{
  size_t n = S*H*Dh;
  size_t nc = S*Dh;
  float* x  = malloc(n*4);
  float* c  = malloc(nc*4);
  float* sn = malloc(nc*4);
  float* yr = malloc(n*4);
  float* yg = malloc(n*4);
  uint32_t st=seed;
  float (*rng)(uint32_t*) = advi ? adv : small;
  for (size_t i=0;i<n;i++) x[i]=rng(&st);
  for (size_t i=0;i<nc;i++){ c[i]=small(&st); sn[i]=small(&st); }
  ref(x,c,sn,yr,S,H,Dh);
  rope_apply_status r = rope_apply_fp32(x,c,sn,yg,S,H,Dh);
  if (r!=ROPE_OK){ fprintf(stderr,"  GPU failed %d\n",r); return 0; }
  char lbl[64];
  snprintf(lbl,sizeof lbl,"%s S=%zu H=%zu Dh=%zu",advi?"adv":"std",S,H,Dh);
  int ok = cmp(yr,yg,n,lbl);
  free(x);free(c);free(sn);free(yr);free(yg);
  return ok;
}

int main(void) {
  fprintf(stderr,"rope_apply bit-exactness\n------------------------\n");
  struct { size_t S, H, Dh; uint32_t seed; } cases[] = {
    { 1, 1,   4, 1 },
    { 1, 1,  64, 2 },
    { 4, 16, 128, 3 },  /* Qwen3 q heads */
    { 4,  8, 128, 4 },  /* Qwen3 k/v heads */
    { 1, 12,  64, 5 },  /* GPT-2-ish */
  };
  int all=1;
  for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
    all &= run(cases[i].S,cases[i].H,cases[i].Dh,cases[i].seed,0);
  fprintf(stderr,"\nadversarial:\n");
  for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
    all &= run(cases[i].S,cases[i].H,cases[i].Dh,cases[i].seed+1000,1);
  fprintf(stderr,"------------------------\n%s\n",all?"ALL PASS":"FAIL");
  return all?0:1;
}
