/*****************************************************************************
 * ac3_imdct_3dn.c: accelerated 3D Now! ac3 DCT
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_imdct_3dn.c,v 1.9 2001/12/30 07:09:55 sam Exp $
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
#include "ac3_imdct_common.h"
#include "ac3_retables.h"

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

void _M( fft_64p )  ( complex_t *x );
void _M( fft_128p ) ( complex_t *a );

static void imdct512_pre_ifft_twiddle_3dn (const int *pmt, complex_t *buf, float *data, float *xcos_sin_sse);
static void imdct512_post_ifft_twiddle_3dn (complex_t *buf, float *xcos_sin_sse);
static void imdct512_window_delay_3dn (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt);
static void imdct512_window_delay_nol_3dn (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt);


void _M( imdct_init ) (imdct_t * p_imdct)
{
	int i;
	float scale = 181.019;

	for (i=0; i < 128; i++)
	{
		float xcos_i = cos(2.0f * M_PI * (8*i+1)/(8*N)) * scale;
		float xsin_i = sin(2.0f * M_PI * (8*i+1)/(8*N)) * scale;
		p_imdct->xcos_sin_sse[i * 4]     = xcos_i;
		p_imdct->xcos_sin_sse[i * 4 + 1] = -xsin_i;
		p_imdct->xcos_sin_sse[i * 4 + 2] = -xsin_i;
		p_imdct->xcos_sin_sse[i * 4 + 3] = -xcos_i;
	}
}

void _M( imdct_do_512 ) (imdct_t * p_imdct, float data[], float delay[])
{
	imdct512_pre_ifft_twiddle_3dn (pm128, p_imdct->buf, data, p_imdct->xcos_sin_sse);
	_M( fft_128p ) (p_imdct->buf);
	imdct512_post_ifft_twiddle_3dn (p_imdct->buf, p_imdct->xcos_sin_sse);
    imdct512_window_delay_3dn (p_imdct->buf, data, window, delay);
}

void _M( imdct_do_512_nol ) (imdct_t * p_imdct, float data[], float delay[])
{
	imdct512_pre_ifft_twiddle_3dn (pm128, p_imdct->buf, data, p_imdct->xcos_sin_sse);  
	_M( fft_128p ) (p_imdct->buf);
	imdct512_post_ifft_twiddle_3dn (p_imdct->buf, p_imdct->xcos_sin_sse);
    imdct512_window_delay_nol_3dn (p_imdct->buf, data, window, delay);
}

static void imdct512_pre_ifft_twiddle_3dn (const int *pmt, complex_t *buf, float *data, float *xcos_sin_sse)
{
    __asm__ __volatile__ (	
    ".align 16\n"
	"pushl %%ebx\n"
	"pushl %%esi\n"
    
	"movl $128, %%ebx\n"         /* loop counter */

    ".align 16\n"
".loop:\n"
	"movl  (%%eax), %%esi\n"
	"movd (%%ecx, %%esi, 8), %%mm1\n"   /* 2j */
    "punpckldq %%mm1, %%mm1\n"          /* 2j | 2j */

	"shll $1, %%esi\n"

	"movq (%%edx, %%esi, 8), %%mm0\n"   /* -s_j | c_j */
	"movq 8(%%edx, %%esi, 8), %%mm2\n"  /* -c_j | -s_j */

	"negl %%esi\n"

	"movd 1020(%%ecx, %%esi, 4), %%mm4\n" /* 255-2j */
    "punpckldq %%mm4, %%mm4\n"  /* 255-2j | 255-2j */
	"addl $4, %%eax\n"

	"pfmul   %%mm4, %%mm0\n"    /* 255-2j * -s_j | 255-2j  * c_j */
	"pfmul   %%mm1, %%mm2\n"    /* 2j * -c_j | 2j * -s_j */
	"addl    $8, %%edi\n"
	"pfadd   %%mm2, %%mm0\n"    /* 2j * -c_j + 255-2j * -s_j | 2j * -s_j + 255-2j * c_j */
    
	"movq  %%mm0, -8(%%edi)\n"
	"decl %%ebx\n"
   	"jnz .loop\n"

	"popl %%esi\n"
	"popl %%ebx\n"

	"femms\n"
    : "=D" (buf)
    : "a" (pmt), "c" (data), "d" (xcos_sin_sse), "D" (buf));
}

