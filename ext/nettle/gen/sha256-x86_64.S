/* Generated from GNU Nettle x86_64/sha_ni/sha256-compress-n.asm by sha256.sh.
   Copyright (C) 2018, 2022 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/
	.file "sha256-compress-n.asm"
	.text
#ifndef __APPLE__
	.align 16
#else /* __APPLE__ */
	.align 4
#endif /* __APPLE__ */
.Lswap_mask:
	.byte 3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12
#ifndef __APPLE__
.globl _nettle_sha256_compress_n_sha_ni
#ifndef _WIN32
.type _nettle_sha256_compress_n_sha_ni,%function
#else /* _WIN32 */
.def _nettle_sha256_compress_n_sha_ni
.scl 2
.type 32
.endef
#endif /* _WIN32 */
_nettle_sha256_compress_n_sha_ni: endbr64
#ifdef _WIN32
        push	%rdi
      sub	$80, %rsp
      movdqa	%xmm6, 0(%rsp)
      movdqa	%xmm7, 16(%rsp)
      movdqa	%xmm8, 32(%rsp)
      movdqa	%xmm9, 48(%rsp)
      movdqa	%xmm10, 64(%rsp)
      mov	%rcx, %rdi
            push	%rsi
      mov	%rdx, %rsi
      mov	%r8, %rdx
      mov	%r9, %rcx
#endif /* _WIN32 */
#else /* __APPLE__ */
.globl __nettle_sha256_compress_n_sha_ni
__nettle_sha256_compress_n_sha_ni: endbr64
#endif /* __APPLE__ */
	test	%rdx, %rdx
	jz	.Lend
	movups	(%rdi), %xmm10
	movups	16(%rdi), %xmm5
	pshufd	$0x1b, %xmm10, %xmm10
	pshufd	$0x1b, %xmm5, %xmm5
	movdqa	%xmm5, %xmm6
	punpckhqdq %xmm10, %xmm5
	punpcklqdq %xmm10, %xmm6
	movdqa	.Lswap_mask(%rip), %xmm9
