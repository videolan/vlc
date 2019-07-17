/*****************************************************************************
 * chroma_yuv.c : ARM NEONv1 YUV 4:2:0 to YUV 4:2:2 chroma conversion for VLC
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_cpu.h>
#include "arm_neon/chroma_neon.h"

static int Open (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("ARM NEON video chroma conversions"))
    set_capability ("video converter", 250)
    set_callback(Open)
vlc_module_end ()

#define DEFINE_PACK(pack, pict) \
    struct yuv_pack pack = { (pict)->Y_PIXELS, (pict)->Y_PITCH }
#define DEFINE_PLANES(planes, pict) \
    struct yuv_planes planes = { \
        (pict)->Y_PIXELS, (pict)->U_PIXELS, (pict)->V_PIXELS, (pict)->Y_PITCH }
#define DEFINE_PLANES_SWAP(planes, pict) \
    struct yuv_planes planes = { \
        (pict)->Y_PIXELS, (pict)->V_PIXELS, (pict)->U_PIXELS, (pict)->Y_PITCH }

#define DEFINE_UV_PLANES(planes, pict) \
    struct uv_planes planes = { \
        (pict)->U_PIXELS, (pict)->V_PIXELS, (pict)->U_PITCH }
#define DEFINE_UV_PLANES_SWAP(planes, pict) \
    struct uv_planes planes = { \
        (pict)->V_PIXELS, (pict)->U_PIXELS, (pict)->U_PITCH }
#define DEFINE_UV_PACK(pack, pict) \
    struct yuv_pack pack = { (pict)->U_PIXELS, (pict)->U_PITCH }

/* Planar YUV420 to packed YUV422 */
static void I420_YUYV (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES(in, src);
    i420_yuyv_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I420_YUYV)

static void I420_YVYU (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES_SWAP(in, src);
    i420_yuyv_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I420_YVYU)

static void I420_UYVY (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES(in, src);
    i420_uyvy_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I420_UYVY)

static void I420_VYUY (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES_SWAP(in, src);
    i420_uyvy_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I420_VYUY)


/* Semiplanar NV12/21/16/24 to planar I420/YV12/I422/I444 */
static void copy_y_plane(filter_t *filter, picture_t *src, picture_t *dst)
{
    uint8_t *src_y = src->Y_PIXELS;
    uint8_t *dst_y = dst->Y_PIXELS;
    if (src->Y_PITCH == dst->Y_PITCH) {
        memcpy(dst_y, src_y, dst->Y_PITCH * filter->fmt_in.video.i_height);
    } else {
        for (unsigned y = 0; y < filter->fmt_in.video.i_height;
                y++, dst_y += dst->Y_PITCH, src_y += src->Y_PITCH)
            memcpy(dst_y, src_y, filter->fmt_in.video.i_width);
    }
}

#define SEMIPLANAR_FILTERS(name, h_subsamp, v_subsamp)                    \
static void name (filter_t *filter, picture_t *src,                       \
                  picture_t *dst)                                         \
{                                                                         \
    DEFINE_UV_PLANES(out, dst);                                           \
    DEFINE_UV_PACK(in, src);                                              \
    copy_y_plane (filter, src, dst);                                      \
    deinterleave_chroma_neon (&out, &in,                                  \
                              filter->fmt_in.video.i_width  / h_subsamp,  \
                              filter->fmt_in.video.i_height / v_subsamp); \
}                                                                         \
VIDEO_FILTER_WRAPPER (name)                                               \

#define SEMIPLANAR_FILTERS_SWAP(name, h_subsamp, v_subsamp)               \
static void name (filter_t *filter, picture_t *src,                       \
                  picture_t *dst)                                         \
{                                                                         \
    DEFINE_UV_PLANES_SWAP(out, dst);                                      \
    DEFINE_UV_PACK(in, src);                                              \
    copy_y_plane (filter, src, dst);                                      \
    deinterleave_chroma_neon (&out, &in,                                  \
                              filter->fmt_in.video.i_width  / h_subsamp,  \
                              filter->fmt_in.video.i_height / v_subsamp); \
}                                                                         \
VIDEO_FILTER_WRAPPER (name)                                               \

SEMIPLANAR_FILTERS (Semiplanar_Planar_420, 2, 2)
SEMIPLANAR_FILTERS_SWAP (Semiplanar_Planar_420_Swap, 2, 2)
SEMIPLANAR_FILTERS (Semiplanar_Planar_422, 2, 1)
SEMIPLANAR_FILTERS (Semiplanar_Planar_444, 1, 1)


