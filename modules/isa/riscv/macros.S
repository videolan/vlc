/*****************************************************************************
 * macros.S: RISC-V common assembler macros
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

.macro func sym
	.text
	.global	\sym
	.hidden	\sym
	.type	\sym, %function
	.align	2
\sym:

	.macro	endfunc
		.size	\sym, . - \sym
		.previous
		.purgem	endfunc
	.endm
.endm

#if !defined (__riscv_zicfilp)
.macro	lpad	lpl
	auipc	zero, \lpl
.endm
#endif
