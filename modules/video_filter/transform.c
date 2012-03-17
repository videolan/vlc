/*****************************************************************************
 * transform.c : transform image module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif
#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define CFG_PREFIX "transform-"

#define TYPE_TEXT N_("Transform type")
static const char * const type_list[] = { "90", "180", "270", "hflip", "vflip" };
static const char * const type_list_text[] = { N_("Rotate by 90 degrees"),
  N_("Rotate by 180 degrees"), N_("Rotate by 270 degrees"),
  N_("Flip horizontally"), N_("Flip vertically") };

vlc_module_begin()
    set_description(N_("Video transformation filter"))
    set_shortname(N_("Transformation"))
    set_help(N_("Rotate or flip the video"))
    set_capability("video filter2", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)

    add_string(CFG_PREFIX "type", "90", TYPE_TEXT, TYPE_TEXT, false)
        change_string_list(type_list, type_list_text, 0)

    add_shortcut("transform")
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
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
static void R90(int *sx, int *sy, int w, int h, int dx, int dy)
{
    VLC_UNUSED( h );
    *sx = dy;
    *sy = w - 1 - dx;
}
static void R180(int *sx, int *sy, int w, int h, int dx, int dy)
{
    *sx = w - dx;
    *sy = h - dy;
}
static void R270(int *sx, int *sy, int w, int h, int dx, int dy)
{
    VLC_UNUSED( w );
    *sx = h - 1 - dy;
    *sy = dx;
}
typedef void (*convert_t)(int *, int *, int, int, int, int);

#define PLANAR(f) \
static void Plane8_##f(plane_t *restrict dst, const plane_t *restrict src) \
{ \
    for (int y = 0; y < dst->i_visible_lines; y++) { \
        for (int x = 0; x < dst->i_visible_pitch; x++) { \
            int sx, sy; \
            (f)(&sx, &sy, dst->i_visible_pitch, dst->i_visible_lines, x, y); \
            dst->p_pixels[y * dst->i_pitch + x] = \
                src->p_pixels[sy * src->i_pitch + sx]; \
        } \
    } \
} \
 \
static void Plane16_##f(plane_t *restrict dst, const plane_t *restrict src) \
{ \
    const uint16_t *src_pixels = (const uint16_t *)src->p_pixels; \
    uint16_t *restrict dst_pixels = (uint16_t *)dst->p_pixels; \
    unsigned src_pitch = src->i_pitch / 2; \
    unsigned dst_pitch = dst->i_pitch / 2; \
    unsigned dst_visible_width = dst->i_visible_pitch / 2; \
 \
    for (int y = 0; y < dst->i_visible_lines; y++) { \
        for (unsigned x = 0; x < dst_visible_width; x++) { \
            int sx, sy; \
            (f)(&sx, &sy, dst_visible_width, dst->i_visible_lines, x, y);\
            dst_pixels[y * dst_pitch + x] = \
                src_pixels[sy * src_pitch + sx]; \
        } \
    } \
} \
 \
static void Plane32_##f(plane_t *restrict dst, const plane_t *restrict src) \
{ \
    const uint32_t *src_pixels = (const uint32_t *)src->p_pixels; \
    uint32_t *restrict dst_pixels = (uint32_t *)dst->p_pixels; \
    unsigned src_pitch = src->i_pitch / 4; \
    unsigned dst_pitch = dst->i_pitch / 4; \
    unsigned dst_visible_width = dst->i_visible_pitch / 4; \
 \
    for (int y = 0; y < dst->i_visible_lines; y++) { \
        for (unsigned x = 0; x < dst_visible_width; x++) { \
            int sx, sy; \
            (f)(&sx, &sy, dst_visible_width, dst->i_visible_lines, x, y);\
            dst_pixels[y * dst_pitch + x] = \
                src_pixels[sy * src_pitch + sx]; \
        } \
    } \
}

PLANAR(HFlip)
PLANAR(VFlip)
PLANAR(R90)
PLANAR(R180)
PLANAR(R270)

typedef struct {
    char      name[8];
    bool      is_rotated;
    convert_t convert;
    convert_t iconvert;
    void      (*plane8) (plane_t *dst, const plane_t *src);
    void      (*plane16)(plane_t *dst, const plane_t *src);
    void      (*plane32)(plane_t *dst, const plane_t *src);
} transform_description_t;

#define DESC(str, rotated, f, invf) \
    { str, rotated, f, invf, Plane8_##f, Plane16_##f, Plane32_##f }

static const transform_description_t descriptions[] = {
    DESC("90",    true,  R90,   R270),
    DESC("180",   false, R180,  R180),
    DESC("270",   true,  R270,  R90),
    DESC("hflip", false, HFlip, HFlip),
    DESC("vflip", false, VFlip, VFlip),
};

static const size_t n_transforms =
    sizeof (descriptions) / sizeof (descriptions[0]);

struct filter_sys_t {
    const vlc_chroma_description_t *chroma;
    void (*plane)(plane_t *, const plane_t *);
    convert_t convert;
};

static picture_t *Filter(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;

    picture_t *dst = filter_NewPicture(filter);
    if (!dst) {
        picture_Release(src);
        return NULL;
    }

    const vlc_chroma_description_t *chroma = sys->chroma;
    for (unsigned i = 0; i < chroma->plane_count; i++)
         sys->plane(&dst->p[i], &src->p[i]);

    picture_CopyProperties(dst, src);
    picture_Release(src);
    return dst;
}

static int Mouse(filter_t *filter, vlc_mouse_t *mouse,
                 const vlc_mouse_t *mold, const vlc_mouse_t *mnew)
{
    VLC_UNUSED( mold );

    const video_format_t *fmt = &filter->fmt_out.video;
    const filter_sys_t   *sys = filter->p_sys;

    *mouse = *mnew;
    sys->convert(&mouse->i_x, &mouse->i_y,
                 fmt->i_visible_width, fmt->i_visible_height,
                 mouse->i_x, mouse->i_y);
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *object)
{
    filter_t *filter = (filter_t *)object;
    const video_format_t *src = &filter->fmt_in.video;
    video_format_t       *dst = &filter->fmt_out.video;

    const vlc_chroma_description_t *chroma =
        vlc_fourcc_GetChromaDescription(src->i_chroma);
    if (chroma == NULL)
        return VLC_EGENERIC;

    filter_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->chroma = chroma;

    char *type_name = var_InheritString(filter, CFG_PREFIX"type");
    const transform_description_t *dsc = NULL;

    for (size_t i = 0; i < n_transforms; i++)
        if (type_name && !strcmp(descriptions[i].name, type_name)) {
            dsc = &descriptions[i];
            break;
        }
    if (dsc == NULL) {
        dsc = &descriptions[0];
        msg_Warn(filter, "No valid transform mode provided, using '%s'",
                 dsc->name);
    }

    free(type_name);

    switch (chroma->pixel_size) {
        case 1:
            sys->plane = dsc->plane8;
            break;
        case 2:
            sys->plane = dsc->plane16;
            break;
        case 4:
            sys->plane = dsc->plane32;
            break;
        default:
            msg_Err(filter, "Unsupported pixel size %u (chroma %4.4s)",
                    chroma->pixel_size, (char *)&src->i_chroma);
            goto error;
    }

    sys->convert = dsc->convert;
    if (dsc->is_rotated) {
        for (unsigned i = 0; i < chroma->plane_count; i++) {
            if (chroma->p[i].w.num * chroma->p[i].h.den
             != chroma->p[i].h.num * chroma->p[i].w.den) {
                msg_Err(filter, "Format rotation not possible (chroma %4.4s)",
                        (char *)&src->i_chroma); /* I422... */
                goto error;
            }
        }

        if (!filter->b_allow_fmt_out_change) {
            msg_Err(filter, "Format change is not allowed");
            goto error;
        }

        dst->i_width          = src->i_height;
        dst->i_visible_width  = src->i_visible_height;
        dst->i_height         = src->i_width;
        dst->i_visible_height = src->i_visible_width;
        dst->i_sar_num        = src->i_sar_den;
        dst->i_sar_den        = src->i_sar_num;
    }

    /* Deal with weird packed formats */
    switch (src->i_chroma) {
        case VLC_CODEC_YUYV:
        case VLC_CODEC_UYVY:
        case VLC_CODEC_YVYU:
        case VLC_CODEC_VYUY:
            if (dsc->is_rotated) {
                msg_Err(filter, "Format rotation not possible (chroma %4.4s)",
                        (char *)&src->i_chroma);
                goto error;
            }
            sys->plane = dsc->plane32; /* 32-bits, not 16-bits! */
            break;
        case VLC_CODEC_NV12:
        case VLC_CODEC_NV21:
            goto error;
    }

    dst->i_x_offset       = INT_MAX;
    dst->i_y_offset       = INT_MAX;
    for (int i = 0; i < 2; i++) {
        int tx, ty;
        dsc->iconvert(&tx, &ty, src->i_width, src->i_height,
                      src->i_x_offset + i * (src->i_visible_width  - 1),
                      src->i_y_offset + i * (src->i_visible_height - 1));
        dst->i_x_offset = __MIN(dst->i_x_offset, (unsigned)(1 + tx));
        dst->i_y_offset = __MIN(dst->i_y_offset, (unsigned)(1 + ty));
    }

    filter->p_sys           = sys;
    filter->pf_video_filter = Filter;
    filter->pf_video_mouse  = Mouse;
    return VLC_SUCCESS;
