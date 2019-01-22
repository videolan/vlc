/*****************************************************************************
 * i422_yuy2.h : YUV to YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2002 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damienf@videolan.org>
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

#ifdef MODULE_NAME_IS_i422_yuy2_mmx

#if defined(CAN_COMPILE_MMX)

/* MMX assembly */
 
#define MMX_CALL(MMX_INSTRUCTIONS)          \
    do {                                    \
    __asm__ __volatile__(                   \
        ".p2align 3 \n\t"                   \
        MMX_INSTRUCTIONS                    \
        :                                   \
        : "r" (p_line), "r" (p_y),          \
          "r" (p_u), "r" (p_v)              \
        : "mm0", "mm1", "mm2" );            \
        p_line += 16; p_y += 8;             \
        p_u += 4; p_v += 4;                 \
    } while(0)

#define MMX_END __asm__ __volatile__ ( "emms" )

#define MMX_YUV422_YUYV "                                                 \n\
movq       (%1), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movd       (%2), %%mm1  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%3), %%mm2  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
punpcklbw %%mm2, %%mm1  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
movq      %%mm0, %%mm2  #                     y7 y6 y5 y4 y3 y2 y1 y0     \n\
punpcklbw %%mm1, %%mm2  #                     v1 y3 u1 y2 v0 y1 u0 y0     \n\
movq      %%mm2, (%0)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm0  #                     v3 y7 u3 y6 v2 y5 u2 y4     \n\
movq      %%mm0, 8(%0)  # Store high YUYV                                 \n\
"

#define MMX_YUV422_YVYU "                                                 \n\
movq       (%1), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movd       (%2), %%mm2  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%3), %%mm1  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
punpcklbw %%mm2, %%mm1  #                     u3 v3 u2 v2 u1 v1 u0 v0     \n\
movq      %%mm0, %%mm2  #                     y7 y6 y5 y4 y3 y2 y1 y0     \n\
punpcklbw %%mm1, %%mm2  #                     u1 y3 v1 y2 u0 y1 v0 y0     \n\
movq      %%mm2, (%0)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm0  #                     u3 y7 v3 y6 u2 y5 v2 y4     \n\
movq      %%mm0, 8(%0)  # Store high YUYV                                 \n\
"

#define MMX_YUV422_UYVY "                                                 \n\
movq       (%1), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movd       (%2), %%mm1  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%3), %%mm2  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
punpcklbw %%mm2, %%mm1  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
movq      %%mm1, %%mm2  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
punpcklbw %%mm0, %%mm2  #                     y3 v1 y2 u1 y1 v0 y0 u0     \n\
movq      %%mm2, (%0)   # Store low UYVY                                  \n\
punpckhbw %%mm0, %%mm1  #                     y7 v3 y6 u3 y5 v2 y4 u2     \n\
movq      %%mm1, 8(%0)  # Store high UYVY                                 \n\
"

#elif defined(HAVE_MMX_INTRINSICS)

/* MMX intrinsics */

#include <mmintrin.h>

#define MMX_CALL(MMX_INSTRUCTIONS)  \
    do {                            \
        __m64 mm0, mm1, mm2;        \
        MMX_INSTRUCTIONS            \
        p_line += 16; p_y += 8;     \
        p_u += 4; p_v += 4;         \
    } while(0)

#define MMX_END _mm_empty()

#define MMX_YUV422_YUYV                     \
    mm0 = (__m64)*(uint64_t*)p_y;           \
    mm1 = _mm_cvtsi32_si64(*(int*)p_u);     \
    mm2 = _mm_cvtsi32_si64(*(int*)p_v);     \
    mm1 = _mm_unpacklo_pi8(mm1, mm2);       \
    mm2 = mm0;                              \
    mm2 = _mm_unpacklo_pi8(mm2, mm1);       \
    *(uint64_t*)p_line = (uint64_t)mm2;     \
    mm0 = _mm_unpackhi_pi8(mm0, mm1);       \
    *(uint64_t*)(p_line+8) = (uint64_t)mm0;
 
#define MMX_YUV422_YVYU                     \
    mm0 = (__m64)*(uint64_t*)p_y;           \
    mm2 = _mm_cvtsi32_si64(*(int*)p_u);     \
    mm1 = _mm_cvtsi32_si64(*(int*)p_v);     \
    mm1 = _mm_unpacklo_pi8(mm1, mm2);       \
    mm2 = mm0;                              \
    mm2 = _mm_unpacklo_pi8(mm2, mm1);       \
    *(uint64_t*)p_line = (uint64_t)mm2;     \
    mm0 = _mm_unpackhi_pi8(mm0, mm1);       \
    *(uint64_t*)(p_line+8) = (uint64_t)mm0;

