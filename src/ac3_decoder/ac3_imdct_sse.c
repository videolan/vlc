/*****************************************************************************
 * ac3_imdct_sse.c: ac3 DCT
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_imdct_sse.c,v 1.1 2001/05/14 15:58:04 reno Exp $
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

#include "defs.h"

#include <math.h>
#include <stdio.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "stream_control.h"
#include "input_ext-dec.h"

#include "ac3_decoder.h"

#include "ac3_imdct_sse.h"

static const float window[] = {
	0.00014, 0.00024, 0.00037, 0.00051, 0.00067, 0.00086, 0.00107, 0.00130,
	0.00157, 0.00187, 0.00220, 0.00256, 0.00297, 0.00341, 0.00390, 0.00443,
	0.00501, 0.00564, 0.00632, 0.00706, 0.00785, 0.00871, 0.00962, 0.01061,
	0.01166, 0.01279, 0.01399, 0.01526, 0.01662, 0.01806, 0.01959, 0.02121,
	0.02292, 0.02472, 0.02662, 0.02863, 0.03073, 0.03294, 0.03527, 0.03770,
	0.04025, 0.04292, 0.04571, 0.04862, 0.05165, 0.05481, 0.05810, 0.06153,
	0.06508, 0.06878, 0.07261, 0.07658, 0.08069, 0.08495, 0.08935, 0.09389,
	0.09859, 0.10343, 0.10842, 0.11356, 0.11885, 0.12429, 0.12988, 0.13563,
	0.14152, 0.14757, 0.15376, 0.16011, 0.16661, 0.17325, 0.18005, 0.18699,
	0.19407, 0.20130, 0.20867, 0.21618, 0.22382, 0.23161, 0.23952, 0.24757,
	0.25574, 0.26404, 0.27246, 0.28100, 0.28965, 0.29841, 0.30729, 0.31626,
	0.32533, 0.33450, 0.34376, 0.35311, 0.36253, 0.37204, 0.38161, 0.39126,
	0.40096, 0.41072, 0.42054, 0.43040, 0.44030, 0.45023, 0.46020, 0.47019,
	0.48020, 0.49022, 0.50025, 0.51028, 0.52031, 0.53033, 0.54033, 0.55031,
	0.56026, 0.57019, 0.58007, 0.58991, 0.59970, 0.60944, 0.61912, 0.62873,
	0.63827, 0.64774, 0.65713, 0.66643, 0.67564, 0.68476, 0.69377, 0.70269,
	0.71150, 0.72019, 0.72877, 0.73723, 0.74557, 0.75378, 0.76186, 0.76981,
	0.77762, 0.78530, 0.79283, 0.80022, 0.80747, 0.81457, 0.82151, 0.82831,
	0.83496, 0.84145, 0.84779, 0.85398, 0.86001, 0.86588, 0.87160, 0.87716,
	0.88257, 0.88782, 0.89291, 0.89785, 0.90264, 0.90728, 0.91176, 0.91610,
	0.92028, 0.92432, 0.92822, 0.93197, 0.93558, 0.93906, 0.94240, 0.94560,
	0.94867, 0.95162, 0.95444, 0.95713, 0.95971, 0.96217, 0.96451, 0.96674,
	0.96887, 0.97089, 0.97281, 0.97463, 0.97635, 0.97799, 0.97953, 0.98099,
	0.98236, 0.98366, 0.98488, 0.98602, 0.98710, 0.98811, 0.98905, 0.98994,
	0.99076, 0.99153, 0.99225, 0.99291, 0.99353, 0.99411, 0.99464, 0.99513,
	0.99558, 0.99600, 0.99639, 0.99674, 0.99706, 0.99736, 0.99763, 0.99788,
	0.99811, 0.99831, 0.99850, 0.99867, 0.99882, 0.99895, 0.99908, 0.99919,
	0.99929, 0.99938, 0.99946, 0.99953, 0.99959, 0.99965, 0.99969, 0.99974,
	0.99978, 0.99981, 0.99984, 0.99986, 0.99988, 0.99990, 0.99992, 0.99993,
	0.99994, 0.99995, 0.99996, 0.99997, 0.99998, 0.99998, 0.99998, 0.99999,
	0.99999, 0.99999, 0.99999, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000,
	1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000
};

static const int pm128[128] =
{
	0, 16, 32, 48, 64, 80,  96, 112,  8, 40, 72, 104, 24, 56,  88, 120,
	4, 20, 36, 52, 68, 84, 100, 116, 12, 28, 44,  60, 76, 92, 108, 124,
	2, 18, 34, 50, 66, 82,  98, 114, 10, 42, 74, 106, 26, 58,  90, 122,
	6, 22, 38, 54, 70, 86, 102, 118, 14, 46, 78, 110, 30, 62,  94, 126,
	1, 17, 33, 49, 65, 81,  97, 113,  9, 41, 73, 105, 25, 57,  89, 121,
	5, 21, 37, 53, 69, 85, 101, 117, 13, 29, 45,  61, 77, 93, 109, 125,
	3, 19, 35, 51, 67, 83,  99, 115, 11, 43, 75, 107, 27, 59,  91, 123,
	7, 23, 39, 55, 71, 87, 103, 119, 15, 31, 47,  63, 79, 95, 111, 127
}; 

void fft_64p_sse (complex_t *x);
void fft_128p_sse(complex_t *a);
static void imdct512_pre_ifft_twiddle_sse (const int *pmt, complex_t *buf, float *data, float *xcos_sin_sse);
static void imdct512_post_ifft_twiddle_sse (complex_t *buf, float *xcos_sin_sse);
static void imdct512_window_delay_sse (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt);
static void imdct512_window_delay_nol_sse (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt);


int imdct_init_sse (imdct_t * p_imdct)
{
	int i;
	float scale = 181.019;

	intf_WarnMsg (1, "ac3dec: using MMX_SSE for imdct");
	p_imdct->imdct_do_512 = imdct_do_512_sse;
	p_imdct->imdct_do_512_nol = imdct_do_512_nol_sse;
	p_imdct->fft_64p = fft_64p_sse;

	for (i=0; i < 128; i++)
	{
		float xcos_i = cos(2.0f * M_PI * (8*i+1)/(8*N)) * scale;
		float xsin_i = sin(2.0f * M_PI * (8*i+1)/(8*N)) * scale;
		p_imdct->xcos_sin_sse[i * 4]     = xcos_i;
		p_imdct->xcos_sin_sse[i * 4 + 1] = -xsin_i;
		p_imdct->xcos_sin_sse[i * 4 + 2] = -xsin_i;
		p_imdct->xcos_sin_sse[i * 4 + 3] = -xcos_i;
	}
	return 0;
}

void imdct_do_512_sse (imdct_t * p_imdct, float data[], float delay[])
{
	imdct512_pre_ifft_twiddle_sse (pm128, p_imdct->buf, data, p_imdct->xcos_sin_sse);
	fft_128p_sse (p_imdct->buf);
	imdct512_post_ifft_twiddle_sse (p_imdct->buf, p_imdct->xcos_sin_sse);
    imdct512_window_delay_sse (p_imdct->buf, data, window, delay);
}


void imdct_do_512_nol_sse (imdct_t * p_imdct, float data[], float delay[])
{
	imdct512_pre_ifft_twiddle_sse (pm128, p_imdct->buf, data, p_imdct->xcos_sin_sse);  
	fft_128p_sse (p_imdct->buf);
	imdct512_post_ifft_twiddle_sse (p_imdct->buf, p_imdct->xcos_sin_sse);
    imdct512_window_delay_nol_sse (p_imdct->buf, data, window, delay);
}

static void imdct512_pre_ifft_twiddle_sse (const int *pmt, complex_t *buf, float *data, float *xcos_sin_sse)
{
    __asm__ __volatile__ (	
	"pushl %%ebp\n"
	"movl  %%esp, %%ebp\n"
	"addl  $-4, %%esp\n" /* local variable, loop counter */
	
	"pushl %%eax\n"
	"pushl %%ebx\n"
	"pushl %%ecx\n"
	"pushl %%edx\n"
	"pushl %%edi\n"
	"pushl %%esi\n"

	"movl  8(%%ebp), %%eax\n" 	/* pmt */
	"movl 12(%%ebp), %%ebx\n"	/* buf */
	"movl 16(%%ebp), %%ecx\n"	/* data */
	"movl 20(%%ebp), %%edx\n" 	/* xcos_sin_sse */
	"movl $64, -4(%%ebp)\n"
	
