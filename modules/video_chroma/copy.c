/*****************************************************************************
 * copy.c: Fast YV12/NV12 copy
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
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
static void CopyPlane(uint8_t *dst, size_t dst_pitch,
                      const uint8_t *src, size_t src_pitch,
                      unsigned height, int bitshift);

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

#define COPY16_SHIFTR(x) \
    "psrlw "x", %%xmm1\n"
#define COPY16_SHIFTL(x) \
    "psllw "x", %%xmm1\n"

#define COPY16_S(dstp, srcp, load, store, shiftstr) \
    asm volatile (                      \
        load "  0(%[src]), %%xmm1\n"    \
        shiftstr                        \
        store " %%xmm1,    0(%[dst])\n" \
        : : [dst]"r"(dstp), [src]"r"(srcp) : "memory", "xmm1")

#define COPY16(dstp, srcp, load, store) COPY16_S(dstp, srcp, load, store, "")

#define COPY64_SHIFTR(x) \
    "psrlw "x", %%xmm1\n" \
    "psrlw "x", %%xmm2\n" \
    "psrlw "x", %%xmm3\n" \
    "psrlw "x", %%xmm4\n"
#define COPY64_SHIFTL(x) \
    "psllw "x", %%xmm1\n" \
    "psllw "x", %%xmm2\n" \
    "psllw "x", %%xmm3\n" \
    "psllw "x", %%xmm4\n"

#define COPY64_S(dstp, srcp, load, store, shiftstr) \
    asm volatile (                      \
        load "  0(%[src]), %%xmm1\n"    \
        load " 16(%[src]), %%xmm2\n"    \
        load " 32(%[src]), %%xmm3\n"    \
        load " 48(%[src]), %%xmm4\n"    \
        shiftstr                        \
        store " %%xmm1,    0(%[dst])\n" \
        store " %%xmm2,   16(%[dst])\n" \
        store " %%xmm3,   32(%[dst])\n" \
        store " %%xmm4,   48(%[dst])\n" \
        : : [dst]"r"(dstp), [src]"r"(srcp) : "memory", "xmm1", "xmm2", "xmm3", "xmm4")

#define COPY64(dstp, srcp, load, store) \
    COPY64_S(dstp, srcp, load, store, "")

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
                         unsigned width, unsigned height, int bitshift)
{
    assert(((intptr_t)dst & 0x0f) == 0 && (dst_pitch & 0x0f) == 0);

    asm volatile ("mfence");

#define SSE_USWC_COPY(shiftstr16, shiftstr64) \
    for (unsigned y = 0; y < height; y++) { \
        const unsigned unaligned = (-(uintptr_t)src) & 0x0f; \
        unsigned x = unaligned; \
        if (vlc_CPU_SSE4_1()) { \
            if (!unaligned) { \
                for (; x+63 < width; x += 64) \
                    COPY64_S(&dst[x], &src[x], "movntdqa", "movdqa", shiftstr64); \
            } else { \
                COPY16_S(dst, src, "movdqu", "movdqa", shiftstr16); \
                for (; x+63 < width; x += 64) \
                    COPY64_S(&dst[x], &src[x], "movntdqa", "movdqu", shiftstr64); \
            } \
        } else { \
            if (!unaligned) { \
                for (; x+63 < width; x += 64) \
                    COPY64_S(&dst[x], &src[x], "movdqa", "movdqa", shiftstr64); \
            } else { \
                COPY16_S(dst, src, "movdqu", "movdqa", shiftstr16); \
                for (; x+63 < width; x += 64) \
                    COPY64_S(&dst[x], &src[x], "movdqa", "movdqu", shiftstr64); \
            } \
        } \
        /* The following should not happen since buffers are generally well aligned */ \
        if (x < width) \
            CopyPlane(&dst[x], dst_pitch - x, &src[x], src_pitch - x, 1, bitshift); \
        src += src_pitch; \
        dst += dst_pitch; \
    }

    switch (bitshift)
    {
        case 0:
            SSE_USWC_COPY("", "")
            break;
        case -6:
            SSE_USWC_COPY(COPY16_SHIFTL("$6"), COPY64_SHIFTL("$6"))
            break;
        case 6:
            SSE_USWC_COPY(COPY16_SHIFTR("$6"), COPY64_SHIFTR("$6"))
            break;
        case 2:
            SSE_USWC_COPY(COPY16_SHIFTR("$2"), COPY64_SHIFTR("$2"))
            break;
        case -2:
            SSE_USWC_COPY(COPY16_SHIFTL("$2"), COPY64_SHIFTL("$2"))
            break;
        case 4:
            SSE_USWC_COPY(COPY16_SHIFTR("$4"), COPY64_SHIFTR("$4"))
            break;
        case -4:
            SSE_USWC_COPY(COPY16_SHIFTL("$2"), COPY64_SHIFTL("$2"))
            break;
        default:
            vlc_assert_unreachable();
    }
