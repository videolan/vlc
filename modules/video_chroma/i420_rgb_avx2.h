/*****************************************************************************
 * i420_rgb_avx2.h: AVX2 YUV transformation assembly
 *****************************************************************************
 * Copyright (C) 1999-2012, 2019 VLC authors and VideoLAN
 *
 * Authors: Damien Fouilleul <damienf@videolan.org>
 *          Lyndon Brown <jnqnfe@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#if defined(CAN_COMPILE_AVX2)

/* AVX2 assembly */

#define AVX2_CALL(AVX2_INSTRUCTIONS)    \
    do {                                \
    __asm__ __volatile__(               \
        "p2align 3 \n\t"                \
        AVX2_INSTRUCTIONS               \
        :                               \
        : [y]"r"(p_y), [u]"r"(p_u),     \
          [v]"r"(p_v), [b]"r"(p_buffer) \
        : "eax", "ymm0", "ymm1", "ymm2", "ymm3", \
                 "ymm4", "ymm5", "ymm6", "ymm7" ); \
    } while(0)

#define AVX2_END  __asm__ __volatile__ ( "sfence" ::: "memory" )

#define AVX2_INIT_16_ALIGNED "                                                \n\
vmovdqa     xmm0, [[u]]      ; Load 16 Cb into lower half     ... u2  u1  u0  \n\
vmovdqa     xmm1, [[v]]      ; Load 16 Cr into lower half     ... v2  v1  v0  \n\
vmovdqa     ymm6, [[y]]      ; Load 32 Y                      ... y2  y1  y0  \n\
"

#define AVX2_INIT_16_UNALIGNED "                                              \n\
vmovdqu     xmm0, [[u]]      ; Load 16 Cb into lower half     ... u2  u1  u0  \n\
vmovdqu     xmm1, [[v]]      ; Load 16 Cr into lower half     ... v2  v1  v0  \n\
vmovdqu     ymm6, [[y]]      ; Load 32 Y                      ... y2  y1  y0  \n\
prefetchnta [[b]]            ; Tell CPU not to cache output RGB data          \n\
"

#define AVX2_INIT_32_ALIGNED "                                                \n\
vmovdqa     xmm0, [[u]]      ; Load 16 Cb into lower half     ... u2  u1  u0  \n\
vmovdqa     xmm1, [[v]]      ; Load 16 Cr into lower half     ... v2  v1  v0  \n\
vmovdqa     ymm6, [[y]]      ; Load 32 Y                      ... y2  y1  y0  \n\
"

#define AVX2_INIT_32_UNALIGNED "                                              \n\
vmovdqu     xmm0, [[u]]      ; Load 16 Cb into lower half     ... u2  u1  u0  \n\
vmovdqu     xmm1, [[v]]      ; Load 16 Cr into lower half     ... v2  v1  v0  \n\
vmovdqu     ymm6, [[y]]      ; Load 32 Y                      ... y2  y1  y0  \n\
prefetchnta [[b]]            ; Tell CPU not to cache output RGB data          \n\
"

#define AVX2_YUV_MUL "                                                        \n\
; convert the chroma part                                                     \n\
vpmovzxbw  ymm0, xmm0        ; Zero extend u                 ... 00 u1 00 u0  \n\
vpmovzxbw  ymm1, xmm1        ; Zero extend v                 ... 00 v1 00 v0  \n\
mov        eax, 0x00800080   ;                                                \n\
vmovd      ymm5, eax         ;                                                \n\
vpshufd    ymm5, ymm5, 0     ; Set ymm5 to                   ... 00 80 00 80  \n\
vpsubsw    ymm0, ymm0, ymm5  ; Cb -= 128                                      \n\
vpsubsw    ymm1, ymm1, ymm5  ; Cr -= 128                                      \n\
vpsllw     ymm0, ymm0, 3     ; Promote precision                              \n\
vpsllw     ymm1, ymm1, 3     ; Promote precision                              \n\
mov        eax, 0xf37df37d   ;                                                \n\
vmovd      ymm4, eax         ;                                                \n\
vpshufd    ymm4, ymm4, 0     ; Set ymm4 to                   ... f3 7d f3 7d  \n\
vpmulhw    ymm2, ymm0, ymm4  ; Mul Cb with green coeff -> Cb green            \n\
mov        eax, 0xe5fce5fc   ;                                                \n\
vmovd      ymm5, eax         ;                                                \n\
vpshufd    ymm5, ymm5, 0     ; Set ymm5 to                   ... e5 fc e5 fc  \n\
vpmulhw    ymm3, ymm1, ymm5  ; Mul Cr with green coeff -> Cr green            \n\
mov        eax, 0x40934093   ;                                                \n\
vmovd      ymm4, eax         ;                                                \n\
vpshufd    ymm4, ymm4, 0     ; Set ymm4 to                   ... 40 93 40 93  \n\
vpmulhw    ymm0, ymm0, ymm4  ; Mul Cb -> Cblue               ... 00 b1 00 b0  \n\
mov        eax, 0x33123312   ;                                                \n\
vmovd      ymm5, eax         ;                                                \n\
vpshufd    ymm5, ymm5, 0     ; Set ymm5 to                   ... 33 12 33 12  \n\
vpmulhw    ymm1, ymm1, ymm5  ; Mul Cr -> Cred                ... 00 r1 00 r0  \n\
vpaddsw    ymm2, ymm2, ymm3  ; Cb green + Cr green -> Cgreen                  \n\
                                                                              \n\
; convert the luma part                                                       \n\
mov        eax, 0x10101010   ;                                                \n\
vmovd      ymm5, eax         ;                                                \n\
vpshufd    ymm5, ymm5, 0     ; Set ymm5 to                   ... 10 10 10 10  \n\
vpsubusb   ymm6, ymm6, ymm5  ; Y -= 16                                        \n\
vpsrlw     ymm7, ymm6, 8     ; get Y odd                     ... 00 y3 00 y1  \n\
mov        eax, 0x00ff00ff   ;                                                \n\
vmovd      ymm5, eax         ;                                                \n\
vpshufd    ymm5, ymm5, 0     ; set ymm5 to                   ... 00 ff 00 ff  \n\
vpand      ymm6, ymm6, ymm5  ; get Y even                    ... 00 y2 00 y0  \n\
vpsllw     ymm6, ymm6, 3     ; Promote precision                              \n\
vpsllw     ymm7, ymm7, 3     ; Promote precision                              \n\
mov        eax, 0x253f253f   ;                                                \n\
vmovd      ymm5, eax         ;                                                \n\
vpshufd    ymm5, ymm5, 0     ; set ymm5 to                   ... 25 3f 25 3f  \n\
vpmulhw    ymm6, ymm6, ymm5  ; Mul 16 Y even                 ... 00 y2 00 y0  \n\
vpmulhw    ymm7, ymm7, ymm5  ; Mul 16 Y odd                  ... 00 y3 00 y1  \n\
"

