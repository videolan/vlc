/*****************************************************************************
 * ac3_srfft_3dn.c: accelerated 3D Now! ac3 fft functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ac3_srfft_3dn.c,v 1.5 2001/12/30 07:09:55 sam Exp $
 *
 * Authors: Renaud Dartus <reno@videolan.org>
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

void hsqrt2_3dn (void);
void C_1_3dn (void);
static void fft_4_3dn (complex_t *x);
static void fft_8_3dn (complex_t *x);
static void fft_asmb_3dn (int k, complex_t *x, complex_t *wTB,
	     const complex_t *d, const complex_t *d_3);

void _M( fft_64p ) ( complex_t *a )
{
	fft_8_3dn(&a[0]); fft_4_3dn(&a[8]); fft_4_3dn(&a[12]);
	fft_asmb_3dn(2, &a[0], &a[8], &delta16[0], &delta16_3[0]);
  
	fft_8_3dn(&a[16]), fft_8_3dn(&a[24]);
	fft_asmb_3dn(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

	fft_8_3dn(&a[32]); fft_4_3dn(&a[40]); fft_4_3dn(&a[44]);
	fft_asmb_3dn(2, &a[32], &a[40], &delta16[0], &delta16_3[0]);

	fft_8_3dn(&a[48]); fft_4_3dn(&a[56]); fft_4_3dn(&a[60]);
	fft_asmb_3dn(2, &a[48], &a[56], &delta16[0], &delta16_3[0]);

	fft_asmb_3dn(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);
}

void _M( fft_128p ) ( complex_t *a )
{
    fft_8_3dn(&a[0]); fft_4_3dn(&a[8]); fft_4_3dn(&a[12]);
	fft_asmb_3dn(2, &a[0], &a[8], &delta16[0], &delta16_3[0]);
  
	fft_8_3dn(&a[16]), fft_8_3dn(&a[24]);
	fft_asmb_3dn(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

	fft_8_3dn(&a[32]); fft_4_3dn(&a[40]); fft_4_3dn(&a[44]);
	fft_asmb_3dn(2, &a[32], &a[40], &delta16[0], &delta16_3[0]);

	fft_8_3dn(&a[48]); fft_4_3dn(&a[56]); fft_4_3dn(&a[60]);
	fft_asmb_3dn(2, &a[48], &a[56], &delta16[0], &delta16_3[0]);

	fft_asmb_3dn(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);

	fft_8_3dn(&a[64]); fft_4_3dn(&a[72]); fft_4_3dn(&a[76]);
	/* fft_16(&a[64]); */
	fft_asmb_3dn(2, &a[64], &a[72], &delta16[0], &delta16_3[0]);

	fft_8_3dn(&a[80]); fft_8_3dn(&a[88]);
  
	/* fft_32(&a[64]); */
	fft_asmb_3dn(4, &a[64], &a[80],&delta32[0], &delta32_3[0]);

	fft_8_3dn(&a[96]); fft_4_3dn(&a[104]), fft_4_3dn(&a[108]);
	/* fft_16(&a[96]); */
	fft_asmb_3dn(2, &a[96], &a[104], &delta16[0], &delta16_3[0]);

	fft_8_3dn(&a[112]), fft_8_3dn(&a[120]);
	/* fft_32(&a[96]); */
	fft_asmb_3dn(4, &a[96], &a[112], &delta32[0], &delta32_3[0]);
  
	/* fft_128(&a[0]); */
	fft_asmb_3dn(16, &a[0], &a[64], &delta128[0], &delta128_3[0]);
}

void hsqrt2_3dn (void)
{
    __asm__ (
     ".float 0f0.707106781188\n"
     ".float 0f0.707106781188\n"
     ".float 0f-0.707106781188\n"
     ".float 0f-0.707106781188\n"
     );
}

void C_1_3dn (void)
{
    __asm__ (
     ".float 0f-1.0\n"
     ".float 0f1.0\n"
     ".float 0f-1.0\n"
     ".float 0f1.0\n"
     );
}

