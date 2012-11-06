/*****************************************************************************
 * blend2.cpp: Blend one picture with alpha onto another picture
 *****************************************************************************
 * Copyright (C) 2012 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include "filter_picture.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_description(N_("Video pictures blending"))
    set_capability("video blending", 100)
    set_callbacks(Open, Close)
vlc_module_end()

static inline unsigned div255(unsigned v)
{
    /* It is exact for 8 bits, and has a max error of 1 for 9 and 10 bits
     * while respecting full opacity/transparency */
    return ((v >> 8) + v + 1) >> 8;
    //return v / 255;
}

template <typename T>
void merge(T *dst, unsigned src, unsigned f)
{
    *dst = div255((255 - f) * (*dst) + src * f);
}

struct CPixel {
    unsigned i, j, k;
    unsigned a;
};

class CPicture {
public:
    CPicture(const picture_t *picture,
             const video_format_t *fmt,
             unsigned x, unsigned y) : picture(picture), fmt(fmt), x(x), y(y)
    {
    }
    CPicture(const CPicture &src) : picture(src.picture), fmt(src.fmt), x(src.x), y(src.y)
    {
    }
    const video_format_t *getFormat() const
    {
        return fmt;
    }
    bool isFull(unsigned) const
    {
        return true;
    }

protected:
    template <unsigned ry>
    uint8_t *getLine(unsigned plane = 0) const
    {
        return &picture->p[plane].p_pixels[(y / ry) * picture->p[plane].i_pitch];
    }
    const picture_t *picture;
    const video_format_t *fmt;
    unsigned x;
    unsigned y;
};

template <typename pixel, unsigned rx, unsigned ry, bool has_alpha, bool swap_uv>
class CPictureYUVPlanar : public CPicture {
public:
    CPictureYUVPlanar(const CPicture &cfg) : CPicture(cfg)
    {
        data[0] = CPicture::getLine< 1>(0);
        data[1] = CPicture::getLine<ry>(swap_uv ? 2 : 1);
        data[2] = CPicture::getLine<ry>(swap_uv ? 1 : 2);
        if (has_alpha)
            data[3] = CPicture::getLine<1>(3);
    }
    void get(CPixel *px, unsigned dx, bool full = true) const
    {
        px->i = *getPointer(0, dx);
        if (full) {
            px->j = *getPointer(1, dx);
            px->k = *getPointer(2, dx);
        }
        if (has_alpha)
            px->a = *getPointer(3, dx);
    }
    void merge(unsigned dx, const CPixel &spx, unsigned a, bool full)
    {
        ::merge(getPointer(0, dx), spx.i, a);
        if (full) {
            ::merge(getPointer(1, dx), spx.j, a);
            ::merge(getPointer(2, dx), spx.k, a);
        }
    }
    bool isFull(unsigned dx) const
    {
        return (y % ry) == 0 && ((x + dx) % rx) == 0;
    }
    void nextLine()
    {
        y++;
        data[0] += picture->p[0].i_pitch;
        if ((y % ry) == 0) {
            data[1] += picture->p[swap_uv ? 2 : 1].i_pitch;
            data[2] += picture->p[swap_uv ? 1 : 2].i_pitch;
        }
        if (has_alpha)
            data[3] += picture->p[3].i_pitch;
    }
private:
    pixel *getPointer(unsigned plane, unsigned dx) const
    {
        if (plane == 1 || plane == 2)
            return (pixel*)&data[plane][(x + dx) / rx * sizeof(pixel)];
        else
            return (pixel*)&data[plane][(x + dx) /  1 * sizeof(pixel)];
    }
    uint8_t *data[4];
};

template <bool swap_uv>
class CPictureYUVSemiPlanar : public CPicture {
public:
    CPictureYUVSemiPlanar(const CPicture &cfg) : CPicture(cfg)
    {
        data[0] = CPicture::getLine<1>(0);
        data[1] = CPicture::getLine<2>(1);
    }
    void get(CPixel *px, unsigned dx, bool full = true) const
    {
        px->i = *getPointer(0, dx);
        if (full) {
            px->j = getPointer(1, dx)[swap_uv];
            px->k = getPointer(1, dx)[!swap_uv];
        }
    }
    void merge(unsigned dx, const CPixel &spx, unsigned a, bool full)
    {
        ::merge(getPointer(0, dx), spx.i, a);
        if (full) {
            ::merge(&getPointer(1, dx)[ swap_uv], spx.j, a);
            ::merge(&getPointer(1, dx)[!swap_uv], spx.k, a);
        }
    }
    bool isFull(unsigned dx) const
    {
        return (y % 2) == 0 && ((x + dx) % 2) == 0;
    }
    void nextLine()
    {
        y++;
        data[0] += picture->p[0].i_pitch;
        if ((y % 2) == 0)
            data[1] += picture->p[1].i_pitch;
    }
private:
    uint8_t *getPointer(unsigned plane, unsigned dx) const
    {
        if (plane == 0)
            return &data[plane][x + dx];
        else
            return &data[plane][(x + dx) / 2 * 2];
    }
    uint8_t *data[2];
};

