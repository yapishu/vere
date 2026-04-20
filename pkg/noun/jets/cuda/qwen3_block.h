/*
 * qwen3_block.h — fused per-transformer-block kernel chain.
 *
 * One call performs the complete Qwen3 block:
 *   x  = rmsnorm(x, input_ln)
 *   q,k,v = mlx2-matmul on x (3 projs)
 *   q,k = per-head rmsnorm
 *   q,k = rope-apply
 *   attn = gqa-attention(q, k, v)
 *   o   = mlx2-matmul(attn, o_proj)
 *   x   = x + o
 *   x2  = rmsnorm(x, post_ln)
 *   mlp = down( silu(gate(x2)) * up(x2) )
 *   x   = x + mlp
 *
 * All intermediate activations live entirely in VRAM; only the initial
 * x is uploaded and only the final x is downloaded.  Weights, RoPE
 * tables, and RMSNorm gammas are all VRAM device pointers — caller
 * resolves via vram_cache_probe / vram_cache_get_or_upload.  Gammas
 * are shared across forwards, cos/sin across all 28 blocks of one
 * forward, so one HtoD per forward is enough for those.
 *
 * Determinism: every sub-step uses the same kernels as the per-op jets,
 * which are byte-exact against Hoon.  Fusion is only in execution
 * locality, not algorithm.
 */

#ifndef URBIT_CUDA_QWEN3_BLOCK_H
#define URBIT_CUDA_QWEN3_BLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum {
  QW3_OK = 0,
  QW3_NO_CUDA,
  QW3_ALLOC_FAIL,
  QW3_LAUNCH_FAIL,
  QW3_INVALID_ARG
} qw3_block_status;

/* Per-block weight + gamma VRAM device pointers.  Filled once per
 * block by the caller (all looked up via vram_cache_probe / upload);
 * then the forward kernel chain just chases pointers. */
typedef struct {
  uintptr_t qw, qs, qb;
  uintptr_t kw, ks, kb;
  uintptr_t vw, vs, vb;
  uintptr_t ow, os, ob;
  uintptr_t gate_w, gate_s, gate_b;
  uintptr_t up_w,   up_s,   up_b;
  uintptr_t down_w, down_s, down_b;
  uintptr_t input_ln, post_ln, q_norm, k_norm;
} qw3_block_dptrs;

/* Run one Qwen3 block.  All uintptr_t args are VRAM device pointers
 * (as returned by vram_cache_probe / get_or_upload).  All other
 * pointers are host fp32 buffers. */
qw3_block_status
qw3_block_fp32(const float* x,                /* [S, D] host */
               float*       y,                /* [S, D] host output */
               /* mlx2 projection weights (VRAM dptrs): */
               uintptr_t q_w, uintptr_t q_s, uintptr_t q_b,
               uintptr_t k_w, uintptr_t k_s, uintptr_t k_b,
               uintptr_t v_w, uintptr_t v_s, uintptr_t v_b,
               uintptr_t o_w, uintptr_t o_s, uintptr_t o_b,
               uintptr_t gate_w, uintptr_t gate_s, uintptr_t gate_b,
               uintptr_t up_w,   uintptr_t up_s,   uintptr_t up_b,
               uintptr_t down_w, uintptr_t down_s, uintptr_t down_b,
               /* rmsnorm gammas (VRAM dptrs): */
               uintptr_t input_ln_dptr,       /* [D] */
               uintptr_t post_ln_dptr,        /* [D] */
               uintptr_t q_norm_dptr,         /* [Dh] */
               uintptr_t k_norm_dptr,         /* [Dh] */
               /* rope tables (VRAM dptrs): */
               uintptr_t cos_dptr,            /* [S, Dh] */
               uintptr_t sin_dptr,            /* [S, Dh] */
               /* shape / config: */
               size_t S, size_t D, size_t D_ff,
               size_t H, size_t KH, size_t Dh,
               size_t group_size,
               float  rms_eps);

/* Full multi-block forward.  Runs all `n_blocks` blocks back-to-back
 * on GPU with x kept in VRAM between blocks; one HtoD for the input
 * embedding + one DtoH for the final x.  Scratch is allocated once
 * and reused across every block, so the 28-block stack only incurs
 * one sync barrier (at the final DtoH) instead of 28. */
qw3_block_status
qw3_forward_fp32(const float* x_host,                 /* [S, D] */
                 float*       y_host,                 /* [S, D] */
                 const qw3_block_dptrs* blocks,
                 size_t       n_blocks,
                 uintptr_t    cos_dptr,
                 uintptr_t    sin_dptr,
                 size_t S, size_t D, size_t D_ff,
                 size_t H, size_t KH, size_t Dh,
                 size_t group_size,
                 float  rms_eps);

#ifdef __cplusplus
}
#endif

#endif
