/*
 * mlx2_matmul.h — fused MLX2 dequant + fp32 matmul.
 *
 * MLX 2-bit format:
 *   w:      [out, packed_cols] uint32  (16 packed int2 values per uint32)
 *   scales: [out, groups_per_row] fp32
 *   biases: [out, groups_per_row] fp32
 *   in_features     = packed_cols * 16
 *   groups_per_row  = in_features / group_size
 *
 * Per element, the dequanted weight at logical (i, o) is:
 *   q      = (w[o, i/16] >> ((i % 16) * 2)) & 0x3
 *   w_fp   = scales[o, i/group_size] * q + biases[o, i/group_size]
 *
 * Fused kernel: y[s, o] = sum_i  x[s, i] * w_fp[i, o]
 *
 * Determinism: each output element is computed by one thread; the sum over
 * i runs strictly left-to-right using fmaf.  Bit-identical to a CPU that
 * dequants into a full [in, out] fp32 matrix and then runs sgemm_ref on it.
 *
 * The packed w buffer is the hot one — size scales with out*in/16.  Pass it
 * via a cached VRAM pointer to avoid the 1-MB/Qwen3-matmul upload.
 */

#ifndef URBIT_CUDA_MLX2_MATMUL_H
#define URBIT_CUDA_MLX2_MATMUL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum {
  MLX2_MATMUL_OK = 0,
  MLX2_MATMUL_NO_CUDA,
  MLX2_MATMUL_ALLOC_FAIL,
  MLX2_MATMUL_LAUNCH_FAIL,
  MLX2_MATMUL_INVALID_ARG
} mlx2_matmul_status;

/*
 * Host-side variant: all buffers are host pointers, uploaded fresh each
 * call.  Useful for warmup and testing.
 */
mlx2_matmul_status
mlx2_matmul_fresh(const float*    x,           /* [S, in_features]  */
                  const uint32_t* w_packed,    /* [out_features, packed_cols] */
                  const float*    scales,      /* [out_features, groups_per_row] */
                  const float*    biases,      /* [out_features, groups_per_row] */
                  float*          y,           /* [S, out_features] */
                  size_t S,
                  size_t in_features,
                  size_t out_features,
                  size_t group_size);

/*
 * Cached-weights variant: w, scales, biases are VRAM device pointers
 * (uintptr_t to avoid leaking cuda.h into this header).  x and y are host
 * pointers.  This is the hot path: the ~1 MB of packed weights per matmul
 * is already resident; we only pay for x upload and y download.
 */
mlx2_matmul_status
mlx2_matmul_cached(const float* x,
                   uintptr_t    w_packed_dptr,
                   uintptr_t    scales_dptr,
                   uintptr_t    biases_dptr,
                   float*       y,
                   size_t S,
                   size_t in_features,
                   size_t out_features,
                   size_t group_size);

#ifdef __cplusplus
}
#endif

#endif  /* URBIT_CUDA_MLX2_MATMUL_H */