template <unsigned offset_y, unsigned offset_u, unsigned offset_v>
class CPictureYUVPacked : public CPicture {
public:
    CPictureYUVPacked(const CPicture &cfg) : CPicture(cfg)
    {
        data = CPicture::getLine<1>(0);
    }
    void get(CPixel *px, unsigned dx, bool full = true) const
    {
        uint8_t *data = getPointer(dx);
        px->i = data[offset_y];
        if (full) {
            px->j = data[offset_u];
            px->k = data[offset_v];
        }
    }
    void merge(unsigned dx, const CPixel &spx, unsigned a, bool full)
    {
        uint8_t *data = getPointer(dx);
        ::merge(&data[offset_y], spx.i, a);
        if (full) {
            ::merge(&data[offset_u], spx.j, a);
            ::merge(&data[offset_v], spx.k, a);
        }
    }
    bool isFull(unsigned dx) const
    {
        return ((x + dx) % 2) == 0;
    }
    void nextLine()
    {
        y++;
        data += picture->p[0].i_pitch;
    }
private:
    uint8_t *getPointer(unsigned dx) const
    {
        return &data[(x + dx) * 2];
    }
    uint8_t *data;
};

class CPictureYUVP : public CPicture {
public:
    CPictureYUVP(const CPicture &cfg) : CPicture(cfg)
    {
        data = CPicture::getLine<1>(0);
    }
    void get(CPixel *px, unsigned dx, bool = true) const
    {
        px->i = *getPointer(dx);
    }
    void nextLine()
    {
        y++;
        data += picture->p[0].i_pitch;
    }
private:
    uint8_t *getPointer(unsigned dx) const
    {
        return &data[x + dx];
    }
    uint8_t *data;
};

template <unsigned bytes, bool has_alpha>
class CPictureRGBX : public CPicture {
public:
    CPictureRGBX(const CPicture &cfg) : CPicture(cfg)
    {
        if (has_alpha) {
            offset_r = 0;
            offset_g = 1;
            offset_b = 2;
            offset_a = 3;
        } else {
#ifdef WORDS_BIGENDIAN
            offset_r = (8 * bytes - fmt->i_lrshift) / 8;
            offset_g = (8 * bytes - fmt->i_lgshift) / 8;
            offset_b = (8 * bytes - fmt->i_lbshift) / 8;
#else
            offset_r = fmt->i_lrshift / 8;
            offset_g = fmt->i_lgshift / 8;
            offset_b = fmt->i_lbshift / 8;
#endif
        }
        data = CPicture::getLine<1>(0);
    }
    void get(CPixel *px, unsigned dx, bool = true) const
    {
        const uint8_t *src = getPointer(dx);
        px->i = src[offset_r];
        px->j = src[offset_g];
        px->k = src[offset_b];
        if (has_alpha)
            px->a = src[offset_a];
    }
    void merge(unsigned dx, const CPixel &spx, unsigned a, bool)
    {
        uint8_t *dst = getPointer(dx);
        ::merge(&dst[offset_r], spx.i, a);
        ::merge(&dst[offset_g], spx.j, a);
        ::merge(&dst[offset_b], spx.k, a);
    }
    void nextLine()
    {
        y++;
        data += picture->p[0].i_pitch;
    }
private:
    uint8_t *getPointer(unsigned dx) const
    {
        return &data[(x + dx) * bytes];
    }
    unsigned offset_r;
    unsigned offset_g;
    unsigned offset_b;
    unsigned offset_a;
    uint8_t *data;
};