static void imdct512_post_ifft_twiddle_3dn (complex_t *buf, float *xcos_sin_sse)
{
    __asm__ __volatile__ ( 
    ".align 16\n"
	"pushl %%ebx\n"
	"movl $64, %%ebx\n"         /* loop counter */

    ".align 16\n"
".loop1:\n"
	"movq	(%%eax), %%mm0\n"   /* im0 | re0 */
	"movq	  %%mm0, %%mm1\n"   /* im0 | re0 */
    "punpckldq %%mm0, %%mm0\n"  /* re0 | re0 */
    "punpckhdq %%mm1, %%mm1\n"  /* im0 | im0 */
    
	"movq  (%%ecx), %%mm2\n"    /* -s | c */
	"movq 8(%%ecx), %%mm3\n"    /* -c | -s */
    "movq    %%mm3, %%mm4\n"

    "punpckhdq %%mm2,%%mm3\n"   /* -s | -c */
    "punpckldq %%mm2,%%mm4\n"   /*  c | -s */
    
	"movq  8(%%eax), %%mm2\n"   /* im1 | re1 */
	"movq   %%mm2, %%mm5\n"     /* im1 | re1 */
    "punpckldq %%mm2, %%mm2\n"  /* re1 | re1 */
    "punpckhdq %%mm5, %%mm5\n"  /* im1 | im1 */

   	"pfmul %%mm3, %%mm0\n"      /* -s * re0 | -c * re0 */
	"pfmul %%mm4, %%mm1\n"      /* c * im0 | -s * im0 */

	"movq  16(%%ecx), %%mm6\n"  /* -s1 | c1 */
	"movq  24(%%ecx), %%mm7\n"  /* -c1 | -s1 */
    "movq   %%mm7, %%mm4\n"
    
    "punpckhdq %%mm6,%%mm7\n"   /* -s1 | -c1 */
    "punpckldq %%mm6,%%mm4\n"   /*  c1 | -s1 */
    
	"pfmul %%mm7, %%mm2\n"      /* -s1*re1 | -c1*re1 */
	"pfmul %%mm4, %%mm5\n"      /* c1*im1 | -s1*im1 */

	"pfadd %%mm1, %%mm0\n"      /* -s * re0 + c * im0 | -c * re0 - s * im0 */
	"pfadd %%mm5, %%mm2\n"      /* -s1 * re1 + c1 * im1 | -c1 * re1 - s1 * im1 */

	"movq %%mm0, (%%eax)\n"
	"movq %%mm2, 8(%%eax)\n"
	"addl $32, %%ecx\n"
	"addl $16, %%eax\n"
	"decl %%ebx\n"
	"jnz .loop1\n"

	"popl %%ebx\n"
	"femms\n"
    : "=a" (buf)
    : "a" (buf), "c" (xcos_sin_sse) );
}

