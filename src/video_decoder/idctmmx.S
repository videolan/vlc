/*
 * the input data is tranposed and each 16 bit element in the 8x8 matrix
 * is left aligned:
 * for example in 11...1110000 format
 * If the iDCT is of I macroblock then 0.5 needs to be added to the;DC Component
 * (element[0][0] of the matrix)
 */

.data
	.align 16
	.type	 preSC,@object
preSC:  .short  16384,22725,21407,19266,16384,12873,8867,4520
        .short  22725,31521,29692,26722,22725,17855,12299,6270
        .short  21407,29692,27969,25172,21407,16819,11585,5906
        .short  19266,26722,25172,22654,19266,15137,10426,5315
        .short  16384,22725,21407,19266,16384,12873,8867,4520
        .short  12873,17855,16819,15137,25746,20228,13933,7103
        .short  17734,24598,23170,20853,17734,13933,9597,4892
        .short  18081,25080,23624,21261,18081,14206,9785,4988
	.size	 preSC,128
	.align 8
	.type	x0005000200010001,@object
	.size	x0005000200010001,8
x0005000200010001:
	.long	0x00010001,0x00050002
	.align 8
	.type	x0040000000000000,@object
	.size	x0040000000000000,8
x0040000000000000:
	.long	0, 0x00400000
	.align 8
	.type	x5a825a825a825a82,@object
	.size	x5a825a825a825a82,8
x5a825a825a825a82:
	.long	0x5a825a82, 0x5a825a82
	.align 8
	.type	x539f539f539f539f,@object
	.size	x539f539f539f539f,8
x539f539f539f539f:
	.long	0x539f539f,0x539f539f
	.align 8
	.type	x4546454645464546,@object
	.size	x4546454645464546,8
x4546454645464546:
	.long	0x45464546,0x45464546
	.align 8
	.type	x61f861f861f861f8,@object
	.size	x61f861f861f861f8,8
x61f861f861f861f8:
	.long	0x61f861f8,0x61f861f8
	.align 8
	.type	 scratch1,@object
	.size	 scratch1,8
scratch1:
	.long 0,0
	.align 8
	.type	 scratch3,@object
	.size	 scratch3,8
scratch3:
	.long 0,0
	.align 8
	.type	 scratch5,@object
	.size	 scratch5,8
scratch5:
	.long 0,0
	.align 8
	.type	 scratch7,@object
	.size	 scratch7,8
scratch7:
	.long 0,0
	.type	 x0,@object
	.size	 x0,8
x0:
	.long 0,0
	.align 8
.text
	.align 4
.globl vdec_IDCT
	.type	 vdec_IDCT,@function
vdec_IDCT:
	pushl %ebp
	movl %esp,%ebp
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %esi
	pushl %edi
	leal preSC, %ecx
	movl 12(%ebp),%esi
	movq (%esi), %mm0
	movq 8(%esi), %mm1
	movq 16(%esi), %mm2
	movq 24(%esi), %mm3
	movq 32(%esi), %mm4
	movq 40(%esi), %mm5
	movq 48(%esi), %mm6
	movq 56(%esi), %mm7
	psllw $4, %mm0
	psllw $4, %mm1
	psllw $4, %mm2
	psllw $4, %mm3
	psllw $4, %mm4
	psllw $4, %mm5
	psllw $4, %mm6
	psllw $4, %mm7
	movq %mm0,  (%esi)
	movq %mm1, 8(%esi)
	movq %mm2,16(%esi)
	movq %mm3,24(%esi)
	movq %mm4,32(%esi)
	movq %mm5,40(%esi)
	movq %mm6,48(%esi)
	movq %mm7,56(%esi)
	movq 64(%esi), %mm0
	movq 72(%esi), %mm1
	movq 80(%esi), %mm2
	movq 88(%esi), %mm3
	movq 96(%esi), %mm4
	movq 104(%esi), %mm5
	movq 112(%esi), %mm6
	movq 120(%esi), %mm7
	psllw $4, %mm0
	psllw $4, %mm1
	psllw $4, %mm2
	psllw $4, %mm3
	psllw $4, %mm4
	psllw $4, %mm5
	psllw $4, %mm6
	psllw $4, %mm7
	movq %mm0,64(%esi)
	movq %mm1,72(%esi)
	movq %mm2,80(%esi)
	movq %mm3,88(%esi)
	movq %mm4,96(%esi)
	movq %mm5,104(%esi)
	movq %mm6,112(%esi)
	movq %mm7,120(%esi)
