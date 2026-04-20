/*
 * gqa_attention.h — fused Grouped-Query Attention on fp32.
 *
 *   q: [S, H, Dh]   (query heads)
 *   k: [S, KH, Dh]  (KV heads)
 *   v: [S, KH, Dh]
 *   y: [S, H*Dh]    (output: attention-weighted values, heads concat'd)
 *
 * Each query head h maps to KV head `h / (H / KH)` — standard GQA group
 * mapping.  Causal mask: query at position p can only attend to keys at
 * positions j where j <= p.  Softmax uses the max-subtract trick for
 * numerical stability.
 *
 * Determinism: per-(h, p) softmax + reductions run single-threaded
 * within a block via thread 0.  Output slice is then computed in
 * parallel by all threads using sequential fmaf over j.
 *
 * Model-agnostic: works for any multi-head/grouped-query attention
 * (H == KH ⇒ standard MHA, KH == 1 ⇒ MQA).  Not yet covered: sliding
 * window, non-causal attention.
 */

#ifndef URBIT_CUDA_GQA_ATTENTION_H
#define URBIT_CUDA_GQA_ATTENTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef enum {
  GQA_OK = 0,
  GQA_NO_CUDA,
  GQA_ALLOC_FAIL,
  GQA_LAUNCH_FAIL,
  GQA_INVALID_ARG
} gqa_attention_status;

gqa_attention_status
gqa_attention_fp32(const float* q,
                   const float* k,
                   const float* v,
                   float*       y,
                   size_t       S,
                   size_t       H,
                   size_t       KH,
                   size_t       Dh);

#ifdef __cplusplus
}
#endif

#endif
