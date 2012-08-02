 @*****************************************************************************
 @ yuyv_i422.S : ARM NEONv1 packed to planar YUV422 conversion
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

	.syntax unified
	.fpu neon
	.text

#define I	r0
#define IPAD	r1
#define WIDTH	r2
#define HEIGHT	r3
#define Y	r4
#define U	r5
#define V	r6
#define COUNT	ip
#define YPAD	lr

	.align 2
	.global yuyv_i422_neon
	.type	yuyv_i422_neon, %function
yuyv_i422_neon:
	push		{r4-r6,lr}
	ldmia		r0,	{Y, U, V, YPAD}
	ldmia		r1,	{I, IPAD}
	cmp		HEIGHT,	#0
	sub		YPAD,	YPAD,	WIDTH
	sub		IPAD,	IPAD,	WIDTH,	lsl #1
1:
	movsgt		COUNT,	WIDTH
	pople		{r4-r6,pc}
2:
	pld		[I, #64]
	subs		COUNT,	COUNT,	#16
	vld1.u8		{q0-q1},	[I,:128]!
	vuzp.u8		q0,	q1
	@ TODO: unroll (1 cycle stall)
	vuzp.u8		d2,	d3
	vst1.u8		{q0},		[Y,:128]!
	vst1.u8		{d2},		[U,:64]!
	vst1.u8		{d3},		[V,:64]!
	bgt		2b

	subs		HEIGHT,	#1
	add		I,	I,	IPAD
	add		Y,	Y,	YPAD
	add		U,	U,	YPAD,	lsr #1
	add		V,	V,	YPAD,	lsr #1
	b		1b

	.global uyvy_i422_neon
	.type	uyvy_i422_neon, %function
uyvy_i422_neon:
	push		{r4-r6,lr}
	ldmia		r0,	{Y, U, V, YPAD}
	ldmia		r1,	{I, IPAD}
	cmp		HEIGHT,	#0
	sub		YPAD,	YPAD,	WIDTH
	sub		IPAD,	IPAD,	WIDTH,	lsl #1
1:
	movsgt		COUNT,	WIDTH
	pople		{r4-r6,pc}
2:
	pld		[I, #64]
	subs		COUNT,	COUNT,	#16
	vld1.u8		{q0-q1},	[I,:128]!
	vuzp.u8		q0,	q1
	vuzp.u8		d0,	d1
	vst1.u8		{q1},		[Y,:128]!
	vst1.u8		{d0},		[U,:64]!
	vst1.u8		{d1},		[V,:64]!
	bgt		2b

	subs		HEIGHT,	#1
	add		I,	I,	IPAD
	add		Y,	Y,	YPAD
	add		U,	U,	YPAD,	lsr #1
	add		V,	V,	YPAD,	lsr #1
	b		1b