/* column 0: even part
 * use V4, V12, V0, V8 to produce V22..V25
 */
	movq 8*12(%ecx), %mm0	/* maybe the first mul can be done together */
				/* with the dequantization in iHuff module */
	pmulhw 8*12(%esi), %mm0		/* V12 */
	movq 8*4(%ecx), %mm1
	pmulhw 8*4(%esi), %mm1		/* V4 */
	movq (%ecx), %mm3
	psraw $1, %mm0			/* t64=t66 */
	pmulhw (%esi), %mm3		/* V0 */
	movq 8*8(%ecx), %mm5		/* duplicate V4 */
	movq %mm1, %mm2			/* added 11/1/96 */
	pmulhw 8*8(%esi),%mm5		/* V8 */
	psubsw %mm0, %mm1		/* V16 */
	pmulhw x5a825a825a825a82, %mm1	/* 23170 ->V18 */
	paddsw %mm0, %mm2		/* V17 */
	movq %mm2, %mm0			/* duplicate V17 */
	psraw $1, %mm2			/* t75=t82 */
	psraw $2, %mm0			/* t72 */
	movq %mm3, %mm4			/* duplicate V0 */
	paddsw %mm5, %mm3		/* V19 */
	psubsw %mm5, %mm4		/* V20 ;mm5 free */
/* moved from the block below */
	movq 8*10(%ecx), %mm7
	psraw $1, %mm3			/* t74=t81 */
	movq %mm3, %mm6			/* duplicate t74=t81 */
	psraw $2, %mm4			/* t77=t79 */
	psubsw %mm0, %mm1		/* V21 ; mm0 free */
	paddsw %mm2, %mm3		/* V22 */
	movq %mm1, %mm5			/* duplicate V21 */
	paddsw %mm4, %mm1		/* V23 */
	movq %mm3, 8*4(%esi)		/* V22 */
	psubsw %mm5, %mm4		/* V24; mm5 free */
	movq %mm1, 8*12(%esi)		/* V23 */
	psubsw %mm2, %mm6		/* V25; mm2 free */
	movq %mm4, (%esi)		/* V24 */
/* keep mm6 alive all along the next block */
	/* movq %mm6, 8*8(%esi) 	V25 */
/* column 0: odd part
 * use V2, V6, V10, V14 to produce V31, V39, V40, V41
 */
/* moved above: movq 8*10(%ecx), %mm7 */

	pmulhw 8*10(%esi), %mm7		/* V10 */
	movq 8*6(%ecx), %mm0
	pmulhw 8*6(%esi), %mm0		/* V6 */
	movq 8*2(%ecx), %mm5
	movq %mm7, %mm3			/* duplicate V10 */
	pmulhw 8*2(%esi), %mm5		/* V2 */
	movq 8*14(%ecx), %mm4
	psubsw %mm0, %mm7		/* V26 */
	pmulhw 8*14(%esi), %mm4		/* V14 */
	paddsw %mm0, %mm3		/* V29 ; free mm0 */
	movq %mm7, %mm1			/* duplicate V26 */
	psraw $1, %mm3			/* t91=t94 */
	pmulhw x539f539f539f539f,%mm7	/* V33 */
	psraw $1, %mm1			/* t96 */
	movq %mm5, %mm0			/* duplicate V2 */
	psraw $2, %mm4			/* t85=t87 */
	paddsw %mm4,%mm5		/* V27 */
	psubsw %mm4, %mm0		/* V28 ; free mm4 */
	movq %mm0, %mm2			/* duplicate V28 */
	psraw $1, %mm5			/* t90=t93 */
	pmulhw x4546454645464546,%mm0	/* V35 */
	psraw $1, %mm2			/* t97 */
	movq %mm5, %mm4			/* duplicate t90=t93 */
	psubsw %mm2, %mm1		/* V32 ; free mm2 */
	pmulhw x61f861f861f861f8,%mm1	/* V36 */
	psllw $1, %mm7			/* t107 */
	paddsw %mm3, %mm5		/* V31 */
	psubsw %mm3, %mm4		/* V30 ; free mm3 */
	pmulhw x5a825a825a825a82,%mm4	/* V34 */
	nop
	psubsw %mm1, %mm0		/* V38 */
	psubsw %mm7, %mm1		/* V37 ; free mm7 */
	psllw $1, %mm1			/* t114 */
/* move from the next block */
	movq %mm6, %mm3			/* duplicate V25 */
/* move from the next block */
	movq 8*4(%esi), %mm7		/* V22 */
	psllw $1, %mm0			/* t110 */
	psubsw %mm5, %mm0		/* V39 (mm5 needed for next block) */
	psllw $2, %mm4			/* t112 */
/* moved from the next block */
	movq 8*12(%esi), %mm2		/* V23 */
	psubsw %mm0, %mm4		/* V40 */
	paddsw %mm4, %mm1		/* V41; free mm0 */
/* moved from the next block */
	psllw $1, %mm2			/* t117=t125 */
