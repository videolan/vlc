/*****************************************************************************
 * video_yuv_mmx.S: YUV transformation, optimized for MMX processors
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * 
 * Authors:
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Following functions are defined:
 * vout_YUV420_16_MMX
 *            This function performs YUV12-to-RGB16 color conversion for H26x.
 *            It handles any format in which there are three fields, the low
 *            order field being B and fully contained in the low order byte, the
 *            second field being G and being somewhere in bits 4 through 11,
 *            and the high order field being R and fully contained in the high
 *            order byte.
 *
 *            The YUV12 input is planar, 8 bits per pel.  The Y plane may have
 *            a pitch of up to 768.  It may have a width less than or equal
 *            to the pitch.  It must be DWORD aligned, and preferably QWORD
 *            aligned.  Pitch and Width must be a multiple of four.  For best
 *            performance, Pitch should not be 4 more than a multiple of 32.
 *            Height may be any amount, but must be a multiple of two.  The U
 *            and V planes may have a different pitch than the Y plane, subject
 *            to the same limitations.
 *****************************************************************************/

//.include iammx.inc
//.include locals.inc

.data
    .align 16

RGB_formats:
    .long  RGB565
    .long  RGB555
    .long  RGB664
    .long  RGB655

Minusg:             .long 0x00800080, 0x00800080
Yadd:               .long 0x10101010, 0x10101010
VtR:                .long 0x00660066, 0x00660066
VtG:                .long 0x00340034, 0x00340034
UtG:                .long 0x00190019, 0x00190019
UtB:                .long 0x00810081, 0x00810081
Ymul:               .long 0x004a004a, 0x004a004a
UVtG:               .long 0x00340019, 0x00340019
VtRUtB:             .long 0x01990205, 0x01990205
fourbitu:           .quad 0xf0f0f0f0f0f0f0f0
fivebitu:           .quad 0xe0e0e0e0e0e0e0e0
sixbitu:            .quad 0xc0c0c0c0c0c0c0c0

.text

#define LocalFrameSize  156
#define RegisterStorageSize  16

//#define DOUBLE /*double le nombre de colonnes */

/* Arguments: */
#define YPlane                    LocalFrameSize + RegisterStorageSize +  4
#define UPlane                    LocalFrameSize + RegisterStorageSize +  8
#define VPlane                    LocalFrameSize + RegisterStorageSize + 12
#define FrameWidth                LocalFrameSize + RegisterStorageSize + 16
#define FrameHeight               LocalFrameSize + RegisterStorageSize + 20
#define YPitch                    LocalFrameSize + RegisterStorageSize + 24
#define ChromaPitch               LocalFrameSize + RegisterStorageSize + 28
#define AspectAdjustmentCount     LocalFrameSize + RegisterStorageSize + 32
#define ColorConvertedFrame       LocalFrameSize + RegisterStorageSize + 36
#define DCIOffset                 LocalFrameSize + RegisterStorageSize + 40
#define CCOffsetToLine0           LocalFrameSize + RegisterStorageSize + 44
#define CCOPitch                  LocalFrameSize + RegisterStorageSize + 48
#define CCType                    LocalFrameSize + RegisterStorageSize + 52
#define EndOfArgList              LocalFrameSize + RegisterStorageSize + 56

/* Locals (on local stack frame) */
#define CCOCursor        0
#define CCOSkipDistance  4
#define ChromaLineLen    8
#define YCursor          12
#define DistanceFromVToU 16
#define EndOfChromaLine  20
#define AspectCount      24
#define AspectBaseCount  28
#define tmpYCursorEven   32
#define tmpYCursorOdd    36
#define tmpCCOPitch      40
#define temp_mmx         44
#define RLeftShift       92
#define GLeftShift       100
#define RRightShift      108
#define GRightShift      116
#define BRightShift      124
#define RUpperLimit      132
#define GUpperLimit      140
#define BUpperLimit      148

