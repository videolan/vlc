/*****************************************************************************
 * i420_rgb_sse2.h: SSE2 YUV transformation assembly
 *****************************************************************************
 * Copyright (C) 1999-2012 VLC authors and VideoLAN
 *
 * Authors: Damien Fouilleul <damienf@videolan.org>
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
#if defined(CAN_COMPILE_SSE2)

/* SSE2 assembly */

#define SSE2_CALL(SSE2_INSTRUCTIONS)    \
    do {                                \
    __asm__ __volatile__(               \
        ".p2align 3 \n\t"               \
        SSE2_INSTRUCTIONS               \
        :                               \
        : "r" (p_y), "r" (p_u),         \
          "r" (p_v), "r" (p_buffer)     \
        : "eax", "xmm0", "xmm1", "xmm2", "xmm3", \
                 "xmm4", "xmm5", "xmm6", "xmm7" ); \
    } while(0)

#define SSE2_END  __asm__ __volatile__ ( "sfence" ::: "memory" )

#define SSE2_INIT_16_ALIGNED "                                        \n\
movq        (%1), %%xmm0    # Load 8 Cb       00 00 00 ... u2 u1 u0   \n\
movq        (%2), %%xmm1    # Load 8 Cr       00 00 00 ... v2 v1 v0   \n\
pxor      %%xmm4, %%xmm4    # zero mm4                                \n\
movdqa      (%0), %%xmm6    # Load 16 Y       YF YE YD ... Y2 Y1 Y0   \n\
"

#define SSE2_INIT_16_UNALIGNED "                                      \n\
movq        (%1), %%xmm0    # Load 8 Cb       00 00 00 ... u2 u1 u0   \n\
movq        (%2), %%xmm1    # Load 8 Cr       00 00 00 ... v2 v1 v0   \n\
pxor      %%xmm4, %%xmm4    # zero mm4                                \n\
movdqu      (%0), %%xmm6    # Load 16 Y       YF YE YD ... Y2 Y1 Y0   \n\
prefetchnta (%3)            # Tell CPU not to cache output RGB data   \n\
"

#define SSE2_INIT_32_ALIGNED "                                        \n\
movq        (%1), %%xmm0    # Load 8 Cb       00 00 00 ... u2 u1 u0   \n\
movq        (%2), %%xmm1    # Load 8 Cr       00 00 00 ... v2 v1 v0   \n\
pxor      %%xmm4, %%xmm4    # zero mm4                                \n\
movdqa      (%0), %%xmm6    # Load 16 Y       YF YE YD ... Y2 Y1 Y0   \n\
"

#define SSE2_INIT_32_UNALIGNED "                                      \n\
movq        (%1), %%xmm0    # Load 8 Cb       00 00 00 ... u2 u1 u0   \n\
movq        (%2), %%xmm1    # Load 8 Cr       00 00 00 ... v2 v1 v0   \n\
pxor      %%xmm4, %%xmm4    # zero mm4                                \n\
movdqu      (%0), %%xmm6    # Load 16 Y       YF YE YD ... Y2 Y1 Y0   \n\
prefetchnta (%3)            # Tell CPU not to cache output RGB data   \n\
"

#define SSE2_YUV_MUL "                                                      \n\
# convert the chroma part                                                   \n\
punpcklbw %%xmm4, %%xmm0        # scatter 8 Cb    00 u7 ... 00 u1 00 u0     \n\
punpcklbw %%xmm4, %%xmm1        # scatter 8 Cr    00 v7 ... 00 v1 00 v0     \n\
movl      $0x00800080, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     00 80 ... 00 80 00 80     \n\
psubsw    %%xmm5, %%xmm0        # Cb -= 128                                 \n\
psubsw    %%xmm5, %%xmm1        # Cr -= 128                                 \n\
psllw     $3, %%xmm0            # Promote precision                         \n\
psllw     $3, %%xmm1            # Promote precision                         \n\
movdqa    %%xmm0, %%xmm2        # Copy 8 Cb       00 u7 ... 00 u1 00 u0     \n\
movdqa    %%xmm1, %%xmm3        # Copy 8 Cr       00 v7 ... 00 v1 00 v0     \n\
movl      $0xf37df37d, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     f3 7d ... f3 7d f3 7d     \n\
pmulhw    %%xmm5, %%xmm2        # Mul Cb with green coeff -> Cb green       \n\
movl      $0xe5fce5fc, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     e5 fc ... e5 fc e5 fc     \n\
pmulhw    %%xmm5, %%xmm3        # Mul Cr with green coeff -> Cr green       \n\
movl      $0x40934093, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     40 93 ... 40 93 40 93     \n\
pmulhw    %%xmm5, %%xmm0        # Mul Cb -> Cblue 00 b7 ... 00 b1 00 b0     \n\
movl      $0x33123312, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     33 12 ... 33 12 33 12     \n\
pmulhw    %%xmm5, %%xmm1        # Mul Cr -> Cred  00 r7 ... 00 r1 00 r0     \n\
paddsw    %%xmm3, %%xmm2        # Cb green + Cr green -> Cgreen             \n\
                                                                            \n\
# convert the luma part                                                     \n\
movl      $0x10101010, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     10 10 ... 10 10 10 10     \n\
psubusb   %%xmm5, %%xmm6        # Y -= 16                                   \n\
movdqa    %%xmm6, %%xmm7        # Copy 16 Y       YF YE YD ... Y2 Y1 Y0     \n\
movl      $0x00ff00ff, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     00 ff ... 00 ff 00 ff     \n\
pand      %%xmm5, %%xmm6        # get Y even      00 YD ... 00 Y2 00 Y0     \n\
psrlw     $8, %%xmm7            # get Y odd       00 YF ... 00 Y3 00 Y1     \n\
psllw     $3, %%xmm6            # Promote precision                         \n\
psllw     $3, %%xmm7            # Promote precision                         \n\
movl      $0x253f253f, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     25 3f ... 25 3f 25 3f     \n\
pmulhw    %%xmm5, %%xmm6        # Mul 8 Y even    00 yD ... 00 y2 00 y0     \n\
pmulhw    %%xmm5, %%xmm7        # Mul 8 Y odd     00 yF ... 00 y3 00 y1     \n\
"

