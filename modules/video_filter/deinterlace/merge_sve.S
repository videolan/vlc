/******************************************************************************
 * merge_sve.S : ARM SVE mean
 ******************************************************************************
 * Copyright (C) 2018 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "../../arm_neon/asm.S"

	.arch armv8-a+sve

	/* TODO: prefetch, unroll */

	.text
function merge8_arm_sve
	bti	c
	mov	x4, #0
	b	2f
1:	ld1b	{z0.h}, p0/z, [x1, x4]
	ld1b	{z1.h}, p0/z, [x2, x4]
	add	z0.h, z0.h, z1.h
	lsr	z0.h, z0.h, #1
	st1b	{z0.h}, p0, [x0, x4]
	inch	x4
2:	whilelt	p0.h, x4, x3
	b.first	1b
	ret

function merge16_arm_sve
	bti	c
	mov	x4, #0
	b	2f
1:	ld1h	{z0.s}, p0/z, [x1, x4, lsl #1]
	ld1h	{z1.s}, p0/z, [x2, x4, lsl #1]
	add	z0.s, z0.s, z1.s
	lsr	z0.s, z0.s, #1
	st1h	{z0.s}, p0, [x0, x4, lsl #1]
	incw	x4
2:	whilelt	p0.s, x4, x3
	b.first	1b
	ret
