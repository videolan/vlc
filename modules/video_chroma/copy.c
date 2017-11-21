/*****************************************************************************
 * copy.c: Fast YV12/NV12 copy
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Victorien Le Couviour--Tuffet <victorien.lecouviour.tuffet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef COPY_TEST
# undef NDEBUG
#endif

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_cpu.h>
#include <assert.h>

#include "copy.h"

#define ASSERT_PLANE(i) assert(src[i]); \
    assert(src_pitch[i])

#define ASSERT_2PLANES \
    assert(dst); \
    ASSERT_PLANE(0); \
    ASSERT_PLANE(1); \
    assert(height)

#define ASSERT_3PLANES ASSERT_2PLANES; \
    ASSERT_PLANE(2)

int CopyInitCache(copy_cache_t *cache, unsigned width)
{
#ifdef CAN_COMPILE_SSE2
    cache->size = __MAX((width + 0x3f) & ~ 0x3f, 16384);
    cache->buffer = aligned_alloc(64, cache->size);
    if (!cache->buffer)
        return VLC_EGENERIC;
#else
    (void) cache; (void) width;
#endif
    return VLC_SUCCESS;
}

void CopyCleanCache(copy_cache_t *cache)
{
#ifdef CAN_COMPILE_SSE2
    aligned_free(cache->buffer);
    cache->buffer = NULL;
    cache->size   = 0;
#else
    (void) cache;
#endif
}

#ifdef CAN_COMPILE_SSE2
/* Copy 16/64 bytes from srcp to dstp loading data with the SSE>=2 instruction
 * load and storing data with the SSE>=2 instruction store.
 */
#define COPY16(dstp, srcp, load, store) \
    asm volatile (                      \
        load "  0(%[src]), %%xmm1\n"    \
        store " %%xmm1,    0(%[dst])\n" \
        : : [dst]"r"(dstp), [src]"r"(srcp) : "memory", "xmm1")

#define COPY64(dstp, srcp, load, store) \
    asm volatile (                      \
        load "  0(%[src]), %%xmm1\n"    \
        load " 16(%[src]), %%xmm2\n"    \
        load " 32(%[src]), %%xmm3\n"    \
        load " 48(%[src]), %%xmm4\n"    \
        store " %%xmm1,    0(%[dst])\n" \
        store " %%xmm2,   16(%[dst])\n" \
        store " %%xmm3,   32(%[dst])\n" \
        store " %%xmm4,   48(%[dst])\n" \
        : : [dst]"r"(dstp), [src]"r"(srcp) : "memory", "xmm1", "xmm2", "xmm3", "xmm4")

#ifndef __SSE4_1__
# undef vlc_CPU_SSE4_1
# define vlc_CPU_SSE4_1() ((cpu & VLC_CPU_SSE4_1) != 0)
#endif

#ifndef __SSSE3__
# undef vlc_CPU_SSSE3
# define vlc_CPU_SSSE3() ((cpu & VLC_CPU_SSSE3) != 0)
#endif

#ifndef __SSE2__
# undef vlc_CPU_SSE2
# define vlc_CPU_SSE2() ((cpu & VLC_CPU_SSE2) != 0)
#endif

#ifdef COPY_TEST_NOOPTIM
# undef vlc_CPU_SSE4_1
# define vlc_CPU_SSE4_1() (0)
# undef vlc_CPU_SSE3
# define vlc_CPU_SSE3() (0)
# undef vlc_CPU_SSSE3
# define vlc_CPU_SSSE3() (0)
# undef vlc_CPU_SSE2
# define vlc_CPU_SSE2() (0)
#endif

/* Optimized copy from "Uncacheable Speculative Write Combining" memory
 * as used by some video surface.
 * XXX It is really efficient only when SSE4.1 is available.
 */
VLC_SSE
static void CopyFromUswc(uint8_t *dst, size_t dst_pitch,
                         const uint8_t *src, size_t src_pitch,
                         unsigned width, unsigned height,
                         unsigned cpu)
{
#if defined (__SSE4_1__) || !defined(CAN_COMPILE_SSSE3)
    VLC_UNUSED(cpu);
#endif
    assert(((intptr_t)dst & 0x0f) == 0 && (dst_pitch & 0x0f) == 0);

    asm volatile ("mfence");

    for (unsigned y = 0; y < height; y++) {
        const unsigned unaligned = (-(uintptr_t)src) & 0x0f;
        unsigned x = unaligned;

#ifdef CAN_COMPILE_SSE4_1
        if (vlc_CPU_SSE4_1()) {
            if (!unaligned) {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movntdqa", "movdqa");
            } else {
                COPY16(dst, src, "movdqu", "movdqa");
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movntdqa", "movdqu");
            }
        } else
#endif
        {
            if (!unaligned) {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movdqa", "movdqa");
            } else {
                COPY16(dst, src, "movdqu", "movdqa");
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movdqa", "movdqu");
            }
        }

        for (; x < width; x++)
            dst[x] = src[x];

        src += src_pitch;
        dst += dst_pitch;
    }
    asm volatile ("mfence");
}