/* Planar YUV422 to packed YUV422 */
static void I422_YUYV (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES(in, src);
    i422_yuyv_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I422_YUYV)

static void I422_YVYU (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES_SWAP(in, src);
    i422_yuyv_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I422_YVYU)

static void I422_UYVY (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES(in, src);
    i422_uyvy_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I422_UYVY)

static void I422_VYUY (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PACK(out, dst);
    DEFINE_PLANES_SWAP(in, src);
    i422_uyvy_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (I422_VYUY)


/* Packed YUV422 to planar YUV422 */
static void YUYV_I422 (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PLANES(out, dst);
    DEFINE_PACK(in, src);
    yuyv_i422_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (YUYV_I422)

static void YVYU_I422 (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PLANES_SWAP(out, dst);
    DEFINE_PACK(in, src);
    yuyv_i422_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (YVYU_I422)

static void UYVY_I422 (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PLANES(out, dst);
    DEFINE_PACK(in, src);
    uyvy_i422_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (UYVY_I422)

static void VYUY_I422 (filter_t *filter, picture_t *src, picture_t *dst)
{
    DEFINE_PLANES_SWAP(out, dst);
    DEFINE_PACK(in, src);
    uyvy_i422_neon (&out, &in, filter->fmt_in.video.i_width,
                    filter->fmt_in.video.i_height);
}
VIDEO_FILTER_WRAPPER (VYUY_I422)

static int Open (vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if (!vlc_CPU_ARM_NEON())
        return VLC_EGENERIC;
    if ((filter->fmt_in.video.i_width != filter->fmt_out.video.i_width)
     || (filter->fmt_in.video.i_height != filter->fmt_out.video.i_height))
        return VLC_EGENERIC;

    switch (filter->fmt_in.video.i_chroma)
    {
        /* Planar to packed */
        case VLC_CODEC_I420:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_YUYV:
                    filter->pf_video_filter = I420_YUYV_Filter;
                    break;
                case VLC_CODEC_UYVY:
                    filter->pf_video_filter = I420_UYVY_Filter;
                    break;
                case VLC_CODEC_YVYU:
                    filter->pf_video_filter = I420_YVYU_Filter;
                    break;
                case VLC_CODEC_VYUY:
                    filter->pf_video_filter = I420_VYUY_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        case VLC_CODEC_YV12:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_YUYV:
                    filter->pf_video_filter = I420_YVYU_Filter;
                    break;
                case VLC_CODEC_UYVY:
                    filter->pf_video_filter = I420_VYUY_Filter;
                    break;
                case VLC_CODEC_YVYU:
                    filter->pf_video_filter = I420_YUYV_Filter;
                    break;
                case VLC_CODEC_VYUY:
                    filter->pf_video_filter = I420_UYVY_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        case VLC_CODEC_I422:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_YUYV:
                    filter->pf_video_filter = I422_YUYV_Filter;
                    break;
                case VLC_CODEC_UYVY:
                    filter->pf_video_filter = I422_UYVY_Filter;
                    break;
                case VLC_CODEC_YVYU:
                    filter->pf_video_filter = I422_YVYU_Filter;
                    break;
                case VLC_CODEC_VYUY:
                    filter->pf_video_filter = I422_VYUY_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        /* Semiplanar to planar */
        case VLC_CODEC_NV12:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I420:
                    filter->pf_video_filter = Semiplanar_Planar_420_Filter;
                    break;
                case VLC_CODEC_YV12:
                    filter->pf_video_filter = Semiplanar_Planar_420_Swap_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        case VLC_CODEC_NV21:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I420:
                    filter->pf_video_filter = Semiplanar_Planar_420_Swap_Filter;
                    break;
                case VLC_CODEC_YV12:
                    filter->pf_video_filter = Semiplanar_Planar_420_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        case VLC_CODEC_NV16:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I422:
                    filter->pf_video_filter = Semiplanar_Planar_422_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        case VLC_CODEC_NV24:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I444:
                    filter->pf_video_filter = Semiplanar_Planar_444_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        /* Packed to planar */
        case VLC_CODEC_YUYV:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I422:
                    filter->pf_video_filter = YUYV_I422_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }

        case VLC_CODEC_UYVY:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I422:
                    filter->pf_video_filter = UYVY_I422_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }

        case VLC_CODEC_YVYU:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I422:
                    filter->pf_video_filter = YVYU_I422_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }


        case VLC_CODEC_VYUY:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I422:
                    filter->pf_video_filter = VYUY_I422_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
