/*****************************************************************************
 * orient.c: image reorientation video conversion
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 * Copyright (C) 2010 Laurent Aimar
 * Copyright (C) 2012 Rémi Denis-Courmont
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#   include "config.h"
#endif
#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_mouse.h>
#include <vlc_picture.h>

static void HFlip(int *sx, int *sy, int w, int h, int dx, int dy)
{
    VLC_UNUSED( h );
    *sx = w - 1 - dx;
    *sy = dy;
}
static void VFlip(int *sx, int *sy, int w, int h, int dx, int dy)
{
    VLC_UNUSED( w );
    *sx = dx;
    *sy = h - 1 - dy;
}
static void Transpose(int *sx, int *sy, int w, int h, int dx, int dy)
{
    VLC_UNUSED( h ); VLC_UNUSED( w );
    *sx = dy;
    *sy = dx;
}
static void AntiTranspose(int *sx, int *sy, int w, int h, int dx, int dy)
{
    *sx = h - 1 - dy;
    *sy = w - 1 - dx;
}
static void R90(int *sx, int *sy, int w, int h, int dx, int dy)
{
    VLC_UNUSED( h );
    *sx = dy;
    *sy = w - 1 - dx;
}
static void R180(int *sx, int *sy, int w, int h, int dx, int dy)
{
    *sx = w - 1 - dx;
    *sy = h - 1 - dy;
}
static void R270(int *sx, int *sy, int w, int h, int dx, int dy)
{
    VLC_UNUSED( w );
    *sx = h - 1 - dy;
    *sy = dx;
}
typedef void (*convert_t)(int *, int *, int, int, int, int);

#define PLANE(f,bits) \
static void Plane##bits##_##f(plane_t *restrict dst, const plane_t *restrict src) \
{ \
    const uint##bits##_t *src_pixels = (const void *)src->p_pixels; \
    uint##bits##_t *restrict dst_pixels = (void *)dst->p_pixels; \
    const unsigned src_width = src->i_pitch / sizeof (*src_pixels); \
    const unsigned dst_width = dst->i_pitch / sizeof (*dst_pixels); \
    const unsigned dst_visible_width = dst->i_visible_pitch / sizeof (*dst_pixels); \
 \
    for (int y = 0; y < dst->i_visible_lines; y++) { \
        for (unsigned x = 0; x < dst_visible_width; x++) { \
            int sx, sy; \
            (f)(&sx, &sy, dst_visible_width, dst->i_visible_lines, x, y); \
            dst_pixels[y * dst_width + x] = \
                src_pixels[sy * src_width + sx]; \
        } \
    } \
}

static void Plane_VFlip(plane_t *restrict dst, const plane_t *restrict src)
{
    const uint8_t *src_pixels = src->p_pixels;
    uint8_t *restrict dst_pixels = dst->p_pixels;

    dst_pixels += dst->i_pitch * dst->i_visible_lines;
    for (int y = 0; y < dst->i_visible_lines; y++) {
        dst_pixels -= dst->i_pitch;
        memcpy(dst_pixels, src_pixels, dst->i_visible_pitch);
        src_pixels += src->i_pitch;
    }
}

#define I422(f) \
static void Plane422_##f(plane_t *restrict dst, const plane_t *restrict src) \
{ \
    for (int y = 0; y < dst->i_visible_lines; y += 2) { \
        for (int x = 0; x < dst->i_visible_pitch; x++) { \
            int sx, sy, uv; \
            (f)(&sx, &sy, dst->i_visible_pitch, dst->i_visible_lines / 2, \
                x, y / 2); \
            uv = (1 + src->p_pixels[2 * sy * src->i_pitch + sx] + \
                src->p_pixels[(2 * sy + 1) * src->i_pitch + sx]) / 2; \
            dst->p_pixels[y * dst->i_pitch + x] = uv; \
            dst->p_pixels[(y + 1) * dst->i_pitch + x] = uv; \
        } \
    } \
}

#define YUY2(f) \
static void PlaneYUY2_##f(plane_t *restrict dst, const plane_t *restrict src) \
{ \
    unsigned dst_visible_width = dst->i_visible_pitch / 2; \
 \
    for (int y = 0; y < dst->i_visible_lines; y += 2) { \
        for (unsigned x = 0; x < dst_visible_width; x+= 2) { \
            int sx0, sy0, sx1, sy1; \
            (f)(&sx0, &sy0, dst_visible_width, dst->i_visible_lines, x, y); \
            (f)(&sx1, &sy1, dst_visible_width, dst->i_visible_lines, \
                x + 1, y + 1); \
            dst->p_pixels[(y + 0) * dst->i_pitch + 2 * (x + 0)] = \
                src->p_pixels[sy0 * src->i_pitch + 2 * sx0]; \
            dst->p_pixels[(y + 0) * dst->i_pitch + 2 * (x + 1)] = \
                src->p_pixels[sy1 * src->i_pitch + 2 * sx0]; \
            dst->p_pixels[(y + 1) * dst->i_pitch + 2 * (x + 0)] = \
                src->p_pixels[sy0 * src->i_pitch + 2 * sx1]; \
            dst->p_pixels[(y + 1) * dst->i_pitch + 2 * (x + 1)] = \
                src->p_pixels[sy1 * src->i_pitch + 2 * sx1]; \
 \
            int sx, sy, u, v; \
            (f)(&sx, &sy, dst_visible_width / 2, dst->i_visible_lines / 2, \
                x / 2, y / 2); \
            u = (1 + src->p_pixels[2 * sy * src->i_pitch + 4 * sx + 1] + \
                src->p_pixels[(2 * sy + 1) * src->i_pitch + 4 * sx + 1]) / 2; \
            v = (1 + src->p_pixels[2 * sy * src->i_pitch + 4 * sx + 3] + \
                src->p_pixels[(2 * sy + 1) * src->i_pitch + 4 * sx + 3]) / 2; \
            dst->p_pixels[(y + 0) * dst->i_pitch + 2 * x + 1] = u; \
            dst->p_pixels[(y + 0) * dst->i_pitch + 2 * x + 3] = v; \
            dst->p_pixels[(y + 1) * dst->i_pitch + 2 * x + 1] = u; \
            dst->p_pixels[(y + 1) * dst->i_pitch + 2 * x + 3] = v; \
        } \
    } \
}

#undef PLANES // already exists on Windows
#define PLANES(f) \
PLANE(f,8) PLANE(f,16) PLANE(f,32)

PLANES(HFlip)
#define Plane8_VFlip Plane_VFlip
#define Plane16_VFlip Plane_VFlip
#define Plane32_VFlip Plane_VFlip
PLANES(Transpose)
PLANES(AntiTranspose)
PLANES(R90)
PLANES(R180)
PLANES(R270)

#define Plane422_HFlip Plane16_HFlip
#define Plane422_VFlip Plane_VFlip
#define Plane422_R180  Plane16_R180
I422(Transpose)
I422(AntiTranspose)
I422(R90)
I422(R270)

#define PlaneYUY2_HFlip Plane32_HFlip
#define PlaneYUY2_VFlip Plane_VFlip
#define PlaneYUY2_R180  Plane32_R180
YUY2(Transpose)
YUY2(AntiTranspose)
YUY2(R90)
YUY2(R270)

typedef struct {
    convert_t convert;
    void      (*plane8) (plane_t *dst, const plane_t *src);
    void      (*plane16)(plane_t *dst, const plane_t *src);
    void      (*plane32)(plane_t *dst, const plane_t *src);
    void      (*i422)(plane_t *dst, const plane_t *src);
    void      (*yuyv)(plane_t *dst, const plane_t *src);
} transform_description_t;

#define DESC(f) \
    { f, Plane8_##f, Plane16_##f, Plane32_##f, \
      Plane422_##f, PlaneYUY2_##f }

static const transform_description_t descriptions[] = {
    [TRANSFORM_R90] =            DESC(R90),
    [TRANSFORM_R180] =           DESC(R180),
    [TRANSFORM_R270] =           DESC(R270),
    [TRANSFORM_HFLIP] =          DESC(HFlip),
    [TRANSFORM_VFLIP] =          DESC(VFlip),
    [TRANSFORM_TRANSPOSE] =      DESC(Transpose),
    [TRANSFORM_ANTI_TRANSPOSE] = DESC(AntiTranspose),
};

typedef struct
{
    const vlc_chroma_description_t *chroma;
    void (*plane[PICTURE_PLANE_MAX])(plane_t *, const plane_t *);
    convert_t convert;
} filter_sys_t;

static picture_t *Filter(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;
    picture_t *dst = filter_NewPicture(filter);

    if (likely(dst != NULL)) {
        const vlc_chroma_description_t *chroma = sys->chroma;

        for (unsigned i = 0; i < chroma->plane_count; i++)
            (sys->plane[i])(&dst->p[i], &src->p[i]);

        picture_CopyProperties(dst, src);
    }

    picture_Release(src);
    return dst;
}

static int Mouse(filter_t *filter, vlc_mouse_t *mouse,
                 const vlc_mouse_t *mold)
{
    VLC_UNUSED( mold );

    const video_format_t *fmt = &filter->fmt_out.video;
    const filter_sys_t   *sys = filter->p_sys;

    sys->convert(&mouse->i_x, &mouse->i_y,
                 fmt->i_visible_width, fmt->i_visible_height,
                 mouse->i_x, mouse->i_y);
    return VLC_SUCCESS;
}

static int Open(filter_t *filter)
{
    const video_format_t *src = &filter->fmt_in.video;
    video_format_t       *dst = &filter->fmt_out.video;
    video_transform_t transform = video_format_GetTransform(src->orientation,
                                                            dst->orientation);

    if (transform == TRANSFORM_IDENTITY)
        return VLC_ENOTSUP; /* Nothing for this module to work at */

    video_format_t src_trans = *src;
    video_format_TransformBy(&src_trans, transform);

    if (dst->i_chroma         != src_trans.i_chroma ||
        dst->i_width          != src_trans.i_width ||
        dst->i_visible_width  != src_trans.i_visible_width ||
        dst->i_height         != src_trans.i_height ||
        dst->i_visible_height != src_trans.i_visible_height ||
        dst->i_x_offset       != src_trans.i_x_offset ||
        dst->i_y_offset       != src_trans.i_y_offset)
        return VLC_ENOTSUP; /* This module cannot rescale */

    const vlc_chroma_description_t *chroma =
        vlc_fourcc_GetChromaDescription(src->i_chroma);
    if (chroma == NULL)
        return VLC_ENOTSUP;

    filter_sys_t *sys = vlc_obj_malloc(VLC_OBJECT(filter), sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    const transform_description_t *const dsc = &descriptions[transform];

    sys->chroma = chroma;

    switch (chroma->pixel_size) {
        case 1:
            sys->plane[0] = dsc->plane8;
            break;
        case 2:
            sys->plane[0] = dsc->plane16;
            break;
        case 4:
            sys->plane[0] = dsc->plane32;
            break;
        default:
            return VLC_ENOTSUP;
    }

    for (unsigned i = 1; i < PICTURE_PLANE_MAX; i++)
        sys->plane[i] = sys->plane[0];
    sys->convert = dsc->convert;

    if (ORIENT_IS_SWAP(transform)) {
        switch (src->i_chroma) {
            case VLC_CODEC_I422:
            case VLC_CODEC_J422:
                sys->plane[2] = sys->plane[1] = dsc->i422;
                break;
            default:
                for (unsigned i = 0; i < chroma->plane_count; i++)
                    if (chroma->p[i].w.num * chroma->p[i].h.den
                     != chroma->p[i].h.num * chroma->p[i].w.den)
                        return VLC_ENOTSUP;
        }
    }

    /* Deal with weird packed formats */
    switch (src->i_chroma) {
        case VLC_CODEC_UYVY:
        case VLC_CODEC_VYUY:
            if (ORIENT_IS_SWAP(transform))
                return VLC_ENOTSUP;
            /* fallthrough */
        case VLC_CODEC_YUYV:
        case VLC_CODEC_YVYU:
            sys->plane[0] = dsc->yuyv; /* 32-bits, not 16-bits! */
            break;
        case VLC_CODEC_NV12:
        case VLC_CODEC_NV21:
            return VLC_ENOTSUP;
    }

    static const struct vlc_filter_operations filter_ops =
    {
        .filter_video = Filter,
        .video_mouse = Mouse,
    };
    filter->ops = &filter_ops;
    filter->p_sys           = sys;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_description(N_("Video reorientation"))
    set_shortname(N_("Reorient"))
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callback_video_converter(Open, 200)
vlc_module_end()