VLC_SSE
static void Copy2d(uint8_t *dst, size_t dst_pitch,
                   const uint8_t *src, size_t src_pitch,
                   unsigned width, unsigned height)
{
    assert(((intptr_t)src & 0x0f) == 0 && (src_pitch & 0x0f) == 0);

    for (unsigned y = 0; y < height; y++) {
        unsigned x = 0;

        bool unaligned = ((intptr_t)dst & 0x0f) != 0;
        if (!unaligned) {
            for (; x+63 < width; x += 64)
                COPY64(&dst[x], &src[x], "movdqa", "movntdq");
        } else {
            for (; x+63 < width; x += 64)
                COPY64(&dst[x], &src[x], "movdqa", "movdqu");
        }

        for (; x < width; x++)
            dst[x] = src[x];

        src += src_pitch;
        dst += dst_pitch;
    }
}

VLC_SSE
static void
SSE_InterleaveUV(uint8_t *dst, size_t dst_pitch,
                 uint8_t *srcu, size_t srcu_pitch,
                 uint8_t *srcv, size_t srcv_pitch,
                 unsigned int width, unsigned int height, uint8_t pixel_size,
                 unsigned int cpu)
{
    assert(!((intptr_t)srcu & 0xf) && !(srcu_pitch & 0x0f) &&
           !((intptr_t)srcv & 0xf) && !(srcv_pitch & 0x0f));

#if defined(__SSSE3__) || !defined (CAN_COMPILE_SSSE3)
    VLC_UNUSED(cpu);
#endif

    static const uint8_t shuffle_8[] = { 0, 8,
                                         1, 9,
                                         2, 10,
                                         3, 11,
                                         4, 12,
                                         5, 13,
                                         6, 14,
                                         7, 15 };
    static const uint8_t shuffle_16[] = { 0, 1, 8, 9,
                                          2, 3, 10, 11,
                                          4, 5, 12, 13,
                                          6, 7, 14, 15 };
    const uint8_t *shuffle = pixel_size == 1 ? shuffle_8 : shuffle_16;

    for (unsigned int y = 0; y < height; ++y)
    {
        unsigned int    x;

#define LOAD2X32                        \
    "movhpd 0x00(%[src2]), %%xmm0\n"    \
    "movlpd 0x00(%[src1]), %%xmm0\n"    \
                                        \
    "movhpd 0x08(%[src2]), %%xmm1\n"    \
    "movlpd 0x08(%[src1]), %%xmm1\n"    \
                                        \
    "movhpd 0x10(%[src2]), %%xmm2\n"    \
    "movlpd 0x10(%[src1]), %%xmm2\n"    \
                                        \
    "movhpd 0x18(%[src2]), %%xmm3\n"    \
    "movlpd 0x18(%[src1]), %%xmm3\n"

#define STORE64                         \
    "movdqu %%xmm0, 0x00(%[dst])\n"     \
    "movdqu %%xmm1, 0x10(%[dst])\n"     \
    "movdqu %%xmm2, 0x20(%[dst])\n"     \
    "movdqu %%xmm3, 0x30(%[dst])\n"

#ifdef CAN_COMPILE_SSSE3
        if (vlc_CPU_SSSE3())
            for (x = 0; x < (width & ~31); x += 32)
                asm volatile
                    (
                        "movdqu (%[shuffle]), %%xmm7\n"
                        LOAD2X32
                        "pshufb %%xmm7, %%xmm0\n"
                        "pshufb %%xmm7, %%xmm1\n"
                        "pshufb %%xmm7, %%xmm2\n"
                        "pshufb %%xmm7, %%xmm3\n"
                        STORE64
                        : : [dst]"r"(dst+2*x),
                            [src1]"r"(srcu+x), [src2]"r"(srcv+x),
                            [shuffle]"r"(shuffle)
                        : "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm7"
                    );
        else
#endif

        {
            assert(pixel_size == 1);
            for (x = 0; x < (width & ~31); x += 32)
                asm volatile
                    (
                        LOAD2X32
                        "movhlps   %%xmm0, %%xmm4\n"
                        "punpcklbw %%xmm4, %%xmm0\n"

                        "movhlps   %%xmm1, %%xmm4\n"
                        "punpcklbw %%xmm4, %%xmm1\n"

                        "movhlps   %%xmm2, %%xmm4\n"
                        "punpcklbw %%xmm4, %%xmm2\n"

                        "movhlps   %%xmm3, %%xmm4\n"
                        "punpcklbw %%xmm4, %%xmm3\n"
                        STORE64
                        : : [dst]"r"(dst+2*x),
                            [src1]"r"(srcu+x), [src2]"r"(srcv+x)
                        : "memory",
                          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm7"
                    );
        }
#undef LOAD2X32
#undef STORE64

        if (pixel_size == 1)
        {
            for (; x < width; x++) {
                dst[2*x+0] = srcu[x];
                dst[2*x+1] = srcv[x];
            }
        }
        else
        {
            for (; x < width; x+= 2) {
                dst[2*x+0] = srcu[x];
                dst[2*x+1] = srcu[x + 1];
                dst[2*x+2] = srcv[x];
                dst[2*x+3] = srcv[x + 1];
            }
        }
        srcu += srcu_pitch;
        srcv += srcv_pitch;
        dst += dst_pitch;
    }
}

