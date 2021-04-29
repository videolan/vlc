/*****************************************************************************
 * i420_yuy2.h : YUV to YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damien@videolan.org>
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

#if defined( PLUGIN_SSE2 )

#if defined(CAN_COMPILE_SSE2)

/* SSE2 assembly */

#define SSE2_CALL(SSE2_INSTRUCTIONS)    \
    do {                                \
    __asm__ __volatile__(               \
        ".p2align 3 \n\t                \
movq        (%0), %%xmm1  # Load 8 Cb         00 00 00 ... u2 u1 u0   \n\
movq        (%1), %%xmm2  # Load 8 Cr         00 00 00 ... v2 v1 v0   \n\
" \
        :                               \
        : "r" (p_u),  "r" (p_v)         \
        : "xmm1", "xmm2");              \
    __asm__ __volatile__(               \
        ".p2align 3 \n\t"               \
        SSE2_INSTRUCTIONS               \
        :                               \
        : "r" (p_line1), "r" (p_line2), \
          "r" (p_y1),  "r" (p_y2)       \
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"); \
        p_line1 += 32; p_line2 += 32;   \
        p_y1 += 16; p_y2 += 16;         \
        p_u += 8; p_v += 8;             \
    } while(0)

#define SSE2_END  __asm__ __volatile__ ( "sfence" ::: "memory" )

#define SSE2_YUV420_YUYV_ALIGNED "                                     \n\
movdqa      (%2), %%xmm0  # Load 16 Y          yF yE yD ... y2 y1 y0   \n\
movdqa      (%3), %%xmm3  # Load 16 Y          YF YE YD ... Y2 Y1 Y0   \n\
punpcklbw %%xmm2, %%xmm1  #                    00 00 ... v1 u1 v0 u0   \n\
movdqa    %%xmm0, %%xmm2  #                    yF yE yD ... y2 y1 y0   \n\
punpcklbw %%xmm1, %%xmm2  #                    v3 y7 ... v0 y1 u0 y0   \n\
movntdq   %%xmm2, (%0)    # Store low YUYV                             \n\
punpckhbw %%xmm1, %%xmm0  #                    v7 yF ... v4 y9 u4 y8   \n\
movntdq   %%xmm0, 16(%0)  # Store high YUYV                            \n\
movdqa    %%xmm3, %%xmm4  #                    YF YE YD ... Y2 Y1 Y0   \n\
punpcklbw %%xmm1, %%xmm4  #                    v3 Y7 ... v0 Y1 u0 Y0   \n\
movntdq   %%xmm4, (%1)    # Store low YUYV                             \n\
punpckhbw %%xmm1, %%xmm3  #                    v7 YF ... v4 Y9 u4 Y8   \n\
movntdq   %%xmm3, 16(%1)  # Store high YUYV                            \n\
"

#define SSE2_YUV420_YUYV_UNALIGNED "                                   \n\
movdqu      (%2), %%xmm0  # Load 16 Y          yF yE yD ... y2 y1 y0   \n\
movdqu      (%3), %%xmm3  # Load 16 Y          YF YE YD ... Y2 Y1 Y0   \n\
prefetchnta (%0)          # Tell CPU not to cache output YUYV data     \n\
prefetchnta (%1)          # Tell CPU not to cache output YUYV data     \n\
punpcklbw %%xmm2, %%xmm1  #                    00 00 ... v1 u1 v0 u0   \n\
movdqa    %%xmm0, %%xmm2  #                    yF yE yD ... y2 y1 y0   \n\
punpcklbw %%xmm1, %%xmm2  #                    v3 y7 ... v0 y1 u0 y0   \n\
movdqu    %%xmm2, (%0)    # Store low YUYV                             \n\
punpckhbw %%xmm1, %%xmm0  #                    v7 yF ... v4 y9 u4 y8   \n\
movdqu    %%xmm0, 16(%0)  # Store high YUYV                            \n\
movdqa    %%xmm3, %%xmm4  #                    YF YE YD ... Y2 Y1 Y0   \n\
punpcklbw %%xmm1, %%xmm4  #                    v3 Y7 ... v0 Y1 u0 Y0   \n\
movdqu    %%xmm4, (%1)    # Store low YUYV                             \n\
punpckhbw %%xmm1, %%xmm3  #                    v7 YF ... v4 Y9 u4 Y8   \n\
movdqu    %%xmm3, 16(%1)  # Store high YUYV                            \n\
"

#define SSE2_YUV420_YVYU_ALIGNED "                                     \n\
movdqa      (%2), %%xmm0  # Load 16 Y          yF yE yD ... y2 y1 y0   \n\
movdqa      (%3), %%xmm3  # Load 16 Y          YF YE YD ... Y2 Y1 Y0   \n\
punpcklbw %%xmm1, %%xmm2  #                    u7 v7 ... u1 v1 u0 v0   \n\
movdqa    %%xmm0, %%xmm1  #                    yF yE yD ... y2 y1 y0   \n\
punpcklbw %%xmm2, %%xmm1  #                    u3 y7 ... u0 y1 v0 y0   \n\
movntdq   %%xmm1, (%0)    # Store low YUYV                             \n\
punpckhbw %%xmm2, %%xmm0  #                    u7 yF ... u4 y9 v4 y8   \n\
movntdq   %%xmm0, 16(%0)  # Store high YUYV                            \n\
movdqa    %%xmm3, %%xmm4  #                    YF YE YD ... Y2 Y1 Y0   \n\
punpcklbw %%xmm2, %%xmm4  #                    u3 Y7 ... u0 Y1 v0 Y0   \n\
movntdq   %%xmm4, (%1)    # Store low YUYV                             \n\
punpckhbw %%xmm2, %%xmm3  #                    u7 YF ... u4 Y9 v4 Y8   \n\
movntdq   %%xmm3, 16(%1)  # Store high YUYV                            \n\
"

#define SSE2_YUV420_YVYU_UNALIGNED "                                    \n\
movdqu      (%2), %%xmm0  # Load 16 Y           yF yE yD ... y2 y1 y0   \n\
movdqu      (%3), %%xmm3  # Load 16 Y           YF YE YD ... Y2 Y1 Y0   \n\
prefetchnta (%0)          # Tell CPU not to cache output YVYU data      \n\
prefetchnta (%1)          # Tell CPU not to cache output YVYU data      \n\
punpcklbw %%xmm1, %%xmm2  #                     u7 v7 ... u1 v1 u0 v0   \n\
movdqu    %%xmm0, %%xmm1  #                     yF yE yD ... y2 y1 y0   \n\
punpcklbw %%xmm2, %%xmm1  #                     u3 y7 ... u0 y1 v0 y0   \n\
movdqu    %%xmm1, (%0)    # Store low YUYV                              \n\
punpckhbw %%xmm2, %%xmm0  #                     u7 yF ... u4 y9 v4 y8   \n\
movdqu    %%xmm0, 16(%0)  # Store high YUYV                             \n\
movdqu    %%xmm3, %%xmm4  #                     YF YE YD ... Y2 Y1 Y0   \n\
punpcklbw %%xmm2, %%xmm4  #                     u3 Y7 ... u0 Y1 v0 Y0   \n\
movdqu    %%xmm4, (%1)    # Store low YUYV                              \n\
punpckhbw %%xmm2, %%xmm3  #                     u7 YF ... u4 Y9 v4 Y8   \n\
movdqu    %%xmm3, 16(%1)  # Store high YUYV                             \n\
"

#define SSE2_YUV420_UYVY_ALIGNED "                                      \n\
movdqa      (%2), %%xmm0  # Load 16 Y           yF yE yD ... y2 y1 y0   \n\
movdqa      (%3), %%xmm3  # Load 16 Y           YF YE YD ... Y2 Y1 Y0   \n\
punpcklbw %%xmm2, %%xmm1  #                     v7 u7 ... v1 u1 v0 u0   \n\
movdqa    %%xmm1, %%xmm2  #                     v7 u7 ... v1 u1 v0 u0   \n\
punpcklbw %%xmm0, %%xmm2  #                     y7 v3 ... y1 v0 y0 u0   \n\
movntdq   %%xmm2, (%0)    # Store low UYVY                              \n\
movdqa    %%xmm1, %%xmm2  #                     v7 u7 ... v1 u1 v0 u0   \n\
punpckhbw %%xmm0, %%xmm2  #                     yF v7 ... y9 v4 y8 u4   \n\
movntdq   %%xmm2, 16(%0)  # Store high UYVY                             \n\
movdqa    %%xmm1, %%xmm4  #                     v7 u7 ... v1 u1 v0 u0   \n\
punpcklbw %%xmm3, %%xmm4  #                     Y7 v3 ... Y1 v0 Y0 u0   \n\
movntdq   %%xmm4, (%1)    # Store low UYVY                              \n\
punpckhbw %%xmm3, %%xmm1  #                     YF v7 ... Y9 v4 Y8 u4   \n\
movntdq   %%xmm1, 16(%1)  # Store high UYVY                             \n\
"

#define SSE2_YUV420_UYVY_UNALIGNED "                                    \n\
movdqu      (%2), %%xmm0  # Load 16 Y           yF yE yD ... y2 y1 y0   \n\
movdqu      (%3), %%xmm3  # Load 16 Y           YF YE YD ... Y2 Y1 Y0   \n\
prefetchnta (%0)          # Tell CPU not to cache output UYVY data      \n\
prefetchnta (%1)          # Tell CPU not to cache output UYVY data      \n\
punpcklbw %%xmm2, %%xmm1  #                     v7 u7 ... v1 u1 v0 u0   \n\
movdqu    %%xmm1, %%xmm2  #                     v7 u7 ... v1 u1 v0 u0   \n\
punpcklbw %%xmm0, %%xmm2  #                     y7 v3 ... y1 v0 y0 u0   \n\
movdqu    %%xmm2, (%0)    # Store low UYVY                              \n\
movdqu    %%xmm1, %%xmm2  #                     v7 u7 ... v1 u1 v0 u0   \n\
punpckhbw %%xmm0, %%xmm2  #                     yF v7 ... y9 v4 y8 u4   \n\
movdqu    %%xmm2, 16(%0)  # Store high UYVY                             \n\
movdqu    %%xmm1, %%xmm4  #                     v7 u7 ... v1 u1 v0 u0   \n\
punpcklbw %%xmm3, %%xmm4  #                     Y7 v3 ... Y1 v0 Y0 u0   \n\
movdqu    %%xmm4, (%1)    # Store low UYVY                              \n\
punpckhbw %%xmm3, %%xmm1  #                     YF v7 ... Y9 v4 Y8 u4   \n\
movdqu    %%xmm1, 16(%1)  # Store high UYVY                             \n\
"

#elif defined(HAVE_SSE2_INTRINSICS)

/* SSE2 intrinsics */

#include <emmintrin.h>

#define SSE2_CALL(SSE2_INSTRUCTIONS)            \
    do {                                        \
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4;   \
        SSE2_INSTRUCTIONS                       \
        p_line1 += 32; p_line2 += 32;           \
        p_y1 += 16; p_y2 += 16;                 \
        p_u += 8; p_v += 8;                     \
    } while(0)

#define SSE2_END  _mm_sfence()

#define SSE2_YUV420_YUYV_ALIGNED                    \
    xmm1 = _mm_loadl_epi64((__m128i *)p_u);         \
    xmm2 = _mm_loadl_epi64((__m128i *)p_v);         \
    xmm0 = _mm_load_si128((__m128i *)p_y1);         \
    xmm3 = _mm_load_si128((__m128i *)p_y2);         \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);           \
    xmm2 = xmm0;                                    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm1);           \
    _mm_stream_si128((__m128i*)(p_line1), xmm2);    \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm1);           \
    _mm_stream_si128((__m128i*)(p_line1+16), xmm0); \
    xmm4 = xmm3;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);           \
    _mm_stream_si128((__m128i*)(p_line2), xmm4);    \
    xmm3 = _mm_unpackhi_epi8(xmm3, xmm1);           \
    _mm_stream_si128((__m128i*)(p_line1+16), xmm3);

