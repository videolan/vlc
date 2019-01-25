/*****************************************************************************
 * i420_yuy2.h : YUV to YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2019 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damien@videolan.org>
 *          Lyndon Brown <jnqnfe@gmail.com>
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

#if defined( MODULE_NAME_IS_i420_yuy2_sse2 )

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
movdqa      (%2), %%xmm0  # Load 16 Y1         yF yE yD ... y2 y1 y0   \n\
movdqa      (%3), %%xmm3  # Load 16 Y2         YF YE YD ... Y2 Y1 Y0   \n\
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
movdqu      (%2), %%xmm0  # Load 16 Y1         yF yE yD ... y2 y1 y0   \n\
movdqu      (%3), %%xmm3  # Load 16 Y2         YF YE YD ... Y2 Y1 Y0   \n\
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
movdqa      (%2), %%xmm0  # Load 16 Y1         yF yE yD ... y2 y1 y0   \n\
movdqa      (%3), %%xmm3  # Load 16 Y2         YF YE YD ... Y2 Y1 Y0   \n\
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
movdqu      (%2), %%xmm0  # Load 16 Y1          yF yE yD ... y2 y1 y0   \n\
movdqu      (%3), %%xmm3  # Load 16 Y2          YF YE YD ... Y2 Y1 Y0   \n\
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
movdqa      (%2), %%xmm0  # Load 16 Y1          yF yE yD ... y2 y1 y0   \n\
movdqa      (%3), %%xmm3  # Load 16 Y2          YF YE YD ... Y2 Y1 Y0   \n\
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
movdqu      (%2), %%xmm0  # Load 16 Y1          yF yE yD ... y2 y1 y0   \n\
movdqu      (%3), %%xmm3  # Load 16 Y2          YF YE YD ... Y2 Y1 Y0   \n\
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

#elif defined( MODULE_NAME_IS_i420_yuy2_avx2 )

#if defined(CAN_COMPILE_AVX2)

/* AVX2 assembly */

#define AVX2_CALL(AVX2_INSTRUCTIONS)     \
    do {                                 \
    __asm__ __volatile__(                \
        "p2align 3 \n\t"                 \
        AVX2_INSTRUCTIONS                \
        :                                \
        : [l1]"r"(p_line1), [l2]"r"(p_line2), \
          [y1]"r"(p_y1),  [y2]"r"(p_y2), \
          [u]"r"(p_u),  [v]"r"(p_v)      \
        : "ymm0", "ymm1", "ymm2", "ymm3", "ymm4"); \
        p_line1 += 64; p_line2 += 64;    \
        p_y1 += 32; p_y2 += 32;          \
        p_u += 16; p_v += 16;            \
    } while(0)

#define AVX2_END  __asm__ __volatile__ ( "sfence" ::: "memory" )

#define AVX2_INIT_ALIGNED "                                                   \n\
vmovdqa     ymm0, [[y1]]     ; Load 32 Y1                     ... y2  y1  y0  \n\
vmovdqa     ymm1, [[y2]]     ; Load 32 Y2                     ... Y2  Y1  Y0  \n\
vmovdqa     xmm2, [[u]]      ; Load 16 Cb into lower half     ... u2  u1  u0  \n\
vmovdqa     xmm3, [[v]]      ; Load 16 Cr into lower half     ... v2  v1  v0  \n\
"

#define AVX2_INIT_UNALIGNED "                                                 \n\
vmovdqu     ymm0, [[y1]]     ; Load 32 Y1                     ... y2  y1  y0  \n\
vmovdqu     ymm1, [[y2]]     ; Load 32 Y2                     ... Y2  Y1  Y0  \n\
vmovdqu     xmm2, [[u]]      ; Load 16 Cb into lower half     ... u2  u1  u0  \n\
vmovdqu     xmm3, [[v]]      ; Load 16 Cr into lower half     ... v2  v1  v0  \n\
prefetchnta [[l1]]           ; Tell CPU not to cache output data              \n\
prefetchnta [[l2]]           ; Tell CPU not to cache output data              \n\
"