VLC_SSE
static void SSE_SplitUV(uint8_t *dstu, size_t dstu_pitch,
                        uint8_t *dstv, size_t dstv_pitch,
                        const uint8_t *src, size_t src_pitch,
                        unsigned width, unsigned height, uint8_t pixel_size,
                        unsigned cpu)
{
#if defined(__SSSE3__) || !defined (CAN_COMPILE_SSSE3)
    VLC_UNUSED(cpu);
#endif
    assert(pixel_size == 1 || pixel_size == 2);
    assert(((intptr_t)src & 0xf) == 0 && (src_pitch & 0x0f) == 0);

#define LOAD64 \
    "movdqa  0(%[src]), %%xmm0\n" \
    "movdqa 16(%[src]), %%xmm1\n" \
    "movdqa 32(%[src]), %%xmm2\n" \
    "movdqa 48(%[src]), %%xmm3\n"

#define STORE2X32 \
    "movq   %%xmm0,   0(%[dst1])\n" \
    "movq   %%xmm1,   8(%[dst1])\n" \
    "movhpd %%xmm0,   0(%[dst2])\n" \
    "movhpd %%xmm1,   8(%[dst2])\n" \
    "movq   %%xmm2,  16(%[dst1])\n" \
    "movq   %%xmm3,  24(%[dst1])\n" \
    "movhpd %%xmm2,  16(%[dst2])\n" \
    "movhpd %%xmm3,  24(%[dst2])\n"

#ifdef CAN_COMPILE_SSSE3
    if (vlc_CPU_SSSE3())
    {
        static const uint8_t shuffle_8[] = { 0, 2, 4, 6, 8, 10, 12, 14,
                                             1, 3, 5, 7, 9, 11, 13, 15 };
        static const uint8_t shuffle_16[] = {  0,  1,  4,  5,  8,  9, 12, 13,
                                               2,  3,  6,  7, 10, 11, 14, 15 };
        const uint8_t *shuffle = pixel_size == 1 ? shuffle_8 : shuffle_16;
        for (unsigned y = 0; y < height; y++) {
            unsigned x = 0;
            for (; x < (width & ~31); x += 32) {
                asm volatile (
                    "movdqu (%[shuffle]), %%xmm7\n"
                    LOAD64
                    "pshufb  %%xmm7, %%xmm0\n"
                    "pshufb  %%xmm7, %%xmm1\n"
                    "pshufb  %%xmm7, %%xmm2\n"
                    "pshufb  %%xmm7, %%xmm3\n"
                    STORE2X32
                    : : [dst1]"r"(&dstu[x]), [dst2]"r"(&dstv[x]), [src]"r"(&src[2*x]), [shuffle]"r"(shuffle) : "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm7");
            }
            if (pixel_size == 1)
            {
                for (; x < width; x++) {
                    dstu[x] = src[2*x+0];
                    dstv[x] = src[2*x+1];
                }
            }
            else
            {
                for (; x < width; x+= 2) {
                    dstu[x] = src[2*x+0];
                    dstu[x+1] = src[2*x+1];
                    dstv[x] = src[2*x+2];
                    dstv[x+1] = src[2*x+3];
                }
            }
            src  += src_pitch;
            dstu += dstu_pitch;
            dstv += dstv_pitch;
        }
    } else
#endif
    {
        assert(pixel_size == 1);
        static const uint8_t mask[] = { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
                                        0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00 };

        for (unsigned y = 0; y < height; y++)
        {
            unsigned x = 0;
            for (; x < (width & ~31); x += 32) {
                asm volatile (
                    "movdqu (%[mask]), %%xmm7\n"
                    LOAD64
                    "movdqa   %%xmm0, %%xmm4\n"
                    "movdqa   %%xmm1, %%xmm5\n"
                    "movdqa   %%xmm2, %%xmm6\n"
                    "psrlw    $8,     %%xmm0\n"
                    "psrlw    $8,     %%xmm1\n"
                    "pand     %%xmm7, %%xmm4\n"
                    "pand     %%xmm7, %%xmm5\n"
                    "pand     %%xmm7, %%xmm6\n"
                    "packuswb %%xmm4, %%xmm0\n"
                    "packuswb %%xmm5, %%xmm1\n"
                    "pand     %%xmm3, %%xmm7\n"
                    "psrlw    $8,     %%xmm2\n"
                    "psrlw    $8,     %%xmm3\n"
                    "packuswb %%xmm6, %%xmm2\n"
                    "packuswb %%xmm7, %%xmm3\n"
                    STORE2X32
                    : : [dst2]"r"(&dstu[x]), [dst1]"r"(&dstv[x]), [src]"r"(&src[2*x]), [mask]"r"(mask) : "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
            }
            for (; x < width; x++) {
                dstu[x] = src[2*x+0];
                dstv[x] = src[2*x+1];
            }
            src  += src_pitch;
            dstu += dstu_pitch;
            dstv += dstv_pitch;
        }
    }
#undef STORE2X32
#undef LOAD64
}

static void SSE_CopyPlane(uint8_t *dst, size_t dst_pitch,
                          const uint8_t *src, size_t src_pitch,
                          uint8_t *cache, size_t cache_size,
                          unsigned height, unsigned cpu)
{
    const unsigned w16 = (src_pitch+15) & ~15;
    const unsigned hstep = cache_size / w16;
    assert(hstep > 0);

    if (src_pitch == dst_pitch)
        memcpy(dst, src, src_pitch * height);
    else
    for (unsigned y = 0; y < height; y += hstep) {
        const unsigned hblock =  __MIN(hstep, height - y);

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w16,
                     src, src_pitch,
                     src_pitch, hblock, cpu);

        /* Copy from our cache to the destination */
        Copy2d(dst, dst_pitch,
               cache, w16,
               src_pitch, hblock);

        /* */
        src += src_pitch * hblock;
        dst += dst_pitch * hblock;
    }
}

