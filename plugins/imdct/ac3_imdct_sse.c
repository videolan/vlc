/*****************************************************************************
 * ac3_imdct_sse.c: accelerated SSE ac3 DCT
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_imdct_sse.c,v 1.10 2001/12/30 07:09:55 sam Exp $
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
#include "ac3_imdct_common.h"
#include "ac3_retables.h"

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

void _M( fft_64p )  ( complex_t *x );
void _M( fft_128p ) ( complex_t *a );

static void imdct512_pre_ifft_twiddle_sse  ( const int *, complex_t *,
                                             float *, float * );
static void imdct512_post_ifft_twiddle_sse ( complex_t *, float * );
static void imdct512_window_delay_sse      ( complex_t *, float *,
                                             float *, float * );
static void imdct512_window_delay_nol_sse  ( complex_t *, float *,
                                             float *, float * );

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
    imdct512_pre_ifft_twiddle_sse( pm128, p_imdct->buf, data,
                                   p_imdct->xcos_sin_sse );
    _M( fft_128p ) ( p_imdct->buf );
    imdct512_post_ifft_twiddle_sse( p_imdct->buf, p_imdct->xcos_sin_sse );
    imdct512_window_delay_sse( p_imdct->buf, data, window, delay );
}


void _M( imdct_do_512_nol ) (imdct_t * p_imdct, float data[], float delay[])
{
    imdct512_pre_ifft_twiddle_sse( pm128, p_imdct->buf, data,
                                   p_imdct->xcos_sin_sse );
    _M( fft_128p ) ( p_imdct->buf );
    imdct512_post_ifft_twiddle_sse( p_imdct->buf, p_imdct->xcos_sin_sse );
    imdct512_window_delay_nol_sse( p_imdct->buf, data, window, delay );
}

static void imdct512_pre_ifft_twiddle_sse (const int *pmt, complex_t *buf, float *data, float *xcos_sin_sse)
{
    __asm__ __volatile__ (    
    ".align 16\n"
    "pushl %%ebp\n"
    "movl  %%esp, %%ebp\n"
    "addl  $-4, %%esp\n" /* local variable, loop counter */
    
    "pushl %%eax\n"
    "pushl %%ebx\n"
    "pushl %%ecx\n"
    "pushl %%edx\n"
    "pushl %%edi\n"
    "pushl %%esi\n"

    "movl %%edi, %%ebx\n"   /* buf */
    "movl $64, -4(%%ebp)\n"
    
    ".align 16\n"
".loop:\n"
    "movl  (%%eax), %%esi\n"
    "movl 4(%%eax), %%edi\n"
    "movss (%%ecx, %%esi, 8), %%xmm1\n" /* 2j */
    "movss (%%ecx, %%edi, 8), %%xmm3\n" /* 2(j+1) */

    "shll $1, %%esi\n"
    "shll $1, %%edi\n"

    "movaps (%%edx, %%esi, 8), %%xmm0\n" /* -c_j | -s_j | -s_j | c_j */
    "movaps (%%edx, %%edi, 8), %%xmm2\n" /* -c_j+1 | -s_j+1 | -s_j+1 | c_j+1 */

    "negl %%esi\n"
    "negl %%edi\n"

    "movss 1020(%%ecx, %%esi, 4), %%xmm4\n" /* 255-2j */
    "addl  $8, %%eax\n"
    "movss 1020(%%ecx, %%edi, 4), %%xmm5\n" /* 255-2(j+1) */

    "shufps  $0, %%xmm1, %%xmm4\n" /* 2j | 2j | 255-2j | 255-2j */
    "shufps  $0, %%xmm3, %%xmm5\n" /* 2(j+1) | 2(j+1) | 255-2(j+1) | 255-2(j+1) */
    "mulps   %%xmm4, %%xmm0\n"
    "mulps   %%xmm5, %%xmm2\n"
    "movhlps %%xmm0, %%xmm1\n"
    "movhlps %%xmm2, %%xmm3\n"
    "addl    $16, %%ebx\n"
    "addps   %%xmm1, %%xmm0\n"
    "addps   %%xmm3, %%xmm2\n"
    "movlhps %%xmm2, %%xmm0\n"
    
    "movaps  %%xmm0, -16(%%ebx)\n"
    "decl    -4(%%ebp)\n"
    "jnz     .loop\n"

    "popl %%esi\n"
    "popl %%edi\n"
    "popl %%edx\n"
    "popl %%ecx\n"
    "popl %%ebx\n"
    "popl %%eax\n"

    "addl $4, %%esp\n"
    "popl %%ebp\n"
    : "=D" (buf)
    : "a" (pmt), "c" (data), "d" (xcos_sin_sse), "D" (buf));
    
}