/* column 0: output butterfly */
/* moved above:
 * movq %mm6, %mm3			duplicate V25
 * movq 8*4(%esi), %mm7			V22
 * movq 8*12(%esi), %mm2		V23
 * psllw $1, %mm2			t117=t125
 */
	psubsw %mm1, %mm6		/* tm6 */
	paddsw %mm1, %mm3		/* tm8; free mm1 */
	movq %mm7, %mm1			/* duplicate V22 */
	paddsw %mm5, %mm7		/* tm0 */
	movq %mm3, 8*8(%esi)		/* tm8; free mm3 */
	psubsw %mm5, %mm1		/* tm14; free mm5 */
	movq %mm6, 8*6(%esi)		/* tm6; free mm6 */
	movq %mm2, %mm3			/* duplicate t117=t125 */
	movq (%esi), %mm6		/* V24 */
	paddsw %mm0, %mm2		/* tm2 */
	movq %mm7, (%esi)		/* tm0; free mm7 */
	psubsw %mm0, %mm3		/* tm12; free mm0 */
	movq %mm1, 8*14(%esi)		/* tm14; free mm1 */
	psllw $1, %mm6			/* t119=t123 */
	movq %mm2, 8*2(%esi)		/* tm2; free mm2 */
	movq %mm6, %mm0			/* duplicate t119=t123 */
	movq %mm3, 8*12(%esi)		/* tm12; free mm3 */
	paddsw %mm4, %mm6		/* tm4 */
/* moved from next block */
	movq 8*5(%ecx), %mm1
	psubsw %mm4, %mm0		/* tm10; free mm4 */
/* moved from next block */
	pmulhw 8*5(%esi), %mm1		/* V5 */
	movq %mm6, 8*4(%esi)		/* tm4; free mm6 */
	movq %mm0, 8*10(%esi)		/* tm10; free mm0 */
/* column 1: even part
 * use V5, V13, V1, V9 to produce V56..V59
 */
/* moved to prev block:
 *	movq 8*5(%ecx), %mm1
 *	pmulhw 8*5(%esi), %mm1		 V5
 */
	movq 8*13(%ecx), %mm7
	psllw $1, %mm1			/* t128=t130 */
	pmulhw 8*13(%esi), %mm7		/* V13 */
	movq %mm1, %mm2			/* duplicate t128=t130 */
	movq 8(%ecx), %mm3
	pmulhw 8(%esi), %mm3		/* V1 */
	movq 8*9(%ecx), %mm5
	psubsw %mm7, %mm1		/* V50 */
	pmulhw 8*9(%esi), %mm5		/* V9 */
	paddsw %mm7, %mm2		/* V51 */
	pmulhw x5a825a825a825a82, %mm1	/* 23170 ->V52 */
	movq %mm2, %mm6			/* duplicate V51 */
	psraw $1, %mm2			/* t138=t144 */
	movq %mm3, %mm4			/* duplicate V1 */
	psraw $2, %mm6			/* t136 */
	paddsw %mm5, %mm3		/* V53 */
	psubsw %mm5, %mm4		/* V54 ;mm5 free */
	movq %mm3, %mm7			/* duplicate V53 */
/* moved from next block */
	movq 8*11(%ecx), %mm0
	psraw $1, %mm4			/* t140=t142 */
	psubsw %mm6, %mm1		/* V55 ; mm6 free */
	paddsw %mm2, %mm3		/* V56 */
	movq %mm4, %mm5			/* duplicate t140=t142 */
	paddsw %mm1, %mm4		/* V57 */
	movq %mm3, 8*5(%esi)		/* V56 */
	psubsw %mm1, %mm5		/* V58; mm1 free */
	movq %mm4, 8*13(%esi)		/* V57 */
	psubsw %mm2, %mm7		/* V59; mm2 free */
	movq %mm5, 8*9(%esi)		/* V58 */
/* keep mm7 alive all along the next block
 *	movq %mm7, 8(%esi)		V59
 * moved above
 *	movq 8*11(%ecx), %mm0
 */
	pmulhw 8*11(%esi), %mm0		/* V11 */
	movq 8*7(%ecx), %mm6
	pmulhw 8*7(%esi), %mm6		/* V7 */
	movq 8*15(%ecx), %mm4
	movq %mm0, %mm3			/* duplicate V11 */
	pmulhw 8*15(%esi), %mm4		/* V15 */
	movq 8*3(%ecx), %mm5
	psllw $1, %mm6			/* t146=t152 */
	pmulhw 8*3(%esi), %mm5		/* V3 */
	paddsw %mm6, %mm0		/* V63 */
/* note that V15 computation has a correction step:
 * this is a 'magic' constant that rebiases the results to be closer to the
 * expected result.  this magic constant can be refined to reduce the error
 * even more by doing the correction step in a later stage when the number
 * is actually multiplied by 16
 */
	paddw x0005000200010001, %mm4
	psubsw %mm6, %mm3		/* V60 ; free mm6 */
	psraw $1, %mm0			/* t154=t156 */
	movq %mm3, %mm1			/* duplicate V60 */
	pmulhw x539f539f539f539f, %mm1	/* V67 */
	movq %mm5, %mm6			/* duplicate V3 */
	psraw $2, %mm4			/* t148=t150 */
	paddsw %mm4, %mm5		/* V61 */
	psubsw %mm4, %mm6		/* V62 ; free mm4 */
	movq %mm5, %mm4			/* duplicate V61 */
	psllw $1, %mm1			/* t169 */
	paddsw %mm0, %mm5		/* V65 -> result */
	psubsw %mm0, %mm4		/* V64 ; free mm0 */
	pmulhw x5a825a825a825a82, %mm4	/* V68 */
	psraw $1, %mm3			/* t158 */
	psubsw %mm6, %mm3		/* V66 */
	movq %mm5, %mm2			/* duplicate V65 */
	pmulhw x61f861f861f861f8, %mm3	/* V70 */
	psllw $1, %mm6			/* t165 */
	pmulhw x4546454645464546, %mm6	/* V69 */
	psraw $1, %mm2			/* t172 */