static void
SSE_InterleavePlanes(uint8_t *dst, size_t dst_pitch,
                     const uint8_t *srcu, size_t srcu_pitch,
                     const uint8_t *srcv, size_t srcv_pitch,
                     uint8_t *cache, size_t cache_size,
                     unsigned int height, uint8_t pixel_size, unsigned int cpu)
{
    assert(srcu_pitch == srcv_pitch);
    unsigned int const  w16 = (srcu_pitch+15) & ~15;
    unsigned int const  hstep = (cache_size) / (2*w16);
    assert(hstep > 0);

    for (unsigned int y = 0; y < height; y += hstep)
    {
        unsigned int const      hblock = __MIN(hstep, height - y);

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w16, srcu, srcu_pitch,
                     srcu_pitch, hblock, cpu);
        CopyFromUswc(cache+w16*hblock, w16, srcv, srcv_pitch,
                     srcv_pitch, hblock, cpu);

        /* Copy from our cache to the destination */
        SSE_InterleaveUV(dst, dst_pitch, cache, w16,
                         cache+w16*hblock, w16, srcu_pitch, hblock, pixel_size,
                         cpu);

        /* */
        srcu += hblock * srcu_pitch;
        srcv += hblock * srcv_pitch;
        dst += hblock * dst_pitch;
    }
}

static void SSE_SplitPlanes(uint8_t *dstu, size_t dstu_pitch,
                            uint8_t *dstv, size_t dstv_pitch,
                            const uint8_t *src, size_t src_pitch,
                            uint8_t *cache, size_t cache_size,
                            unsigned height, uint8_t pixel_size, unsigned cpu)
{
    const unsigned w16 = (src_pitch+15) & ~15;
    const unsigned hstep = cache_size / w16;
    assert(hstep > 0);

    for (unsigned y = 0; y < height; y += hstep) {
        const unsigned hblock =  __MIN(hstep, height - y);

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w16, src, src_pitch,
                     src_pitch, hblock, cpu);

        /* Copy from our cache to the destination */
        SSE_SplitUV(dstu, dstu_pitch, dstv, dstv_pitch,
                    cache, w16, src_pitch / 2, hblock, pixel_size, cpu);

        /* */
        src  += src_pitch  * hblock;
        dstu += dstu_pitch * hblock;
        dstv += dstv_pitch * hblock;
    }
}

static void SSE_Copy420_P_to_P(picture_t *dst, const uint8_t *src[static 3],
                               const size_t src_pitch[static 3], unsigned height,
                               const copy_cache_t *cache, unsigned cpu)
{
    for (unsigned n = 0; n < 3; n++) {
        const unsigned d = n > 0 ? 2 : 1;
        SSE_CopyPlane(dst->p[n].p_pixels, dst->p[n].i_pitch,
                      src[n], src_pitch[n],
                      cache->buffer, cache->size,
                      (height+d-1)/d, cpu);
    }
    asm volatile ("emms");
}


static void SSE_Copy420_SP_to_SP(picture_t *dst, const uint8_t *src[static 2],
                                 const size_t src_pitch[static 2], unsigned height,
                                 const copy_cache_t *cache, unsigned cpu)
{
    SSE_CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
                  src[0], src_pitch[0],
                  cache->buffer, cache->size,
                  height, cpu);
    SSE_CopyPlane(dst->p[1].p_pixels, dst->p[1].i_pitch,
                  src[1], src_pitch[1],
                  cache->buffer, cache->size,
                  height/2, cpu);
    asm volatile ("emms");
}

static void
SSE_Copy420_SP_to_P(picture_t *dest, const uint8_t *src[static 2],
                    const size_t src_pitch[static 2], unsigned int height,
                    const copy_cache_t *cache, uint8_t pixel_size,
                    unsigned int cpu)
{
    SSE_CopyPlane(dest->p[0].p_pixels, dest->p[0].i_pitch,
                  src[0], src_pitch[0], cache->buffer, cache->size,
                  height, cpu);
    SSE_SplitPlanes(dest->p[1].p_pixels, dest->p[1].i_pitch,
                    dest->p[2].p_pixels, dest->p[2].i_pitch,
                    src[1], src_pitch[1], cache->buffer, cache->size,
                    height / 2, pixel_size, cpu);
    asm volatile ("emms");
}