#define MMX_YUV422_UYVY                     \
    mm0 = (__m64)*(uint64_t*)p_y;           \
    mm1 = _mm_cvtsi32_si64(*(int*)p_u);     \
    mm2 = _mm_cvtsi32_si64(*(int*)p_v);     \
    mm1 = _mm_unpacklo_pi8(mm1, mm2);       \
    mm2 = mm1;                              \
    mm2 = _mm_unpacklo_pi8(mm2, mm0);       \
    *(uint64_t*)p_line = (uint64_t)mm2;     \
    mm1 = _mm_unpackhi_pi8(mm1, mm0);       \
    *(uint64_t*)(p_line+8) = (uint64_t)mm1;

#endif
 
#elif defined( MODULE_NAME_IS_i422_yuy2_sse2 )

#if defined(CAN_COMPILE_SSE2)

/* SSE2 assembly */

#define SSE2_CALL(SSE2_INSTRUCTIONS)        \
    do {                                    \
    __asm__ __volatile__(                   \
        ".p2align 3 \n\t"                   \
        SSE2_INSTRUCTIONS                   \
        :                                   \
        : "r" (p_line), "r" (p_y),          \
          "r" (p_u), "r" (p_v)              \
        : "xmm0", "xmm1", "xmm2" );         \
        p_line += 32; p_y += 16;            \
        p_u += 8; p_v += 8;                 \
    } while(0)

#define SSE2_END  __asm__ __volatile__ ( "sfence" ::: "memory" )

#define SSE2_YUV422_YUYV_ALIGNED "                                      \n\
movdqa      (%1), %%xmm0  # Load 16 Y           yF yE yD ... y2 y1 y0   \n\
movq        (%2), %%xmm1  # Load 8 Cb           00 00 00 ... u2 u1 u0   \n\
movq        (%3), %%xmm2  # Load 8 Cr           00 00 00 ... v2 v1 v0   \n\
punpcklbw %%xmm2, %%xmm1  #                     v7 u7 ... v1 u1 v0 u0   \n\
movdqa    %%xmm0, %%xmm2  #                     yF yE yD ... y2 y1 y0   \n\
punpcklbw %%xmm1, %%xmm2  #                     v3 y7 ... v0 y1 u0 y0   \n\
movntdq   %%xmm2, (%0)    # Store low YUYV                              \n\
punpckhbw %%xmm1, %%xmm0  #                     v7 yF ... v4 y9 u4 y8   \n\
movntdq   %%xmm0, 16(%0)  # Store high YUYV                             \n\
"

#define SSE2_YUV422_YUYV_UNALIGNED "                                    \n\
movdqu      (%1), %%xmm0  # Load 16 Y           yF yE yD ... y2 y1 y0   \n\
movq        (%2), %%xmm1  # Load 8 Cb           00 00 00 ... u2 u1 u0   \n\
movq        (%3), %%xmm2  # Load 8 Cr           00 00 00 ... v2 v1 v0   \n\
prefetchnta (%0)          # Tell CPU not to cache output YUYV data      \n\
punpcklbw %%xmm2, %%xmm1  #                     v7 u7 ... v1 u1 v0 u0   \n\
movdqa    %%xmm0, %%xmm2  #                     yF yE yD ... y2 y1 y0   \n\
punpcklbw %%xmm1, %%xmm2  #                     v3 y7 ... v0 y1 u0 y0   \n\
movdqu    %%xmm2, (%0)    # Store low YUYV                              \n\
punpckhbw %%xmm1, %%xmm0  #                     v7 yF ... v4 y9 u4 y8   \n\
movdqu    %%xmm0, 16(%0)  # Store high YUYV                             \n\
"