/* moved from next block */
	movq 8*5(%esi), %mm0		/* V56 */
	psllw $1, %mm4			/* t174 */
/* moved from next block */
	psraw $1, %mm0			/* t177=t188 */
	nop
	psubsw %mm3, %mm6		/* V72 */
	psubsw %mm1, %mm3		/* V71 ; free mm1 */
	psubsw %mm2, %mm6		/* V73 ; free mm2 */
/* moved from next block */
	psraw $1, %mm5			/* t178=t189 */
	psubsw %mm6, %mm4		/* V74 */
/* moved from next block */
	movq %mm0, %mm1			/* duplicate t177=t188 */
	paddsw %mm4, %mm3		/* V75 */
/* moved from next block */
	paddsw %mm5, %mm0		/* tm1 */
/* location
 *  5 - V56
 * 13 - V57
 *  9 - V58
 *  X - V59, mm7
 *  X - V65, mm5
 *  X - V73, mm6
 *  X - V74, mm4
 *  X - V75, mm3
 * free mm0, mm1 & mm2
 * moved above
 *	movq 8*5(%esi), %mm0		V56
 *	psllw $1, %mm0			t177=t188 ! new !!
 *	psllw $1, %mm5			t178=t189 ! new !!
 *	movq %mm0, %mm1			duplicate t177=t188
 *	paddsw %mm5, %mm0		tm1
 */
	movq 8*13(%esi), %mm2		/* V57 */
	psubsw %mm5, %mm1		/* tm15; free mm5 */
	movq %mm0, 8(%esi)		/* tm1; free mm0 */
	psraw $1, %mm7			/* t182=t184 ! new !! */
/* save the store as used directly in the transpose
 *	movq %mm1, 120(%esi)		tm15; free mm1
 */
	movq %mm7, %mm5			/* duplicate t182=t184 */
	psubsw %mm3, %mm7		/* tm7 */
	paddsw %mm3, %mm5		/* tm9; free mm3 */
	movq 8*9(%esi), %mm0		/* V58 */
	movq %mm2, %mm3			/* duplicate V57 */
	movq %mm7, 8*7(%esi)		/* tm7; free mm7 */
	psubsw %mm6, %mm3		/* tm13 */
	paddsw %mm6, %mm2		/* tm3 ; free mm6 */
/* moved up from the transpose */
	movq %mm3, %mm7
/* moved up from the transpose */
	punpcklwd %mm1, %mm3
	movq %mm0, %mm6			/* duplicate V58 */
	movq %mm2, 8*3(%esi)		/* tm3; free mm2 */
	paddsw %mm4, %mm0		/* tm5 */
	psubsw %mm4, %mm6		/* tm11; free mm4 */
/* moved up from the transpose */
	punpckhwd %mm1, %mm7
	movq %mm0, 8*5(%esi)		/* tm5; free mm0 */
/* moved up from the transpose */
	movq %mm5, %mm2
/* transpose - M4 part
 *  ---------       ---------
 * | M1 | M2 |     | M1'| M3'|
 *  ---------  -->  ---------
 * | M3 | M4 |     | M2'| M4'|
 *  ---------       ---------
 * Two alternatives: use full mmword approach so the following code can be
 * scheduled before the transpose is done without stores, or use the faster
 * half mmword stores (when possible)
 */
	movd %mm3, 8*9+4(%esi)		/* MS part of tmt9 */
	punpcklwd %mm6, %mm5
	movd %mm7, 8*13+4(%esi)		/* MS part of tmt13 */
	punpckhwd %mm6, %mm2
	movd %mm5, 8*9(%esi)		/* LS part of tmt9 */
	punpckhdq %mm3, %mm5		/* free mm3 */
	movd %mm2, 8*13(%esi)		/* LS part of tmt13 */
	punpckhdq %mm7, %mm2		/* free mm7 */
/* moved up from the M3 transpose */
	movq 8*8(%esi), %mm0
/* moved up from the M3 transpose */
	movq 8*10(%esi), %mm1
/* moved up from the M3 transpose */
	movq %mm0, %mm3
/* shuffle the rest of the data, and write it with 2 mmword writes */
	movq %mm5, 8*11(%esi)		/* tmt11 */
/* moved up from the M3 transpose */
	punpcklwd %mm1, %mm0
	movq %mm2, 8*15(%esi)		/* tmt15 */