static void SSE_Copy420_P_to_SP(picture_t *dst, const uint8_t *src[static 3],
                                const size_t src_pitch[static 3],
                                unsigned height, const copy_cache_t *cache,
                                uint8_t pixel_size, unsigned cpu)
{
    SSE_CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
                  src[0], src_pitch[0],
                  cache->buffer, cache->size,
                  height, cpu);
    SSE_InterleavePlanes(dst->p[1].p_pixels, dst->p[1].i_pitch,
                         src[U_PLANE], src_pitch[U_PLANE],
                         src[V_PLANE], src_pitch[V_PLANE],
                         cache->buffer, cache->size, height / 2, pixel_size, cpu);
    asm volatile ("emms");
}
#undef COPY64
#endif /* CAN_COMPILE_SSE2 */

static void CopyPlane(uint8_t *dst, size_t dst_pitch,
                      const uint8_t *src, size_t src_pitch,
                      unsigned height)
{
    if (src_pitch == dst_pitch)
        memcpy(dst, src, src_pitch * height);
    else
    for (unsigned y = 0; y < height; y++) {
        memcpy(dst, src, src_pitch);
        src += src_pitch;
        dst += dst_pitch;
    }
}

void Copy420_SP_to_SP(picture_t *dst, const uint8_t *src[static 2],
                      const size_t src_pitch[static 2], unsigned height,
                      const copy_cache_t *cache)
{
    ASSERT_2PLANES;
#ifdef CAN_COMPILE_SSE2
    unsigned cpu = vlc_CPU();
    if (vlc_CPU_SSE2())
        return SSE_Copy420_SP_to_SP(dst, src, src_pitch, height,
                                    cache, cpu);
#else
    (void) cache;
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height);
    CopyPlane(dst->p[1].p_pixels, dst->p[1].i_pitch,
              src[1], src_pitch[1], height/2);
}

#define SPLIT_PLANES(type, pitch_den) do { \
    for (unsigned y = 0; y < height; y++) { \
        for (unsigned x = 0; x < src_pitch / pitch_den; x++) { \
            ((type *) dstu)[x] = ((const type *) src)[2*x+0]; \
            ((type *) dstv)[x] = ((const type *) src)[2*x+1]; \
        } \
        src  += src_pitch; \
        dstu += dstu_pitch; \
        dstv += dstv_pitch; \
    } \
} while(0)

static void SplitPlanes(uint8_t *dstu, size_t dstu_pitch,
                        uint8_t *dstv, size_t dstv_pitch,
                        const uint8_t *src, size_t src_pitch, unsigned height)
{
    SPLIT_PLANES(uint8_t, 2);
}

static void SplitPlanes16(uint8_t *dstu, size_t dstu_pitch,
                          uint8_t *dstv, size_t dstv_pitch,
                          const uint8_t *src, size_t src_pitch, unsigned height)
{
    SPLIT_PLANES(uint16_t, 4);
}

void Copy420_SP_to_P(picture_t *dst, const uint8_t *src[static 2],
                     const size_t src_pitch[static 2], unsigned height,
                     const copy_cache_t *cache)
{
    ASSERT_2PLANES;
#ifdef CAN_COMPILE_SSE2
    unsigned    cpu = vlc_CPU();

    if (vlc_CPU_SSE2())
        return SSE_Copy420_SP_to_P(dst, src, src_pitch, height, cache, 1, cpu);
#else
    VLC_UNUSED(cache);
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height);
    SplitPlanes(dst->p[1].p_pixels, dst->p[1].i_pitch,
                dst->p[2].p_pixels, dst->p[2].i_pitch,
                src[1], src_pitch[1], height/2);
}

void Copy420_16_SP_to_P(picture_t *dst, const uint8_t *src[static 2],
                        const size_t src_pitch[static 2], unsigned height,
                        const copy_cache_t *cache)
{
    ASSERT_2PLANES;
#ifdef CAN_COMPILE_SSE3
    unsigned    cpu = vlc_CPU();

    if (vlc_CPU_SSSE3())
        return SSE_Copy420_SP_to_P(dst, src, src_pitch, height, cache, 2, cpu);
#else
    VLC_UNUSED(cache);
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height);
    SplitPlanes16(dst->p[1].p_pixels, dst->p[1].i_pitch,
                  dst->p[2].p_pixels, dst->p[2].i_pitch,
                  src[1], src_pitch[1], height/2);
}

#define INTERLEAVE_UV() do { \
    for ( unsigned int line = 0; line < copy_lines; line++ ) { \
        for ( unsigned int col = 0; col < copy_pitch; col++ ) { \
            *dstUV++ = *srcU++; \
            *dstUV++ = *srcV++; \
        } \
        dstUV += i_extra_pitch_uv; \
        srcU  += i_extra_pitch_u; \
        srcV  += i_extra_pitch_v; \
    } \
}while(0)

