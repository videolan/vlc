/*****************************************************************************
 * startcode_helper.h: Startcodes helpers
 *****************************************************************************
 * Copyright (C) 2016, 2019 VideoLAN Authors
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
#ifndef VLC_STARTCODE_HELPER_H_
#define VLC_STARTCODE_HELPER_H_

#include <vlc_cpu.h>

#if !defined(CAN_COMPILE_AVX2) && defined(HAVE_AVX2_INTRINSICS)
   #include <immintrin.h>
#endif
#if !defined(CAN_COMPILE_SSE2) && defined(HAVE_SSE2_INTRINSICS)
   #include <emmintrin.h>
#endif

/* Looks up efficiently for an AnnexB startcode 0x00 0x00 0x01
 * by using a 4 times faster trick than single byte lookup. */

#define TRY_MATCH(p,a) {\
     if (p[a+1] == 0) {\
            if (p[a+0] == 0 && p[a+2] == 1)\
                return a+p;\
            if (p[a+2] == 0 && p[a+3] == 1)\
                return a+p+1;\
        }\
        if (p[a+3] == 0) {\
            if (p[a+2] == 0 && p[a+4] == 1)\
                return a+p+2;\
            if (p[a+4] == 0 && p[a+5] == 1)\
                return a+p+3;\
        }\
    }

#if defined(CAN_COMPILE_AVX2) || defined(HAVE_AVX2_INTRINSICS)

__attribute__ ((__target__ ("avx2")))
static inline const uint8_t * startcode_FindAnnexB_AVX2( const uint8_t *p, const uint8_t *end )
{
    end -= 3;

    /* First align to 32 */
    const uint8_t *alignedend = p + 32 - ((intptr_t)p & 31);
    for (; p < alignedend && p <= end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    if( p == end )
        return NULL;

    alignedend = end - ((intptr_t) end & 31);
    if( alignedend > p )
    {
#ifdef CAN_COMPILE_AVX2
        asm volatile(
            "vpxor   %%ymm1, %%ymm1\n"
            ::: "ymm1"
        );
#else
        __m256i zeros = _mm256_set1_epi8( 0x00 );
#endif
        for( ; p < alignedend; p += 32)
        {
            uint32_t match;
#ifdef CAN_COMPILE_AVX2
            asm volatile(
                "vmovdqa   0(%[v]),   %%ymm0\n"
                "vpcmpeqb   %%ymm1,   %%ymm0\n"
                "vpmovmskb  %%ymm0,   %[match]\n"
                : [match]"=r"(match)
                : [v]"r"(p)
                : "ymm0"
            );
#else
            __m256i v = _mm256_load_si256((__m256i*)p);
            __m256i res = _mm256_cmpeq_epi8( zeros, v );
            match = _mm256_movemask_epi8( res ); /* mask will be in reversed match order */
#endif
            if( match & 0x0000000F )
                TRY_MATCH(p, 0);
            if( match & 0x000000F0 )
                TRY_MATCH(p, 4);
            if( match & 0x00000F00 )
                TRY_MATCH(p, 8);
            if( match & 0x0000F000 )
                TRY_MATCH(p, 12);
            if( match & 0x000F0000 )
                TRY_MATCH(p, 16);
            if( match & 0x00F00000 )
                TRY_MATCH(p, 20);
            if( match & 0x0F000000 )
                TRY_MATCH(p, 24);
            if( match & 0xF0000000 )
                TRY_MATCH(p, 28);
        }
    }

    for (; p <= end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return NULL;
}

#endif

#if defined(CAN_COMPILE_SSE2) || defined(HAVE_SSE2_INTRINSICS)

__attribute__ ((__target__ ("sse2")))
static inline const uint8_t * startcode_FindAnnexB_SSE2( const uint8_t *p, const uint8_t *end )
{
    end -= 3;

    /* First align to 16 */
    /* Skipping this step and doing unaligned loads isn't faster */
    const uint8_t *alignedend = p + 16 - ((intptr_t)p & 15);
    for (; p < alignedend && p <= end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    if( p == end )
        return NULL;

    alignedend = end - ((intptr_t) end & 15);
    if( alignedend > p )
    {
#ifdef CAN_COMPILE_SSE2
        asm volatile(
            "pxor   %%xmm1, %%xmm1\n"
            ::: "xmm1"
        );
#else
        __m128i zeros = _mm_set1_epi8( 0x00 );
#endif
        for( ; p < alignedend; p += 16)
        {
            uint32_t match;
#ifdef CAN_COMPILE_SSE2
            asm volatile(
                "movdqa   0(%[v]),   %%xmm0\n"
                "pcmpeqb   %%xmm1,   %%xmm0\n"
                "pmovmskb  %%xmm0,   %[match]\n"
                : [match]"=r"(match)
                : [v]"r"(p)
                : "xmm0"
            );
#else
            __m128i v = _mm_load_si128((__m128i*)p);
            __m128i res = _mm_cmpeq_epi8( zeros, v );
            match = _mm_movemask_epi8( res ); /* mask will be in reversed match order */
#endif
            if( match & 0x000F )
                TRY_MATCH(p, 0);
            if( match & 0x00F0 )
                TRY_MATCH(p, 4);
            if( match & 0x0F00 )
                TRY_MATCH(p, 8);
            if( match & 0xF000 )
                TRY_MATCH(p, 12);
        }
    }

    for (; p <= end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return NULL;
}

#endif

/* That code is adapted from libav's ff_avc_find_startcode_internal
 * and i believe the trick originated from
 * https://graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
 */
static inline const uint8_t * startcode_FindAnnexB_Bits( const uint8_t *p, const uint8_t *end )
{
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p <= end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(const uint32_t*)p;
        if ((x - 0x01010101) & (~x) & 0x80808080)
        {
            /* matching DW isn't faster */
            TRY_MATCH(p, 0);
        }
    }

    for (end += 3; p <= end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return NULL;
}
#undef TRY_MATCH

#if defined(CAN_COMPILE_AVX2) || defined(HAVE_AVX2_INTRINSICS) || defined(CAN_COMPILE_SSE2) || defined(HAVE_SSE2_INTRINSICS)
static inline const uint8_t * startcode_FindAnnexB( const uint8_t *p, const uint8_t *end )
{
#if defined(CAN_COMPILE_AVX2) || defined(HAVE_AVX2_INTRINSICS)
    if (vlc_CPU_AVX2())
        return startcode_FindAnnexB_AVX2(p, end);
#endif
#if defined(CAN_COMPILE_SSE2) || defined(HAVE_SSE2_INTRINSICS)
    if (vlc_CPU_SSE2())
        return startcode_FindAnnexB_SSE2(p, end);
#endif
    return startcode_FindAnnexB_Bits(p, end);
}
#else
    #define startcode_FindAnnexB startcode_FindAnnexB_Bits
#endif

#endif
