/*****************************************************************************
 * ac3_srfft_sse.c: accelerated SSE ac3 fft functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ac3_srfft_sse.c,v 1.11 2001/12/30 07:09:55 sam Exp $
 *
 * Authors: Renaud Dartus <reno@videolan.org>
 *          Aaron Holtzman <aholtzma@engr.uvic.ca>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <math.h>
#include <stdio.h>

#include <videolan/vlc.h>

#include "ac3_imdct.h"
#include "ac3_srfft.h"


static float hsqrt2_sse[] ATTR_ALIGN(16) =
    { 0.707106781188, 0.707106781188, -0.707106781188, -0.707106781188 };

static float C_1_sse[] ATTR_ALIGN(16) =
    { -1.0, 1.0, -1.0, 1.0 };

typedef struct {
        int k;
        void * C1;
    } ck_sse_t;


static void fft_4_sse (complex_t *x);
static void fft_8_sse (complex_t *x);
static void fft_asmb_sse (ck_sse_t * ck, int k, complex_t *x, complex_t *wTB,
         const complex_t *d, const complex_t *d_3);

void _M( fft_64p ) ( complex_t *a )
{
    ck_sse_t ck;
    ck.C1 = C_1_sse;

    fft_8_sse(&a[0]); fft_4_sse(&a[8]); fft_4_sse(&a[12]);
    fft_asmb_sse(&ck, 2, &a[0], &a[8], &delta16[0], &delta16_3[0]);
  
    fft_8_sse(&a[16]), fft_8_sse(&a[24]);
    fft_asmb_sse(&ck, 4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

    fft_8_sse(&a[32]); fft_4_sse(&a[40]); fft_4_sse(&a[44]);
    fft_asmb_sse(&ck, 2, &a[32], &a[40], &delta16[0], &delta16_3[0]);

    fft_8_sse(&a[48]); fft_4_sse(&a[56]); fft_4_sse(&a[60]);
    fft_asmb_sse(&ck, 2, &a[48], &a[56], &delta16[0], &delta16_3[0]);

    fft_asmb_sse(&ck, 8, &a[0], &a[32],&delta64[0], &delta64_3[0]);
}

void _M( fft_128p ) ( complex_t *a )
{
    ck_sse_t ck;
    ck.C1 = C_1_sse;
            
    fft_8_sse(&a[0]); fft_4_sse(&a[8]); fft_4_sse(&a[12]);
    fft_asmb_sse(&ck, 2, &a[0], &a[8], &delta16[0], &delta16_3[0]);
  
    fft_8_sse(&a[16]), fft_8_sse(&a[24]);
    fft_asmb_sse(&ck, 4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

    fft_8_sse(&a[32]); fft_4_sse(&a[40]); fft_4_sse(&a[44]);
    fft_asmb_sse(&ck, 2, &a[32], &a[40], &delta16[0], &delta16_3[0]);

    fft_8_sse(&a[48]); fft_4_sse(&a[56]); fft_4_sse(&a[60]);
    fft_asmb_sse(&ck, 2, &a[48], &a[56], &delta16[0], &delta16_3[0]);

    fft_asmb_sse(&ck, 8, &a[0], &a[32],&delta64[0], &delta64_3[0]);

    fft_8_sse(&a[64]); fft_4_sse(&a[72]); fft_4_sse(&a[76]);
    /* fft_16(&a[64]); */
    fft_asmb_sse(&ck, 2, &a[64], &a[72], &delta16[0], &delta16_3[0]);

    fft_8_sse(&a[80]); fft_8_sse(&a[88]);
  
    /* fft_32(&a[64]); */
    fft_asmb_sse(&ck, 4, &a[64], &a[80],&delta32[0], &delta32_3[0]);

    fft_8_sse(&a[96]); fft_4_sse(&a[104]), fft_4_sse(&a[108]);
    /* fft_16(&a[96]); */
    fft_asmb_sse(&ck, 2, &a[96], &a[104], &delta16[0], &delta16_3[0]);

    fft_8_sse(&a[112]), fft_8_sse(&a[120]);
    /* fft_32(&a[96]); */
    fft_asmb_sse(&ck, 4, &a[96], &a[112], &delta32[0], &delta32_3[0]);
  
    /* fft_128(&a[0]); */
    fft_asmb_sse(&ck, 16, &a[0], &a[64], &delta128[0], &delta128_3[0]);
}