static void imdct512_post_ifft_twiddle_sse (complex_t *buf, float *xcos_sin_sse)
{
    __asm__ __volatile__ ( 
    ".align 16\n"
    "pushl %%ebx\n"
    "movl  $32, %%ebx\n"               /* loop counter */

    ".align 16\n"
".loop1:\n"
    "movaps (%%eax), %%xmm0\n"         /*  im1 | re1 | im0 | re0 */

    "movaps (%%ecx), %%xmm2\n"         /* -c | -s | -s | c */
    "movhlps %%xmm0, %%xmm1\n"         /* im1 | re1 */
    "movaps 16(%%ecx), %%xmm3\n"       /* -c1 | -s1 | -s1 | c1 */

    "shufps $0x50, %%xmm0, %%xmm0\n"   /* im0 | im0 | re0 | re0 */
    "shufps $0x50, %%xmm1, %%xmm1\n"   /* im1 | im1 | re1 | re1 */

    "movaps  16(%%eax), %%xmm4\n"      /* im3 | re3 | im2 | re2 */

    "shufps  $0x27, %%xmm2, %%xmm2\n"  /* c | -s | -s | -c */
    "movhlps %%xmm4, %%xmm5\n"         /* im3 | re3 */
    "shufps  $0x27, %%xmm3, %%xmm3\n"  /* c1 | -s1 | -s1 | -c1 */

    "movaps 32(%%ecx), %%xmm6\n"       /* -c2 | -s2 | -s2 | c2 */
    "movaps 48(%%ecx), %%xmm7\n"       /* -c3 | -s3 | -s3 | c3 */

    "shufps $0x50, %%xmm4, %%xmm4\n"   /* im2 | im2 | re2 | re2 */
    "shufps $0x50, %%xmm5, %%xmm5\n"   /* im3 | im3 | re3 | re3 */

    "mulps %%xmm2, %%xmm0\n"
    "mulps %%xmm3, %%xmm1\n"

    "shufps $0x27, %%xmm6, %%xmm6\n"   /* c2 | -s2 | -s2 | -c2 */
    "shufps $0x27, %%xmm7, %%xmm7\n"   /* c3 | -s3 | -s3 | -c3 */

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

    "movaps %%xmm0, (%%eax)\n"
    "movaps %%xmm4, 16(%%eax)\n"
    "addl $64, %%ecx\n"
    "addl $32, %%eax\n"
    "decl %%ebx\n"
    "jnz .loop1\n"

    "popl %%ebx\n"
    : "=a" (buf)
    : "a" (buf), "c" (xcos_sin_sse) );
}

static void imdct512_window_delay_sse (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt)
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

    "movl %%esi, %%ebp\n"    /* buf */
    "movl $16, %%ebx\n"         /* loop count */
    "leal 516(%%ebp), %%esi\n"  /* buf[64].im */
    "leal 504(%%ebp), %%edi\n"  /* buf[63].re */

    ".align 16\n"
