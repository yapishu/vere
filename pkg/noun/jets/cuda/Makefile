# Build libvere_cuda.a from the CUDA sources.
#
# Invoked by the zig build step (see build.zig) when -Dcuda=true.
# Can also be run standalone for testing: `make all test`
#
# Strict IEEE flags enforce the cross-device determinism guarantee.

NVCC        ?= nvcc
AR          ?= ar
CFLAGS      := -O2 -std=c++14 --fmad=false -ftz=false \
               -prec-div=true -prec-sqrt=true \
               -Xcompiler "-fPIC -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
# Support Turing/Ampere/Ada/Hopper/Blackwell.  Adjust if needed.
GENCODES    := \
  -gencode arch=compute_75,code=sm_75 \
  -gencode arch=compute_80,code=sm_80 \
  -gencode arch=compute_86,code=sm_86 \
  -gencode arch=compute_89,code=sm_89 \
  -gencode arch=compute_90,code=sm_90

OBJS := sgemm_det.o mlx2_matmul.o rms_norm.o rope_apply.o silu_mul.o gqa_attention.o qwen3_block.o qwen3_forward.o qwen3_decode.o vram_cache.o backend.o sgemm_ref.o

LIB  := libvere_cuda.a

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.cu
	$(NVCC) $(CFLAGS) $(GENCODES) -c $< -o $@

sgemm_ref.o: sgemm_ref.c
	$(CC) -O2 -fPIC -c $< -o $@

det_test: sgemm_det.o vram_cache.o sgemm_ref.o det_test.c
	$(NVCC) $(CFLAGS) $^ -o $@ -lm

mlx2_test: mlx2_matmul.o vram_cache.o mlx2_test.c
	$(NVCC) $(CFLAGS) $^ -o $@ -lm

rms_norm_test: rms_norm.o rms_norm_test.c
	$(NVCC) $(CFLAGS) $^ -o $@ -lm

rope_apply_test: rope_apply.o rope_apply_test.c
	$(NVCC) $(CFLAGS) $^ -o $@ -lm

silu_mul_test: silu_mul.o silu_mul_test.c
	$(NVCC) $(CFLAGS) $^ -o $@ -lm

gqa_attention_test: gqa_attention.o gqa_attention_test.c
	$(NVCC) $(CFLAGS) $^ -o $@ -lm

test: det_test mlx2_test rms_norm_test rope_apply_test silu_mul_test gqa_attention_test
	./det_test
	./mlx2_test
	./rms_norm_test
	./rope_apply_test
	./silu_mul_test
	./gqa_attention_test

clean:
	rm -f $(OBJS) $(LIB) det_test mlx2_test

.PHONY: all test clean
