/*
 * Fast JIT powered scaler for ARM
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

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#include "arm_jit_swscale.h"
#include "arm_colorconv.h"

/* Size of cpu instructions cache, we should never exceed it in generated code */
#define INSTRUCTIONS_CACHE_SIZE 32768

/* Supported output formats */
#define FMT_OMAPFB_YUV422 1
#define FMT_OMAPFB_YUV420 2

extern void __clear_cache (char *beg, char *end);

/*
 * API is similar to API from ffmpeg libswscale
 */
typedef struct SwsContextArmJit {
    int fmt;
    int source_w;
    int source_h;
    int target_w;
    int target_h;
    uint32_t *codebuffer;
    int *linebuffer;
    int armv6_is_supported;
} SwsContextArmJit;


//#define JIT_DEBUG

#define INTERPOLATE_COPY_FIRST   0
#define INTERPOLATE_AVERAGE_1_3  1
#define INTERPOLATE_AVERAGE_2_2  2
#define INTERPOLATE_AVERAGE_3_1  3

/**
 * Get two nearest pixels from the source image
 *
 * @todo get rid of the floating point math
 */
static inline int get_pix(int quality, int orig_w, int dest_w, int x, int *p1, int *p2)
{
    double offs = ((double)x + 0.5) / (double)dest_w * (double)orig_w;
    double dist;
    int pix1 = floor(offs - 0.5);
    int pix2 = ceil(offs - 0.5);
    // Special boundary cases
    if (pix1 < 0) {
        *p1 = *p2 = 0;
        return INTERPOLATE_COPY_FIRST;
    }
    if (pix2 >= orig_w) {
        *p1 = *p2 = orig_w - 1;
        return INTERPOLATE_COPY_FIRST;
    }
    dist = offs - ((double)pix1 + 0.5);
#if 0
    if (quality >= 3) {
        if (dist > 0.125 && dist < 0.375) {
            *p1 = pix1;
            *p2 = pix2;
            return INTERPOLATE_AVERAGE_3_1;
        }
        if (dist > 0.625 && dist < 0.875) {
            *p1 = pix1;
            *p2 = pix2;
            return INTERPOLATE_AVERAGE_1_3;
        }
    }
#endif
    if (quality >= 2) {
        if (dist > 0.25 && dist < 0.75) {
            *p1 = pix1;
            *p2 = pix2;
            return INTERPOLATE_AVERAGE_2_2;
        }
    }

    if (dist < 0.5) {
        *p1 = *p2 = pix1;
        return INTERPOLATE_COPY_FIRST;
    } else {
        *p1 = *p2 = pix2;
        return INTERPOLATE_COPY_FIRST;
    }
}

static uint32_t *generate_arm_cmd_ldrb_r_r_offs(uint32_t *cmdbuffer, int dstreg, int basereg, int offset)
{
#ifdef JIT_DEBUG
    printf("ldrb     r%d, [r%d, #%d]\n", dstreg, basereg, offset);
#endif
    *cmdbuffer++ = 0xE5D00000 | (basereg << 16) | (dstreg << 12) | (offset);
    return cmdbuffer;
}

static uint32_t *generate_arm_cmd_add_r_r_r_lsl(uint32_t *cmdbuffer, int dstreg, int r1, int r2, int r2_shift)
{
#ifdef JIT_DEBUG
    printf("add      r%d, r%d, r%d, lsl #%d\n", dstreg, r1, r2, r2_shift);
#endif
    *cmdbuffer++ = 0xE0800000 | (r1 << 16) | (dstreg << 12) | (r2_shift << 7) | (r2);
    return cmdbuffer;
}

static uint32_t *generate_arm_cmd_mov_r_r_lsr(uint32_t *cmdbuffer, int dstreg, int r, int shift)
{
#ifdef JIT_DEBUG
    printf("mov      r%d, r%d, lsr #%d\n", dstreg, r, shift);
#endif
    *cmdbuffer++ = 0xE1A00020 | (dstreg << 12) | (shift << 7) | (r);
    return cmdbuffer;
}

/**
 * Generation of 32-bit output scaled data
 * @param quality - scaling quality level
 * @param buf1reg - register that holds a pointer to the buffer with data for the first output byte
 * @param buf2reg - register that holds a pointer to the buffer with data for the second output byte
 * @param buf3reg - register that holds a pointer to the buffer with data for the third output byte
 * @param buf4reg - register that holds a pointer to the buffer with data for the fourth output byte
 */