#undef SSE_USWC_COPY

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
                 unsigned int width, unsigned int height, uint8_t pixel_size)
{
    assert(!((intptr_t)srcu & 0xf) && !(srcu_pitch & 0x0f) &&
           !((intptr_t)srcv & 0xf) && !(srcv_pitch & 0x0f));

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
                        unsigned width, unsigned height, uint8_t pixel_size)
{
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
                          unsigned height, int bitshift)
{
    const size_t copy_pitch = __MIN(src_pitch, dst_pitch);
    assert(copy_pitch > 0);
    const unsigned w16 = (copy_pitch+15) & ~15;
    const unsigned hstep = cache_size / w16;
    const unsigned cache_width = __MIN(src_pitch, cache_size);
    assert(hstep > 0);

    /* If SSE4.1: CopyFromUswc is faster than memcpy */
    if (!vlc_CPU_SSE4_1() && bitshift == 0 && src_pitch == dst_pitch)
        memcpy(dst, src, copy_pitch * height);
    else
    for (unsigned y = 0; y < height; y += hstep) {
        const unsigned hblock =  __MIN(hstep, height - y);

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w16, src, src_pitch, cache_width, hblock, bitshift);

        /* Copy from our cache to the destination */
        Copy2d(dst, dst_pitch, cache, w16, copy_pitch, hblock);

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
                     unsigned int height, uint8_t pixel_size, int bitshift)
{
    assert(srcu_pitch == srcv_pitch);
    size_t copy_pitch = __MIN(dst_pitch / 2, srcu_pitch);
    unsigned int const  w16 = (srcu_pitch+15) & ~15;
    unsigned int const  hstep = (cache_size) / (2*w16);
    const unsigned cacheu_width = __MIN(srcu_pitch, cache_size);
    const unsigned cachev_width = __MIN(srcv_pitch, cache_size);
    assert(hstep > 0);

    for (unsigned int y = 0; y < height; y += hstep)
    {
        unsigned int const      hblock = __MIN(hstep, height - y);

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w16, srcu, srcu_pitch, cacheu_width, hblock, bitshift);
        CopyFromUswc(cache+w16*hblock, w16, srcv, srcv_pitch,
                     cachev_width, hblock, bitshift);

        /* Copy from our cache to the destination */
        SSE_InterleaveUV(dst, dst_pitch, cache, w16,
                         cache + w16 * hblock, w16,
                         copy_pitch, hblock, pixel_size);

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
                            unsigned height, uint8_t pixel_size, int bitshift)
{
    size_t copy_pitch = __MIN(__MIN(src_pitch / 2, dstu_pitch), dstv_pitch);
    const unsigned w16 = (src_pitch+15) & ~15;
    const unsigned hstep = cache_size / w16;
    const unsigned cache_width = __MIN(src_pitch, cache_size);
    assert(hstep > 0);

    for (unsigned y = 0; y < height; y += hstep) {
        const unsigned hblock =  __MIN(hstep, height - y);

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w16, src, src_pitch, cache_width, hblock, bitshift);

        /* Copy from our cache to the destination */
        SSE_SplitUV(dstu, dstu_pitch, dstv, dstv_pitch,
                    cache, w16, copy_pitch, hblock, pixel_size);

        /* */
        src  += src_pitch  * hblock;
        dstu += dstu_pitch * hblock;
        dstv += dstv_pitch * hblock;
    }
}