class CPictureRGB16 : public CPicture {
public:
    CPictureRGB16(const CPicture &cfg) : CPicture(cfg)
    {
        data = CPicture::getLine<1>(0);
    }
    void get(CPixel *px, unsigned dx, bool = true) const
    {
        const uint16_t data = *getPointer(dx);
        px->i = (data & fmt->i_rmask) >> fmt->i_lrshift;
        px->j = (data & fmt->i_gmask) >> fmt->i_lgshift;
        px->k = (data & fmt->i_bmask) >> fmt->i_lbshift;
    }
    void merge(unsigned dx, const CPixel &spx, unsigned a, bool full)
    {
        CPixel dpx;
        get(&dpx, dx, full);

        ::merge(&dpx.i, spx.i, a);
        ::merge(&dpx.j, spx.j, a);
        ::merge(&dpx.k, spx.k, a);

        *getPointer(dx) = (dpx.i << fmt->i_lrshift) |
                          (dpx.j << fmt->i_lgshift) |
                          (dpx.k << fmt->i_lbshift);
    }
    void nextLine()
    {
        y++;
        data += picture->p[0].i_pitch;
    }
private:
    uint16_t *getPointer(unsigned dx) const
    {
        return (uint16_t*)&data[(x + dx) * 2];
    }
    uint8_t *data;
};

typedef CPictureYUVPlanar<uint8_t,  1,1, true,  false> CPictureYUVA;

typedef CPictureYUVPlanar<uint8_t,  4,4, false, true>  CPictureYV9;
typedef CPictureYUVPlanar<uint8_t,  4,4, false, false> CPictureI410_8;

typedef CPictureYUVPlanar<uint8_t,  4,1, false, false> CPictureI411_8;

typedef CPictureYUVSemiPlanar<false>                   CPictureNV12;
typedef CPictureYUVSemiPlanar<true>                    CPictureNV21;

typedef CPictureYUVPlanar<uint8_t,  2,2, false, true>  CPictureYV12;
typedef CPictureYUVPlanar<uint8_t,  2,2, false, false> CPictureI420_8;
typedef CPictureYUVPlanar<uint16_t, 2,2, false, false> CPictureI420_16;

typedef CPictureYUVPlanar<uint8_t,  2,1, false, false> CPictureI422_8;
typedef CPictureYUVPlanar<uint16_t, 2,1, false, false> CPictureI422_16;

typedef CPictureYUVPlanar<uint8_t,  1,1, false, false> CPictureI444_8;
typedef CPictureYUVPlanar<uint16_t, 1,1, false, false> CPictureI444_16;

typedef CPictureYUVPacked<0, 1, 3> CPictureYUYV;
typedef CPictureYUVPacked<1, 0, 2> CPictureUYVY;
typedef CPictureYUVPacked<0, 3, 1> CPictureYVYU;
typedef CPictureYUVPacked<1, 2, 0> CPictureVYUY;

typedef CPictureRGBX<4, true>  CPictureRGBA;
typedef CPictureRGBX<4, false> CPictureRGB32;
typedef CPictureRGBX<3, false> CPictureRGB24;

struct convertNone {
    convertNone(const video_format_t *, const video_format_t *) {}
    void operator()(CPixel &)
    {
    }
};

template <unsigned dst, unsigned src>
struct convertBits {
    convertBits(const video_format_t *, const video_format_t *) {}
    void operator()(CPixel &p)
    {
        p.i = p.i * ((1 << dst) - 1) /  ((1 << src) - 1);
        p.j = p.j * ((1 << dst) - 1) /  ((1 << src) - 1);
        p.k = p.k * ((1 << dst) - 1) /  ((1 << src) - 1);
    }
};
typedef convertBits< 9, 8> convert8To9Bits;
typedef convertBits<10, 8> convert8To10Bits;

struct convertRgbToYuv8 {
    convertRgbToYuv8(const video_format_t *, const video_format_t *) {}
    void operator()(CPixel &p)
    {
        uint8_t y, u, v;
        rgb_to_yuv(&y, &u, &v, p.i, p.j, p.k);
        p.i = y;
        p.j = u;
        p.k = v;
    }
};

struct convertYuv8ToRgb {
    convertYuv8ToRgb(const video_format_t *, const video_format_t *) {}
    void operator()(CPixel &p)
    {
        int r, g, b;
        yuv_to_rgb(&r, &g, &b, p.i, p.j, p.k);
        p.i = r;
        p.j = g;
        p.k = b;
    }
};

struct convertRgbToRgbSmall {
    convertRgbToRgbSmall(const video_format_t *dst, const video_format_t *) : fmt(*dst) {}
    void operator()(CPixel &p)
    {
        p.i >>= fmt.i_rrshift;
        p.j >>= fmt.i_rgshift;
        p.k >>= fmt.i_rbshift;
    }
private:
    const video_format_t &fmt;
};