error:
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *object)
{
    filter_t     *filter = (filter_t *)object;
    filter_sys_t *sys    = filter->p_sys;

    free(sys);
}

#if 0
static void FilterI422( vout_thread_t *p_vout,
                        const picture_t *p_pic, picture_t *p_outpic )
{
    int i_index;
    switch( p_vout->p_sys->i_mode )
    {
        case TRANSFORM_MODE_180:
        case TRANSFORM_MODE_HFLIP:
        case TRANSFORM_MODE_VFLIP:
            /* Fall back on the default implementation */
            FilterPlanar( p_vout, p_pic, p_outpic );
            return;

        case TRANSFORM_MODE_90:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                if( i_index == 0 )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        uint8_t *p_line_end;

                        p_out_end -= p_outpic->p[i_index].i_pitch
                                      - p_outpic->p[i_index].i_visible_pitch;
                        p_line_end = p_in + p_pic->p[i_index].i_visible_lines *
                            i_pitch;

                        for( ; p_in < p_line_end ; )
                        {
                            p_line_end -= i_pitch;
                            *(--p_out_end) = *p_line_end;
                        }

                        p_in++;
                    }
                }
                else /* i_index == 1 or 2 */
                {
                    for( ; p_out < p_out_end ; )
                    {
                        uint8_t *p_line_end, *p_out_end2;

                        p_out_end -= p_outpic->p[i_index].i_pitch
                                      - p_outpic->p[i_index].i_visible_pitch;
                        p_out_end2 = p_out_end - p_outpic->p[i_index].i_pitch;
                        p_line_end = p_in + p_pic->p[i_index].i_visible_lines *
                            i_pitch;

                        for( ; p_in < p_line_end ; )
                        {
                            uint8_t p1, p2;

                            p_line_end -= i_pitch;
                            p1 = *p_line_end;
                            p_line_end -= i_pitch;
                            p2 = *p_line_end;

                            /* Trick for (x+y)/2 without overflow, based on
                             *   x + y == (x ^ y) + 2 * (x & y) */
                            *(--p_out_end) = (p1 & p2) + ((p1 ^ p2) / 2);
                            *(--p_out_end2) = (p1 & p2) + ((p1 ^ p2) / 2);
                        }

                        p_out_end = p_out_end2;
                        p_in++;
                    }
                }
            }
            break;

        case TRANSFORM_MODE_270:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                if( i_index == 0 )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        uint8_t *p_in_end;

                        p_in_end = p_in + p_pic->p[i_index].i_visible_lines *
                            i_pitch;

                        for( ; p_in < p_in_end ; )
                        {
                            p_in_end -= i_pitch;
                            *p_out++ = *p_in_end;
                        }

                        p_out += p_outpic->p[i_index].i_pitch
                                  - p_outpic->p[i_index].i_visible_pitch;
                        p_in++;
                    }
                }
                else /* i_index == 1 or 2 */
                {
                    for( ; p_out < p_out_end ; )
                    {
                        uint8_t *p_in_end, *p_out2;

                        p_in_end = p_in + p_pic->p[i_index].i_visible_lines *
                            i_pitch;
                        p_out2 = p_out + p_outpic->p[i_index].i_pitch;

                        for( ; p_in < p_in_end ; )
                        {
                            uint8_t p1, p2;

                            p_in_end -= i_pitch;
                            p1 = *p_in_end;
                            p_in_end -= i_pitch;
                            p2 = *p_in_end;

                            /* Trick for (x+y)/2 without overflow, based on
                             *   x + y == (x ^ y) + 2 * (x & y) */
                            *p_out++ = (p1 & p2) + ((p1 ^ p2) / 2);
                            *p_out2++ = (p1 & p2) + ((p1 ^ p2) / 2);
                        }

                        p_out2 += p_outpic->p[i_index].i_pitch
                                   - p_outpic->p[i_index].i_visible_pitch;
                        p_out = p_out2;
                        p_in++;
                    }
                }
            }
            break;

        default:
            break;
    }
}