static void SSE_Copy420_P_to_P(picture_t *dst, const uint8_t *src[static 3],
                               const size_t src_pitch[static 3], unsigned height,
                               const copy_cache_t *cache)
{
    for (unsigned n = 0; n < 3; n++) {
        const unsigned d = n > 0 ? 2 : 1;
        SSE_CopyPlane(dst->p[n].p_pixels, dst->p[n].i_pitch,
                      src[n], src_pitch[n],
                      cache->buffer, cache->size,
                      (height+d-1)/d, 0);
    }
    asm volatile ("emms");
}


static void SSE_Copy420_SP_to_SP(picture_t *dst, const uint8_t *src[static 2],
                                 const size_t src_pitch[static 2], unsigned height,
                                 const copy_cache_t *cache)
{
    SSE_CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch, src[0], src_pitch[0],
                  cache->buffer, cache->size, height, 0);
    SSE_CopyPlane(dst->p[1].p_pixels, dst->p[1].i_pitch, src[1], src_pitch[1],
                  cache->buffer, cache->size, (height+1) / 2, 0);
    asm volatile ("emms");
}

static void
SSE_Copy420_SP_to_P(picture_t *dest, const uint8_t *src[static 2],
                    const size_t src_pitch[static 2], unsigned int height,
                    uint8_t pixel_size, int bitshift, const copy_cache_t *cache)
{
    SSE_CopyPlane(dest->p[0].p_pixels, dest->p[0].i_pitch,
                  src[0], src_pitch[0], cache->buffer, cache->size, height, bitshift);

    SSE_SplitPlanes(dest->p[1].p_pixels, dest->p[1].i_pitch,
                    dest->p[2].p_pixels, dest->p[2].i_pitch,
                    src[1], src_pitch[1], cache->buffer, cache->size,
                    (height+1) / 2, pixel_size, bitshift);
    asm volatile ("emms");
}

static void SSE_Copy420_P_to_SP(picture_t *dst, const uint8_t *src[static 3],
                                const size_t src_pitch[static 3],
                                unsigned height, uint8_t pixel_size,
                                int bitshift, const copy_cache_t *cache)
{
    SSE_CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch, src[0], src_pitch[0],
                  cache->buffer, cache->size, height, bitshift);
    SSE_InterleavePlanes(dst->p[1].p_pixels, dst->p[1].i_pitch,
                         src[U_PLANE], src_pitch[U_PLANE],
                         src[V_PLANE], src_pitch[V_PLANE],
                         cache->buffer, cache->size, (height+1) / 2, pixel_size, bitshift);
    asm volatile ("emms");
}
#undef COPY64
#endif /* CAN_COMPILE_SSE2 */

static void CopyPlane(uint8_t *dst, size_t dst_pitch,
                      const uint8_t *src, size_t src_pitch,
                      unsigned height, int bitshift)
{
    const size_t copy_pitch = __MIN(src_pitch, dst_pitch);
    if (bitshift != 0)
    {
        for (unsigned y = 0; y < height; y++)
        {
            uint16_t *dst16 = (uint16_t *) dst;
            const uint16_t *src16 = (const uint16_t *) src;

            if (bitshift > 0)
                for (unsigned x = 0; x < (copy_pitch / 2); x++)
                    *dst16++ = (*src16++) >> (bitshift & 0xf);
            else
                for (unsigned x = 0; x < (copy_pitch / 2); x++)
                    *dst16++ = (*src16++) << ((-bitshift) & 0xf);
            src += src_pitch;
            dst += dst_pitch;
        }
    }
    else if (src_pitch == dst_pitch)
        memcpy(dst, src, copy_pitch * height);
    else
    for (unsigned y = 0; y < height; y++) {
        memcpy(dst, src, copy_pitch);
        src += src_pitch;
        dst += dst_pitch;
    }
}

void CopyPacked(picture_t *dst, const uint8_t *src, const size_t src_pitch,
                unsigned height, const copy_cache_t *cache)
{
    assert(dst);
    assert(src); assert(src_pitch);
    assert(height);

#ifdef CAN_COMPILE_SSE2
    if (vlc_CPU_SSE4_1())
        return SSE_CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch, src, src_pitch,
                             cache->buffer, cache->size, height, 0);
#else
    (void) cache;
#endif
        CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch, src, src_pitch,
                  height, 0);
}