static uint32_t *generate_32bit_scaled_data_write(
    uint32_t *p,
    int quality, int orig_w, int dest_w,
    int buf1reg, int size1, int offs1,
    int buf2reg, int size2, int offs2,
    int buf3reg, int size3, int offs3,
    int buf4reg, int size4, int offs4)
{
    int p1, p2;
    int type_y1, type_y2, type_u, type_v;
    // First stage: perform data loading
    type_y1 = get_pix(quality, orig_w / size1, dest_w / size1, offs1 / size1, &p1, &p2);
    if (type_y1 == INTERPOLATE_COPY_FIRST) {
        // Special case, no interpolation is needed, so load this data
        // directly into destination register
        p = generate_arm_cmd_ldrb_r_r_offs(p, 4, buf1reg, p1);
    } else {
        p = generate_arm_cmd_ldrb_r_r_offs(p, 5, buf1reg, p1);
        p = generate_arm_cmd_ldrb_r_r_offs(p, 6, buf1reg, p2);
    }
    // u
    type_u = get_pix(quality, orig_w / size2, dest_w / size2, offs2 / size2, &p1, &p2);
    p = generate_arm_cmd_ldrb_r_r_offs(p, 7, buf2reg, p1);
    if (type_u != INTERPOLATE_COPY_FIRST) p = generate_arm_cmd_ldrb_r_r_offs(p, 8, buf2reg, p2);
    // y2
    type_y2 = get_pix(quality, orig_w / size3, dest_w / size3, offs3 / size3, &p1, &p2);
    p = generate_arm_cmd_ldrb_r_r_offs(p, 9, buf3reg, p1);
    if (type_y2 != INTERPOLATE_COPY_FIRST) p = generate_arm_cmd_ldrb_r_r_offs(p, 10, buf3reg, p2);
    // v
    type_v = get_pix(quality, orig_w / size4, dest_w / size4, offs4 / size4, &p1, &p2);
    p = generate_arm_cmd_ldrb_r_r_offs(p, 11, buf4reg, p1);
    if (type_v != INTERPOLATE_COPY_FIRST) p = generate_arm_cmd_ldrb_r_r_offs(p, 12, buf4reg, p2);
    // Second stage: perform data shuffling
    if (type_y1 == INTERPOLATE_AVERAGE_2_2) {
        p = generate_arm_cmd_add_r_r_r_lsl(p, 14, 5, 6, 0);
        p = generate_arm_cmd_mov_r_r_lsr(p, 4, 14, 1);
    }
    if (type_u == INTERPOLATE_COPY_FIRST) {
        p = generate_arm_cmd_add_r_r_r_lsl(p, 4, 4, 7, 8);
    } else if (type_u == INTERPOLATE_AVERAGE_2_2) {
        p = generate_arm_cmd_add_r_r_r_lsl(p, 14, 7, 8, 0);
        p = generate_arm_cmd_mov_r_r_lsr(p, 14, 14, 1);
        p = generate_arm_cmd_add_r_r_r_lsl(p, 4, 4, 14, 8);
    }
    if (type_y2 == INTERPOLATE_COPY_FIRST) {
        p = generate_arm_cmd_add_r_r_r_lsl(p, 4, 4, 9, 16);
    } else if (type_y2 == INTERPOLATE_AVERAGE_2_2) {
        p = generate_arm_cmd_add_r_r_r_lsl(p, 14, 9, 10, 0);
        p = generate_arm_cmd_mov_r_r_lsr(p, 14, 14, 1);
        p = generate_arm_cmd_add_r_r_r_lsl(p, 4, 4, 14, 16);
    }
    if (type_v == INTERPOLATE_COPY_FIRST) {
        p = generate_arm_cmd_add_r_r_r_lsl(p, 4, 4, 11, 24);
    } else if (type_v == INTERPOLATE_AVERAGE_2_2) {
        p = generate_arm_cmd_add_r_r_r_lsl(p, 14, 11, 12, 0);
        p = generate_arm_cmd_mov_r_r_lsr(p, 14, 14, 1);
        p = generate_arm_cmd_add_r_r_r_lsl(p, 4, 4, 14, 24);
    }
    // Third stage: store data and advance output buffer pointer
    *p++ = 0xE4834004; // str r4, [r3], #4
    return p;
}

/**
 * Scaler code should assume:
 * r0 - y plane
 * r1 - u plane
 * r2 - v plane
 * r3 - destination buffer
 * r4 - result for storage into output buffer
 * r5, r6 - source data for y1 calculation
 * r7, r8 - source data for u calculation
 * r9, r10 - source data for y2 calculation
 * r11, r12 - source data for v calculation
 * r14 (lr) - accumulator
 *
 * @param cmdbuffer - bugger for dynamically generated code
 * @return - number of instructions generated
 */
