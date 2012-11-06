 @*****************************************************************************
 @ simple_channel_mixer.S : ARM NEON channel mixer
 @*****************************************************************************
 @ Copyright (C) 2012 David Geldreich <david.geldreich@free.fr>
 @                    Sébastien Toque
 @
 @ This program is free software; you can redistribute it and/or modify it
 @ under the terms of the GNU Lesser General Public License as published by
 @ the Free Software Foundation; either version 2.1 of the License, or
 @ (at your option) any later version.
 @
 @ This program is distributed in the hope that it will be useful,
 @ but WITHOUT ANY WARRANTY; without even the implied warranty of
 @ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 @ GNU Lesser General Public License for more details.
 @
 @ You should have received a copy of the GNU Lesser General Public License
 @ along with this program; if not, write to the Free Software Foundation,
 @ Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 @****************************************************************************/

	.fpu neon
	.text
	.align 2

#define DST		r0
#define SRC		r1
#define NUM		r2
#define LFE		r3
#define COEFF	r4

coeff_7to2:
	.float 0.5
	.float 0.5
	.float 0.25
	.float 0.25
	.global convert_7to2_neon_asm
	.type	convert_7to2_neon_asm, %function
convert_7to2_neon_asm:
	push {r4,lr}

	adr COEFF, coeff_7to2
	vld1.32 {q0},[COEFF]
0:                                                @ use local label
	vld1.32 {q2},[SRC]!                           @ load 0,1,2,3
	vmul.f32 q2,q2,q0                             @ 0.5*src[0] 0.5*src[1] 0.25*src[2] 0.25*src[3]
	vld1.32 {d6},[SRC]!                           @ load 4,5
	vmul.f32 d6,d6,d1                             @ 0.25*src[4] 0.25*src[5]
	vadd.f32 d4,d4,d5                             @ 0.5*src[0] + 0.25*src[2]
                                                  @ 0.5*src[1] + 0.25*src[3]
	vadd.f32 d4,d4,d6                             @ 0.5*src[0] + 0.25*src[2] + 0.25*src[4]
                                                  @ 0.5*src[1] + 0.25*src[3] + 0.25*src[5]
	flds s14,[SRC]                                @ load 6
	vdup.32 d7,d7[0]
	teq LFE,#0
	ite eq
	addeq SRC,SRC,#4
	addne SRC,SRC,#8                              @ skip the lfe channel
	vadd.f32 d4,d4,d7                             @ 0.5*src[0] + 0.25*src[2] + 0.25*src[4] + src[6]
                                                  @ 0.5*src[1] + 0.25*src[3] + 0.25*src[5] + src[6]
	vst1.32 d4, [DST]!
	subs NUM,NUM,#1
	bne 0b

	pop {r4,pc}


coeff_5to2:
	.float 0.5
	.float 0.5
	.float 0.33
	.float 0.33
	.global convert_5to2_neon_asm
	.type	convert_5to2_neon_asm, %function
convert_5to2_neon_asm:
	push {r4,lr}

	adr COEFF, coeff_5to2
	vld1.32 {q0},[COEFF]                          @ load constants
0:                                                @ use local label
	vld1.32 {q1},[SRC]!                           @ load 0,1,2,3
	flds s8,[SRC]                                 @ load 4
	vdup.32 d4,d4[0]
	teq LFE,#0
	ite eq
	addeq SRC,SRC,#4
	addne SRC,SRC,#8                              @ skip the lfe channel
	vmul.f32 q1,q1,q0                             @ 0.5*src[0] 0.5*src[1] 0.33*src[2] 0.33*src[3]/3
	vadd.f32 d2,d2,d3                             @ 0.5*src[0] + 0.33*src[2]
                                                  @ 0.5*src[1] + 0.33*src[3]
	vadd.f32 d2,d2,d4                             @ 0.5*src[0] + 0.33*src[2] + src[4]
                                                  @ 0.5*src[1] + 0.33*src[3] + src[4]
	vst1.32 d2,[DST]!
	subs NUM,NUM,#1
	bne 0b

	pop {r4,pc}


coeff_4to2:
	.float 0.5
	.float 0.5
	.global convert_4to2_neon_asm
	.type	convert_4to2_neon_asm, %function
convert_4to2_neon_asm:
	push {r4,lr}

	adr COEFF, coeff_4to2
	vld1.32 {d0},[COEFF]                          @ load constants
0:                                                @ use local label
	vld1.32 {q1},[SRC]!
	vmul.f32 d2,d2,d0                             @ 0.5*src[0] 0.5*src[1]
	vdup.32 d4,d3[0]                              @ dup src[2]
	vdup.32 d3,d3[1]                              @ dup src[3]
	vadd.f32 d2,d2,d3                             @ +src[3]
	vadd.f32 d2,d2,d4                             @ +src[2]
	vst1.32 d2,[DST]!
	subs NUM,NUM,#1
	bne 0b

	pop {r4,pc}


coeff_3to2:
	.float 0.5
	.float 0.5
	.global convert_3to2_neon_asm
	.type	convert_3to2_neon_asm, %function
convert_3to2_neon_asm:
	push {r4,lr}

	adr COEFF, coeff_3to2
	vld1.32 {d0},[COEFF]                          @ load constants
