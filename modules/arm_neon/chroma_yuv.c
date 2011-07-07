/*****************************************************************************
 * i420_yuy2.c : ARM NEONv1 YUV 4:2:0 to YUV :2:2 chroma conversion for VLC
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_cpu.h>
#include "chroma_neon.h"

static int Open (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("ARM NEON video chroma conversions"))
    set_capability ("video filter2", 250)
    set_callbacks (Open, NULL)
vlc_module_end ()

#define DEFINE_PACK(pack, pict) \
    struct yuv_pack pack = { (pict)->Y_PIXELS, (pict)->Y_PITCH }
#define DEFINE_PLANES(planes, pict) \
    struct yuv_planes planes = { \
        (pict)->Y_PIXELS, (pict)->U_PIXELS, (pict)->V_PIXELS, (pict)->Y_PITCH }
#define DEFINE_PLANES_SWAP(planes, pict) \
    struct yuv_planes planes = { \
        (pict)->Y_PIXELS, (pict)->V_PIXELS, (pict)->U_PIXELS, (pict)->Y_PITCH }

static void I420_YUYV (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES(in, src);
    i420_yuyv_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I420_YUYV)

static void YV12_YUYV (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES_SWAP(in, src);
    i420_yuyv_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (YV12_YUYV)

static void I420_UYVY (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES(in, src);
    i420_uyvy_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I420_UYVY)

static void YV12_UYVY (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES_SWAP(in, src);
    i420_uyvy_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (YV12_UYVY)

static int Open (vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if (!(vlc_CPU() & CPU_CAPABILITY_NEON))
        return VLC_EGENERIC;
    if (((filter->fmt_in.video.i_width | filter->fmt_in.video.i_height) & 1)
     || (filter->fmt_in.video.i_width != filter->fmt_out.video.i_width)
     || (filter->fmt_in.video.i_height != filter->fmt_out.video.i_height))
        return VLC_EGENERIC;

    switch (filter->fmt_in.video.i_chroma)
    {
        case VLC_CODEC_I420:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_YUYV:
                    filter->pf_video_filter = I420_YUYV_Filter;
                    break;
                case VLC_CODEC_UYVY:
                    filter->pf_video_filter = I420_UYVY_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        case VLC_CODEC_YV12:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_YUYV:
                    filter->pf_video_filter = YV12_YUYV_Filter;
                    break;
                case VLC_CODEC_UYVY:
                    filter->pf_video_filter = YV12_UYVY_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
