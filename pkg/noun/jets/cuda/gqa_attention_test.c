/* gqa_attention_test.c — bit-exactness for GQA attention kernel vs CPU
 * reference implementing the same thread-0-sequential-reduction form. */

#include "gqa_attention.h"
#include "expf_hoon.cuh"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static uint32_t
xs(uint32_t* s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;*s=x;return x;}

static float
small(uint32_t* s){int32_t v=(int32_t)xs(s);return (float)v/(float)0x7FFFFFFF*0.5f;}

static void
ref(const float* q, const float* k, const float* v, float* y,
    size_t S, size_t H, size_t KH, size_t Dh)
{
  size_t group = H / KH;
  float inv = 1.0f / sqrtf((float)Dh);
  /* scratch for scores / probs per (h, p) */
  float* scr = (float*)malloc(S * sizeof(float));
  float* pb  = (float*)malloc(S * sizeof(float));
  for ( size_t h = 0; h < H; h++ ) {
    size_t kvh = h / group;
    for ( size_t p = 0; p < S; p++ ) {
      const float* qp = q + p * H * Dh + h * Dh;
      float mx = -INFINITY;
      for ( size_t j = 0; j <= p; j++ ) {
        const float* kj = k + j * KH * Dh + kvh * Dh;
        float s = 0.0f;
        for ( size_t e = 0; e < Dh; e++ ) s = s + qp[e] * kj[e];
        s = s * inv;
        scr[j] = s;
        if ( s > mx ) mx = s;
      }
      float sum = 0.0f;
      for ( size_t j = 0; j <= p; j++ ) {
        float e = expf_hoon(scr[j] - mx);
        scr[j] = e;
        sum = sum + e;
      }
      for ( size_t j = 0; j <= p; j++ ) pb[j] = scr[j] / sum;
      for ( size_t j = p + 1; j < S; j++ ) pb[j] = 0.0f;
      for ( size_t d = 0; d < Dh; d++ ) {
        float out = 0.0f;
        for ( size_t j = 0; j < S; j++ ) {
          float vj = v[j * KH * Dh + kvh * Dh + d];
          out = out + pb[j] * vj;
        }
        y[p * H * Dh + h * Dh + d] = out;
      }
    }
  }
  free(scr); free(pb);
}

static int
cmp_exact(const float* a, const float* b, size_t n, const char* lbl)
{
  size_t diff=0, first=(size_t)-1;
  for (size_t i=0;i<n;i++){
    uint32_t ua,ub; memcpy(&ua,&a[i],4); memcpy(&ub,&b[i],4);
    if (ua!=ub){ if (first==(size_t)-1) first=i; diff++; }
  }
  if (!diff){ fprintf(stderr,"  %-48s BIT-EXACT (%zu)\n",lbl,n); return 1; }
  uint32_t ua,ub; memcpy(&ua,&a[first],4); memcpy(&ub,&b[first],4);
  fprintf(stderr,"  %-48s MISMATCH %zu/%zu @%zu a=%08x b=%08x\n",lbl,diff,n,first,ua,ub);
  return 0;
}

/* CUDA expf and libm expf differ by ≤1 ULP — softmax outputs propagate
 * that up to ~2 ULP in y.  Accept that for the CPU↔GPU comparison;
 * GPU↔GPU is strictly bit-exact. */
static int
cmp_ulp(const float* a, const float* b, size_t n, int max_ulp, const char* lbl)
{
  size_t diff=0, max_d=0;
  for (size_t i=0;i<n;i++){
    uint32_t ua,ub; memcpy(&ua,&a[i],4); memcpy(&ub,&b[i],4);
    int32_t sa=(int32_t)ua, sb=(int32_t)ub;
    if (sa<0) sa = (int32_t)0x80000000 - sa;
    if (sb<0) sb = (int32_t)0x80000000 - sb;
    int32_t d = sa>sb ? sa-sb : sb-sa;
    if (d > max_ulp){ diff++; if ((size_t)d>max_d) max_d=d; }
  }
  if (!diff){ fprintf(stderr,"  %-48s WITHIN %d ULP (%zu)\n",lbl,max_ulp,n); return 1; }
  fprintf(stderr,"  %-48s %zu/%zu exceed %d ulp (max %zu)\n",lbl,diff,n,max_ulp,max_d);
  return 0;
}

static int
run(size_t S, size_t H, size_t KH, size_t Dh, uint32_t seed)
{
  size_t q_n = S*H*Dh, kv_n = S*KH*Dh;
  float* q  = malloc(q_n*4);
  float* k  = malloc(kv_n*4);
  float* vv = malloc(kv_n*4);
  float* yr = malloc(q_n*4);
  float* yg = malloc(q_n*4);
  float* yg2= malloc(q_n*4);
  uint32_t st=seed;
  for (size_t i=0;i<q_n;i++) q[i]=small(&st);
  for (size_t i=0;i<kv_n;i++) k[i]=small(&st);
  for (size_t i=0;i<kv_n;i++) vv[i]=small(&st);
  ref(q,k,vv,yr,S,H,KH,Dh);
  gqa_attention_status r=gqa_attention_fp32(q,k,vv,yg,S,H,KH,Dh);
  gqa_attention_status r2=gqa_attention_fp32(q,k,vv,yg2,S,H,KH,Dh);
  if (r!=GQA_OK||r2!=GQA_OK){ fprintf(stderr,"  GPU err %d/%d\n",r,r2); return 0; }
  char lbl[80];
  snprintf(lbl,sizeof lbl,"S=%zu H=%zu KH=%zu Dh=%zu gpu==gpu",S,H,KH,Dh);
  int ok1 = cmp_exact(yg,yg2,q_n,lbl);
  snprintf(lbl,sizeof lbl,"S=%zu H=%zu KH=%zu Dh=%zu gpu==cpu",S,H,KH,Dh);
  int ok2 = cmp_exact(yr,yg,q_n,lbl);
  free(q);free(k);free(vv);free(yr);free(yg);free(yg2);
  return ok1 && ok2;
}

int main(void){
  fprintf(stderr,"gqa_attention bit-exactness\n---------------------------\n");
  struct { size_t S, H, KH, Dh; uint32_t seed; } cases[] = {
    { 1,  1, 1,   4, 1 },
    { 2,  2, 1,   4, 2 },
    { 4,  4, 2,  64, 3 },
    { 4, 16, 8, 128, 4 },  /* Qwen3 */
    { 8, 16, 8, 128, 5 },
  };
  int all=1;
  for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
    all &= run(cases[i].S,cases[i].H,cases[i].KH,cases[i].Dh,cases[i].seed);
  fprintf(stderr,"---------------------------\n%s\n",all?"ALL PASS":"FAIL");
  return all?0:1;
}