void Copy420_SP_to_SP(picture_t *dst, const uint8_t *src[static 2],
                      const size_t src_pitch[static 2], unsigned height,
                      const copy_cache_t *cache)
{
    ASSERT_2PLANES;
#ifdef CAN_COMPILE_SSE2
    if (vlc_CPU_SSE2())
        return SSE_Copy420_SP_to_SP(dst, src, src_pitch, height, cache);
#else
    (void) cache;
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height, 0);
    CopyPlane(dst->p[1].p_pixels, dst->p[1].i_pitch,
              src[1], src_pitch[1], (height+1)/2, 0);
}

#define SPLIT_PLANES(type, pitch_den) do { \
    size_t copy_pitch = __MIN(__MIN(src_pitch / pitch_den, dstu_pitch), dstv_pitch); \
    for (unsigned y = 0; y < height; y++) { \
        for (unsigned x = 0; x < copy_pitch; x++) { \
            ((type *) dstu)[x] = ((const type *) src)[2*x+0]; \
            ((type *) dstv)[x] = ((const type *) src)[2*x+1]; \
        } \
        src  += src_pitch; \
        dstu += dstu_pitch; \
        dstv += dstv_pitch; \
    } \
} while(0)

#define SPLIT_PLANES_SHIFTR(type, pitch_den, bitshift) do { \
    size_t copy_pitch = __MIN(__MIN(src_pitch / pitch_den, dstu_pitch), dstv_pitch); \
    for (unsigned y = 0; y < height; y++) { \
        for (unsigned x = 0; x < copy_pitch; x++) { \
            ((type *) dstu)[x] = (((const type *) src)[2*x+0]) >> (bitshift); \
            ((type *) dstv)[x] = (((const type *) src)[2*x+1]) >> (bitshift); \
        } \
        src  += src_pitch; \
        dstu += dstu_pitch; \
        dstv += dstv_pitch; \
    } \
} while(0)

#define SPLIT_PLANES_SHIFTL(type, pitch_den, bitshift) do { \
    size_t copy_pitch = __MIN(__MIN(src_pitch / pitch_den, dstu_pitch), dstv_pitch); \
    for (unsigned y = 0; y < height; y++) { \
        for (unsigned x = 0; x < copy_pitch; x++) { \
            ((type *) dstu)[x] = (((const type *) src)[2*x+0]) << (bitshift); \
            ((type *) dstv)[x] = (((const type *) src)[2*x+1]) << (bitshift); \
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
                          const uint8_t *src, size_t src_pitch, unsigned height,
                          int bitshift)
{
    if (bitshift == 0)
        SPLIT_PLANES(uint16_t, 4);
    else if (bitshift > 0)
        SPLIT_PLANES_SHIFTR(uint16_t, 4, bitshift & 0xf);
    else
        SPLIT_PLANES_SHIFTL(uint16_t, 4, (-bitshift) & 0xf);
}

void Copy420_SP_to_P(picture_t *dst, const uint8_t *src[static 2],
                     const size_t src_pitch[static 2], unsigned height,
                     const copy_cache_t *cache)
{
    ASSERT_2PLANES;
#ifdef CAN_COMPILE_SSE2
    if (vlc_CPU_SSE2())
        return SSE_Copy420_SP_to_P(dst, src, src_pitch, height, 1, 0, cache);
#else
    VLC_UNUSED(cache);
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height, 0);
    SplitPlanes(dst->p[1].p_pixels, dst->p[1].i_pitch,
                dst->p[2].p_pixels, dst->p[2].i_pitch,
                src[1], src_pitch[1], (height+1)/2);
}

void Copy420_16_SP_to_P(picture_t *dst, const uint8_t *src[static 2],
                        const size_t src_pitch[static 2], unsigned height,
                        int bitshift, const copy_cache_t *cache)
{
    ASSERT_2PLANES;
    assert(bitshift >= -6 && bitshift <= 6 && (bitshift % 2 == 0));

#ifdef CAN_COMPILE_SSE3
    if (vlc_CPU_SSSE3())
        return SSE_Copy420_SP_to_P(dst, src, src_pitch, height, 2, bitshift, cache);
#else
    VLC_UNUSED(cache);
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height, bitshift);
    SplitPlanes16(dst->p[1].p_pixels, dst->p[1].i_pitch,
                  dst->p[2].p_pixels, dst->p[2].i_pitch,
                  src[1], src_pitch[1], (height+1)/2, bitshift);
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