/*
 * extern void C ConvertYUV420RGB16MMX (
 *                                     U8* YPlane,
 *                                     U8* UPlane,
 *                                     U8* VPlane,
 *                                     UN  FrameWidth,
 *                                     UN  FrameHeight,
 *                                     UN  YPitch,
 *                                     UN  VPitch,
 *                                     UN  AspectAdjustmentCount,
 *                                     U8* ColorConvertedFrame,
 *                                     U32 DCIOffset,
 *                                     U32 CCOffsetToLine0,
 *                                     IN  CCOPitch,
 *                                     IN  CCType)
 *
 *  The local variables are on the stack,
 *  The tables are in the one and only data segment.
 *
 *  CCOffsetToLine0 is relative to ColorConvertedFrame.
 *  CCType  used by RGB color convertors to determine the exact conversion type.
 *    RGB565 = 0
 *    RGB555 = 1
 *    RGB664 = 2
 *    RGB655 = 3
 */

.globl ConvertYUV420RGB16MMX
ConvertYUV420RGB16MMX:
  pushl      %esi
  pushl      %edi

  pushl      %ebp
  pushl      %ebx

  subl       $LocalFrameSize,%esp
  movl       CCType(%esp),%eax
  cmpl       $4,%eax
  jae        finish

  jmp        *RGB_formats(,%eax,4)

RGB555:
  xorl       %eax,%eax
  movl       $2,%ebx                 /* 10-8 for byte shift */
  movl       %ebx,RLeftShift(%esp)
  movl       %eax,RLeftShift+4(%esp)
  movl       $5,%ebx
  movl       %ebx,GLeftShift(%esp)
  movl       %eax,GLeftShift+4(%esp)
  movl       $9,%ebx
  movl       %ebx,RRightShift(%esp)
  movl       %eax,RRightShift+4(%esp)
  movl       %ebx,GRightShift(%esp)
  movl       %eax,GRightShift+4(%esp)
  movl       %ebx,BRightShift(%esp)
  movl       %eax,BRightShift+4(%esp)
  movq       fivebitu,%mm0
  movq       %mm0,RUpperLimit(%esp)
  movq       %mm0,GUpperLimit(%esp)
  movq       %mm0,BUpperLimit(%esp)
  jmp        RGBEND

RGB664:
  xorl       %eax,%eax
  movl       $2,%ebx                 /* 8-6 */
  movl       %ebx,RLeftShift(%esp)
  movl       %eax,RLeftShift+4(%esp)
  movl       $4,%ebx
  movl       %ebx,GLeftShift(%esp)
  movl       %eax,GLeftShift+4(%esp)
  movl       $8,%ebx
  movl       %ebx,RRightShift(%esp)
  movl       %eax,RRightShift+4(%esp)
  movl       %ebx,GRightShift(%esp)
  movl       %eax,GRightShift+4(%esp)
  movl       $10,%ebx
  movl       %ebx,BRightShift(%esp)
  movl       %eax,BRightShift+4(%esp)
  movq       sixbitu,%mm0
  movq       %mm0,RUpperLimit(%esp)
  movq       %mm0,GUpperLimit(%esp)
  movq       fourbitu,%mm0
  movq       %mm0,BUpperLimit(%esp)
  jmp        RGBEND

RGB655:
  xorl       %eax,%eax
  movl       $2,%ebx                 /* 8-6 */
  movl       %ebx,RLeftShift(%esp)
  movl       %eax,RLeftShift+4(%esp)
  movl       $5,%ebx
  movl       %ebx,GLeftShift(%esp)
  movl       %eax,GLeftShift+4(%esp)
  movl       $8,%ebx
  movl       %ebx,RRightShift(%esp)
  movl       %eax,RRightShift+4(%esp)
  movl       $9,%ebx
  movl       %ebx,GRightShift(%esp)
  movl       %eax,GRightShift+4(%esp)
  movl       %ebx,BRightShift(%esp)
  movl       %eax,BRightShift+4(%esp)
  movq       sixbitu,%mm0
  movq       %mm0,RUpperLimit(%esp)
  movq       fivebitu,%mm0
  movq       %mm0,GUpperLimit(%esp)
  movq       %mm0,BUpperLimit(%esp)
  jmp        RGBEND