void Copy420_P_to_SP(picture_t *dst, const uint8_t *src[static 3],
                     const size_t src_pitch[static 3], unsigned height,
                     const copy_cache_t *cache)
{
    ASSERT_3PLANES;
#ifdef CAN_COMPILE_SSE2
    unsigned cpu = vlc_CPU();
    if (vlc_CPU_SSE2())
        return SSE_Copy420_P_to_SP(dst, src, src_pitch, height, cache, 1, cpu);
#else
    (void) cache;
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height);

    const unsigned copy_lines = height / 2;
    const unsigned copy_pitch = src_pitch[1];

    const int i_extra_pitch_uv = dst->p[1].i_pitch - 2 * copy_pitch;
    const int i_extra_pitch_u  = src_pitch[U_PLANE] - copy_pitch;
    const int i_extra_pitch_v  = src_pitch[V_PLANE] - copy_pitch;

    uint8_t *dstUV = dst->p[1].p_pixels;
    const uint8_t *srcU  = src[U_PLANE];
    const uint8_t *srcV  = src[V_PLANE];
    INTERLEAVE_UV();
}

void Copy420_16_P_to_SP(picture_t *dst, const uint8_t *src[static 3],
                        const size_t src_pitch[static 3], unsigned height,
                        const copy_cache_t *cache)
{
    ASSERT_3PLANES;
#ifdef CAN_COMPILE_SSE2
    unsigned cpu = vlc_CPU();
    if (vlc_CPU_SSSE3())
        return SSE_Copy420_P_to_SP(dst, src, src_pitch, height, cache, 2, cpu);
#else
    (void) cache;
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height);

    const unsigned copy_lines = height / 2;
    const unsigned copy_pitch = src_pitch[1] / 2;

    const int i_extra_pitch_uv = dst->p[1].i_pitch / 2 - 2 * copy_pitch;
    const int i_extra_pitch_u  = src_pitch[U_PLANE] / 2 - copy_pitch;
    const int i_extra_pitch_v  = src_pitch[V_PLANE] / 2 - copy_pitch;

    uint16_t *dstUV = (void*) dst->p[1].p_pixels;
    const uint16_t *srcU  = (const uint16_t *) src[U_PLANE];
    const uint16_t *srcV  = (const uint16_t *) src[V_PLANE];
    INTERLEAVE_UV();
}

void CopyFromI420_10ToP010(picture_t *dst, const uint8_t *src[static 3],
                           const size_t src_pitch[static 3],
                           unsigned height, const copy_cache_t *cache)
{
    (void) cache;

    const int i_extra_pitch_dst_y = (dst->p[0].i_pitch  - src_pitch[0]) / 2;
    const int i_extra_pitch_src_y = (src_pitch[Y_PLANE] - src_pitch[0]) / 2;
    uint16_t *dstY = (uint16_t *) dst->p[0].p_pixels;
    const uint16_t *srcY = (const uint16_t *) src[Y_PLANE];
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < (src_pitch[0] / 2); x++) {
            *dstY++ = *srcY++ << 6;
        }
        dstY += i_extra_pitch_dst_y;
        srcY += i_extra_pitch_src_y;
    }

    const unsigned copy_lines = height / 2;
    const unsigned copy_pitch = src_pitch[1] / 2;

    const int i_extra_pitch_uv = dst->p[1].i_pitch / 2 - 2 * copy_pitch;
    const int i_extra_pitch_u  = src_pitch[U_PLANE] / 2 - copy_pitch;
    const int i_extra_pitch_v  = src_pitch[V_PLANE] / 2 - copy_pitch;

    uint16_t *dstUV = (uint16_t *) dst->p[1].p_pixels;
    const uint16_t *srcU  = (const uint16_t *) src[U_PLANE];
    const uint16_t *srcV  = (const uint16_t *) src[V_PLANE];
    for ( unsigned int line = 0; line < copy_lines; line++ )
    {
        for ( unsigned int col = 0; col < copy_pitch; col++ )
        {
            *dstUV++ = *srcU++ << 6;
            *dstUV++ = *srcV++ << 6;
        }
        dstUV += i_extra_pitch_uv;
        srcU  += i_extra_pitch_u;
        srcV  += i_extra_pitch_v;
    }
}

void Copy420_P_to_P(picture_t *dst, const uint8_t *src[static 3],
                    const size_t src_pitch[static 3], unsigned height,
                    const copy_cache_t *cache)
{
    ASSERT_3PLANES;
#ifdef CAN_COMPILE_SSE2
    unsigned cpu = vlc_CPU();
    if (vlc_CPU_SSE2())
        return SSE_Copy420_P_to_P(dst, src, src_pitch, height, cache, cpu);
#else
    (void) cache;
#endif

     CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
               src[0], src_pitch[0], height);
     CopyPlane(dst->p[1].p_pixels, dst->p[1].i_pitch,
               src[1], src_pitch[1], height / 2);
     CopyPlane(dst->p[2].p_pixels, dst->p[2].i_pitch,
               src[2], src_pitch[2], height / 2);
}

void picture_SwapUV(picture_t *picture)
{
    assert(picture->i_planes == 3);

    plane_t tmp_plane = picture->p[1];
    picture->p[1] = picture->p[2];
    picture->p[2] = tmp_plane;
}

