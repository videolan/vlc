/*****************************************************************************
 * yuv_rgb.c : ARM NEONv1 YUV to RGB32 chroma conversion for VLC
 *****************************************************************************
 * Copyright (C) 2011 Sébastien Toque
 *                    Rémi Denis-Courmont
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
#include <vlc_cpu.h>
#include "chroma_neon.h"

static int Open (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("ARM NEON video chroma YUV->RGBA"))
    set_capability ("video filter2", 250)
    set_callbacks (Open, NULL)
vlc_module_end ()

/*
static int CoefY[256];
static int CoefRV[256];
static int CoefGU[256];
static int CoefGV[256];
static int CoefBU[256];

// C reference version of the converter
static void I420_RGBA_C (filter_t *filter, picture_t *src, picture_t *dst)
{
    const uint8_t *const out = dst->p->p_pixels;
    const size_t width = src->p[Y_PLANE].i_visible_pitch;
    const size_t height = src->p[Y_PLANE].i_visible_lines;

    const int ypitch = src->p[Y_PLANE].i_pitch;
    const int uvpitch = src->p[U_PLANE].i_pitch;
    const int dpitch = dst->p->i_pitch / dst->p->i_pixel_pitch;

    for (size_t j = 0; j <  height; ++j)
    {
        const int y = j * ypitch;
        const int u = (j>>1) * uvpitch;
        const int d = j * dpitch;

        for (size_t i = 0; i < width; ++i)
        {
            uint8_t Y = src->Y_PIXELS[y + i];
            uint8_t U = src->U_PIXELS[u + (i>>1)];
            uint8_t V = src->V_PIXELS[u + (i>>1)];

            //coef = float * Precision + .5 (Precision=32768)
            int R = CoefY[Y] + CoefRV[V];
            int G = CoefY[Y] + CoefGU[U] + CoefGV[V];
            int B = CoefY[Y] + CoefBU[U];

            //rgb = (rgb+Precision/2) / Precision (Precision=32768)
            R = R >> 15;
            G = G >> 15;
            B = B >> 15;

            if (unlikely(R < 0)) R = 0;
            if (unlikely(G < 0)) G = 0;
            if (unlikely(B < 0)) B = 0;
            if (unlikely(R > 255)) R = 255;
            if (unlikely(G > 255)) G = 255;
            if (unlikely(B > 255)) B = 255;

            ((uint32_t*)out)[d + i] = R | (G<<8) | (B<<16) | (0xff<<24);
        }
    }
}*/

static void I420_RGBA (filter_t *filter, picture_t *src, picture_t *dst)
{
    struct yuv_pack out = { dst->p->p_pixels, dst->p->i_pitch };
    struct yuv_planes in = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS, src->Y_PITCH };
    i420_rgb_neon (&out, &in, filter->fmt_in.video.i_width, filter->fmt_in.video.i_height);
}

static void I420_RV16 (filter_t *filter, picture_t *src, picture_t *dst)
{
    struct yuv_pack out = { dst->p->p_pixels, dst->p->i_pitch };
    struct yuv_planes in = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS, src->Y_PITCH };
    i420_rv16_neon (&out, &in, filter->fmt_in.video.i_width, filter->fmt_in.video.i_height);
}

static void YV12_RGBA (filter_t *filter, picture_t *src, picture_t *dst)
{
    struct yuv_pack out = { dst->p->p_pixels, dst->p->i_pitch };
    struct yuv_planes in = { src->Y_PIXELS, src->V_PIXELS, src->U_PIXELS, src->Y_PITCH };
    i420_rgb_neon (&out, &in, filter->fmt_in.video.i_width, filter->fmt_in.video.i_height);
}

static void NV21_RGBA (filter_t *filter, picture_t *src, picture_t *dst)
{
    struct yuv_pack out = { dst->p->p_pixels, dst->p->i_pitch };
    struct yuv_planes in = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS, src->Y_PITCH };
    nv21_rgb_neon (&out, &in, filter->fmt_in.video.i_width, filter->fmt_in.video.i_height);
}

static void NV12_RGBA (filter_t *filter, picture_t *src, picture_t *dst)
{
    struct yuv_pack out = { dst->p->p_pixels, dst->p->i_pitch };
    struct yuv_planes in = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS, src->Y_PITCH };
    nv12_rgb_neon (&out, &in, filter->fmt_in.video.i_width, filter->fmt_in.video.i_height);
}

VIDEO_FILTER_WRAPPER (I420_RGBA)
VIDEO_FILTER_WRAPPER (I420_RV16)
VIDEO_FILTER_WRAPPER (YV12_RGBA)
VIDEO_FILTER_WRAPPER (NV21_RGBA)
VIDEO_FILTER_WRAPPER (NV12_RGBA)

static int Open (vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if (!vlc_CPU_ARM_NEON())
        return VLC_EGENERIC;

    if (((filter->fmt_in.video.i_width | filter->fmt_in.video.i_height) & 1)
     || (filter->fmt_in.video.i_width != filter->fmt_out.video.i_width)
     || (filter->fmt_in.video.i_height != filter->fmt_out.video.i_height))
        return VLC_EGENERIC;

    switch (filter->fmt_out.video.i_chroma)
    {
        case VLC_CODEC_RGB16:
            switch (filter->fmt_in.video.i_chroma)
            {
                case VLC_CODEC_I420:
                    filter->pf_video_filter = I420_RV16_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        case VLC_CODEC_RGB32:
            if(        filter->fmt_out.video.i_rmask != 0x000000ff
                    || filter->fmt_out.video.i_gmask != 0x0000ff00
                    || filter->fmt_out.video.i_bmask != 0x00ff0000 )
                return VLC_EGENERIC;

            switch (filter->fmt_in.video.i_chroma)
            {
                case VLC_CODEC_I420:
                    filter->pf_video_filter = I420_RGBA_Filter;
                    break;
                case VLC_CODEC_YV12:
                    filter->pf_video_filter = YV12_RGBA_Filter;
                    break;
                case VLC_CODEC_NV21:
                    filter->pf_video_filter = NV21_RGBA_Filter;
                    break;
                case VLC_CODEC_NV12:
                    filter->pf_video_filter = NV12_RGBA_Filter;
                    break;
                default:
                    return VLC_EGENERIC;
            }
            break;

        default:
            return VLC_EGENERIC;
    }

    //precompute some values for the C version
    /*const int coefY  = (int)(1.164 * 32768 + 0.5);
    const int coefRV = (int)(1.793 * 32768 + 0.5);
    const int coefGU = (int)(0.213 * 32768 + 0.5);
    const int coefGV = (int)(0.533 * 32768 + 0.5);
    const int coefBU = (int)(2.113 * 32768 + 0.5);
    for (int i=0; i<256; ++i)
    {
        CoefY[i] = coefY * (i-16) + 16384;
        CoefRV[i] = coefRV*(i-128);
        CoefGU[i] = -coefGU*(i-128);
        CoefGV[i] = -coefGV*(i-128);
        CoefBU[i] = coefBU*(i-128);
    }*/

    msg_Dbg(filter, "%4.4s(%dx%d) to %4.4s(%dx%d)",
            (char*)&filter->fmt_in.video.i_chroma, filter->fmt_in.video.i_width, filter->fmt_in.video.i_height,
            (char*)&filter->fmt_out.video.i_chroma, filter->fmt_out.video.i_width, filter->fmt_out.video.i_height);

    return VLC_SUCCESS;
}