#define SSE2_YUV420_YUYV_UNALIGNED                  \
    xmm1 = _mm_loadl_epi64((__m128i *)p_u);         \
    xmm2 = _mm_loadl_epi64((__m128i *)p_v);         \
    xmm0 = _mm_loadu_si128((__m128i *)p_y1);        \
    xmm3 = _mm_loadu_si128((__m128i *)p_y2);        \
    _mm_prefetch(p_line1, _MM_HINT_NTA);            \
    _mm_prefetch(p_line2, _MM_HINT_NTA);            \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);           \
    xmm2 = xmm0;                                    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm1);           \
    _mm_storeu_si128((__m128i*)(p_line1), xmm2);    \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm1);           \
    _mm_storeu_si128((__m128i*)(p_line1+16), xmm0); \
    xmm4 = xmm3;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);           \
    _mm_storeu_si128((__m128i*)(p_line2), xmm4);    \
    xmm3 = _mm_unpackhi_epi8(xmm3, xmm1);           \
    _mm_storeu_si128((__m128i*)(p_line1+16), xmm3);

#define SSE2_YUV420_YVYU_ALIGNED                    \
    xmm1 = _mm_loadl_epi64((__m128i *)p_v);         \
    xmm2 = _mm_loadl_epi64((__m128i *)p_u);         \
    xmm0 = _mm_load_si128((__m128i *)p_y1);         \
    xmm3 = _mm_load_si128((__m128i *)p_y2);         \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);           \
    xmm2 = xmm0;                                    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm1);           \
    _mm_stream_si128((__m128i*)(p_line1), xmm2);    \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm1);           \
    _mm_stream_si128((__m128i*)(p_line1+16), xmm0); \
    xmm4 = xmm3;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);           \
    _mm_stream_si128((__m128i*)(p_line2), xmm4);    \
    xmm3 = _mm_unpackhi_epi8(xmm3, xmm1);           \
    _mm_stream_si128((__m128i*)(p_line1+16), xmm3);

