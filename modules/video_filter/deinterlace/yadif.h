/*
 * Copyright (C) 2006 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with FFmpeg; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#if defined(__GNUC__)
#  define DECLARE_ALIGNED(n,t,v)      t __attribute__ ((aligned (n))) v
#  if VLC_GCC_VERSION(3,1)
#    define DECLARE_ASM_CONST(n,t,v)    static const t __attribute__((used)) __attribute__ ((aligned (n))) v
#  else
#    define DECLARE_ASM_CONST(n,t,v)    static const t __attribute__ ((aligned (n))) v
#  endif
#endif

typedef intptr_t x86_reg;
typedef struct { uint64_t a, b; } xmm_reg;

DECLARE_ASM_CONST(16, const xmm_reg, pb_1) = {0x0101010101010101ULL, 0x0101010101010101ULL};
DECLARE_ASM_CONST(16, const xmm_reg, pw_1) = {0x0001000100010001ULL, 0x0001000100010001ULL};


#ifdef CAN_COMPILE_SSSE3
#if defined(__SSE__) || VLC_GCC_VERSION(4, 4) || defined(__clang__)
// ================ SSSE3 =================
#define HAVE_YADIF_SSSE3
#define COMPILE_TEMPLATE_SSE 1
#define COMPILE_TEMPLATE_SSSE3 1
#define VLC_TARGET VLC_SSE
#define RENAME(a) a ## _ssse3
#include "yadif_template.h"
#undef COMPILE_TEMPLATE_SSE
#undef COMPILE_TEMPLATE_SSSE3
#undef VLC_TARGET
#undef RENAME
#endif
#endif

#ifdef CAN_COMPILE_SSE2
#if defined(__SSE__) || VLC_GCC_VERSION(4, 4) || defined(__clang__)
// ================= SSE2 =================
#define HAVE_YADIF_SSE2
#define COMPILE_TEMPLATE_SSE 1
#define VLC_TARGET VLC_SSE
#define RENAME(a) a ## _sse2
#include "yadif_template.h"
#undef COMPILE_TEMPLATE_SSE
#undef VLC_TARGET
#undef RENAME
#endif
#endif

#ifdef CAN_COMPILE_MMX
#if defined(__MMX__) || VLC_GCC_VERSION(4, 4) || defined(__clang__)
// ================ MMX =================
#define HAVE_YADIF_MMX
#define VLC_TARGET VLC_MMX
#define RENAME(a) a ## _mmx
#include "yadif_template.h"
#undef VLC_TARGET
#undef RENAME
#endif
#endif

#define FFABS abs

#define CHECK(j)\
    {   int score = FFABS(cur[mrefs-1+(j)] - cur[prefs-1-(j)])\
                  + FFABS(cur[mrefs  +(j)] - cur[prefs  -(j)])\
                  + FFABS(cur[mrefs+1+(j)] - cur[prefs+1-(j)]);\
        if (score < spatial_score) {\
            spatial_score= score;\
            spatial_pred= (cur[mrefs  +(j)] + cur[prefs  -(j)])>>1;\

#define FILTER \
    for (x = 0;  x < w; x++) { \
        int c = cur[mrefs]; \
        int d = (prev2[0] + next2[0])>>1; \
        int e = cur[prefs]; \
        int temporal_diff0 = FFABS(prev2[0] - next2[0]); \
        int temporal_diff1 =(FFABS(prev[mrefs] - c) + FFABS(prev[prefs] - e) )>>1; \
        int temporal_diff2 =(FFABS(next[mrefs] - c) + FFABS(next[prefs] - e) )>>1; \
        int diff = FFMAX3(temporal_diff0>>1, temporal_diff1, temporal_diff2); \
        int spatial_pred = (c+e)>>1; \
        int spatial_score = FFABS(cur[mrefs-1] - cur[prefs-1]) + FFABS(c-e) \
                          + FFABS(cur[mrefs+1] - cur[prefs+1]) - 1; \
 \
        CHECK(-1) CHECK(-2) }} }} \
        CHECK( 1) CHECK( 2) }} }} \
 \
        if (mode < 2) { \
            int b = (prev2[2*mrefs] + next2[2*mrefs])>>1; \
            int f = (prev2[2*prefs] + next2[2*prefs])>>1; \
            int max = FFMAX3(d-e, d-c, FFMIN(b-c, f-e)); \
            int min = FFMIN3(d-e, d-c, FFMAX(b-c, f-e)); \
 \
            diff = FFMAX3(diff, min, -max); \
        } \
 \
        if (spatial_pred > d + diff) \
           spatial_pred = d + diff; \
        else if (spatial_pred < d - diff) \
           spatial_pred = d - diff; \
 \
        dst[0] = spatial_pred; \
 \
        dst++; \
        cur++; \
        prev++; \
        next++; \
        prev2++; \
        next2++; \
    }

static void yadif_filter_line_c(uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next, int w, int prefs, int mrefs, int parity, int mode) {
    int x;
    uint8_t *prev2= parity ? prev : cur ;
    uint8_t *next2= parity ? cur  : next;
    FILTER
}

static void yadif_filter_line_c_16bit(uint16_t *dst, uint16_t *prev, uint16_t *cur, uint16_t *next, int w, int prefs, int mrefs, int parity, int mode) {
    int x;
    uint16_t *prev2= parity ? prev : cur ;
    uint16_t *next2= parity ? cur  : next;
    mrefs /= 2;
    prefs /= 2;
    FILTER
}