int picture_UpdatePlanes(picture_t *picture, uint8_t *data, unsigned pitch)
{
    /* fill in buffer info in first plane */
    picture->p->p_pixels = data;
    picture->p->i_pitch  = pitch;
    picture->p->i_lines  = picture->format.i_height;
    assert(picture->p->i_visible_pitch <= picture->p->i_pitch);
    assert(picture->p->i_visible_lines <= picture->p->i_lines);

    /*  Fill chroma planes for biplanar YUV */
    if (picture->format.i_chroma == VLC_CODEC_NV12 ||
        picture->format.i_chroma == VLC_CODEC_NV21 ||
        picture->format.i_chroma == VLC_CODEC_P010) {

        for (int n = 1; n < picture->i_planes; n++) {
            const plane_t *o = &picture->p[n-1];
            plane_t *p = &picture->p[n];

            p->p_pixels = o->p_pixels + o->i_lines * o->i_pitch;
            p->i_pitch  = pitch;
            p->i_lines  = picture->format.i_height;
            assert(p->i_visible_pitch <= p->i_pitch);
            assert(p->i_visible_lines <= p->i_lines);
        }
        /* The dx/d3d buffer is always allocated as NV12 */
        if (vlc_fourcc_AreUVPlanesSwapped(picture->format.i_chroma, VLC_CODEC_NV12)) {
            /* TODO : Swap NV21 UV planes to match NV12 */
            return VLC_EGENERIC;
        }
    }

    /*  Fill chroma planes for planar YUV */
    else
    if (picture->format.i_chroma == VLC_CODEC_I420 ||
        picture->format.i_chroma == VLC_CODEC_J420 ||
        picture->format.i_chroma == VLC_CODEC_YV12) {

        for (int n = 1; n < picture->i_planes; n++) {
            const plane_t *o = &picture->p[n-1];
            plane_t *p = &picture->p[n];

            p->p_pixels = o->p_pixels + o->i_lines * o->i_pitch;
            p->i_pitch  = pitch / 2;
            p->i_lines  = picture->format.i_height / 2;
        }
        /* The dx/d3d buffer is always allocated as YV12 */
        if (vlc_fourcc_AreUVPlanesSwapped(picture->format.i_chroma, VLC_CODEC_YV12)) {
            uint8_t *p_tmp = picture->p[1].p_pixels;
            picture->p[1].p_pixels = picture->p[2].p_pixels;
            picture->p[2].p_pixels = p_tmp;
        }
    }
    return VLC_SUCCESS;
}

#ifdef COPY_TEST

#include <vlc_picture.h>

struct test_dst
{
    vlc_fourcc_t chroma;
    void (*conv)(picture_t *, const uint8_t *[], const size_t [], unsigned,
                 const copy_cache_t *);
};

struct test_conv
{
    vlc_fourcc_t src_chroma;
    struct test_dst dsts[3];
};

static const struct test_conv convs[] = {
    { .src_chroma = VLC_CODEC_NV12,
      .dsts = { { VLC_CODEC_I420, Copy420_SP_to_P },
                { VLC_CODEC_NV12, Copy420_SP_to_SP } },
    },
    { .src_chroma = VLC_CODEC_I420,
      .dsts = { { VLC_CODEC_I420, Copy420_P_to_P },
                { VLC_CODEC_NV12, Copy420_P_to_SP } },
    },
    { .src_chroma = VLC_CODEC_P010,
      .dsts = { { VLC_CODEC_I420_10B, Copy420_16_SP_to_P } },
    },
    { .src_chroma = VLC_CODEC_I420_10B,
      .dsts = { { VLC_CODEC_P010, Copy420_16_P_to_SP } },
    },
};
#define NB_CONVS ARRAY_SIZE(convs)

struct test_size
{
    int i_width;
    int i_height;
    int i_visible_width;
    int i_visible_height;
};
static const struct test_size sizes[] = {
    { 1, 1, 1, 1 },
    { 3, 3, 3, 3 },
    { 65, 39, 65, 39 },
    { 560, 369, 540, 350 },
    { 1274, 721, 1200, 720 },
    { 1920, 1088, 1920, 1080 },
    { 3840, 2160, 3840, 2160 },
#if 0 /* too long */
    { 8192, 8192, 8192, 8192 },
#endif
};
#define NB_SIZES ARRAY_SIZE(sizes)

static void piccheck(picture_t *pic, const vlc_chroma_description_t *dsc,
                     bool init)
{
#define ASSERT_COLOR() do { \
    fprintf(stderr, "error: pixel doesn't match @ plane: %d: %d x %d: %X\n", i, x, y, *(--p)); \
    assert(!"error: pixel doesn't match"); \
} while(0)

