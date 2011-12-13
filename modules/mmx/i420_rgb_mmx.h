/*****************************************************************************
 * i420_rgb_mmx.h: MMX YUV transformation assembly
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Olie Lho <ollie@sis.com.tw>
 *          Gaël Hendryckx <jimmy@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damienf@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef MODULE_NAME_IS_i420_rgb_mmx

/* hope these constant values are cache line aligned */
static const uint64_t mmx_80w     = 0x0080008000800080ULL; /* Will be referenced as %4 in inline asm */
static const uint64_t mmx_10w     = 0x1010101010101010ULL; /* -- as %5 */
static const uint64_t mmx_00ffw   = 0x00ff00ff00ff00ffULL; /* -- as %6 */
static const uint64_t mmx_Y_coeff = 0x253f253f253f253fULL; /* -- as %7 */

static const uint64_t mmx_U_green = 0xf37df37df37df37dULL; /* -- as %8 */
static const uint64_t mmx_U_blue  = 0x4093409340934093ULL; /* -- as %9 */
static const uint64_t mmx_V_red   = 0x3312331233123312ULL; /* -- as %10 */
static const uint64_t mmx_V_green = 0xe5fce5fce5fce5fcULL; /* -- as %11 */

static const uint64_t mmx_mask_f8 = 0xf8f8f8f8f8f8f8f8ULL; /* -- as %12 */
static const uint64_t mmx_mask_fc = 0xfcfcfcfcfcfcfcfcULL; /* -- as %13 */

#if defined(CAN_COMPILE_MMX)

/* MMX assembly */
 
#define MMX_CALL(MMX_INSTRUCTIONS)      \
    do {                                \
    __asm__ __volatile__(               \
        ".p2align 3 \n\t"               \
        MMX_INSTRUCTIONS                \
        :                               \
        : "r" (p_y), "r" (p_u),         \
          "r" (p_v), "r" (p_buffer),    \
	  "m" (mmx_80w), "m" (mmx_10w), \
	  "m" (mmx_00ffw), "m" (mmx_Y_coeff), \
	  "m" (mmx_U_green), "m" (mmx_U_blue), \
	  "m" (mmx_V_red), "m" (mmx_V_green), \
	  "m" (mmx_mask_f8), "m" (mmx_mask_fc) \
        : "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7" );  \
    } while(0)

#define MMX_END __asm__ __volatile__ ( "emms" )

#define MMX_INIT_16 "                                                       \n\
movd       (%1), %%mm0      # Load 4 Cb       00 00 00 00 u3 u2 u1 u0       \n\
movd       (%2), %%mm1      # Load 4 Cr       00 00 00 00 v3 v2 v1 v0       \n\
pxor      %%mm4, %%mm4      # zero mm4                                      \n\
movq       (%0), %%mm6      # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0       \n\
"

#define MMX_INIT_16_GRAY "                                                  \n\
movq      (%0), %%mm6       # Load 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0       \n\
#movl      $0, (%3)         # cache preload for image                       \n\
"

#define MMX_INIT_32 "                                                       \n\
movd      (%1), %%mm0       # Load 4 Cb       00 00 00 00 u3 u2 u1 u0       \n\
movl        $0, (%3)        # cache preload for image                       \n\
movd      (%2), %%mm1       # Load 4 Cr       00 00 00 00 v3 v2 v1 v0       \n\
pxor     %%mm4, %%mm4       # zero mm4                                      \n\
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
psubsw    %4, %%mm0     # Cb -= 128                                 \n\
psubsw    %4, %%mm1     # Cr -= 128                                 \n\
psllw     $3, %%mm0             # Promote precision                         \n\
psllw     $3, %%mm1             # Promote precision                         \n\
movq      %%mm0, %%mm2          # Copy 4 Cb       00 u3 00 u2 00 u1 00 u0   \n\
movq      %%mm1, %%mm3          # Copy 4 Cr       00 v3 00 v2 00 v1 00 v0   \n\
pmulhw    %8, %%mm2 # Mul Cb with green coeff -> Cb green       \n\
pmulhw    %11, %%mm3 # Mul Cr with green coeff -> Cr green       \n\
pmulhw    %9, %%mm0  # Mul Cb -> Cblue 00 b3 00 b2 00 b1 00 b0   \n\
pmulhw    %10, %%mm1   # Mul Cr -> Cred  00 r3 00 r2 00 r1 00 r0   \n\
paddsw    %%mm3, %%mm2          # Cb green + Cr green -> Cgreen             \n\
                                                                            \n\
# convert the luma part                                                     \n\
psubusb   %5, %%mm6     # Y -= 16                                   \n\
movq      %%mm6, %%mm7          # Copy 8 Y        Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0   \n\
pand      %6, %%mm6   # get Y even      00 Y6 00 Y4 00 Y2 00 Y0   \n\
psrlw     $8, %%mm7             # get Y odd       00 Y7 00 Y5 00 Y3 00 Y1   \n\
psllw     $3, %%mm6             # Promote precision                         \n\
psllw     $3, %%mm7             # Promote precision                         \n\
pmulhw    %7, %%mm6 # Mul 4 Y even    00 y6 00 y4 00 y2 00 y0   \n\
pmulhw    %7, %%mm7 # Mul 4 Y odd     00 y7 00 y5 00 y3 00 y1   \n\
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
psubusb   %5, %%mm6                                                 \n\
movq      %%mm6, %%mm7                                                      \n\
pand      %6, %%mm6                                               \n\
psrlw     $8, %%mm7                                                         \n\
psllw     $3, %%mm6                                                         \n\
psllw     $3, %%mm7                                                         \n\
pmulhw    %7, %%mm6                                             \n\
pmulhw    %7, %%mm7                                             \n\
packuswb  %%mm6, %%mm6                                                      \n\
packuswb  %%mm7, %%mm7                                                      \n\
punpcklbw %%mm7, %%mm6                                                      \n\
"

#define MMX_UNPACK_16_GRAY "                                                \n\
movq      %%mm6, %%mm5                                                      \n\
pand      %12, %%mm6                                             \n\
pand      %13, %%mm5                                             \n\
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
pand      %12, %%mm0 # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
psrlw     $3,%%mm0              # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pand      %12, %%mm2 # g7g6g5g4 g3______ g7g6g5g4 g3______       \n\
pand      %12, %%mm1 # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
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
pand      %12, %%mm0 # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
pand      %13, %%mm2 # g7g6g5g4 g3g2____ g7g6g5g4 g3g2____       \n\
pand      %12, %%mm1 # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
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
 * mm0 -> B, mm1 -> R, mm2 -> G
 */