RGB565:
  xorl       %eax,%eax
  movl       $3,%ebx                 /* 8-5 */
  movl       %ebx,RLeftShift(%esp)
  movl       %eax,RLeftShift+4(%esp)
  movl       $5,%ebx
  movl       %ebx,GLeftShift(%esp)
  movl       %eax,GLeftShift+4(%esp)
  movl       $9,%ebx
  movl       %ebx,RRightShift(%esp)
  movl       %eax,RRightShift+4(%esp)
  movl       %ebx,BRightShift(%esp)
  movl       %eax,BRightShift+4(%esp)
  movl       $8,%ebx
  movl       %ebx,GRightShift(%esp)
  movl       %eax,GRightShift+4(%esp)
  movq       fivebitu,%mm0
  movq       %mm0,RUpperLimit(%esp)
  movq       %mm0,BUpperLimit(%esp)
  movq       sixbitu,%mm0
  movq       %mm0,GUpperLimit(%esp)
//  jmp        RGBEND

RGBEND:
  movl       VPlane(%esp),%ebx
  movl       UPlane(%esp),%ecx
  subl       %ebx,%ecx
  movl       %ecx,DistanceFromVToU(%esp)

  movl       ColorConvertedFrame(%esp),%eax
  addl       DCIOffset(%esp),%eax
  addl       CCOffsetToLine0(%esp),%eax
  movl       %eax,CCOCursor(%esp)


  movl       YPitch(%esp),%ecx
  movl       FrameWidth(%esp),%ebx
  movl       CCOPitch(%esp),%eax
  subl       %ebx,%eax                   /* CCOPitch-FrameWidth */
  subl       %ebx,%eax                   /* CCOPitch-2*FrameWidth */
  sarl       %ebx                        /* FrameWidth/2 */
  movl       YPlane(%esp),%esi           /* Fetch cursor over luma plane. */
  movl       %ebx,ChromaLineLen(%esp)    /* FrameWidth/2 */
  movl       %eax,CCOSkipDistance(%esp)  /* CCOPitch-3*FrameWidth */
  movl       %esi,YCursor(%esp)
  movl       AspectAdjustmentCount(%esp),%edx
  movl       VPlane(%esp),%esi

  cmpl       $1,%edx
  je         finish
  movl       %edx,AspectCount(%esp)
  movl       %edx,AspectBaseCount(%esp)
  xorl       %eax,%eax

  movl       ChromaLineLen(%esp),%edi
  movl       %edi,EndOfChromaLine(%esp)
  movl       CCOCursor(%esp),%edi

  movl       DistanceFromVToU(%esp),%edx
  movl       YCursor(%esp),%ebp         /* Fetch Y Pitch. */
  movl       FrameWidth(%esp),%ebx

  addl       %ebx,%ebp
  movl       %ebp,tmpYCursorEven(%esp)
  movl       YPitch(%esp),%eax
  addl       %eax,%ebp
  movl       %ebp,tmpYCursorOdd(%esp)

  sarl       %ebx
  addl       %ebx,%esi
  addl       %esi,%edx
  negl       %ebx
  movl       %ebx,FrameWidth(%esp)

/*
 *  Register Usage:
 */

PrepareChromaLine:
  movl       AspectCount(%esp),%ebp
  movl       FrameWidth(%esp),%ebx
  subl       $2,%ebp
  movl       CCOPitch(%esp),%eax
  movl       %eax,tmpCCOPitch(%esp)
  ja         continue

  xorl       %eax,%eax
  addl       AspectAdjustmentCount(%esp),%ebp
  movl       %eax,tmpCCOPitch(%esp)
continue:
  movl       %ebp,AspectCount(%esp)

do_next_8x2_block:
  movl       tmpYCursorEven(%esp),%ebp