#define AVX2_YUV_ADD "                                                        \n\
; Do horizontal and vertical scaling                                          \n\
vpaddsw    ymm3, ymm0, ymm7  ; Y odd  + Cblue                ... 00 B3 00 B1  \n\
vpaddsw    ymm0, ymm0, ymm6  ; Y even + Cblue                ... 00 B2 00 B0  \n\
vpaddsw    ymm4, ymm1, ymm7  ; Y odd  + Cred                 ... 00 R3 00 R1  \n\
vpaddsw    ymm1, ymm1, ymm6  ; Y even + Cred                 ... 00 R2 00 R0  \n\
vpaddsw    ymm5, ymm2, ymm7  ; Y odd  + Cgreen               ... 00 G3 00 G1  \n\
vpaddsw    ymm2, ymm2, ymm6  ; Y even + Cgreen               ... 00 G2 00 G0  \n\
                                                                              \n\
; Limit RGB even to 0..255                                                    \n\
vpackuswb  ymm0, ymm0, ymm0  ; Saturate and pack   ... B4 B2 B0 ... B4 B2 B0  \n\
vpackuswb  ymm1, ymm1, ymm1  ; Saturate and pack   ... R4 R2 R0 ... R4 R2 R0  \n\
vpackuswb  ymm2, ymm2, ymm2  ; Saturate and pack   ... G4 G2 G0 ... G4 G2 G0  \n\
                                                                              \n\
; Limit RGB odd to 0..255                                                     \n\
vpackuswb  ymm3, ymm3, ymm3  ; Saturate and pack   ... B5 B3 B1 ... B5 B3 B1  \n\
vpackuswb  ymm4, ymm4, ymm4  ; Saturate and pack   ... R5 R3 R1 ... R5 R3 R1  \n\
vpackuswb  ymm5, ymm5, ymm5  ; Saturate and pack   ... G5 G3 G1 ... G5 G3 G1  \n\
                                                                              \n\
; Interleave RGB even and odd                                                 \n\
vpunpcklbw ymm0, ymm0, ymm3  ;                                  ... B2 B1 B0  \n\
vpunpcklbw ymm1, ymm1, ymm4  ;                                  ... R2 R1 R0  \n\
vpunpcklbw ymm2, ymm2, ymm5  ;                                  ... G2 G1 G0  \n\
"

#define AVX2_UNPACK_15_ALIGNED " ; Note, much of this shows bit patterns (of a pair of bytes) \n\
; mask unneeded bits off                                              \n\
mov        eax, 0xf8f8f8f8   ;                                        \n\
vmovd      ymm5, eax         ;                                        \n\
vpshufd    ymm5, ymm5, 0     ; set ymm5 to     f8 f8 ... f8 f8 f8 f8  \n\
vpand      ymm0, ymm0, ymm5  ; b7b6b5b4 b3______ b7b6b5b4 b3______    \n\
vpsrlw     ymm0, ymm0, 3     ; ______b7 b6b5b4b3 ______b7 b6b5b4b3    \n\
vpand      ymm2, ymm2, ymm5  ; g7g6g5g4 g3______ g7g6g5g4 g3______    \n\
vpand      ymm1, ymm1, ymm5  ; r7r6r5r4 r3______ r7r6r5r4 r3______    \n\
vpsrlw     ymm1, ymm1, 1     ; __r7r6r5 r4r3____ __r7r6r5 r4r3____    \n\
                                                                      \n\
; pack the 3 separate RGB bytes into 2 for pixels 0-15                \n\
vpmovzxbw  ymm5, xmm2        ; ________ ________ g7g6g5g4 g3______    \n\
vpunpcklbw ymm4, ymm0, ymm1  ; __r7r6r5 r4r3____ ______b7 b6b5b4b3    \n\
vpsllw     ymm5, ymm5, 2     ; ________ ____g7g6 g5g4g3__ ________    \n\
vpor       ymm4, ymm4, ymm5  ; __r7r6r5 r4r3g7g6 g5g4g3b7 b6b5b4b3    \n\
vmovntdq   [[b]], ymm4       ; store pixels 0-15                      \n\
                                                                      \n\
; pack the 3 separate RGB bytes into 2 for pixels 16-31               \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                               \n\
vpunpckhbw ymm7, ymm2, ymm3  ; ________ ________ g7g6g5g4 g3______    \n\
vpunpckhbw ymm6, ymm0, ymm1  ; __r7r6r5 r4r3____ ______b7 b6b5b4b3    \n\
vpsllw     ymm7, ymm7, 2     ; ________ ____g7g6 g5g4g3__ ________    \n\
vpor       ymm6, ymm6, ymm7  ; __r7r6r5 r4r3g7g6 g5g4g3b7 b6b5b4b3    \n\
vmovntdq   [[b]+32], ymm6    ; store pixels 16-31                     \n\
"

#define AVX2_UNPACK_15_UNALIGNED " ; Note, much of this shows bit patterns (of a pair of bytes) \n\
; mask unneeded bits off                                              \n\
mov        eax, 0xf8f8f8f8   ;                                        \n\
vmovd      ymm5, eax         ;                                        \n\
vpshufd    ymm5, ymm5, 0     ; set ymm5 to     f8 f8 ... f8 f8 f8 f8  \n\
vpand      ymm0, ymm0, ymm5  ; b7b6b5b4 b3______ b7b6b5b4 b3______    \n\
vpsrlw     ymm0, ymm0, 3     ; ______b7 b6b5b4b3 ______b7 b6b5b4b3    \n\
vpand      ymm2, ymm2, ymm5  ; g7g6g5g4 g3______ g7g6g5g4 g3______    \n\
vpand      ymm1, ymm1, ymm5  ; r7r6r5r4 r3______ r7r6r5r4 r3______    \n\
vpsrlw     ymm1, ymm1, 1     ; __r7r6r5 r4r3____ __r7r6r5 r4r3____    \n\
                                                                      \n\