".loop:\n"
	"movl  (%%eax), %%esi\n"
	"movl 4(%%eax), %%edi\n"
	"movss (%%ecx, %%esi, 8), %%xmm1\n" /* 2j */
	"movss (%%ecx, %%edi, 8), %%xmm3\n" /* 2(j+1) */

	"shll $1, %%esi\n"
	"shll $1, %%edi\n"

	"movups (%%edx, %%esi, 8), %%xmm0\n" /* -c_j | -s_j | -s_j | c_j */
	"movups (%%edx, %%edi, 8), %%xmm2\n" /* -c_j+1 | -s_j+1 | -s_j+1 | c_j+1 */

	"negl %%esi\n"
	"negl %%edi\n"

	"movss 1020(%%ecx, %%esi, 4), %%xmm4\n" /* 255-2j */
	"addl $8, %%eax\n"
	"movss 1020(%%ecx, %%edi, 4), %%xmm5\n" /* 255-2(j+1) */

	"shufps $0, %%xmm1, %%xmm4\n" /* 2j | 2j | 255-2j | 255-2j */
	"shufps $0, %%xmm3, %%xmm5\n" /* 2(j+1) | 2(j+1) | 255-2(j+1) | 255-2(j+1) */
	"mulps   %%xmm4, %%xmm0\n"
	"mulps   %%xmm5, %%xmm2\n"
	"movhlps %%xmm0, %%xmm1\n"
	"movhlps %%xmm2, %%xmm3\n"
	"addl    $16, %%ebx\n"
	"addps   %%xmm1, %%xmm0\n"
	"addps   %%xmm3, %%xmm2\n"
	"movlhps %%xmm2, %%xmm0\n"
    
	"movups  %%xmm0, -16(%%ebx)\n"
	"decl -4(%%ebp)\n"
   	"jnz .loop\n"

	"popl %%esi\n"
	"popl %%edi\n"
	"popl %%edx\n"
	"popl %%ecx\n"
	"popl %%ebx\n"
	"popl %%eax\n"

	"addl $4, %%esp\n"
	"popl %%ebp\n"
    ::);
}