static int generate_yuv420p_to_yuyv422_line_scaler(uint32_t *cmdbuffer, int maxcmdcount, int orig_w, int dest_w, int quality)
{
    int i, p1, p2, cmdcount;
    int type_y1, type_y2, type_u, type_v;

    uint32_t *p = cmdbuffer;

    *p++ = 0xE92D4FF0; // stmfd  sp!, {r4-r11, lr} @ save all registers

    // Process a pair of destination pixels per loop iteration (it should result in 32-bit value write)
    for (i = 0; i < dest_w; i += 2) {
        p = generate_32bit_scaled_data_write(
            p, quality, orig_w, dest_w,
            0, 1, i + 0,
            1, 2, i,
            0, 1, i + 1,
            2, 2, i);
    }
    *p++ = 0xE8BD8FF0; // ldmfd  sp!, {r4-r11, pc} @ restore all registers and return
    cmdcount = p - cmdbuffer;

#ifdef JIT_DEBUG
    printf("@ number of instructions = %d\n", cmdcount);
    FILE *f = fopen("cmdbuf.bin", "w+");
    fwrite(cmdbuffer, 1, INSTRUCTIONS_CACHE_SIZE, f);
    fclose(f);
#endif
    return cmdcount;
}

static int generate_yuv420p_to_yuv420_line_scaler(uint32_t *cmdbuffer, int maxcmdcount, int orig_w, int dest_w, int quality)
{
    int i = 0, p1, p2, cmdcount;
    int type_y1, type_y2, type_u, type_v;

    uint32_t *p = cmdbuffer;

    #define SRC_Y 0
    #define SRC_U 1

    *p++ = 0xE92D4FF0; // stmfd  sp!, {r4-r11, lr} @ save all registers

    while (i + 8 <= dest_w) {
        p = generate_32bit_scaled_data_write(
            p, quality, orig_w, dest_w,
            SRC_Y, 1, i + 0 * 1,
            SRC_U, 2, i + 0 * 2,
            SRC_U, 2, i + 1 * 2,
            SRC_Y, 1, i + 1 * 1);
        p = generate_32bit_scaled_data_write(
            p, quality, orig_w, dest_w,
            SRC_Y, 1, i + 3 * 1,
            SRC_Y, 1, i + 2 * 1,
            SRC_Y, 1, i + 4 * 1,
            SRC_U, 2, i + 2 * 2);
        p = generate_32bit_scaled_data_write(
            p, quality, orig_w, dest_w,
            SRC_U, 2, i + 3 * 2,
            SRC_Y, 1, i + 5 * 1,
            SRC_Y, 1, i + 7 * 1,
            SRC_Y, 1, i + 6 * 1);
        i += 8;
    }
    *p++ = 0xE8BD8FF0; // ldmfd  sp!, {r4-r11, pc} @ restore all registers and return
    cmdcount = p - cmdbuffer;

#ifdef JIT_DEBUG
    printf("@ number of instructions = %d\n", cmdcount);
    FILE *f = fopen("cmdbuf.bin", "w+");
    fwrite(cmdbuffer, 1, INSTRUCTIONS_CACHE_SIZE, f);
    fclose(f);
#endif
    return cmdcount;
}


/******************************************************************************/

static struct SwsContextArmJit *sws_arm_jit_create_scaler_internal(int source_w, int source_h, int target_w, int target_h, int quality, int fmt)
{
    int i, p1, p2;
    uint32_t *p = mmap(0, INSTRUCTIONS_CACHE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fmt == FMT_OMAPFB_YUV422) {
        generate_yuv420p_to_yuyv422_line_scaler(p, INSTRUCTIONS_CACHE_SIZE / 4, source_w, target_w, quality);
    } else if (fmt == FMT_OMAPFB_YUV420) {
        generate_yuv420p_to_yuv420_line_scaler(p, INSTRUCTIONS_CACHE_SIZE / 4, source_w, target_w, quality);
    } else {
        return NULL;
    }
    
    int *linebuffer = (int *)malloc(target_h * sizeof(int));
    for (i = 0; i < target_h; i ++) {
        get_pix(1, source_h, target_h, i, &p1, &p2);
        linebuffer[i] = p1;
    }

    __clear_cache((char *)p, (char *)p + INSTRUCTIONS_CACHE_SIZE);

    SwsContextArmJit *context = (SwsContextArmJit *)malloc(sizeof(SwsContextArmJit));
    memset(context, 0, sizeof(SwsContextArmJit));
    context->source_w = source_w;
    context->source_h = source_h;
    context->target_w = target_w;
    context->target_h = target_h;
    context->codebuffer = p;
    context->linebuffer = linebuffer;
    context->fmt = fmt;
    context->armv6_is_supported = 0;
    return context;
}