#define INTERLEAVE_UV_SHIFTR(bitshitf) do { \
    for ( unsigned int line = 0; line < copy_lines; line++ ) { \
        for ( unsigned int col = 0; col < copy_pitch; col++ ) { \
            *dstUV++ = (*srcU++) >> (bitshitf); \
            *dstUV++ = (*srcV++) >> (bitshitf); \
        } \
        dstUV += i_extra_pitch_uv; \
        srcU  += i_extra_pitch_u; \
        srcV  += i_extra_pitch_v; \
    } \
}while(0)

#define INTERLEAVE_UV_SHIFTL(bitshitf) do { \
    for ( unsigned int line = 0; line < copy_lines; line++ ) { \
        for ( unsigned int col = 0; col < copy_pitch; col++ ) { \
            *dstUV++ = (*srcU++) << (bitshitf); \
            *dstUV++ = (*srcV++) << (bitshitf); \
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
    if (vlc_CPU_SSE2())
        return SSE_Copy420_P_to_SP(dst, src, src_pitch, height, 1, 0, cache);
#else
    (void) cache;
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height, 0);

    const unsigned copy_lines = (height+1) / 2;
    unsigned copy_pitch = src_pitch[1];
    if (copy_pitch > (size_t)dst->p[1].i_pitch / 2)
        copy_pitch = dst->p[1].i_pitch / 2;

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
                        int bitshift, const copy_cache_t *cache)
{
    ASSERT_3PLANES;
    assert(bitshift >= -6 && bitshift <= 6 && (bitshift % 2 == 0));
#ifdef CAN_COMPILE_SSE2
    if (vlc_CPU_SSSE3())
        return SSE_Copy420_P_to_SP(dst, src, src_pitch, height, 2, bitshift, cache);
#else
    (void) cache;
#endif

    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
              src[0], src_pitch[0], height, bitshift);

    const unsigned copy_lines = (height+1) / 2;
    const unsigned copy_pitch = src_pitch[1] / 2;

    const int i_extra_pitch_uv = dst->p[1].i_pitch / 2 - 2 * copy_pitch;
    const int i_extra_pitch_u  = src_pitch[U_PLANE] / 2 - copy_pitch;
    const int i_extra_pitch_v  = src_pitch[V_PLANE] / 2 - copy_pitch;

    uint16_t *dstUV = (void*) dst->p[1].p_pixels;
    const uint16_t *srcU  = (const uint16_t *) src[U_PLANE];
    const uint16_t *srcV  = (const uint16_t *) src[V_PLANE];

    if (bitshift == 0)
        INTERLEAVE_UV();
    else if (bitshift > 0)
        INTERLEAVE_UV_SHIFTR(bitshift & 0xf);
    else
        INTERLEAVE_UV_SHIFTL((-bitshift) & 0xf);
}

void Copy420_P_to_P(picture_t *dst, const uint8_t *src[static 3],
                    const size_t src_pitch[static 3], unsigned height,
                    const copy_cache_t *cache)
{
    ASSERT_3PLANES;
#ifdef CAN_COMPILE_SSE2
    if (vlc_CPU_SSE2())
        return SSE_Copy420_P_to_P(dst, src, src_pitch, height, cache);
#else
    (void) cache;
#endif

     CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
               src[0], src_pitch[0], height, 0);
     CopyPlane(dst->p[1].p_pixels, dst->p[1].i_pitch,
               src[1], src_pitch[1], (height+1) / 2, 0);
     CopyPlane(dst->p[2].p_pixels, dst->p[2].i_pitch,
               src[2], src_pitch[2], (height+1) / 2, 0);
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
            p->i_lines  = picture->format.i_height / 2;
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
        if (vlc_fourcc_AreUVPlanesSwapped(picture->format.i_chroma, VLC_CODEC_YV12))
            picture_SwapUV( picture );
    }
    return VLC_SUCCESS;
}

#ifdef COPY_TEST

#include <vlc_picture.h>

struct test_dst
{
    vlc_fourcc_t chroma;
    int bitshift;
    union
    {
        void (*conv)(picture_t *, const uint8_t *[], const size_t [], unsigned,
                     const copy_cache_t *);
        void (*conv16)(picture_t *, const uint8_t *[], const size_t [], unsigned, int,
                     const copy_cache_t *);
    };
};

