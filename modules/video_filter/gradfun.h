/*
 * Copyright (c) 2010 Nolan Lum <nol888@gmail.com>
 * Copyright (c) 2009 Loren Merritt <lorenm@u.washignton.edu>
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Debanding algorithm (from gradfun2db by prunedtree):
 * Boxblur.
 * Foreach pixel, if it's within threshold of the blurred value, make it closer.
 * So now we have a smoothed and higher bitdepth version of all the shallow
 * gradients, while leaving detailed areas untouched.
 * Dither it back to 8bit.
 */

struct vf_priv_s {
    int thresh;
    int radius;
    uint16_t *buf;
    void (*filter_line)(uint8_t *dst, uint8_t *src, uint16_t *dc,
                        int width, int thresh, const uint16_t *dithers);
    void (*blur_line)(uint16_t *dc, uint16_t *buf, uint16_t *buf1,
                      uint8_t *src, int sstride, int width);
};

static const uint16_t __attribute__((aligned(16))) pw_7f[8] = {127,127,127,127,127,127,127,127};
static const uint16_t __attribute__((aligned(16))) pw_ff[8] = {255,255,255,255,255,255,255,255};
static const uint16_t __attribute__((aligned(16))) dither[8][8] = {
    {  0, 96, 24,120,  6,102, 30,126 },
    { 64, 32, 88, 56, 70, 38, 94, 62 },
    { 16,112,  8,104, 22,118, 14,110 },
    { 80, 48, 72, 40, 86, 54, 78, 46 },
    {  4,100, 28,124,  2, 98, 26,122 },
    { 68, 36, 92, 60, 66, 34, 90, 58 },
    { 20,116, 12,108, 18,114, 10,106 },
    { 84, 52, 76, 44, 82, 50, 74, 42 },
};

static void filter_line_c(uint8_t *dst, uint8_t *src, uint16_t *dc,
                          int width, int thresh, const uint16_t *dithers)
{
    int x;
    for (x=0; x<width; x++, dc+=x&1) {
        int pix = src[x]<<7;
        int delta = dc[0] - pix;
        int m = abs(delta) * thresh >> 16;
        m = FFMAX(0, 127-m);
        m = m*m*delta >> 14;
        pix += m + dithers[x&7];
        dst[x] = av_clip_uint8(pix>>7);
    }
}

static void blur_line_c(uint16_t *dc, uint16_t *buf, uint16_t *buf1,
                        uint8_t *src, int sstride, int width)
{
    int x, v, old;
    for (x=0; x<width; x++) {
        v = buf1[x] + src[2*x] + src[2*x+1] + src[2*x+sstride] + src[2*x+1+sstride];
        old = buf[x];
        buf[x] = v;
        dc[x] = v - old;
    }
}

