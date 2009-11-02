/*
 * ARM assembly optimized color format conversion functions
 * (YV12 -> YUY2, YV12 -> some custom YUV420 format used by
 * Epson graphics chip in Nokia N800)
 *
 * Copyright (C) 2007 Siarhei Siamashka <ssvb@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
        .text

/*******************************************************/

        .align
        .global yv12_to_yuy2_line_arm
        .func yv12_to_yuy2_line_arm
yv12_to_yuy2_line_arm:

#define DST     r0
#define SRC_Y   r1
#define SRC_U   r2
#define SRC_V   r3
#define WIDTH   ip

        ldr     ip, [sp], #0
        stmfd   sp!, {r4-r8, lr}

#define TMP1    r8
#define TMP2    r12
#define TMP3    lr

        bic     WIDTH, #1

        subs    WIDTH, #8
        blt     2f
1:
        ldrb    r4, [SRC_Y], #1
        ldrb    TMP1, [SRC_U], #1
        ldrb    TMP2, [SRC_Y], #1
        ldrb    TMP3, [SRC_V], #1
        add     r4, r4, TMP1, lsl #8
        add     r4, r4, TMP2, lsl #16
        add     r4, r4, TMP3, lsl #24

        ldrb    r5, [SRC_Y], #1
        ldrb    TMP1, [SRC_U], #1
        ldrb    TMP2, [SRC_Y], #1
        ldrb    TMP3, [SRC_V], #1
        add     r5, r5, TMP1, lsl #8
        add     r5, r5, TMP2, lsl #16
        add     r5, r5, TMP3, lsl #24

        ldrb    r6, [SRC_Y], #1
        ldrb    TMP1, [SRC_U], #1
        ldrb    TMP2, [SRC_Y], #1
        ldrb    TMP3, [SRC_V], #1
        add     r6, r6, TMP1, lsl #8
        add     r6, r6, TMP2, lsl #16
        add     r6, r6, TMP3, lsl #24

        ldrb    r7, [SRC_Y], #1
        ldrb    TMP1, [SRC_U], #1
        ldrb    TMP2, [SRC_Y], #1
        ldrb    TMP3, [SRC_V], #1
        add     r7, r7, TMP1, lsl #8
        add     r7, r7, TMP2, lsl #16
        add     r7, r7, TMP3, lsl #24

        stmia   DST!, {r4-r7}
        subs    WIDTH, WIDTH, #8
        bge     1b
2:
        adds    WIDTH, WIDTH, #8
        ble     4f
3:
        ldrb    r4, [SRC_Y], #1
        ldrb    TMP1, [SRC_U], #1
        ldrb    TMP2, [SRC_Y], #1
        ldrb    TMP3, [SRC_V], #1
        add     r4, r4, TMP1, lsl #8
        add     r4, r4, TMP2, lsl #16
        add     r4, r4, TMP3, lsl #24
        str     r4, [DST], #4
        subs    WIDTH, WIDTH, #2
        bgt     3b
4:
        ldmfd  sp!, {r4-r8, pc}

#undef  DST
#undef  SRC_Y
#undef  SRC_U
#undef  SRC_V
#undef  WIDTH
#undef  TMP1
#undef  TMP2
#undef  TMP3

        .endfunc

/*******************************************************/

#define DST     r0
#define SRC_Y   r1
#define SRC_U   r2
#define WIDTH   r3
#define TMP1    r10
#define TMP2    r12
#define TMP3    lr

.macro YUV420_function_template function_name, USE_PLD, USE_ARMV6

        .align
        .global \function_name
        .func \function_name
\function_name:

/* Read information about 4 pixels, convert them to YUV420 and store into 6 bytes using 16-bit writes */
.macro  CONVERT_4_PIXELS_MACROBLOCK
        ldrb    r4, [SRC_Y], #1
        ldrb    TMP1, [SRC_U], #1
        ldrb    r5, [SRC_U], #1
        ldrb    TMP2, [SRC_Y], #1
        ldrb    r6, [SRC_Y, #1]
        ldrb    TMP3, [SRC_Y], #2
        add     r4, r4, TMP1, lsl #8
        add     r5, r5, TMP2, lsl #8
        add     r6, r6, TMP3, lsl #8
        strh    r4, [DST], #2
        strh    r5, [DST], #2
        strh    r6, [DST], #2
.endm

.if \USE_ARMV6

.macro  CONVERT_8_PIXELS_MACROBLOCK_1 DST_REG1, DST_REG2, FLAG1, FLAG2, PLD_FLAG
.if \FLAG1 == 0
        ldrb    \DST_REG1, [SRC_U], #1
        ldrh    TMP1, [SRC_Y], #2
        ldrb    TMP2, [SRC_U], #1
.endif
.if \FLAG2 == 1
        ldrh    \DST_REG2, [SRC_Y], #2
.endif
.if \PLD_FLAG == 1
        pld     [SRC_Y, #48]
.endif
        add     \DST_REG1, \DST_REG1, TMP1, lsl #8
        add     \DST_REG1, \DST_REG1, TMP2, lsl #24
.if \FLAG2 == 1
        ldrb    TMP1, [SRC_U], #1
        ldrb    TMP2, [SRC_Y], #1
.endif
        rev16   \DST_REG1, \DST_REG1
.endm

.macro  CONVERT_8_PIXELS_MACROBLOCK_2 DST_REG1, DST_REG2, FLAG1, FLAG2, DUMMY1
.if \FLAG1 == 0
        ldrh    \DST_REG1, [SRC_Y], #2
        ldrb    TMP1, [SRC_U], #1
        ldrb    TMP2, [SRC_Y], #1
.endif
.if \FLAG2 == 1
        ldrb    \DST_REG2, [SRC_Y], #1
.endif
        add     \DST_REG1, \DST_REG1, TMP1, lsl #16
        add     \DST_REG1, \DST_REG1, TMP2, lsl #24
.if \FLAG2 == 1
        ldrb    TMP1, [SRC_U], #1
        ldrh    TMP2, [SRC_Y], #2
.endif
        rev16   \DST_REG1, \DST_REG1
.endm

.macro  CONVERT_8_PIXELS_MACROBLOCK_3 DST_REG1, DST_REG2, FLAG1, FLAG2, DUMMY1
.if \FLAG1 == 0
        ldrb    \DST_REG1, [SRC_Y], #1
        ldrb    TMP1, [SRC_U], #1
        ldrh    TMP2, [SRC_Y], #2
.endif
.if \FLAG2 == 1
        ldrb    \DST_REG2, [SRC_U], #1
.endif
        add     \DST_REG1, \DST_REG1, TMP1, lsl #8
        add     \DST_REG1, \DST_REG1, TMP2, lsl #16
.if \FLAG2 == 1
        ldrh    TMP1, [SRC_Y], #2
        ldrb    TMP2, [SRC_U], #1
.endif
        rev16   \DST_REG1, \DST_REG1
.endm

.else

/* Prepare the first 32-bit output value for 8 pixels macroblock */
.macro  CONVERT_8_PIXELS_MACROBLOCK_1 DST_REG, DUMMY1, DUMMY2, DUMMY3, PLD_FLAG
        ldrb    \DST_REG, [SRC_Y], #1
        ldrb    TMP1, [SRC_U], #1
        ldrb    TMP2, [SRC_U], #1
        ldrb    TMP3, [SRC_Y], #1
.if \USE_PLD && (\PLD_FLAG == 1)
        pld     [SRC_Y, #48]
.endif
        add     \DST_REG, \DST_REG, TMP1, lsl #8
        add     \DST_REG, \DST_REG, TMP2, lsl #16
        add     \DST_REG, \DST_REG, TMP3, lsl #24
.endm

/* Prepare the second 32-bit output value for 8 pixels macroblock */
.macro  CONVERT_8_PIXELS_MACROBLOCK_2 DST_REG, DUMMY1, DUMMY2, DUMMY3, DUMMY4
        ldrb    \DST_REG, [SRC_Y, #1]
        ldrb    TMP1, [SRC_Y], #2
        ldrb    TMP2, [SRC_Y], #1
        ldrb    TMP3, [SRC_U], #1
        add     \DST_REG, \DST_REG, TMP1, lsl #8
        add     \DST_REG, \DST_REG, TMP2, lsl #16
        add     \DST_REG, \DST_REG, TMP3, lsl #24
.endm

/* Prepare the third 32-bit output value for 8 pixels macroblock */
.macro  CONVERT_8_PIXELS_MACROBLOCK_3 DST_REG, DUMMY1, DUMMY2, DUMMY3, DUMMY4
        ldrb    \DST_REG, [SRC_U], #1
        ldrb    TMP1, [SRC_Y], #1
        ldrb    TMP2, [SRC_Y, #1]
        ldrb    TMP3, [SRC_Y], #2
        add     \DST_REG, \DST_REG, TMP1, lsl #8
        add     \DST_REG, \DST_REG, TMP2, lsl #16
        add     \DST_REG, \DST_REG, TMP3, lsl #24
.endm

.endif

.if \USE_PLD
        pld     [SRC_Y]
.endif
        stmfd   sp!, {r4-r8, r10, lr}

        /* Destination buffer should be at least 16-bit aligned, image width should be multiple of 4 */
        bic     DST, #1
        bic     WIDTH, #3

        /* Ensure 32-bit alignment of the destination buffer */
        tst     DST, #2
        beq     1f
        subs    WIDTH, #4
        blt     6f
        CONVERT_4_PIXELS_MACROBLOCK
1:
        subs    WIDTH, #32
        blt     3f
2:      /* Convert 32 pixels per loop iteration */
        CONVERT_8_PIXELS_MACROBLOCK_1 r4, r6, 0, 1, 1 /* Also do cache preload for SRC_Y */
        CONVERT_8_PIXELS_MACROBLOCK_2 r6, r7, 1, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_3 r7, r8, 1, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_1 r8, r5, 1, 1, 0
        stmia   DST!, {r4, r6, r7, r8}

        subs    WIDTH, #32

        CONVERT_8_PIXELS_MACROBLOCK_2 r5, r6, 1, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_3 r6, r7, 1, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_1 r7, r8, 1, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_2 r8, r4, 1, 1, 0
        stmia   DST!, {r5, r6, r7, r8}
.if \USE_PLD
         /* Do cache preload for SRC_U */
        pld     [SRC_U, #48]
.endif
        CONVERT_8_PIXELS_MACROBLOCK_3 r4, r6, 1, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_1 r6, r7, 1, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_2 r7, r8, 1, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_3 r8, r4, 1, 0, 0
        stmia   DST!, {r4, r6, r7, r8}

        bge     2b
3:
        adds    WIDTH, WIDTH, #32
        ble     6f

        subs    WIDTH, WIDTH, #8
        blt     5f
4:      /* Convert remaining pixels processing them 8 per iteration */
        CONVERT_8_PIXELS_MACROBLOCK_1 r4, r5, 0, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_2 r5, r6, 1, 1, 0
        CONVERT_8_PIXELS_MACROBLOCK_3 r6, r7, 1, 0, 0
        stmia   DST!, {r4-r6}
        subs    WIDTH, WIDTH, #8
        bge     4b
5:      /* Convert the last 4 pixels if needed */
        adds    WIDTH, WIDTH, #8
        ble     6f
        CONVERT_4_PIXELS_MACROBLOCK
        subs    WIDTH, #4
        bgt     4b
6:      /* Restore all registers and return */
        ldmfd  sp!, {r4-r8, r10, pc}

.purgem CONVERT_4_PIXELS_MACROBLOCK
.purgem CONVERT_8_PIXELS_MACROBLOCK_1
.purgem CONVERT_8_PIXELS_MACROBLOCK_2
.purgem CONVERT_8_PIXELS_MACROBLOCK_3

#undef  DST
#undef  SRC_Y
#undef  SRC_U
#undef  WIDTH
#undef  TMP1
#undef  TMP2
#undef  TMP3

        .endfunc

.endm

YUV420_function_template yv12_to_yuv420_line_arm,   0, 0
YUV420_function_template yv12_to_yuv420_line_armv5, 1, 0
YUV420_function_template yv12_to_yuv420_line_armv6, 1, 1