/* moved up from the M3 transpose */
	punpckhwd %mm1, %mm3
/* transpose - M3 part
 * moved up to previous code section
 *	movq 8*8(%esi), %mm0
 *	movq 8*10(%esi), %mm1
 *	movq %mm0, %mm3
 *	punpcklwd %mm1, %mm0
 *	punpckhwd %mm1, %mm3
 */
	movq 8*12(%esi), %mm6
	movq 8*14(%esi), %mm4
	movq %mm6, %mm2
/* shuffle the data and write the lower parts of the transposed in 4 dwords */
	punpcklwd %mm4, %mm6
	movq %mm0, %mm1
	punpckhdq %mm6, %mm1
	movq %mm3, %mm7
	punpckhwd %mm4, %mm2		/* free mm4 */
	punpckldq %mm6, %mm0		/* free mm6 */
/* moved from next block */
	movq 8*13(%esi), %mm4		/* tmt13 */
	punpckldq %mm2, %mm3
	punpckhdq %mm2, %mm7		/* free mm2 */
/* moved from next block */
	movq %mm3, %mm5			/* duplicate tmt5 */
/* column 1: even part (after transpose)
* moved above
*	movq %mm3, %mm5			duplicate tmt5
*	movq 8*13(%esi), %mm4		tmt13
*/
	psubsw %mm4, %mm3		/* V134 */
	pmulhw x5a825a825a825a82, %mm3	/* 23170 ->V136 */
	movq 8*9(%esi), %mm6		/* tmt9 */
	paddsw %mm4, %mm5		/* V135 ; mm4 free */
	movq %mm0, %mm4			/* duplicate tmt1 */
	paddsw %mm6, %mm0		/* V137 */
	psubsw %mm6, %mm4		/* V138 ; mm6 free */
	psllw $2, %mm3			/* t290 */
	psubsw %mm5, %mm3		/* V139 */
	movq %mm0, %mm6			/* duplicate V137 */
	paddsw %mm5, %mm0		/* V140 */
	movq %mm4, %mm2			/* duplicate V138 */
	paddsw %mm3, %mm2		/* V141 */
	psubsw %mm3, %mm4		/* V142 ; mm3 free */
	movq %mm0, 8*9(%esi)		/* V140 */
	psubsw %mm5, %mm6		/* V143 ; mm5 free */
/* moved from next block */
	movq 8*11(%esi), %mm0		/* tmt11 */
	movq %mm2, 8*13(%esi)		/* V141 */
/* moved from next block */
	movq %mm0, %mm2			/* duplicate tmt11 */
/* column 1: odd part (after transpose) */
/* moved up to the prev block
 *	movq 8*11(%esi), %mm0		tmt11
 *	movq %mm0, %mm2			duplicate tmt11
 */
	movq 8*15(%esi), %mm5		/* tmt15 */
	psubsw %mm7, %mm0		/* V144 */
	movq %mm0, %mm3			/* duplicate V144 */
	paddsw %mm7, %mm2		/* V147 ; free mm7 */
	pmulhw x539f539f539f539f, %mm0	/* 21407-> V151 */
	movq %mm1, %mm7			/* duplicate tmt3 */
	paddsw %mm5, %mm7		/* V145 */
	psubsw %mm5, %mm1		/* V146 ; free mm5 */
	psubsw %mm1, %mm3		/* V150 */
	movq %mm7, %mm5			/* duplicate V145 */
	pmulhw x4546454645464546, %mm1	/* 17734-> V153 */
	psubsw %mm2, %mm5		/* V148 */
	pmulhw x61f861f861f861f8, %mm3	/* 25080-> V154 */
	psllw $2, %mm0			/* t311 */
	pmulhw x5a825a825a825a82, %mm5	/* 23170-> V152 */
	paddsw %mm2, %mm7		/* V149 ; free mm2 */
	psllw $1, %mm1			/* t313 */
	nop	/* without the nop - freeze here for one clock */
	movq %mm3, %mm2			/* duplicate V154 */
	psubsw %mm0, %mm3		/* V155 ; free mm0 */
	psubsw %mm2, %mm1		/* V156 ; free mm2 */
/* moved from the next block */
	movq %mm6, %mm2			/* duplicate V143 */
/* moved from the next block */
	movq 8*13(%esi), %mm0		/* V141 */
	psllw $1, %mm1			/* t315 */
	psubsw %mm7, %mm1		/* V157 (keep V149) */
	psllw $2, %mm5			/* t317 */
	psubsw %mm1, %mm5		/* V158 */
	psllw $1, %mm3			/* t319 */
	paddsw %mm5, %mm3		/* V159 */
