/*****************************************************************************
 * ac3_downmix_sse.c: ac3 downmix functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: ac3_downmix_sse.c,v 1.1 2001/05/14 15:58:04 reno Exp $
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

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "stream_control.h"
#include "input_ext-dec.h"
#include "ac3_decoder.h"


void sqrt2 (void)
{
    __asm__ (".float 0f0.7071068");
}

void downmix_3f_2r_to_2ch_sse (float * samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
    "pushl %%ecx\n"
	"movl  $64,  %%ecx\n"	        /* loop counter */

	"movss	(%%ebx), %%xmm5\n"	    /* unit */
	"shufps	$0, %%xmm5, %%xmm5\n"	/* unit | unit | unit | unit */

	"movss	4(%%ebx), %%xmm6\n"		/* clev */
	"shufps	$0, %%xmm6, %%xmm6\n"	/* clev | clev | clev | clev */

	"movss	8(%%ebx), %%xmm7\n"		/* slev */
	"shufps	$0, %%xmm7, %%xmm7\n"	/* slev | slev | slev | slev */

".loop:\n"
	"movups	(%%eax),     %%xmm0\n"  /* left */
	"movups	2048(%%eax), %%xmm1\n"  /* right */
	"movups 1024(%%eax), %%xmm2\n"	/* center */
	"movups	3072(%%eax), %%xmm3\n"	/* leftsur */
	"movups	4096(%%eax), %%xmm4\n"	/* rithgsur */
	"mulps	%%xmm5, %%xmm0\n"
	"mulps	%%xmm5, %%xmm1\n"
	"mulps	%%xmm6, %%xmm2\n"
	"addps	%%xmm2, %%xmm0\n"
	"addps 	%%xmm2, %%xmm1\n"
	"mulps	%%xmm7, %%xmm3\n"
	"mulps	%%xmm7, %%xmm4\n"
	"addps	%%xmm3, %%xmm0\n"
	"addps	%%xmm4, %%xmm1\n"

	"movups	%%xmm0, (%%eax)\n"
	"movups	%%xmm1, 1024(%%eax)\n"

	"addl	$16, %%eax\n"
	"decl 	%%ecx\n"
	"jnz	.loop\n"
    
    "popl   %%ecx\n"
    : "=a" (samples)
    : "a" (samples), "b" (dm_par));
}

