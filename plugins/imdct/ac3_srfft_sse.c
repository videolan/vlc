/*****************************************************************************
 * ac3_srfft_sse.c: accelerated SSE ac3 fft functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: ac3_srfft_sse.c,v 1.1 2001/05/15 16:19:42 sam Exp $
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

#define MODULE_NAME imdctsse
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdio.h>

#include "defs.h"

#include <math.h>
#include <stdio.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "ac3_imdct.h"
#include "ac3_srfft.h"

void hsqrt2 (void);
void C_1 (void);
static void fft_4_sse (complex_t *x);
static void fft_8_sse (complex_t *x);
static void fft_asmb_sse (int k, complex_t *x, complex_t *wTB,
         const complex_t *d, const complex_t *d_3);

void _M( fft_64p ) ( complex_t *a )
{
    fft_8_sse(&a[0]); fft_4_sse(&a[8]); fft_4_sse(&a[12]);
    fft_asmb_sse(2, &a[0], &a[8], &delta16[0], &delta16_3[0]);
  
    fft_8_sse(&a[16]), fft_8_sse(&a[24]);
    fft_asmb_sse(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

    fft_8_sse(&a[32]); fft_4_sse(&a[40]); fft_4_sse(&a[44]);
    fft_asmb_sse(2, &a[32], &a[40], &delta16[0], &delta16_3[0]);

    fft_8_sse(&a[48]); fft_4_sse(&a[56]); fft_4_sse(&a[60]);
    fft_asmb_sse(2, &a[48], &a[56], &delta16[0], &delta16_3[0]);

    fft_asmb_sse(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);
}

void _M( fft_128p ) ( complex_t *a )
{
    fft_8_sse(&a[0]); fft_4_sse(&a[8]); fft_4_sse(&a[12]);
    fft_asmb_sse(2, &a[0], &a[8], &delta16[0], &delta16_3[0]);
  
    fft_8_sse(&a[16]), fft_8_sse(&a[24]);
    fft_asmb_sse(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

    fft_8_sse(&a[32]); fft_4_sse(&a[40]); fft_4_sse(&a[44]);
    fft_asmb_sse(2, &a[32], &a[40], &delta16[0], &delta16_3[0]);

    fft_8_sse(&a[48]); fft_4_sse(&a[56]); fft_4_sse(&a[60]);
    fft_asmb_sse(2, &a[48], &a[56], &delta16[0], &delta16_3[0]);

    fft_asmb_sse(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);

    fft_8_sse(&a[64]); fft_4_sse(&a[72]); fft_4_sse(&a[76]);
    /* fft_16(&a[64]); */
    fft_asmb_sse(2, &a[64], &a[72], &delta16[0], &delta16_3[0]);

    fft_8_sse(&a[80]); fft_8_sse(&a[88]);
  
    /* fft_32(&a[64]); */
    fft_asmb_sse(4, &a[64], &a[80],&delta32[0], &delta32_3[0]);

    fft_8_sse(&a[96]); fft_4_sse(&a[104]), fft_4_sse(&a[108]);
    /* fft_16(&a[96]); */
    fft_asmb_sse(2, &a[96], &a[104], &delta16[0], &delta16_3[0]);

    fft_8_sse(&a[112]), fft_8_sse(&a[120]);
    /* fft_32(&a[96]); */
    fft_asmb_sse(4, &a[96], &a[112], &delta32[0], &delta32_3[0]);
  
    /* fft_128(&a[0]); */
    fft_asmb_sse(16, &a[0], &a[64], &delta128[0], &delta128_3[0]);
}

void hsqrt2 (void)
{
    __asm__ (
     ".float 0f0.707106781188\n"
     ".float 0f0.707106781188\n"
     ".float 0f-0.707106781188\n"
     ".float 0f-0.707106781188\n"
     );
}

void C_1 (void)
{
    __asm__ (
     ".float 0f-1.0\n"
     ".float 0f1.0\n"
     ".float 0f-1.0\n"
     ".float 0f1.0\n"
     );
}