static void fft_4_sse (complex_t *x)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "movaps   (%%eax), %%xmm0\n"    /* x[1] | x[0] */
    "movaps 16(%%eax), %%xmm2\n"    /* x[3] | x[2] */
    "movaps  %%xmm0, %%xmm1\n"      /* x[1] | x[0] */
    "addps   %%xmm2, %%xmm0\n"      /* x[1] + x[3] | x[0] + x[2] */
    "subps   %%xmm2, %%xmm1\n"      /* x[1] - x[3] | x[0] - x[2] */
    "xorps   %%xmm6, %%xmm6\n"
    "movhlps %%xmm1, %%xmm4\n"      /* ? | x[1] - x[3] */
    "movhlps %%xmm0, %%xmm3\n"      /* ? | x[1] + x[3] */
    "subss   %%xmm4, %%xmm6\n"      /* 0 | -(x[1] - x[3]).re */
    "movlhps %%xmm1, %%xmm0\n"      /* x[0] - x[2] | x[0] + x[2] */
    "movlhps %%xmm6, %%xmm4\n"      /* 0 | -(x[1] - x[3]).re | (x[1] - x[3]).im | (x[3]-x[1]).re */
    "movaps  %%xmm0, %%xmm2\n"      /* x[0] - x[2] | x[0] + x[2] */
    "shufps   $0x94, %%xmm4, %%xmm3\n" /* i*(x[1] - x[3]) | x[1] + x[3] */
    "addps   %%xmm3, %%xmm0\n"
    "subps   %%xmm3, %%xmm2\n"
    "movaps  %%xmm0,   (%%eax)\n"
    "movaps  %%xmm2, 16(%%eax)\n"
    : "=a" (x)
    : "a" (x) );
}

static void fft_8_sse (complex_t *x)
{
    __asm__ __volatile__ (
    ".align 16\n"
    
    "movlps   (%%eax), %%xmm0\n"    /* x[0] */
    "movlps 32(%%eax), %%xmm1\n"    /* x[4] */
    "movhps 16(%%eax), %%xmm0\n"    /* x[2] | x[0] */
    "movhps 48(%%eax), %%xmm1\n"    /* x[6] | x[4] */
    "movaps  %%xmm0, %%xmm2\n"      /* x[2] | x[0] */
    "xorps   %%xmm3, %%xmm3\n"
    "addps   %%xmm1, %%xmm0\n"      /* x[2] + x[6] | x[0] + x[4] */
    "subps   %%xmm1, %%xmm2\n"      /* x[2] - x[6] | x[0] - x[4] */
    "movhlps %%xmm0, %%xmm5\n"      /* x[2] + x[6] */
    "movhlps %%xmm2, %%xmm4\n"      /* x[2] - x[6] */
    "movlhps %%xmm2, %%xmm0\n"      /* x[0] - x[4] | x[0] + x[4] */
    "subss   %%xmm4, %%xmm3\n"      /* (x[2]-x[6]).im | -(x[2]-x[6]).re */
    "movaps  %%xmm0, %%xmm7\n"      /* x[0] - x[4] | x[0] + x[4] */
    "movaps  %%xmm3, %%xmm4\n"      /* (x[2]-x[6]).im | -(x[2]-x[6]).re */
    "movlps 8(%%eax), %%xmm1\n"     /* x[1] */
    "shufps   $0x14, %%xmm4, %%xmm5\n" /* i*(x[2] - x[6]) | x[2] + x[6] */

    "addps   %%xmm5, %%xmm0\n"      /* yt = i*(x2-x6)+x0-x4 | x2+x6+x0+x4 */
    "subps   %%xmm5, %%xmm7\n"      /* yb = i*(x6-x2)+x0-x4 | -x6-x2+x0+x4 */

    "movhps 24(%%eax), %%xmm1\n"    /* x[3] | x[1] */
    "movlps 40(%%eax), %%xmm2\n"    /* x[5] */
    "movhps 56(%%eax), %%xmm2\n"    /* x[7] | x[5] */
    "movaps  %%xmm1, %%xmm3\n"      /* x[3] | x[1] */
    "addps   %%xmm2, %%xmm1\n"      /* x[3] + x[7] | x[1] + x[5] */
    "subps   %%xmm2, %%xmm3\n"      /* x[3] - x[7] | x[1] - x[5] */
    "movaps (%%ecx), %%xmm4\n"      /* -1/sqrt2 | -1/sqrt2 | 1/sqrt2 | 1/sqrt2 */
    "movaps  %%xmm3, %%xmm6\n"      /* x[3] - x[7] | x[1] - x[5] */
    "mulps   %%xmm4, %%xmm3\n"      /* -1/s2*(x[3] - x[7]) | 1/s2*(x[1] - x[5]) */
    "shufps   $0xc8, %%xmm4, %%xmm4\n" /* -1/sqrt2 | 1/sqrt2 | -1/sqrt2 | 1/sqrt2 */
    "shufps   $0xb1, %%xmm6, %%xmm6\n" /* (x3-x7).re|(x3-x7).im|(x1-x5).re|(x1-x5).im */
    "mulps   %%xmm4, %%xmm6\n"      /* (x7-x3).re/s2|(x3-x7).im/s2|(x5-x1).re/s2|(x1-x5).im/s2 */
    "addps   %%xmm3, %%xmm6\n"      /* (-1-i)/sqrt2 * (x[3]-x[7]) | (1-i)/sqrt2 * (x[1] - x[5]) */
    "movhlps %%xmm1, %%xmm5\n"      /* x[3] + x[7] */
    "movlhps %%xmm6, %%xmm1\n"      /* (1+i)/sqrt2 * (x[1]-x[5]) | x[1]+x[5] */
    "shufps   $0xe4, %%xmm6, %%xmm5\n" /* (-1-i)/sqrt2 * (x[3]-x[7]) | x[3]+x[7] */
    "movaps  %%xmm1, %%xmm3\n"      /* (1-i)/sqrt2 * (x[1]-x[5]) | x[1]+x[5] */
    "addps   %%xmm5, %%xmm1\n"      /* u */
    "subps   %%xmm5, %%xmm3\n"      /* v */
    "movaps  %%xmm0, %%xmm2\n"      /* yb */
    "movaps  %%xmm7, %%xmm4\n"      /* yt */
    "movaps (%%edx), %%xmm5\n"
    "mulps   %%xmm5, %%xmm3\n"
    "addps   %%xmm1, %%xmm0\n"      /* yt + u */
    "subps   %%xmm1, %%xmm2\n"      /* yt - u */
    "shufps   $0xb1, %%xmm3, %%xmm3\n" /* -i * v */
    "movaps  %%xmm0, (%%eax)\n"
    "movaps  %%xmm2, 32(%%eax)\n"
    "addps   %%xmm3, %%xmm4\n"      /* yb - i*v */
    "subps   %%xmm3, %%xmm7\n"      /* yb + i*v */
    "movaps  %%xmm4, 16(%%eax)\n"
    "movaps  %%xmm7, 48(%%eax)\n"

    : "=a" (x)
    : "a" (x), "c" (hsqrt2_sse), "d" (C_1_sse));
}