static void fft_4_3dn (complex_t *x)
{
    __asm__ __volatile__ (
    ".align 16\n"
	"movq    (%%eax), %%mm0\n"      /* x[0] */
	"movq   8(%%eax), %%mm1\n"      /* x[1] */
	"movq  16(%%eax), %%mm2\n"      /* x[2] */
	"movq  24(%%eax), %%mm3\n"      /* x[3] */
	"movq    %%mm0, %%mm4\n"	    /* x[1] */
	"movq    %%mm1, %%mm5\n"		/* x[1] */
	"movq    %%mm0, %%mm6\n"		/* x[0] */
	"pfadd   %%mm2, %%mm0\n"		/* x[0] + x[2] */
	"pfadd   %%mm3, %%mm1\n"		/* x[1] + x[3] */
	"pfsub   %%mm2, %%mm4\n"		/* x[0] - x[2] */
	"pfsub   %%mm3, %%mm5\n"		/* x[1] - x[3] */

    "pfadd   %%mm1, %%mm0\n"        /* x[0] + x[2] + x[1] + x[3] */
    "pfsub   %%mm1, %%mm6\n"        /* x[0] + x[2] - x[1] - x[3] */

    "movq   %%mm0, (%%eax)\n"
    "movq   %%mm6, 16(%%eax)\n"
   
    "pxor    %%mm6, %%mm6\n"
    "movq    %%mm5, %%mm2\n"        /* x[1] - x[3] */
    "movq    %%mm4, %%mm3\n"        /* x[0] - x[2] */
    "pfsub   %%mm5, %%mm6\n"        /* x[3] - x[1] */
    
    "punpckhdq %%mm2,%%mm2\n"       /* x[1] - x[3].im */
    "punpckldq %%mm6,%%mm6\n"       /* x[3] - x[1].re */
    "punpckhdq %%mm6,%%mm2\n"       /* x[3] - x[1].re,  x[1] - x[3].im */
    
	"pfsub   %%mm2, %%mm4\n"        /* x0i-x2i-x3r+x1.r,x0r-x2r-x1i+x3i */
    "pfadd   %%mm3, %%mm2\n"        /* x0i-x2i+x3r-x1.r, x0r-x2r+x1i-x3.i */

    "movq  %%mm2,  8(%%eax)\n"    /* mm4_2 + mm6_1, mm4_1 + mm5_2 */
	"movq  %%mm4, 24(%%eax)\n"    /* mm4_2 - mm6_1, mm4_1 - mm5_2 */
	"femms\n"
    : "=a" (x)
    : "a" (x) );
}