; pack the 3 separate RGB bytes into 2 for pixels 0-15                \n\
vpmovzxbw  ymm5, xmm2        ; ________ ________ g7g6g5g4 g3______    \n\
vpunpcklbw ymm4, ymm0, ymm1  ; __r7r6r5 r4r3____ ______b7 b6b5b4b3    \n\
vpsllw     ymm5, ymm5, 2     ; ________ ____g7g6 g5g4g3__ ________    \n\
vpor       ymm4, ymm4, ymm5  ; __r7r6r5 r4r3g7g6 g5g4g3b7 b6b5b4b3    \n\
vmovdqu    [[b]], ymm4       ; store pixels 0-15                      \n\
                                                                      \n\
; pack the 3 separate RGB bytes into 2 for pixels 16-31               \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                               \n\
vpunpckhbw ymm7, ymm2, ymm3  ; ________ ________ g7g6g5g4 g3______    \n\
vpunpckhbw ymm6, ymm0, ymm1  ; __r7r6r5 r4r3____ ______b7 b6b5b4b3    \n\
vpsllw     ymm7, ymm7, 2     ; ________ ____g7g6 g5g4g3__ ________    \n\
vpor       ymm6, ymm6, ymm7  ; __r7r6r5 r4r3g7g6 g5g4g3b7 b6b5b4b3    \n\
vmovdqu    [[b]+32], ymm6    ; store pixels 16-31                     \n\
"

#define AVX2_UNPACK_16_ALIGNED " ; Note, much of this shows bit patterns (of a pair of bytes) \n\
; mask unneeded bits off                                              \n\
mov        eax, 0xf8f8f8f8   ;                                        \n\
vmovd      ymm5, eax         ;                                        \n\
vpshufd    ymm5, ymm5, 0     ; set ymm5 to     f8 f8 ... f8 f8 f8 f8  \n\
vpand      ymm0, ymm0, ymm5  ; b7b6b5b4 b3______ b7b6b5b4 b3______    \n\
vpand      ymm1, ymm1, ymm5  ; r7r6r5r4 r3______ r7r6r5r4 r3______    \n\
mov        eax, 0xfcfcfcfc   ;                                        \n\
vmovd      ymm6, eax         ;                                        \n\
vpshufd    ymm6, ymm6, 0     ; set ymm5 to     fc fc ... fc fc fc fc  \n\
vpand      ymm2, ymm2, ymm6  ; g7g6g5g4 g3g2____ g7g6g5g4 g3g2____    \n\
vpsrlw     ymm0, ymm0, 3     ; ______b7 b6b5b4b3 ______b7 b6b5b4b3    \n\
                                                                      \n\
; pack the 3 separate RGB bytes into 2 for pixels 0-15                \n\
vpmovzxbw  ymm5, xmm2        ; ________ ________ g7g6g5g4 g3g2____    \n\
vpunpcklbw ymm4, ymm0, ymm1  ; r7r6r5r4 r3______ ______b7 b6b5b4b3    \n\
vpsllw     ymm5, ymm5, 3     ; ________ __g7g6g5 g4g3g2__ ________    \n\
vpor       ymm4, ymm4, ymm5  ; r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3    \n\
vmovntdq   [[b]], ymm4       ; store pixesl 0-15                      \n\
                                                                      \n\
; pack the 3 separate RGB bytes into 2 for pixels 16-31               \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                               \n\
vpunpckhbw ymm7, ymm2, ymm3  ; ________ ________ g7g6g5g4 g3g2____    \n\
vpunpckhbw ymm6, ymm0, ymm1  ; r7r6r5r4 r3______ ______b7 b6b5b4b3    \n\
vpsllw     ymm7, ymm7, 3     ; ________ __g7g6g5 g4g3g2__ ________    \n\
vpor       ymm6, ymm6, ymm7  ; r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3    \n\
vmovntdq   [[b]+32], ymm6    ; store pixels 16-31                     \n\
"

#define AVX2_UNPACK_16_UNALIGNED " ; Note, much of this shows bit patterns (of a pair of bytes) \n\
; mask unneeded bits off                                              \n\
mov        eax, 0xf8f8f8f8   ;                                        \n\
vmovd      ymm5, eax         ;                                        \n\
vpshufd    ymm5, ymm5, 0     ; set ymm5 to     f8 f8 ... f8 f8 f8 f8  \n\
vpand      ymm0, ymm0, ymm5  ; b7b6b5b4 b3______ b7b6b5b4 b3______    \n\
vpand      ymm1, ymm1, ymm5  ; r7r6r5r4 r3______ r7r6r5r4 r3______    \n\
mov        eax, 0xfcfcfcfc   ;                                        \n\
vmovd      ymm6, eax         ;                                        \n\
vpshufd    ymm6, ymm6, 0     ; set ymm5 to     fc fc ... fc fc fc fc  \n\
vpand      ymm2, ymm2, ymm6  ; g7g6g5g4 g3g2____ g7g6g5g4 g3g2____    \n\
vpsrlw     ymm0, ymm0, 3     ; ______b7 b6b5b4b3 ______b7 b6b5b4b3    \n\
                                                                      \n\
; pack the 3 separate RGB bytes into 2 for pixels 0-15                \n\
vpmovzxbw  ymm5, xmm2        ; ________ ________ g7g6g5g4 g3g2____    \n\
vpunpcklbw ymm4, ymm0, ymm1  ; r7r6r5r4 r3______ ______b7 b6b5b4b3    \n\
vpsllw     ymm5, ymm5, 3     ; ________ __g7g6g5 g4g3g2__ ________    \n\
vpor       ymm4, ymm4, ymm5  ; r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3    \n\
vmovdqu    [[b]], ymm4       ; store pixesl 0-15                      \n\
                                                                      \n\
; pack the 3 separate RGB bytes into 2 for pixels 16-31               \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                               \n\
vpunpckhbw ymm7, ymm2, ymm3  ; ________ ________ g7g6g5g4 g3g2____    \n\
vpunpckhbw ymm6, ymm0, ymm1  ; r7r6r5r4 r3______ ______b7 b6b5b4b3    \n\
vpsllw     ymm7, ymm7, 3     ; ________ __g7g6g5 g4g3g2__ ________    \n\
vpor       ymm6, ymm6, ymm7  ; r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3    \n\
vmovdqu    [[b]+32], ymm6    ; store pixels 16-31                     \n\
"