void downmix_2f_2r_to_2ch_sse (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
	"pushl %%ecx\n"
	"movl  $64, %%ecx\n"            /* loop counter */

	"movss  (%%ebx), %%xmm5\n"	    /* unit */
	"shufps $0, %%xmm5, %%xmm5\n"   /* unit | unit | unit | unit */

	"movss	8(%%ebx), %%xmm7\n"		/* slev */
	"shufps	$0, %%xmm7, %%xmm7\n"	/* slev | slev | slev | slev */

".loop3:\n"
	"movups	(%%eax), %%xmm0\n"      /* left */
	"movups	1024(%%eax), %%xmm1\n"  /* right */
	"movups 2048(%%eax), %%xmm3\n"	/* leftsur */
	"movups	3072(%%eax), %%xmm4\n"	/* rightsur */
	"mulps	%%xmm5, %%xmm0\n"
	"mulps	%%xmm5, %%xmm1\n"
	"mulps	%%xmm7, %%xmm3\n"
	"mulps	%%xmm7, %%xmm4\n"
	"addps	%%xmm3, %%xmm0\n"
	"addps	%%xmm4, %%xmm1\n"

	"movups	%%xmm0, (%%eax)\n"
	"movups	%%xmm1, 1024(%%eax)\n"

	"addl	$16, %%eax\n"
	"decl 	%%ecx\n"
	"jnz	.loop3\n"

	"popl	%%ecx\n"
    : "=a" (samples)
    : "a" (samples), "b" (dm_par));
}
void downmix_3f_1r_to_2ch_sse (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (

	"pushl	%%ecx\n"
	"movl	$64, %%ecx\n"		    /* loop counter */

	"movss	(%%ebx), %%xmm5\n"	    /* unit */
	"shufps	$0, %%xmm5, %%xmm5\n"	/* unit | unit | unit | unit */

	"movss	4(%%ebx), %%xmm6\n"		/* clev */
	"shufps	$0, %%xmm6, %%xmm6\n"	/* clev | clev | clev | clev */

	"movss	8(%%ebx), %%xmm7\n"		/* slev */
	"shufps	$0, %%xmm7, %%xmm7\n"	/* slev | slev | slev | slev */

".loop4:\n"
	"movups	(%%eax), %%xmm0\n"      /* left */
	"movups	2048(%%eax), %%xmm1\n"  /* right */
	"movups	1024(%%eax), %%xmm2\n"	/* center */
    "movups	3072(%%eax), %%xmm3\n"	/* sur */
	"mulps	%%xmm5, %%xmm0\n"
	"mulps	%%xmm5, %%xmm1\n"
	"mulps	%%xmm6, %%xmm2\n"
	"addps	%%xmm2, %%xmm0\n"
	"mulps	%%xmm7, %%xmm3\n"
	"addps 	%%xmm2, %%xmm1\n"
	"subps	%%xmm3, %%xmm0\n"
	"addps	%%xmm3, %%xmm1\n"

	"movups	%%xmm0, (%%eax)\n"
	"movups	%%xmm1, 1024(%%eax)\n"

	"addl	$16, %%eax\n"
	"decl 	%%ecx\n"
	"jnz	.loop4\n"

	"popl	%%ecx\n"
    : "=a" (samples)
    : "a" (samples), "b" (dm_par));

}
void downmix_2f_1r_to_2ch_sse (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
	"pushl	%%ecx\n"
	"movl	$64, %%ecx\n"		    /* loop counter */

	"movss	(%%ebx), %%xmm5\n"	    /* unit */
	"shufps	$0, %%xmm5, %%xmm5\n"	/* unit | unit | unit | unit */

	"movss	8(%%ebx), %%xmm7\n"		/* slev */
	"shufps	$0, %%xmm7, %%xmm7\n"	/* slev | slev | slev | slev */

".loop5:\n"
	"movups	(%%eax), %%xmm0\n"      /* left */
	"movups	1024(%%eax), %%xmm1\n"  /* right */
	"movups	2048(%%eax), %%xmm3\n"	/* sur */
	"mulps	%%xmm5, %%xmm0\n"
	"mulps	%%xmm5, %%xmm1\n"
	"mulps	%%xmm7, %%xmm3\n"
	"subps	%%xmm3, %%xmm0\n"
	"addps	%%xmm3, %%xmm1\n"

	"movups	%%xmm0, (%%eax)\n"
	"movups	%%xmm1, 1024(%%eax)\n"

	"addl	$16, %%eax\n"
	"decl 	%%ecx\n"
	"jnz	.loop5\n"

	"popl	%%ecx\n"
    : "=a" (samples)
    : "a" (samples), "b" (dm_par));


}
void downmix_3f_0r_to_2ch_sse (float *samples, dm_par_t * dm_par)
{
    __asm__ __volatile__ (
	"pushl	%%ecx\n"
	"movl	$64, %%ecx\n"		    /* loop counter */

	"movss	(%%ebx), %%xmm5\n"	    /* unit */
	"shufps	$0, %%xmm5, %%xmm5\n"	/* unit | unit | unit | unit */

	"movss	4(%%ebx), %%xmm6\n"		/* clev */
	"shufps	$0, %%xmm6, %%xmm6\n"	/* clev | clev | clev | clev */

".loop6:\n"
	"movups	(%%eax), %%xmm0\n"      /*left */
	"movups	2048(%%eax), %%xmm1\n"  /* right */
	"movups 1024(%%eax), %%xmm2\n"	/* center */
	"mulps	%%xmm5, %%xmm0\n"
	"mulps	%%xmm5, %%xmm1\n"
	"mulps	%%xmm6, %%xmm2\n"
	"addps	%%xmm2, %%xmm0\n"
	"addps 	%%xmm2, %%xmm1\n"

	"movups	%%xmm0, (%%eax)\n"
	"movups	%%xmm1, 1024(%%eax)\n"

	"addl	$16, %%eax\n"
	"decl 	%%ecx\n"
	"jnz	.loop6\n"

	"popl	%%ecx\n"
    : "=a" (samples)
    : "a" (samples), "b" (dm_par));
}
    