static void imdct512_window_delay_3dn (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt)
{
    __asm__ __volatile__ (
    ".align 16\n"

	"pushl %%eax\n"
	"pushl %%ebx\n"
	"pushl %%ecx\n"
	"pushl %%edx\n"
	"pushl %%edi\n"
	"pushl %%esi\n"
	"pushl %%ebp\n"

	"movl %%esi, %%ebp\n"         /* buf */
	"movl $32, %%ebx\n"         /* loop count */
	"leal 516(%%ebp), %%esi\n"  /* buf[64].im */
	"leal 504(%%ebp), %%edi\n"  /* buf[63].re */

        
    ".align 16\n"
".first_128_samples:\n"
	"movd   (%%esi), %%mm0\n" /* im0 */
	"movd  8(%%esi), %%mm2\n" /* im1 */
	"movd   (%%edi), %%mm1\n" /* re0 */
	"movd -8(%%edi), %%mm3\n" /* re1 */

    "pxor   %%mm4, %%mm4\n"
    "pxor   %%mm5, %%mm5\n"
    "pfsub  %%mm0, %%mm4\n" /* -im0 */
    "pfsub  %%mm2, %%mm5\n" /* -im1 */
    
	"punpckldq %%mm1, %%mm4\n"      /* re0 | -im0 */
	"punpckldq %%mm3, %%mm5\n"      /* re1 | -im1 */

	"movq  (%%edx), %%mm0\n"      /* w1 | w0 */
	"movq 8(%%edx), %%mm1\n"      /* w3 | w2 */
	"movq  (%%ecx), %%mm2\n"      /* d1 | d0 */
	"movq 8(%%ecx), %%mm3\n"      /* d3 | d2 */

    "pfmul     %%mm4, %%mm0\n"      /* w1*re0 | -w0*im0 */
	"pfmul     %%mm5, %%mm1\n"      /* w3*re1 | -w2*im1 */

    "pfadd     %%mm2, %%mm0\n"      /* w1*re0+d1 | -w0*im0+d0 */
    "pfadd     %%mm3, %%mm1\n"      /* w3*re1+d3 | -w2*im1+d2 */

	"addl $16, %%edx\n"
	"movq %%mm0,  (%%eax)\n"
	"movq %%mm1, 8(%%eax)\n"
	"addl $16, %%ecx\n"
	"addl $16, %%esi\n"
	"addl $16, %%eax\n"
    "addl $-16, %%edi\n"
	"decl %%ebx\n"
	"jnz .first_128_samples\n"

	"movl %%ebp, %%esi\n"    /* buf[0].re */
	"movl $32, %%ebx\n"         /* loop count */
	"leal 1020(%%ebp), %%edi\n" /* buf[127].im */
    
    ".align 16\n"
".second_128_samples:\n"
	"movd   (%%esi), %%mm0\n" /* buf[i].re */
	"movd  8(%%esi), %%mm2\n" /* re1 */
	"movd   (%%edi), %%mm1\n" /* buf[127-i].im */
	"movd -8(%%edi), %%mm3\n" /* im1 */
    
    "pxor   %%mm4, %%mm4\n"
    "pxor   %%mm5, %%mm5\n"
    "pfsub  %%mm0, %%mm4\n" /* -re0 */
    "pfsub  %%mm2, %%mm5\n" /* -re1 */
    
	"punpckldq %%mm1, %%mm4\n"     /* im0 | -re0 */
	"punpckldq %%mm3, %%mm5\n"     /* im1 | -re1 */

	"movq (%%edx), %%mm0\n"  /* w1 | w0 */
	"movq 8(%%edx), %%mm1\n"  /* w3 | w2 */
	"movq (%%ecx), %%mm2\n"  /* d1 | d0 */
	"movq 8(%%ecx), %%mm3\n"  /* d3 | d2 */

	"addl $16, %%esi\n"
    
    "pfmul     %%mm4, %%mm0\n"      /* w1*im0 | -w0*re0 */
	"pfmul     %%mm5, %%mm1\n"      /* w3*im1 | -w2*re1 */
    
	"pfadd %%mm2, %%mm0\n"      /* w1*im0+d1 | -w0*re0+d0 */
	"pfadd %%mm3, %%mm1\n"      /* w3*im1+d3 | -w2*re1+d2 */
    
	"addl $-16, %%edi\n"
	
    "movq %%mm0, (%%eax)\n"
    "movq %%mm1, 8(%%eax)\n"
    
    "addl $16, %%edx\n"
	"addl $16, %%eax\n"
	"addl $16, %%ecx\n"
	"decl %%ebx\n"
	"jnz .second_128_samples\n"

	"leal 512(%%ebp), %%esi\n"  /* buf[64].re */
	"leal 508(%%ebp), %%edi\n"  /* buf[63].im */
	"movl $32, %%ebx\n"         /* loop count */
    "addl $-1024, %%ecx\n"      /* delay */

    ".align 16\n"
".first_128_delay:\n"
	"movd   (%%esi), %%mm0\n" /* re0 */
	"movd  8(%%esi), %%mm2\n" /* re1 */
	"movd   (%%edi), %%mm1\n" /* im0 */
	"movd -8(%%edi), %%mm3\n" /* im1 */

    "pxor   %%mm4, %%mm4\n"
    "pxor   %%mm5, %%mm5\n"
    "pfsub  %%mm0, %%mm4\n" /* -re0 */
    "pfsub  %%mm2, %%mm5\n" /* -re1 */
    
	"punpckldq %%mm1, %%mm4\n"     /* im0 | -re0 */
	"punpckldq %%mm3, %%mm5\n"     /* im1 | -re1 */

    
	"movq -16(%%edx), %%mm1\n"   /* w3 | w2 */
	"movq  -8(%%edx), %%mm0\n"   /* w1 | w0 */
    
	"addl $-16, %%edx\n"

    "pfmul     %%mm4, %%mm0\n"      /* w1*im0 | -w0*re0 */
	"pfmul     %%mm5, %%mm1\n"      /* w3*im1 | -w2*re1 */

	"movq %%mm0, (%%ecx)\n"
	"movq %%mm1, 8(%%ecx)\n"
	"addl  $16, %%esi\n"
	"addl $-16, %%edi\n"
	"addl  $16, %%ecx\n"
	"decl %%ebx\n"
	"jnz .first_128_delay\n"

	"leal    4(%%ebp), %%esi\n" /* buf[0].im */
	"leal 1016(%%ebp), %%edi\n" /* buf[127].re */
	"movl $32, %%ebx\n"         /* loop count */
    
    ".align 16\n"
".second_128_delay:\n"
	"movd   (%%esi), %%mm0\n" /* im0 */
	"movd  8(%%esi), %%mm2\n" /* im1 */
	"movd   (%%edi), %%mm1\n" /* re0 */
	"movd -8(%%edi), %%mm3\n" /* re1 */

    "pxor   %%mm4, %%mm4\n"
    "pxor   %%mm5, %%mm5\n"
    "pfsub  %%mm1, %%mm4\n" /* -re0 */
    "pfsub  %%mm3, %%mm5\n" /* -re1 */
    
	"punpckldq %%mm4, %%mm0\n"     /* -re0 | im0 */
	"punpckldq %%mm5, %%mm2\n"     /* -re1 | im1 */

    
	"movq -16(%%edx), %%mm1\n"   /* w3 | w2 */
	"movq  -8(%%edx), %%mm3\n"   /* w1 | w0 */
    
	"addl $-16, %%edx\n"

    "pfmul     %%mm0, %%mm1\n"      /* -w1*re0 | w0*im0 */
	"pfmul     %%mm2, %%mm3\n"      /* -w3*re1 | w2*im1 */

    
	"movq %%mm1, (%%ecx)\n"
	"movq %%mm3, 8(%%ecx)\n"
	"addl  $16, %%esi\n"
	"addl $-16, %%edi\n"
	"addl  $16, %%ecx\n"
	"decl %%ebx\n"
    "jnz .second_128_delay\n"

	"popl %%ebp\n"
	"popl %%esi\n"
	"popl %%edi\n"
	"popl %%edx\n"
	"popl %%ecx\n"
	"popl %%ebx\n"
	"popl %%eax\n"
	
	"femms\n"
    : "=S" (buf), "=a" (data_ptr), "=c" (delay_prt), "=d" (window_prt)
    : "S" (buf), "a" (data_ptr), "c" (delay_prt), "d" (window_prt));
}