#define AVX2_UNPACK_32_ARGB_ALIGNED "                                     \n\
vpxor      ymm3, ymm3, ymm3  ; zero ymm3                                  \n\
vpunpcklbw ymm4, ymm0, ymm2  ; Interleave low b,g    ...  G1  B1  G0  B0  \n\
vpmovzxbw  ymm5, xmm1        ; Zero extend low r     ...  00  R1  00  R0  \n\
vpunpcklwd ymm6, ymm4, ymm5  ; Interleave b,g,r,0    ...  00  R0  G0  B0  \n\
vmovntdq   [[b]], ymm6       ; Store ARGB7 ... ARGB0                      \n\
vpunpckhwd ymm7, ymm4, ymm5  ; Interleave b,g,r,0    ...  00  R8  G8  B8  \n\
vmovntdq   [[b]+32], ymm7    ; Store ARGB15 ... ARGB8                     \n\
vpunpckhbw ymm0, ymm0, ymm2  ; Interleave high b,g   ... G17 B17 G16 B16  \n\
vpunpckhbw ymm1, ymm1, ymm3  ; Interleave high r,0   ...  00 R17  00 R16  \n\
vpunpcklwd ymm2, ymm0, ymm1  ; Interleave b,g,r,0    ...  00 R16 G16 B16  \n\
vmovntdq   [[b]+64], ymm2    ; Store ARGB23 ... ARGB16                    \n\
vpunpckhwd ymm3, ymm0, ymm1  ; Interleave b,g,r,0    ...  00 R24 G24 B24  \n\
vmovntdq   [[b]+96], ymm3    ; Store ARGB31 ... ARGB24                    \n\
"

#define AVX2_UNPACK_32_ARGB_UNALIGNED "                                   \n\
vpxor      ymm3, ymm3, ymm3  ; zero ymm3                                  \n\
vpunpcklbw ymm4, ymm0, ymm2  ; Interleave low b,g    ...  G1  B1  G0  B0  \n\
vpmovzxbw  ymm5, xmm1        ; Zero extend low r     ...  00  R1  00  R0  \n\
vpunpcklwd ymm6, ymm4, ymm5  ; Interleave b,g,r,0    ...  00  R0  G0  B0  \n\
vmovdqu    [[b]], ymm6       ; Store ARGB7 ... ARGB0                      \n\
vpunpckhwd ymm7, ymm4, ymm5  ; Interleave b,g,r,0    ...  00  R8  G8  B8  \n\
vmovdqu    [[b]+32], ymm7    ; Store ARGB15 ... ARGB8                     \n\
vpunpckhbw ymm0, ymm0, ymm2  ; Interleave high b,g   ... G17 B17 G16 B16  \n\
vpunpckhbw ymm1, ymm1, ymm3  ; Interleave high r,0   ...  00 R17  00 R16  \n\
vpunpcklwd ymm2, ymm0, ymm1  ; Interleave b,g,r,0    ...  00 R16 G16 B16  \n\
vmovdqu    [[b]+64], ymm2    ; Store ARGB23 ... ARGB16                    \n\
vpunpckhwd ymm3, ymm0, ymm1  ; Interleave b,g,r,0    ...  00 R24 G24 B24  \n\
vmovdqu    [[b]+96], ymm3    ; Store ARGB31 ... ARGB24                    \n\
"

#define AVX2_UNPACK_32_RGBA_ALIGNED "                                     \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                                   \n\
vpunpcklbw ymm4, ymm2, ymm1  ; Interleave low g,r    ...  R1  G1  R0  G0  \n\
vpunpcklbw ymm5, ymm3, ymm0  ; Interleave low 0,b    ...  B1  00  B0  00  \n\
vpunpcklwd ymm6, ymm5, ymm4  ; Interleave 0,b,g,r    ...  R0  B0  G0  00  \n\
vmovntdq   [[b]], ymm6       ; Store RGBA7 ... RGBA0                      \n\
vpunpckhwd ymm7, ymm5, ymm4  ; Interleave 0,b,g,r    ...  R8  G8  B8  00  \n\
vmovntdq   [[b]+32], ymm7    ; Store RGBA15 ... RGBA8                     \n\
vpunpckhbw ymm1, ymm2, ymm1  ; Interleave high g,r   ... R17 G17 R16 G16  \n\
vpunpckhbw ymm0, ymm3, ymm0  ; Interleave high 0,b   ... B17  00 B16  00  \n\
vpunpcklwd ymm2, ymm0, ymm1  ; Interleave 0,b,g,r    ... R16 G16 B16  00  \n\
vmovntdq   [[b]+64], ymm2    ; Store RGBA23 ... RGBA16                    \n\
vpunpckhwd ymm3, ymm0, ymm1  ; Interleave 0,b,g,r    ... R24 G24 B24  00  \n\
vmovntdq   [[b]+96], ymm3    ; Store RGBA31 ... RGBA24                    \n\
"

#define AVX2_UNPACK_32_RGBA_UNALIGNED "                                   \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                                   \n\
vpunpcklbw ymm4, ymm2, ymm1  ; Interleave low g,r    ...  R1  G1  R0  G0  \n\
vpunpcklbw ymm5, ymm3, ymm0  ; Interleave low 0,b    ...  B1  00  B0  00  \n\
vpunpcklwd ymm6, ymm5, ymm4  ; Interleave 0,b,g,r    ...  R0  B0  G0  00  \n\
vmovdqu    [[b]], ymm6       ; Store RGBA7 ... RGBA0                      \n\
vpunpckhwd ymm7, ymm5, ymm4  ; Interleave 0,b,g,r    ...  R8  G8  B8  00  \n\
vmovdqu    [[b]+32], ymm7    ; Store RGBA15 ... RGBA8                     \n\
vpunpckhbw ymm1, ymm2, ymm1  ; Interleave high g,r   ... R17 G17 R16 G16  \n\
vpunpckhbw ymm0, ymm3, ymm0  ; Interleave high 0,b   ... B17  00 B16  00  \n\
vpunpcklwd ymm2, ymm0, ymm1  ; Interleave 0,b,g,r    ... R16 G16 B16  00  \n\
vmovdqu    [[b]+64], ymm2    ; Store RGBA23 ... RGBA16                    \n\
vpunpckhwd ymm3, ymm0, ymm1  ; Interleave 0,b,g,r    ... R24 G24 B24  00  \n\
vmovdqu    [[b]+96], ymm3    ; Store RGBA31 ... RGBA24                    \n\
"

