/*****************************************************************************
 * ac3_downmix_sse.c: accelerated SSE ac3 downmix functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: ac3_downmix_sse.c,v 1.8 2001/12/10 04:53:10 sam Exp $
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

#define MODULE_NAME downmixsse
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include "common.h"

#include "ac3_downmix.h"

const float sqrt2_sse __asm__ ("sqrt2_sse") __attribute__ ((aligned (16))) = 0.7071068;

void _M( downmix_3f_2r_to_2ch ) (float * samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "movl  $64, %%ebx\n"            /* loop counter */

    "movss     (%%ecx), %%xmm5\n"   /* unit */
    "shufps $0, %%xmm5, %%xmm5\n"   /* unit | unit | unit | unit */

    "movss    4(%%ecx), %%xmm6\n"   /* clev */
    "shufps $0, %%xmm6, %%xmm6\n"   /* clev | clev | clev | clev */

    "movss    8(%%ecx), %%xmm7\n"   /* slev */
    "shufps $0, %%xmm7, %%xmm7\n"   /* slev | slev | slev | slev */

    ".align 16\n"
".loop:\n"
    "movaps     (%%eax), %%xmm0\n"  /* left */
    "movaps 2048(%%eax), %%xmm1\n"  /* right */
    "movaps 1024(%%eax), %%xmm2\n"  /* center */
    "movaps 3072(%%eax), %%xmm3\n"  /* leftsur */
    "movaps 4096(%%eax), %%xmm4\n"  /* rithgsur */
    "mulps %%xmm5, %%xmm0\n"
    "mulps %%xmm5, %%xmm1\n"
    "mulps %%xmm6, %%xmm2\n"
    "addps %%xmm2, %%xmm0\n"
    "addps %%xmm2, %%xmm1\n"
    "mulps %%xmm7, %%xmm3\n"
    "mulps %%xmm7, %%xmm4\n"
    "addps %%xmm3, %%xmm0\n"
    "addps %%xmm4, %%xmm1\n"

    "movaps %%xmm0, (%%eax)\n"
    "movaps %%xmm1, 1024(%%eax)\n"

    "addl $16, %%eax\n"
    "decl %%ebx\n"
    "jnz  .loop\n"
    
    "popl %%ebx\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}