#define MMX_UNPACK_32_ARGB "                                                \n\
pxor      %%mm3, %%mm3  # zero mm3                                          \n\
movq      %%mm0, %%mm4  #                 B7 B6 B5 B4 B3 B2 B1 B0           \n\
punpcklbw %%mm2, %%mm4  #                 G3 B3 G2 B2 G1 B1 G0 B0           \n\
movq      %%mm1, %%mm5  #                 R7 R6 R5 R4 R3 R2 R1 R0           \n\
punpcklbw %%mm3, %%mm5  #                 00 R3 00 R2 00 R1 00 R0           \n\
movq      %%mm4, %%mm6  #                 G3 B3 G2 B2 G1 B1 G0 B0           \n\
punpcklwd %%mm5, %%mm4  #                 00 R1 B1 G1 00 R0 B0 G0           \n\
movq      %%mm4, (%3)   # Store ARGB1 ARGB0                                 \n\
punpckhwd %%mm5, %%mm6  #                 00 R3 B3 G3 00 R2 B2 G2           \n\
movq      %%mm6, 8(%3)  # Store ARGB3 ARGB2                                 \n\
punpckhbw %%mm2, %%mm0  #                 G7 B7 G6 B6 G5 B5 G4 B4           \n\
punpckhbw %%mm3, %%mm1  #                 00 R7 00 R6 00 R5 00 R4           \n\
movq      %%mm0, %%mm5  #                 G7 B7 G6 B6 G5 B5 G4 B4           \n\
punpcklwd %%mm1, %%mm5  #                 00 R5 B5 G5 00 R4 B4 G4           \n\
movq      %%mm5, 16(%3) # Store ARGB5 ARGB4                                 \n\
punpckhwd %%mm1, %%mm0  #                 00 R7 B7 G7 00 R6 B6 G6           \n\
movq      %%mm0, 24(%3) # Store ARGB7 ARGB6                                 \n\
"

#define MMX_UNPACK_32_RGBA "                                                \n\
pxor      %%mm3, %%mm3  # zero mm3                                          \n\
movq      %%mm2, %%mm4  #                 G7 G6 G5 G4 G3 G2 G1 G0           \n\
punpcklbw %%mm1, %%mm4  #                 R3 G3 R2 G2 R1 G1 R0 G0           \n\
punpcklbw %%mm0, %%mm3  #                 B3 00 B2 00 B1 00 B0 00           \n\
movq      %%mm3, %%mm5  #                 R3 00 R2 00 R1 00 R0 00           \n\
punpcklwd %%mm4, %%mm3  #                 R1 G1 B1 00 R0 G0 B0 00           \n\
movq      %%mm3, (%3)   # Store RGBA1 RGBA0                                 \n\
punpckhwd %%mm4, %%mm5  #                 R3 G3 B3 00 R2 G2 B2 00           \n\
movq      %%mm5, 8(%3)  # Store RGBA3 RGBA2                                 \n\
pxor      %%mm6, %%mm6  # zero mm6                                          \n\
punpckhbw %%mm1, %%mm2  #                 R7 G7 R6 G6 R5 G5 R4 G4           \n\
punpckhbw %%mm0, %%mm6  #                 B7 00 B6 00 B5 00 B4 00           \n\
movq      %%mm6, %%mm0  #                 B7 00 B6 00 B5 00 B4 00           \n\
punpcklwd %%mm2, %%mm6  #                 R5 G5 B5 00 R4 G4 B4 00           \n\
movq      %%mm6, 16(%3) # Store RGBA5 RGBA4                                 \n\
punpckhwd %%mm2, %%mm0  #                 R7 G7 B7 00 R6 G6 B6 00           \n\
movq      %%mm0, 24(%3) # Store RGBA7 RGBA6                                 \n\
"

#define MMX_UNPACK_32_BGRA "                                                \n\
pxor      %%mm3, %%mm3  # zero mm3                                          \n\
movq      %%mm2, %%mm4  #                 G7 G6 G5 G4 G3 G2 G1 G0           \n\
punpcklbw %%mm0, %%mm4  #                 B3 G3 B2 G2 B1 G1 B0 G0           \n\
punpcklbw %%mm1, %%mm3  #                 R3 00 R2 00 R1 00 R0 00           \n\
movq      %%mm3, %%mm5  #                 R3 00 R2 00 R1 00 R0 00           \n\
punpcklwd %%mm4, %%mm3  #                 B1 G1 R1 00 B0 G0 R0 00           \n\
movq      %%mm3, (%3)   # Store BGRA1 BGRA0                                 \n\
punpckhwd %%mm4, %%mm5  #                 B3 G3 R3 00 B2 G2 R2 00           \n\
movq      %%mm5, 8(%3)  # Store BGRA3 BGRA2                                 \n\
pxor      %%mm6, %%mm6  # zero mm6                                          \n\
punpckhbw %%mm0, %%mm2  #                 B7 G7 B6 G6 B5 G5 B4 G4           \n\
punpckhbw %%mm1, %%mm6  #                 R7 00 R6 00 R5 00 R4 00           \n\
movq      %%mm6, %%mm0  #                 R7 00 R6 00 R5 00 R4 00           \n\
punpcklwd %%mm2, %%mm6  #                 B5 G5 R5 00 B4 G4 R4 00           \n\
movq      %%mm6, 16(%3) # Store BGRA5 BGRA4                                 \n\
punpckhwd %%mm2, %%mm0  #                 B7 G7 R7 00 B6 G6 R6 00           \n\
movq      %%mm0, 24(%3) # Store BGRA7 BGRA6                                 \n\
"

#define MMX_UNPACK_32_ABGR "                                                \n\
pxor      %%mm3, %%mm3  # zero mm3                                          \n\
movq      %%mm1, %%mm4  #                 R7 R6 R5 R4 R3 R2 R1 R0           \n\
punpcklbw %%mm2, %%mm4  #                 G3 R3 G2 R2 G1 R1 G0 R0           \n\
movq      %%mm0, %%mm5  #                 B7 B6 B5 B4 B3 B2 B1 B0           \n\
punpcklbw %%mm3, %%mm5  #                 00 B3 00 B2 00 B1 00 B0           \n\
movq      %%mm4, %%mm6  #                 G3 R3 G2 R2 G1 R1 G0 R0           \n\
punpcklwd %%mm5, %%mm4  #                 00 B1 G1 R1 00 B0 G0 R0           \n\
movq      %%mm4, (%3)   # Store ABGR1 ABGR0                                 \n\
punpckhwd %%mm5, %%mm6  #                 00 B3 G3 R3 00 B2 G2 R2           \n\
movq      %%mm6, 8(%3)  # Store ABGR3 ABGR2                                 \n\
punpckhbw %%mm2, %%mm1  #                 G7 R7 G6 R6 G5 R5 G4 R4           \n\
punpckhbw %%mm3, %%mm0  #                 00 B7 00 B6 00 B5 00 B4           \n\
movq      %%mm1, %%mm2  #                 G7 R7 G6 R6 G5 R5 G4 R4           \n\
punpcklwd %%mm0, %%mm1  #                 00 B5 G5 R5 00 B4 G4 R4           \n\
movq      %%mm1, 16(%3) # Store ABGR5 ABGR4                                 \n\
punpckhwd %%mm0, %%mm2  #                 B7 G7 R7 00 B6 G6 R6 00           \n\
movq      %%mm2, 24(%3) # Store ABGR7 ABGR6                                 \n\
"

#elif defined(HAVE_MMX_INTRINSICS)

/* MMX intrinsics */

#include <mmintrin.h>

#define MMX_CALL(MMX_INSTRUCTIONS)  \
    do {                            \
        __m64 mm0, mm1, mm2, mm3,   \
              mm4, mm5, mm6, mm7;   \
        MMX_INSTRUCTIONS            \
    } while(0)

#define MMX_END _mm_empty()
 
#define MMX_INIT_16                     \
    mm0 = _mm_cvtsi32_si64(*(int*)p_u); \
    mm1 = _mm_cvtsi32_si64(*(int*)p_v); \
    mm4 = _mm_setzero_si64();           \
    mm6 = (__m64)*(uint64_t *)p_y;

