/* Generated from GNU Nettle arm64/crypto/sha256-compress-n.asm by sha256.sh.
   Copyright (C) 2021 Mamone Tarsha

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
.arch armv8-a+crypto
.text
    .text
    .align 4
#ifndef __APPLE__
.globl _nettle_sha256_compress_n_arm64
.type _nettle_sha256_compress_n_arm64,%function
_nettle_sha256_compress_n_arm64:
#else /* __APPLE__ */
.globl __nettle_sha256_compress_n_arm64
__nettle_sha256_compress_n_arm64:
#endif /* __APPLE__ */
    cbz            x2, .Lend
    ld1            {v4.4s,v5.4s},[x0]
.Loop:
    mov            v16.16b,v4.16b
    mov            v17.16b,v5.16b
    ld1            {v0.16b,v1.16b,v2.16b,v3.16b},[x3],#64
    rev32          v0.16b,v0.16b
    rev32          v1.16b,v1.16b
    rev32          v2.16b,v2.16b
    rev32          v3.16b,v3.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v0.4s,v6.4s
    sha256su0      v0.4s,v1.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q16,v6.4s
    sha256su1      v0.4s,v2.4s,v3.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v1.4s,v6.4s
    sha256su0      v1.4s,v2.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v1.4s,v3.4s,v0.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v2.4s,v6.4s
    sha256su0      v2.4s,v3.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v2.4s,v0.4s,v1.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v3.4s,v6.4s
    sha256su0      v3.4s,v0.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v3.4s,v1.4s,v2.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v0.4s,v6.4s
    sha256su0      v0.4s,v1.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v0.4s,v2.4s,v3.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v1.4s,v6.4s
    sha256su0      v1.4s,v2.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v1.4s,v3.4s,v0.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v2.4s,v6.4s
    sha256su0      v2.4s,v3.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v2.4s,v0.4s,v1.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v3.4s,v6.4s
    sha256su0      v3.4s,v0.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v3.4s,v1.4s,v2.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v0.4s,v6.4s
    sha256su0      v0.4s,v1.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v0.4s,v2.4s,v3.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v1.4s,v6.4s
    sha256su0      v1.4s,v2.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v1.4s,v3.4s,v0.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v2.4s,v6.4s
    sha256su0      v2.4s,v3.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v2.4s,v0.4s,v1.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v3.4s,v6.4s
    sha256su0      v3.4s,v0.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    sha256su1      v3.4s,v1.4s,v2.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v0.4s,v6.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v1.4s,v6.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1],#16
    add            v6.4s,v2.4s,v6.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    mov            v7.16b,v4.16b
    ld1            {v6.4s},[x1]
    add            v6.4s,v3.4s,v6.4s
    sha256h        q4,q5,v6.4s
    sha256h2       q5,q7,v6.4s
    add            v4.4s,v4.4s,v16.4s
    add            v5.4s,v5.4s,v17.4s
    subs           x2, x2, #1
    sub            x1, x1, #240
    b.ne           .Loop
    st1            {v4.4s,v5.4s},[x0]
.Lend:
    mov            x0, x3
    ret
#ifndef __APPLE__
.size _nettle_sha256_compress_n_arm64, . - _nettle_sha256_compress_n_arm64
.section .note.GNU-stack,"",%progbits
#endif /* ! __APPLE__ */