#define SSE2_YUV420_YVYU_UNALIGNED                  \
    xmm1 = _mm_loadl_epi64((__m128i *)p_v);         \
    xmm2 = _mm_loadl_epi64((__m128i *)p_u);         \
    xmm0 = _mm_loadu_si128((__m128i *)p_y1);        \
    xmm3 = _mm_loadu_si128((__m128i *)p_y2);        \
    _mm_prefetch(p_line1, _MM_HINT_NTA);            \
    _mm_prefetch(p_line2, _MM_HINT_NTA);            \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);           \
    xmm2 = xmm0;                                    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm1);           \
    _mm_storeu_si128((__m128i*)(p_line1), xmm2);    \
    xmm0 = _mm_unpackhi_epi8(xmm0, xmm1);           \
    _mm_storeu_si128((__m128i*)(p_line1+16), xmm0); \
    xmm4 = xmm3;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);           \
    _mm_storeu_si128((__m128i*)(p_line2), xmm4);    \
    xmm3 = _mm_unpackhi_epi8(xmm3, xmm1);           \
    _mm_storeu_si128((__m128i*)(p_line1+16), xmm3);

#define SSE2_YUV420_UYVY_ALIGNED                    \
    xmm1 = _mm_loadl_epi64((__m128i *)p_u);         \
    xmm2 = _mm_loadl_epi64((__m128i *)p_v);         \
    xmm0 = _mm_load_si128((__m128i *)p_y1);         \
    xmm3 = _mm_load_si128((__m128i *)p_y2);         \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);           \
    xmm2 = xmm1;                                    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm0);           \
    _mm_stream_si128((__m128i*)(p_line1), xmm2);    \
    xmm2 = xmm1;                                    \
    xmm2 = _mm_unpackhi_epi8(xmm2, xmm0);           \
    _mm_stream_si128((__m128i*)(p_line1+16), xmm2); \
    xmm4 = xmm1;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm3);           \
    _mm_stream_si128((__m128i*)(p_line2), xmm4);    \
    xmm1 = _mm_unpackhi_epi8(xmm1, xmm3);           \
    _mm_stream_si128((__m128i*)(p_line1+16), xmm1);