void stream_sample_1ch_to_s16_sse (s16 *s16_samples, float *left)
{
    __asm__ __volatile__ (
    "pushl %%ecx\n"
    "pushl %%edx\n"

	"movl   $sqrt2, %%edx\n"
	"movss (%%edx), %%xmm7\n"
    "shufps $0, %%xmm7, %%xmm7\n"   /* sqrt2 | sqrt2 | sqrt2 | sqrt2 */
	"movl $64, %%ecx\n"

".loop2:\n"
	"movups (%%ebx), %%xmm0\n"	    /* c3 | c2 | c1 | c0 */
	"mulps   %%xmm7, %%xmm0\n"
	"movhlps %%xmm0, %%xmm2\n"	    /* c3 | c2 */

	"cvtps2pi %%xmm0, %%mm0\n"	    /* c1 c0 --> mm0, int_32 */
	"cvtps2pi %%xmm2, %%mm1\n"	    /* c3 c2 --> mm1, int_32 */

	"packssdw %%mm0, %%mm0\n"	    /* c1 c1 c0 c0 --> mm0, int_16 */
	"packssdw %%mm1, %%mm1\n"	    /* c3 c3 c2 c2 --> mm1, int_16 */

    "movq %%mm0, (%%eax)\n"
	"movq %%mm1, 8(%%eax)\n"
	"addl $16, %%eax\n"
	"addl $16, %%ebx\n"

	"decl %%ecx\n"
	"jnz .loop2\n"

	"popl %%edx\n"
	"popl %%ecx\n"
	"emms\n"
    : "=a" (s16_samples), "=b" (left)
    : "a" (s16_samples), "b" (left));
}

void stream_sample_2ch_to_s16_sse (s16 *s16_samples, float *left, float *right)
{

	__asm__ __volatile__ (
    "pushl %%ecx\n"
	"movl $64, %%ecx\n"

".loop1:\n"
	"movups  (%%ebx), %%xmm0\n"	/* l3 | l2 | l1 | l0 */
	"movups  (%%edx), %%xmm1\n"	/* r3 | r2 | r1 | r0 */
	"movhlps  %%xmm0, %%xmm2\n"	/* l3 | l2 */
	"movhlps  %%xmm1, %%xmm3\n"	/* r3 | r2 */
	"unpcklps %%xmm1, %%xmm0\n"	/* r1 | l1 | r0 | l0 */
	"unpcklps %%xmm3, %%xmm2\n"	/* r3 | l3 | r2 | l2 */

	"cvtps2pi %%xmm0, %%mm0\n"	/* r0 l0 --> mm0, int_32 */
	"movhlps  %%xmm0, %%xmm0\n"
	"cvtps2pi %%xmm0, %%mm1\n"	/* r1 l1 --> mm1, int_32 */
	"cvtps2pi %%xmm2, %%mm2\n"	/* r2 l2 --> mm2, int_32 */
	"movhlps  %%xmm2, %%xmm2\n"
	"cvtps2pi %%xmm2, %%mm3\n"	/* r3 l3 --> mm3, int_32 */
    
	"packssdw %%mm1, %%mm0\n"	/* r1 l1 r0 l0 --> mm0, int_16 */
	"packssdw %%mm3, %%mm2\n"	/* r3 l3 r2 l2 --> mm2, int_16 */

	"movq %%mm0, (%%eax)\n"
	"movq %%mm2, 8(%%eax)\n"
	"addl $16, %%eax\n"
	"addl $16, %%ebx\n"
	"addl $16, %%edx\n"

	"decl %%ecx\n"
	"jnz .loop1\n"

	"popl %%ecx\n"
	"emms\n"
    : "=a" (s16_samples), "=b" (left), "=d" (right)
    : "a" (s16_samples), "b" (left), "d" (right));
    
}