#define SSE2_YUV_ADD "                                                      \n\
# Do horizontal and vertical scaling                                        \n\
movdqa    %%xmm0, %%xmm3        # Copy Cblue                                \n\
movdqa    %%xmm1, %%xmm4        # Copy Cred                                 \n\
movdqa    %%xmm2, %%xmm5        # Copy Cgreen                               \n\
paddsw    %%xmm6, %%xmm0        # Y even + Cblue  00 BE ... 00 B2 00 B0     \n\
paddsw    %%xmm7, %%xmm3        # Y odd  + Cblue  00 BF ... 00 B3 00 B1     \n\
paddsw    %%xmm6, %%xmm1        # Y even + Cred   00 RE ... 00 R2 00 R0     \n\
paddsw    %%xmm7, %%xmm4        # Y odd  + Cred   00 RF ... 00 R3 00 R1     \n\
paddsw    %%xmm6, %%xmm2        # Y even + Cgreen 00 GE ... 00 G2 00 G0     \n\
paddsw    %%xmm7, %%xmm5        # Y odd  + Cgreen 00 GF ... 00 G3 00 G1     \n\
                                                                            \n\
# Limit RGB even to 0..255                                                  \n\
packuswb  %%xmm0, %%xmm0        #             ... B4 B2 B0 ... B4 B2 B0     \n\
packuswb  %%xmm1, %%xmm1        #             ... R4 R2 R0 ... R4 R2 R0     \n\
packuswb  %%xmm2, %%xmm2        #             ... G4 G2 G0 ... G4 G2 G0     \n\
                                                                            \n\
# Limit RGB odd to 0..255                                                   \n\
packuswb  %%xmm3, %%xmm3        #             ... B5 B3 B1 ... B5 B3 B1     \n\
packuswb  %%xmm4, %%xmm4        #             ... R5 R3 R1 ... R5 R3 R1     \n\
packuswb  %%xmm5, %%xmm5        #             ... G5 G3 G1 ... G5 G3 G1     \n\
                                                                            \n\
# Interleave RGB even and odd                                               \n\
punpcklbw %%xmm3, %%xmm0        #                 BF BE BD ... B2 B1 B0     \n\
punpcklbw %%xmm4, %%xmm1        #                 RF RE RD ... R2 R1 R0     \n\
punpcklbw %%xmm5, %%xmm2        #                 GF GE GD ... G2 G1 G0     \n\
"

#define SSE2_UNPACK_15_ALIGNED "# Note, much of this shows bit patterns (of a pair of bytes) \n\
# mask unneeded bits off                                                    \n\
movl      $0xf8f8f8f8, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8 f8 ... f8 f8 f8 f8     \n\
pand      %%xmm5, %%xmm0        # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
psrlw     $3,%%xmm0             # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pand      %%xmm5, %%xmm2        # g7g6g5g4 g3______ g7g6g5g4 g3______       \n\
pand      %%xmm5, %%xmm1        # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
psrlw     $1,%%xmm1             # __r7r6r5 r4r3____ __r7r6r5 r4r3____       \n\
pxor      %%xmm4, %%xmm4        # zero mm4                                  \n\
movdqa    %%xmm0, %%xmm5        # Copy BF-B0                                \n\
movdqa    %%xmm2, %%xmm7        # Copy GF-G0                                \n\
                                                                            \n\
# pack the 3 separate RGB bytes into 2 for pixels 0-7                       \n\
punpcklbw %%xmm4, %%xmm2        # ________ ________ g7g6g5g4 g3______       \n\
punpcklbw %%xmm1, %%xmm0        # __r7r6r5 r4r3____ ______b7 b6b5b4b3       \n\
psllw     $2,%%xmm2             # ________ ____g7g6 g5g4g3__ ________       \n\
por       %%xmm2, %%xmm0        # __r7r6r5 r4r3g7g6 g5g4g3b7 b6b5b4b3       \n\
movntdq   %%xmm0, (%3)          # store pixel 0-7                           \n\
                                                                            \n\
# pack the 3 separate RGB bytes into 2 for pixels 8-15                      \n\
punpckhbw %%xmm4, %%xmm7        # ________ ________ g7g6g5g4 g3______       \n\
punpckhbw %%xmm1, %%xmm5        # __r7r6r5 r4r3____ ______b7 b6b5b4b3       \n\
psllw     $2,%%xmm7             # ________ ____g7g6 g5g4g3__ ________       \n\
por       %%xmm7, %%xmm5        # __r7r6r5 r4r3g7g6 g5g4g3b7 b6b5b4b3       \n\
movntdq   %%xmm5, 16(%3)        # store pixel 8-15                          \n\
"

#define SSE2_UNPACK_15_UNALIGNED "# Note, much of this shows bit patterns (of a pair of bytes) \n\
# mask unneeded bits off                                                    \n\
movl      $0xf8f8f8f8, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8 f8 ... f8 f8 f8 f8     \n\
pand      %%xmm5, %%xmm0        # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
psrlw     $3,%%xmm0             # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pand      %%xmm5, %%xmm2        # g7g6g5g4 g3______ g7g6g5g4 g3______       \n\
pand      %%xmm5, %%xmm1        # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
psrlw     $1,%%xmm1             # __r7r6r5 r4r3____ __r7r6r5 r4r3____       \n\
pxor      %%xmm4, %%xmm4        # zero mm4                                  \n\
movdqa    %%xmm0, %%xmm5        # Copy BF-B0                                \n\
movdqa    %%xmm2, %%xmm7        # Copy GF-G0                                \n\
                                                                            \n\
# pack the 3 separate RGB bytes into 2 for pixels 0-7                       \n\
punpcklbw %%xmm4, %%xmm2        # ________ ________ g7g6g5g4 g3______       \n\
punpcklbw %%xmm1, %%xmm0        # __r7r6r5 r4r3____ ______b7 b6b5b4b3       \n\
psllw     $2,%%xmm2             # ________ ____g7g6 g5g4g3__ ________       \n\
por       %%xmm2, %%xmm0        # __r7r6r5 r4r3g7g6 g5g4g3b7 b6b5b4b3       \n\
movdqu    %%xmm0, (%3)          # store pixel 0-7                           \n\
                                                                            \n\
# pack the 3 separate RGB bytes into 2 for pixels 8-15                      \n\
punpckhbw %%xmm4, %%xmm7        # ________ ________ g7g6g5g4 g3______       \n\
punpckhbw %%xmm1, %%xmm5        # __r7r6r5 r4r3____ ______b7 b6b5b4b3       \n\
psllw     $2,%%xmm7             # ________ ____g7g6 g5g4g3__ ________       \n\
por       %%xmm7, %%xmm5        # __r7r6r5 r4r3g7g6 g5g4g3b7 b6b5b4b3       \n\
movdqu    %%xmm5, 16(%3)        # store pixel 8-15                          \n\
"