#define MMX_INIT_32                     \
    mm0 = _mm_cvtsi32_si64(*(int*)p_u); \
    *(uint16_t *)p_buffer = 0;          \
    mm1 = _mm_cvtsi32_si64(*(int*)p_v); \
    mm4 = _mm_setzero_si64();           \
    mm6 = (__m64)*(uint64_t *)p_y;

#define MMX_YUV_MUL                                 \
    mm0 = _mm_unpacklo_pi8(mm0, mm4);               \
    mm1 = _mm_unpacklo_pi8(mm1, mm4);               \
    mm0 = _mm_subs_pi16(mm0, (__m64)mmx_80w);       \
    mm1 = _mm_subs_pi16(mm1, (__m64)mmx_80w);       \
    mm0 = _mm_slli_pi16(mm0, 3);                    \
    mm1 = _mm_slli_pi16(mm1, 3);                    \
    mm2 = mm0;                                      \
    mm3 = mm1;                                      \
    mm2 = _mm_mulhi_pi16(mm2, (__m64)mmx_U_green);  \
    mm3 = _mm_mulhi_pi16(mm3, (__m64)mmx_V_green);  \
    mm0 = _mm_mulhi_pi16(mm0, (__m64)mmx_U_blue);   \
    mm1 = _mm_mulhi_pi16(mm1, (__m64)mmx_V_red);    \
    mm2 = _mm_adds_pi16(mm2, mm3);                  \
    \
    mm6 = _mm_subs_pu8(mm6, (__m64)mmx_10w);        \
    mm7 = mm6;                                      \
    mm6 = _mm_and_si64(mm6, (__m64)mmx_00ffw);      \
    mm7 = _mm_srli_pi16(mm7, 8);                    \
    mm6 = _mm_slli_pi16(mm6, 3);                    \
    mm7 = _mm_slli_pi16(mm7, 3);                    \
    mm6 = _mm_mulhi_pi16(mm6, (__m64)mmx_Y_coeff);  \
    mm7 = _mm_mulhi_pi16(mm7, (__m64)mmx_Y_coeff);

#define MMX_YUV_ADD                     \
    mm3 = mm0;                          \
    mm4 = mm1;                          \
    mm5 = mm2;                          \
    mm0 = _mm_adds_pi16(mm0, mm6);      \
    mm3 = _mm_adds_pi16(mm3, mm7);      \
    mm1 = _mm_adds_pi16(mm1, mm6);      \
    mm4 = _mm_adds_pi16(mm4, mm7);      \
    mm2 = _mm_adds_pi16(mm2, mm6);      \
    mm5 = _mm_adds_pi16(mm5, mm7);      \
    \
    mm0 = _mm_packs_pu16(mm0, mm0);     \
    mm1 = _mm_packs_pu16(mm1, mm1);     \
    mm2 = _mm_packs_pu16(mm2, mm2);     \
    \
    mm3 = _mm_packs_pu16(mm3, mm3);     \
    mm4 = _mm_packs_pu16(mm4, mm4);     \
    mm5 = _mm_packs_pu16(mm5, mm5);     \
    \
    mm0 = _mm_unpacklo_pi8(mm0, mm3);   \
    mm1 = _mm_unpacklo_pi8(mm1, mm4);   \
    mm2 = _mm_unpacklo_pi8(mm2, mm5);

#define MMX_UNPACK_15                               \
    mm0 = _mm_and_si64(mm0, (__m64)mmx_mask_f8);    \
    mm0 = _mm_srli_pi16(mm0, 3);                    \
    mm2 = _mm_and_si64(mm2, (__m64)mmx_mask_f8);    \
    mm1 = _mm_and_si64(mm1, (__m64)mmx_mask_f8);    \
    mm1 = _mm_srli_pi16(mm1, 1);                    \
    mm4 = _mm_setzero_si64();                       \
    mm5 = mm0;                                      \
    mm7 = mm2;                                      \
    \
    mm2 = _mm_unpacklo_pi8(mm2, mm4);               \
    mm0 = _mm_unpacklo_pi8(mm0, mm1);               \
    mm2 = _mm_slli_pi16(mm2, 2);                    \
    mm0 = _mm_or_si64(mm0, mm2);                    \
    mm6 = (__m64)*(uint64_t *)(p_y + 8);            \
    *(uint64_t *)p_buffer = (uint64_t)mm0;          \
    \
    mm7 = _mm_unpackhi_pi8(mm7, mm4);               \
    mm5 = _mm_unpackhi_pi8(mm5, mm1);               \
    mm7 = _mm_slli_pi16(mm7, 2);                    \
    mm0 = _mm_cvtsi32_si64((int)*(uint32_t *)(p_u + 4)); \
    mm5 = _mm_or_si64(mm5, mm7);                    \
    mm1 = _mm_cvtsi32_si64((int)*(uint32_t *)(p_v + 4)); \
    *(uint64_t *)(p_buffer + 4) = (uint64_t)mm5;

#define MMX_UNPACK_16                               \
    mm0 = _mm_and_si64(mm0, (__m64)mmx_mask_f8);    \
    mm2 = _mm_and_si64(mm2, (__m64)mmx_mask_fc);    \
    mm1 = _mm_and_si64(mm1, (__m64)mmx_mask_f8);    \
    mm0 = _mm_srli_pi16(mm0, 3);                    \
    mm4 = _mm_setzero_si64();                       \
    mm5 = mm0;                                      \
    mm7 = mm2;                                      \
    \
    mm2 = _mm_unpacklo_pi8(mm2, mm4);               \
    mm0 = _mm_unpacklo_pi8(mm0, mm1);               \
    mm2 = _mm_slli_pi16(mm2, 3);                    \
    mm0 = _mm_or_si64(mm0, mm2);                    \
    mm6 = (__m64)*(uint64_t *)(p_y + 8);            \
    *(uint64_t *)p_buffer = (uint64_t)mm0;          \
    \
    mm7 = _mm_unpackhi_pi8(mm7, mm4);               \
    mm5 = _mm_unpackhi_pi8(mm5, mm1);               \
    mm7 = _mm_slli_pi16(mm7, 3);                    \
    mm0 = _mm_cvtsi32_si64((int)*(uint32_t *)(p_u + 4)); \
    mm5 = _mm_or_si64(mm5, mm7);                    \
    mm1 = _mm_cvtsi32_si64((int)*(uint32_t *)(p_v + 4)); \
    *(uint64_t *)(p_buffer + 4) = (uint64_t)mm5;

#define MMX_UNPACK_32_ARGB                      \
    mm3 = _mm_setzero_si64();                   \
    mm4 = mm0;                                  \
    mm4 = _mm_unpacklo_pi8(mm4, mm2);           \
    mm5 = mm1;                                  \
    mm5 = _mm_unpacklo_pi8(mm5, mm3);           \
    mm6 = mm4;                                  \
    mm4 = _mm_unpacklo_pi16(mm4, mm5);          \
    *(uint64_t *)p_buffer = (uint64_t)mm4;      \
    mm6 = _mm_unpackhi_pi16(mm6, mm5);          \
    *(uint64_t *)(p_buffer + 2) = (uint64_t)mm6;\
    mm0 = _mm_unpackhi_pi8(mm0, mm2);           \
    mm1 = _mm_unpackhi_pi8(mm1, mm3);           \
    mm5 = mm0;                                  \
    mm5 = _mm_unpacklo_pi16(mm5, mm1);          \
    *(uint64_t *)(p_buffer + 4) = (uint64_t)mm5;\
    mm0 = _mm_unpackhi_pi16(mm0, mm1);          \
    *(uint64_t *)(p_buffer + 6) = (uint64_t)mm0;