#define SSE2_YUV422_YVYU_ALIGNED "                                      \n\
movdqa      (%1), %%xmm0  # Load 16 Y           yF yE yD ... y2 y1 y0   \n\
movq        (%2), %%xmm2  # Load 8 Cb           00 00 00 ... u2 u1 u0   \n\
movq        (%3), %%xmm1  # Load 8 Cr           00 00 00 ... v2 v1 v0   \n\
punpcklbw %%xmm2, %%xmm1  #                     u7 v7 ... u1 v1 u0 v0   \n\
movdqa    %%xmm0, %%xmm2  #                     yF yE yD ... y2 y1 y0   \n\
punpcklbw %%xmm1, %%xmm2  #                     u3 y7 ... u0 y1 v0 y0   \n\
movntdq   %%xmm2, (%0)    # Store low YUYV                              \n\
punpckhbw %%xmm1, %%xmm0  #                     u7 yF ... u4 y9 v4 y8   \n\
movntdq   %%xmm0, 16(%0)  # Store high YUYV                             \n\
"

#define SSE2_YUV422_YVYU_UNALIGNED "                                    \n\
movdqu      (%1), %%xmm0  # Load 16 Y           yF yE yD ... y2 y1 y0   \n\
movq        (%2), %%xmm2  # Load 8 Cb           00 00 00 ... u2 u1 u0   \n\
movq        (%3), %%xmm1  # Load 8 Cr           00 00 00 ... v2 v1 v0   \n\
prefetchnta (%0)          # Tell CPU not to cache output YUYV data      \n\
punpcklbw %%xmm2, %%xmm1  #                     u7 v7 ... u1 v1 u0 v0   \n\
movdqa    %%xmm0, %%xmm2  #                     yF yE yD ... y2 y1 y0   \n\
punpcklbw %%xmm1, %%xmm2  #                     u3 y7 ... u0 y1 v0 y0   \n\
movdqu    %%xmm2, (%0)    # Store low YUYV                              \n\
punpckhbw %%xmm1, %%xmm0  #                     u7 yF ... u4 y9 v4 y8   \n\
movdqu    %%xmm0, 16(%0)  # Store high YUYV                             \n\
"

#define SSE2_YUV422_UYVY_ALIGNED "                                      \n\
movdqa      (%1), %%xmm0  # Load 16 Y           yF yE yD ... y2 y1 y0   \n\
movq        (%2), %%xmm1  # Load 8 Cb           00 00 00 ... u2 u1 u0   \n\
movq        (%3), %%xmm2  # Load 8 Cr           00 00 00 ... v2 v1 v0   \n\
punpcklbw %%xmm2, %%xmm1  #                     v7 u7 ... v1 u1 v0 u0   \n\
movdqa    %%xmm1, %%xmm2  #                     v7 u7 ... v1 u1 v0 u0   \n\
punpcklbw %%xmm0, %%xmm2  #                     y7 v3 ... y1 v0 y0 u0   \n\
movntdq   %%xmm2, (%0)    # Store low UYVY                              \n\
punpckhbw %%xmm0, %%xmm1  #                     yF v7 ... y9 v4 y8 u4   \n\
movntdq   %%xmm1, 16(%0)  # Store high UYVY                             \n\
"

#define SSE2_YUV422_UYVY_UNALIGNED "                                    \n\
movdqu      (%1), %%xmm0  # Load 16 Y           yF yE yD ... y2 y1 y0   \n\
movq        (%2), %%xmm1  # Load 8 Cb           00 00 00 ... u2 u1 u0   \n\
movq        (%3), %%xmm2  # Load 8 Cr           00 00 00 ... v2 v1 v0   \n\
prefetchnta (%0)          # Tell CPU not to cache output YUYV data      \n\
punpcklbw %%xmm2, %%xmm1  #                     v7 u7 ... v1 u1 v0 u0   \n\
movdqa    %%xmm1, %%xmm2  #                     v7 u7 ... v1 u1 v0 u0   \n\
punpcklbw %%xmm0, %%xmm2  #                     y7 v3 ... y1 v0 y0 u0   \n\
movdqu    %%xmm2, (%0)    # Store low UYVY                              \n\
punpckhbw %%xmm0, %%xmm1  #                     yF v7 ... y9 v4 y8 u4   \n\
movdqu    %%xmm1, 16(%0)  # Store high UYVY                             \n\
"

#elif defined(HAVE_SSE2_INTRINSICS)

/* SSE2 intrinsics */

#include <emmintrin.h>

#define SSE2_CALL(SSE2_INSTRUCTIONS)    \
    do {                                \
        __m128i xmm0, xmm1, xmm2;        \
        SSE2_INSTRUCTIONS               \
        p_line += 32; p_y += 16;        \
        p_u += 8; p_v += 8;             \
    } while(0)