static void fft_4_sse (complex_t *x)
{
    __asm__ __volatile__ (
    "movups   (%%eax), %%xmm0\n"    /* x[1] | x[0] */
    "movups 16(%%eax), %%xmm2\n"    /* x[3] | x[2] */
    "movups  %%xmm0, %%xmm1\n"        /* x[1] | x[0] */
    "addps   %%xmm2, %%xmm0\n"        /* x[1] + x[3] | x[0] + x[2] */
    "subps   %%xmm2, %%xmm1\n"        /* x[1] - x[3] | x[0] - x[2] */
    "xorps   %%xmm6, %%xmm6\n"
    "movhlps %%xmm1, %%xmm4\n"        /* ? | x[1] - x[3] */
    "movhlps %%xmm0, %%xmm3\n"        /* ? | x[1] + x[3] */
    "subss   %%xmm4, %%xmm6\n"        /* 0 | -(x[1] - x[3]).re */
    "movlhps %%xmm1, %%xmm0\n"        /* x[0] - x[2] | x[0] + x[2] */
    "movlhps %%xmm6, %%xmm4\n"        /* 0 | -(x[1] - x[3]).re | (x[1] - x[3]).im | (x[3]-x[1]).re */
    "movups  %%xmm0, %%xmm2\n"        /* x[0] - x[2] | x[0] + x[2] */
    "shufps   $0x94, %%xmm4, %%xmm3\n" /* i*(x[1] - x[3]) | x[1] + x[3] */
    "addps   %%xmm3, %%xmm0\n"
    "subps   %%xmm3, %%xmm2\n"
    "movups  %%xmm0,   (%%eax)\n"
    "movups  %%xmm2, 16(%%eax)\n"
    : "=a" (x)
    : "a" (x) );
}