/* here is even line */
  movd       (%edx,%ebx,),%mm1       /* 4 u values */
  pxor       %mm0,%mm0               /* mm0=0 */
  movd       (%esi,%ebx,),%mm2       /* 4 v values */
  punpcklbw  %mm0,%mm1               /* get 4 unsign u */
  psubw      Minusg,%mm1             /* get 4 unsign u-128 */
  punpcklbw  %mm0,%mm2               /* get unsign v */
  psubw      Minusg,%mm2             /* get unsign v-128 */
  movq       %mm1,%mm3               /* save the u-128 unsign */
  movq       %mm1,%mm5               /* save u-128 unsign */
  punpcklwd  %mm2,%mm1               /* get 2 low u, v unsign pairs */
  pmaddwd    UVtG,%mm1
  punpckhwd  %mm2,%mm3               /* create high 2 unsign uv pairs */
  pmaddwd    UVtG,%mm3
  movq       %mm2,temp_mmx(%esp)       /* save v-128 */
  movq       (%ebp,%ebx,2),%mm6      /* mm6 has 8 y pixels */
  psubusb    Yadd,%mm6               /* mm6 has 8 y-16 pixels */
  packssdw   %mm3,%mm1               /* packed the results to signed words */
  movq       %mm6,%mm7               /* save the 8 y-16 pixels */
  punpcklbw  %mm0,%mm6               /* mm6 has 4 low y-16 unsign */
  pmullw     Ymul,%mm6
  punpckhbw  %mm0,%mm7               /* mm7 has 4 high y-16 unsign */
  pmullw     Ymul,%mm7
  movq       %mm1,%mm4
  movq       %mm1,temp_mmx+8(%esp)     /* save 4 chroma G values */
  punpcklwd  %mm1,%mm1               /* chroma G replicate low 2 */
  movq       %mm6,%mm0               /* low  y */
  punpckhwd  %mm4,%mm4               /* chroma G replicate high 2 */
  movq       %mm7,%mm3               /* high y */
  psubw      %mm1,%mm6               /* 4 low G */
  psraw      GRightShift(%esp),%mm6
  psubw      %mm4,%mm7               /* 4 high G values in signed 16 bit */
  movq       %mm5,%mm2
  punpcklwd  %mm5,%mm5               /* replicate the 2 low u pixels */
  pmullw     UtB,%mm5
  punpckhwd  %mm2,%mm2
  psraw      GRightShift(%esp),%mm7
  pmullw     UtB,%mm2
  packuswb   %mm7,%mm6               /* mm6: G7 G6 G5 G4 G3 G2 G1 G0 */
  movq       %mm5,temp_mmx+16(%esp)    /* low chroma B */
  paddw      %mm0,%mm5               /* 4 low B values in signed 16 bit */
  movq       %mm2,temp_mmx+40(%esp)    /* high chroma B */
  paddw      %mm3,%mm2               /* 4 high B values in signed 16 bit */
  psraw      BRightShift(%esp),%mm5  /* low B scaled down by 6+(8-5) */
  psraw      BRightShift(%esp),%mm2  /* high B scaled down by 6+(8-5) */
  packuswb   %mm2,%mm5               /* mm5: B7 B6 B5 B4 B3 B2 B1 B0 */

  movq       temp_mmx(%esp),%mm2       /* 4 v values */
  movq       %mm5,%mm1               /* save B */
  movq       %mm2,%mm7
  punpcklwd  %mm2,%mm2               /* replicate the 2 low v pixels */
  pmullw     VtR,%mm2
  punpckhwd  %mm7,%mm7
  pmullw     VtR,%mm7
  paddusb    BUpperLimit(%esp),%mm1  /* mm1: saturate B+0FF-15 */
  movq       %mm2,temp_mmx+24(%esp)    /* low chroma R */
  paddw      %mm0,%mm2               /* 4 low R values in signed 16 bit */
  psraw      RRightShift(%esp),%mm2  /* low R scaled down by 6+(8-5) */
  pxor       %mm4,%mm4               /* mm4=0 for 8-&gt;16 conversion */
  movq       %mm7,temp_mmx+32(%esp)    /* high chroma R */
  paddw      %mm3,%mm7               /* 4 high R values in signed 16 bit */
  psraw      RRightShift(%esp),%mm7  /* high R scaled down by 6+(8-5) */
  psubusb    BUpperLimit(%esp),%mm1
  packuswb   %mm7,%mm2               /* mm2: R7 R6 R5 R4 R3 R2 R1 R0 */
  paddusb    GUpperLimit(%esp),%mm6  /* G fast patch ih */
  psubusb    GUpperLimit(%esp),%mm6  /* fast patch ih */
  paddusb    RUpperLimit(%esp),%mm2  /* R */
  psubusb    RUpperLimit(%esp),%mm2

/*
 * here we are packing from RGB24 to RGB16
 * input:
 *         mm6: G7 G6 G5 G4 G3 G2 G1 G0
 *         mm1: B7 B6 B5 B4 B3 B2 B1 B0
 *         mm2: R7 R6 R5 R4 R3 R2 R1 R0
 * assuming 8 original pixels in 0-H representation on mm6, mm5, mm2
 * when  H=2**xBITS-1 (x is for R G B)
 * output:
 *        mm1- result: 4 low RGB16
 *        mm7- result: 4 high RGB16
 * using: mm0- zero register
 *        mm3- temporary results
 * algorithm:
 *   for (i=0; i&lt;8; i++) {
 *     RGB[i]=256*(R[i]&lt;&lt;(8-5))+(G[i]&lt;&lt;5)+B[i];
 *   }
 */

  psllq      RLeftShift(%esp),%mm2   /* position R in the most significant
                                        part of the byte */
  movq       %mm1,%mm7               /* mm1: Save B */