static void fft_asmb_sse (ck_sse_t * ck, int k, complex_t *x, complex_t *wTB,
         const complex_t *d, const complex_t *d_3)
{
    ck->k = k;
    
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebp\n"
    "movl %%esp, %%ebp\n"

    "subl $8, %%esp\n"
    
    "pushl %%eax\n"
    "pushl %%ebx\n"
    "pushl %%ecx\n"
    "pushl %%edx\n"
    "pushl %%esi\n"
    "pushl %%edi\n"

    "movl 4(%%ecx), %%ebx\n"
    "movl %%ebx, -4(%%ebp)\n"
    "movl (%%ecx), %%ecx\n"

    "movl %%ecx, -8(%%ebp)\n"   /* k */
    "addl $8, %%edx\n" 
    "addl $8, %%esi\n"
    "shll $4, %%ecx\n"          /* 16k */

    /* TRANSZERO and TRANS */
    ".align 16\n"
    "movaps (%%eax), %%xmm0\n"     /* x[1] | x[0] */
    "movaps (%%edi), %%xmm1\n"     /* wT[1] | wT[0] */
    "movaps (%%edi, %%ecx), %%xmm2\n" /* wB[1] | wB[0] */
    "movlps (%%edx), %%xmm3\n"     /* d */
    "movlps (%%esi), %%xmm4\n"     /* d3 */
    "movhlps %%xmm1, %%xmm5\n"     /* wT[1] */
    "movhlps %%xmm2, %%xmm6\n"     /* wB[1] */
    "shufps $0x50, %%xmm3, %%xmm3\n" /* d[1].im | d[1].im | d[1].re | d[1].re */
    "shufps $0x50, %%xmm4, %%xmm4\n" /* d3[1].im | d3[1].im | d3[i].re | d3[i].re */
    "movlhps %%xmm5, %%xmm5\n"      /* wT[1] | wT[1] */
    "movlhps %%xmm6, %%xmm6\n"      /* wB[1] | wB[1] */
    "mulps   %%xmm3, %%xmm5\n"
    "mulps   %%xmm4, %%xmm6\n"
    "movhlps %%xmm5, %%xmm7\n"      /* wT[1].im * d[1].im | wT[1].re * d[1].im */
    "movlhps %%xmm6, %%xmm5\n"      /* wB[1].im * d3[1].re | wB[1].re * d3[1].re | wT[1].im * d[1].re | wT[1].re * d[1].re */
    "shufps $0xb1, %%xmm6, %%xmm7\n" /* wB[1].re * d3[1].im | wB[i].im * d3[1].im | wT[1].re * d[1].im | wT[1].im * d[1].im */
    "movl  -4(%%ebp), %%ebx\n"
    "movaps (%%ebx), %%xmm4\n"
    "mulps   %%xmm4, %%xmm7\n"
    "addps   %%xmm7, %%xmm5\n"      /* wB[1] * d3[1] | wT[1] * d[1] */
    "movlhps %%xmm5, %%xmm1\n"      /* d[1] * wT[1] | wT[0] */
    "shufps  $0xe4, %%xmm5, %%xmm2\n" /* d3[1] * wB[1] | wB[0] */
    "movaps  %%xmm1, %%xmm3\n"      /* d[1] * wT[1] | wT[0] */
    "leal   (%%eax, %%ecx, 2), %%ebx\n"
    "addps  %%xmm2, %%xmm1\n"       /* u */
    "subps  %%xmm2, %%xmm3\n"       /* v */
    "mulps  %%xmm4, %%xmm3\n"
    "movaps (%%eax, %%ecx), %%xmm5\n" /* xk[1] | xk[0] */
    "shufps $0xb1, %%xmm3, %%xmm3\n"  /* -i * v */
    "movaps %%xmm0, %%xmm2\n"       /* x[1] | x[0] */
    "movaps %%xmm5, %%xmm6\n"       /* xk[1] | xk[0] */
    "addps  %%xmm1, %%xmm0\n"
    "subps  %%xmm1, %%xmm2\n"
    "addps  %%xmm3, %%xmm5\n"
    "subps  %%xmm3, %%xmm6\n"
    "movaps %%xmm0, (%%eax)\n"
    "movaps %%xmm2, (%%ebx)\n"
    "movaps %%xmm5, (%%eax, %%ecx)\n"
    "movaps %%xmm6, (%%ebx, %%ecx)\n"
    "addl $16, %%eax\n"
    "addl $16, %%edi\n"
    "addl  $8, %%edx\n"
    "addl  $8, %%esi\n"
    "decl -8(%%ebp)\n"

    ".align 16\n"
".loop:\n"
    "movaps (%%edi), %%xmm0\n"      /* wT[1] | wT[0] */
    "movaps (%%edx), %%xmm1\n"      /* d[1] | d[0] */

    "movaps (%%edi, %%ecx), %%xmm4\n" /* wB[1] | wB[0] */
    "movaps (%%esi), %%xmm5\n"      /* d3[1] | d3[0] */

    "movhlps %%xmm0, %%xmm2\n"      /* wT[1] */
    "movhlps %%xmm1, %%xmm3\n"      /* d[1] */

    "movhlps %%xmm4, %%xmm6\n"      /* wB[1] */
    "movhlps %%xmm5, %%xmm7\n"      /* d3[1] */

    "shufps $0x50, %%xmm1, %%xmm1\n" /* d[0].im | d[0].im | d[0].re | d[0].re */
    "shufps $0x50, %%xmm3, %%xmm3\n" /* d[1].im | d[1].im | d[1].re | d[1].re */

    "movlhps %%xmm0, %%xmm0\n"       /* wT[0] | wT[0] */
    "shufps $0x50, %%xmm5, %%xmm5\n" /* d3[0].im | d3[0].im | d3[0].re | d3[0].re */
    "movlhps %%xmm2, %%xmm2\n"       /* wT[1] | wT[1] */
    "shufps $0x50, %%xmm7, %%xmm7\n" /* d3[1].im | d3[1].im | d3[1].re | d3[1].re */

    "mulps   %%xmm1, %%xmm0\n"  /* d[0].im * wT[0].im | d[0].im * wT[0].re | d[0].re * wT[0].im | d[0].re * wT[0].re */
    "mulps   %%xmm3, %%xmm2\n"  /* d[1].im * wT[1].im | d[1].im * wT[1].re | d[1].re * wT[1].im | d[1].re * wT[1].re */
    "movlhps %%xmm4, %%xmm4\n"  /* wB[0] | wB[0] */
    "movlhps %%xmm6, %%xmm6\n"  /* wB[1] | wB[1] */
    
    "movhlps %%xmm0, %%xmm1\n"  /* d[0].im * wT[0].im | d[0].im * wT[0].re */
    "movlhps %%xmm2, %%xmm0\n"  /* d[1].re * wT[1].im | d[1].re * wT[1].re | d[0].re * wT[0].im | d[0].re * wT[0].re */
    "mulps   %%xmm5, %%xmm4\n"  /* wB[0].im * d3[0].im | wB[0].re * d3[0].im | wB[0].im * d3[0].re | wB[0].re * d3[0].re */
    "mulps   %%xmm7, %%xmm6\n"  /* wB[1].im * d3[1].im | wB[1].re * d3[1].im | wB[1].im * d3[1].re | wB[1].re * d3[1].re */
    "shufps $0xb1, %%xmm2, %%xmm1\n" /* d[1].im * wT[1].re | d[1].im * wT[1].im | d[0].im * wT[0].re | d[0].im * wT[0].im */
    "movl -4(%%ebp), %%ebx\n"
    "movaps (%%ebx), %%xmm3\n"  /* 1.0 | -1.0 | 1.0 | -1.0 */

    "movhlps %%xmm4, %%xmm5\n"  /* wB[0].im * d3[0].im | wB[0].re * d3[0].im */
    "mulps   %%xmm3, %%xmm1\n"  /* d[1].im * wT[1].re | -d[1].im * wT[1].im | d[0].im * wT[0].re | -d[0].im * wT[0].im */
    "movlhps %%xmm6, %%xmm4\n"  /* wB[1].im * d3[1].re | wB[1].re * d3[1].re | wB[0].im * d3[0].re | wB[0].im * d3[0].re */
    "addps   %%xmm1, %%xmm0\n"  /* wT[1] * d[1] | wT[0] * d[0] */

    "shufps $0xb1, %%xmm6, %%xmm5\n" /* wB[1].re * d3[1].im | wB[1].im * d3[1].im | wB[0].re * d3[0].im | wB[0].im * d3[0].im */
    "mulps   %%xmm3, %%xmm5\n"  /* wB[1].re * d3[1].im | -wB[1].im * d3[1].im | wB[0].re * d3[0].im | -wB[0].im * d3[0].im */
    "addps   %%xmm5, %%xmm4\n"  /* wB[1] * d3[1] | wB[0] * d3[0] */

    "movaps %%xmm0, %%xmm1\n"   /* wT[1] * d[1] | wT[0] * d[0] */
    "addps  %%xmm4, %%xmm0\n"   /* u */
    "subps  %%xmm4, %%xmm1\n"   /* v */
    "movaps (%%eax), %%xmm6\n"  /* x[1] | x[0] */
    "leal   (%%eax, %%ecx, 2), %%ebx\n"
    "mulps  %%xmm3, %%xmm1\n"
    "addl $16, %%edi\n"
    "addl $16, %%esi\n"
    "shufps $0xb1, %%xmm1, %%xmm1\n"    /* -i * v */
    "movaps (%%eax, %%ecx), %%xmm7\n"   /* xk[1] | xk[0] */
    "movaps %%xmm6, %%xmm2\n"
    "movaps %%xmm7, %%xmm4\n"
    "addps  %%xmm0, %%xmm6\n"
    "subps  %%xmm0, %%xmm2\n"
    "movaps %%xmm6, (%%eax)\n"
    "movaps %%xmm2, (%%ebx)\n"
    "addps  %%xmm1, %%xmm7\n"
    "subps  %%xmm1, %%xmm4\n"
    "addl $16, %%edx\n"
    "movaps %%xmm7, (%%eax, %%ecx)\n"
    "movaps %%xmm4, (%%ebx, %%ecx)\n"

    "addl $16, %%eax\n"
    "decl -8(%%ebp)\n"
    "jnz .loop\n"

    ".align 16\n"
".end:\n"
    "popl %%edi\n"
    "popl %%esi\n"
    "popl %%edx\n"
    "popl %%ecx\n"
    "popl %%ebx\n"
    "popl %%eax\n"
    
    "addl $8, %%esp\n"
    
    "leave\n"
    : "=a" (x), "=D" (wTB)
    : "c" (ck), "a" (x), "D" (wTB), "d" (d), "S" (d_3) );
}