static void imdct512_post_ifft_twiddle_sse (complex_t *buf, float *xcos_sin_sse)
{
    __asm__ __volatile__ ( 
	"pushl %%ecx\n"
	"movl $32, %%ecx\n"                 /* loop counter */

".loop1:\n"
	"movups	(%%eax), %%xmm0\n"          /*  im1 | re1 | im0 | re0 */

	"movups  (%%ebx), %%xmm2\n"         /* -c | -s | -s | c */
	"movhlps  %%xmm0, %%xmm1\n"         /* im1 | re1 */
	"movups  16(%%ebx), %%xmm3\n"       /* -c1 | -s1 | -s1 | c1 */

	"shufps $0x50, %%xmm0, %%xmm0\n"    /* im0 | im0 | re0 | re0 */
	"shufps $0x50, %%xmm1, %%xmm1\n"    /* im1 | im1 | re1 | re1 */

	"movups  16(%%eax), %%xmm4\n"       /* im3 | re3 | im2 | re2 */

    "shufps $0x27, %%xmm2, %%xmm2\n"    /* c | -s | -s | -c */
	"movhlps  %%xmm4, %%xmm5\n"         /* im3 | re3 */
    "shufps $0x27, %%xmm3, %%xmm3\n"    /* c1 | -s1 | -s1 | -c1 */

	"movups  32(%%ebx), %%xmm6\n"       /* -c2 | -s2 | -s2 | c2 */
	"movups  48(%%ebx), %%xmm7\n"       /* -c3 | -s3 | -s3 | c3 */

	"shufps $0x50, %%xmm4, %%xmm4\n"    /* im2 | im2 | re2 | re2 */
	"shufps $0x50, %%xmm5, %%xmm5\n"    /* im3 | im3 | re3 | re3 */

	"mulps %%xmm2, %%xmm0\n"
	"mulps %%xmm3, %%xmm1\n"

	"shufps $0x27, %%xmm6, %%xmm6\n"    /* c2 | -s2 | -s2 | -c2 */
	"shufps $0x27, %%xmm7, %%xmm7\n"    /* c3 | -s3 | -s3 | -c3 */

	"movhlps %%xmm0, %%xmm2\n"
	"movhlps %%xmm1, %%xmm3\n"

	"mulps %%xmm6, %%xmm4\n"
	"mulps %%xmm7, %%xmm5\n"

	"addps %%xmm2, %%xmm0\n"
	"addps %%xmm3, %%xmm1\n"

	"movhlps %%xmm4, %%xmm6\n"
	"movhlps %%xmm5, %%xmm7\n"

	"addps %%xmm6, %%xmm4\n"
	"addps %%xmm7, %%xmm5\n"

	"movlhps %%xmm1, %%xmm0\n"
	"movlhps %%xmm5, %%xmm4\n"

	"movups %%xmm0, (%%eax)\n"
	"movups %%xmm4, 16(%%eax)\n"
	"addl $64, %%ebx\n"
	"addl $32, %%eax\n"
	"decl %%ecx\n"
	"jnz .loop1\n"

	"popl %%ecx\n"
    : "=a" (buf)
    : "a" (buf), "b" (xcos_sin_sse) );
}