/* column 1: output butterfly (after transform)
 * moved to the prev block
 *	movq %mm6, %mm2			duplicate V143
 *	movq 8*13(%esi), %mm0		V141
 */
	psubsw %mm3, %mm2		/* V163 */
	paddsw %mm3, %mm6		/* V164 ; free mm3 */
	movq %mm4, %mm3			/* duplicate V142 */
	psubsw %mm5, %mm4		/* V165 ; free mm5 */
	movq %mm2, scratch7		/* out7 */
	psraw $4, %mm6
	psraw $4, %mm4
	paddsw %mm5, %mm3		/* V162 */
	movq 8*9(%esi), %mm2		/* V140 */
	movq %mm0, %mm5			/* duplicate V141 */
/* in order not to perculate this line up,
 * we read 72(%esi) very near to this location
 */
	movq %mm6, 8*9(%esi)		/* out9 */
	paddsw %mm1, %mm0		/* V161 */
	movq %mm3, scratch5		/* out5 */
	psubsw %mm1, %mm5		/* V166 ; free mm1 */
	movq %mm4, 8*11(%esi)		/* out11 */
	psraw $4, %mm5
	movq %mm0, scratch3		/* out3 */
	movq %mm2, %mm4			/* duplicate V140 */
	movq %mm5, 8*13(%esi)		/* out13 */
	paddsw %mm7, %mm2		/* V160 */
/* moved from the next block */
	movq 8(%esi), %mm0
	psubsw %mm7, %mm4		/* V167 ; free mm7 */
/* moved from the next block */
	movq 8*3(%esi), %mm7
	psraw $4, %mm4
	movq %mm2, scratch1		/* out1 */
/* moved from the next block */
	movq %mm0, %mm1
	movq %mm4, 8*15(%esi)		/* out15 */
/* moved from the next block */
	punpcklwd %mm7, %mm0
/* transpose - M2 parts
 * moved up to the prev block
 *	movq 8(%esi), %mm0
 *	movq 8*3(%esi), %mm7
 *	movq %mm0, %mm1
 *	punpcklwd %mm7, %mm0
 */
	movq 8*5(%esi), %mm5
	punpckhwd %mm7, %mm1
	movq 8*7(%esi), %mm4
	movq %mm5, %mm3
/* shuffle the data and write the lower parts of the trasposed in 4 dwords */
	movd %mm0, 8*8(%esi)		/* LS part of tmt8 */
	punpcklwd %mm4, %mm5
	movd %mm1, 8*12(%esi)		/* LS part of tmt12 */
	punpckhwd %mm4, %mm3
	movd %mm5, 8*8+4(%esi)		/* MS part of tmt8 */
	punpckhdq %mm5, %mm0		/* tmt10 */
	movd %mm3, 8*12+4(%esi)		/* MS part of tmt12 */
	punpckhdq %mm3, %mm1		/* tmt14 */
/* transpose - M1 parts */
	movq (%esi), %mm7
	movq 8*2(%esi), %mm2
	movq %mm7, %mm6
	movq 8*4(%esi), %mm5
	punpcklwd %mm2, %mm7
	movq 8*6(%esi), %mm4
	punpckhwd %mm2, %mm6		/* free mm2 */
	movq %mm5, %mm3
	punpcklwd %mm4, %mm5
	punpckhwd %mm4, %mm3		/* free mm4 */
	movq %mm7, %mm2
	movq %mm6, %mm4
	punpckldq %mm5, %mm7		/* tmt0 */
	punpckhdq %mm5, %mm2		/* tmt2 ; free mm5 */
/* shuffle the rest of the data, and write it with 2 mmword writes */
	punpckldq %mm3, %mm6		/* tmt4 */
/* moved from next block */
	movq %mm2, %mm5			/* duplicate tmt2 */
	punpckhdq %mm3, %mm4		/* tmt6 ; free mm3 */
/* moved from next block */
	movq %mm0, %mm3			/* duplicate tmt10 */
/* column 0: odd part (after transpose)
 *moved up to prev block
 *	movq %mm0, %mm3			duplicate tmt10
 *	movq %mm2, %mm5			duplicate tmt2
 */
	psubsw %mm4, %mm0		/* V110 */
	paddsw %mm4, %mm3		/* V113 ; free mm4 */
	movq %mm0, %mm4			/* duplicate V110 */
	paddsw %mm1, %mm2		/* V111 */
	pmulhw x539f539f539f539f, %mm0	/* 21407-> V117 */
	psubsw %mm1, %mm5		/* V112 ; free mm1 */
	psubsw %mm5, %mm4		/* V116 */
	movq %mm2, %mm1			/* duplicate V111 */
	pmulhw x4546454645464546, %mm5	/* 17734-> V119 */
	psubsw %mm3, %mm2		/* V114 */
	pmulhw x61f861f861f861f8, %mm4	/* 25080-> V120 */
	paddsw %mm3, %mm1		/* V115 ; free mm3 */
	pmulhw x5a825a825a825a82, %mm2	/* 23170-> V118 */
	psllw $2, %mm0			/* t266 */
	movq %mm1, (%esi)		/* save V115 */
	psllw $1, %mm5			/* t268 */
	psubsw %mm4, %mm5		/* V122 */
	psubsw %mm0, %mm4		/* V121 ; free mm0 */
	psllw $1, %mm5			/* t270 */
	psubsw %mm1, %mm5		/* V123 ; free mm1 */
	psllw $2, %mm2			/* t272 */
	psubsw %mm5, %mm2		/* V124 (keep V123) */
	psllw $1, %mm4			/* t274 */
	movq %mm5, 8*2(%esi)		/* save V123 ; free mm5 */
	paddsw %mm2, %mm4		/* V125 (keep V124) */
