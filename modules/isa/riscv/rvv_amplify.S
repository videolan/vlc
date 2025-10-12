/*****************************************************************************
 * amplify.S: RISC-V V software amplification
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

	.option	arch, +b, +v

func	rvv_amplify_f32
	lpad	0
	srli	a2, a2, 2
#if defined (__riscv_float_abi_soft)
	fmv.w.x	fa0, a3
#endif
1:
	vsetvli	t0, a2, e32, m8, ta, ma
	vle32.v	v16, (a1)
	sh2add	a1, t0, a1
	vfmul.vf	v16, v16, fa0
	sub	a2, a2, t0
	vse32.v	v16, (a0)
	sh2add	a0, t0, a0
	bnez	a2, 1b

	ret
endfunc

func	rvv_amplify_f64
	lpad	0
	srli	a2, a2, 3
#if defined (__riscv_float_abi_soft) || defined (__riscv_float_abi_single)
	fmv.d.x	fa0, a3
#endif
1:
	vsetvli	t0, a2, e64, m8, ta, ma
	vle64.v	v16, (a1)
	sh3add	a1, t0, a1
	vfmul.vf	v16, v16, fa0
	sub	a2, a2, t0
	vse64.v	v16, (a0)
	sh3add	a0, t0, a0
	bnez	a2, 1b

	ret
endfunc

func	rvv_amplify_i16
	lpad	0
	srli	a2, a2, 1
1:
	vsetvli	t0, a2, e16, m8, ta, ma
	vle16.v	v16, (a1)
	sh1add	a1, t0, a1
	vmulhsu.vx	v16, v16, a3
	sub	a2, a2, t0
	vse16.v	v16, (a0)
	sh1add	a0, t0, a0
	bnez	a2, 1b

	ret
endfunc

func	rvv_amplify_i32
	lpad	0
	srli	a2, a2, 2
1:
	vsetvli	t0, a2, e32, m8, ta, ma
	vle32.v	v16, (a1)
	sh2add	a1, t0, a1
	vmulhsu.vx	v16, v16, a3
	sub	a2, a2, t0
	vse32.v	v16, (a0)
	sh2add	a0, t0, a0
	bnez	a2, 1b

	ret
endfunc

func	rvv_amplify_u8
	lpad	0
1:
	vsetvli	t0, a2, e8, m8, ta, ma
	vle8.v	v16, (a1)
	add	a1, a1, t0
	vmulhu.vx	v16, v16, a3
	sub	a2, a2, t0
	vse8.v	v16, (a0)
	add	a0, a0, t0
	bnez	a2, 1b

	ret
endfunc