#define PICCHECK(type_u, type_uv, colors_P, color_UV, pitch_den) do { \
    for (int i = 0; i < pic->i_planes; ++i) \
    { \
        const struct plane_t *plane = &pic->p[i]; \
        for (int y = 0; y < plane->i_visible_lines; ++y) \
        { \
            if (pic->i_planes == 2 && i == 1) \
            { \
                type_uv *p = (type_uv *)&plane->p_pixels[y * plane->i_pitch]; \
                for (int x = 0; x < plane->i_visible_pitch / 2 / pitch_den; ++x) \
                    if (init) \
                        *(p++) = color_UV; \
                    else if (*(p++) != color_UV) \
                        ASSERT_COLOR(); \
            } \
            else \
            { \
                type_u *p = (type_u *) &plane->p_pixels[y * plane->i_pitch]; \
                for (int x = 0; x < plane->i_visible_pitch / pitch_den; ++x) \
                    if (init) \
                        *(p++) = colors_P[i]; \
                    else if (*(p++) != colors_P[i]) \
                        ASSERT_COLOR(); \
            } \
        } \
    } \
} while (0)

    assert(pic->i_planes == 2 || pic->i_planes == 3);
    const uint8_t colors_8_P[3] = { 0x42, 0xF1, 0x36 };
    const uint16_t color_8_UV = 0x36F1;

    const uint16_t colors_16_P[3] = { 0x4210, 0x14F1, 0x4536 };
    const uint32_t color_16_UV = 0x453614F1;

    assert(dsc->pixel_size == 1 || dsc->pixel_size == 2);
    if (dsc->pixel_size == 1)
        PICCHECK(uint8_t, uint16_t, colors_8_P, color_8_UV, 1);
    else
        PICCHECK(uint16_t, uint32_t, colors_16_P, color_16_UV, 2);
}

static void pic_rsc_destroy(picture_t *pic)
{
    for (unsigned i = 0; i < 3; i++)
        free(pic->p[i].p_pixels);
    free(pic);
}

static picture_t *pic_new_unaligned(const video_format_t *fmt)
{
    /* Allocate a no-aligned picture in order to ease buffer overflow detection
     * from the source picture */
    const vlc_chroma_description_t *dsc = vlc_fourcc_GetChromaDescription(fmt->i_chroma);
    assert(dsc);
    picture_resource_t rsc = { .pf_destroy = pic_rsc_destroy };
    for (unsigned i = 0; i < dsc->plane_count; i++)
    {
        rsc.p[i].i_lines = ((fmt->i_visible_height + 1) & ~ 1) * dsc->p[i].h.num / dsc->p[i].h.den;
        rsc.p[i].i_pitch = ((fmt->i_visible_width + 1) & ~ 1) * dsc->pixel_size * dsc->p[i].w.num / dsc->p[i].w.den;
        rsc.p[i].p_pixels = malloc(rsc.p[i].i_lines * rsc.p[i].i_pitch);
        assert(rsc.p[i].p_pixels);
    }
    return picture_NewFromResource(fmt, &rsc);
}

int main(void)
{
    alarm(10);

    unsigned cpu = vlc_CPU();
#ifndef COPY_TEST_NOOPTIM
    if (!vlc_CPU_SSE2())
    {
        fprintf(stderr, "WARNING: could not test SSE\n");
        return 0;
    }
#endif

    for (size_t i = 0; i < NB_CONVS; ++i)
    {
        const struct test_conv *conv = &convs[i];

        for (size_t j = 0; j < NB_SIZES; ++j)
        {
            const struct test_size *size = &sizes[j];

            const vlc_chroma_description_t *src_dsc =
                vlc_fourcc_GetChromaDescription(conv->src_chroma);
            assert(src_dsc);

            video_format_t fmt;
            video_format_Init(&fmt, 0);
            video_format_Setup(&fmt, conv->src_chroma,
                               size->i_width, size->i_height,
                               size->i_visible_width, size->i_visible_height,
                               1, 1);
            picture_t *src = pic_new_unaligned(&fmt);
            assert(src);
            piccheck(src, src_dsc, true);

            copy_cache_t cache;
            int ret = CopyInitCache(&cache, src->format.i_width
                                    * src_dsc->pixel_size);
            assert(ret == VLC_SUCCESS);

            for (size_t f = 0; conv->dsts[f].chroma != 0; ++f)
            {
                const struct test_dst *test_dst= &conv->dsts[f];

                const vlc_chroma_description_t *dst_dsc =
                    vlc_fourcc_GetChromaDescription(test_dst->chroma);
                assert(dst_dsc);
                fmt.i_chroma = test_dst->chroma;
                picture_t *dst = picture_NewFromFormat(&fmt);
                assert(dst);

                const uint8_t * src_planes[3] = { src->p[Y_PLANE].p_pixels,
                                                  src->p[U_PLANE].p_pixels,
                                                  src->p[V_PLANE].p_pixels };
                const size_t    src_pitches[3] = { src->p[Y_PLANE].i_pitch,
                                                   src->p[U_PLANE].i_pitch,
                                                   src->p[V_PLANE].i_pitch };

                fprintf(stderr, "testing: %u x %u (vis: %u x %u) %4.4s -> %4.4s\n",
                        size->i_width, size->i_height,
                        size->i_visible_width, size->i_visible_height,
                        (const char *) &src->format.i_chroma,
                        (const char *) &dst->format.i_chroma);
                test_dst->conv(dst, src_planes, src_pitches,
                                src->format.i_visible_height, &cache);
                piccheck(dst, dst_dsc, false);
                picture_Release(dst);
            }
            picture_Release(src);
            CopyCleanCache(&cache);
        }
    }
    return 0;
}

#endif