#define SSE2_UNPACK_16_ALIGNED "# Note, much of this shows bit patterns (of a pair of bytes) \n\
# mask unneeded bits off                                                    \n\
movl      $0xf8f8f8f8, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8 f8 ... f8 f8 f8 f8     \n\
pand      %%xmm5, %%xmm0        # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
pand      %%xmm5, %%xmm1        # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
movl      $0xfcfcfcfc, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     fc fc ... fc fc fc fc     \n\
pand      %%xmm5, %%xmm2        # g7g6g5g4 g3g2____ g7g6g5g4 g3g2____       \n\
psrlw     $3,%%xmm0             # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pxor      %%xmm4, %%xmm4        # zero mm4                                  \n\
movdqa    %%xmm0, %%xmm5        # Copy BF-B0                                \n\
movdqa    %%xmm2, %%xmm7        # Copy GF-G0                                \n\
                                                                            \n\
# pack the 3 separate RGB bytes into 2 for pixels 0-7                       \n\
punpcklbw %%xmm4, %%xmm2        # ________ ________ g7g6g5g4 g3g2____       \n\
punpcklbw %%xmm1, %%xmm0        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%xmm2             # ________ __g7g6g5 g4g3g2__ ________       \n\
por       %%xmm2, %%xmm0        # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movntdq   %%xmm0, (%3)          # store pixel 0-7                           \n\
                                                                            \n\
# pack the 3 separate RGB bytes into 2 for pixels 8-15                      \n\
punpckhbw %%xmm4, %%xmm7        # ________ ________ g7g6g5g4 g3g2____       \n\
punpckhbw %%xmm1, %%xmm5        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%xmm7             # ________ __g7g6g5 g4g3g2__ ________       \n\
por       %%xmm7, %%xmm5        # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movntdq   %%xmm5, 16(%3)        # store pixel 8-15                          \n\
"

#define SSE2_UNPACK_16_UNALIGNED "# Note, much of this shows bit patterns (of a pair of bytes) \n\
# mask unneeded bits off                                                    \n\
movl      $0xf8f8f8f8, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8 f8 ... f8 f8 f8 f8     \n\
pand      %%xmm5, %%xmm0        # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
pand      %%xmm5, %%xmm1        # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
movl      $0xfcfcfcfc, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     fc fc ... fc fc fc fc     \n\
pand      %%xmm5, %%xmm2        # g7g6g5g4 g3g2____ g7g6g5g4 g3g2____       \n\
psrlw     $3,%%xmm0             # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pxor      %%xmm4, %%xmm4        # zero mm4                                  \n\
movdqa    %%xmm0, %%xmm5        # Copy BF-B0                                \n\
movdqa    %%xmm2, %%xmm7        # Copy GF-G0                                \n\
                                                                            \n\
# pack the 3 separate RGB bytes into 2 for pixels 0-7                       \n\
punpcklbw %%xmm4, %%xmm2        # ________ ________ g7g6g5g4 g3g2____       \n\
punpcklbw %%xmm1, %%xmm0        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%xmm2             # ________ __g7g6g5 g4g3g2__ ________       \n\
por       %%xmm2, %%xmm0        # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movdqu    %%xmm0, (%3)          # store pixel 0-7                           \n\
                                                                            \n\
# pack the 3 separate RGB bytes into 2 for pixels 8-15                      \n\
punpckhbw %%xmm4, %%xmm7        # ________ ________ g7g6g5g4 g3g2____       \n\
punpckhbw %%xmm1, %%xmm5        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%xmm7             # ________ __g7g6g5 g4g3g2__ ________       \n\
por       %%xmm7, %%xmm5        # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movdqu    %%xmm5, 16(%3)        # store pixel 8-15                          \n\
"

#define SSE2_UNPACK_32_ARGB_ALIGNED "                                     \n\
pxor      %%xmm3, %%xmm3  # zero xmm3                                     \n\
movdqa    %%xmm0, %%xmm4  #               BF BE BD ... B2 B1 B0           \n\
punpcklbw %%xmm2, %%xmm4  #               G7 B7 ... G1 B1 G0 B0           \n\
movdqa    %%xmm1, %%xmm5  #               RF RE RD ... R2 R1 R0           \n\
punpcklbw %%xmm3, %%xmm5  #               00 R7 ... 00 R1 00 R0           \n\
movdqa    %%xmm4, %%xmm6  #               G7 B7 ... G1 B1 G0 B0           \n\
punpcklwd %%xmm5, %%xmm4  #               00 R3 ... 00 R0 G0 B0           \n\
movntdq   %%xmm4, (%3)    # Store ARGB3 ... ARGB0                         \n\
punpckhwd %%xmm5, %%xmm6  #               00 R7 ... 00 R4 G4 B4           \n\
movntdq   %%xmm6, 16(%3)  # Store ARGB7 ... ARGB4                         \n\
punpckhbw %%xmm2, %%xmm0  #               GB BB ... G9 B9 G8 B8           \n\
punpckhbw %%xmm3, %%xmm1  #               00 RF ... 00 R9 00 R8           \n\
movdqa    %%xmm0, %%xmm5  #               GF BF ... G9 B9 G8 B8           \n\
punpcklwd %%xmm1, %%xmm5  #               00 RB ... 00 R8 G8 B8           \n\
movntdq   %%xmm5, 32(%3)  # Store ARGB11 ... ARGB8                        \n\
punpckhwd %%xmm1, %%xmm0  #               00 RF ... 00 RC GC BC           \n\
movntdq   %%xmm0, 48(%3)  # Store ARGB15 ... ARGB12                       \n\
"