#define MMX_UNPACK_32_RGBA                      \
    mm3 = _mm_setzero_si64();                   \
    mm4 = mm2;                                  \
    mm4 = _mm_unpacklo_pi8(mm4, mm1);           \
    mm3 = _mm_unpacklo_pi8(mm3, mm0);           \
    mm5 = mm3;                                  \
    mm3 = _mm_unpacklo_pi16(mm3, mm4);          \
    *(uint64_t *)p_buffer = (uint64_t)mm3;      \
    mm5 = _mm_unpackhi_pi16(mm5, mm4);          \
    *(uint64_t *)(p_buffer + 2) = (uint64_t)mm5;\
    mm6 = _mm_setzero_si64();                   \
    mm2 = _mm_unpackhi_pi8(mm2, mm1);           \
    mm6 = _mm_unpackhi_pi8(mm6, mm0);           \
    mm0 = mm6;                                  \
    mm6 = _mm_unpacklo_pi16(mm6, mm2);          \
    *(uint64_t *)(p_buffer + 4) = (uint64_t)mm6;\
    mm0 = _mm_unpackhi_pi16(mm0, mm2);          \
    *(uint64_t *)(p_buffer + 6) = (uint64_t)mm0;

#define MMX_UNPACK_32_BGRA                      \
    mm3 = _mm_setzero_si64();                   \
    mm4 = mm2;                                  \
    mm4 = _mm_unpacklo_pi8(mm4, mm0);           \
    mm3 = _mm_unpacklo_pi8(mm3, mm1);           \
    mm5 = mm3;                                  \
    mm3 = _mm_unpacklo_pi16(mm3, mm4);          \
    *(uint64_t *)p_buffer = (uint64_t)mm3;      \
    mm5 = _mm_unpackhi_pi16(mm5, mm4);          \
    *(uint64_t *)(p_buffer + 2) = (uint64_t)mm5;\
    mm6 = _mm_setzero_si64();                   \
    mm2 = _mm_unpackhi_pi8(mm2, mm0);           \
    mm6 = _mm_unpackhi_pi8(mm6, mm1);           \
    mm0 = mm6;                                  \
    mm6 = _mm_unpacklo_pi16(mm6, mm2);          \
    *(uint64_t *)(p_buffer + 4) = (uint64_t)mm6;\
    mm0 = _mm_unpackhi_pi16(mm0, mm2);          \
    *(uint64_t *)(p_buffer + 6) = (uint64_t)mm0;

#define MMX_UNPACK_32_ABGR                      \
    mm3 = _mm_setzero_si64();                   \
    mm4 = mm1;                                  \
    mm4 = _mm_unpacklo_pi8(mm4, mm2);           \
    mm5 = mm0;                                  \
    mm5 = _mm_unpacklo_pi8(mm5, mm3);           \
    mm6 = mm4;                                  \
    mm4 = _mm_unpacklo_pi16(mm4, mm5);          \
    *(uint64_t *)p_buffer = (uint64_t)mm4;      \
    mm6 = _mm_unpackhi_pi16(mm6, mm5);          \
    *(uint64_t *)(p_buffer + 2) = (uint64_t)mm6;\
    mm1 = _mm_unpackhi_pi8(mm1, mm2);           \
    mm0 = _mm_unpackhi_pi8(mm0, mm3);           \
    mm2 = mm1;                                  \
    mm1 = _mm_unpacklo_pi16(mm1, mm0);          \
    *(uint64_t *)(p_buffer + 4) = (uint64_t)mm1;\
    mm2 = _mm_unpackhi_pi16(mm2, mm0);          \
    *(uint64_t *)(p_buffer + 6) = (uint64_t)mm2;

#endif

#elif defined( MODULE_NAME_IS_i420_rgb_sse2 )

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

#define SSE2_INIT_16_ALIGNED "                                              \n\
movq        (%1), %%xmm0    # Load 8 Cb       00 00 00 00 u3 u2 u1 u0       \n\
movq        (%2), %%xmm1    # Load 8 Cr       00 00 00 00 v3 v2 v1 v0       \n\
pxor      %%xmm4, %%xmm4    # zero mm4                                      \n\
movdqa      (%0), %%xmm6    # Load 16 Y       Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0       \n\
"

#define SSE2_INIT_16_UNALIGNED "                                            \n\
movq        (%1), %%xmm0    # Load 8 Cb       00 00 00 00 u3 u2 u1 u0       \n\
movq        (%2), %%xmm1    # Load 8 Cr       00 00 00 00 v3 v2 v1 v0       \n\
pxor      %%xmm4, %%xmm4    # zero mm4                                      \n\
movdqu      (%0), %%xmm6    # Load 16 Y       Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0       \n\
prefetchnta (%3)            # Tell CPU not to cache output RGB data         \n\
"

#define SSE2_INIT_32_ALIGNED "                                              \n\
movq        (%1), %%xmm0    # Load 8 Cb       00 00 00 00 u3 u2 u1 u0       \n\
movq        (%2), %%xmm1    # Load 8 Cr       00 00 00 00 v3 v2 v1 v0       \n\
pxor      %%xmm4, %%xmm4    # zero mm4                                      \n\
movdqa      (%0), %%xmm6    # Load 16 Y       Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0       \n\
"

#define SSE2_INIT_32_UNALIGNED "                                            \n\
movq        (%1), %%xmm0    # Load 8 Cb       00 00 00 00 u3 u2 u1 u0       \n\
movq        (%2), %%xmm1    # Load 8 Cr       00 00 00 00 v3 v2 v1 v0       \n\
pxor      %%xmm4, %%xmm4    # zero mm4                                      \n\
movdqu      (%0), %%xmm6    # Load 16 Y       Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0       \n\
prefetchnta (%3)            # Tell CPU not to cache output RGB data         \n\
"

#define SSE2_YUV_MUL "                                                      \n\
# convert the chroma part                                                   \n\
punpcklbw %%xmm4, %%xmm0        # scatter 8 Cb    00 u3 00 u2 00 u1 00 u0   \n\
punpcklbw %%xmm4, %%xmm1        # scatter 8 Cr    00 v3 00 v2 00 v1 00 v0   \n\
movl      $0x00800080, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     0080 0080 ... 0080 0080   \n\
psubsw    %%xmm5, %%xmm0        # Cb -= 128                                 \n\
psubsw    %%xmm5, %%xmm1        # Cr -= 128                                 \n\
psllw     $3, %%xmm0            # Promote precision                         \n\
psllw     $3, %%xmm1            # Promote precision                         \n\
movdqa    %%xmm0, %%xmm2        # Copy 8 Cb       00 u3 00 u2 00 u1 00 u0   \n\
movdqa    %%xmm1, %%xmm3        # Copy 8 Cr       00 v3 00 v2 00 v1 00 v0   \n\
movl      $0xf37df37d, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     f37d f37d ... f37d f37d   \n\
pmulhw    %%xmm5, %%xmm2        # Mul Cb with green coeff -> Cb green       \n\
movl      $0xe5fce5fc, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     e5fc e5fc ... e5fc e5fc   \n\
pmulhw    %%xmm5, %%xmm3        # Mul Cr with green coeff -> Cr green       \n\
movl      $0x40934093, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     4093 4093 ... 4093 4093   \n\
pmulhw    %%xmm5, %%xmm0        # Mul Cb -> Cblue 00 b3 00 b2 00 b1 00 b0   \n\
movl      $0x33123312, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to     3312 3312 ... 3312 3312   \n\
pmulhw    %%xmm5, %%xmm1        # Mul Cr -> Cred  00 r3 00 r2 00 r1 00 r0   \n\
paddsw    %%xmm3, %%xmm2        # Cb green + Cr green -> Cgreen             \n\
                                                                            \n\
