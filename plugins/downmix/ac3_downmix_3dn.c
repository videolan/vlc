/*****************************************************************************
 * ac3_downmix_3dn.c: accelerated 3D Now! ac3 downmix functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: ac3_downmix_3dn.c,v 1.6 2001/11/28 15:08:05 massiot Exp $
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

#define MODULE_NAME downmix3dn
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include "config.h"
#include "common.h"

#include "ac3_downmix.h"

static const float sqrt2_3dn __asm__ ("sqrt2_3dn") = 0.7071068;

void _M( downmix_3f_2r_to_2ch ) (float * samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "movl  $128,  %%ebx\n"            /* loop counter */

    "movd    (%%ecx), %%mm5\n"        /* unit */
    "punpckldq %%mm5, %%mm5\n"        /* unit | unit */

    "movd    4(%%ecx), %%mm6\n"        /* clev */
    "punpckldq %%mm6, %%mm6\n"        /* clev | clev */

    "movd    8(%%ecx), %%mm7\n"        /* slev */
    "punpckldq %%mm7, %%mm7\n"        /* slev | slev */

    ".align 16\n"
".loop:\n"
    "movq    (%%eax),     %%mm0\n"   /* left */
    "movq    2048(%%eax), %%mm1\n"   /* right */
    "movq   1024(%%eax), %%mm2\n"    /* center */
    "movq    3072(%%eax), %%mm3\n"    /* leftsur */
    "movq    4096(%%eax), %%mm4\n"    /* rightsur */
    "pfmul    %%mm5, %%mm0\n"
    "pfmul    %%mm5, %%mm1\n"
    "pfmul    %%mm6, %%mm2\n"
    "pfadd    %%mm2, %%mm0\n"
    "pfadd     %%mm2, %%mm1\n"
    "pfmul  %%mm7, %%mm3\n"
    "pfmul    %%mm7, %%mm4\n"
    "pfadd    %%mm3, %%mm0\n"
    "pfadd    %%mm4, %%mm1\n"

    "movq    %%mm0, (%%eax)\n"
    "movq    %%mm1, 1024(%%eax)\n"

    "addl    $8, %%eax\n"
    "decl     %%ebx\n"
    "jnz    .loop\n"
    
    "popl   %%ebx\n"
    "femms\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}