#if HAVE_MMX2
VLC_MMX
static void filter_line_mmx2(uint8_t *dst, uint8_t *src, uint16_t *dc,
                             int width, int thresh, const uint16_t *dithers)
{
    intptr_t x;
    if (width&3) {
        x = width&~3;
        filter_line_c(dst+x, src+x, dc+x/2, width-x, thresh, dithers);
        width = x;
    }
    x = -width;
    __asm__ volatile(
        "movd          %4, %%mm5 \n"
        "pxor       %%mm7, %%mm7 \n"
        "pshufw $0, %%mm5, %%mm5 \n"
        "movq          %6, %%mm6 \n"
        "movq          %5, %%mm4 \n"
        "1: \n"
        "movd     (%2,%0), %%mm0 \n"
        "movd     (%3,%0), %%mm1 \n"
        "punpcklbw  %%mm7, %%mm0 \n"
        "punpcklwd  %%mm1, %%mm1 \n"
        "psllw         $7, %%mm0 \n"
        "pxor       %%mm2, %%mm2 \n"
        "psubw      %%mm0, %%mm1 \n" // delta = dc - pix
        "psubw      %%mm1, %%mm2 \n"
        "pmaxsw     %%mm1, %%mm2 \n"
        "pmulhuw    %%mm5, %%mm2 \n" // m = abs(delta) * thresh >> 16
        "psubw      %%mm6, %%mm2 \n"
        "pminsw     %%mm7, %%mm2 \n" // m = -max(0, 127-m)
        "pmullw     %%mm2, %%mm2 \n"
        "paddw      %%mm4, %%mm0 \n" // pix += dither
        "pmulhw     %%mm2, %%mm1 \n"
        "psllw         $2, %%mm1 \n" // m = m*m*delta >> 14
        "paddw      %%mm1, %%mm0 \n" // pix += m
        "psraw         $7, %%mm0 \n"
        "packuswb   %%mm0, %%mm0 \n"
        "movd       %%mm0, (%1,%0) \n" // dst = clip(pix>>7)
        "add           $4, %0 \n"
        "jl 1b \n"
        "emms \n"
        :"+r"(x)
        :"r"(dst+width), "r"(src+width), "r"(dc+width/2),
         "rm"(thresh), "m"(*dithers), "m"(*pw_7f)
        :"mm0", "mm1", "mm2", "mm4", "mm5", "mm6", "memory"
    );
}
#endif

#if HAVE_SSSE3
VLC_SSE
static void filter_line_ssse3(uint8_t *dst, uint8_t *src, uint16_t *dc,
                              int width, int thresh, const uint16_t *dithers)
{
    intptr_t x;
    if (width&7) {
        // could be 10% faster if I somehow eliminated this
        x = width&~7;
        filter_line_c(dst+x, src+x, dc+x/2, width-x, thresh, dithers);
        width = x;
    }
    x = -width;
    __asm__ volatile(
        "movd           %4, %%xmm5 \n"
        "pxor       %%xmm7, %%xmm7 \n"
        "pshuflw $0,%%xmm5, %%xmm5 \n"
        "movdqa         %6, %%xmm6 \n"
        "punpcklqdq %%xmm5, %%xmm5 \n"
        "movdqa         %5, %%xmm4 \n"
        "1: \n"
        "movq      (%2,%0), %%xmm0 \n"
        "movq      (%3,%0), %%xmm1 \n"
        "punpcklbw  %%xmm7, %%xmm0 \n"
        "punpcklwd  %%xmm1, %%xmm1 \n"
        "psllw          $7, %%xmm0 \n"
        "psubw      %%xmm0, %%xmm1 \n" // delta = dc - pix
        "pabsw      %%xmm1, %%xmm2 \n"
        "pmulhuw    %%xmm5, %%xmm2 \n" // m = abs(delta) * thresh >> 16
        "psubw      %%xmm6, %%xmm2 \n"
        "pminsw     %%xmm7, %%xmm2 \n" // m = -max(0, 127-m)
        "pmullw     %%xmm2, %%xmm2 \n"
        "psllw          $1, %%xmm2 \n"
        "paddw      %%xmm4, %%xmm0 \n" // pix += dither
        "pmulhrsw   %%xmm2, %%xmm1 \n" // m = m*m*delta >> 14
        "paddw      %%xmm1, %%xmm0 \n" // pix += m
        "psraw          $7, %%xmm0 \n"
        "packuswb   %%xmm0, %%xmm0 \n"
        "movq       %%xmm0, (%1,%0) \n" // dst = clip(pix>>7)
        "add            $8, %0 \n"
        "jl 1b \n"
        :"+&r"(x)
        :"r"(dst+width), "r"(src+width), "r"(dc+width/2),
         "rm"(thresh), "m"(*dithers), "m"(*pw_7f)
        :"xmm0", "xmm1", "xmm2", "xmm4", "xmm5", "xmm6", "xmm7", "memory"
    );
}
#endif // HAVE_SSSE3