#define SSE2_UNPACK_32_ARGB_UNALIGNED "                                   \n\
pxor      %%xmm3, %%xmm3  # zero xmm3                                     \n\
movdqa    %%xmm0, %%xmm4  #               BF BE BD ... B2 B1 B0           \n\
punpcklbw %%xmm2, %%xmm4  #               G7 B7 ... G1 B1 G0 B0           \n\
movdqa    %%xmm1, %%xmm5  #               RF RE RD ... R2 R1 R0           \n\
punpcklbw %%xmm3, %%xmm5  #               00 R7 ... 00 R1 00 R0           \n\
movdqa    %%xmm4, %%xmm6  #               G7 B7 ... G1 B1 G0 B0           \n\
punpcklwd %%xmm5, %%xmm4  #               00 R3 ... 00 R0 G0 B0           \n\
movdqu    %%xmm4, (%3)    # Store ARGB3 ... ARGB0                         \n\
punpckhwd %%xmm5, %%xmm6  #               00 R7 ... 00 R4 G4 B4           \n\
movdqu    %%xmm6, 16(%3)  # Store ARGB7 ... ARGB4                         \n\
punpckhbw %%xmm2, %%xmm0  #               GF BF ... G9 B9 G8 B8           \n\
punpckhbw %%xmm3, %%xmm1  #               00 RF ... 00 R9 00 R8           \n\
movdqa    %%xmm0, %%xmm5  #               GF BF ... G9 B9 G8 B8           \n\
punpcklwd %%xmm1, %%xmm5  #               00 RB ... 00 R8 G8 B8           \n\
movdqu    %%xmm5, 32(%3)  # Store ARGB11 ... ARGB8                        \n\
punpckhwd %%xmm1, %%xmm0  #               00 RF ... 00 RC GC BC           \n\
movdqu    %%xmm0, 48(%3)  # Store ARGB15 ... ARGB12                       \n\
"

#define SSE2_UNPACK_32_RGBA_ALIGNED "                                     \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                      \n\
movdqa    %%xmm2, %%xmm4  #                 G7 G6 G5 ... G2 G1 G0         \n\
punpcklbw %%xmm1, %%xmm4  #                 R7 G7 ... R1 G1 R0 G0         \n\
punpcklbw %%xmm0, %%xmm3  #                 B7 00 ... B1 00 B0 00         \n\
movdqa    %%xmm3, %%xmm5  #                 R7 00 ... R1 00 R0 00         \n\
punpcklwd %%xmm4, %%xmm3  #                 R3 G3 ... R0 B0 G0 00         \n\
movntdq   %%xmm3, (%3)    # Store RGBA3 ... RGBA0                         \n\
punpckhwd %%xmm4, %%xmm5  #                 R7 G7 ... R4 G4 B4 00         \n\
movntdq   %%xmm5, 16(%3)  # Store RGBA7 ... RGBA4                         \n\
pxor      %%xmm6, %%xmm6  # zero mm6                                      \n\
punpckhbw %%xmm1, %%xmm2  #                 RB GB ... R9 G9 R8 G8         \n\
punpckhbw %%xmm0, %%xmm6  #                 BF 00 ... B9 00 B8 00         \n\
movdqa    %%xmm6, %%xmm0  #                 BF 00 ... B9 00 B8 00         \n\
punpcklwd %%xmm2, %%xmm6  #                 RB GB ... R8 G8 B8 00         \n\
movntdq   %%xmm6, 32(%3)  # Store BGRA11 ... RGBA8                        \n\
punpckhwd %%xmm2, %%xmm0  #                 RF GF ... RC GC BC 00         \n\
movntdq   %%xmm0, 48(%3)  # Store RGBA15 ... RGBA12                       \n\
"

#define SSE2_UNPACK_32_RGBA_UNALIGNED "                                   \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                      \n\
movdqa    %%xmm2, %%xmm4  #                 GF GE GD ... G2 G1 G0         \n\
punpcklbw %%xmm1, %%xmm4  #                 R7 G7 ... R1 G1 R0 G0         \n\
punpcklbw %%xmm0, %%xmm3  #                 B7 00 ... B1 00 B0 00         \n\
movdqa    %%xmm3, %%xmm5  #                 R7 00 ... R1 00 R0 00         \n\
punpcklwd %%xmm4, %%xmm3  #                 R3 G3 ... R0 B0 G0 00         \n\
movdqu    %%xmm3, (%3)    # Store RGBA3 ... RGBA0                         \n\
punpckhwd %%xmm4, %%xmm5  #                 R7 G7 ... R4 G4 B4 00         \n\
movdqu    %%xmm5, 16(%3)  # Store RGBA7 ... RGBA4                         \n\
pxor      %%xmm6, %%xmm6  # zero mm6                                      \n\
punpckhbw %%xmm1, %%xmm2  #                 RF GF ... R9 G9 R8 G8         \n\
punpckhbw %%xmm0, %%xmm6  #                 BF 00 ... B9 00 B8 00         \n\
movdqa    %%xmm6, %%xmm0  #                 BF 00 ... B9 00 B8 00         \n\
punpcklwd %%xmm2, %%xmm6  #                 RB GB ... R8 G8 B8 00         \n\
movdqu    %%xmm6, 32(%3)  # Store RGBA11 ... RGBA8                        \n\
punpckhwd %%xmm2, %%xmm0  #                 RF GF ... RC GC BC 00         \n\
movdqu    %%xmm0, 48(%3)  # Store RGBA15 ... RGBA12                       \n\
"

#define SSE2_UNPACK_32_BGRA_ALIGNED "                                     \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                      \n\
movdqa    %%xmm2, %%xmm4  #                 G7 G6 G5 ... G2 G1 G0         \n\
punpcklbw %%xmm0, %%xmm4  #                 B7 G7 ... B1 G1 B0 G0         \n\
punpcklbw %%xmm1, %%xmm3  #                 R7 00 ... R1 00 R0 00         \n\
movdqa    %%xmm3, %%xmm5  #                 R7 00 ... R1 00 R0 00         \n\
punpcklwd %%xmm4, %%xmm3  #                 B3 G3 ... B0 G0 R0 00         \n\
movntdq   %%xmm3, (%3)    # Store BGRA3 ... BGRA0                         \n\
punpckhwd %%xmm4, %%xmm5  #                 B7 G7 ... B4 G4 R4 00         \n\
movntdq   %%xmm5, 16(%3)  # Store BGRA7 ... BGRA4                         \n\
pxor      %%xmm6, %%xmm6  # zero mm6                                      \n\
punpckhbw %%xmm0, %%xmm2  #                 BF GF ... B9 G9 B8 G8         \n\
punpckhbw %%xmm1, %%xmm6  #                 RF 00 ... R9 00 R8 00         \n\
movdqa    %%xmm6, %%xmm0  #                 RF 00 ... R9 00 R8 00         \n\
punpcklwd %%xmm2, %%xmm6  #                 BB GB ... B8 G8 R8 00         \n\
movntdq   %%xmm6, 32(%3)  # Store BGRA11 ... BGRA8                        \n\
punpckhwd %%xmm2, %%xmm0  #                 BF GF ... BC GC RC 00         \n\
movntdq   %%xmm0, 48(%3)  # Store BGRA15 ... BGRA12                       \n\
"