#define AVX2_UNPACK_32_BGRA_ALIGNED "                                     \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                                   \n\
vpunpcklbw ymm4, ymm2, ymm0  ; Interleave low g,b    ...  B1  G1  B0  G0  \n\
vpunpcklbw ymm5, ymm3, ymm1  ; Interleave low 0,r    ...  R1  00  R0  00  \n\
vpunpcklwd ymm6, ymm5, ymm4  ; Interleave 0,r,g,b    ...  B0  G0  R0  00  \n\
vmovntdq   [[b]], ymm6       ; Store BGRA7 ... BGRA0                      \n\
vpunpckhwd ymm7, ymm5, ymm4  ; Interleave 0,r,g,b    ...  B8  G8  R8  00  \n\
vmovntdq   [[b]+32], ymm7    ; Store BGRA15 ... BGRA8                     \n\
vpunpckhbw ymm0, ymm2, ymm0  ; Interleave high g,b   ... B17 G17 B16 G16  \n\
vpunpckhbw ymm1, ymm6, ymm1  ; Interleave high 0,r   ... R17  00 R16  00  \n\
vpunpcklwd ymm2, ymm1, ymm0  ; Interleave 0,r,g,b    ... B16 G16 R16  00  \n\
vmovntdq   [[b]+64], ymm2    ; Store BGRA23 ... BGRA16                    \n\
vpunpckhwd ymm3, ymm1, ymm0  ; Interleave 0,r,g,b    ... B24 G24 R24  00  \n\
vmovntdq   [[b]+96], ymm3    ; Store BGRA31 ... BGRA24                    \n\
"

#define AVX2_UNPACK_32_BGRA_UNALIGNED "                                   \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                                   \n\
vpunpcklbw ymm4, ymm2, ymm0  ; Interleave low g,b    ...  B1  G1  B0  G0  \n\
vpunpcklbw ymm5, ymm3, ymm1  ; Interleave low 0,r    ...  R1  00  R0  00  \n\
vpunpcklwd ymm6, ymm5, ymm4  ; Interleave 0,r,g,b    ...  B0  G0  R0  00  \n\
vmovdqu    [[b]], ymm6       ; Store BGRA7 ... BGRA0                      \n\
vpunpckhwd ymm7, ymm5, ymm4  ; Interleave 0,r,g,b    ...  B8  G8  R8  00  \n\
vmovdqu    [[b]+32], ymm7    ; Store BGRA15 ... BGRA8                     \n\
vpunpckhbw ymm0, ymm2, ymm0  ; Interleave high g,b   ... B17 G17 B16 G16  \n\
vpunpckhbw ymm1, ymm6, ymm1  ; Interleave high 0,r   ... R17  00 R16  00  \n\
vpunpcklwd ymm2, ymm1, ymm0  ; Interleave 0,r,g,b    ... B16 G16 R16  00  \n\
vmovdqu    [[b]+64], ymm2    ; Store BGRA23 ... BGRA16                    \n\
vpunpckhwd ymm3, ymm1, ymm0  ; Interleave 0,r,g,b    ... B24 G24 R24  00  \n\
vmovdqu    [[b]+96], ymm3    ; Store BGRA31 ... BGRA24                    \n\
"

#define AVX2_UNPACK_32_ABGR_ALIGNED "                                     \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                                   \n\
vpunpcklbw ymm4, ymm1, ymm2  ; Interleave low r,g    ...  G1  R1  G0  R0  \n\
vpmovzxbw  ymm5, xmm0        ; Zero extend low b     ...  00  B1  00  B0  \n\
vpunpcklwd ymm6, ymm4, ymm5  ; Interleave r,g,b,0    ...  00  B0  G0  R0  \n\
vmovntdq   [[b]], ymm6       ; Store ABGR7 ... ABGR0                      \n\
vpunpckhwd ymm7, ymm4, ymm5  ; Interleave r,g,b,0    ...  00  B8  G8  R8  \n\
vmovntdq   [[b]+32], ymm7    ; Store ABGR15 ... ABGR8                     \n\
vpunpckhbw ymm1, ymm1, ymm2  ; Interleave high r,g   ... G17 R17 G16 R16  \n\
vpunpckhbw ymm0, ymm0, ymm3  ; Interleave high b,0   ...  00 B17  00 B16  \n\
vpunpcklwd ymm2, ymm1, ymm0  ; Interleave r,g,b,0    ...  00 B16 G16 R16  \n\
vmovntdq   [[b]+64], ymm2    ; Store ABGR23 ... ABGR16                    \n\
vpunpckhwd ymm3, ymm1, ymm0  ; Interleave r,g,b,0    ...  00 B24 G24 R24  \n\
vmovntdq   [[b]+96], ymm3    ; Store ABGR31 ... ABGR24                    \n\
"

#define AVX2_UNPACK_32_ABGR_UNALIGNED "                                   \n\
vpxor      ymm3, ymm3, ymm3  ; zero mm3                                   \n\
vpunpcklbw ymm4, ymm1, ymm2  ; Interleave low r,g    ...  G1  R1  G0  R0  \n\
vpmovzxbw  ymm5, xmm0        ; Zero extend low b     ...  00  B1  00  B0  \n\
vpunpcklwd ymm6, ymm4, ymm5  ; Interleave r,g,b,0    ...  00  B0  G0  R0  \n\
vmovdqu    [[b]], ymm6       ; Store ABGR7 ... ABGR0                      \n\
vpunpckhwd ymm7, ymm4, ymm5  ; Interleave r,g,b,0    ...  00  B8  G8  R8  \n\
vmovdqu    [[b]+32], ymm7    ; Store ABGR15 ... ABGR8                     \n\
vpunpckhbw ymm1, ymm1, ymm2  ; Interleave high r,g   ... G17 R17 G16 R16  \n\
vpunpckhbw ymm0, ymm0, ymm3  ; Interleave high b,0   ...  00 B17  00 B16  \n\
vpunpcklwd ymm2, ymm1, ymm0  ; Interleave r,g,b,0    ...  00 B16 G16 R16  \n\
vmovdqu    [[b]+64], ymm2    ; Store ABGR23 ... ABGR16                    \n\
vpunpckhwd ymm3, ymm1, ymm0  ; Interleave r,g,b,0    ...  00 B24 G24 R24  \n\
vmovdqu    [[b]+96], ymm3    ; Store ABGR31 ... ABGR24                    \n\
"

#elif defined(HAVE_AVX2_INTRINSICS)

/* AVX2 intrinsics */

#include <immintrin.h>