struct test_conv
{
    vlc_fourcc_t src_chroma;
    struct test_dst dsts[3];
};

static const struct test_conv convs[] = {
    { .src_chroma = VLC_CODEC_NV12,
      .dsts = { { VLC_CODEC_I420, 0, .conv = Copy420_SP_to_P },
                { VLC_CODEC_NV12, 0, .conv = Copy420_SP_to_SP } },
    },
    { .src_chroma = VLC_CODEC_I420,
      .dsts = { { VLC_CODEC_I420, 0, .conv = Copy420_P_to_P },
                { VLC_CODEC_NV12, 0, .conv = Copy420_P_to_SP } },
    },
    { .src_chroma = VLC_CODEC_P010,
      .dsts = { { VLC_CODEC_I420_10L, 6, .conv16 = Copy420_16_SP_to_P } },
    },
    { .src_chroma = VLC_CODEC_I420_10L,
      .dsts = { { VLC_CODEC_P010, -6, .conv16 = Copy420_16_P_to_SP } },
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
#define ASSERT_COLOR(good) do { \
    fprintf(stderr, "error: pixel doesn't match @ plane: %d: %d x %d: 0x%X vs 0x%X\n", i, x, y, *(--p), good); \
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
                        ASSERT_COLOR(color_UV); \
            } \
            else \
            { \
                type_u *p = (type_u *) &plane->p_pixels[y * plane->i_pitch]; \
                for (int x = 0; x < plane->i_visible_pitch / pitch_den; ++x) \
                    if (init) \
                        *(p++) = colors_P[i]; \
                    else if (*(p++) != colors_P[i]) \
                        ASSERT_COLOR(colors_P[i]); \
            } \
        } \
    } \
} while (0)

    assert(pic->i_planes == 2 || pic->i_planes == 3);
    assert(dsc->pixel_size == 1 || dsc->pixel_size == 2);

    if (dsc->pixel_size == 1)
    {
        const uint8_t colors_8_P[3] = { 0x42, 0xF1, 0x36 };
        const uint16_t color_8_UV = ntoh16(0xF136);
        PICCHECK(uint8_t, uint16_t, colors_8_P, color_8_UV, 1);
    }
    else
    {
        const unsigned mask = (1 << dsc->pixel_bits) - 1;
        uint16_t colors_16_P[3] = { 0x1042 &mask, 0xF114 &mask, 0x3645 &mask};

        switch (pic->format.i_chroma)
        {
            case VLC_CODEC_P010:
                for (size_t i = 0; i < 3; ++i)
                    colors_16_P[i] <<= 6;
                break;
            case VLC_CODEC_I420_10L:
                break;
            default:
                vlc_assert_unreachable();
        }

        uint32_t color_16_UV = GetDWLE( &colors_16_P[1] );

        PICCHECK(uint16_t, uint32_t, colors_16_P, color_16_UV, 2);
    }
}

static void pic_rsc_destroy(picture_t *pic)
{
    for (unsigned i = 0; i < 3; i++)
        free(pic->p[i].p_pixels);
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
        rsc.p[i].i_lines = ((fmt->i_visible_height + (dsc->p[i].h.den - 1)) / dsc->p[i].h.den) * dsc->p[i].h.num;
        rsc.p[i].i_pitch = ((fmt->i_visible_width + (dsc->p[i].w.den - 1)) / dsc->p[i].w.den) * dsc->p[i].w.num * dsc->pixel_size;
        rsc.p[i].p_pixels = malloc(rsc.p[i].i_lines * rsc.p[i].i_pitch);
        assert(rsc.p[i].p_pixels);
    }
    return picture_NewFromResource(fmt, &rsc);
}

int main(void)
{
    alarm(10);

#ifndef COPY_TEST_NOOPTIM
    if (!vlc_CPU_SSE2())
    {
        fprintf(stderr, "WARNING: could not test SSE\n");
        return 77;
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
                if (test_dst->bitshift == 0)
                    test_dst->conv(dst, src_planes, src_pitches,
                                   src->format.i_visible_height, &cache);
                else
                    test_dst->conv16(dst, src_planes, src_pitches,
                                   src->format.i_visible_height, test_dst->bitshift,
                                   &cache);
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