#define AVX2_YUV420_YUYV_ALIGNED "                                            \n\
vpunpcklbw ymm2, ymm2, ymm3  ; Interleave u,v             ... v1  u1  v0  u0  \n\
vpunpcklbw ymm3, ymm0, ymm2  ; Interleave (low) y1,uv     ... v0  y1  u0  y0  \n\
vmovntdq   [[l1]], ymm3      ; Store low YUYV                                 \n\
vpunpckhbw ymm4, ymm0, ymm2  ; Interleave (high) y1,uv    ... v4 y17  u4 y16  \n\
vmovntdq   [[l1]+32], ymm4   ; Store high YUYV                                \n\
vpunpcklbw ymm3, ymm1, ymm2  ; Interleave (low) y2,uv     ... v0  Y1  u0  Y0  \n\
vmovntdq   [[l2]], ymm3      ; Store low YUYV                                 \n\
vpunpckhbw ymm4, ymm1, ymm2  ; Interleave (high) y2,uv    ... v4 Y17  u4 Y16  \n\
vmovntdq   [[l2]+32], ymm4   ; Store high YUYV                                \n\
"

#define AVX2_YUV420_YUYV_UNALIGNED "                                          \n\
vpunpcklbw ymm2, ymm2, ymm3  ; Interleave u,v             ... v1  u1  v0  u0  \n\
vpunpcklbw ymm3, ymm0, ymm2  ; Interleave (low) y1,uv     ... v0  y1  u0  y0  \n\
vmovdqu    [[l1]], ymm3      ; Store low YUYV                                 \n\
vpunpckhbw ymm4, ymm0, ymm2  ; Interleave (high) y1,uv    ... v4 y17  u4 y16  \n\
vmovdqu    [[l1]+32], ymm4   ; Store high YUYV                                \n\
vpunpcklbw ymm3, ymm1, ymm2  ; Interleave (low) y2,uv     ... v0  Y1  u0  Y0  \n\
vmovdqu    [[l2]], ymm3      ; Store low YUYV                                 \n\
vpunpckhbw ymm4, ymm1, ymm2  ; Interleave (high) y2,uv    ... v4 Y17  u4 Y16  \n\
vmovdqu    [[l2]+32], ymm4   ; Store high YUYV                                \n\
"

#define AVX2_YUV420_YVYU_ALIGNED "                                            \n\
vpunpcklbw ymm2, ymm3, ymm2  ; Interleave v,u             ... u1  v1  u0  v0  \n\
vpunpcklbw ymm3, ymm0, ymm2  ; Interleave (low) y1,vu     ... u0  y1  v0  y0  \n\
vmovntdq   [[l1]], ymm3      ; Store low YUYV                                 \n\
vpunpckhbw ymm4, ymm0, ymm2  ; Interleave (high) y1,vu    ... u4 y17  v4 y16  \n\
vmovntdq   [[l1]+32], ymm4   ; Store high YUYV                                \n\
vpunpcklbw ymm3, ymm1, ymm2  ; Interleave (low) y2,vu     ... u0  Y1  v0  Y0  \n\
vmovntdq   [[l2]], ymm3      ; Store low YUYV                                 \n\
vpunpckhbw ymm4, ymm1, ymm2  ; Interleave (high) y2,vu    ... u4 Y17  v4 Y16  \n\
vmovntdq   [[l2]+32], ymm4   ; Store high YUYV                                \n\
"

#define AVX2_YUV420_YVYU_UNALIGNED "                                          \n\
vpunpcklbw ymm2, ymm3, ymm2  ; Interleave v,u             ... u1  v1  u0  v0  \n\
vpunpcklbw ymm3, ymm0, ymm2  ; Interleave (low) y1,vu     ... u0  y1  v0  y0  \n\
vmovdqu    [[l1]], ymm3      ; Store low YUYV                                 \n\
vpunpckhbw ymm4, ymm0, ymm2  ; Interleave (high) y1,vu    ... u4 y17  v4 y16  \n\
vmovdqu    [[l1]+32], ymm4   ; Store high YUYV                                \n\
vpunpcklbw ymm3, ymm1, ymm2  ; Interleave (low) y2,vu     ... u0  Y1  v0  Y0  \n\
vmovdqu    [[l2]], ymm3      ; Store low YUYV                                 \n\
vpunpckhbw ymm4, ymm1, ymm2  ; Interleave (high) y2,vu    ... u4 Y17  v4 Y16  \n\
vmovdqu    [[l2]+32], ymm4   ; Store high YUYV                                \n\
"