struct convertYuvpToAny {
    void operator()(CPixel &p)
    {
        unsigned index = p.i;
        p.i = palette.palette[index][0];
        p.j = palette.palette[index][1];
        p.k = palette.palette[index][2];
        p.a = palette.palette[index][3];
    }
protected:
    video_palette_t palette;
};
struct convertYuvpToYuva8 : public convertYuvpToAny {
    convertYuvpToYuva8(const video_format_t *, const video_format_t *src)
    {
        palette = *src->p_palette;
    }
};
struct convertYuvpToRgba : public convertYuvpToAny {
    convertYuvpToRgba(const video_format_t *, const video_format_t *src)
    {
        const video_palette_t *p = src->p_palette;
        for (int i = 0; i < p->i_entries; i++) {
            int r, g, b;
            yuv_to_rgb(&r, &g, &b,
                       p->palette[i][0],
                       p->palette[i][1],
                       p->palette[i][2]);
            palette.palette[i][0] = r;
            palette.palette[i][1] = g;
            palette.palette[i][2] = b;
            palette.palette[i][3] = p->palette[i][3];
        }
    }
};

template <class G, class F>
struct compose {
    compose(const video_format_t *dst, const video_format_t *src) : f(dst, src), g(dst, src) {}
    void operator()(CPixel &p)
    {
        f(p);
        g(p);
    }
private:
    F f;
    G g;
};

template <class TDst, class TSrc, class TConvert>
void Blend(const CPicture &dst_data, const CPicture &src_data,
           unsigned width, unsigned height, int alpha)
{
    TSrc src(src_data);
    TDst dst(dst_data);
    TConvert convert(dst_data.getFormat(), src_data.getFormat());

    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            CPixel spx;

            src.get(&spx, x);
            convert(spx);

            unsigned a = div255(alpha * spx.a);
            if (a <= 0)
                continue;

            if (dst.isFull(x))
                dst.merge(x, spx, a, true);
            else
                dst.merge(x, spx, a, false);
        }
        src.nextLine();
        dst.nextLine();
    }
}

typedef void (*blend_function_t)(const CPicture &dst_data, const CPicture &src_data,
                                 unsigned width, unsigned height, int alpha);

static const struct {
    vlc_fourcc_t     dst;
    vlc_fourcc_t     src;
    blend_function_t blend;
} blends[] = {
#undef RGB
#undef YUV
#define RGB(csp, picture, cvt) \
    { csp, VLC_CODEC_YUVA, Blend<picture, CPictureYUVA, compose<cvt, convertYuv8ToRgb> > }, \
    { csp, VLC_CODEC_RGBA, Blend<picture, CPictureRGBA, compose<cvt, convertNone> > }, \
    { csp, VLC_CODEC_YUVP, Blend<picture, CPictureYUVP, compose<cvt, convertYuvpToRgba> > }
#define YUV(csp, picture, cvt) \
    { csp, VLC_CODEC_YUVA, Blend<picture, CPictureYUVA, compose<cvt, convertNone> > }, \
    { csp, VLC_CODEC_RGBA, Blend<picture, CPictureRGBA, compose<cvt, convertRgbToYuv8> > }, \
    { csp, VLC_CODEC_YUVP, Blend<picture, CPictureYUVP, compose<cvt, convertYuvpToYuva8> > }

    RGB(VLC_CODEC_RGB15,    CPictureRGB16,    convertRgbToRgbSmall),
    RGB(VLC_CODEC_RGB16,    CPictureRGB16,    convertRgbToRgbSmall),
    RGB(VLC_CODEC_RGB24,    CPictureRGB24,    convertNone),
    RGB(VLC_CODEC_RGB32,    CPictureRGB32,    convertNone),

    YUV(VLC_CODEC_YV9,      CPictureYV9,      convertNone),
    YUV(VLC_CODEC_I410,     CPictureI410_8,   convertNone),

    YUV(VLC_CODEC_I411,     CPictureI411_8,   convertNone),

    YUV(VLC_CODEC_YV12,     CPictureYV12,     convertNone),
    YUV(VLC_CODEC_NV12,     CPictureNV12,     convertNone),
    YUV(VLC_CODEC_NV21,     CPictureNV21,     convertNone),
    YUV(VLC_CODEC_J420,     CPictureI420_8,   convertNone),
    YUV(VLC_CODEC_I420,     CPictureI420_8,   convertNone),
#ifdef WORDS_BIGENDIAN
    YUV(VLC_CODEC_I420_9B,  CPictureI420_16,  convert8To9Bits),
    YUV(VLC_CODEC_I420_10B, CPictureI420_16,  convert8To10Bits),
#else
    YUV(VLC_CODEC_I420_9L,  CPictureI420_16,  convert8To9Bits),
    YUV(VLC_CODEC_I420_10L, CPictureI420_16,  convert8To10Bits),
#endif

    YUV(VLC_CODEC_J422,     CPictureI422_8,   convertNone),
    YUV(VLC_CODEC_I422,     CPictureI422_8,   convertNone),
#ifdef WORDS_BIGENDIAN
    YUV(VLC_CODEC_I422_9B,  CPictureI422_16,  convert8To9Bits),
    YUV(VLC_CODEC_I422_10B, CPictureI422_16,  convert8To10Bits),
#else
    YUV(VLC_CODEC_I422_9L,  CPictureI422_16,  convert8To9Bits),
    YUV(VLC_CODEC_I422_10L, CPictureI422_16,  convert8To10Bits),
#endif

    YUV(VLC_CODEC_J444,     CPictureI444_8,   convertNone),
    YUV(VLC_CODEC_I444,     CPictureI444_8,   convertNone),
#ifdef WORDS_BIGENDIAN
    YUV(VLC_CODEC_I444_9B,  CPictureI444_16,  convert8To9Bits),
    YUV(VLC_CODEC_I444_10B, CPictureI444_16,  convert8To10Bits),
#else
    YUV(VLC_CODEC_I444_9L,  CPictureI444_16,  convert8To9Bits),
    YUV(VLC_CODEC_I444_10L, CPictureI444_16,  convert8To10Bits),
#endif

    YUV(VLC_CODEC_YUYV,     CPictureYUYV,     convertNone),
    YUV(VLC_CODEC_UYVY,     CPictureUYVY,     convertNone),
    YUV(VLC_CODEC_YVYU,     CPictureYVYU,     convertNone),
    YUV(VLC_CODEC_VYUY,     CPictureVYUY,     convertNone),

#undef RGB
#undef YUV
};