#define AVX2_CALL(AVX2_INSTRUCTIONS)        \
    do {                                    \
        __m256i ymm0, ymm1, ymm2, ymm3,     \
                ymm4, ymm5, ymm6, ymm7;     \
        AVX2_INSTRUCTIONS                   \
    } while(0)

#define AVX2_END  _mm_sfence()

#define AVX2_INIT_16_ALIGNED                       \
    ymm0 = _mm256_inserti128_si256(ymm0, *((__m128i*)p_u), 0); \
    ymm1 = _mm256_inserti128_si256(ymm1, *((__m128i*)p_v), 0); \
    ymm6 = _mm256_load_si256((__m256i *)p_y);

#define AVX2_INIT_16_UNALIGNED                     \
    ymm0 = _mm256_inserti128_si256(ymm0, *((__m128i*)p_u), 0); \
    ymm1 = _mm256_inserti128_si256(ymm1, *((__m128i*)p_v), 0); \
    ymm6 = _mm256_loadu_si256((__m256i *)p_y);     \
    _mm_prefetch(p_buffer, _MM_HINT_NTA);

#define AVX2_INIT_32_ALIGNED                       \
    ymm0 = _mm256_inserti128_si256(ymm0, *((__m128i*)p_u), 0); \
    ymm1 = _mm256_inserti128_si256(ymm1, *((__m128i*)p_v), 0); \
    ymm6 = _mm256_load_si256((__m256i *)p_y);

#define AVX2_INIT_32_UNALIGNED                     \
    ymm0 = _mm256_inserti128_si256(ymm0, *((__m128i*)p_u), 0); \
    ymm1 = _mm256_inserti128_si256(ymm1, *((__m128i*)p_v), 0); \
    ymm6 = _mm256_loadu_si256((__m256i *)p_y);     \
    _mm_prefetch(p_buffer, _MM_HINT_NTA);

#define AVX2_YUV_MUL                           \
    ymm0 = _mm256_cvtepu8_epi16(xmm0);         \
    ymm1 = _mm256_cvtepu8_epi16(xmm1);         \
    ymm5 = _mm256_set1_epi32(0x00800080UL);    \
    ymm0 = _mm256_subs_epi16(ymm0, ymm5);      \
    ymm1 = _mm256_subs_epi16(ymm1, ymm5);      \
    ymm0 = _mm256_slli_epi16(ymm0, 3);         \
    ymm1 = _mm256_slli_epi16(ymm1, 3);         \
    ymm4 = _mm256_set1_epi32(0xf37df37dUL);    \
    ymm2 = _mm256_mulhi_epi16(ymm0, ymm4);     \
    ymm5 = _mm256_set1_epi32(0xe5fce5fcUL);    \
    ymm3 = _mm256_mulhi_epi16(ymm1, ymm5);     \
    ymm4 = _mm256_set1_epi32(0x40934093UL);    \
    ymm0 = _mm256_mulhi_epi16(ymm0, ymm4);     \
    ymm5 = _mm256_set1_epi32(0x33123312UL);    \
    ymm1 = _mm256_mulhi_epi16(ymm1, ymm5);     \
    ymm2 = _mm256_adds_epi16(ymm2, ymm3);      \
    \
    ymm5 = _mm256_set1_epi32(0x10101010UL);    \
    ymm6 = _mm256_subs_epu8(ymm6, ymm5);       \
    ymm7 = _mm256_srli_epi16(ymm6, 8);         \
    ymm5 = _mm256_set1_epi32(0x00ff00ffUL);    \
    ymm6 = _mm256_and_si256(ymm6, ymm5);       \
    ymm6 = _mm256_slli_epi16(ymm6, 3);         \
    ymm7 = _mm256_slli_epi16(ymm7, 3);         \
    ymm5 = _mm256_set1_epi32(0x253f253fUL);    \
    ymm6 = _mm256_mulhi_epi16(ymm6, ymm5);     \
    ymm7 = _mm256_mulhi_epi16(ymm7, ymm5);

#define AVX2_YUV_ADD                           \
    ymm3 = _mm256_adds_epi16(ymm0, ymm7);      \
    ymm0 = _mm256_adds_epi16(ymm0, ymm6);      \
    ymm4 = _mm256_adds_epi16(ymm1, ymm7);      \
    ymm1 = _mm256_adds_epi16(ymm1, ymm6);      \
    ymm5 = _mm256_adds_epi16(ymm2, ymm7);      \
    ymm2 = _mm256_adds_epi16(ymm2, ymm6);      \
    \
    ymm0 = _mm256_packus_epi16(ymm0, ymm0);    \
    ymm1 = _mm256_packus_epi16(ymm1, ymm1);    \
    ymm2 = _mm256_packus_epi16(ymm2, ymm2);    \
    \
    ymm3 = _mm256_packus_epi16(ymm3, ymm3);    \
    ymm4 = _mm256_packus_epi16(ymm4, ymm4);    \
    ymm5 = _mm256_packus_epi16(ymm5, ymm5);    \
    \
    ymm0 = _mm256_unpacklo_epi8(ymm0, ymm3);   \
    ymm1 = _mm256_unpacklo_epi8(ymm1, ymm4);   \
    ymm2 = _mm256_unpacklo_epi8(ymm2, ymm5);

#define AVX2_UNPACK_15_ALIGNED                         \
    ymm5 = _mm256_set1_epi32(0xf8f8f8f8UL);            \
    ymm0 = _mm256_and_si256(ymm0, ymm5);               \
    ymm0 = _mm256_srli_epi16(ymm0, 3);                 \
    ymm2 = _mm256_and_si256(ymm2, ymm5);               \
    ymm1 = _mm256_and_si256(ymm1, ymm5);               \
    ymm1 = _mm256_srli_epi16(ymm1, 1);                 \
    \
    ymm5 = _mm256_cvtepu8_epi16(xmm2);                 \
    ymm4 = _mm256_unpacklo_epi8(ymm0, ymm1);           \
    ymm5 = _mm256_slli_epi16(ymm5, 2);                 \
    ymm4 = _mm256_or_si256(ymm4, ymm5);                \
    _mm256_stream_si256((__m256i*)p_buffer, ymm4);     \
    \
    ymm3 = _mm256_setzero_si256();                     \
    ymm7 = _mm256_unpackhi_epi8(ymm2, ymm3);           \
    ymm6 = _mm256_unpackhi_epi8(ymm0, ymm1);           \
    ymm7 = _mm256_slli_epi16(ymm7, 2);                 \
    ymm6 = _mm256_or_si256(ymm6, ymm7);                \
    _mm256_stream_si256((__m256i*)(p_buffer+16), ymm6);