#define AVX2_YUV420_UYVY_ALIGNED "                                            \n\
vpunpcklbw ymm2, ymm2, ymm3  ; Interleave u,v             ... v1  u1  v0  u0  \n\
vpunpcklbw ymm3, ymm2, ymm0  ; Interleave (low) uv,y1     ... y1  v0  y0  u0  \n\
vmovntdq   [[l1]], ymm3      ; Store low UYVY                                 \n\
vpunpckhbw ymm4, ymm2, ymm0  ; Interleave (high) uv,y1   ... y17  v8 y16  u8  \n\
vmovntdq   [[l1]+32], ymm4   ; Store high UYVY                                \n\
vpunpcklbw ymm3, ymm2, ymm1  ; Interleave (low) uv,y2     ... Y1  v0  Y0  u0  \n\
vmovntdq   [[l2]], ymm3      ; Store low UYVY                                 \n\
vpunpckhbw ymm4, ymm2, ymm1  ; Interleave (high) uv,y2   ... Y17  v8 Y16  u8  \n\
vmovntdq   [[l2]+32], ymm4   ; Store high UYVY                                \n\
"

#define AVX2_YUV420_UYVY_UNALIGNED "                                          \n\
vpunpcklbw ymm2, ymm2, ymm3  ; Interleave u,v             ... v1  u1  v0  u0  \n\
vpunpcklbw ymm3, ymm2, ymm0  ; Interleave (low) uv,y1     ... y1  v0  y0  u0  \n\
vmovdqu    [[l1]], ymm3      ; Store low UYVY                                 \n\
vpunpckhbw ymm4, ymm2, ymm0  ; Interleave (high) uv,y1   ... y17  v8 y16  u8  \n\
vmovdqu    [[l1]+32], ymm4   ; Store high UYVY                                \n\
vpunpcklbw ymm3, ymm2, ymm1  ; Interleave (low) uv,y2     ... Y1  v0  Y0  u0  \n\
vmovdqu    [[l2]], ymm3      ; Store low UYVY                                 \n\
vpunpckhbw ymm4, ymm2, ymm1  ; Interleave (high) uv,y2   ... Y17  v8 Y16  u8  \n\
vmovdqu    [[l2]+32], ymm4   ; Store high UYVY                                \n\
"

#elif defined(HAVE_AVX2_INTRINSICS)

/* AVX2 intrinsics */

#include <immintrin.h>

#define AVX2_CALL(AVX2_INSTRUCTIONS)            \
    do {                                        \
        __m256i ymm0, ymm1, ymm2, ymm3, ymm4;   \
        AVX2_INSTRUCTIONS                       \
        p_line1 += 64; p_line2 += 64;           \
        p_y1 += 32; p_y2 += 32;                 \
        p_u += 16; p_v += 16;                   \
    } while(0)

#define AVX2_END  _mm_sfence()

#define AVX2_INIT_ALIGNED                       \
    ymm0 = _mm256_load_si256((__m256i *)p_y1);  \
    ymm1 = _mm256_load_si256((__m256i *)p_y2);  \
    ymm2 = _mm256_inserti128_si256(ymm2, *((__m128i*)p_u), 0); \
    ymm3 = _mm256_inserti128_si256(ymm3, *((__m128i*)p_v), 0);

#define AVX2_INIT_UNALIGNED                     \
    ymm0 = _mm256_loadu_si256((__m256i *)p_y1); \
    ymm1 = _mm256_loadu_si256((__m256i *)p_y2); \
    ymm2 = _mm256_inserti128_si256(ymm2, *((__m128i*)p_u), 0); \
    ymm3 = _mm256_inserti128_si256(ymm3, *((__m128i*)p_v), 0); \
    _mm_prefetch(p_line1, _MM_HINT_NTA);        \
    _mm_prefetch(p_line2, _MM_HINT_NTA);