void _M( downmix_2f_2r_to_2ch ) (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "movl  $64, %%ebx\n"            /* loop counter */

    "movss     (%%ecx), %%xmm5\n"   /* unit */
    "shufps $0, %%xmm5, %%xmm5\n"   /* unit | unit | unit | unit */

    "movss    8(%%ecx), %%xmm7\n"   /* slev */
    "shufps $0, %%xmm7, %%xmm7\n"   /* slev | slev | slev | slev */

    ".align 16\n"
".loop3:\n"
    "movaps     (%%eax), %%xmm0\n"  /* left */
    "movaps 1024(%%eax), %%xmm1\n"  /* right */
    "movaps 2048(%%eax), %%xmm3\n"  /* leftsur */
    "movaps 3072(%%eax), %%xmm4\n"  /* rightsur */
    "mulps %%xmm5, %%xmm0\n"
    "mulps %%xmm5, %%xmm1\n"
    "mulps %%xmm7, %%xmm3\n"
    "mulps %%xmm7, %%xmm4\n"
    "addps %%xmm3, %%xmm0\n"
    "addps %%xmm4, %%xmm1\n"

    "movaps %%xmm0, (%%eax)\n"
    "movaps %%xmm1, 1024(%%eax)\n"

    "addl $16, %%eax\n"
    "decl %%ebx\n"
    "jnz  .loop3\n"

    "popl %%ebx\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}

void _M( downmix_3f_1r_to_2ch ) (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "movl  $64, %%ebx\n"            /* loop counter */

    "movss     (%%ecx), %%xmm5\n"   /* unit */
    "shufps $0, %%xmm5, %%xmm5\n"   /* unit | unit | unit | unit */

    "movss    4(%%ecx), %%xmm6\n"   /* clev */
    "shufps $0, %%xmm6, %%xmm6\n"   /* clev | clev | clev | clev */

    "movss    8(%%ecx), %%xmm7\n"   /* slev */
    "shufps $0, %%xmm7, %%xmm7\n"   /* slev | slev | slev | slev */

    ".align 16\n"
".loop4:\n"
    "movaps     (%%eax), %%xmm0\n"  /* left */
    "movaps 2048(%%eax), %%xmm1\n"  /* right */
    "movaps 1024(%%eax), %%xmm2\n"  /* center */
    "movaps 3072(%%eax), %%xmm3\n"  /* sur */
    "mulps %%xmm5, %%xmm0\n"
    "mulps %%xmm5, %%xmm1\n"
    "mulps %%xmm6, %%xmm2\n"
    "addps %%xmm2, %%xmm0\n"
    "mulps %%xmm7, %%xmm3\n"
    "addps %%xmm2, %%xmm1\n"
    "subps %%xmm3, %%xmm0\n"
    "addps %%xmm3, %%xmm1\n"

    "movaps %%xmm0, (%%eax)\n"
    "movaps %%xmm1, 1024(%%eax)\n"

    "addl $16, %%eax\n"
    "decl %%ebx\n"
    "jnz  .loop4\n"

    "popl %%ebx\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}

void _M( downmix_2f_1r_to_2ch ) (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "movl  $64, %%ebx\n"            /* loop counter */

    "movss     (%%ecx), %%xmm5\n"   /* unit */
    "shufps $0, %%xmm5, %%xmm5\n"   /* unit | unit | unit | unit */

    "movss    8(%%ecx), %%xmm7\n"   /* slev */
    "shufps $0, %%xmm7, %%xmm7\n"   /* slev | slev | slev | slev */

    ".align 16\n"
".loop5:\n"
    "movaps     (%%eax), %%xmm0\n"  /* left */
    "movaps 1024(%%eax), %%xmm1\n"  /* right */
    "movaps 2048(%%eax), %%xmm3\n"  /* sur */
    "mulps %%xmm5, %%xmm0\n"
    "mulps %%xmm5, %%xmm1\n"
    "mulps %%xmm7, %%xmm3\n"
    "subps %%xmm3, %%xmm0\n"
    "addps %%xmm3, %%xmm1\n"

    "movaps %%xmm0, (%%eax)\n"
    "movaps %%xmm1, 1024(%%eax)\n"

    "addl $16, %%eax\n"
    "decl %%ebx\n"
    "jnz  .loop5\n"

    "popl %%ebx\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}

void _M( downmix_3f_0r_to_2ch ) (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "movl  $64, %%ebx\n"           /* loop counter */

    "movss     (%%ecx), %%xmm5\n"  /* unit */
    "shufps $0, %%xmm5, %%xmm5\n"  /* unit | unit | unit | unit */

    "movss    4(%%ecx), %%xmm6\n"  /* clev */
    "shufps $0, %%xmm6, %%xmm6\n"  /* clev | clev | clev | clev */

    ".align 16\n"
".loop6:\n"
    "movaps     (%%eax), %%xmm0\n"  /*left */
    "movaps 2048(%%eax), %%xmm1\n"  /* right */
    "movaps 1024(%%eax), %%xmm2\n"  /* center */
    "mulps %%xmm5, %%xmm0\n"
    "mulps %%xmm5, %%xmm1\n"
    "mulps %%xmm6, %%xmm2\n"
    "addps %%xmm2, %%xmm0\n"
    "addps %%xmm2, %%xmm1\n"

    "movaps %%xmm0, (%%eax)\n"
    "movaps %%xmm1, 1024(%%eax)\n"

    "addl $16, %%eax\n"
    "decl %%ebx\n"
    "jnz  .loop6\n"

    "popl %%ebx\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}
    
void _M( stream_sample_1ch_to_s16 ) (s16 *s16_samples, float *left)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "pushl %%edx\n"

    "movl   $sqrt2_sse, %%edx\n"
    "movss  (%%edx), %%xmm7\n"
    "shufps $0, %%xmm7, %%xmm7\n"  /* sqrt2 | sqrt2 | sqrt2 | sqrt2 */
    "movl   $64, %%ebx\n"
    
    ".align 16\n"
".loop2:\n"
    "movaps (%%ecx), %%xmm0\n"     /* c3 | c2 | c1 | c0 */
    "mulps   %%xmm7, %%xmm0\n"
    "movhlps %%xmm0, %%xmm2\n"     /* c3 | c2 */

    "cvtps2pi %%xmm0, %%mm0\n"     /* c1 c0 --> mm0, int_32 */
    "cvtps2pi %%xmm2, %%mm1\n"     /* c3 c2 --> mm1, int_32 */

    "packssdw %%mm0, %%mm0\n"      /* c1 c1 c0 c0 --> mm0, int_16 */
    "packssdw %%mm1, %%mm1\n"      /* c3 c3 c2 c2 --> mm1, int_16 */

    "movq %%mm0, (%%eax)\n"
    "movq %%mm1, 8(%%eax)\n"
    "addl $16, %%eax\n"
    "addl $16, %%ecx\n"

    "decl %%ebx\n"
    "jnz .loop2\n"

    "popl %%edx\n"
    "popl %%ebx\n"
    "emms\n"
    : "=a" (s16_samples), "=c" (left)
    : "a" (s16_samples), "c" (left));
}

void _M( stream_sample_2ch_to_s16 ) (s16 *s16_samples, float *left, float *right)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "movl  $64, %%ebx\n"

    ".align 16\n"
".loop1:\n"
    "movaps  (%%ecx), %%xmm0\n"   /* l3 | l2 | l1 | l0 */
    "movaps  (%%edx), %%xmm1\n"   /* r3 | r2 | r1 | r0 */
    "movhlps  %%xmm0, %%xmm2\n"   /* l3 | l2 */
    "movhlps  %%xmm1, %%xmm3\n"   /* r3 | r2 */
    "unpcklps %%xmm1, %%xmm0\n"   /* r1 | l1 | r0 | l0 */
    "unpcklps %%xmm3, %%xmm2\n"   /* r3 | l3 | r2 | l2 */

    "cvtps2pi %%xmm0, %%mm0\n"    /* r0 l0 --> mm0, int_32 */
    "movhlps  %%xmm0, %%xmm0\n"
    "cvtps2pi %%xmm0, %%mm1\n"    /* r1 l1 --> mm1, int_32 */
    "cvtps2pi %%xmm2, %%mm2\n"    /* r2 l2 --> mm2, int_32 */
    "movhlps  %%xmm2, %%xmm2\n"
    "cvtps2pi %%xmm2, %%mm3\n"    /* r3 l3 --> mm3, int_32 */
    
    "packssdw %%mm1, %%mm0\n"     /* r1 l1 r0 l0 --> mm0, int_16 */
    "packssdw %%mm3, %%mm2\n"     /* r3 l3 r2 l2 --> mm2, int_16 */

    "movq %%mm0, (%%eax)\n"
    "movq %%mm2, 8(%%eax)\n"
    "addl $16, %%eax\n"
    "addl $16, %%ecx\n"
    "addl $16, %%edx\n"

    "decl %%ebx\n"
    "jnz .loop1\n"

    "popl %%ebx\n"
    "emms\n"
    : "=a" (s16_samples), "=c" (left), "=d" (right)
    : "a" (s16_samples), "c" (left), "d" (right));
    
}

