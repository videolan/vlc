 @*****************************************************************************
 @ i420_yuyv_neon.S : ARM NEONv1 I420 to YUYV chroma conversion
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

#define O1	r0
#define	O2	r1
#define	PITCH	r2
#define	S_OFF	r3
#define	Y1	r4
#define	Y2	r5
#define	U	r6
#define	V	r7
#define	HEIGHT	r8
#define	END_O1	r12

	.align
	.global i420_yuyv_neon
	.type	i420_yuyv_neon, %function
i420_yuyv_neon:
	push		{r4-r8, lr}
	ldr		HEIGHT, [sp, #(4*6)]
	ldmia		r1,	{Y1, U, V}
	add		O2,	O1,	PITCH, lsl #1
	add		Y2,	Y1,	PITCH
	add		Y2,	S_OFF
1:
	mov		END_O1,	O2
	pld		[Y2]
2:
	pld		[U, #64]
	vld1.u8		{d2},		[U,:64]!
	pld		[V, #64]
	vld1.u8		{d3},		[V,:64]!
	pld		[Y1, #64]
	vzip.u8		d2,	d3
	vld1.u8		{q0},		[Y1,:128]!
	pld		[Y2, #64]
	vmov		q3,	q1
	vzip.u8		q0,	q1
	vld1.u8		{q2},		[Y2,:128]!
	vzip.u8		q2,	q3
	vst1.u8		{q0-q1},	[O1,:128]!
	vst1.u8		{q2-q3},	[O2,:128]!

	cmp		O1,	END_O1
	bne		2b

	sub		HEIGHT,	#2
	mov		O1,	O2
	add		O2,	PITCH,	lsl #1
	add		Y2,	S_OFF
	mov		Y1,	Y2
	add		Y2,	PITCH
	add		Y2,	S_OFF
	add		U,	S_OFF,	lsr #1
	add		V,	S_OFF,	lsr #1

	cmp		HEIGHT,	#0
	bne		1b

	pop		{r4-r8, pc}

	.global i420_uyvy_neon
	.type	i420_uyvy_neon, %function
i420_uyvy_neon:
	push		{r4-r8, lr}
	ldr		HEIGHT, [sp, #(4*6)]
	ldmia		r1,	{Y1, U, V}
	add		O2,	O1,	PITCH, lsl #1
	add		Y2,	Y1,	PITCH
	add		Y2,	S_OFF
1:
	mov		END_O1,	O2
2:
	pld		[U, #64]
	vld1.u8		{d0},		[U,:64]!
	pld		[V, #64]
	vld1.u8		{d1},		[V,:64]!
	pld		[Y1, #64]
	vzip.u8		d0,	d1
	vld1.u8		{q1},		[Y1,:128]!
	pld		[Y2, #64]
	vmov		q2,	q0
	vzip.u8		q0,	q1
	vld1.u8		{q3},		[Y2,:128]!
	vzip.u8		q2,	q3
	vst1.u8		{q0-q1},	[O1,:128]!
	vst1.u8		{q2-q3},	[O2,:128]!

	cmp		O1,	END_O1
	bne		2b

	sub		HEIGHT,	#2
	mov		O1,	O2
	add		O2,	PITCH,	lsl #1
	add		Y2,	S_OFF
	mov		Y1,	Y2
	add		Y2,	PITCH
	add		Y2,	S_OFF
	add		U,	S_OFF,	lsr #1
	add		V,	S_OFF,	lsr #1

	cmp		HEIGHT,	#0
	bne		1b

	pop		{r4-r8, pc}