# convert the luma part                                                     \n\
movl      $0x10101010, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # Set xmm5 to   1010 1010 ... 1010 1010     \n\
psubusb   %%xmm5, %%xmm6        # Y -= 16                                   \n\
movdqa    %%xmm6, %%xmm7        # Copy 16 Y       Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0   \n\
movl      $0x00ff00ff, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     00ff 00ff ... 00ff 00ff   \n\
pand      %%xmm5, %%xmm6        # get Y even      00 Y6 00 Y4 00 Y2 00 Y0   \n\
psrlw     $8, %%xmm7            # get Y odd       00 Y7 00 Y5 00 Y3 00 Y1   \n\
psllw     $3, %%xmm6            # Promote precision                         \n\
psllw     $3, %%xmm7            # Promote precision                         \n\
movl      $0x253f253f, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     253f 253f ... 253f 253f   \n\
pmulhw    %%xmm5, %%xmm6        # Mul 8 Y even    00 y6 00 y4 00 y2 00 y0   \n\
pmulhw    %%xmm5, %%xmm7        # Mul 8 Y odd     00 y7 00 y5 00 y3 00 y1   \n\
"

#define SSE2_YUV_ADD "                                                      \n\
# Do horizontal and vertical scaling                                        \n\
movdqa    %%xmm0, %%xmm3        # Copy Cblue                                \n\
movdqa    %%xmm1, %%xmm4        # Copy Cred                                 \n\
movdqa    %%xmm2, %%xmm5        # Copy Cgreen                               \n\
paddsw    %%xmm6, %%xmm0        # Y even + Cblue  00 B6 00 B4 00 B2 00 B0   \n\
paddsw    %%xmm7, %%xmm3        # Y odd  + Cblue  00 B7 00 B5 00 B3 00 B1   \n\
paddsw    %%xmm6, %%xmm1        # Y even + Cred   00 R6 00 R4 00 R2 00 R0   \n\
paddsw    %%xmm7, %%xmm4        # Y odd  + Cred   00 R7 00 R5 00 R3 00 R1   \n\
paddsw    %%xmm6, %%xmm2        # Y even + Cgreen 00 G6 00 G4 00 G2 00 G0   \n\
paddsw    %%xmm7, %%xmm5        # Y odd  + Cgreen 00 G7 00 G5 00 G3 00 G1   \n\
                                                                            \n\
# Limit RGB even to 0..255                                                  \n\
packuswb  %%xmm0, %%xmm0        # B6 B4 B2 B0 / B6 B4 B2 B0                 \n\
packuswb  %%xmm1, %%xmm1        # R6 R4 R2 R0 / R6 R4 R2 R0                 \n\
packuswb  %%xmm2, %%xmm2        # G6 G4 G2 G0 / G6 G4 G2 G0                 \n\
                                                                            \n\
# Limit RGB odd to 0..255                                                   \n\
packuswb  %%xmm3, %%xmm3        # B7 B5 B3 B1 / B7 B5 B3 B1                 \n\
packuswb  %%xmm4, %%xmm4        # R7 R5 R3 R1 / R7 R5 R3 R1                 \n\
packuswb  %%xmm5, %%xmm5        # G7 G5 G3 G1 / G7 G5 G3 G1                 \n\
                                                                            \n\
# Interleave RGB even and odd                                               \n\
punpcklbw %%xmm3, %%xmm0        #                 B7 B6 B5 B4 B3 B2 B1 B0   \n\
punpcklbw %%xmm4, %%xmm1        #                 R7 R6 R5 R4 R3 R2 R1 R0   \n\
punpcklbw %%xmm5, %%xmm2        #                 G7 G6 G5 G4 G3 G2 G1 G0   \n\
"

#define SSE2_UNPACK_15_ALIGNED "                                            \n\
# mask unneeded bits off                                                    \n\
movl      $0xf8f8f8f8, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8f8 f8f8 ... f8f8 f8f8   \n\
pand      %%xmm5, %%xmm0        # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
psrlw     $3,%%xmm0             # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pand      %%xmm5, %%xmm2        # g7g6g5g4 g3______ g7g6g5g4 g3______       \n\
pand      %%xmm5, %%xmm1        # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
psrlw     $1,%%xmm1             # __r7r6r5 r4r3____ __r7r6r5 r4r3____       \n\
pxor      %%xmm4, %%xmm4        # zero mm4                                  \n\
movdqa    %%xmm0, %%xmm5        # Copy B15-B0                               \n\
movdqa    %%xmm2, %%xmm7        # Copy G15-G0                               \n\
                                                                            \n\
# convert rgb24 plane to rgb15 pack for pixel 0-7                           \n\
punpcklbw %%xmm4, %%xmm2        # ________ ________ g7g6g5g4 g3______       \n\
punpcklbw %%xmm1, %%xmm0        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $2,%%xmm2             # ________ ____g7g6 g5g4g3__ ________       \n\
por       %%xmm2, %%xmm0        # r7r6r5r4 r3__g7g6 g5g4g3b7 b6b5b4b3       \n\
movntdq   %%xmm0, (%3)          # store pixel 0-7                           \n\
                                                                            \n\
# convert rgb24 plane to rgb15 pack for pixel 8-15                          \n\
punpckhbw %%xmm4, %%xmm7        # ________ ________ g7g6g5g4 g3______       \n\
punpckhbw %%xmm1, %%xmm5        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $2,%%xmm7             # ________ ____g7g6 g5g4g3__ ________       \n\
por       %%xmm7, %%xmm5        # r7r6r5r4 r3__g7g6 g5g4g3b7 b6b5b4b3       \n\
movntdq   %%xmm5, 16(%3)        # store pixel 4-7                           \n\
"

#define SSE2_UNPACK_15_UNALIGNED "                                          \n\
# mask unneeded bits off                                                    \n\
movl      $0xf8f8f8f8, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8f8 f8f8 ... f8f8 f8f8   \n\
pand      %%xmm5, %%xmm0        # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
psrlw     $3,%%xmm0             # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pand      %%xmm5, %%xmm2        # g7g6g5g4 g3______ g7g6g5g4 g3______       \n\
pand      %%xmm5, %%xmm1        # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
psrlw     $1,%%xmm1             # __r7r6r5 r4r3____ __r7r6r5 r4r3____       \n\
pxor      %%xmm4, %%xmm4        # zero mm4                                  \n\
movdqa    %%xmm0, %%xmm5        # Copy B15-B0                               \n\
movdqa    %%xmm2, %%xmm7        # Copy G15-G0                               \n\
                                                                            \n\