static void fft_8_sse (complex_t *x)
{
    __asm__ __volatile__ (
    "pushl   %%ebx\n"
    
    "movlps   (%%eax), %%xmm0\n"    /* x[0] */
    "movlps 32(%%eax), %%xmm1\n"    /* x[4] */
    "movhps 16(%%eax), %%xmm0\n"    /* x[2] | x[0] */
    "movhps 48(%%eax), %%xmm1\n"    /* x[6] | x[4] */
    "movups  %%xmm0, %%xmm2\n"        /* x[2] | x[0] */
    "xorps   %%xmm3, %%xmm3\n"
    "addps   %%xmm1, %%xmm0\n"        /* x[2] + x[6] | x[0] + x[4] */
    "subps   %%xmm1, %%xmm2\n"        /* x[2] - x[6] | x[0] - x[4] */
    "movhlps %%xmm0, %%xmm5\n"         /* x[2] + x[6] */
    "movhlps %%xmm2, %%xmm4\n"      /* x[2] - x[6] */
    "movlhps %%xmm2, %%xmm0\n"        /* x[0] - x[4] | x[0] + x[4] */
    "subss   %%xmm4, %%xmm3\n"        /* (x[2]-x[6]).im | -(x[2]-x[6]).re */
    "movups  %%xmm0, %%xmm7\n"        /* x[0] - x[4] | x[0] + x[4] */
    "movups  %%xmm3, %%xmm4\n"        /* (x[2]-x[6]).im | -(x[2]-x[6]).re */
    "movlps 8(%%eax), %%xmm1\n"        /* x[1] */
    "shufps   $0x14, %%xmm4, %%xmm5\n" /* i*(x[2] - x[6]) | x[2] + x[6] */

    "addps   %%xmm5, %%xmm0\n"        /* yt = i*(x2-x6)+x0-x4 | x2+x6+x0+x4 */
    "subps   %%xmm5, %%xmm7\n"        /* yb = i*(x6-x2)+x0-x4 | -x6-x2+x0+x4 */

    "movhps 24(%%eax), %%xmm1\n"    /* x[3] | x[1] */
    "movl   $hsqrt2, %%ebx\n"
    "movlps 40(%%eax), %%xmm2\n"    /* x[5] */
    "movhps 56(%%eax), %%xmm2\n"    /* x[7] | x[5] */
    "movups  %%xmm1, %%xmm3\n"        /* x[3] | x[1] */
    "addps   %%xmm2, %%xmm1\n"        /* x[3] + x[7] | x[1] + x[5] */
    "subps   %%xmm2, %%xmm3\n"        /* x[3] - x[7] | x[1] - x[5] */
    "movups (%%ebx), %%xmm4\n"        /* -1/sqrt2 | -1/sqrt2 | 1/sqrt2 | 1/sqrt2 */
    "movups  %%xmm3, %%xmm6\n"        /* x[3] - x[7] | x[1] - x[5] */
    "mulps   %%xmm4, %%xmm3\n"      /* -1/s2*(x[3] - x[7]) | 1/s2*(x[1] - x[5]) */
    "shufps   $0xc8, %%xmm4, %%xmm4\n" /* -1/sqrt2 | 1/sqrt2 | -1/sqrt2 | 1/sqrt2 */
    "shufps   $0xb1, %%xmm6, %%xmm6\n" /* (x3-x7).re|(x3-x7).im|(x1-x5).re|(x1-x5).im */
    "mulps   %%xmm4, %%xmm6\n"      /* (x7-x3).re/s2|(x3-x7).im/s2|(x5-x1).re/s2|(x1-x5).im/s2 */
    "addps   %%xmm3, %%xmm6\n"        /* (-1-i)/sqrt2 * (x[3]-x[7]) | (1-i)/sqrt2 * (x[1] - x[5]) */
    "movhlps %%xmm1, %%xmm5\n"        /* x[3] + x[7] */
    "movlhps %%xmm6, %%xmm1\n"        /* (1+i)/sqrt2 * (x[1]-x[5]) | x[1]+x[5] */
    "shufps   $0xe4, %%xmm6, %%xmm5\n"    /* (-1-i)/sqrt2 * (x[3]-x[7]) | x[3]+x[7] */
    "movups  %%xmm1, %%xmm3\n"        /* (1-i)/sqrt2 * (x[1]-x[5]) | x[1]+x[5] */
    "movl      $C_1, %%ebx\n"
    "addps   %%xmm5, %%xmm1\n"        /* u */
    "subps   %%xmm5, %%xmm3\n"        /* v */
    "movups  %%xmm0, %%xmm2\n"        /* yb */
    "movups  %%xmm7, %%xmm4\n"        /* yt */
    "movups (%%ebx), %%xmm5\n"
    "mulps   %%xmm5, %%xmm3\n"
    "addps   %%xmm1, %%xmm0\n"        /* yt + u */
    "subps   %%xmm1, %%xmm2\n"        /* yt - u */
    "shufps   $0xb1, %%xmm3, %%xmm3\n" /* -i * v */
    "movups  %%xmm0, (%%eax)\n"
    "movups  %%xmm2, 32(%%eax)\n"
    "addps   %%xmm3, %%xmm4\n"        /* yb - i*v */
    "subps   %%xmm3, %%xmm7\n"        /* yb + i*v */
    "movups  %%xmm4, 16(%%eax)\n"
    "movups  %%xmm7, 48(%%eax)\n"

    "popl    %%ebx\n"
    : "=a" (x)
    : "a" (x));
}

    
static void fft_asmb_sse (int k, complex_t *x, complex_t *wTB,
         const complex_t *d, const complex_t *d_3)
{
    __asm__ __volatile__ (
    "pushl %%ebp\n"
    "movl %%esp, %%ebp\n"

    "subl $4, %%esp\n"
    
    "pushl %%eax\n"
    "pushl %%ebx\n"
    "pushl %%ecx\n"
    "pushl %%edx\n"
    "pushl %%esi\n"
    "pushl %%edi\n"

    "movl  8(%%ebp), %%ecx\n"   /* k */
    "movl 12(%%ebp), %%eax\n"   /* x */
    "movl %%ecx, -4(%%ebp)\n"   /* k */
    "movl 16(%%ebp), %%ebx\n"   /* wT */
    "movl 20(%%ebp), %%edx\n"   /* d */
    "movl 24(%%ebp), %%esi\n"   /* d3 */
    "shll $4, %%ecx\n"          /* 16k */
    "addl $8, %%edx\n"
    "leal (%%eax, %%ecx, 2), %%edi\n"
    "addl $8, %%esi\n"
    
    /* TRANSZERO and TRANS */
    "movups (%%eax), %%xmm0\n"      /* x[1] | x[0] */
    "movups (%%ebx), %%xmm1\n"      /* wT[1] | wT[0] */
    "movups (%%ebx, %%ecx), %%xmm2\n" /* wB[1] | wB[0] */
    "movlps (%%edx), %%xmm3\n"      /* d */
    "movlps (%%esi), %%xmm4\n"      /* d3 */
    "movhlps %%xmm1, %%xmm5\n"      /* wT[1] */
    "movhlps %%xmm2, %%xmm6\n"      /* wB[1] */
    "shufps $0x50, %%xmm3, %%xmm3\n" /* d[1].im | d[1].im | d[1].re | d[1].re */
    "shufps $0x50, %%xmm4, %%xmm4\n" /* d3[1].im | d3[1].im | d3[i].re | d3[i].re */
    "movlhps %%xmm5, %%xmm5\n"      /* wT[1] | wT[1] */
    "movlhps %%xmm6, %%xmm6\n"      /* wB[1] | wB[1] */
    "mulps   %%xmm3, %%xmm5\n"
    "mulps   %%xmm4, %%xmm6\n"
    "movhlps %%xmm5, %%xmm7\n"      /* wT[1].im * d[1].im | wT[1].re * d[1].im */
    "movlhps %%xmm6, %%xmm5\n"      /* wB[1].im * d3[1].re | wB[1].re * d3[1].re | wT[1].im * d[1].re | wT[1].re * d[1].re */
    "shufps $0xb1, %%xmm6, %%xmm7\n" /* wB[1].re * d3[1].im | wB[i].im * d3[1].im | wT[1].re * d[1].im | wT[1].im * d[1].im */
    "movl $C_1, %%edi\n"
    "movups (%%edi), %%xmm4\n"
    "mulps   %%xmm4, %%xmm7\n"
    "addps   %%xmm7, %%xmm5\n"      /* wB[1] * d3[1] | wT[1] * d[1] */
    "movlhps %%xmm5, %%xmm1\n"      /* d[1] * wT[1] | wT[0] */
    "shufps  $0xe4, %%xmm5, %%xmm2\n" /* d3[1] * wB[1] | wB[0] */
    "movups  %%xmm1, %%xmm3\n"      /* d[1] * wT[1] | wT[0] */
    "leal   (%%eax, %%ecx, 2), %%edi\n"
    "addps  %%xmm2, %%xmm1\n"       /* u */
    "subps  %%xmm2, %%xmm3\n"       /* v */
    "mulps  %%xmm4, %%xmm3\n"
    "movups (%%eax, %%ecx), %%xmm5\n" /* xk[1] | xk[0] */
    "shufps $0xb1, %%xmm3, %%xmm3\n"  /* -i * v */
    "movups %%xmm0, %%xmm2\n"         /* x[1] | x[0] */
    "movups %%xmm5, %%xmm6\n"         /* xk[1] | xk[0] */
    "addps  %%xmm1, %%xmm0\n"
    "subps  %%xmm1, %%xmm2\n"
    "addps  %%xmm3, %%xmm5\n"
    "subps  %%xmm3, %%xmm6\n"
    "movups %%xmm0, (%%eax)\n"
    "movups %%xmm2, (%%edi)\n"
    "movups %%xmm5, (%%eax, %%ecx)\n"
    "movups %%xmm6, (%%edi, %%ecx)\n"
    "addl $16, %%eax\n"
    "addl $16, %%ebx\n"
    "addl  $8, %%edx\n"
    "addl  $8, %%esi\n"
    "decl -4(%%ebp)\n"

".loop:\n"
    "movups (%%ebx), %%xmm0\n"      /* wT[1] | wT[0] */
    "movups (%%edx), %%xmm1\n"      /* d[1] | d[0] */

    "movups (%%ebx, %%ecx), %%xmm4\n" /* wB[1] | wB[0] */
    "movups (%%esi), %%xmm5\n"      /* d3[1] | d3[0] */

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
    "shufps $0xb1, %%xmm2, %%xmm1\n"    /* d[1].im * wT[1].re | d[1].im * wT[1].im | d[0].im * wT[0].re | d[0].im * wT[0].im */
    "movl $C_1, %%edi\n"
    "movups (%%edi), %%xmm3\n"  /* 1.0 | -1.0 | 1.0 | -1.0 */

    "movhlps %%xmm4, %%xmm5\n"  /* wB[0].im * d3[0].im | wB[0].re * d3[0].im */
    "mulps   %%xmm3, %%xmm1\n"  /* d[1].im * wT[1].re | -d[1].im * wT[1].im | d[0].im * wT[0].re | -d[0].im * wT[0].im */
    "movlhps %%xmm6, %%xmm4\n"  /* wB[1].im * d3[1].re | wB[1].re * d3[1].re | wB[0].im * d3[0].re | wB[0].im * d3[0].re */
    "addps   %%xmm1, %%xmm0\n"  /* wT[1] * d[1] | wT[0] * d[0] */

    "shufps $0xb1, %%xmm6, %%xmm5\n"    /* wB[1].re * d3[1].im | wB[1].im * d3[1].im | wB[0].re * d3[0].im | wB[0].im * d3[0].im */
    "mulps   %%xmm3, %%xmm5\n"  /* wB[1].re * d3[1].im | -wB[1].im * d3[1].im | wB[0].re * d3[0].im | -wB[0].im * d3[0].im */
    "addps   %%xmm5, %%xmm4\n"  /* wB[1] * d3[1] | wB[0] * d3[0] */

    "movups %%xmm0, %%xmm1\n"   /* wT[1] * d[1] | wT[0] * d[0] */
    "addps  %%xmm4, %%xmm0\n"   /* u */
    "subps  %%xmm4, %%xmm1\n"   /* v */
    "movups (%%eax), %%xmm6\n"  /* x[1] | x[0] */
    "leal   (%%eax, %%ecx, 2), %%edi\n"
    "mulps  %%xmm3, %%xmm1\n"
    "addl $16, %%ebx\n"
    "addl $16, %%esi\n"
    "shufps $0xb1, %%xmm1, %%xmm1\n"    /* -i * v */
    "movups (%%eax, %%ecx), %%xmm7\n"   /* xk[1] | xk[0] */
    "movups %%xmm6, %%xmm2\n"
    "movups %%xmm7, %%xmm4\n"
    "addps  %%xmm0, %%xmm6\n"
    "subps  %%xmm0, %%xmm2\n"
    "movups %%xmm6, (%%eax)\n"
    "movups %%xmm2, (%%edi)\n"
    "addps  %%xmm1, %%xmm7\n"
    "subps  %%xmm1, %%xmm4\n"
    "addl $16, %%edx\n"
    "movups %%xmm7, (%%eax, %%ecx)\n"
    "movups %%xmm4, (%%edi, %%ecx)\n"

    "addl $16, %%eax\n"
    "decl -4(%%ebp)\n"
    "jnz .loop\n"

".end:\n"
    "popl %%edi\n"
    "popl %%esi\n"
    "popl %%edx\n"
    "popl %%ecx\n"
    "popl %%ebx\n"
    "popl %%eax\n"
    
    "addl $4, %%esp\n"

    "leave\n"
    ::);
}

