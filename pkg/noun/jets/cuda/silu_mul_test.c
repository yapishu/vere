/* silu_mul_test.c — bit-exactness vs CPU reference using same expf form. */
#include "silu_mul.h"
#include "expf_hoon.cuh"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static uint32_t xs(uint32_t* s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;*s=x;return x;}
static float small(uint32_t* s){int32_t v=(int32_t)xs(s);return (float)v/(float)0x7FFFFFFF;}

static void ref(const float* a, const float* b, float* y, size_t N) {
  /* Mirror GPU kernel: expf_hoon (not libm expf) for byte-exact match. */
  for (size_t i=0;i<N;i++){
    float av=a[i];
    float sig=1.0f/(1.0f+expf_hoon(-av));
    y[i]=(av*sig)*b[i];
  }
}

static int cmp_exact(const float* a, const float* b, size_t n, const char* lbl) {
  size_t diff=0, first=(size_t)-1;
  for (size_t i=0;i<n;i++){
    uint32_t ua,ub; memcpy(&ua,&a[i],4); memcpy(&ub,&b[i],4);
    if (ua!=ub){ if (first==(size_t)-1) first=i; diff++; }
  }
  if (!diff){ fprintf(stderr,"  %-32s BIT-EXACT (%zu)\n",lbl,n); return 1; }
  uint32_t ua,ub; memcpy(&ua,&a[first],4); memcpy(&ub,&b[first],4);
  fprintf(stderr,"  %-32s MISMATCH %zu/%zu @%zu %08x vs %08x\n",lbl,diff,n,first,ua,ub);
  return 0;
}

/* CPU expf and CUDA expf aren't byte-identical (neither is IEEE correctly
 * rounded — both within 1 ULP).  Accept ≤2 ULP difference for the GPU↔CPU
 * check; the GPU↔GPU check below uses cmp_exact for strict bit equality. */
static int cmp_ulp(const float* a, const float* b, size_t n, int max_ulp, const char* lbl) {
  size_t diff=0, first=(size_t)-1, max_d=0;
  for (size_t i=0;i<n;i++){
    uint32_t ua,ub; memcpy(&ua,&a[i],4); memcpy(&ub,&b[i],4);
    int32_t sa=(int32_t)ua, sb=(int32_t)ub;
    if (sa<0) sa = (int32_t)0x80000000 - sa;
    if (sb<0) sb = (int32_t)0x80000000 - sb;
    int32_t d = sa>sb ? sa-sb : sb-sa;
    if (d > max_ulp){ if (first==(size_t)-1) first=i; diff++; if ((size_t)d>max_d) max_d=d; }
  }
  if (!diff){ fprintf(stderr,"  %-32s WITHIN %d ULP (%zu)\n",lbl,max_ulp,n); return 1; }
  fprintf(stderr,"  %-32s %zu/%zu exceed %d ulp (max %zu)\n",lbl,diff,n,max_ulp,max_d);
  return 0;
}

int main(void){
  fprintf(stderr,"silu_mul determinism\n--------------------\n");
  size_t sizes[] = {1, 16, 1024, 24576 /* Qwen3 MLP hidden */, 98304};
  int all=1;
  uint32_t st=1;
  for (size_t k=0;k<sizeof(sizes)/sizeof(sizes[0]);k++){
    size_t N=sizes[k];
    float* a=malloc(N*4); float* b=malloc(N*4);
    float* yr=malloc(N*4); float* yg=malloc(N*4); float* yg2=malloc(N*4);
    for (size_t i=0;i<N;i++){ a[i]=small(&st)*3.0f; b[i]=small(&st); }
    ref(a,b,yr,N);
    silu_mul_status r=silu_mul_fp32(a,b,yg,N);
    silu_mul_status r2=silu_mul_fp32(a,b,yg2,N);
    if (r!=SILU_OK || r2!=SILU_OK){ fprintf(stderr,"  GPU err %d/%d\n",r,r2); all=0; }
    else {
      char lbl[48];
      snprintf(lbl,sizeof lbl,"N=%zu gpu==gpu",N);
      all &= cmp_exact(yg,yg2,N,lbl);
      snprintf(lbl,sizeof lbl,"N=%zu gpu==cpu",N);
      all &= cmp_exact(yr,yg,N,lbl);
    }
    free(a);free(b);free(yr);free(yg);free(yg2);
  }
  fprintf(stderr,"--------------------\n%s\n",all?"ALL PASS":"FAIL");
  return all?0:1;
}