# convert rgb24 plane to rgb15 pack for pixel 0-7                           \n\
punpcklbw %%xmm4, %%xmm2        # ________ ________ g7g6g5g4 g3______       \n\
punpcklbw %%xmm1, %%xmm0        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $2,%%xmm2             # ________ ____g7g6 g5g4g3__ ________       \n\
por       %%xmm2, %%xmm0        # r7r6r5r4 r3__g7g6 g5g4g3b7 b6b5b4b3       \n\
movdqu    %%xmm0, (%3)          # store pixel 0-7                           \n\
                                                                            \n\
# convert rgb24 plane to rgb15 pack for pixel 8-15                          \n\
punpckhbw %%xmm4, %%xmm7        # ________ ________ g7g6g5g4 g3______       \n\
punpckhbw %%xmm1, %%xmm5        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $2,%%xmm7             # ________ ____g7g6 g5g4g3__ ________       \n\
por       %%xmm7, %%xmm5        # r7r6r5r4 r3__g7g6 g5g4g3b7 b6b5b4b3       \n\
movdqu    %%xmm5, 16(%3)        # store pixel 4-7                           \n\
"

#define SSE2_UNPACK_16_ALIGNED "                                            \n\
# mask unneeded bits off                                                    \n\
movl      $0xf8f8f8f8, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8f8 f8f8 ... f8f8 f8f8   \n\
pand      %%xmm5, %%xmm0        # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
pand      %%xmm5, %%xmm1        # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
movl      $0xfcfcfcfc, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8f8 f8f8 ... f8f8 f8f8   \n\
pand      %%xmm5, %%xmm2        # g7g6g5g4 g3g2____ g7g6g5g4 g3g2____       \n\
psrlw     $3,%%xmm0             # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pxor      %%xmm4, %%xmm4        # zero mm4                                  \n\
movdqa    %%xmm0, %%xmm5        # Copy B15-B0                               \n\
movdqa    %%xmm2, %%xmm7        # Copy G15-G0                               \n\
                                                                            \n\
# convert rgb24 plane to rgb16 pack for pixel 0-7                           \n\
punpcklbw %%xmm4, %%xmm2        # ________ ________ g7g6g5g4 g3g2____       \n\
punpcklbw %%xmm1, %%xmm0        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%xmm2             # ________ __g7g6g5 g4g3g2__ ________       \n\
por       %%xmm2, %%xmm0        # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movntdq   %%xmm0, (%3)          # store pixel 0-7                           \n\
                                                                            \n\
# convert rgb24 plane to rgb16 pack for pixel 8-15                          \n\
punpckhbw %%xmm4, %%xmm7        # ________ ________ g7g6g5g4 g3g2____       \n\
punpckhbw %%xmm1, %%xmm5        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%xmm7             # ________ __g7g6g5 g4g3g2__ ________       \n\
por       %%xmm7, %%xmm5        # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movntdq   %%xmm5, 16(%3)        # store pixel 4-7                           \n\
"

#define SSE2_UNPACK_16_UNALIGNED "                                          \n\
# mask unneeded bits off                                                    \n\
movl      $0xf8f8f8f8, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8f8 f8f8 ... f8f8 f8f8   \n\
pand      %%xmm5, %%xmm0        # b7b6b5b4 b3______ b7b6b5b4 b3______       \n\
pand      %%xmm5, %%xmm1        # r7r6r5r4 r3______ r7r6r5r4 r3______       \n\
movl      $0xfcfcfcfc, %%eax    #                                           \n\
movd      %%eax, %%xmm5         #                                           \n\
pshufd    $0, %%xmm5, %%xmm5    # set xmm5 to     f8f8 f8f8 ... f8f8 f8f8   \n\
pand      %%xmm5, %%xmm2        # g7g6g5g4 g3g2____ g7g6g5g4 g3g2____       \n\
psrlw     $3,%%xmm0             # ______b7 b6b5b4b3 ______b7 b6b5b4b3       \n\
pxor      %%xmm4, %%xmm4        # zero mm4                                  \n\
movdqa    %%xmm0, %%xmm5        # Copy B15-B0                               \n\
movdqa    %%xmm2, %%xmm7        # Copy G15-G0                               \n\
                                                                            \n\
# convert rgb24 plane to rgb16 pack for pixel 0-7                           \n\
punpcklbw %%xmm4, %%xmm2        # ________ ________ g7g6g5g4 g3g2____       \n\
punpcklbw %%xmm1, %%xmm0        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%xmm2             # ________ __g7g6g5 g4g3g2__ ________       \n\
por       %%xmm2, %%xmm0        # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movdqu    %%xmm0, (%3)          # store pixel 0-7                           \n\
                                                                            \n\
# convert rgb24 plane to rgb16 pack for pixel 8-15                          \n\
punpckhbw %%xmm4, %%xmm7        # ________ ________ g7g6g5g4 g3g2____       \n\
punpckhbw %%xmm1, %%xmm5        # r7r6r5r4 r3______ ______b7 b6b5b4b3       \n\
psllw     $3,%%xmm7             # ________ __g7g6g5 g4g3g2__ ________       \n\
por       %%xmm7, %%xmm5        # r7r6r5r4 r3g7g6g5 g4g3g2b7 b6b5b4b3       \n\
movdqu    %%xmm5, 16(%3)        # store pixel 4-7                           \n\
"

#define SSE2_UNPACK_32_ARGB_ALIGNED "                                       \n\
pxor      %%xmm3, %%xmm3  # zero xmm3                                       \n\
movdqa    %%xmm0, %%xmm4  #               B7 B6 B5 B4 B3 B2 B1 B0           \n\
punpcklbw %%xmm2, %%xmm4  #               G3 B3 G2 B2 G1 B1 G0 B0           \n\
movdqa    %%xmm1, %%xmm5  #               R7 R6 R5 R4 R3 R2 R1 R0           \n\
punpcklbw %%xmm3, %%xmm5  #               00 R3 00 R2 00 R1 00 R0           \n\
movdqa    %%xmm4, %%xmm6  #               G3 B3 G2 B2 G1 B1 G0 B0           \n\
punpcklwd %%xmm5, %%xmm4  #               00 R1 B1 G1 00 R0 B0 G0           \n\
movntdq   %%xmm4, (%3)    # Store ARGB3 ARGB2 ARGB1 ARGB0                   \n\
punpckhwd %%xmm5, %%xmm6  #               00 R3 B3 G3 00 R2 B2 G2           \n\
movntdq   %%xmm6, 16(%3)  # Store ARGB7 ARGB6 ARGB5 ARGB4                   \n\
punpckhbw %%xmm2, %%xmm0  #               G7 B7 G6 B6 G5 B5 G4 B4           \n\
punpckhbw %%xmm3, %%xmm1  #               00 R7 00 R6 00 R5 00 R4           \n\
movdqa    %%xmm0, %%xmm5  #               G7 B7 G6 B6 G5 B5 G4 B4           \n\
punpcklwd %%xmm1, %%xmm5  #               00 R5 B5 G5 00 R4 B4 G4           \n\
movntdq   %%xmm5, 32(%3)  # Store ARGB11 ARGB10 ARGB9 ARGB8                 \n\
punpckhwd %%xmm1, %%xmm0  #               00 R7 B7 G7 00 R6 B6 G6           \n\
movntdq   %%xmm0, 48(%3)  # Store ARGB15 ARGB14 ARGB13 ARGB12               \n\
"