void _M( downmix_2f_2r_to_2ch ) (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "movl  $128, %%ebx\n"       /* loop counter */

    "movd  (%%ecx), %%mm5\n"    /* unit */
    "punpckldq %%mm5, %%mm5\n"  /* unit | unit */

    "movd    8(%%ecx), %%mm7\n"    /* slev */
    "punpckldq %%mm7, %%mm7\n"    /* slev | slev */

    ".align 16\n"
".loop3:\n"
    "movq   (%%eax), %%mm0\n"       /* left */
    "movq   1024(%%eax), %%mm1\n"   /* right */
    "movq   2048(%%eax), %%mm3\n"    /* leftsur */
    "movq   3072(%%eax), %%mm4\n"    /* rightsur */
    "pfmul    %%mm5, %%mm0\n"
    "pfmul    %%mm5, %%mm1\n"
    "pfmul    %%mm7, %%mm3\n"
    "pfmul    %%mm7, %%mm4\n"
    "pfadd    %%mm3, %%mm0\n"
    "pfadd    %%mm4, %%mm1\n"

    "movq    %%mm0, (%%eax)\n"
    "movq    %%mm1, 1024(%%eax)\n"

    "addl    $8, %%eax\n"
    "decl     %%ebx\n"
    "jnz    .loop3\n"

    "popl    %%ebx\n"
    "femms\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}

void _M( downmix_3f_1r_to_2ch ) (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl    %%ebx\n"
    "movl    $128, %%ebx\n"            /* loop counter */

    "movd    (%%ecx), %%mm5\n"        /* unit */
    "punpckldq %%mm5, %%mm5\n"        /* unit | unit */

    "movd    4(%%ecx), %%mm6\n"        /* clev */
    "punpckldq %%mm6, %%mm6\n"        /* clev | clev */

    "movd    8(%%ecx), %%mm7\n"        /* slev */
    "punpckldq %%mm7, %%mm7\n"      /* slev | slev */

    ".align 16\n"
".loop4:\n"
    "movq    (%%eax), %%mm0\n"       /* left */
    "movq    2048(%%eax), %%mm1\n"   /* right */
    "movq    1024(%%eax), %%mm2\n"    /* center */
    "movq    3072(%%eax), %%mm3\n"    /* sur */
    "pfmul    %%mm5, %%mm0\n"
    "pfmul    %%mm5, %%mm1\n"
    "pfmul    %%mm6, %%mm2\n"
    "pfadd    %%mm2, %%mm0\n"
    "pfmul    %%mm7, %%mm3\n"
    "pfadd     %%mm2, %%mm1\n"
    "pfsub    %%mm3, %%mm0\n"
    "pfadd    %%mm3, %%mm1\n"

    "movq    %%mm0, (%%eax)\n"
    "movq    %%mm1, 1024(%%eax)\n"

    "addl    $8, %%eax\n"
    "decl     %%ebx\n"
    "jnz    .loop4\n"

    "popl    %%ebx\n"
    "femms\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}

void _M( downmix_2f_1r_to_2ch ) (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl    %%ebx\n"
    "movl    $128, %%ebx\n"            /* loop counter */

    "movd    (%%ecx), %%mm5\n"        /* unit */
    "punpckldq %%mm5, %%mm5\n"        /* unit | unit */

    "movd    8(%%ecx), %%mm7\n"        /* slev */
    "punpckldq %%mm7, %%mm7\n"      /* slev | slev */

    ".align 16\n"
".loop5:\n"
    "movq    (%%eax), %%mm0\n"       /* left */
    "movq    1024(%%eax), %%mm1\n"   /* right */
    "movq    2048(%%eax), %%mm3\n"    /* sur */
    "pfmul    %%mm5, %%mm0\n"
    "pfmul    %%mm5, %%mm1\n"
    "pfmul    %%mm7, %%mm3\n"
    "pfsub    %%mm3, %%mm0\n"
    "pfadd    %%mm3, %%mm1\n"

    "movq    %%mm0, (%%eax)\n"
    "movq    %%mm1, 1024(%%eax)\n"

    "addl    $8, %%eax\n"
    "decl     %%ebx\n"
    "jnz    .loop5\n"

    "popl    %%ebx\n"
    "femms\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}

void _M( downmix_3f_0r_to_2ch ) (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl    %%ebx\n"
    "movl    $128, %%ebx\n"            /* loop counter */

    "movd    (%%ecx), %%mm5\n"        /* unit */
    "punpckldq %%mm5, %%mm5\n"        /* unit | unit */

    "movd    4(%%ecx), %%mm6\n"        /* clev */
    "punpckldq %%mm6, %%mm6\n"      /* clev | clev */

    ".align 16\n"
".loop6:\n"
    "movq    (%%eax), %%mm0\n"       /*left */
    "movq    2048(%%eax), %%mm1\n"   /* right */
    "movq   1024(%%eax), %%mm2\n"   /* center */
    "pfmul    %%mm5, %%mm0\n"
    "pfmul    %%mm5, %%mm1\n"
    "pfmul    %%mm6, %%mm2\n"
    "pfadd    %%mm2, %%mm0\n"
    "pfadd     %%mm2, %%mm1\n"

    "movq    %%mm0, (%%eax)\n"
    "movq    %%mm1, 1024(%%eax)\n"

    "addl    $8, %%eax\n"
    "decl     %%ebx\n"
    "jnz    .loop6\n"

    "popl    %%ebx\n"
    "femms\n"
    : "=a" (samples)
    : "a" (samples), "c" (dm_par));
}

void _M( stream_sample_1ch_to_s16 ) (s16 *s16_samples, float *left)
{
    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "pushl %%edx\n"

    "movl   $sqrt2_3dn, %%edx\n"
    "movd  (%%edx), %%mm7\n"
    "punpckldq %%mm7, %%mm7\n"   /* sqrt2 | sqrt2 */
    "movl $128, %%ebx\n"

    ".align 16\n"
".loop2:\n"
    "movq (%%ecx), %%mm0\n"        /* c1 | c0 */
    "pfmul   %%mm7, %%mm0\n"

    "pf2id %%mm0, %%mm0\n"        /* c1 c0 --> mm0, int_32 */

    "packssdw %%mm0, %%mm0\n"        /* c1 c1 c0 c0 --> mm0, int_16 */

    "movq %%mm0, (%%eax)\n"
    "addl $8, %%eax\n"
    "addl $8, %%ecx\n"

    "decl %%ebx\n"
    "jnz .loop2\n"

    "popl %%edx\n"
    "popl %%ebx\n"
    "femms\n"
    : "=a" (s16_samples), "=c" (left)
    : "a" (s16_samples), "c" (left));
}

void _M( stream_sample_2ch_to_s16 ) (s16 *s16_samples, float *left, float *right)
{

    __asm__ __volatile__ (
    ".align 16\n"
    "pushl %%ebx\n"
    "movl $128, %%ebx\n"

    ".align 16\n"
".loop1:\n"
    "movq  (%%ecx), %%mm0\n"    /* l1 | l0 */
    "movq  (%%edx), %%mm1\n"    /* r1 | r0 */
    "movq   %%mm0,  %%mm2\n"    /* l1 | l0 */
    "punpckldq %%mm1, %%mm0\n"    /* r0 | l0 */
    "punpckhdq %%mm1, %%mm2\n"    /* r1 | l1 */

    "pf2id    %%mm0, %%mm0\n"    /* r0 l0 --> mm0, int_32 */
    "pf2id    %%mm2, %%mm2\n"    /* r0 l0 --> mm0, int_32 */
    
    "packssdw %%mm2, %%mm0\n"    /* r1 l1 r0 l0 --> mm0, int_16 */

    "movq %%mm0, (%%eax)\n"
    "movq %%mm2, 8(%%eax)\n"
    "addl $8, %%eax\n"
    "addl $8, %%ecx\n"
    "addl $8, %%edx\n"

    "decl %%ebx\n"
    "jnz .loop1\n"

    "popl %%ebx\n"
    "femms\n"
    : "=a" (s16_samples), "=c" (left), "=d" (right)
    : "a" (s16_samples), "c" (left), "d" (right));
    
}