#define SSE2_YUV420_UYVY_UNALIGNED                  \
    xmm1 = _mm_loadl_epi64((__m128i *)p_u);         \
    xmm2 = _mm_loadl_epi64((__m128i *)p_v);         \
    xmm0 = _mm_loadu_si128((__m128i *)p_y1);        \
    xmm3 = _mm_loadu_si128((__m128i *)p_y2);        \
    _mm_prefetch(p_line1, _MM_HINT_NTA);            \
    _mm_prefetch(p_line2, _MM_HINT_NTA);            \
    xmm1 = _mm_unpacklo_epi8(xmm1, xmm2);           \
    xmm2 = xmm1;                                    \
    xmm2 = _mm_unpacklo_epi8(xmm2, xmm0);           \
    _mm_storeu_si128((__m128i*)(p_line1), xmm2);    \
    xmm2 = xmm1;                                    \
    xmm2 = _mm_unpackhi_epi8(xmm2, xmm0);           \
    _mm_storeu_si128((__m128i*)(p_line1+16), xmm2); \
    xmm4 = xmm1;                                    \
    xmm4 = _mm_unpacklo_epi8(xmm4, xmm3);           \
    _mm_storeu_si128((__m128i*)(p_line2), xmm4);    \
    xmm1 = _mm_unpackhi_epi8(xmm1, xmm3);           \
    _mm_storeu_si128((__m128i*)(p_line1+16), xmm1);

#endif

#endif

/* Used in both accelerated and C modules */

#define C_YUV420_YVYU( )                                                    \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_v)++;                      \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_u)++;                      \

#define C_YUV420_Y211( )                                                    \
    *(p_line1)++ = *(p_y1); p_y1 += 2;                                      \
    *(p_line2)++ = *(p_y2); p_y2 += 2;                                      \
    *(p_line1)++ = *(p_line2)++ = *(p_u) - 0x80; p_u += 2;                  \
    *(p_line1)++ = *(p_y1); p_y1 += 2;                                      \
    *(p_line2)++ = *(p_y2); p_y2 += 2;                                      \
    *(p_line1)++ = *(p_line2)++ = *(p_v) - 0x80; p_v += 2;                  \


#define C_YUV420_YUYV( )                                                    \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_u)++;                      \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_v)++;                      \

#define C_YUV420_UYVY( )                                                    \
    *(p_line1)++ =            *(p_line2)++ = *(p_u)++;                      \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \
    *(p_line1)++ =            *(p_line2)++ = *(p_v)++;                      \
    *(p_line1)++ = *(p_y1)++; *(p_line2)++ = *(p_y2)++;                     \