/*
 * note: no need for shift to place B on the least significant part of the byte
 *   R in left position, B in the right position so they can be combined
 */

  punpcklbw  %mm2,%mm1               /* mm1: 4 low 16 bit RB */
  pxor       %mm0,%mm0               /* mm0: 0 */
  punpckhbw  %mm2,%mm7               /* mm5: 4 high 16 bit RB */
  movq       %mm6,%mm3               /* mm3: G */
  punpcklbw  %mm0,%mm6               /* mm6: low 4 G 16 bit */
  psllw      GLeftShift(%esp),%mm6   /* shift low G 5 positions */
  punpckhbw  %mm0,%mm3               /* mm3: high 4 G 16 bit */
  por        %mm6,%mm1               /* mm1: low RBG16 */
  psllw      GLeftShift(%esp),%mm3   /* shift high G 5 positions */
  por        %mm3,%mm7               /* mm5: high RBG16 */

  movl       tmpYCursorOdd(%esp),%ebp  /* moved to here to save cycles
                                           before odd line */
  movq       %mm1,(%edi)             /* !! aligned */

/*- start odd line */
  movq       (%ebp,%ebx,2),%mm1      /* mm1 has 8 y pixels */
  pxor       %mm2,%mm2
  psubusb    Yadd,%mm1               /* mm1 has 8 pixels y-16 */
  movq       %mm1,%mm5
  punpcklbw  %mm2,%mm1               /* get 4 low y-16 unsign pixels word */
  pmullw     Ymul,%mm1               /* low 4 luminance contribution */
  punpckhbw  %mm2,%mm5               /* 4 high y-16 */
  pmullw     Ymul,%mm5               /* high 4 luminance contribution */
  movq       %mm7,8(%edi)            /* !! aligned */
  movq       %mm1,%mm0
  paddw      temp_mmx+24(%esp),%mm0    /* low 4 R */
  movq       %mm5,%mm6
  psraw      RRightShift(%esp),%mm0  /* low R scaled down by 6+(8-5) */
  paddw      temp_mmx+32(%esp),%mm5    /* high 4 R */
  movq       %mm1,%mm2
  psraw      RRightShift(%esp),%mm5  /* high R scaled down by 6+(8-5) */
  paddw      temp_mmx+16(%esp),%mm2    /* low 4 B */
  packuswb   %mm5,%mm0               /* mm0: R7 R6 R5 R4 R3 R2 R1 R0 */
  psraw      BRightShift(%esp),%mm2  /* low B scaled down by 6+(8-5) */
  movq       %mm6,%mm5
  paddw      temp_mmx+40(%esp),%mm6    /* high 4 B */
  psraw      BRightShift(%esp),%mm6  /* high B scaled down by 6+(8-5) */
  movq       temp_mmx+8(%esp),%mm3     /* chroma G  low 4 */
  packuswb   %mm6,%mm2               /* mm2: B7 B6 B5 B4 B3 B2 B1 B0 */
  movq       %mm3,%mm4
  punpcklwd  %mm3,%mm3               /* replicate low 2 */
  punpckhwd  %mm4,%mm4               /* replicate high 2 */
  psubw      %mm3,%mm1               /* 4 low G */
  psraw      GRightShift(%esp),%mm1  /* low G scaled down by 6+(8-5) */
  psubw      %mm4,%mm5               /* 4 high G values in signed 16 bit */
  psraw      GRightShift(%esp),%mm5  /* high G scaled down by 6+(8-5) */
  paddusb    BUpperLimit(%esp),%mm2  /* mm1: saturate B+0FF-15 */
  packuswb   %mm5,%mm1               /*mm1: G7 G6 G5 G4 G3 G2 G1 G0 */
  psubusb    BUpperLimit(%esp),%mm2
  paddusb    GUpperLimit(%esp),%mm1  /* G */
  psubusb    GUpperLimit(%esp),%mm1
  paddusb    RUpperLimit(%esp),%mm0  /* R */
  movl       tmpCCOPitch(%esp),%eax
  psubusb    RUpperLimit(%esp),%mm0