#define SSE2_UNPACK_32_BGRA_UNALIGNED "                                   \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                      \n\
movdqa    %%xmm2, %%xmm4  #                 GF GE GD ... G2 G1 G0         \n\
punpcklbw %%xmm0, %%xmm4  #                 B7 G7 ... B1 G1 B0 G0         \n\
punpcklbw %%xmm1, %%xmm3  #                 R7 00 ... R1 00 R0 00         \n\
movdqa    %%xmm3, %%xmm5  #                 R7 00 ... R1 00 R0 00         \n\
punpcklwd %%xmm4, %%xmm3  #                 B3 G3 ... B0 G0 R0 00         \n\
movdqu    %%xmm3, (%3)    # Store BGRA3 ... BGRA0                         \n\
punpckhwd %%xmm4, %%xmm5  #                 B7 G7 ... B4 G4 R4 00         \n\
movdqu    %%xmm5, 16(%3)  # Store BGRA7 ... BGRA4                         \n\
pxor      %%xmm6, %%xmm6  # zero mm6                                      \n\
punpckhbw %%xmm0, %%xmm2  #                 BC GC ... B9 G9 B8 G8         \n\
punpckhbw %%xmm1, %%xmm6  #                 RC 00 ... R9 00 R8 00         \n\
movdqa    %%xmm6, %%xmm0  #                 RC 00 ... R9 00 R8 00         \n\
punpcklwd %%xmm2, %%xmm6  #                 BB GB ... B8 G8 R8 00         \n\
movdqu    %%xmm6, 32(%3)  # Store BGRA11 ... BGRA8                        \n\
punpckhwd %%xmm2, %%xmm0  #                 BF GF ... BC GC RC 00         \n\
movdqu    %%xmm0, 48(%3)  # Store BGRA15 ... BGRA12                       \n\
"

#define SSE2_UNPACK_32_ABGR_ALIGNED "                                     \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                      \n\
movdqa    %%xmm1, %%xmm4  #                 RF RE RD ... R2 R1 R0         \n\
punpcklbw %%xmm2, %%xmm4  #                 G7 R7 ... G1 R1 G0 R0         \n\
movdqa    %%xmm0, %%xmm5  #                 BF BE BD ... B2 B1 B0         \n\
punpcklbw %%xmm3, %%xmm5  #                 00 B7 ... 00 B1 00 B0         \n\
movdqa    %%xmm4, %%xmm6  #                 G7 R7 ... G1 R1 G0 R0         \n\
punpcklwd %%xmm5, %%xmm4  #                 00 B3 ... 00 B0 G0 R0         \n\
movntdq   %%xmm4, (%3)    # Store ABGR3 ... ABGR0                         \n\
punpckhwd %%xmm5, %%xmm6  #                 00 B7 ... 00 B4 G4 R4         \n\
movntdq   %%xmm6, 16(%3)  # Store ABGR7 ... ABGR4                         \n\
punpckhbw %%xmm2, %%xmm1  #                 GF RF ... G9 R9 G8 R8         \n\
punpckhbw %%xmm3, %%xmm0  #                 00 BF ... 00 B9 00 B8         \n\
movdqa    %%xmm1, %%xmm2  #                 GF RF ... G9 R9 G8 R8         \n\
punpcklwd %%xmm0, %%xmm1  #                 00 BB ... 00 B8 G8 R8         \n\
movntdq   %%xmm1, 32(%3)  # Store ABGR11 ... ABGR8                        \n\
punpckhwd %%xmm0, %%xmm2  #                 00 BF ... 00 BC GC RC         \n\
movntdq   %%xmm2, 48(%3)  # Store ABGR15 ... ABGR12                       \n\
"

#define SSE2_UNPACK_32_ABGR_UNALIGNED "                                   \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                      \n\
movdqa    %%xmm1, %%xmm4  #                 RF RE RD ... R2 R1 R0         \n\
punpcklbw %%xmm2, %%xmm4  #                 G7 R7 ... G1 R1 G0 R0         \n\
movdqa    %%xmm0, %%xmm5  #                 BF BE BD ... B2 B1 B0         \n\
punpcklbw %%xmm3, %%xmm5  #                 00 B7 ... 00 B1 00 B0         \n\
movdqa    %%xmm4, %%xmm6  #                 G7 R7 ... G1 R1 G0 R0         \n\
punpcklwd %%xmm5, %%xmm4  #                 00 B3 ... 00 B0 G0 R0         \n\
movdqu    %%xmm4, (%3)    # Store ABGR3 ... ABGR0                         \n\
punpckhwd %%xmm5, %%xmm6  #                 00 B7 ... 00 B4 G4 R4         \n\
movdqu    %%xmm6, 16(%3)  # Store ABGR7 ... ABGR4                         \n\
punpckhbw %%xmm2, %%xmm1  #                 GF RF ... G9 R9 G8 R8         \n\
punpckhbw %%xmm3, %%xmm0  #                 00 BF ... 00 B9 00 B8         \n\
movdqa    %%xmm1, %%xmm2  #                 GF RF ... G9 R9 G8 R8         \n\
punpcklwd %%xmm0, %%xmm1  #                 00 BB ... 00 B8 G8 R8         \n\
movdqu    %%xmm1, 32(%3)  # Store ABGR11 ... ABGR8                        \n\
punpckhwd %%xmm0, %%xmm2  #                 00 BF ... 00 BC GC RC         \n\
movdqu    %%xmm2, 48(%3)  # Store ABGR15 ... ABGR12                       \n\
"

#elif defined(HAVE_SSE2_INTRINSICS)

/* SSE2 intrinsics */

#include <emmintrin.h>

#define SSE2_CALL(SSE2_INSTRUCTIONS)        \
    do {                                    \
        __m128i xmm0, xmm1, xmm2, xmm3,     \
                xmm4, xmm5, xmm6, xmm7;     \
        SSE2_INSTRUCTIONS                   \
    } while(0)

#define SSE2_END  _mm_sfence()

#define SSE2_INIT_16_ALIGNED                \
    xmm0 = _mm_loadl_epi64((__m128i *)p_u); \
    xmm1 = _mm_loadl_epi64((__m128i *)p_v); \
    xmm4 = _mm_setzero_si128();             \
    xmm6 = _mm_load_si128((__m128i *)p_y);