#define AVX2_UNPACK_15_UNALIGNED                       \
    ymm5 = _mm256_set1_epi32(0xf8f8f8f8UL);            \
    ymm0 = _mm256_and_si256(ymm0, ymm5);               \
    ymm0 = _mm256_srli_epi16(ymm0, 3);                 \
    ymm2 = _mm256_and_si256(ymm2, ymm5);               \
    ymm1 = _mm256_and_si256(ymm1, ymm5);               \
    ymm1 = _mm256_srli_epi16(ymm1, 1);                 \
    \
    ymm5 = _mm256_cvtepu8_epi16(xmm2);                 \
    ymm4 = _mm256_unpacklo_epi8(ymm0, ymm1);           \
    ymm5 = _mm256_slli_epi16(ymm5, 2);                 \
    ymm4 = _mm256_or_si256(ymm4, ymm5);                \
    _mm256_storeu_si256((__m256i*)p_buffer, ymm4);     \
    \
    ymm3 = _mm256_setzero_si256();                     \
    ymm7 = _mm256_unpackhi_epi8(ymm2, ymm3);           \
    ymm6 = _mm256_unpackhi_epi8(ymm0, ymm1);           \
    ymm7 = _mm256_slli_epi16(ymm7, 2);                 \
    ymm6 = _mm256_or_si256(ymm6, ymm7);                \
    _mm256_storeu_si256((__m256i*)(p_buffer+16), ymm6);

#define AVX2_UNPACK_16_ALIGNED                         \
    ymm5 = _mm256_set1_epi32(0xf8f8f8f8UL);            \
    ymm0 = _mm256_and_si256(ymm0, ymm5);               \
    ymm1 = _mm256_and_si256(ymm1, ymm5);               \
    ymm6 = _mm256_set1_epi32(0xfcfcfcfcUL);            \
    ymm2 = _mm256_and_si256(ymm2, ymm6);               \
    ymm0 = _mm256_srli_epi16(ymm0, 3);                 \
    \
    ymm5 = _mm256_cvtepu8_epi16(xmm2);                 \
    ymm4 = _mm256_unpacklo_epi8(ymm0, ymm1);           \
    ymm5 = _mm256_slli_epi16(ymm5, 3);                 \
    ymm4 = _mm256_or_si256(ymm4, ymm5);                \
    _mm256_stream_si256((__m256i*)p_buffer, ymm4);     \
    \
    ymm3 = _mm256_setzero_si256();                     \
    ymm7 = _mm256_unpackhi_epi8(ymm2, ymm3);           \
    ymm6 = _mm256_unpackhi_epi8(ymm0, ymm1);           \
    ymm7 = _mm256_slli_epi16(ymm7, 3);                 \
    ymm6 = _mm256_or_si256(ymm6, ymm7);                \
    _mm256_stream_si256((__m256i*)(p_buffer+16), ymm6);

#define AVX2_UNPACK_16_UNALIGNED                       \
    ymm5 = _mm256_set1_epi32(0xf8f8f8f8UL);            \
    ymm0 = _mm256_and_si256(ymm0, ymm5);               \
    ymm1 = _mm256_and_si256(ymm1, ymm5);               \
    ymm6 = _mm256_set1_epi32(0xfcfcfcfcUL);            \
    ymm2 = _mm256_and_si256(ymm2, ymm6);               \
    ymm0 = _mm256_srli_epi16(ymm0, 3);                 \
    \
    ymm5 = _mm256_cvtepu8_epi16(xmm2);                 \
    ymm4 = _mm256_unpacklo_epi8(ymm0, ymm1);           \
    ymm5 = _mm256_slli_epi16(ymm5, 3);                 \
    ymm4 = _mm256_or_si256(ymm4, ymm5);                \
    _mm256_storeu_si256((__m256i*)p_buffer, ymm4);     \
    \
    ymm3 = _mm256_setzero_si256();                     \
    ymm7 = _mm256_unpackhi_epi8(ymm2, ymm3);           \
    ymm6 = _mm256_unpackhi_epi8(ymm0, ymm1);           \
    ymm7 = _mm256_slli_epi16(ymm7, 3);                 \
    ymm6 = _mm256_or_si256(ymm6, ymm7);                \
    _mm256_storeu_si256((__m256i*)(p_buffer+16), ymm6);

#define AVX2_UNPACK_32_ARGB_ALIGNED                    \
    ymm3 = _mm256_setzero_si256();                     \
    ymm4 = _mm256_unpacklo_epi8(ymm0, ymm2);           \
    ymm5 = _mm256_cvtepu8_epi16(xmm1);                 \
    ymm6 = _mm256_unpacklo_epi16(ymm4, ymm5);          \
    _mm256_stream_si256((__m256i*)(p_buffer), ymm6);   \
    ymm7 = _mm256_unpackhi_epi16(ymm4, ymm5);          \
    _mm256_stream_si256((__m256i*)(p_buffer+8), ymm7); \
    ymm0 = _mm256_unpackhi_epi8(ymm0, ymm2);           \
    ymm1 = _mm256_unpackhi_epi8(ymm1, ymm3);           \
    ymm2 = _mm256_unpacklo_epi16(ymm0, ymm1);          \
    _mm256_stream_si256((__m256i*)(p_buffer+16), ymm2);\
    ymm3 = _mm256_unpackhi_epi16(ymm0, ymm1);          \
    _mm256_stream_si256((__m256i*)(p_buffer+24), ymm3);

#define AVX2_UNPACK_32_ARGB_UNALIGNED                  \
    ymm3 = _mm256_setzero_si256();                     \
    ymm4 = _mm256_unpacklo_epi8(ymm0, ymm2);           \
    ymm5 = _mm256_cvtepu8_epi16(xmm1);                 \
    ymm6 = _mm256_unpacklo_epi16(ymm4, ymm5);          \
    _mm256_storeu_si256((__m256i*)(p_buffer), ymm6);   \
    ymm7 = _mm256_unpackhi_epi16(ymm4, ymm5);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+8), ymm7); \
    ymm0 = _mm256_unpackhi_epi8(ymm0, ymm2);           \
    ymm1 = _mm256_unpackhi_epi8(ymm1, ymm3);           \
    ymm2 = _mm256_unpacklo_epi16(ymm0, ymm1);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+16), ymm2);\
    ymm3 = _mm256_unpackhi_epi16(ymm0, ymm1);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+24), ymm3);