/*
 * here we are packing from RGB24 to RGB16
 *        mm1: G7 G6 G5 G4 G3 G2 G1 G0
 *        mm2: B7 B6 B5 B4 B3 B2 B1 B0
 *        mm0: R7 R6 R5 R4 R3 R2 R1 R0
 * output:
 *        mm2- result: 4 low RGB16
 *        mm7- result: 4 high RGB16
 * using: mm4- zero register
 *        mm3- temporary results
 */

  psllq      RLeftShift(%esp),%mm0   /* position R in the most significant
                                        part of the byte */
  movq       %mm2,%mm7               /* mm7: Save B */

/*
 * note: no need for shift to place B on the least significant part of the byte
 *   R in left position, B in the right position so they can be combined
 */

  punpcklbw  %mm0,%mm2               /* mm1: 4 low 16 bit RB */
  pxor       %mm4,%mm4               /* mm4: 0 */
  movq       %mm1,%mm3               /* mm3: G */
  punpckhbw  %mm0,%mm7               /* mm7: 4 high 16 bit RB */
  punpcklbw  %mm4,%mm1               /* mm1: low 4 G 16 bit */
  punpckhbw  %mm4,%mm3               /* mm3: high 4 G 16 bit */
  psllw      GLeftShift(%esp),%mm1   /* shift low G 5 positions */
  por        %mm1,%mm2               /* mm2: low RBG16 */
  psllw      GLeftShift(%esp),%mm3   /* shift high G 5 positions */
  por        %mm3,%mm7               /* mm7: high RBG16 */
#ifdef DOUBLE
  movq       %mm2,%mm1
  movq       %mm7,%mm5
  movq       %mm2,%mm0
  movq       %mm7,%mm3
  punpckhwd  %mm2,%mm1
  punpckhwd  %mm7,%mm5
  punpcklwd  %mm2,%mm0
  punpcklwd  %mm7,%mm3
  movq       %mm0,(%edi,%eax,)
  movq       %mm1,8(%edi,%eax,)
  movq       %mm3,16(%edi,%eax,)
  movq       %mm5,24(%edi,%eax,)
  addl       $32,%edi
  addl       $4,%ebx
#endif
#ifndef DOUBLE
  movq       %mm2,(%edi,%eax,)
  movq       %mm7,8(%edi,%eax,)      /* aligned */
  addl       $16,%edi                /* ih take 16 bytes (8 pixels-16 bit) */
  addl       $4,%ebx                 /* ? to take 4 pixels together
                                        instead of 2 */
#endif
  jl         do_next_8x2_block

  addl       CCOSkipDistance(%esp),%edi /* go to begin of next line */
  addl       tmpCCOPitch(%esp),%edi     /* skip odd line (if it is needed) */
// Leax       AspectCount
// Lebp       CCOPitch               ; skip odd line

// sub        eax, 2
// jg         @f

// Addeax     AspectBaseCount
// xor        ebp, ebp

//@@:
//  Seax       AspectCount
//  add        edi, ebp

  movl       YPitch(%esp),%eax
  movl       tmpYCursorOdd(%esp),%ebp
  addl       %eax,%ebp               /* skip one line */
//  lea        ebp, [ebp+2*eax]        /* skip two lines */
  movl       %ebp,tmpYCursorEven(%esp)
//  Sebp       tmpYCursorOdd

  addl       %eax,%ebp               /* skip one line */
  movl       %ebp,tmpYCursorOdd(%esp)
//  Lebp       tmpYCursorEven
//  lea        ebp, [ebp+2*eax]
//  Sebp       tmpYCursorEven


  addl       ChromaPitch(%esp),%esi
  addl       ChromaPitch(%esp),%edx


//  Leax       YLimit                  /* Done with last line? */
//  cmp        ebp, eax
//  jbe        PrepareChromaLine
  subw       $2,FrameHeight(%esp)
  ja         PrepareChromaLine

/******************************************************************************/

finish:
  emms
  addl       $LocalFrameSize,%esp

  popl       %ebx
  popl       %ebp
  popl       %edi
  popl       %esi
  ret