".first_128_samples:\n"
    "movss   (%%esi), %%xmm0\n"
    "movss  8(%%esi), %%xmm2\n"
    "movss   (%%edi), %%xmm1\n"
    "movss -8(%%edi), %%xmm3\n"

    "movlhps %%xmm2, %%xmm0\n"      /* 0.0 | im1 | 0.0 | im0 */
    "movlhps %%xmm3, %%xmm1\n"      /* 0.0 | re1 | 0.0 | re0 */

    "movaps (%%edx), %%xmm4\n"      /* w3 | w2 | w1 | w0 */
    "movaps (%%ecx), %%xmm5\n"      /* d3 | d2 | d1 | d0 */
    "shufps $0xb1, %%xmm1, %%xmm1\n"/* re1 | 0.0 | re0 | 0.0 */

    "movss  16(%%esi), %%xmm6\n"    /* im2 */
    "movss  24(%%esi), %%xmm7\n"    /* im3 */
    "subps     %%xmm1, %%xmm0\n"    /* -re1 | im1 | -re0 | im0 */
    "movss -16(%%edi), %%xmm2\n"    /* re2 */
    "movss -24(%%edi), %%xmm3\n"    /* re3 */
    "mulps     %%xmm4, %%xmm0\n"
    "movlhps   %%xmm7, %%xmm6\n"    /* 0.0 | im3 | 0.0 | im2 */
    "movlhps   %%xmm3, %%xmm2\n"    /* 0.0 | re3 | 0.0 | re2 */
    "addps  %%xmm5, %%xmm0\n"
    "shufps $0xb1, %%xmm2, %%xmm2\n"/* re3 | 0.0 | re2 | 0.0 */
    "movaps 16(%%edx), %%xmm4\n"    /* w7 | w6 | w5 | w4 */
    "movaps 16(%%ecx), %%xmm5\n"    /* d7 | d6 | d5 | d4 */
    "subps  %%xmm2, %%xmm6\n"       /* -re3 | im3 | -re2 | im2 */
    "addl   $32, %%edx\n"
    "movaps %%xmm0, (%%eax)\n"
    "addl   $32, %%ecx\n"
    "mulps  %%xmm4, %%xmm6\n"
    "addl   $32, %%esi\n"
    "addl   $32, %%eax\n"
    "addps  %%xmm5, %%xmm6\n"
    "addl   $-32, %%edi\n"
    "movaps %%xmm6, -16(%%eax)\n"
    "decl   %%ebx\n"
    "jnz .first_128_samples\n"

    "movl %%ebp, %%esi\n"    /* buf[0].re */
    "movl $16, %%ebx\n"         /* loop count */
    "leal 1020(%%ebp), %%edi\n" /* buf[127].im */
    
    ".align 16\n"
".second_128_samples:\n"
    "movss   (%%esi), %%xmm0\n" /* buf[i].re */
    "movss  8(%%esi), %%xmm2\n" /* re1 */
    "movss   (%%edi), %%xmm1\n" /* buf[127-i].im */
    "movss -8(%%edi), %%xmm3\n" /* im1 */

    "movlhps %%xmm2, %%xmm0\n"  /* 0.0 | re1 | 0.0 | re0 */
    "movlhps %%xmm3, %%xmm1\n"  /* 0.0 | im1 | 0.0 | im1 */

    "movaps (%%edx), %%xmm4\n"  /* w3 | w2 | w1 | w0 */
    "movaps (%%ecx), %%xmm5\n"  /* d3 | d2 | d1 | d0 */

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
    "movaps 16(%%edx), %%xmm4\n"    /* w7 | w6 | w5 | w4 */
    "addl   $32, %%esi\n"
    "subps  %%xmm2, %%xmm6\n"       /* -im3 | re3 | -im2 | re2 */
    "addps  %%xmm5, %%xmm0\n"
    "mulps  %%xmm4, %%xmm6\n"
    "addl   $-32, %%edi\n"
    "movaps 16(%%ecx), %%xmm5\n"    /* d7 | d6 | d5 | d4 */
    "movaps %%xmm0, (%%eax)\n"
    "addps  %%xmm5, %%xmm6\n"
    "addl   $32, %%edx\n"
    "addl   $32, %%eax\n"
    "addl   $32, %%ecx\n"
    "movaps %%xmm6, -16(%%eax)\n"
    "decl   %%ebx\n"
    "jnz .second_128_samples\n"

    "leal 512(%%ebp), %%esi\n"  /* buf[64].re */
    "leal 508(%%ebp), %%edi\n"  /* buf[63].im */
    "movl $16, %%ebx\n"         /* loop count */
    "addl $-1024, %%ecx\n"  /* delay */

    ".align 16\n"
