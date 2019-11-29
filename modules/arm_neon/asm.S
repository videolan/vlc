/*
 * Copyright (c) 2018 Janne Grunau <janne-libav@jannau.net>
 * Copyright (c) 2019 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef __APPLE__
#   define EXTERN_ASM _
#else
#   define EXTERN_ASM
#endif

#if defined(__APPLE__) || defined(_WIN32)
#   define HAVE_AS_ARCH_DIRECTIVE 0
#   define HAVE_AS_FPU_DIRECTIVE  0
#else
#   define HAVE_AS_ARCH_DIRECTIVE 1
#   define HAVE_AS_FPU_DIRECTIVE  1
#endif

.macro  function name
	.globl  EXTERN_ASM\name
#ifdef __ELF__
	.type   EXTERN_ASM\name, %function
#endif
EXTERN_ASM\name:
.endm

.macro	bti	targets=
.ifb	\targets
	hint	#32
.else
.ifc	\targets, c
	hint	#34
.else
.ifc	\targets, j
	hint	#36
.else
.ifc	\targets, jc
	hint	#38
.else
.error	"invalid BTI target"
.endif
.endif
.endif
.endif
.endm