#define SSE2_INIT_16_UNALIGNED              \
    xmm0 = _mm_loadl_epi64((__m128i *)p_u); \
    xmm1 = _mm_loadl_epi64((__m128i *)p_v); \
    xmm4 = _mm_setzero_si128();             \
    xmm6 = _mm_loadu_si128((__m128i *)p_y); \
    _mm_prefetch(p_buffer, _MM_HINT_NTA);

#define SSE2_INIT_32_ALIGNED                \
    xmm0 = _mm_loadl_epi64((__m128i *)p_u); \
    xmm1 = _mm_loadl_epi64((__m128i *)p_v); \
    xmm4 = _mm_setzero_si128();             \
    xmm6 = _mm_load_si128((__m128i *)p_y);

#define SSE2_INIT_32_UNALIGNED              \
    xmm0 = _mm_loadl_epi64((__m128i *)p_u); \
    xmm1 = _mm_loadl_epi64((__m128i *)p_v); \
    xmm4 = _mm_setzero_si128();             \
    xmm6 = _mm_loadu_si128((__m128i *)p_y); \
    _mm_prefetch(p_buffer, _MM_HINT_NTA);

#define SSE2_YUV_MUL                        \
    xmm0 = _mm_unpacklo_epi8(xmm0, xmm4);   \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm4);   \
    xmm5 = _mm_set1_epi32(0x00800080UL);    \
    xmm0 = _mm_subs_epi16(xmm0, xmm5);      \
    xmm1 = _mm_subs_epi16(xmm1, xmm5);      \
    xmm0 = _mm_slli_epi16(xmm0, 3);         \
    xmm1 = _mm_slli_epi16(xmm1, 3);         \
    xmm2 = xmm0;                            \
    xmm3 = xmm1;                            \
    xmm5 = _mm_set1_epi32(0xf37df37dUL);    \
    xmm2 = _mm_mulhi_epi16(xmm2, xmm5);     \
    xmm5 = _mm_set1_epi32(0xe5fce5fcUL);    \
    xmm3 = _mm_mulhi_epi16(xmm3, xmm5);     \
    xmm5 = _mm_set1_epi32(0x40934093UL);    \
    xmm0 = _mm_mulhi_epi16(xmm0, xmm5);     \
    xmm5 = _mm_set1_epi32(0x33123312UL);    \
    xmm1 = _mm_mulhi_epi16(xmm1, xmm5);     \
    xmm2 = _mm_adds_epi16(xmm2, xmm3);      \
    \
    xmm5 = _mm_set1_epi32(0x10101010UL);    \
    xmm6 = _mm_subs_epu8(xmm6, xmm5);       \
    xmm7 = xmm6;                            \
    xmm5 = _mm_set1_epi32(0x00ff00ffUL);    \
    xmm6 = _mm_and_si128(xmm6, xmm5);       \
    xmm7 = _mm_srli_epi16(xmm7, 8);         \
    xmm6 = _mm_slli_epi16(xmm6, 3);         \
    xmm7 = _mm_slli_epi16(xmm7, 3);         \
    xmm5 = _mm_set1_epi32(0x253f253fUL);    \
    xmm6 = _mm_mulhi_epi16(xmm6, xmm5);     \
    xmm7 = _mm_mulhi_epi16(xmm7, xmm5);

#define SSE2_YUV_ADD                        \
    xmm3 = xmm0;                            \
    xmm4 = xmm1;                            \
    xmm5 = xmm2;                            \
    xmm0 = _mm_adds_epi16(xmm0, xmm6);      \
    xmm3 = _mm_adds_epi16(xmm3, xmm7);      \
    xmm1 = _mm_adds_epi16(xmm1, xmm6);      \
    xmm4 = _mm_adds_epi16(xmm4, xmm7);      \
    xmm2 = _mm_adds_epi16(xmm2, xmm6);      \
    xmm5 = _mm_adds_epi16(xmm5, xmm7);      \
    \
    xmm0 = _mm_packus_epi16(xmm0, xmm0);    \
    xmm1 = _mm_packus_epi16(xmm1, xmm1);    \
    xmm2 = _mm_packus_epi16(xmm2, xmm2);    \
    \
    xmm3 = _mm_packus_epi16(xmm3, xmm3);    \
    xmm4 = _mm_packus_epi16(xmm4, xmm4);    \
    xmm5 = _mm_packus_epi16(xmm5, xmm5);    \
    \
    xmm0 = _mm_unpacklo_epi8(xmm0, xmm3);   \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm4);   \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm5);

#define SSE2_UNPACK_15_ALIGNED                      \
    xmm5 = _mm_set1_epi32(0xf8f8f8f8UL);            \
    xmm0 = _mm_and_si128(xmm0, xmm5);               \
    xmm0 = _mm_srli_epi16(xmm0, 3);                 \
    xmm2 = _mm_and_si128(xmm2, xmm5);               \
    xmm1 = _mm_and_si128(xmm1, xmm5);               \
    xmm1 = _mm_srli_epi16(xmm1, 1);                 \
    xmm4 = _mm_setzero_si128();                     \
    xmm5 = xmm0;                                    \
    xmm7 = xmm2;                                    \
    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm4);           \
    xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);           \
    xmm2 = _mm_slli_epi16(xmm2, 2);                 \
    xmm0 = _mm_or_si128(xmm0, xmm2);                \
    _mm_stream_si128((__m128i*)p_buffer, xmm0);     \
    \
    xmm7 = _mm_unpackhi_epi8(xmm7, xmm4);           \
    xmm5 = _mm_unpackhi_epi8(xmm5, xmm1);           \
    xmm7 = _mm_slli_epi16(xmm7, 2);                 \
    xmm5 = _mm_or_si128(xmm5, xmm7);                \
    _mm_stream_si128((__m128i*)(p_buffer+8), xmm5);