static void imdct512_window_delay_sse (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt)
{
    __asm__ __volatile__ (
	"pushl %%ebp\n"
	"movl  %%esp, %%ebp\n"
	
	"pushl %%eax\n"
	"pushl %%ebx\n"
	"pushl %%ecx\n"
	"pushl %%edx\n"
	"pushl %%esi\n"
	"pushl %%edi\n"

	"movl 20(%%ebp), %%ebx\n"   /* delay */
	"movl 16(%%ebp), %%edx\n"   /* window */

	"movl 8(%%ebp), %%eax\n"    /* buf */
	"movl $16, %%ecx\n"         /* loop count */
	"leal 516(%%eax), %%esi\n"  /* buf[64].im */
	"leal 504(%%eax), %%edi\n"  /* buf[63].re */
	"movl  12(%%ebp), %%eax\n"  /* data */

".first_128_samples:\n"
	"movss   (%%esi), %%xmm0\n"
	"movss  8(%%esi), %%xmm2\n"
	"movss   (%%edi), %%xmm1\n"
	"movss -8(%%edi), %%xmm3\n"

	"movlhps %%xmm2, %%xmm0\n"      /* 0.0 | im1 | 0.0 | im0 */
	"movlhps %%xmm3, %%xmm1\n"      /* 0.0 | re1 | 0.0 | re0 */

	"movups (%%edx), %%xmm4\n"      /* w3 | w2 | w1 | w0 */
	"movups (%%ebx), %%xmm5\n"      /* d3 | d2 | d1 | d0 */
	"shufps $0xb1, %%xmm1, %%xmm1\n"/* re1 | 0.0 | re0 | 0.0 */

	"movss  16(%%esi), %%xmm6\n"    /* im2 */
	"movss  24(%%esi), %%xmm7\n"    /* im3 */
	"subps     %%xmm1, %%xmm0\n"    /* -re1 | im1 | -re0 | im0 */
	"movss -16(%%edi), %%xmm2\n"    /* re2 */
	"movss -24(%%edi), %%xmm3\n"    /* re3 */
	"mulps     %%xmm4, %%xmm0\n"
	"movlhps   %%xmm7, %%xmm6\n"    /* 0.0 | im3 | 0.0 | im2 */
	"movlhps   %%xmm3, %%xmm2\n"    /* 0.0 | re3 | 0.0 | re2 */
	"addps %%xmm5, %%xmm0\n"
	"shufps $0xb1, %%xmm2, %%xmm2\n"/* re3 | 0.0 | re2 | 0.0 */
	"movups 16(%%edx), %%xmm4\n"    /* w7 | w6 | w5 | w4 */
	"movups 16(%%ebx), %%xmm5\n"    /* d7 | d6 | d5 | d4 */
	"subps %%xmm2, %%xmm6\n"        /* -re3 | im3 | -re2 | im2 */
	"addl $32, %%edx\n"
	"movups %%xmm0, (%%eax)\n"
	"addl $32, %%ebx\n"
	"mulps %%xmm4, %%xmm6\n"
	"addl $32, %%esi\n"
	"addl $32, %%eax\n"
	"addps %%xmm5, %%xmm6\n"
    "addl $-32, %%edi\n"
	"movups %%xmm6, -16(%%eax)\n"
	"decl %%ecx\n"
	"jnz .first_128_samples\n"

	"movl 8(%%ebp), %%esi\n"    /* buf[0].re */
	"leal 1020(%%esi), %%edi\n" /* buf[127].im */
	"movl $16, %%ecx\n"         /* loop count */
    
".second_128_samples:\n"
	"movss   (%%esi), %%xmm0\n" /* buf[i].re */
	"movss  8(%%esi), %%xmm2\n" /* re1 */
	"movss   (%%edi), %%xmm1\n" /* buf[127-i].im */
	"movss -8(%%edi), %%xmm3\n" /* im1 */

	"movlhps %%xmm2, %%xmm0\n"  /* 0.0 | re1 | 0.0 | re0 */
	"movlhps %%xmm3, %%xmm1\n"  /* 0.0 | im1 | 0.0 | im1 */

	"movups (%%edx), %%xmm4\n"  /* w3 | w2 | w1 | w0 */
	"movups (%%ebx), %%xmm5\n"  /* d3 | d2 | d1 | d0 */

	"shufps $0xb1, %%xmm1, %%xmm1\n"/* im1 | 0.0 | im0 | 0.0 */
	"movss  16(%%esi), %%xmm6\n"    /* re2 */
	"movss  24(%%esi), %%xmm7\n"    /* re3 */
	"movss -16(%%edi), %%xmm2\n"    /* im2 */
	"movss -24(%%edi), %%xmm3\n"    /* im3 */
	"subps   %%xmm1, %%xmm0\n"      /* -im1 | re1 | -im0 | re0 */
	"movlhps %%xmm7, %%xmm6\n"      /* 0.0 | re3 | 0.0 | re2 */
	"movlhps %%xmm3, %%xmm2\n"      /* 0.0 | im3 | 0.0 | im2 */
	"mulps   %%xmm4, %%xmm0\n"
	"shufps $0xb1, %%xmm2, %%xmm2\n"/* im3 | 0.0 | im2 | 0.0 */
	"movups 16(%%edx), %%xmm4\n"    /* w7 | w6 | w5 | w4 */
	"addl $32, %%esi\n"
	"subps %%xmm2, %%xmm6\n"        /* -im3 | re3 | -im2 | re2 */
	"addps %%xmm5, %%xmm0\n"
	"mulps %%xmm4, %%xmm6\n"
	"addl $-32, %%edi\n"
	"movups 16(%%ebx), %%xmm5\n"    /* d7 | d6 | d5 | d4 */
	"movups %%xmm0, (%%eax)\n"
	"addps %%xmm5, %%xmm6\n"
	"addl $32, %%edx\n"
	"addl $32, %%eax\n"
	"addl $32, %%ebx\n"
	"movups %%xmm6, -16(%%eax)\n"
	"decl %%ecx\n"
	"jnz .second_128_samples\n"

	"movl   8(%%ebp), %%eax\n"
	"leal 512(%%eax), %%esi\n"  /* buf[64].re */
	"leal 508(%%eax), %%edi\n"  /* buf[63].im */
	"movl $16, %%ecx\n"         /* loop count */
	"movl  20(%%ebp), %%eax\n"  /* delay */

".first_128_delay:\n"
	"movss   (%%esi), %%xmm0\n"
	"movss  8(%%esi), %%xmm2\n"
	"movss   (%%edi), %%xmm1\n"
	"movss -8(%%edi), %%xmm3\n"

	"movlhps %%xmm2, %%xmm0\n"      /* 0.0 | re1 | 0.0 | re0 */
	"movlhps %%xmm3, %%xmm1\n"      /* 0.0 | im1 | 0.0 | im0 */

	"movups -16(%%edx), %%xmm4\n"   /* w3 | w2 | w1 | w0 */
    "shufps $0xb1, %%xmm1, %%xmm1\n"/* im1 | 0.0 | im0 | 0.0 */
	"movss  16(%%esi), %%xmm6\n"    /* re2 */
	"movss  24(%%esi), %%xmm7\n"    /* re3 */
	"movss -16(%%edi), %%xmm2\n"    /* im2 */
	"movss -24(%%edi), %%xmm3\n"    /* im3 */
	"subps     %%xmm1, %%xmm0\n"    /* -im1 | re1 | -im0 | re0 */
	"addl $-32, %%edx\n"
	"movlhps %%xmm7, %%xmm6\n"      /* 0.0 | re3 | 0.0 | re2 */
	"movlhps %%xmm3, %%xmm2\n"      /* 0.0 | im3 | 0.0 | im2 */
    "mulps   %%xmm4, %%xmm0\n"
	"movups (%%edx), %%xmm5\n"      /* w7 | w6 | w5 | w4 */
	"shufps $0xb1, %%xmm2, %%xmm2\n"/* im3 | 0.0 | im2 | 0.0 */
	"movups %%xmm0, (%%eax)\n"
	"addl $32, %%esi\n"
	"subps %%xmm2, %%xmm6\n"        /* -im3 | re3 | -im2 | re2 */
	"addl $-32, %%edi\n"
	"mulps %%xmm5, %%xmm6\n"
	"addl $32, %%eax\n"
	"movups %%xmm6, -16(%%eax)\n"
	"decl %%ecx\n"
	"jnz .first_128_delay\n"

	"movl    8(%%ebp), %%ebx\n"
	"leal    4(%%ebx), %%esi\n" /* buf[0].im */
	"leal 1016(%%ebx), %%edi\n" /* buf[127].re */
	"movl $16, %%ecx\n"         /* loop count */
    
".second_128_delay:\n"
	"movss   (%%esi), %%xmm0\n"
	"movss  8(%%esi), %%xmm2\n"
	"movss   (%%edi), %%xmm1\n"
	"movss -8(%%edi), %%xmm3\n"

	"movlhps %%xmm2, %%xmm0\n"      /* 0.0 | im1 | 0.0 | im0 */
	"movlhps %%xmm3, %%xmm1\n"      /* 0.0 | re1 | 0.0 | re0 */

	"movups -16(%%edx), %%xmm4\n"   /* w3 | w2 | w1 | w0 */
	"shufps $0xb1, %%xmm1, %%xmm1\n"/* re1 | 0.0 | re0 | 0.0 */
	"movss  16(%%esi), %%xmm6\n"    /* im2 */
	"movss  24(%%esi), %%xmm7\n"    /* im3 */
	"movss -16(%%edi), %%xmm2\n"    /* re2 */
	"movss -24(%%edi), %%xmm3\n"    /* re3 */
	"subps %%xmm0, %%xmm1\n"        /* re1 | -im1 | re0 | -im0 */
	"addl $-32, %%edx\n"
	"movlhps %%xmm7, %%xmm6\n"      /* 0.0 | im3 | 0.0 | im2 */
	"movlhps %%xmm3, %%xmm2\n"      /* 0.0 | re3 | 0.0 | re2 */
	"mulps   %%xmm4, %%xmm1\n"
	"movups (%%edx), %%xmm5\n"      /* w7 | w6 | w5 | w4 */
	"shufps $0xb1, %%xmm2, %%xmm2\n"/* re3 | 0.0 | re2 | 0.0 */
	"movups %%xmm1, (%%eax)\n"
	"addl $32, %%esi\n"
	"subps %%xmm6, %%xmm2\n"        /* re | -im3 | re | -im2 */
	"addl $-32, %%edi\n"
	"mulps %%xmm5, %%xmm2\n"
	"addl $32, %%eax\n"
	"movups %%xmm2, -16(%%eax)\n"
	"decl %%ecx\n"
	"jnz .second_128_delay\n"

	"popl %%edi\n"
	"popl %%esi\n"
	"popl %%edx\n"
	"popl %%ecx\n"
	"popl %%ebx\n"
	"popl %%eax\n"
	
	"leave\n"
    ::);
}