#define AVX2_YUV420_YUYV_ALIGNED                       \
    ymm2 = _mm256_unpacklo_epi8(ymm2, ymm3);           \
    ymm3 = _mm256_unpacklo_epi8(ymm0, ymm2);           \
    _mm256_stream_si256((__m256i*)(p_line1), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm0, ymm2);           \
    _mm256_stream_si256((__m256i*)(p_line1+32), ymm4); \
    ymm3 = _mm256_unpacklo_epi8(ymm1, ymm2);           \
    _mm256_stream_si256((__m256i*)(p_line2), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm1, ymm2);           \
    _mm256_stream_si256((__m256i*)(p_line1+32), ymm4);

#define AVX2_YUV420_YUYV_UNALIGNED                     \
    ymm2 = _mm256_unpacklo_epi8(ymm2, ymm3);           \
    ymm3 = _mm256_unpacklo_epi8(ymm0, ymm2);           \
    _mm256_storeu_si256((__m256i*)(p_line1), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm0, ymm2);           \
    _mm256_storeu_si256((__m256i*)(p_line1+32), ymm4); \
    ymm3 = _mm256_unpacklo_epi8(ymm1, ymm2);           \
    _mm256_storeu_si256((__m256i*)(p_line2), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm1, ymm2);           \
    _mm256_storeu_si256((__m256i*)(p_line1+32), ymm4);

#define AVX2_YUV420_YVYU_ALIGNED                       \
    ymm2 = _mm256_unpacklo_epi8(ymm3, ymm2);           \
    ymm3 = _mm256_unpacklo_epi8(ymm0, ymm2);           \
    _mm256_stream_si256((__m256i*)(p_line1), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm0, ymm2);           \
    _mm256_stream_si256((__m256i*)(p_line1+32), ymm4); \
    ymm3 = _mm256_unpacklo_epi8(ymm1, ymm2);           \
    _mm256_stream_si256((__m256i*)(p_line2), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm1, ymm2);           \
    _mm256_stream_si256((__m256i*)(p_line1+32), ymm4);

#define AVX2_YUV420_YVYU_UNALIGNED                     \
    ymm2 = _mm256_unpacklo_epi8(ymm3, ymm2);           \
    ymm3 = _mm256_unpacklo_epi8(ymm0, ymm2);           \
    _mm256_storeu_si256((__m256i*)(p_line1), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm0, ymm2);           \
    _mm256_storeu_si256((__m256i*)(p_line1+32), ymm4); \
    ymm3 = _mm256_unpacklo_epi8(ymm1, ymm2);           \
    _mm256_storeu_si256((__m256i*)(p_line2), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm1, ymm2);           \
    _mm256_storeu_si256((__m256i*)(p_line1+32), ymm4);

#define AVX2_YUV420_UYVY_ALIGNED                       \
    ymm2 = _mm256_unpacklo_epi8(ymm2, ymm3);           \
    ymm3 = _mm256_unpacklo_epi8(ymm2, ymm0);           \
    _mm256_stream_si256((__m128i*)(p_line1), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm2, ymm0);           \
    _mm256_stream_si256((__m256i*)(p_line1+32), ymm4); \
    ymm3 = _mm256_unpacklo_epi8(ymm2, ymm1);           \
    _mm256_stream_si256((__m256i*)(p_line2), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm2, ymm1);           \
    _mm256_stream_si256((__m256i*)(p_line1+32), ymm4);

#define AVX2_YUV420_UYVY_UNALIGNED                     \
    ymm2 = _mm256_unpacklo_epi8(ymm2, ymm3);           \
    ymm3 = _mm256_unpacklo_epi8(ymm2, ymm0);           \
    _mm256_storeu_si256((__m128i*)(p_line1), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm2, ymm0);           \
    _mm256_storeu_si256((__m256i*)(p_line1+32), ymm4); \
    ymm3 = _mm256_unpacklo_epi8(ymm2, ymm1);           \
    _mm256_storeu_si256((__m256i*)(p_line2), ymm3);    \
    ymm4 = _mm256_unpackhi_epi8(ymm2, ymm1);           \
    _mm256_storeu_si256((__m256i*)(p_line1+32), ymm4);

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