#define SSE2_UNPACK_15_UNALIGNED                    \
    xmm5 = _mm_set1_epi32(0xf8f8f8f8UL);            \
    xmm0 = _mm_and_si128(xmm0, xmm5);               \
    xmm0 = _mm_srli_epi16(xmm0, 3);                 \
    xmm2 = _mm_and_si128(xmm2, xmm5);               \
    xmm1 = _mm_and_si128(xmm1, xmm5);               \
    xmm1 = _mm_srli_epi16(xmm1, 1);                 \
    xmm4 = _mm_setzero_si128();                     \
    xmm5 = xmm0;                                    \
    xmm7 = xmm2;                                    \
    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm4);           \
    xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);           \
    xmm2 = _mm_slli_epi16(xmm2, 2);                 \
    xmm0 = _mm_or_si128(xmm0, xmm2);                \
    _mm_storeu_si128((__m128i*)p_buffer, xmm0);     \
    \
    xmm7 = _mm_unpackhi_epi8(xmm7, xmm4);           \
    xmm5 = _mm_unpackhi_epi8(xmm5, xmm1);           \
    xmm7 = _mm_slli_epi16(xmm7, 2);                 \
    xmm5 = _mm_or_si128(xmm5, xmm7);                \
    _mm_storeu_si128((__m128i*)(p_buffer+8), xmm5);

#define SSE2_UNPACK_16_ALIGNED                      \
    xmm5 = _mm_set1_epi32(0xf8f8f8f8UL);            \
    xmm0 = _mm_and_si128(xmm0, xmm5);               \
    xmm1 = _mm_and_si128(xmm1, xmm5);               \
    xmm5 = _mm_set1_epi32(0xfcfcfcfcUL);            \
    xmm2 = _mm_and_si128(xmm2, xmm5);               \
    xmm0 = _mm_srli_epi16(xmm0, 3);                 \
    xmm4 = _mm_setzero_si128();                     \
    xmm5 = xmm0;                                    \
    xmm7 = xmm2;                                    \
    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm4);           \
    xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);           \
    xmm2 = _mm_slli_epi16(xmm2, 3);                 \
    xmm0 = _mm_or_si128(xmm0, xmm2);                \
    _mm_stream_si128((__m128i*)p_buffer, xmm0);     \
    \
    xmm7 = _mm_unpackhi_epi8(xmm7, xmm4);           \
    xmm5 = _mm_unpackhi_epi8(xmm5, xmm1);           \
    xmm7 = _mm_slli_epi16(xmm7, 3);                 \
    xmm5 = _mm_or_si128(xmm5, xmm7);                \
    _mm_stream_si128((__m128i*)(p_buffer+8), xmm5);

#define SSE2_UNPACK_16_UNALIGNED                    \
    xmm5 = _mm_set1_epi32(0xf8f8f8f8UL);            \
    xmm0 = _mm_and_si128(xmm0, xmm5);               \
    xmm1 = _mm_and_si128(xmm1, xmm5);               \
    xmm5 = _mm_set1_epi32(0xfcfcfcfcUL);            \
    xmm2 = _mm_and_si128(xmm2, xmm5);               \
    xmm0 = _mm_srli_epi16(xmm0, 3);                 \
    xmm4 = _mm_setzero_si128();                     \
    xmm5 = xmm0;                                    \
    xmm7 = xmm2;                                    \
    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm4);           \
    xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);           \
    xmm2 = _mm_slli_epi16(xmm2, 3);                 \
    xmm0 = _mm_or_si128(xmm0, xmm2);                \
    _mm_storeu_si128((__m128i*)p_buffer, xmm0);     \
    \
    xmm7 = _mm_unpackhi_epi8(xmm7, xmm4);           \
    xmm5 = _mm_unpackhi_epi8(xmm5, xmm1);           \
    xmm7 = _mm_slli_epi16(xmm7, 3);                 \
    xmm5 = _mm_or_si128(xmm5, xmm7);                \
    _mm_storeu_si128((__m128i*)(p_buffer+8), xmm5);

#define SSE2_UNPACK_32_ARGB_ALIGNED                 \
    xmm3 = _mm_setzero_si128();                     \
    xmm4 = xmm0;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm2);           \
    xmm5 = xmm1;                                    \
    xmm5 = _mm_unpacklo_epi8(xmm5, xmm3);           \
    xmm6 = xmm4;                                    \
    xmm4 = _mm_unpacklo_epi16(xmm4, xmm5);          \
    _mm_stream_si128((__m128i*)(p_buffer), xmm4);   \
    xmm6 = _mm_unpackhi_epi16(xmm6, xmm5);          \
    _mm_stream_si128((__m128i*)(p_buffer+4), xmm6); \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm2);           \
    xmm1 = _mm_unpackhi_epi8(xmm1, xmm3);           \
    xmm5 = xmm0;                                    \
    xmm5 = _mm_unpacklo_epi16(xmm5, xmm1);          \
    _mm_stream_si128((__m128i*)(p_buffer+8), xmm5); \
    xmm0 = _mm_unpackhi_epi16(xmm0, xmm1);          \
    _mm_stream_si128((__m128i*)(p_buffer+12), xmm0);

#define SSE2_UNPACK_32_ARGB_UNALIGNED               \
    xmm3 = _mm_setzero_si128();                     \
    xmm4 = xmm0;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm2);           \
    xmm5 = xmm1;                                    \
    xmm5 = _mm_unpacklo_epi8(xmm5, xmm3);           \
    xmm6 = xmm4;                                    \
    xmm4 = _mm_unpacklo_epi16(xmm4, xmm5);          \
    _mm_storeu_si128((__m128i*)(p_buffer), xmm4);   \
    xmm6 = _mm_unpackhi_epi16(xmm6, xmm5);          \
    _mm_storeu_si128((__m128i*)(p_buffer+4), xmm6); \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm2);           \
    xmm1 = _mm_unpackhi_epi8(xmm1, xmm3);           \
    xmm5 = xmm0;                                    \
    xmm5 = _mm_unpacklo_epi16(xmm5, xmm1);          \
    _mm_storeu_si128((__m128i*)(p_buffer+8), xmm5); \
    xmm0 = _mm_unpackhi_epi16(xmm0, xmm1);          \
    _mm_storeu_si128((__m128i*)(p_buffer+12), xmm0);

#define SSE2_UNPACK_32_RGBA_ALIGNED                 \
    xmm3 = _mm_setzero_si128();                     \
    xmm4 = xmm2;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);           \
    xmm3 = _mm_unpacklo_epi8(xmm3, xmm0);           \
    xmm5 = xmm3;                                    \
    xmm3 = _mm_unpacklo_epi16(xmm3, xmm4);          \
    _mm_stream_si128((__m128i*)(p_buffer), xmm3);   \
    xmm5 = _mm_unpackhi_epi16(xmm5, xmm4);          \
    _mm_stream_si128((__m128i*)(p_buffer+4), xmm5); \
    xmm6 = _mm_setzero_si128();                     \
    xmm2 = _mm_unpackhi_epi8(xmm2, xmm1);           \
    xmm6 = _mm_unpackhi_epi8(xmm6, xmm0);           \
    xmm0 = xmm6;                                    \
    xmm6 = _mm_unpacklo_epi16(xmm6, xmm2);          \
    _mm_stream_si128((__m128i*)(p_buffer+8), xmm6); \
    xmm0 = _mm_unpackhi_epi16(xmm0, xmm2);          \
    _mm_stream_si128((__m128i*)(p_buffer+12), xmm0);