static void FilterYUYV( vout_thread_t *p_vout,
                        const picture_t *p_pic, picture_t *p_outpic )
{
    int i_index;
    int i_y_offset, i_u_offset, i_v_offset;
    if( GetPackedYuvOffsets( p_pic->format.i_chroma, &i_y_offset,
                             &i_u_offset, &i_v_offset ) != VLC_SUCCESS )
        return;

    switch( p_vout->p_sys->i_mode )
    {
        case TRANSFORM_MODE_VFLIP:
            /* Fall back on the default implementation */
            FilterPlanar( p_vout, p_pic, p_outpic );
            return;

        case TRANSFORM_MODE_90:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                int i_offset  = i_u_offset;
                int i_offset2 = i_v_offset;
                for( ; p_out < p_out_end ; )
                {
                    uint8_t *p_line_end;

                    p_out_end -= p_outpic->p[i_index].i_pitch
                                  - p_outpic->p[i_index].i_visible_pitch;
                    p_line_end = p_in + p_pic->p[i_index].i_visible_lines *
                        i_pitch;

                    for( ; p_in < p_line_end ; )
                    {
                        p_line_end -= i_pitch;
                        p_out_end -= 4;
                        p_out_end[i_y_offset+2] = p_line_end[i_y_offset];
                        p_out_end[i_u_offset] = p_line_end[i_offset];
                        p_line_end -= i_pitch;
                        p_out_end[i_y_offset] = p_line_end[i_y_offset];
                        p_out_end[i_v_offset] = p_line_end[i_offset2];
                    }

                    p_in += 2;

                    {
                        int a = i_offset;
                        i_offset = i_offset2;
                        i_offset2 = a;
                    }
                }
            }
            break;

        case TRANSFORM_MODE_180:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                uint8_t *p_in = p_pic->p[i_index].p_pixels;
                uint8_t *p_in_end = p_in + p_pic->p[i_index].i_visible_lines
                                            * p_pic->p[i_index].i_pitch;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;

                for( ; p_in < p_in_end ; )
                {
                    uint8_t *p_line_start = p_in_end
                                             - p_pic->p[i_index].i_pitch;
                    p_in_end -= p_pic->p[i_index].i_pitch
                                 - p_pic->p[i_index].i_visible_pitch;

                    for( ; p_line_start < p_in_end ; )
                    {
                        p_in_end -= 4;
                        p_out[i_y_offset] = p_in_end[i_y_offset+2];
                        p_out[i_u_offset] = p_in_end[i_u_offset];
                        p_out[i_y_offset+2] = p_in_end[i_y_offset];
                        p_out[i_v_offset] = p_in_end[i_v_offset];
                        p_out += 4;
                    }

                    p_out += p_outpic->p[i_index].i_pitch
                              - p_outpic->p[i_index].i_visible_pitch;
                }
            }
            break;

        case TRANSFORM_MODE_270:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                int i_offset  = i_u_offset;
                int i_offset2 = i_v_offset;
                for( ; p_out < p_out_end ; )
                {
                    uint8_t *p_in_end;

                    p_in_end = p_in
                             + p_pic->p[i_index].i_visible_lines * i_pitch;

                    for( ; p_in < p_in_end ; )
                    {
                        p_in_end -= i_pitch;
                        p_out[i_y_offset] = p_in_end[i_y_offset];
                        p_out[i_u_offset] = p_in_end[i_offset];
                        p_in_end -= i_pitch;
                        p_out[i_y_offset+2] = p_in_end[i_y_offset];
                        p_out[i_v_offset] = p_in_end[i_offset2];
                        p_out += 4;
                    }

                    p_out += p_outpic->p[i_index].i_pitch
                           - p_outpic->p[i_index].i_visible_pitch;
                    p_in += 2;

                    {
                        int a = i_offset;
                        i_offset = i_offset2;
                        i_offset2 = a;
                    }
                }
            }
            break;

        case TRANSFORM_MODE_HFLIP:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                uint8_t *p_in = p_pic->p[i_index].p_pixels;
                uint8_t *p_in_end = p_in + p_pic->p[i_index].i_visible_lines
                                         * p_pic->p[i_index].i_pitch;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;

                for( ; p_in < p_in_end ; )
                {
                    uint8_t *p_line_end = p_in
                                        + p_pic->p[i_index].i_visible_pitch;

                    for( ; p_in < p_line_end ; )
                    {
                        p_line_end -= 4;
                        p_out[i_y_offset] = p_line_end[i_y_offset+2];
                        p_out[i_u_offset] = p_line_end[i_u_offset];
                        p_out[i_y_offset+2] = p_line_end[i_y_offset];
                        p_out[i_v_offset] = p_line_end[i_v_offset];
                        p_out += 4;
                    }

                    p_in += p_pic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch
                                - p_outpic->p[i_index].i_visible_pitch;
                }
            }
            break;

        default:
            break;
    }
}
#endif
