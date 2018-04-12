/*****************************************************************************
 * startcode_helper.h: Startcodes helpers
 *****************************************************************************
 * Copyright (C) 2016 VideoLAN Authors
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

#if defined(CAN_COMPILE_SSE2) || defined(HAVE_SSE2_INTRINSICS)

__attribute__ ((__target__ ("sse2")))
static inline const uint8_t * startcode_FindAnnexB_SSE2( const uint8_t *p, const uint8_t *end )
{
    /* First align to 16 */
    /* Skipping this step and doing unaligned loads isn't faster */
    const uint8_t *alignedend = p + 16 - ((intptr_t)p & 15);
    for (end -= 3; p < alignedend && p <= end; p++) {
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

#if defined(CAN_COMPILE_SSE2) || defined(HAVE_SSE2_INTRINSICS)
static inline const uint8_t * startcode_FindAnnexB( const uint8_t *p, const uint8_t *end )
{
    if (vlc_CPU_SSE2())
        return startcode_FindAnnexB_SSE2(p, end);
    else
        return startcode_FindAnnexB_Bits(p, end);
}
#else
    #define startcode_FindAnnexB startcode_FindAnnexB_Bits
#endif

#endif