.Loop:
	movups	(%rcx), %xmm1
	pshufb	%xmm9, %xmm1
	movdqa	%xmm5, %xmm7
	movdqa	%xmm6, %xmm8
	movdqa	(%rsi), %xmm0
	paddd	%xmm1, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movups	16(%rcx), %xmm2
	pshufb	%xmm9, %xmm2
	movdqa	16(%rsi), %xmm0
	paddd	%xmm2, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	sha256msg1 %xmm2, %xmm1
	movups	32(%rcx), %xmm3
	pshufb	%xmm9, %xmm3
	movdqa	32(%rsi), %xmm0
	paddd	%xmm3, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	sha256msg1 %xmm3, %xmm2
	movups	48(%rcx), %xmm4
	pshufb	%xmm9, %xmm4
	movdqa	48(%rsi), %xmm0
	paddd	%xmm4, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm4, %xmm10
	palignr	$4, %xmm3, %xmm10
	paddd	%xmm10, %xmm1
	sha256msg2 %xmm4, %xmm1
	sha256msg1 %xmm4, %xmm3
	movdqa	64(%rsi), %xmm0
	paddd	%xmm1, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm1, %xmm10
	palignr	$4, %xmm4, %xmm10
	paddd	%xmm10, %xmm2
	sha256msg2 %xmm1, %xmm2
	sha256msg1 %xmm1, %xmm4
	movdqa	80(%rsi), %xmm0
	paddd	%xmm2, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm2, %xmm10
	palignr	$4, %xmm1, %xmm10
	paddd	%xmm10, %xmm3
	sha256msg2 %xmm2, %xmm3
	sha256msg1 %xmm2, %xmm1
	movdqa	96(%rsi), %xmm0
	paddd	%xmm3, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm3, %xmm10
	palignr	$4, %xmm2, %xmm10
	paddd	%xmm10, %xmm4
	sha256msg2 %xmm3, %xmm4
	sha256msg1 %xmm3, %xmm2
	movdqa	112(%rsi), %xmm0
	paddd	%xmm4, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm4, %xmm10
	palignr	$4, %xmm3, %xmm10
	paddd	%xmm10, %xmm1
	sha256msg2 %xmm4, %xmm1
	sha256msg1 %xmm4, %xmm3
	movdqa	128(%rsi), %xmm0
	paddd	%xmm1, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm1, %xmm10
	palignr	$4, %xmm4, %xmm10
	paddd	%xmm10, %xmm2
	sha256msg2 %xmm1, %xmm2
	sha256msg1 %xmm1, %xmm4
	movdqa	144(%rsi), %xmm0
	paddd	%xmm2, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm2, %xmm10
	palignr	$4, %xmm1, %xmm10
	paddd	%xmm10, %xmm3
	sha256msg2 %xmm2, %xmm3
	sha256msg1 %xmm2, %xmm1
	movdqa	160(%rsi), %xmm0
	paddd	%xmm3, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm3, %xmm10
	palignr	$4, %xmm2, %xmm10
	paddd	%xmm10, %xmm4
	sha256msg2 %xmm3, %xmm4
	sha256msg1 %xmm3, %xmm2
	movdqa	176(%rsi), %xmm0
	paddd	%xmm4, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm4, %xmm10
	palignr	$4, %xmm3, %xmm10
	paddd	%xmm10, %xmm1
	sha256msg2 %xmm4, %xmm1
	sha256msg1 %xmm4, %xmm3
	movdqa	192(%rsi), %xmm0
	paddd	%xmm1, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm1, %xmm10
	palignr	$4, %xmm4, %xmm10
	paddd	%xmm10, %xmm2
	sha256msg2 %xmm1, %xmm2
	sha256msg1 %xmm1, %xmm4
	movdqa	208(%rsi), %xmm0
	paddd	%xmm2, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm2, %xmm10
	palignr	$4, %xmm1, %xmm10
	paddd	%xmm10, %xmm3
	sha256msg2 %xmm2, %xmm3
	movdqa	224(%rsi), %xmm0
	paddd	%xmm3, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	movdqa	%xmm3, %xmm10
	palignr	$4, %xmm2, %xmm10
	paddd	%xmm10, %xmm4
	sha256msg2 %xmm3, %xmm4
	movdqa	240(%rsi), %xmm0
	paddd	%xmm4, %xmm0
	sha256rnds2 %xmm5, %xmm6
	pshufd	$0xe, %xmm0, %xmm0
	sha256rnds2 %xmm6, %xmm5
	paddd %xmm7, %xmm5
	paddd %xmm8, %xmm6
	add	$64, %rcx
	dec	%rdx
	jnz	.Loop
	movdqa	%xmm6, %xmm10
	punpckhqdq %xmm5, %xmm6
	punpcklqdq %xmm5, %xmm10
	pshufd	$0x1b, %xmm6, %xmm6
	pshufd	$0x1b, %xmm10, %xmm10
	movups	%xmm6, 0(%rdi)
	movups	%xmm10, 16(%rdi)
.Lend:
	mov	%rcx, %rax
#ifndef __APPLE__
#ifdef _WIN32
      pop	%rsi
      movdqa	64(%rsp), %xmm10
      movdqa	48(%rsp), %xmm9
      movdqa	32(%rsp), %xmm8
      movdqa	16(%rsp), %xmm7
      movdqa	(%rsp), %xmm6
      add	$80, %rsp
    pop	%rdi
#endif /* _WIN32 */
#endif /* ! __APPLE__ */
	ret
#ifndef __APPLE__
#ifndef _WIN32
.size _nettle_sha256_compress_n_sha_ni, . - _nettle_sha256_compress_n_sha_ni
.section .note.GNU-stack,"",%progbits
#endif /* ! _WIN32 */
#endif /* ! __APPLE__ */