/* column 0: even part (after transpose) */
	movq 8*12(%esi), %mm0		/* tmt12 */
	movq %mm6, %mm3			/* duplicate tmt4 */
	psubsw %mm0, %mm6		/* V100 */
	paddsw %mm0, %mm3		/* V101 ; free mm0 */
	pmulhw x5a825a825a825a82, %mm6	/* 23170 ->V102 */
	movq %mm7, %mm5			/* duplicate tmt0 */
	movq 8*8(%esi), %mm1		/* tmt8 */
	paddsw %mm1, %mm7		/* V103 */
	psubsw %mm1, %mm5		/* V104 ; free mm1 */
	movq %mm7, %mm0			/* duplicate V103 */
	psllw $2, %mm6			/* t245 */
	paddsw %mm3, %mm7		/* V106 */
	movq %mm5, %mm1			/* duplicate V104 */
	psubsw %mm3, %mm6		/* V105 */
	psubsw %mm3, %mm0		/* V109; free mm3 */
	paddsw %mm6, %mm5		/* V107 */
	psubsw %mm6, %mm1		/* V108 ; free mm6 */
/* column 0: output butterfly (after transform) */
	movq %mm1, %mm3			/* duplicate V108 */
	paddsw %mm2, %mm1		/* out4 */
	psraw $4, %mm1
	psubsw %mm2, %mm3		/* out10 ; free mm2 */
	psraw $4, %mm3
	movq %mm0, %mm6			/* duplicate V109 */
	movq %mm1, 8*4(%esi)		/* out4 ; free mm1 */
	psubsw %mm4, %mm0		/* out6 */
	movq %mm3, 8*10(%esi)		/* out10 ; free mm3 */
	psraw $4, %mm0
	paddsw %mm4, %mm6		/* out8 ; free mm4 */
	movq %mm7, %mm1			/* duplicate V106 */
	movq %mm0, 8*6(%esi)		/* out6 ; free mm0 */
	psraw $4, %mm6
	movq (%esi), %mm4		/* V115 */
	movq %mm6, 8*8(%esi)		/* out8 ; free mm6 */
	movq %mm5, %mm2			/* duplicate V107 */
	movq 8*2(%esi), %mm3		/* V123 */
	paddsw %mm4, %mm7		/* out0 */
/* moved up from next block */
	movq scratch3, %mm0
	psraw $4, %mm7
/* moved up from next block */
	movq scratch5, %mm6
	psubsw %mm4, %mm1		/* out14 ; free mm4 */
	paddsw %mm3, %mm5		/* out2 */
	psraw $4, %mm1
	movq %mm7, (%esi)		/* out0 ; free mm7 */
	psraw $4, %mm5
	movq %mm1, 8*14(%esi)		/* out14 ; free mm1 */
	psubsw %mm3, %mm2		/* out12 ; free mm3 */
	movq %mm5, 8*2(%esi)		/* out2 ; free mm5 */
	psraw $4, %mm2
/* moved up to the prev block */
	movq scratch7, %mm4
/* moved up to the prev block */
	psraw $4, %mm0
	movq %mm2, 8*12(%esi)		/* out12 ; free mm2 */
/* moved up to the prev block */
	psraw $4, %mm6
/* move back the data to its correct place
* moved up to the prev block
 *	movq scratch3, %mm0
 *	movq scratch5, %mm6
 *	movq scratch7, %mm4
 *	psraw $4, %mm0
 *	psraw $4, %mm6
*/
	movq scratch1, %mm1
	psraw $4, %mm4
	movq %mm0, 8*3(%esi)		/* out3 */
	psraw $4, %mm1
	movq %mm6, 8*5(%esi)		/* out5 */
	movq %mm4, 8*7(%esi)		/* out7 */
	movq %mm1, 8(%esi)		/* out1 */
/* transpose matrix */
	movl $8, %ebx			/* ebx is x_size */
	movl %esi, %edi			/* pointer to the matrix */
	movl %ebx, %ecx
	sal $2, %ecx
	movl %ebx, %eax
	addl %ebx, %ecx
	subl $4, %eax			/* eax is inner loop variable */
	addl %ebx, %ecx			/* ecx is 6*row size */
	movl %eax, %edx			/* edx is the outer loop variable */
.L1:	movq (%esi), %mm0		/* first line */
	movq (%esi,%ebx,4), %mm2	/* third line */
	movq %mm0, %mm6			/* copy first line */
	punpcklwd (%esi,%ebx,2), %mm0	/* interleave fist and second lines */
	movq %mm2, %mm7			/* copy third line */
	punpcklwd (%esi,%ecx), %mm2	/* interleave third and fourth lines */
	movq %mm0, %mm4			/* copy first intermediate result */
	movq (%esi,%ebx,2), %mm1	/* second line */