0:                                                @ use local label
	vld1.32 {d1},[SRC]!                           @ load 0,1
	flds s4,[SRC]                                 @ load 2
	vdup.32 d2,d2[0]
	teq LFE,#0
	ite eq
	addeq SRC,SRC,#4
	addne SRC,SRC,#8                              @ skip the lfe channel
	vmul.f32 d1,d1,d0                             @ 0.5*src[0] 0.5*src[1]
	vadd.f32 d1,d1,d2                             @ 0.5*src[0] + src[2]
                                                  @ 0.5*src[1] + src[2]
	vst1.32 d1,[DST]!
	subs NUM,NUM,#1
	bne 0b

	pop {r4,pc}


coeff_7to1:
	.float 0.25
	.float 0.25
	.float 0.125
	.float 0.125
	.global convert_7to1_neon_asm
	.type	convert_7to1_neon_asm, %function
convert_7to1_neon_asm:
	push {r4,lr}

	adr COEFF, coeff_7to1
	vld1.32 {q0},[COEFF]
0:                                                @ use local label
	vld1.32 {q1},[SRC]!                           @ load 0,1,2,3
	vmul.f32 q1,q1,q0                             @ 0.25*src[0] 0.25*src[1] 0.125*src[2] 0.125*src[3]
	vld1.32 {d4},[SRC]!                           @ load 4,5
	vmul.f32 d4,d4,d1                             @ 0.125*src[4] 0.125*src[5]
	vadd.f32 d2,d2,d3
	vadd.f32 d2,d2,d4
	flds s10,[SRC]                                @ load 6
	teq LFE,#0
	ite eq
	addeq SRC,SRC,#4
	addne SRC,SRC,#8                              @ skip the lfe channel
	vadd.f32 s4,s4,s5
	vadd.f32 s4,s4,s10
	fsts s4,[DST]
	add DST,DST,#4
	subs NUM,NUM,#1
	bne 0b

	pop {r4,pc}


coeff_5to1:
	.float 0.25
	.float 0.25
	.float 0.16666667
	.float 0.16666667
	.global convert_5to1_neon_asm
	.type	convert_5to1_neon_asm, %function
convert_5to1_neon_asm:
	push {r4,lr}

	adr COEFF, coeff_5to1
	vld1.32 {q0},[COEFF]
0:                                                @ use local label
	vld1.32 {q1},[SRC]!                           @ load 0,1,2,3
	vmul.f32 q1,q1,q0                             @ 0.25*src[0] 0.25*src[1] src[2]/6 src[3]/6
	vadd.f32 d2,d2,d3
	flds s10,[SRC]                                @ load 4
	teq LFE,#0
	ite eq
	addeq SRC,SRC,#4
	addne SRC,SRC,#8                              @ skip the lfe channel
	vadd.f32 s4,s4,s5
	vadd.f32 s4,s4,s10
	fsts s4,[DST]
	add DST,DST,#4
	subs NUM,NUM,#1
	bne 0b

	pop {r4,pc}


coeff_7to4:
	.float 0.5
	.float 0.5
	.float 0.16666667
	.float 0.16666667
	.global convert_7to4_neon_asm
	.type	convert_7to4_neon_asm, %function
convert_7to4_neon_asm:
	push {r4,lr}

	adr COEFF, coeff_7to4
	vld1.32 {q0},[COEFF]
0:                                                @ use local label
	vld1.32 {q1},[SRC]!                           @ load 0,1,2,3
	vmul.f32 q1,q1,q0                             @ 0.5*src[0] 0.5*src[1] src[2]/6 src[3]/6
	vld1.32 {d5},[SRC]!                           @ load 4,5
	flds s14,[SRC]                                @ load 6
	vadd.f32 d2,d2,d3                             @ 0.5*src[0] + src[2]/6
                                                  @ 0.5*src[1] + src[3]/6
	vdup.32 d4,d7[0]                              @ so q2 : src[6] src[6] src[4] src[5]
	vadd.f32 q2,q2,q1                             @ src[6] + 0.5*src[0] + src[2]/6
                                                  @ src[6] + 0.5*src[1] + src[3]/6
                                                  @ src[4] + src[2]/6
                                                  @ src[5] + src[3]/6
	teq LFE,#0
	ite eq
	addeq SRC,SRC,#4
	addne SRC,SRC,#8                              @ skip the lfe channel
	vst1.32 {q2}, [DST]!
	subs NUM,NUM,#1
	bne 0b

	pop {r4,pc}


coeff_5to4:
	.float 0.5
	.float 0.5
	.global convert_5to4_neon_asm
	.type	convert_5to4_neon_asm, %function
convert_5to4_neon_asm:
	push {r4,lr}

	adr COEFF, coeff_5to4
	vld1.32 {d0},[COEFF]
0:                                                @ use local label
	vld1.32 {q1},[SRC]!                           @ load 0,1,2,3
	vmul.f32 d2,d2,d0                             @ 0.5*src[0] 0.5*src[1]
	flds s8,[SRC]                                 @ load 4
	vdup.32 d4,d4[0]
	vadd.f32 d2,d2,d4                             @ 0.5*src[0] + src[4]
                                                  @ 0.5*src[1] + src[4]
                                                  @ src[2]
                                                  @ src[3]
	teq LFE,#0
	ite eq
	addeq SRC,SRC,#4
	addne SRC,SRC,#8                              @ skip the lfe channel
	vst1.32 {q1}, [DST]!
	subs NUM,NUM,#1
	bne 0b

	pop {r4,pc}