".first_128_delay:\n"
    "movss   (%%esi), %%xmm0\n"
    "movss  8(%%esi), %%xmm2\n"
    "movss   (%%edi), %%xmm1\n"
    "movss -8(%%edi), %%xmm3\n"

    "movlhps %%xmm2, %%xmm0\n"      /* 0.0 | re1 | 0.0 | re0 */
    "movlhps %%xmm3, %%xmm1\n"      /* 0.0 | im1 | 0.0 | im0 */

    "movaps -16(%%edx), %%xmm4\n"   /* w3 | w2 | w1 | w0 */
    "shufps $0xb1, %%xmm1, %%xmm1\n"/* im1 | 0.0 | im0 | 0.0 */
    "movss  16(%%esi), %%xmm6\n"    /* re2 */
    "movss  24(%%esi), %%xmm7\n"    /* re3 */
    "movss -16(%%edi), %%xmm2\n"    /* im2 */
    "movss -24(%%edi), %%xmm3\n"    /* im3 */
    "subps   %%xmm1, %%xmm0\n"      /* -im1 | re1 | -im0 | re0 */
    "addl    $-32, %%edx\n"
    "movlhps %%xmm7, %%xmm6\n"      /* 0.0 | re3 | 0.0 | re2 */
    "movlhps %%xmm3, %%xmm2\n"      /* 0.0 | im3 | 0.0 | im2 */
    "mulps   %%xmm4, %%xmm0\n"
    "movaps (%%edx), %%xmm5\n"      /* w7 | w6 | w5 | w4 */
    "shufps $0xb1, %%xmm2, %%xmm2\n"/* im3 | 0.0 | im2 | 0.0 */
    "movaps %%xmm0, (%%ecx)\n"
    "addl   $32, %%esi\n"
    "subps  %%xmm2, %%xmm6\n"       /* -im3 | re3 | -im2 | re2 */
    "addl   $-32, %%edi\n"
    "mulps  %%xmm5, %%xmm6\n"
    "addl   $32, %%ecx\n"
    "movaps %%xmm6, -16(%%ecx)\n"
    "decl   %%ebx\n"
    "jnz .first_128_delay\n"

    "leal    4(%%ebp), %%esi\n" /* buf[0].im */
    "leal 1016(%%ebp), %%edi\n" /* buf[127].re */
    "movl $16, %%ebx\n"         /* loop count */
    
    ".align 16\n"
".second_128_delay:\n"
    "movss   (%%esi), %%xmm0\n"
    "movss  8(%%esi), %%xmm2\n"
    "movss   (%%edi), %%xmm1\n"
    "movss -8(%%edi), %%xmm3\n"

    "movlhps %%xmm2, %%xmm0\n"      /* 0.0 | im1 | 0.0 | im0 */
    "movlhps %%xmm3, %%xmm1\n"      /* 0.0 | re1 | 0.0 | re0 */

    "movaps -16(%%edx), %%xmm4\n"   /* w3 | w2 | w1 | w0 */
    "shufps $0xb1, %%xmm1, %%xmm1\n"/* re1 | 0.0 | re0 | 0.0 */
    "movss  16(%%esi), %%xmm6\n"    /* im2 */
    "movss  24(%%esi), %%xmm7\n"    /* im3 */
    "movss -16(%%edi), %%xmm2\n"    /* re2 */
    "movss -24(%%edi), %%xmm3\n"    /* re3 */
    "subps   %%xmm0, %%xmm1\n"      /* re1 | -im1 | re0 | -im0 */
    "addl    $-32, %%edx\n"
    "movlhps %%xmm7, %%xmm6\n"      /* 0.0 | im3 | 0.0 | im2 */
    "movlhps %%xmm3, %%xmm2\n"      /* 0.0 | re3 | 0.0 | re2 */
    "mulps   %%xmm4, %%xmm1\n"
    "movaps (%%edx), %%xmm5\n"      /* w7 | w6 | w5 | w4 */
    "shufps $0xb1, %%xmm2, %%xmm2\n"/* re3 | 0.0 | re2 | 0.0 */
    "movaps %%xmm1, (%%ecx)\n"
    "addl   $32, %%esi\n"
    "subps  %%xmm6, %%xmm2\n"       /* re | -im3 | re | -im2 */
    "addl   $-32, %%edi\n"
    "mulps  %%xmm5, %%xmm2\n"
    "addl   $32, %%ecx\n"
    "movaps %%xmm2, -16(%%ecx)\n"
    "decl   %%ebx\n"
    "jnz .second_128_delay\n"

    "popl %%ebp\n"
    "popl %%esi\n"
    "popl %%edi\n"
    "popl %%edx\n"
    "popl %%ecx\n"
    "popl %%ebx\n"
    "popl %%eax\n"
    : "=S" (buf), "=a" (data_ptr), "=c" (delay_prt), "=d" (window_prt)
    : "S" (buf), "a" (data_ptr), "c" (delay_prt), "d" (window_prt));
    
}

static void imdct512_window_delay_nol_sse( complex_t *buf, float *data_ptr,
                                           float *window_prt, float *delay_prt )
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
    "movl $16, %%ebx\n"         /* loop count */
    "leal 516(%%ebp), %%esi\n"  /* buf[64].im */
    "leal 504(%%ebp), %%edi\n"  /* buf[63].re */
    
    ".align 16\n"