#define SSE2_END  _mm_sfence()

#define SSE2_YUV422_YUYV_ALIGNED                \
    xmm0 = _mm_load_si128((__m128i *)p_y);      \
    xmm1 = _mm_loadl_epi64((__m128i *)p_u);     \
    xmm2 = _mm_loadl_epi64((__m128i *)p_v);     \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);       \
    xmm2 = xmm0;                                \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm1);       \
    _mm_stream_si128((__m128i*)(p_line), xmm2); \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm1);       \
    _mm_stream_si128((__m128i*)(p_line+16), xmm0);
 
#define SSE2_YUV422_YUYV_UNALIGNED              \
    xmm0 = _mm_loadu_si128((__m128i *)p_y);     \
    xmm1 = _mm_loadl_epi64((__m128i *)p_u);     \
    xmm2 = _mm_loadl_epi64((__m128i *)p_v);     \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);       \
    xmm2 = xmm0;                                \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm1);       \
    _mm_storeu_si128((__m128i*)(p_line), xmm2); \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm1);       \
    _mm_storeu_si128((__m128i*)(p_line+16), xmm0);
 
#define SSE2_YUV422_YVYU_ALIGNED                \
    xmm0 = _mm_load_si128((__m128i *)p_y);      \
    xmm2 = _mm_loadl_epi64((__m128i *)p_u);     \
    xmm1 = _mm_loadl_epi64((__m128i *)p_v);     \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);       \
    xmm2 = xmm0;                                \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm1);       \
    _mm_stream_si128((__m128i*)(p_line), xmm2); \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm1);       \
    _mm_stream_si128((__m128i*)(p_line+16), xmm0);

#define SSE2_YUV422_YVYU_UNALIGNED              \
    xmm0 = _mm_loadu_si128((__m128i *)p_y);     \
    xmm2 = _mm_loadl_epi64((__m128i *)p_u);     \
    xmm1 = _mm_loadl_epi64((__m128i *)p_v);     \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);       \
    xmm2 = xmm0;                                \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm1);       \
    _mm_storeu_si128((__m128i*)(p_line), xmm2); \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm1);       \
    _mm_storeu_si128((__m128i*)(p_line+16), xmm0);

#define SSE2_YUV422_UYVY_ALIGNED                \
    xmm0 = _mm_load_si128((__m128i *)p_y);      \
    xmm1 = _mm_loadl_epi64((__m128i *)p_u);     \
    xmm2 = _mm_loadl_epi64((__m128i *)p_v);     \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);       \
    xmm2 = xmm1;                                \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm0);       \
    _mm_stream_si128((__m128i*)(p_line), xmm2); \
    xmm1 = _mm_unpackhi_epi8(xmm1, xmm0);       \
    _mm_stream_si128((__m128i*)(p_line+16), xmm1);

#define SSE2_YUV422_UYVY_UNALIGNED              \
    xmm0 = _mm_loadu_si128((__m128i *)p_y);     \
    xmm1 = _mm_loadl_epi64((__m128i *)p_u);     \
    xmm2 = _mm_loadl_epi64((__m128i *)p_v);     \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);       \
    xmm2 = xmm1;                                \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm0);       \
    _mm_storeu_si128((__m128i*)(p_line), xmm2); \
    xmm1 = _mm_unpackhi_epi8(xmm1, xmm0);       \
    _mm_storeu_si128((__m128i*)(p_line+16), xmm1);

#endif

#endif

#define C_YUV422_YUYV( p_line, p_y, p_u, p_v )                              \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_u)++;                                                 \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_v)++;                                                 \

#define C_YUV422_YVYU( p_line, p_y, p_u, p_v )                              \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_v)++;                                                 \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_u)++;                                                 \

#define C_YUV422_UYVY( p_line, p_y, p_u, p_v )                              \
    *(p_line)++ = *(p_u)++;                                                 \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_v)++;                                                 \
    *(p_line)++ = *(p_y)++;                                                 \

#define C_YUV422_Y211( p_line, p_y, p_u, p_v )                              \
    *(p_line)++ = *(p_y); p_y += 2;                                         \
    *(p_line)++ = *(p_u) - 0x80; p_u += 2;                                  \
    *(p_line)++ = *(p_y); p_y += 2;                                         \
    *(p_line)++ = *(p_v) - 0x80; p_v += 2;                                  \


