 @*****************************************************************************
 @ neon_s32_s16.S : ARM NEONv1 fi32 to s16n audio sample conversion
 @*****************************************************************************
 @ Copyright (C) 2009 Rémi Denis-Courmont
 @
 @ This program is free software; you can redistribute it and/or modify
 @ it under the terms of the GNU General Public License as published by
 @ the Free Software Foundation; either version 2 of the License, or
 @ (at your option) any later version.
 @
 @ This program is distributed in the hope that it will be useful,
 @ but WITHOUT ANY WARRANTY; without even the implied warranty of
 @ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 @ GNU General Public License for more details.
 @
 @ You should have received a copy of the GNU General Public License
 @ along with this program; if not, write to the Free Software Foundation,
 @ Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 @****************************************************************************/

	.fpu neon
	.text

#define	OUT	r0
#define	IN	r1
#define	N	r2
#define	BUF	r3
#define HALF	ip

	.align
	.global s32_s16_neon
	.type	s32_s16_neon, %function
	@ Converts fixed-point 32-bits to signed 16-bits
	@ Input and output must be on 128-bits boundary
s32_s16_neon:
	pld		[IN]
2:
	cmp		N,	#8
	blt		s32_s16_neon_unaligned
	vld1.s32	{q8-q9},	[IN,:128]!

3:	@ Main loop
	pld		[IN, #64]
	sub		N,	#8
	vqrshrn.s32	d16,	q8,	#13
	vqrshrn.s32	d17,	q9,	#13
	cmp		N,	#8
	blt		4f
	vld1.s32	{q10-q11},	[IN,:128]!
	sub		N,	#8
	vqrshrn.s32	d18,	q10,	#13
	vqrshrn.s32	d19,	q11,	#13
	cmp		N,	#8
	blt		5f
	vld1.s32	{q12-q13},	[IN,:128]!
	sub		N,	#8
	vqrshrn.s32	d20,	q12,	#13
	vqrshrn.s32	d21,	q13,	#13
	vst1.s16	{d16-d19},	[OUT,:128]!
	cmp		N,	#8
	blt		6f
	vld1.s32	{q8-q9},	[IN,:128]!
	vst1.s16	{d20-d21},	[OUT,:128]!
	b		3b
4:
	vst1.s16	{d16-d17},	[OUT,:128]!
	b		7f
5:
	vst1.s16	{d16-d19},	[OUT,:128]!
	b		7f
6:
	vst1.s16	{d20-d21},	[OUT,:128]!
7:
	cmp		N,	#4
	blt		s32_s16_neon_unaligned
	vld1.s32	{q8},		[IN,:128]!
	sub		N,	#4
	vqrshrn.s32	d16,	q8,	#13
	vst1.s16	{d16},		[OUT,:64]!

	@ Fall through for last 0-3 samples

	.global	s32_s16_neon_unaligned
	.type	s32_s16_neon_unaligned, %function
	@ Converts fixed-point 32-bits to signed 16-bits
	@ Input must be on 32-bits boundary, output on 16-bits
s32_s16_neon_unaligned:
	mov		HALF,	#4096
1:
	cmp		N,	#0
	bxeq		lr

	ldr		BUF,	[IN]
	add		IN,	#4
	add		OUT,	#2
	qadd		BUF,	HALF,	BUF
	sub		N,	#1
	ssat		BUF,	#16,	BUF, asr #13
	strh		BUF,	[OUT, #-2]
	b		1b