".first_128_sample:\n"
    "movss   (%%esi), %%xmm0\n"
    "movss  8(%%esi), %%xmm2\n"
    "movss   (%%edi), %%xmm1\n"
    "movss -8(%%edi), %%xmm3\n"

    "movlhps %%xmm2, %%xmm0\n"      /* 0.0 | im1 | 0.0 | im0 */
    "movlhps %%xmm3, %%xmm1\n"      /* 0.0 | re1 | 0.0 | re0 */

    "movaps (%%edx), %%xmm4\n"      /* w3 | w2 | w1 | w0 */
    "shufps $0xb1, %%xmm1, %%xmm1\n"/* re1 | 0.0 | re0 | 0.0 */

    "movss  16(%%esi), %%xmm6\n"    /* im2 */
    "movss  24(%%esi), %%xmm7\n"    /* im3 */
    "subps     %%xmm1, %%xmm0\n"    /* -re1 | im1 | -re0 | im0 */
    "movss -16(%%edi), %%xmm2\n"    /* re2 */
    "movss -24(%%edi), %%xmm3\n"    /* re3 */
    "mulps   %%xmm4, %%xmm0\n"
    "movlhps %%xmm7, %%xmm6\n"      /* 0.0 | im3 | 0.0 | im2 */
    "movlhps %%xmm3, %%xmm2\n"      /* 0.0 | re3 | 0.0 | re2 */
    "shufps $0xb1, %%xmm2, %%xmm2\n"/* re3 | 0.0 | re2 | 0.0 */
    "movaps 16(%%edx), %%xmm4\n"    /* w7 | w6 | w5 | w4 */
    "subps  %%xmm2, %%xmm6\n"       /* -re3 | im3 | -re2 | im2 */
    "addl   $32, %%edx\n"
    "movaps %%xmm0, (%%eax)\n"
    "mulps  %%xmm4, %%xmm6\n"
    "addl   $32, %%esi\n"
    "addl   $32, %%eax\n"
    "addl   $-32, %%edi\n"
    "movaps %%xmm6, -16(%%eax)\n"
    "decl   %%ebx\n"
    "jnz .first_128_sample\n"

    "movl %%ebp, %%esi\n"     /* buf[0].re */
    "movl $16, %%ebx\n"             /* loop count */
    "leal 1020(%%ebp), %%edi\n"     /* buf[127].im */
    
    ".align 16\n"
".second_128_sample:\n"
    "movss   (%%esi), %%xmm0\n"     /* buf[i].re */
    "movss  8(%%esi), %%xmm2\n"     /* re1 */
    "movss   (%%edi), %%xmm1\n"     /* buf[127-i].im */
    "movss -8(%%edi), %%xmm3\n"     /* im1 */

    "movlhps %%xmm2, %%xmm0\n"      /* 0.0 | re1 | 0.0 | re0 */
    "movlhps %%xmm3, %%xmm1\n"      /* 0.0 | im1 | 0.0 | im1 */
    
    "movaps (%%edx), %%xmm4\n"      /* w3 | w2 | w1 | w0 */

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
    "movaps 16(%%edx), %%xmm4\n"    /* w7 | w6 | w5 | w4 */
    "addl   $32, %%esi\n"
    "subps  %%xmm2, %%xmm6\n"       /* -im3 | re3 | -im2 | re2 */
    "mulps  %%xmm4, %%xmm6\n"
    "addl   $-32, %%edi\n"
    "movaps %%xmm0, (%%eax)\n"
    "addl   $32, %%edx\n"
    "addl   $32, %%eax\n"
    "movaps %%xmm6, -16(%%eax)\n"
    "decl   %%ebx\n"
    "jnz .second_128_sample\n"

    "leal 512(%%ebp), %%esi\n"  /* buf[64].re */
    "leal 508(%%ebp), %%edi\n"  /* buf[63].im */
    "movl $16, %%ebx\n"         /* loop count */
    
    ".align 16\n"