static void fft_8_3dn (complex_t *x)
{
  register float wT1_r, wT1_i, wB1_r, wB1_i, wT2_r, wT2_i, wB2_r, wB2_i;
  
  wT1_r = x[1].real;
  wT1_i = x[1].imag;
  wB1_r = x[3].real;
  wB1_i = x[3].imag;

  x[1] = x[2];
  x[2] = x[4];
  x[3] = x[6];
  { /* fft_4 */
      register float yt_r, yt_i, yb_r, yb_i, u_r, u_i, vi_r, vi_i;
  
      yt_r = x[0].real;
      yb_r = yt_r - x[2].real;
      yt_r += x[2].real;

      u_r = x[1].real;
      vi_i = x[3].real - u_r;
      u_r += x[3].real;
  
      u_i = x[1].imag;
      vi_r = u_i - x[3].imag;
      u_i += x[3].imag;

      yt_i = yt_r;
      yt_i += u_r;
      x[0].real = yt_i;
      yt_r -= u_r;
      x[2].real = yt_r;
      yt_i = yb_r;
      yt_i += vi_r;
      x[1].real = yt_i;
      yb_r -= vi_r;
      x[3].real = yb_r;

      yt_i = x[0].imag;
      yb_i = yt_i - x[2].imag;
      yt_i += x[2].imag;

      yt_r = yt_i;
      yt_r += u_i;
      x[0].imag = yt_r;
      yt_i -= u_i;
      x[2].imag = yt_i;
      yt_r = yb_i;
      yt_r += vi_i;
      x[1].imag = yt_r;
      yb_i -= vi_i;
      x[3].imag = yb_i;
  }
  
  /* x[0] x[4] */
  wT2_r = x[5].real;
  wT2_r += x[7].real;
  wT2_r += wT1_r;
  wT2_r += wB1_r;
  wT2_i = wT2_r;
  wT2_r += x[0].real;
  wT2_i = x[0].real - wT2_i;
  x[0].real = wT2_r;
  x[4].real = wT2_i;

  wT2_i = x[5].imag;
  wT2_i += x[7].imag;
  wT2_i += wT1_i;
  wT2_i += wB1_i;
  wT2_r = wT2_i;
  wT2_r += x[0].imag;
  wT2_i = x[0].imag - wT2_i;
  x[0].imag = wT2_r;
  x[4].imag = wT2_i;
  
  /* x[2] x[6] */
  wT2_r = x[5].imag;
  wT2_r -= x[7].imag;
  wT2_r += wT1_i;
  wT2_r -= wB1_i;
  wT2_i = wT2_r;
  wT2_r += x[2].real;
  wT2_i = x[2].real - wT2_i;
  x[2].real = wT2_r;
  x[6].real = wT2_i;

  wT2_i = x[5].real;
  wT2_i -= x[7].real;
  wT2_i += wT1_r;
  wT2_i -= wB1_r;
  wT2_r = wT2_i;
  wT2_r += x[2].imag;
  wT2_i = x[2].imag - wT2_i;
  x[2].imag = wT2_i;
  x[6].imag = wT2_r;

  /* x[1] x[5] */
  wT2_r = wT1_r;
  wT2_r += wB1_i;
  wT2_r -= x[5].real;
  wT2_r -= x[7].imag;
  wT2_i = wT1_i;
  wT2_i -= wB1_r;
  wT2_i -= x[5].imag;
  wT2_i += x[7].real;

  wB2_r = wT2_r;
  wB2_r += wT2_i;
  wT2_i -= wT2_r;
  wB2_r *= HSQRT2;
  wT2_i *= HSQRT2;
  wT2_r = wB2_r;
  wB2_r += x[1].real;
  wT2_r =  x[1].real - wT2_r;

  wB2_i = x[5].real;
  x[1].real = wB2_r;
  x[5].real = wT2_r;

  wT2_r = wT2_i;
  wT2_r += x[1].imag;
  wT2_i = x[1].imag - wT2_i;
  wB2_r = x[5].imag;
  x[1].imag = wT2_r;
  x[5].imag = wT2_i;

  /* x[3] x[7] */
  wT1_r -= wB1_i;
  wT1_i += wB1_r;
  wB1_r = wB2_i - x[7].imag;
  wB1_i = wB2_r + x[7].real;
  wT1_r -= wB1_r;
  wT1_i -= wB1_i;
  wB1_r = wT1_r + wT1_i;
  wB1_r *= HSQRT2;
  wT1_i -= wT1_r;
  wT1_i *= HSQRT2;
  wB2_r = x[3].real;
  wB2_i = wB2_r + wT1_i;
  wB2_r -= wT1_i;
  x[3].real = wB2_i;
  x[7].real = wB2_r;
  wB2_i = x[3].imag;
  wB2_r = wB2_i + wB1_r;
  wB2_i -= wB1_r;
  x[3].imag = wB2_i;
  x[7].imag = wB2_r;
}

    
static void fft_asmb_3dn (int k, complex_t *x, complex_t *wTB,
	     const complex_t *d, const complex_t *d_3)
{
  register complex_t  *x2k, *x3k, *x4k, *wB;
  register float a_r, a_i, a1_r, a1_i, u_r, u_i, v_r, v_i;

  x2k = x + 2 * k;
  x3k = x2k + 2 * k;
  x4k = x3k + 2 * k;
  wB = wTB + 2 * k;
  
  TRANSZERO(x[0],x2k[0],x3k[0],x4k[0]);
  TRANS(x[1],x2k[1],x3k[1],x4k[1],wTB[1],wB[1],d[1],d_3[1]);
  
  --k;
  for(;;) {
     TRANS(x[2],x2k[2],x3k[2],x4k[2],wTB[2],wB[2],d[2],d_3[2]);
     TRANS(x[3],x2k[3],x3k[3],x4k[3],wTB[3],wB[3],d[3],d_3[3]);
     if (!--k) break;
     x += 2;
     x2k += 2;
     x3k += 2;
     x4k += 2;
     d += 2;
     d_3 += 2;
     wTB += 2;
     wB += 2;
  }
}