#define AVX2_UNPACK_32_RGBA_ALIGNED                    \
    ymm3 = _mm256_setzero_si256();                     \
    ymm4 = _mm256_unpacklo_epi8(ymm2, ymm1);           \
    ymm5 = _mm256_unpacklo_epi8(ymm3, ymm0);           \
    ymm6 = _mm256_unpacklo_epi16(ymm5, ymm4);          \
    _mm256_stream_si256((__m256i*)(p_buffer), ymm6);   \
    ymm7 = _mm256_unpackhi_epi16(ymm5, ymm4);          \
    _mm256_stream_si256((__m256i*)(p_buffer+8), ymm7); \
    ymm1 = _mm256_unpackhi_epi8(ymm2, ymm1);           \
    ymm0 = _mm256_unpackhi_epi8(ymm3, ymm0);           \
    ymm2 = _mm256_unpacklo_epi16(ymm0, ymm1);          \
    _mm256_stream_si256((__m256i*)(p_buffer+16), ymm2);\
    ymm3 = _mm256_unpackhi_epi16(ymm0, ymm1);          \
    _mm256_stream_si256((__m256i*)(p_buffer+24), ymm3);

#define AVX2_UNPACK_32_RGBA_UNALIGNED                  \
    ymm3 = _mm256_setzero_si256();                     \
    ymm4 = _mm256_unpacklo_epi8(ymm2, ymm1);           \
    ymm5 = _mm256_unpacklo_epi8(ymm3, ymm0);           \
    ymm6 = _mm256_unpacklo_epi16(ymm5, ymm4);          \
    _mm256_storeu_si256((__m256i*)(p_buffer), ymm6);   \
    ymm7 = _mm256_unpackhi_epi16(ymm5, ymm4);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+8), ymm7); \
    ymm1 = _mm256_unpackhi_epi8(ymm2, ymm1);           \
    ymm0 = _mm256_unpackhi_epi8(ymm3, ymm0);           \
    ymm2 = _mm256_unpacklo_epi16(ymm0, ymm1);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+16), ymm2);\
    ymm3 = _mm256_unpackhi_epi16(ymm0, ymm1);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+24), ymm3);

#define AVX2_UNPACK_32_BGRA_ALIGNED                    \
    ymm3 = _mm256_setzero_si256();                     \
    ymm4 = _mm256_unpacklo_epi8(ymm2, ymm0);           \
    ymm5 = _mm256_unpacklo_epi8(ymm3, ymm1);           \
    ymm6 = _mm256_unpacklo_epi16(ymm5, ymm4);          \
    _mm256_stream_si256((__m256i*)(p_buffer), ymm6);   \
    ymm7 = _mm256_unpackhi_epi16(ymm5, ymm4);          \
    _mm256_stream_si256((__m256i*)(p_buffer+8), ymm7); \
    ymm0 = _mm256_unpackhi_epi8(ymm2, ymm0);           \
    ymm1 = _mm256_unpackhi_epi8(ymm6, ymm1);           \
    ymm2 = _mm256_unpacklo_epi16(ymm1, ymm0);          \
    _mm256_stream_si256((__m256i*)(p_buffer+16), ymm2);\
    ymm3 = _mm256_unpackhi_epi16(ymm1, ymm0);          \
    _mm256_stream_si256((__m256i*)(p_buffer+24), ymm3);

#define AVX2_UNPACK_32_BGRA_UNALIGNED                  \
    ymm3 = _mm256_setzero_si256();                     \
    ymm4 = _mm256_unpacklo_epi8(ymm2, ymm0);           \
    ymm5 = _mm256_unpacklo_epi8(ymm3, ymm1);           \
    ymm6 = _mm256_unpacklo_epi16(ymm5, ymm4);          \
    _mm256_storeu_si256((__m256i*)(p_buffer), ymm6);   \
    ymm7 = _mm256_unpackhi_epi16(ymm5, ymm4);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+8), ymm7); \
    ymm0 = _mm256_unpackhi_epi8(ymm2, ymm0);           \
    ymm1 = _mm256_unpackhi_epi8(ymm6, ymm1);           \
    ymm2 = _mm256_unpacklo_epi16(ymm1, ymm0);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+16), ymm2);\
    ymm3 = _mm256_unpackhi_epi16(ymm1, ymm0);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+24), ymm3);

#define AVX2_UNPACK_32_ABGR_ALIGNED                    \
    ymm3 = _mm256_setzero_si256();                     \
    ymm4 = _mm256_unpacklo_epi8(ymm1, ymm2);           \
    ymm5 = _mm256_cvtepu8_epi16(xmm0);                 \
    ymm6 = _mm256_unpacklo_epi16(ymm4, ymm5);          \
    _mm256_stream_si256((__m256i*)(p_buffer), ymm6);   \
    ymm7 = _mm256_unpackhi_epi16(ymm4, ymm5);          \
    _mm256_stream_si256((__m256i*)(p_buffer+8), ymm7); \
    ymm1 = _mm256_unpackhi_epi8(ymm1, ymm2);           \
    ymm0 = _mm256_unpackhi_epi8(ymm0, ymm3);           \
    ymm2 = _mm256_unpacklo_epi16(ymm1, ymm0);          \
    _mm256_stream_si256((__m256i*)(p_buffer+16), ymm2);\
    ymm3 = _mm256_unpackhi_epi16(ymm1, ymm0);          \
    _mm256_stream_si256((__m256i*)(p_buffer+24), ymm3);

#define AVX2_UNPACK_32_ABGR_UNALIGNED                  \
    ymm3 = _mm256_setzero_si256();                     \
    ymm4 = _mm256_unpacklo_epi8(ymm1, ymm2);           \
    ymm5 = _mm256_cvtepu8_epi16(xmm0);                 \
    ymm6 = _mm256_unpacklo_epi16(ymm4, ymm5);          \
    _mm256_storeu_si256((__m256i*)(p_buffer), ymm6);   \
    ymm7 = _mm256_unpackhi_epi16(ymm4, ymm5);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+8), ymm7); \
    ymm1 = _mm256_unpackhi_epi8(ymm1, ymm2);           \
    ymm0 = _mm256_unpackhi_epi8(ymm0, ymm3);           \
    ymm2 = _mm256_unpacklo_epi16(ymm1, ymm0);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+16), ymm2);\
    ymm3 = _mm256_unpackhi_epi16(ymm1, ymm0);          \
    _mm256_storeu_si256((__m256i*)(p_buffer+24), ymm3);

#endif