".first_128_delays:\n"
    "movss   (%%esi), %%xmm0\n"
    "movss  8(%%esi), %%xmm2\n"
    "movss   (%%edi), %%xmm1\n"
    "movss -8(%%edi), %%xmm3\n"

    "movlhps %%xmm2, %%xmm0\n"  /* 0.0 | re1 | 0.0 | re0 */
    "movlhps %%xmm3, %%xmm1\n"  /* 0.0 | im1 | 0.0 | im0 */

    "movaps -16(%%edx), %%xmm4\n"   /* w3 | w2 | w1 | w0 */
    "shufps $0xb1, %%xmm1, %%xmm1\n"/* im1 | 0.0 | im0 | 0.0 */
    "movss  16(%%esi), %%xmm6\n"    /* re2 */
    "movss  24(%%esi), %%xmm7\n"    /* re3 */
    "movss -16(%%edi), %%xmm2\n"    /* im2 */
    "movss -24(%%edi), %%xmm3\n"    /* im3 */
    "subps %%xmm1, %%xmm0\n"        /* -im1 | re1 | -im0 | re0 */
    "addl  $-32, %%edx\n"
    "movlhps %%xmm7, %%xmm6\n"      /* 0.0 | re3 | 0.0 | re2 */
    "movlhps %%xmm3, %%xmm2\n"      /* 0.0 | im3 | 0.0 | im2 */
    "mulps   %%xmm4, %%xmm0\n"
    "movaps (%%edx), %%xmm5\n"      /* w7 | w6 | w5 | w4 */
    "shufps $0xb1, %%xmm2, %%xmm2\n"/* im3 | 0.0 | im2 | 0.0 */
    "movaps %%xmm0, (%%ecx)\n"
    "addl   $32, %%esi\n"
    "subps  %%xmm2, %%xmm6\n"       /* -im3 | re3 | -im2 | re2 */
    "addl   $-32, %%edi\n"
    "mulps  %%xmm5, %%xmm6\n"
    "addl   $32, %%ecx\n"
    "movaps %%xmm6, -16(%%ecx)\n"
    "decl   %%ebx\n"
    "jnz .first_128_delays\n"

    "leal    4(%%ebp), %%esi\n" /* buf[0].im */
    "leal 1016(%%ebp), %%edi\n" /* buf[127].re */
    "movl $16, %%ebx\n"         /* loop count */
    
    ".align 16\n"
".second_128_delays:\n"
    "movss   (%%esi), %%xmm0\n"
    "movss  8(%%esi), %%xmm2\n"
    "movss   (%%edi), %%xmm1\n"
    "movss -8(%%edi), %%xmm3\n"

    "movlhps %%xmm2, %%xmm0\n"  /* 0.0 | im1 | 0.0 | im0 */
    "movlhps %%xmm3, %%xmm1\n"  /* 0.0 | re1 | 0.0 | re0 */

    "movaps -16(%%edx), %%xmm4\n"   /* w3 | w2 | w1 | w0 */
    "shufps $0xb1, %%xmm1, %%xmm1\n"/* re1 | 0.0 | re0 | 0.0 */
    "movss  16(%%esi), %%xmm6\n"    /* im2 */
    "movss  24(%%esi), %%xmm7\n"    /* im3 */
    "movss -16(%%edi), %%xmm2\n"    /* re2 */
    "movss -24(%%edi), %%xmm3\n"    /* re3 */
    "subps   %%xmm0, %%xmm1\n"      /* re1 | -im1 | re0 | -im0 */
    "addl    $-32, %%edx\n"
    "movlhps %%xmm7, %%xmm6\n"      /* 0.0 | im3 | 0.0 | im2 */
    "movlhps %%xmm3, %%xmm2\n"      /* 0.0 | re3 | 0.0 | re2 */
    "mulps   %%xmm4, %%xmm1\n"
    "movaps (%%edx), %%xmm5\n"      /* w7 | w6 | w5 | w4 */
    "shufps $0xb1, %%xmm2, %%xmm2\n"/* re3 | 0.0 | re2 | 0.0 */
    "movaps %%xmm1, (%%ecx)\n"
    "addl   $32, %%esi\n"
    "subps  %%xmm6, %%xmm2\n"       /* re | -im3 | re | -im2 */
    "addl   $-32, %%edi\n"
    "mulps  %%xmm5, %%xmm2\n"
    "addl   $32, %%ecx\n"
    "movaps %%xmm2, -16(%%ecx)\n"
    "decl   %%ebx\n"
    "jnz .second_128_delays\n"

    "popl %%ebp\n"
    "popl %%esi\n"
    "popl %%edi\n"
    "popl %%edx\n"
    "popl %%ecx\n"
    "popl %%ebx\n"
    "popl %%eax\n"
    : "=S" (buf), "=a" (data_ptr), "=c" (delay_prt), "=d" (window_prt)
    : "S" (buf), "a" (data_ptr), "c" (delay_prt), "d" (window_prt));

}