struct filter_sys_t {
    filter_sys_t() : blend(NULL)
    {
    }
    blend_function_t blend;
};

/**
 * It blends 2 picture together.
 */
static void Blend(filter_t *filter,
                  picture_t *dst, const picture_t *src,
                  int x_offset, int y_offset, int alpha)
{
    filter_sys_t *sys = filter->p_sys;

    if( x_offset < 0 || y_offset < 0 )
    {
        msg_Err( filter, "Blend cannot process negative offsets" );
        return;
    }

    int width  = __MIN((int)filter->fmt_out.video.i_visible_width - x_offset,
                       (int)filter->fmt_in.video.i_visible_width);
    int height = __MIN((int)filter->fmt_out.video.i_visible_height - y_offset,
                       (int)filter->fmt_in.video.i_visible_height);
    if (width <= 0 || height <= 0 || alpha <= 0)
        return;

    video_format_FixRgb(&filter->fmt_out.video);
    video_format_FixRgb(&filter->fmt_in.video);

    sys->blend(CPicture(dst, &filter->fmt_out.video,
                        filter->fmt_out.video.i_x_offset + x_offset,
                        filter->fmt_out.video.i_y_offset + y_offset),
               CPicture(src, &filter->fmt_in.video,
                        filter->fmt_in.video.i_x_offset,
                        filter->fmt_in.video.i_y_offset),
               width, height, alpha);
}

static int Open(vlc_object_t *object)
{
    filter_t *filter = (filter_t *)object;
    const vlc_fourcc_t src = filter->fmt_in.video.i_chroma;
    const vlc_fourcc_t dst = filter->fmt_out.video.i_chroma;

    filter_sys_t *sys = new filter_sys_t();
    for (size_t i = 0; i < sizeof(blends) / sizeof(*blends); i++) {
        if (blends[i].src == src && blends[i].dst == dst)
            sys->blend = blends[i].blend;
    }

    if (!sys->blend) {
       msg_Err(filter, "no matching alpha blending routine (chroma: %4.4s -> %4.4s)",
               (char *)&src, (char *)&dst);
        delete sys;
        return VLC_EGENERIC;
    }

    filter->pf_video_blend = Blend;
    filter->p_sys          = sys;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    filter_t *filter = (filter_t *)object;
    delete filter->p_sys;
}

