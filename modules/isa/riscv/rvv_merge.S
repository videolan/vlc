/******************************************************************************
 * rvv_merge.S: RISC-V Vector mean
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

	.option arch, +v
	.text
	.align	2

	.globl	merge8_rvv
	.type	merge8_rvv, %function
merge8_rvv:
	csrwi	vxrm, 0
1:	vsetvli	t0, a3, e8, m8, ta, ma
	vle8.v	v16, (a1)
	add	a1, a1, t0
	vle8.v	v24, (a2)
	add	a2, a2, t0
	vaaddu.vv	v16, v16, v24
	sub	a3, a3, t0
	vse8.v	v16, (a0)
	add	a0, a0, t0
	bnez	a3, 1b
	ret
	.size	merge8_rvv, . - merge8_rvv

	.globl	merge16_rvv
	.type	merge16_rvv, %function
merge16_rvv:
	csrwi	vxrm, 0
	srli	a3, a3, 1
1:	vsetvli	t0, a3, e16, m8, ta, ma
	slli	t1, t0, 1
	vle16.v	v16, (a1)
	add	a1, a1, t1
	vle16.v	v24, (a2)
	add	a2, a2, t1
	vaaddu.vv	v16, v16, v24
	sub	a3, a3, t0
	vse16.v	v16, (a0)
	add	a0, a0, t1
	bnez	a3, 1b
	ret
	.size	merge16_rvv, . - merge16_rvv

