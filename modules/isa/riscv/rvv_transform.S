/******************************************************************************
 * rvv_transform.S: rectangular transforms using RISC-V Vector Extension
 ******************************************************************************
 * Copyright (C) 2022 Rémi Denis-Courmont
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

#include "macros.S"

	.option arch, +v

	.macro transforms, bits, order
	.if	\bits - (8 << \order)
	.error	"Mismatched parameters"
	.endif

func	rvv_hflip_\bits
	// a0:out_base, a1:out_stride, a2:in_base, a3:in_stride
	// a4:width, a5:height
	lpad	0
	.if	\order
	slli	t4, a4, \order
	add	a2, a2, t4
	.else
	add	a2, a2, a4
	.endif
	li	t6, -(1 << \order)
	add	a2, a2, t6
1:
	mv	t0, a0
	mv	t2, a2
	mv	t4, a4
2:
	vsetvli	t5, t4, e\bits, m8, ta, ma
	sub	t4, t4, t5
	vlse\bits\().v	v0, (t2), t6
	.if	\order
	slli	t5, t5, \order
	.endif
	sub	t2, t2, t5
	vse\bits\().v	v0, (t0)
	add	t0, t0, t5
	bnez	t5, 2b

	add	a5, a5, -1
	add	a0, a0, a1
	add	a2, a2, a3
	bnez	a5, 1b

	ret
endfunc

func	rvv_transpose_\bits
	// a0:out_base, a1:out_stride, a2:in_base, a3:in_stride
	// a4:in_width/out_height, a5:in_height/out_width
	lpad	0
1:
	mv	t0, a0
	mv	t2, a2
	mv	t4, a4
	/* For the sake of locality, the inner loop transposes VL rows at once
	 * rather than one column. */
2:
	vsetvli	t5, a5, e\bits, m8, ta, ma
	vlse\bits\().v	v0, (t2), a3
	addi	t2, t2, (1 << \order)
	vse\bits\().v	v0, (t0)
	addi	t4, t4, -1
	add	t0, t0, a1
	bnez	t4, 2b

	sub	a5, a5, t5
	.if	\order
	slli	t5, t5, \order
	.endif
	mul	t3, a3, t5	// compute bytes in VL rows
	add	a0, a0, t5	// VL output columns done
	add	a2, a2, t3	// VL input rows done
	bnez	a5, 1b

	ret
endfunc
	.endm // transforms

	transforms	 8, 0
	transforms	16, 1
	transforms	32, 2
	transforms	64, 3