/* the next line 'punpcklwd %mm2, %mm0' inverted two pixels. */
/* punpckldq make printing cleaner */
	punpckldq %mm2, %mm0		/* interleave to produce result 1 */
	movq (%esi,%ecx), %mm3		/* fourth line */
	punpckhdq %mm2, %mm4		/* interleave to produce result 2 */
	movq %mm0, (%esi)		/* write result 1 */
	punpckhwd %mm1, %mm6		/* interleave first and second lines */
	movq %mm4, (%esi,%ebx,2)	/* write result 2 */
	punpckhwd %mm3, %mm7		/* interleave 3rd and 4th lines */
	movq %mm6, %mm5			/* copy first intermediate result */
	punpckldq %mm7, %mm6		/* interleave to produce result 3 */
	leal (%edi,%ebx,8), %edi	/* point to 4x4 set 4 rows down */
	punpckhdq %mm7, %mm5		/* interleave to produce result 4 */
	movq %mm6, (%esi,%ebx,4)	/* write result 3 */
	movq %mm5, (%esi,%ecx)		/* write result 4 */
	/* check to see if number of rows left is zero */
	cmpl $0, %edx
	/* last time through you are done and ready to exit */
	je .L3
.L2:	movq 8(%esi), %mm0		/* first line */
	movq 8(%esi,%ebx,4), %mm2	/* third line */
	movq %mm0, %mm6			/* copy first line */
	punpcklwd 8(%esi,%ebx,2), %mm0	/* interleave first and second lines */
	movq %mm2, %mm7			/* copy third line */
	punpcklwd 8(%esi,%ecx), %mm2	/* interleave 3rd and 4th lines */
	movq %mm0, %mm4			/* copy first intermediate */
	movq (%edi), %mm1		/* first line */
	punpckldq %mm2, %mm0		/* interleave to produce 1st result */
	movq (%edi,%ebx,4), %mm3	/* third line */
	punpckhdq %mm2, %mm4		/* interleave to produce 2nd result */
	punpckhwd 8(%esi,%ebx,2), %mm6	/* interleave 1st and 2nd lines */
	movq %mm1, %mm2			/* copy first line */
	punpckhwd 8(%esi,%ecx), %mm7	/* interleave 3rd and 4th lines */
	movq %mm6, %mm5			/* copy first intermediate */
	movq %mm0, (%edi)		/* write result 1 */
	punpckhdq %mm7, %mm5		/* produce third result */
	punpcklwd (%edi,%ebx,2), %mm1	/* interleave 1st and 2nd lines */
	movq %mm3, %mm0			/* copy third line */
	punpckhwd (%edi,%ebx,2), %mm2	/* interleave 1st and 2nd lines */
	movq %mm4, (%edi,%ebx,2)	/* write result 2 */
	punpckldq %mm7, %mm6		/* produce fourth result */
	punpcklwd (%edi,%ecx), %mm3	/* interleave 3rd and 4th lines */
	movq %mm1, %mm4			/* copy first intermediate */
	movq %mm6, (%edi,%ebx,4)	/* write result 3 */
	punpckldq %mm3, %mm1
	punpckhwd (%edi,%ecx), %mm0	/* interleave 3rd and 4th lines */
	movq %mm2, %mm6			/* copy second intermediate */
	movq %mm5, (%edi,%ecx)		/* write result 4 */
	punpckhdq %mm3, %mm4		/* produce second result */
	movq %mm1, 8(%esi)		/* write result 5 */
	punpckldq %mm0, %mm2		/* produce third result */
	movq %mm4, 8(%esi,%ebx,2)	/* write result 6 */
	punpckhdq %mm0, %mm6		/* produce fourth result */
	movq %mm2, 8(%esi,%ebx,4)	/* write result 7 */
	movq %mm6, 8(%esi,%ecx)		/* write result 8 */
	/* increment %esi to point to next 4x4 block in same row */
	addl $8, %esi
	/* increment %edi to point to nxt 4x4 block below current */
	leal (%edi,%ebx,8), %edi
	sub $4, %eax			/* decrement inner loop var */
	jnz .L2
	/* %edi points to start of second row in block just finished */
	sal $1, %edx
	leal 8(%esi,%ebx,8), %esi
	subl %edx, %esi
	/* subtract the number of bytes in last row */
	/* now we point to spot where row=col */
	subl $8, %edx			/* sub 4 from row number */
	sarl $1, %edx
	mov %esi, %edi
	mov %edx, %eax
	/* reset x_size to outer loop variable to start new row */
	jmp .L1
.L3:	emms
	popl %edi
	popl %esi
	popl %edx
	popl %ecx
	popl %ebx
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe1:
	.size	 vdec_IDCT,.Lfe1-vdec_IDCT
