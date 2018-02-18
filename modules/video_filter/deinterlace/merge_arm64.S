 //*****************************************************************************
 // merge_arm64.S : ARM64 NEON mean
 //*****************************************************************************
 // Copyright (C) 2009-2012 Rémi Denis-Courmont
 // Copyright (C) 2016-	   Janne Grunau
 //
 // This program is free software; you can redistribute it and/or modify
 // it under the terms of the GNU Lesser General Public License as published by
 // the Free Software Foundation; either version 2.1 of the License, or
 // (at your option) any later version.
 //
 // This program is distributed in the hope that it will be useful,
 // but WITHOUT ANY WARRANTY; without even the implied warranty of
 // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 // GNU Lesser General Public License for more details.
 //
 // You should have received a copy of the GNU Lesser General Public License
 // along with this program; if not, write to the Free Software Foundation,
 // Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 //****************************************************************************/

#include "../../arm_neon/asm.S"

	.text

#define	DEST	x0
#define	SRC1	x1
#define	SRC2	x2
#define	SIZE	x3

	.align 2
	// NOTE: Offset and pitch must be multiple of 16-bytes in VLC.
function merge8_arm64_neon
	ands		x5, SIZE, #~63
	b.eq		2f
	mov		x10, #64
	add		x11, SRC1, #32
        add             x12, SRC2, #32
1:
	ld1		{v0.16b,v1.16b}, [SRC1], x10
	ld1		{v4.16b,v5.16b}, [SRC2], x10
	ld1		{v2.16b,v3.16b}, [x11], x10
	uhadd		v0.16b, v0.16b, v4.16b
	ld1		{v6.16b,v7.16b}, [x12], x10
	subs		x5,  x5,  #64
	uhadd		v1.16b, v1.16b, v5.16b
	uhadd		v2.16b, v2.16b, v6.16b
	uhadd		v3.16b, v3.16b, v7.16b
	st1		{v0.16b,v1.16b}, [DEST], #32
	st1		{v2.16b,v3.16b}, [DEST], #32
	b.gt		1b
2:
	tbz		SIZE, #5,  3f
	ld1		{v0.16b,v1.16b}, [SRC1], #32
	ld1		{v4.16b,v5.16b}, [SRC2], #32
	uhadd		v0.16b, v0.16b, v4.16b
	uhadd		v1.16b, v1.16b, v5.16b
	st1		{v0.16b,v1.16b}, [DEST], #32
3:
	tbz		SIZE, #4,  4f
	ld1		{v0.16b},  [SRC1]
	ld1		{v4.16b},  [SRC2]
	uhadd		v0.16b, v0.16b, v4.16b
	st1		{v0.16b},  [DEST]
4:
	ret

	.align 2
function merge16_arm64_neon
	ands		x5, SIZE, #~63
	b.eq		2f
1:
	ld1		{v0.8h,v1.8h}, [SRC1], #32
	ld1		{v4.8h,v5.8h}, [SRC2], #32
	ld1		{v2.8h,v3.8h}, [SRC1], #32
	uhadd		v0.8h,  v0.8h,  v4.8h
	ld1		{v6.8h,v7.8h}, [SRC2], #32
	uhadd		v1.8h,  v1.8h,  v5.8h
	uhadd		v2.8h,  v2.8h,  v6.8h
	uhadd		v3.8h,  v3.8h,  v7.8h
	st1		{v0.8h,v1.8h}, [DEST], #32
	st1		{v2.8h,v3.8h}, [DEST], #32
	subs		x5,  x5,  #64
	b.gt		1b
2:
	tbz		SIZE, #5, 3f
	ld1		{v0.8h,v1.8h}, [SRC1], #32
	ld1		{v4.8h,v5.8h}, [SRC2], #32
	uhadd		v0.8h,  v0.8h,  v4.8h
	uhadd		v1.8h,  v1.8h,  v5.8h
	st1		{v0.8h,v1.8h}, [DEST], #32
3:
	tbz	        SIZE, #4,  4f
	ld1		{v0.8h},  [SRC1]
	ld1		{v4.8h},  [SRC2]
	uhadd		v0.8h,  v0.8h,  v4.8h
	st1		{v0.8h},  [DEST]
4:
	ret
