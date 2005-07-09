/*****************************************************************************
 * transforms_yuvmmx.h: MMX YUV transformation assembly
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Olie Lho <ollie@sis.com.tw>
 *          Gaël Hendryckx <jimmy@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* hope these constant values are cache line aligned */
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
#define USED_U64(foo) \
    static const uint64_t foo __asm__ (#foo) __attribute__((used))
#else
#define USED_U64(foo) \
    static const uint64_t foo __asm__ (#foo) __attribute__((unused))
#endif
USED_U64(mmx_80w)     = 0x0080008000800080ULL;
USED_U64(mmx_10w)     = 0x1010101010101010ULL;
USED_U64(mmx_00ffw)   = 0x00ff00ff00ff00ffULL;
USED_U64(mmx_Y_coeff) = 0x253f253f253f253fULL;

USED_U64(mmx_U_green) = 0xf37df37df37df37dULL;
USED_U64(mmx_U_blue)  = 0x4093409340934093ULL;
USED_U64(mmx_V_red)   = 0x3312331233123312ULL;
USED_U64(mmx_V_green) = 0xe5fce5fce5fce5fcULL;

USED_U64(mmx_mask_f8) = 0xf8f8f8f8f8f8f8f8ULL;
USED_U64(mmx_mask_fc) = 0xfcfcfcfcfcfcfcfcULL;
#undef USED_U64

#define MMX_INIT_16 "                                                       \n\
movd      (%1), %%mm0       # Load 4 Cb       00 00 00 00 u3 u2 u1 u0       \n\
movd      (%2), %%mm1       # Load 4 Cr       00 00 00 00 v3 v2 v1 v0       \n\
pxor      %%mm4, %%mm4      # zero mm4                                      \n\
movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0       \n\
#movl      $0, (%3)         # cache preload for image                       \n\
"

#define MMX_INIT_16_GRAY "                                                  \n\
movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0       \n\
#movl      $0, (%3)         # cache preload for image                       \n\
"

#define MMX_INIT_32 "                                                       \n\
movd      (%1), %%mm0       # Load 4 Cb       00 00 00 00 u3 u2 u1 u0       \n\
movl      $0, (%3)          # cache preload for image                       \n\
movd      (%2), %%mm1       # Load 4 Cr       00 00 00 00 v3 v2 v1 v0       \n\
pxor      %%mm4, %%mm4      # zero mm4                                      \n\
movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0       \n\
"

/*
 * Do the multiply part of the conversion for even and odd pixels,
 * register usage:
 * mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
 * mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
 * mm6 -> Y even, mm7 -> Y odd
 */

#define MMX_YUV_MUL "                                                       \n\
# convert the chroma part                                                   \n\
punpcklbw %%mm4, %%mm0          # scatter 4 Cb    00 u3 00 u2 00 u1 00 u0   \n\
punpcklbw %%mm4, %%mm1          # scatter 4 Cr    00 v3 00 v2 00 v1 00 v0   \n\
psubsw    mmx_80w, %%mm0        # Cb -= 128                                 \n\
psubsw    mmx_80w, %%mm1        # Cr -= 128                                 \n\
psllw     $3, %%mm0             # Promote precision                         \n\
psllw     $3, %%mm1             # Promote precision                         \n\
movq      %%mm0, %%mm2          # Copy 4 Cb       00 u3 00 u2 00 u1 00 u0   \n\
movq      %%mm1, %%mm3          # Copy 4 Cr       00 v3 00 v2 00 v1 00 v0   \n\
pmulhw    mmx_U_green, %%mm2    # Mul Cb with green coeff -> Cb green       \n\
pmulhw    mmx_V_green, %%mm3    # Mul Cr with green coeff -> Cr green       \n\
pmulhw    mmx_U_blue, %%mm0     # Mul Cb -> Cblue 00 b3 00 b2 00 b1 00 b0   \n\
pmulhw    mmx_V_red, %%mm1      # Mul Cr -> Cred  00 r3 00 r2 00 r1 00 r0   \n\
paddsw    %%mm3, %%mm2          # Cb green + Cr green -> Cgreen             \n\
                                                                            \n\
# convert the luma part                                                     \n\
psubusb   mmx_10w, %%mm6        # Y -= 16                                   \n\
movq      %%mm6, %%mm7          # Copy 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0   \n\
pand      mmx_00ffw, %%mm6      # get Y even      00 Y6 00 Y4 00 Y2 00 Y0   \n\
psrlw     $8, %%mm7             # get Y odd       00 Y7 00 Y5 00 Y3 00 Y1   \n\
psllw     $3, %%mm6             # Promote precision                         \n\
psllw     $3, %%mm7             # Promote precision                         \n\
pmulhw    mmx_Y_coeff, %%mm6    # Mul 4 Y even    00 y6 00 y4 00 y2 00 y0   \n\
pmulhw    mmx_Y_coeff, %%mm7    # Mul 4 Y odd     00 y7 00 y5 00 y3 00 y1   \n\
"

/*
 * Do the addition part of the conversion for even and odd pixels,
 * register usage:
 * mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
 * mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels,
 * mm6 -> Y even, mm7 -> Y odd
 */

#define MMX_YUV_ADD "                                                       \n\
# Do horizontal and vertical scaling                                        \n\
movq      %%mm0, %%mm3          # Copy Cblue                                \n\
movq      %%mm1, %%mm4          # Copy Cred                                 \n\
movq      %%mm2, %%mm5          # Copy Cgreen                               \n\
paddsw    %%mm6, %%mm0          # Y even + Cblue  00 B6 00 B4 00 B2 00 B0   \n\
paddsw    %%mm7, %%mm3          # Y odd  + Cblue  00 B7 00 B5 00 B3 00 B1   \n\
paddsw    %%mm6, %%mm1          # Y even + Cred   00 R6 00 R4 00 R2 00 R0   \n\
paddsw    %%mm7, %%mm4          # Y odd  + Cred   00 R7 00 R5 00 R3 00 R1   \n\
paddsw    %%mm6, %%mm2          # Y even + Cgreen 00 G6 00 G4 00 G2 00 G0   \n\
paddsw    %%mm7, %%mm5          # Y odd  + Cgreen 00 G7 00 G5 00 G3 00 G1   \n\
                                                                            \n\
# Limit RGB even to 0..255                                                  \n\
packuswb  %%mm0, %%mm0          # B6 B4 B2 B0 / B6 B4 B2 B0                 \n\
packuswb  %%mm1, %%mm1          # R6 R4 R2 R0 / R6 R4 R2 R0                 \n\
packuswb  %%mm2, %%mm2          # G6 G4 G2 G0 / G6 G4 G2 G0                 \n\
                                                                            \n\
# Limit RGB odd to 0..255                                                   \n\
packuswb  %%mm3, %%mm3          # B7 B5 B3 B1 / B7 B5 B3 B1                 \n\
packuswb  %%mm4, %%mm4          # R7 R5 R3 R1 / R7 R5 R3 R1                 \n\
packuswb  %%mm5, %%mm5          # G7 G5 G3 G1 / G7 G5 G3 G1                 \n\
                                                                            \n\
# Interleave RGB even and odd                                               \n\
punpcklbw %%mm3, %%mm0          #                 B7 B6 B5 B4 B3 B2 B1 B0   \n\
punpcklbw %%mm4, %%mm1          #                 R7 R6 R5 R4 R3 R2 R1 R0   \n\
punpcklbw %%mm5, %%mm2          #                 G7 G6 G5 G4 G3 G2 G1 G0   \n\
"

/*
 * Grayscale case, only use Y
 */

#define MMX_YUV_GRAY "                                                      \n\
# convert the luma part                                                     \n\
psubusb   mmx_10w, %%mm6                                                    \n\
movq      %%mm6, %%mm7                                                      \n\
pand      mmx_00ffw, %%mm6                                                  \n\
psrlw     $8, %%mm7                                                         \n\
psllw     $3, %%mm6                                                         \n\
psllw     $3, %%mm7                                                         \n\
pmulhw    mmx_Y_coeff, %%mm6                                                \n\
pmulhw    mmx_Y_coeff, %%mm7                                                \n\
packuswb  %%mm6, %%mm6                                                      \n\
packuswb  %%mm7, %%mm7                                                      \n\
punpcklbw %%mm7, %%mm6                                                      \n\
"

#define MMX_UNPACK_16_GRAY "                                                \n\
movq      %%mm6, %%mm5                                                      \n\
pand      mmx_mask_f8, %%mm6                                                \n\
pand      mmx_mask_fc, %%mm5                                                \n\
movq      %%mm6, %%mm7                                                      \n\
psrlw     $3, %%mm7                                                         \n\
pxor      %%mm3, %%mm3                                                      \n\
movq      %%mm7, %%mm2                                                      \n\
movq      %%mm5, %%mm0                                                      \n\
punpcklbw %%mm3, %%mm5                                                      \n\
punpcklbw %%mm6, %%mm7                                                      \n\
psllw     $3, %%mm5                                                         \n\
por       %%mm5, %%mm7                                                      \n\
movq      %%mm7, (%3)                                                       \n\
punpckhbw %%mm3, %%mm0                                                      \n\
punpckhbw %%mm6, %%mm2                                                      \n\
psllw     $3, %%mm0                                                         \n\
movq      8(%0), %%mm6                                                      \n\
por       %%mm0, %%mm2                                                      \n\
movq      %%mm2, 8(%3)                                                      \n\
"


/*
 * convert RGB plane to RGB 15 bits,
 * mm0 -> B, mm1 -> R, mm2 -> G,
 * mm4 -> GB, mm5 -> AR pixel 4-7,
 * mm6 -> GB, mm7 -> AR pixel 0-3
 */

#define MMX_UNPACK_15 "                                                     \n\
# mask unneeded bits off                                                    \n\
pand      mmx_mask_f8, %%mm0    # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
psrlw     $3,%%mm0              # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pand      mmx_mask_f8, %%mm2    # g7g6g5g4 g3______ g7g6g5g4 g3______       \n\
pand      mmx_mask_f8, %%mm1    # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
psrlw     $1,%%mm1              # __r7r6r5 r4r3____ __r7r6r5 r4r3____       \n\
pxor      %%mm4, %%mm4          # zero mm4                                  \n\
movq      %%mm0, %%mm5          # Copy B7-B0                                \n\
movq      %%mm2, %%mm7          # Copy G7-G0                                \n\
                                                                            \n\
# convert rgb24 plane to rgb15 pack for pixel 0-3                           \n\
punpcklbw %%mm4, %%mm2          # ________ ________ g7g6g5g4 g3______       \n\
punpcklbw %%mm1, %%mm0          # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $2,%%mm2              # ________ ____g7g6 g5g4g3__ ________       \n\
por       %%mm2, %%mm0          # r7r6r5r4 r3__g7g6 g5g4g3b7 b6b5b4b3       \n\
movq      8(%0), %%mm6          # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0   \n\
movq      %%mm0, (%3)           # store pixel 0-3                           \n\
                                                                            \n\
# convert rgb24 plane to rgb16 pack for pixel 0-3                           \n\
punpckhbw %%mm4, %%mm7          # ________ ________ g7g6g5g4 g3______       \n\
punpckhbw %%mm1, %%mm5          # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $2,%%mm7              # ________ ____g7g6 g5g4g3__ ________       \n\
movd      4(%1), %%mm0          # Load 4 Cb       __ __ __ __ u3 u2 u1 u0   \n\
por       %%mm7, %%mm5          # r7r6r5r4 r3__g7g6 g5g4g3b7 b6b5b4b3       \n\
movd      4(%2), %%mm1          # Load 4 Cr       __ __ __ __ v3 v2 v1 v0   \n\
movq      %%mm5, 8(%3)          # store pixel 4-7                           \n\
"

/*
 * convert RGB plane to RGB 16 bits,
 * mm0 -> B, mm1 -> R, mm2 -> G,
 * mm4 -> GB, mm5 -> AR pixel 4-7,
 * mm6 -> GB, mm7 -> AR pixel 0-3
 */

#define MMX_UNPACK_16 "                                                     \n\
# mask unneeded bits off                                                    \n\
pand      mmx_mask_f8, %%mm0    # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
pand      mmx_mask_fc, %%mm2    # g7g6g5g4 g3g2____ g7g6g5g4 g3g2____       \n\
pand      mmx_mask_f8, %%mm1    # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
psrlw     $3,%%mm0              # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pxor      %%mm4, %%mm4          # zero mm4                                  \n\
movq      %%mm0, %%mm5          # Copy B7-B0                                \n\
movq      %%mm2, %%mm7          # Copy G7-G0                                \n\
                                                                            \n\
# convert rgb24 plane to rgb16 pack for pixel 0-3                           \n\
punpcklbw %%mm4, %%mm2          # ________ ________ g7g6g5g4 g3g2____       \n\
punpcklbw %%mm1, %%mm0          # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%mm2              # ________ __g7g6g5 g4g3g2__ ________       \n\
por       %%mm2, %%mm0          # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movq      8(%0), %%mm6          # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0   \n\
movq      %%mm0, (%3)           # store pixel 0-3                           \n\
                                                                            \n\
# convert rgb24 plane to rgb16 pack for pixel 0-3                           \n\
punpckhbw %%mm4, %%mm7          # ________ ________ g7g6g5g4 g3g2____       \n\
punpckhbw %%mm1, %%mm5          # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%mm7              # ________ __g7g6g5 g4g3g2__ ________       \n\
movd      4(%1), %%mm0          # Load 4 Cb       __ __ __ __ u3 u2 u1 u0   \n\
por       %%mm7, %%mm5          # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movd      4(%2), %%mm1          # Load 4 Cr       __ __ __ __ v3 v2 v1 v0   \n\
movq      %%mm5, 8(%3)          # store pixel 4-7                           \n\
"

/*
 * convert RGB plane to RGB packed format,
 * mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0,
 * mm4 -> GB, mm5 -> AR pixel 4-7,
 * mm6 -> GB, mm7 -> AR pixel 0-3
 */

#define MMX_UNPACK_32 "                                                     \n\
pxor      %%mm3, %%mm3  # zero mm3                                          \n\
movq      %%mm0, %%mm6  #                 B7 B6 B5 B4 B3 B2 B1 B0           \n\
movq      %%mm1, %%mm7  #                 R7 R6 R5 R4 R3 R2 R1 R0           \n\
movq      %%mm0, %%mm4  #                 B7 B6 B5 B4 B3 B2 B1 B0           \n\
movq      %%mm1, %%mm5  #                 R7 R6 R5 R4 R3 R2 R1 R0           \n\
punpcklbw %%mm2, %%mm6  #                 G3 B3 G2 B2 G1 B1 G0 B0           \n\
punpcklbw %%mm3, %%mm7  #                 00 R3 00 R2 00 R1 00 R0           \n\
punpcklwd %%mm7, %%mm6  #                 00 R1 B1 G1 00 R0 B0 G0           \n\
movq      %%mm6, (%3)   # Store ARGB1 ARGB0                                 \n\
movq      %%mm0, %%mm6  #                 B7 B6 B5 B4 B3 B2 B1 B0           \n\
punpcklbw %%mm2, %%mm6  #                 G3 B3 G2 B2 G1 B1 G0 B0           \n\
punpckhwd %%mm7, %%mm6  #                 00 R3 G3 B3 00 R2 B3 G2           \n\
movq      %%mm6, 8(%3)  # Store ARGB3 ARGB2                                 \n\
punpckhbw %%mm2, %%mm4  #                 G7 B7 G6 B6 G5 B5 G4 B4           \n\
punpckhbw %%mm3, %%mm5  #                 00 R7 00 R6 00 R5 00 R4           \n\
punpcklwd %%mm5, %%mm4  #                 00 R5 B5 G5 00 R4 B4 G4           \n\
movq      %%mm4, 16(%3) # Store ARGB5 ARGB4                                 \n\
movq      %%mm0, %%mm4  #                 B7 B6 B5 B4 B3 B2 B1 B0           \n\
punpckhbw %%mm2, %%mm4  #                 G7 B7 G6 B6 G5 B5 G4 B4           \n\
punpckhwd %%mm5, %%mm4  #                 00 R7 G7 B7 00 R6 B6 G6           \n\
movq      %%mm4, 24(%3) # Store ARGB7 ARGB6                                 \n\
                                                                            \n\
#movd      4(%1), %%mm0  # Load 4 Cb       00 00 00 00 u3 u2 u1 u0           \n\
#movd      4(%2), %%mm1  # Load 4 Cr       00 00 00 00 v3 v2 v1 v0           \n\
#pxor      %%mm4, %%mm4  # zero mm4                                          \n\
#movq      8(%0), %%mm6  # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0           \n\
"