struct SwsContextArmJit *sws_arm_jit_create_omapfb_yuv422_scaler(int source_w, int source_h, int target_w, int target_h, int quality)
{
    return sws_arm_jit_create_scaler_internal(source_w, source_h, target_w, target_h, quality, FMT_OMAPFB_YUV422);
}

struct SwsContextArmJit *sws_arm_jit_create_omapfb_yuv420_scaler(int source_w, int source_h, int target_w, int target_h, int quality)
{
    return sws_arm_jit_create_scaler_internal(source_w, source_h, target_w, target_h, quality, FMT_OMAPFB_YUV420);
}

struct SwsContextArmJit *sws_arm_jit_create_omapfb_yuv420_scaler_armv6(int source_w, int source_h, int target_w, int target_h, int quality)
{
    struct SwsContextArmJit *s = sws_arm_jit_create_scaler_internal(source_w, source_h, target_w, target_h, quality, FMT_OMAPFB_YUV420);
    if (s) s->armv6_is_supported = 1;
    return s;
}

void sws_arm_jit_free(SwsContextArmJit *context)
{
    if (!context) return;
    munmap(context->codebuffer, INSTRUCTIONS_CACHE_SIZE);
    free(context->linebuffer);
    free(context);
}

static int sws_arm_jit_vscaleonly_internal(SwsContextArmJit *context, uint8_t* src[], int srcStride[], uint8_t* dst[], int dstStride[])
{
    int i, j;

    if (context->fmt == FMT_OMAPFB_YUV420) {
        void (*yv12_to_yuv420_line)(uint16_t *dst, const uint16_t *src_y, const uint8_t *src_c, int w) =
            yv12_to_yuv420_line_arm;
        if (context->armv6_is_supported) yv12_to_yuv420_line = yv12_to_yuv420_line_armv6;

        for (i = 0; i < context->target_h; i++) {
            j = context->linebuffer[i];
            if (i & 1) {
                yv12_to_yuv420_line((uint16_t *)(dst[0] + i * dstStride[0]),
                    src[0] + j * srcStride[0], src[2] + (j / 2) * srcStride[2], context->target_w);
            } else {
                yv12_to_yuv420_line((uint16_t *)(dst[0] + i * dstStride[0]),
                    src[0] + j * srcStride[0], src[1] + (j / 2) * srcStride[1], context->target_w);
            }
        }
        return 1;
    } else if (context->fmt == FMT_OMAPFB_YUV422) {
        void (*yv12_to_yuy2_line)(uint16_t *dst, const uint16_t *src_y, const uint8_t *src_u, const uint8_t *src_v, int w) =
            yv12_to_yuy2_line_arm;
        for (i = 0; i < context->target_h; i++) {
            j = context->linebuffer[i];
            yv12_to_yuy2_line(
                dst[0] + i * dstStride[0],
                src[0] + j * srcStride[0],
                src[1] + (j / 2) * srcStride[1],
                src[2] + (j / 2) * srcStride[2],
                context->target_w);
        }
        return 1;
    }
    return 0;
}

static int sws_arm_jit_scale_internal(SwsContextArmJit *context, uint8_t* src[], int srcStride[], uint8_t* dst[], int dstStride[])
{
    int i, j;
    void (*scale_line)(uint8_t *y, uint8_t *u, uint8_t *v, uint8_t *out) =
        (void (*)(uint8_t *, uint8_t *, uint8_t *, uint8_t *))context->codebuffer;

    if (context->source_w == context->target_w)
        return sws_arm_jit_vscaleonly_internal(context, src, srcStride, dst, dstStride);

    if (context->fmt == FMT_OMAPFB_YUV422) {
        for (i = 0; i < context->target_h; i++) {
            j = context->linebuffer[i];
            scale_line(
                src[0] + j * srcStride[0],
                src[1] + (j / 2) * srcStride[1],
                src[2] + (j / 2) * srcStride[2],
                dst[0] + i * dstStride[0]);
        }
        return 1;
    } else if (context->fmt == FMT_OMAPFB_YUV420) {
        for (i = 0; i < context->target_h; i++) {
            j = context->linebuffer[i];
            scale_line(
                src[0] + j * srcStride[0],
                (i & 1) ? (src[2] + (j / 2) * srcStride[2]) : (src[1] + (j / 2) * srcStride[1]),
                0,
                dst[0] + i * dstStride[0]);
        }
        return 1;
    }
    return 0;
}

int sws_arm_jit_scale(SwsContextArmJit *context, uint8_t* src[], int srcStride[], int y, int h, uint8_t* dst[], int dstStride[])
{
    if (y != 0 || h != context->source_h) return 0; // Slices are not supported yet
    return sws_arm_jit_scale_internal(context, src, srcStride, dst, dstStride);
}