#define SSE2_UNPACK_32_ARGB_UNALIGNED "                                     \n\
pxor      %%xmm3, %%xmm3  # zero xmm3                                       \n\
movdqa    %%xmm0, %%xmm4  #               B7 B6 B5 B4 B3 B2 B1 B0           \n\
punpcklbw %%xmm2, %%xmm4  #               G3 B3 G2 B2 G1 B1 G0 B0           \n\
movdqa    %%xmm1, %%xmm5  #               R7 R6 R5 R4 R3 R2 R1 R0           \n\
punpcklbw %%xmm3, %%xmm5  #               00 R3 00 R2 00 R1 00 R0           \n\
movdqa    %%xmm4, %%xmm6  #               G3 B3 G2 B2 G1 B1 G0 B0           \n\
punpcklwd %%xmm5, %%xmm4  #               00 R1 B1 G1 00 R0 B0 G0           \n\
movdqu    %%xmm4, (%3)    # Store ARGB3 ARGB2 ARGB1 ARGB0                   \n\
punpckhwd %%xmm5, %%xmm6  #               00 R3 B3 G3 00 R2 B2 G2           \n\
movdqu    %%xmm6, 16(%3)  # Store ARGB7 ARGB6 ARGB5 ARGB4                   \n\
punpckhbw %%xmm2, %%xmm0  #               G7 B7 G6 B6 G5 B5 G4 B4           \n\
punpckhbw %%xmm3, %%xmm1  #               00 R7 00 R6 00 R5 00 R4           \n\
movdqa    %%xmm0, %%xmm5  #               G7 B7 G6 B6 G5 B5 G4 B4           \n\
punpcklwd %%xmm1, %%xmm5  #               00 R5 B5 G5 00 R4 B4 G4           \n\
movdqu    %%xmm5, 32(%3)  # Store ARGB11 ARGB10 ARGB9 ARGB8                 \n\
punpckhwd %%xmm1, %%xmm0  #               00 R7 B7 G7 00 R6 B6 G6           \n\
movdqu    %%xmm0, 48(%3)  # Store ARGB15 ARGB14 ARGB13 ARGB12               \n\
"

#define SSE2_UNPACK_32_RGBA_ALIGNED "                                       \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                        \n\
movdqa    %%xmm2, %%xmm4  #                 G7 G6 G5 G4 G3 G2 G1 G0         \n\
punpcklbw %%xmm1, %%xmm4  #                 R3 G3 R2 G2 R1 G1 R0 G0         \n\
punpcklbw %%xmm0, %%xmm3  #                 B3 00 B2 00 B1 00 B0 00         \n\
movdqa    %%xmm3, %%xmm5  #                 R3 00 R2 00 R1 00 R0 00         \n\
punpcklwd %%xmm4, %%xmm3  #                 R1 G1 B1 00 R0 B0 G0 00         \n\
movntdq   %%xmm3, (%3)    # Store RGBA3 RGBA2 RGBA1 RGBA0                   \n\
punpckhwd %%xmm4, %%xmm5  #                 R3 G3 B3 00 R2 G2 B2 00         \n\
movntdq   %%xmm5, 16(%3)  # Store RGBA7 RGBA6 RGBA5 RGBA4                   \n\
pxor      %%xmm6, %%xmm6  # zero mm6                                        \n\
punpckhbw %%xmm1, %%xmm2  #                 R7 G7 R6 G6 R5 G5 R4 G4         \n\
punpckhbw %%xmm0, %%xmm6  #                 B7 00 B6 00 B5 00 B4 00         \n\
movdqa    %%xmm6, %%xmm0  #                 B7 00 B6 00 B5 00 B4 00         \n\
punpcklwd %%xmm2, %%xmm6  #                 R5 G5 B5 00 R4 G4 B4 00         \n\
movntdq   %%xmm6, 32(%3)  # Store BGRA11 BGRA10 BGRA9 RGBA8                 \n\
punpckhwd %%xmm2, %%xmm0  #                 R7 G7 B7 00 R6 G6 B6 00         \n\
movntdq   %%xmm0, 48(%3)  # Store RGBA15 RGBA14 RGBA13 RGBA12               \n\
"

#define SSE2_UNPACK_32_RGBA_UNALIGNED "                                     \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                        \n\
movdqa    %%xmm2, %%xmm4  #                 G7 G6 G5 G4 G3 G2 G1 G0         \n\
punpcklbw %%xmm1, %%xmm4  #                 R3 G3 R2 G2 R1 G1 R0 G0         \n\
punpcklbw %%xmm0, %%xmm3  #                 B3 00 B2 00 B1 00 B0 00         \n\
movdqa    %%xmm3, %%xmm5  #                 R3 00 R2 00 R1 00 R0 00         \n\
punpcklwd %%xmm4, %%xmm3  #                 R1 G1 B1 00 R0 B0 G0 00         \n\
movdqu    %%xmm3, (%3)    # Store RGBA3 RGBA2 RGBA1 RGBA0                   \n\
punpckhwd %%xmm4, %%xmm5  #                 R3 G3 B3 00 R2 G2 B2 00         \n\
movdqu    %%xmm5, 16(%3)  # Store RGBA7 RGBA6 RGBA5 RGBA4                   \n\
pxor      %%xmm6, %%xmm6  # zero mm6                                        \n\
punpckhbw %%xmm1, %%xmm2  #                 R7 G7 R6 G6 R5 G5 R4 G4         \n\
punpckhbw %%xmm0, %%xmm6  #                 B7 00 B6 00 B5 00 B4 00         \n\
movdqa    %%xmm6, %%xmm0  #                 B7 00 B6 00 B5 00 B4 00         \n\
punpcklwd %%xmm2, %%xmm6  #                 R5 G5 B5 00 R4 G4 B4 00         \n\
movdqu    %%xmm6, 32(%3)  # Store RGBA11 RGBA10 RGBA9 RGBA8                 \n\
punpckhwd %%xmm2, %%xmm0  #                 R7 G7 B7 00 R6 G6 B6 00         \n\
movdqu    %%xmm0, 48(%3)  # Store RGBA15 RGBA14 RGBA13 RGBA12               \n\
"