#if HAVE_SSE2 && HAVE_6REGS
#define BLURV(load)\
    intptr_t x = -2*width;\
    __asm__ volatile(\
        "movdqa %6, %%xmm7 \n"\
        "1: \n"\
        load"   (%4,%0), %%xmm0 \n"\
        load"   (%5,%0), %%xmm1 \n"\
        "movdqa  %%xmm0, %%xmm2 \n"\
        "movdqa  %%xmm1, %%xmm3 \n"\
        "psrlw       $8, %%xmm0 \n"\
        "psrlw       $8, %%xmm1 \n"\
        "pand    %%xmm7, %%xmm2 \n"\
        "pand    %%xmm7, %%xmm3 \n"\
        "paddw   %%xmm1, %%xmm0 \n"\
        "paddw   %%xmm3, %%xmm2 \n"\
        "paddw   %%xmm2, %%xmm0 \n"\
        "paddw  (%2,%0), %%xmm0 \n"\
        "movdqa (%1,%0), %%xmm1 \n"\
        "movdqa  %%xmm0, (%1,%0) \n"\
        "psubw   %%xmm1, %%xmm0 \n"\
        "movdqa  %%xmm0, (%3,%0) \n"\
        "add        $16, %0 \n"\
        "jl 1b \n"\
        :"+&r"(x)\
        :"r"(buf+width),\
         "r"(buf1+width),\
         "r"(dc+width),\
         "r"(src+width*2),\
         "r"(src+width*2+sstride),\
         "m"(*pw_ff)\
        :"xmm0", "xmm1", "xmm2", "xmm3", "xmm7", "memory"\
    );

VLC_SSE
static void blur_line_sse2(uint16_t *dc, uint16_t *buf, uint16_t *buf1,
                           uint8_t *src, int sstride, int width)
{
    if (((intptr_t)src|sstride)&15) {
        BLURV("movdqu");
    } else {
        BLURV("movdqa");
    }
}
#endif // HAVE_6REGS && HAVE_SSE2

static void filter_plane(struct vf_priv_s *ctx, uint8_t *dst, uint8_t *src,
                         int width, int height, int dstride, int sstride, int r)
{
    int bstride = ((width+15)&~15)/2;
    int y;
    uint32_t dc_factor = (1<<21)/(r*r);
    uint16_t *dc = ctx->buf+16;
    uint16_t *buf = ctx->buf+bstride+32;
    int thresh = ctx->thresh;

    memset(dc, 0, (bstride+16)*sizeof(*buf));
    for (y=0; y<r; y++)
        ctx->blur_line(dc, buf+y*bstride, buf+(y-1)*bstride, src+2*y*sstride, sstride, width/2);
    for (;;) {
        if (y < height-r) {
            int mod = ((y+r)/2)%r;
            uint16_t *buf0 = buf+mod*bstride;
            uint16_t *buf1 = buf+(mod?mod-1:r-1)*bstride;
            int x, v;
            ctx->blur_line(dc, buf0, buf1, src+(y+r)*sstride, sstride, width/2);
            for (x=v=0; x<r; x++)
                v += dc[x];
            for (; x<width/2; x++) {
                v += dc[x] - dc[x-r];
                dc[x-r] = v * dc_factor >> 16;
            }
            for (; x<(width+r+1)/2; x++)
                dc[x-r] = v * dc_factor >> 16;
            for (x=-r/2; x<0; x++)
                dc[x] = dc[0];
        }
        if (y == r) {
            for (y=0; y<r; y++)
                ctx->filter_line(dst+y*dstride, src+y*sstride, dc-r/2, width, thresh, dither[y&7]);
        }
        ctx->filter_line(dst+y*dstride, src+y*sstride, dc-r/2, width, thresh, dither[y&7]);
        if (++y >= height) break;
        ctx->filter_line(dst+y*dstride, src+y*sstride, dc-r/2, width, thresh, dither[y&7]);
        if (++y >= height) break;
    }
}

