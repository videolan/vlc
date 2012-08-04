 @*****************************************************************************
 @ amplify.S : ARM NEON software amplification
 @*****************************************************************************
 @ Copyright (C) 2012 Rémi Denis-Courmont
 @
 @ This program is free software; you can redistribute it and/or modify
 @ it under the terms of the GNU Lesser General Public License as published by
 @ the Free Software Foundation; either version 2.1 of the License, or
 @ (at your option) any later version.
 @
 @ This program is distributed in the hope that it will be useful,
 @ but WITHOUT ANY WARRANTY; without even the implied warranty of
 @ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 @ GNU Lesser General Public License for more details.
 @
 @ You should have received a copy of the GNU Lesser General Public License
 @ along with this program; if not, write to the Free Software Foundation,
 @ Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 @****************************************************************************/

	.syntax	unified
	.arm
	.fpu	neon
	.text

#define	DST	r0
#define	SRC	r1
#define	SIZE	r2
	.align 2
	.global amplify_float_arm_neon
	.type	amplify_float_arm_neon, %function
amplify_float_arm_neon:
	cmp		SIZE,	#0
	bxeq		lr
#ifdef __ARM_PCS
	vmov		s0,	r3	@ softfp
#endif
	pld		[SRC,	#64]
	vld1.f32	{d16-d17},	[SRC,:128]!
	subs		SIZE,	SIZE,	#16
	vmul.f32	d16,	d16,	d0[0]
	vmul.f32	d17,	d17,	d0[0]
	blo		5f
	pld		[SRC,	#64]
	vld1.f32	{d18-d19},	[SRC,:128]!
	subs		SIZE,	SIZE,	#16
	vmul.f32	d18,	d18,	d0[0]
	vmul.f32	d19,	d19,	d0[0]
	blo		2f
1:	@ main loop starts
	pld		[SRC,	#64]
	vld1.f32	{d20-d21},	[SRC,:128]!
	subs		SIZE,	SIZE,	#16
	vmul.f32	d20,	d20,	d0[0]
	vmul.f32	d21,	d21,	d0[0]
	vst1.f32	{d16-d17},	[DST,:128]!
	blo		3f
	pld		[SRC,	#64]
	vld1.f32	{d16-d17},	[SRC,:128]!
	subs		SIZE,	SIZE,	#16
	vmul.f32	d16,	d16,	d0[0]
	vmul.f32	d17,	d17,	d0[0]
	vst1.f32	{d18-d19},	[DST,:128]!
	blo		4f
	pld		[SRC,	#64]
	vld1.f32	{d18-d19},	[SRC,:128]!
	subs		SIZE,	SIZE,	#16
	vmul.f32	d18,	d18,	d0[0]
	vmul.f32	d19,	d19,	d0[0]
	vst1.f32	{d20-d21},	[DST,:128]!
	bhi		1b
	@ main loop ends
2:	vst1.f32	{d16-d17},	[DST,:128]!
	vst1.f32	{d18-d19},	[DST,:128]!
	bx		lr
3:	vst1.f32	{d18-d19},	[DST,:128]!
	vst1.f32	{d20-d21},	[DST,:128]!
	bx		lr
4:	vst1.f32	{d20-d21},	[DST,:128]!
5:	vst1.f32	{d16-d17},	[DST,:128]!
	bx		lr