#define SSE2_UNPACK_32_RGBA_UNALIGNED               \
    xmm3 = _mm_setzero_si128();                     \
    xmm4 = xmm2;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);           \
    xmm3 = _mm_unpacklo_epi8(xmm3, xmm0);           \
    xmm5 = xmm3;                                    \
    xmm3 = _mm_unpacklo_epi16(xmm3, xmm4);          \
    _mm_storeu_si128((__m128i*)(p_buffer), xmm3);   \
    xmm5 = _mm_unpackhi_epi16(xmm5, xmm4);          \
    _mm_storeu_si128((__m128i*)(p_buffer+4), xmm5); \
    xmm6 = _mm_setzero_si128();                     \
    xmm2 = _mm_unpackhi_epi8(xmm2, xmm1);           \
    xmm6 = _mm_unpackhi_epi8(xmm6, xmm0);           \
    xmm0 = xmm6;                                    \
    xmm6 = _mm_unpacklo_epi16(xmm6, xmm2);          \
    _mm_storeu_si128((__m128i*)(p_buffer+8), xmm6); \
    xmm0 = _mm_unpackhi_epi16(xmm0, xmm2);          \
    _mm_storeu_si128((__m128i*)(p_buffer+12), xmm0);

#define SSE2_UNPACK_32_BGRA_ALIGNED                 \
    xmm3 = _mm_setzero_si128();                     \
    xmm4 = xmm2;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm0);           \
    xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);           \
    xmm5 = xmm3;                                    \
    xmm3 = _mm_unpacklo_epi16(xmm3, xmm4);          \
    _mm_stream_si128((__m128i*)(p_buffer), xmm3);   \
    xmm5 = _mm_unpackhi_epi16(xmm5, xmm4);          \
    _mm_stream_si128((__m128i*)(p_buffer+4), xmm5); \
    xmm6 = _mm_setzero_si128();                     \
    xmm2 = _mm_unpackhi_epi8(xmm2, xmm0);           \
    xmm6 = _mm_unpackhi_epi8(xmm6, xmm1);           \
    xmm0 = xmm6;                                    \
    xmm6 = _mm_unpacklo_epi16(xmm6, xmm2);          \
    _mm_stream_si128((__m128i*)(p_buffer+8), xmm6); \
    xmm0 = _mm_unpackhi_epi16(xmm0, xmm2);          \
    _mm_stream_si128((__m128i*)(p_buffer+12), xmm0);

#define SSE2_UNPACK_32_BGRA_UNALIGNED               \
    xmm3 = _mm_setzero_si128();                     \
    xmm4 = xmm2;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm0);           \
    xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);           \
    xmm5 = xmm3;                                    \
    xmm3 = _mm_unpacklo_epi16(xmm3, xmm4);          \
    _mm_storeu_si128((__m128i*)(p_buffer), xmm3);   \
    xmm5 = _mm_unpackhi_epi16(xmm5, xmm4);          \
    _mm_storeu_si128((__m128i*)(p_buffer+4), xmm5); \
    xmm6 = _mm_setzero_si128();                     \
    xmm2 = _mm_unpackhi_epi8(xmm2, xmm0);           \
    xmm6 = _mm_unpackhi_epi8(xmm6, xmm1);           \
    xmm0 = xmm6;                                    \
    xmm6 = _mm_unpacklo_epi16(xmm6, xmm2);          \
    _mm_storeu_si128((__m128i*)(p_buffer+8), xmm6); \
    xmm0 = _mm_unpackhi_epi16(xmm0, xmm2);          \
    _mm_storeu_si128((__m128i*)(p_buffer+12), xmm0);

#define SSE2_UNPACK_32_ABGR_ALIGNED                 \
    xmm3 = _mm_setzero_si128();                     \
    xmm4 = xmm1;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm2);           \
    xmm5 = xmm0;                                    \
    xmm5 = _mm_unpacklo_epi8(xmm5, xmm3);           \
    xmm6 = xmm4;                                    \
    xmm4 = _mm_unpacklo_epi16(xmm4, xmm5);          \
    _mm_stream_si128((__m128i*)(p_buffer), xmm4);   \
    xmm6 = _mm_unpackhi_epi16(xmm6, xmm5);          \
    _mm_stream_si128((__m128i*)(p_buffer+4), xmm6); \
    xmm1 = _mm_unpackhi_epi8(xmm1, xmm2);           \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm3);           \
    xmm2 = xmm1;                                    \
    xmm1 = _mm_unpacklo_epi16(xmm1, xmm0);          \
    _mm_stream_si128((__m128i*)(p_buffer+8), xmm1); \
    xmm2 = _mm_unpackhi_epi16(xmm2, xmm0);          \
    _mm_stream_si128((__m128i*)(p_buffer+12), xmm2);

#define SSE2_UNPACK_32_ABGR_UNALIGNED               \
    xmm3 = _mm_setzero_si128();                     \
    xmm4 = xmm1;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm2);           \
    xmm5 = xmm0;                                    \
    xmm5 = _mm_unpacklo_epi8(xmm5, xmm3);           \
    xmm6 = xmm4;                                    \
    xmm4 = _mm_unpacklo_epi16(xmm4, xmm5);          \
    _mm_storeu_si128((__m128i*)(p_buffer), xmm4);   \
    xmm6 = _mm_unpackhi_epi16(xmm6, xmm5);          \
    _mm_storeu_si128((__m128i*)(p_buffer+4), xmm6); \
    xmm1 = _mm_unpackhi_epi8(xmm1, xmm2);           \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm3);           \
    xmm2 = xmm1;                                    \
    xmm1 = _mm_unpacklo_epi16(xmm1, xmm0);          \
    _mm_storeu_si128((__m128i*)(p_buffer+8), xmm1); \
    xmm2 = _mm_unpackhi_epi16(xmm2, xmm0);          \
    _mm_storeu_si128((__m128i*)(p_buffer+12), xmm2);

#endif