static void imdct512_window_delay_nol_sse (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt)
{
    __asm__ __volatile__ (
	"pushl %%ebp\n"
	"movl  %%esp, %%ebp\n"
	
	"pushl %%eax\n"
	"pushl %%ebx\n"
	"pushl %%ecx\n"
	"pushl %%edx\n"
	"pushl %%esi\n"
	"pushl %%edi\n"

	/* movl 20(%%ebp), %%ebx delay */
	"movl 16(%%ebp), %%edx\n"   /* window */

	"movl   8(%%ebp), %%eax\n"  /* buf */
	"movl $16, %%ecx\n"         /* loop count */
	"leal 516(%%eax), %%esi\n"  /* buf[64].im */
	"leal 504(%%eax), %%edi\n"  /* buf[63].re */
	"movl  12(%%ebp), %%eax\n"  /* data */
    
".first_128_sample:\n"
	"movss   (%%esi), %%xmm0\n"
	"movss  8(%%esi), %%xmm2\n"
	"movss   (%%edi), %%xmm1\n"
	"movss -8(%%edi), %%xmm3\n"

	"movlhps %%xmm2, %%xmm0\n"      /* 0.0 | im1 | 0.0 | im0 */
	"movlhps %%xmm3, %%xmm1\n"      /* 0.0 | re1 | 0.0 | re0 */

	"movups (%%edx), %%xmm4\n"      /* w3 | w2 | w1 | w0 */
    /* movups (%%ebx), %%xmm5 d3 | d2 | d1 | d0 */
	"shufps $0xb1, %%xmm1, %%xmm1\n"/* re1 | 0.0 | re0 | 0.0 */

	"movss  16(%%esi), %%xmm6\n"    /* im2 */
	"movss  24(%%esi), %%xmm7\n"    /* im3 */
	"subps     %%xmm1, %%xmm0\n"    /* -re1 | im1 | -re0 | im0 */
	"movss -16(%%edi), %%xmm2\n"    /* re2 */
	"movss -24(%%edi), %%xmm3\n"    /* re3 */
	"mulps %%xmm4, %%xmm0\n"
	"movlhps %%xmm7, %%xmm6\n"      /* 0.0 | im3 | 0.0 | im2 */
	"movlhps %%xmm3, %%xmm2\n"      /* 0.0 | re3 | 0.0 | re2 */
	/* addps %%xmm5, %%xmm0 */
	"shufps $0xb1, %%xmm2, %%xmm2\n"/* re3 | 0.0 | re2 | 0.0 */
	"movups 16(%%edx), %%xmm4\n"    /* w7 | w6 | w5 | w4 */
	/* movups 16(%%ebx), %%xmm5  d7 | d6 | d5 | d4 */
	"subps %%xmm2, %%xmm6\n"        /* -re3 | im3 | -re2 | im2 */
    "addl $32, %%edx\n"
	"movups %%xmm0, (%%eax)\n"
	/* addl $32, %%ebx */
	"mulps %%xmm4, %%xmm6\n"
	"addl $32, %%esi\n"
	"addl $32, %%eax\n"
	/* addps %%xmm5, %%xmm6 */
	"addl $-32, %%edi\n"
	"movups %%xmm6, -16(%%eax)\n"
	"decl %%ecx\n"
	"jnz .first_128_sample\n"

	"movl    8(%%ebp), %%esi\n"     /* buf[0].re */
	"leal 1020(%%esi), %%edi\n"     /* buf[127].im */
	"movl $16, %%ecx\n"             /* loop count */
    
".second_128_sample:\n"
	"movss   (%%esi), %%xmm0\n"     /* buf[i].re */
	"movss  8(%%esi), %%xmm2\n"     /* re1 */
	"movss   (%%edi), %%xmm1\n"     /* buf[127-i].im */
	"movss -8(%%edi), %%xmm3\n"     /* im1 */

	"movlhps %%xmm2, %%xmm0\n"      /* 0.0 | re1 | 0.0 | re0 */
	"movlhps %%xmm3, %%xmm1\n"      /* 0.0 | im1 | 0.0 | im1 */
	
	"movups (%%edx), %%xmm4\n"      /* w3 | w2 | w1 | w0 */
	/* movups (%%ebx), %%xmm5 d3 | d2 | d1 | d0 */

	"shufps $0xb1, %%xmm1, %%xmm1\n"/* im1 | 0.0 | im0 | 0.0 */
	"movss  16(%%esi), %%xmm6\n"    /* re2 */
	"movss  24(%%esi), %%xmm7\n"    /* re3 */
	"movss -16(%%edi), %%xmm2\n"    /* im2 */
	"movss -24(%%edi), %%xmm3\n"    /* im3 */
	"subps %%xmm1, %%xmm0\n"        /* -im1 | re1 | -im0 | re0 */
	"movlhps %%xmm7, %%xmm6\n"      /* 0.0 | re3 | 0.0 | re2 */
	"movlhps %%xmm3, %%xmm2\n"      /* 0.0 | im3 | 0.0 | im2 */
	"mulps %%xmm4, %%xmm0\n"
	"shufps $0xb1, %%xmm2, %%xmm2\n"/* im3 | 0.0 | im2 | 0.0 */
	"movups 16(%%edx), %%xmm4\n"    /* w7 | w6 | w5 | w4 */
	"addl $32, %%esi\n"
	"subps %%xmm2, %%xmm6\n"        /* -im3 | re3 | -im2 | re2 */
	/* addps %%xmm5, %%xmm0 */
	"mulps %%xmm4, %%xmm6\n"
	"addl $-32, %%edi\n"
	/* movups 16(%%ebx), %%xmm5  d7 | d6 | d5 | d4 */
	"movups %%xmm0, (%%eax)\n"
	/* addps %%xmm5, %%xmm6 */
	"addl $32, %%edx\n"
	"addl $32, %%eax\n"
	/* addl $32, %%ebx */
	"movups %%xmm6, -16(%%eax)\n"
	"decl %%ecx\n"
	"jnz .second_128_sample\n"

	"movl   8(%%ebp), %%eax\n"
	"leal 512(%%eax), %%esi\n"  /* buf[64].re */
	"leal 508(%%eax), %%edi\n"  /* buf[63].im */
	"movl $16, %%ecx\n"         /* loop count */
	"movl  20(%%ebp), %%eax\n"  /* delay */
    
".first_128_delays:\n"
	"movss   (%%esi), %%xmm0\n"
	"movss  8(%%esi), %%xmm2\n"
	"movss   (%%edi), %%xmm1\n"
	"movss -8(%%edi), %%xmm3\n"

	"movlhps %%xmm2, %%xmm0\n"  /* 0.0 | re1 | 0.0 | re0 */
	"movlhps %%xmm3, %%xmm1\n"  /* 0.0 | im1 | 0.0 | im0 */

	"movups -16(%%edx), %%xmm4\n"   /* w3 | w2 | w1 | w0 */
	"shufps $0xb1, %%xmm1, %%xmm1\n"/* im1 | 0.0 | im0 | 0.0 */
	"movss  16(%%esi), %%xmm6\n"    /* re2 */
	"movss  24(%%esi), %%xmm7\n"    /* re3 */
	"movss -16(%%edi), %%xmm2\n"    /* im2 */
	"movss -24(%%edi), %%xmm3\n"    /* im3 */
	"subps %%xmm1, %%xmm0\n"        /* -im1 | re1 | -im0 | re0 */
	"addl $-32, %%edx\n"
	"movlhps %%xmm7, %%xmm6\n"      /* 0.0 | re3 | 0.0 | re2 */
	"movlhps %%xmm3, %%xmm2\n"      /* 0.0 | im3 | 0.0 | im2 */
	"mulps %%xmm4, %%xmm0\n"
	"movups (%%edx), %%xmm5\n"      /* w7 | w6 | w5 | w4 */
	"shufps $0xb1, %%xmm2, %%xmm2\n"/* im3 | 0.0 | im2 | 0.0 */
	"movups %%xmm0, (%%eax)\n"
	"addl $32, %%esi\n"
	"subps %%xmm2, %%xmm6\n"        /* -im3 | re3 | -im2 | re2 */
	"addl $-32, %%edi\n"
	"mulps %%xmm5, %%xmm6\n"
	"addl $32, %%eax\n"
	"movups %%xmm6, -16(%%eax)\n"
	"decl %%ecx\n"
	"jnz .first_128_delays\n"

	"movl    8(%%ebp), %%ebx\n"
	"leal    4(%%ebx), %%esi\n" /* buf[0].im */
	"leal 1016(%%ebx), %%edi\n" /* buf[127].re */
	"movl $16, %%ecx\n"         /* loop count */
    
".second_128_delays:\n"
	"movss   (%%esi), %%xmm0\n"
	"movss  8(%%esi), %%xmm2\n"
	"movss   (%%edi), %%xmm1\n"
	"movss -8(%%edi), %%xmm3\n"

	"movlhps %%xmm2, %%xmm0\n"  /* 0.0 | im1 | 0.0 | im0 */
	"movlhps %%xmm3, %%xmm1\n"  /* 0.0 | re1 | 0.0 | re0 */

	"movups -16(%%edx), %%xmm4\n"   /* w3 | w2 | w1 | w0 */
	"shufps $0xb1, %%xmm1, %%xmm1\n"/* re1 | 0.0 | re0 | 0.0 */
	"movss  16(%%esi), %%xmm6\n"    /* im2 */
	"movss  24(%%esi), %%xmm7\n"    /* im3 */
	"movss -16(%%edi), %%xmm2\n"    /* re2 */
	"movss -24(%%edi), %%xmm3\n"    /* re3 */
	"subps %%xmm0, %%xmm1\n"        /* re1 | -im1 | re0 | -im0 */
	"addl $-32, %%edx\n"
	"movlhps %%xmm7, %%xmm6\n"      /* 0.0 | im3 | 0.0 | im2 */
	"movlhps %%xmm3, %%xmm2\n"      /* 0.0 | re3 | 0.0 | re2 */
	"mulps %%xmm4, %%xmm1\n"
	"movups (%%edx), %%xmm5\n"      /* w7 | w6 | w5 | w4 */
	"shufps $0xb1, %%xmm2, %%xmm2\n"/* re3 | 0.0 | re2 | 0.0 */
	"movups %%xmm1, (%%eax)\n"
	"addl $32, %%esi\n"
	"subps %%xmm6, %%xmm2\n"        /* re | -im3 | re | -im2 */
	"addl $-32, %%edi\n"
	"mulps %%xmm5, %%xmm2\n"
	"addl $32, %%eax\n"
	"movups %%xmm2, -16(%%eax)\n"
	"decl %%ecx\n"
	"jnz .second_128_delays\n"

	"popl %%edi\n"
	"popl %%esi\n"
	"popl %%edx\n"
	"popl %%ecx\n"
	"popl %%ebx\n"
	"popl %%eax\n"
	
	"leave\n"
    ::);
}