#define SSE2_UNPACK_32_BGRA_ALIGNED "                                       \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                        \n\
movdqa    %%xmm2, %%xmm4  #                 G7 G6 G5 G4 G3 G2 G1 G0         \n\
punpcklbw %%xmm0, %%xmm4  #                 B3 G3 B2 G2 B1 G1 B0 G0         \n\
punpcklbw %%xmm1, %%xmm3  #                 R3 00 R2 00 R1 00 R0 00         \n\
movdqa    %%xmm3, %%xmm5  #                 R3 00 R2 00 R1 00 R0 00         \n\
punpcklwd %%xmm4, %%xmm3  #                 B1 G1 R1 00 B0 G0 R0 00         \n\
movntdq   %%xmm3, (%3)    # Store BGRA3 BGRA2 BGRA1 BGRA0                   \n\
punpckhwd %%xmm4, %%xmm5  #                 B3 G3 R3 00 B2 G2 R2 00         \n\
movntdq   %%xmm5, 16(%3)  # Store BGRA7 BGRA6 BGRA5 BGRA4                   \n\
pxor      %%xmm6, %%xmm6  # zero mm6                                        \n\
punpckhbw %%xmm0, %%xmm2  #                 B7 G7 B6 G6 B5 G5 B4 G4         \n\
punpckhbw %%xmm1, %%xmm6  #                 R7 00 R6 00 R5 00 R4 00         \n\
movdqa    %%xmm6, %%xmm0  #                 R7 00 R6 00 R5 00 R4 00         \n\
punpcklwd %%xmm2, %%xmm6  #                 B5 G5 R5 00 B4 G4 R4 00         \n\
movntdq   %%xmm6, 32(%3)  # Store BGRA11 BGRA10 BGRA9 BGRA8                 \n\
punpckhwd %%xmm2, %%xmm0  #                 B7 G7 R7 00 B6 G6 R6 00         \n\
movntdq   %%xmm0, 48(%3)  # Store BGRA15 BGRA14 BGRA13 BGRA12               \n\
"

#define SSE2_UNPACK_32_BGRA_UNALIGNED "                                     \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                        \n\
movdqa    %%xmm2, %%xmm4  #                 G7 G6 G5 G4 G3 G2 G1 G0         \n\
punpcklbw %%xmm0, %%xmm4  #                 B3 G3 B2 G2 B1 G1 B0 G0         \n\
punpcklbw %%xmm1, %%xmm3  #                 R3 00 R2 00 R1 00 R0 00         \n\
movdqa    %%xmm3, %%xmm5  #                 R3 00 R2 00 R1 00 R0 00         \n\
punpcklwd %%xmm4, %%xmm3  #                 B1 G1 R1 00 B0 G0 R0 00         \n\
movdqu    %%xmm3, (%3)    # Store BGRA3 BGRA2 BGRA1 BGRA0                   \n\
punpckhwd %%xmm4, %%xmm5  #                 B3 G3 R3 00 B2 G2 R2 00         \n\
movdqu    %%xmm5, 16(%3)  # Store BGRA7 BGRA6 BGRA5 BGRA4                   \n\
pxor      %%xmm6, %%xmm6  # zero mm6                                        \n\
punpckhbw %%xmm0, %%xmm2  #                 B7 G7 B6 G6 B5 G5 B4 G4         \n\
punpckhbw %%xmm1, %%xmm6  #                 R7 00 R6 00 R5 00 R4 00         \n\
movdqa    %%xmm6, %%xmm0  #                 R7 00 R6 00 R5 00 R4 00         \n\
punpcklwd %%xmm2, %%xmm6  #                 B5 G5 R5 00 B4 G4 R4 00         \n\
movdqu    %%xmm6, 32(%3)  # Store BGRA11 BGRA10 BGRA9 BGRA8                 \n\
punpckhwd %%xmm2, %%xmm0  #                 B7 G7 R7 00 B6 G6 R6 00         \n\
movdqu    %%xmm0, 48(%3)  # Store BGRA15 BGRA14 BGRA13 BGRA12               \n\
"

#define SSE2_UNPACK_32_ABGR_ALIGNED "                                       \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                        \n\
movdqa    %%xmm1, %%xmm4  #                 R7 R6 R5 R4 R3 R2 R1 R0         \n\
punpcklbw %%xmm2, %%xmm4  #                 G3 R3 G2 R2 G1 R1 G0 R0         \n\
movdqa    %%xmm0, %%xmm5  #                 B7 B6 B5 B4 B3 B2 B1 B0         \n\
punpcklbw %%xmm3, %%xmm5  #                 00 B3 00 B2 00 B1 00 B0         \n\
movdqa    %%xmm4, %%xmm6  #                 G3 R3 G2 R2 G1 R1 G0 R0         \n\
punpcklwd %%xmm5, %%xmm4  #                 00 B1 G1 R1 00 B0 G0 R0         \n\
movntdq   %%xmm4, (%3)    # Store ABGR3 ABGR2 ABGR1 ABGR0                   \n\
punpckhwd %%xmm5, %%xmm6  #                 00 B3 G3 R3 00 B2 G2 R2         \n\
movntdq   %%xmm6, 16(%3)  # Store ABGR7 ABGR6 ABGR5 ABGR4                   \n\
punpckhbw %%xmm2, %%xmm1  #                 G7 R7 G6 R6 G5 R5 G4 R4         \n\
punpckhbw %%xmm3, %%xmm0  #                 00 B7 00 B6 00 B5 00 B4         \n\
movdqa    %%xmm1, %%xmm2  #                 G7 R7 G6 R6 G5 R5 G4 R4         \n\
punpcklwd %%xmm0, %%xmm1  #                 00 B5 G5 R5 00 B4 G4 R4         \n\
movntdq   %%xmm1, 32(%3)  # Store ABGR11 ABGR10 ABGR9 ABGR8                 \n\
punpckhwd %%xmm0, %%xmm2  #                 B7 G7 R7 00 B6 G6 R6 00         \n\
movntdq   %%xmm2, 48(%3)  # Store ABGR15 ABGR14 ABGR13 ABGR12               \n\
"

#define SSE2_UNPACK_32_ABGR_UNALIGNED "                                     \n\
pxor      %%xmm3, %%xmm3  # zero mm3                                        \n\
movdqa    %%xmm1, %%xmm4  #                 R7 R6 R5 R4 R3 R2 R1 R0         \n\
punpcklbw %%xmm2, %%xmm4  #                 G3 R3 G2 R2 G1 R1 G0 R0         \n\
movdqa    %%xmm0, %%xmm5  #                 B7 B6 B5 B4 B3 B2 B1 B0         \n\
punpcklbw %%xmm3, %%xmm5  #                 00 B3 00 B2 00 B1 00 B0         \n\
movdqa    %%xmm4, %%xmm6  #                 G3 R3 G2 R2 G1 R1 G0 R0         \n\
punpcklwd %%xmm5, %%xmm4  #                 00 B1 G1 R1 00 B0 G0 R0         \n\
movdqu    %%xmm4, (%3)    # Store ABGR3 ABGR2 ABGR1 ABGR0                   \n\
punpckhwd %%xmm5, %%xmm6  #                 00 B3 G3 R3 00 B2 G2 R2         \n\
movdqu    %%xmm6, 16(%3)  # Store ABGR7 ABGR6 ABGR5 ABGR4                   \n\
punpckhbw %%xmm2, %%xmm1  #                 G7 R7 G6 R6 G5 R5 G4 R4         \n\
punpckhbw %%xmm3, %%xmm0  #                 00 B7 00 B6 00 B5 00 B4         \n\
movdqa    %%xmm1, %%xmm2  #                 R7 00 R6 00 R5 00 R4 00         \n\
punpcklwd %%xmm0, %%xmm1  #                 00 B5 G5 R5 00 B4 G4 R4         \n\
movdqu    %%xmm1, 32(%3)  # Store ABGR11 ABGR10 ABGR9 ABGR8                 \n\
punpckhwd %%xmm0, %%xmm2  #                 B7 G7 R7 00 B6 G6 R6 00         \n\
movdqu    %%xmm2, 48(%3)  # Store ABGR15 ABGR14 ABGR13 ABGR12               \n\
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
    _mm_storeu_si128((__m128i*)(p_buffer+16), xmm5);

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

#endif