static void imdct512_window_delay_nol_3dn (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt)
{
    __asm__ __volatile__ (
    ".align 16\n"

	"pushl %%eax\n"
	"pushl %%ebx\n"
	"pushl %%ecx\n"
	"pushl %%edx\n"
	"pushl %%edi\n"
	"pushl %%esi\n"
	"pushl %%ebp\n"

	"movl %%esi, %%ebp\n"         /* buf */
	"movl $32, %%ebx\n"         /* loop count */
	"leal 516(%%ebp), %%esi\n"  /* buf[64].im */
	"leal 504(%%ebp), %%edi\n"  /* buf[63].re */

    ".align 16\n"
".first_128_samples2:\n"
	"movd   (%%esi), %%mm0\n" /* im0 */
	"movd  8(%%esi), %%mm2\n" /* im1 */
	"movd   (%%edi), %%mm1\n" /* re0 */
	"movd -8(%%edi), %%mm3\n" /* re1 */

    "pxor   %%mm4, %%mm4\n"
    "pxor   %%mm5, %%mm5\n"
    "pfsub  %%mm0, %%mm4\n" /* -im0 */
    "pfsub  %%mm2, %%mm5\n" /* -im1 */
    
	"punpckldq %%mm1, %%mm4\n"      /* re0 | -im0 */
	"punpckldq %%mm3, %%mm5\n"      /* re1 | -im1 */

	"movq (%%edx), %%mm0\n"      /* w1 | w0 */
	"movq 8(%%edx), %%mm1\n"     /* w3 | w2 */

    "pfmul     %%mm4, %%mm0\n"      /* w1*re0 | -w0*im0 */
	"pfmul     %%mm5, %%mm1\n"      /* w3*re1 | -w2*im1 */

	"addl $16, %%edx\n"
	"movq %%mm0, (%%eax)\n"
	"movq %%mm1, 8(%%eax)\n"
	"addl $16, %%ecx\n"
	"addl $16, %%esi\n"
	"addl $16, %%eax\n"
    "addl $-16, %%edi\n"
	"decl %%ebx\n"
	"jnz .first_128_samples2\n"

	"movl %%ebp, %%esi\n"    /* buf[0].re */
	"movl $32, %%ebx\n"         /* loop count */
	"leal 1020(%%ebp), %%edi\n" /* buf[127].im */
    
    ".align 16\n"
".second_128_samples2:\n"
	"movd   (%%esi), %%mm0\n" /* buf[i].re */
	"movd  8(%%esi), %%mm2\n" /* re1 */
	"movd   (%%edi), %%mm1\n" /* buf[127-i].im */
	"movd -8(%%edi), %%mm3\n" /* im1 */

    "pxor   %%mm4, %%mm4\n"
    "pxor   %%mm5, %%mm5\n"
    "pfsub  %%mm0, %%mm4\n" /* -re0 */
    "pfsub  %%mm2, %%mm5\n" /* -re1 */
    
	"punpckldq %%mm1, %%mm4\n"     /* im0 | -re0 */
	"punpckldq %%mm3, %%mm5\n"     /* im1 | -re1 */

	"movq (%%edx), %%mm0\n"  /* w1 | w0 */
	"movq 8(%%edx), %%mm1\n"  /* w3 | w2 */

	"addl $16, %%esi\n"
    
    "pfmul     %%mm4, %%mm0\n"      /* w1*im0 | -w0*re0 */
	"pfmul     %%mm5, %%mm1\n"      /* w3*im1 | -w2*re1 */
    
	"addl $-16, %%edi\n"
	
    "movq %%mm0, (%%eax)\n"
    "movq %%mm1, 8(%%eax)\n"
    
    "addl $16, %%edx\n"
	"addl $16, %%eax\n"
	"addl $16, %%ecx\n"
	"decl %%ebx\n"
	"jnz .second_128_samples2\n"

	"leal 512(%%ebp), %%esi\n"  /* buf[64].re */
	"leal 508(%%ebp), %%edi\n"  /* buf[63].im */
	"movl $32, %%ebx\n"         /* loop count */
	"addl  $-1024, %%ecx\n"  /* delay */

    ".align 16\n"
".first_128_delays:\n"
	"movd   (%%esi), %%mm0\n" /* re0 */
	"movd  8(%%esi), %%mm2\n" /* re1 */
	"movd   (%%edi), %%mm1\n" /* im0 */
	"movd -8(%%edi), %%mm3\n" /* im1 */

    "pxor   %%mm4, %%mm4\n"
    "pxor   %%mm5, %%mm5\n"
    "pfsub  %%mm0, %%mm4\n" /* -re0 */
    "pfsub  %%mm2, %%mm5\n" /* -re1 */
    
	"punpckldq %%mm1, %%mm4\n"     /* im0 | -re0 */
	"punpckldq %%mm3, %%mm5\n"     /* im1 | -re1 */

    
	"movq -16(%%edx), %%mm1\n"   /* w3 | w2 */
	"movq  -8(%%edx), %%mm0\n"   /* w1 | w0 */
    
	"addl $-16, %%edx\n"

    "pfmul     %%mm4, %%mm0\n"      /* w1*im0 | -w0*re0 */
	"pfmul     %%mm5, %%mm1\n"      /* w3*im1 | -w2*re1 */

    
	"movq %%mm0, (%%ecx)\n"
	"movq %%mm1, 8(%%ecx)\n"
	"addl  $16, %%esi\n"
	"addl $-16, %%edi\n"
	"addl  $16, %%ecx\n"
	"decl %%ebx\n"
	"jnz .first_128_delays\n"

	"leal    4(%%ebp), %%esi\n" /* buf[0].im */
	"leal 1016(%%ebp), %%edi\n" /* buf[127].re */
	"movl $32, %%ebx\n"         /* loop count */
    
    ".align 16\n"
".second_128_delays:\n"
	"movd   (%%esi), %%mm0\n" /* im0 */
	"movd  8(%%esi), %%mm2\n" /* im1 */
	"movd   (%%edi), %%mm1\n" /* re0 */
	"movd -8(%%edi), %%mm3\n" /* re1 */

    "pxor   %%mm4, %%mm4\n"
    "pxor   %%mm5, %%mm5\n"
    "pfsub  %%mm1, %%mm4\n" /* -re0 */
    "pfsub  %%mm3, %%mm5\n" /* -re1 */
    
	"punpckldq %%mm4, %%mm0\n"     /* -re0 | im0 */
	"punpckldq %%mm5, %%mm2\n"     /* -re1 | im1 */

    
	"movq -16(%%edx), %%mm1\n"   /* w3 | w2 */
	"movq  -8(%%edx), %%mm3\n"   /* w1 | w0 */
    
	"addl $-16, %%edx\n"

    "pfmul     %%mm0, %%mm1\n"      /* -w1*re0 | w0*im0 */
	"pfmul     %%mm2, %%mm3\n"      /* -w3*re1 | w2*im1 */

    
	"movq %%mm1, (%%ecx)\n"
	"movq %%mm3, 8(%%ecx)\n"
	"addl  $16, %%esi\n"
	"addl $-16, %%edi\n"
	"addl  $16, %%ecx\n"
	"decl %%ebx\n"
    "jnz .second_128_delays\n"

    "popl %%ebp\n"
	"popl %%esi\n"
	"popl %%edi\n"
	"popl %%edx\n"
	"popl %%ecx\n"
	"popl %%ebx\n"
	"popl %%eax\n"
	
	"femms\n"
    : "=S" (buf), "=a" (data_ptr), "=c" (delay_prt), "=d" (window_prt)
    : "S" (buf), "a" (data_ptr), "c" (delay_prt), "d" (window_prt));
}

