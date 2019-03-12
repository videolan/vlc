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

#ifdef MODULE_NAME_IS_i420_yuy2_mmx

#if defined(CAN_COMPILE_MMX)

/* MMX assembly */
 
#define MMX_CALL(MMX_INSTRUCTIONS)          \
    do {                                    \
    __asm__ __volatile__(                   \
        ".p2align 3 \n\t                    \
movd       (%0), %%mm1  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%1), %%mm2  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
movq       (%2), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movq       (%3), %%mm3  # Load 8 Y            Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0     \n\
" \
        :                                   \
        : "r" (p_u), "r" (p_v),             \
          "r" (p_y1), "r" (p_y2)            \
        : "mm0", "mm1", "mm2", "mm3");      \
    __asm__ __volatile__(                   \
        ".p2align 3 \n\t"                   \
        MMX_INSTRUCTIONS                    \
        :                                   \
        : "r" (p_line1), "r" (p_line2)      \
        : "mm0", "mm1", "mm2", "mm3");      \
        p_line1 += 16; p_line2 += 16;       \
        p_y1 += 8; p_y2 += 8;               \
        p_u += 4; p_v += 4;                 \
    } while(0)

#define MMX_END __asm__ __volatile__ ( "emms" )

#define MMX_YUV420_YUYV "                                                 \n\
punpcklbw %%mm2, %%mm1  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
movq      %%mm0, %%mm2  #                     y7 y6 y5 y4 y3 y2 y1 y0     \n\
punpcklbw %%mm1, %%mm2  #                     v1 y3 u1 y2 v0 y1 u0 y0     \n\
movq      %%mm2, (%0)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm0  #                     v3 y7 u3 y6 v2 y5 u2 y4     \n\
movq      %%mm0, 8(%0)  # Store high YUYV                                 \n\
movq      %%mm3, %%mm4  #                     Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0     \n\
punpcklbw %%mm1, %%mm4  #                     v1 Y3 u1 Y2 v0 Y1 u0 Y0     \n\
movq      %%mm4, (%1)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm3  #                     v3 Y7 u3 Y6 v2 Y5 u2 Y4     \n\
movq      %%mm3, 8(%1)  # Store high YUYV                                 \n\
"

#define MMX_YUV420_YVYU "                                                 \n\
punpcklbw %%mm1, %%mm2  #                     u3 v3 u2 v2 u1 v1 u0 v0     \n\
movq      %%mm0, %%mm1  #                     y7 y6 y5 y4 y3 y2 y1 y0     \n\
punpcklbw %%mm2, %%mm1  #                     u1 y3 v1 y2 u0 y1 v0 y0     \n\
movq      %%mm1, (%0)   # Store low YUYV                                  \n\
punpckhbw %%mm2, %%mm0  #                     u3 y7 v3 y6 u2 y5 v2 y4     \n\
movq      %%mm0, 8(%0)  # Store high YUYV                                 \n\
movq      %%mm3, %%mm4  #                     Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0     \n\
punpcklbw %%mm2, %%mm4  #                     u1 Y3 v1 Y2 u0 Y1 v0 Y0     \n\
movq      %%mm4, (%1)   # Store low YUYV                                  \n\
punpckhbw %%mm2, %%mm3  #                     u3 Y7 v3 Y6 u2 Y5 v2 Y4     \n\
movq      %%mm3, 8(%1)  # Store high YUYV                                 \n\
"

#define MMX_YUV420_UYVY "                                                 \n\
punpcklbw %%mm2, %%mm1  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
movq      %%mm1, %%mm2  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
punpcklbw %%mm0, %%mm2  #                     y3 v1 y2 u1 y1 v0 y0 u0     \n\
movq      %%mm2, (%0)   # Store low UYVY                                  \n\
movq      %%mm1, %%mm2  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
punpckhbw %%mm0, %%mm2  #                     y7 v3 y6 u3 y5 v2 y4 u2     \n\
movq      %%mm2, 8(%0)  # Store high UYVY                                 \n\
movq      %%mm1, %%mm4  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
punpcklbw %%mm3, %%mm4  #                     Y3 v1 Y2 u1 Y1 v0 Y0 u0     \n\
movq      %%mm4, (%1)   # Store low UYVY                                  \n\
punpckhbw %%mm3, %%mm1  #                     Y7 v3 Y6 u3 Y5 v2 Y4 u2     \n\
movq      %%mm1, 8(%1)  # Store high UYVY                                 \n\
"

#elif defined(HAVE_MMX_INTRINSICS)

/* MMX intrinsics */

#include <mmintrin.h>

#define MMX_CALL(MMX_INSTRUCTIONS)          \
    do {                                    \
        __m64 mm0, mm1, mm2, mm3, mm4;      \
        MMX_INSTRUCTIONS                    \
        p_line1 += 16; p_line2 += 16;       \
        p_y1 += 8; p_y2 += 8;               \
        p_u += 4; p_v += 4;                 \
    } while(0)

#define MMX_END _mm_empty()
 
#define MMX_YUV420_YUYV                     \
    mm1 = _mm_cvtsi32_si64(*(int*)p_u);     \
    mm2 = _mm_cvtsi32_si64(*(int*)p_v);     \
    mm0 = (__m64)*(uint64_t*)p_y1;          \
    mm3 = (__m64)*(uint64_t*)p_y2;          \
    mm1 = _mm_unpacklo_pi8(mm1, mm2);       \
    mm2 = mm0;                              \
    mm2 = _mm_unpacklo_pi8(mm2, mm1);       \
    *(uint64_t*)p_line1 = (uint64_t)mm2;    \
    mm0 = _mm_unpackhi_pi8(mm0, mm1);       \
    *(uint64_t*)(p_line1+8) = (uint64_t)mm0;\
    mm4 = mm3;                              \
    mm4 = _mm_unpacklo_pi8(mm4, mm1);       \
    *(uint64_t*)p_line2 = (uint64_t)mm4;    \
    mm3 = _mm_unpackhi_pi8(mm3, mm1);       \
    *(uint64_t*)(p_line2+8) = (uint64_t)mm3;

#define MMX_YUV420_YVYU                     \
    mm2 = _mm_cvtsi32_si64(*(int*)p_u);     \
    mm1 = _mm_cvtsi32_si64(*(int*)p_v);     \
    mm0 = (__m64)*(uint64_t*)p_y1;          \
    mm3 = (__m64)*(uint64_t*)p_y2;          \
    mm1 = _mm_unpacklo_pi8(mm1, mm2);       \
    mm2 = mm0;                              \
    mm2 = _mm_unpacklo_pi8(mm2, mm1);       \
    *(uint64_t*)p_line1 = (uint64_t)mm2;    \
    mm0 = _mm_unpackhi_pi8(mm0, mm1);       \
    *(uint64_t*)(p_line1+8) = (uint64_t)mm0;\
    mm4 = mm3;                              \
    mm4 = _mm_unpacklo_pi8(mm4, mm1);       \
    *(uint64_t*)p_line2 = (uint64_t)mm4;    \
    mm3 = _mm_unpackhi_pi8(mm3, mm1);       \
    *(uint64_t*)(p_line2+8) = (uint64_t)mm3;

#define MMX_YUV420_UYVY                     \
    mm1 = _mm_cvtsi32_si64(*(int*)p_u);     \
    mm2 = _mm_cvtsi32_si64(*(int*)p_v);     \
    mm0 = (__m64)*(uint64_t*)p_y1;          \
    mm3 = (__m64)*(uint64_t*)p_y2;          \
    mm1 = _mm_unpacklo_pi8(mm1, mm2);       \
    mm2 = mm1;                              \
    mm2 = _mm_unpacklo_pi8(mm2, mm0);       \
    *(uint64_t*)p_line1 = (uint64_t)mm2;    \
    mm2 = mm1;                              \
    mm2 = _mm_unpackhi_pi8(mm2, mm0);       \
    *(uint64_t*)(p_line1+8) = (uint64_t)mm2;\
    mm4 = mm1;                              \
    mm4 = _mm_unpacklo_pi8(mm4, mm3);       \
    *(uint64_t*)p_line2 = (uint64_t)mm4;    \
    mm1 = _mm_unpackhi_pi8(mm1, mm3);       \
    *(uint64_t*)(p_line2+8) = (uint64_t)mm1;

#endif

#elif defined( MODULE_NAME_IS_i420_yuy2_sse2 )

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

