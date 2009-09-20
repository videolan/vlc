 @****************************************************************************
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
 @ along with this program; if not, write to the Free Software
 @ Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 @****************************************************************************/

	.fpu neon
	.text

#define O1	r0
#define	O2	r1
#define	PITCH	r2
#define	HEIGHT	r3
#define	Y1	r4
#define	Y2	r5
#define	U	r6
#define	V	r7
#define	END_O1	r8

	.align
	.global i420_uyvy_neon
	.type	i420_uyvy_neon, %function
i420_uyvy_neon:
	push		{r4-r8}
	add		r8,	pc,	#(indexes+64-.-8)
	b		i420_pack_neon

	.global i420_yuyv_neon
	.type	i420_yuyv_neon, %function
i420_yuyv_neon:
	push		{r4-r8}
	add		r8,	pc,	#(indexes-.-8)
	.hidden	i420_pack_neon
i420_pack_neon:
	vld1.u8		{d24-d27},	[r8]!
	ldmia		r1,	{r4, r6, r7}
	vld1.u8		{d28-d31},	[r8]
	add		O2,	O1,	PITCH, lsl #1
	add		Y2,	Y1,	PITCH
1:
	mov		END_O1,	O2
2:
	vld1.u8		{d0-d1},	[Y1,:128]!
	vld1.u8		{d2},		[U,:64]!
	vld1.u8		{d3},		[V,:64]!
	vld1.u8		{d4-d5},	[Y2,:128]!
	vtbl.u8		d16,	{d0-d3},	d24
	vtbl.u8		d17,	{d0-d3},	d25
	vtbl.u8		d18,	{d0-d3},	d26
	vtbl.u8		d19,	{d0-d3},	d27
	vtbl.u8		d20,	{d2-d5},	d28
	vtbl.u8		d21,	{d2-d5},	d29
	vtbl.u8		d22,	{d2-d5},	d30
	vtbl.u8		d23,	{d2-d5},	d31
	vst1.u8		{d16-d19},	[O1,:128]!
	vst1.u8		{d20-d23},	[O2,:128]!

	cmp		O1,	END_O1
	bne		2b

	sub		HEIGHT,	#2
	mov		O1,	O2
	add		O2,	PITCH,	lsl #1
	mov		Y1,	Y2
	add		Y2,	PITCH

	cmp		HEIGHT,	#0
	bne		1b

	pop		{r4-r8}
	bx		lr

	.hidden indexes
indexes:
	@ YUYV1
	.byte	0x00, 0x10, 0x01, 0x18, 0x02, 0x11, 0x03, 0x19
	.byte	0x04, 0x12, 0x05, 0x1A, 0x06, 0x13, 0x07, 0x1B
	.byte	0x08, 0x14, 0x09, 0x1C, 0x0A, 0x15, 0x0B, 0x1D
	.byte	0x0C, 0x16, 0x0D, 0x1E, 0x0E, 0x17, 0x0F, 0x1F
	@ YUYV2
	.byte	0x10, 0x00, 0x11, 0x08, 0x12, 0x01, 0x13, 0x09
	.byte	0x14, 0x02, 0x15, 0x0A, 0x16, 0x03, 0x17, 0x0B
	.byte	0x18, 0x04, 0x19, 0x0C, 0x1A, 0x05, 0x1B, 0x0D
	.byte	0x1C, 0x06, 0x1D, 0x0E, 0x1E, 0x07, 0x1F, 0x0F
	@ UYVY1
	.byte	0x10, 0x00, 0x18, 0x01, 0x11, 0x02, 0x19, 0x03
	.byte	0x12, 0x04, 0x1A, 0x05, 0x13, 0x06, 0x1B, 0x07
	.byte	0x14, 0x08, 0x1C, 0x09, 0x15, 0x0A, 0x1D, 0x0B
	.byte	0x16, 0x0C, 0x1E, 0x0D, 0x17, 0x0E, 0x1F, 0x0F
	@ UYVY2
	.byte	0x00, 0x10, 0x08, 0x11, 0x01, 0x12, 0x09, 0x13
	.byte	0x02, 0x14, 0x0A, 0x15, 0x03, 0x16, 0x0B, 0x17
	.byte	0x04, 0x18, 0x0C, 0x19, 0x05, 0x1A, 0x0D, 0x1B
	.byte	0x06, 0x1C, 0x0E, 0x1D, 0x07, 0x1E, 0x0F, 0x1F
