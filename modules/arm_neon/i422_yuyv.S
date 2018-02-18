 @*****************************************************************************
 @ i422_yuyv.S : ARM NEONv1 I422 to YUYV chroma conversion
 @*****************************************************************************
 @ Copyright (C) 2011 Rémi Denis-Courmont
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

#include "asm.S"

	.syntax unified
#if HAVE_AS_FPU_DIRECTIVE
	.fpu	neon
#endif
	.text

#define O	r0
#define OPAD	r1
#define WIDTH	r2
#define HEIGHT	r3
#define Y	r4
#define U	r5
#define V	r6
#define COUNT	ip
#define YPAD	lr

	.align 2
function i422_yuyv_neon
	push		{r4-r6,lr}
	ldmia		r1,	{Y, U, V, YPAD}
	ldmia		r0,	{O, OPAD}
	cmp		HEIGHT,	#0
	sub		OPAD,	OPAD,	WIDTH,	lsl #1
	sub		YPAD,	YPAD,	WIDTH
1:
	movsgt		COUNT,	WIDTH
	pople		{r4-r6,pc}
2:
	pld		[U, #64]
	vld1.u8		{d2},		[U,:64]!
	pld		[V, #64]
	vld1.u8		{d3},		[V,:64]!
	pld		[Y, #64]
	vzip.u8		d2,	d3
	subs		COUNT,	COUNT,	#16
	vld1.u8		{q0},		[Y,:128]!
	vzip.u8		q0,	q1
	@ TODO: unroll (1 cycle stall)
	vst1.u8		{q0-q1},	[O,:128]!
	bgt		2b

	subs		HEIGHT,	#1
	add		U,	U,	YPAD,	lsr #1
	add		V,	V,	YPAD,	lsr #1
	add		Y,	Y,	YPAD
	add		O,	O,	OPAD
	b		1b

function i422_uyvy_neon
	push		{r4-r6,lr}
	ldmia		r1,	{Y, U, V, YPAD}
	ldmia		r0,	{O, OPAD}
	cmp		HEIGHT,	#0
	sub		OPAD,	OPAD,	WIDTH,	lsl #1
	sub		YPAD,	YPAD,	WIDTH
1:
	movsgt		COUNT,	WIDTH
	pople		{r4-r6,pc}
2:
	pld		[U, #64]
	vld1.u8		{d0},		[U,:64]!
	pld		[V, #64]
	vld1.u8		{d1},		[V,:64]!
	pld		[Y, #64]
	vzip.u8		d0,	d1
	subs		COUNT,	COUNT,	#16
	vld1.u8		{q1},		[Y,:128]!
	vzip.u8		q0,	q1
	vst1.u8		{q0-q1},	[O,:128]!
	bgt		2b

	subs		HEIGHT,	#1
	add		U,	U,	YPAD,	lsr #1
	add		V,	V,	YPAD,	lsr #1
	add		Y,	Y,	YPAD
	add		O,	O,	OPAD
	b		1b
