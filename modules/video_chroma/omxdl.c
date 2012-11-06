/*****************************************************************************
 * omxdl.c : OpenMAX DL chroma conversions for VLC
 *****************************************************************************
 * Copyright (C) 2011 Rémi Denis-Courmont
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
#include <omxtypes.h>
#include <omxIP.h>

static int Open (vlc_object_t *);
static int OpenScaler (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("OpenMAX DL image processing"))
    set_capability ("video filter2", 90)
    set_callbacks (Open, NULL)
vlc_module_end ()

#define SRC_WIDTH  (filter->fmt_in.video.i_width)
#define SRC_HEIGHT (filter->fmt_in.video.i_height)
#define DST_WIDTH  (filter->fmt_out.video.i_width)
#define DST_HEIGHT (filter->fmt_out.video.i_height)

/*** Conversions from I420 ***/
static void I420_RV16 (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->U_PITCH, src->V_PITCH };
    OMX_U16 *out = (void *)dst->p->p_pixels;
    OMX_INT outstep = dst->p->i_pitch;
    OMXSize size = { SRC_WIDTH, SRC_HEIGHT };

    omxIPCS_YCbCr420ToBGR565_U8_U16_P3C3R (in, instep, out, outstep, size);
}
VIDEO_FILTER_WRAPPER (I420_RV16)

/*** Conversions from YV12 ***/
static void YV12_RV16 (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->V_PIXELS, src->U_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->V_PITCH, src->U_PITCH };
    OMX_U16 *out = (void *)dst->p->p_pixels;
    OMX_INT outstep = dst->p->i_pitch;
    OMXSize size = { SRC_WIDTH, SRC_HEIGHT };

    omxIPCS_YCbCr420ToBGR565_U8_U16_P3C3R (in, instep, out, outstep, size);
}
VIDEO_FILTER_WRAPPER (YV12_RV16)

/*** Conversions from I422 ***/
static void I422_I420 (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->U_PITCH, src->V_PITCH };
    OMX_U8 *out[3] = { dst->Y_PIXELS, dst->U_PIXELS, dst->V_PIXELS };
    OMX_INT outstep[3] = { dst->Y_PITCH, dst->U_PITCH, dst->V_PITCH };
    OMXSize size = { SRC_WIDTH, SRC_HEIGHT };

    omxIPCS_YCbCr422ToYCbCr420Rotate_U8_P3R (
        in, instep, out, outstep, size, OMX_IP_DISABLE);
}
VIDEO_FILTER_WRAPPER (I422_I420)

static void I422_YV12 (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->U_PITCH, src->V_PITCH };
    OMX_U8 *out[3] = { dst->Y_PIXELS, dst->V_PIXELS, dst->U_PIXELS };
    OMX_INT outstep[3] = { dst->Y_PITCH, dst->V_PITCH, dst->U_PITCH };
    OMXSize size = { SRC_WIDTH, SRC_HEIGHT };

    omxIPCS_YCbCr422ToYCbCr420Rotate_U8_P3R (
        in, instep, out, outstep, size, OMX_IP_DISABLE);
}
VIDEO_FILTER_WRAPPER (I422_YV12)

/*** Conversions from I444 ***/
static void I444_RV16 (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS };
    OMX_INT instep = src->p->i_pitch;
    OMX_U16 *out = (void *)dst->p->p_pixels;
    OMX_INT outstep = dst->p->i_pitch;
    OMXSize size = {
        filter->fmt_in.video.i_width,
        filter->fmt_in.video.i_height,
    };

    omxIPCS_YCbCr444ToBGR565_U8_U16_P3C3R (in, instep, out, outstep, size);
}
VIDEO_FILTER_WRAPPER (I444_RV16)

/*** Conversions from YUY2 ***/
static void YUYV_RV24 (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in = src->p->p_pixels;
    OMX_INT instep = src->p->i_pitch;
    OMX_U8 *out = dst->p->p_pixels;
    OMX_INT outstep = dst->p->i_pitch;
    OMXSize size = { SRC_WIDTH, SRC_HEIGHT };

    omxIPCS_YCbYCr422ToBGR888_U8_C2C3R (in, instep, out, outstep, size);
}
VIDEO_FILTER_WRAPPER (YUYV_RV24)

static void YUYV_RV16 (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in = src->p->p_pixels;
    OMX_INT instep = src->p->i_pitch;
    OMX_U16 *out = (void *)dst->p->p_pixels;
    OMX_INT outstep = dst->p->i_pitch;
    OMXSize size = { SRC_WIDTH, SRC_HEIGHT };

    omxIPCS_YCbYCr422ToBGR565_U8_U16_C2C3R (in, instep, out, outstep, size);
}
VIDEO_FILTER_WRAPPER (YUYV_RV16)

/*** Conversions from UYVY ***/
static void UYVY_I420 (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in = src->p->p_pixels;
    OMX_INT instep = src->p->i_pitch;
    OMX_U8 *out[3] = { dst->Y_PIXELS, dst->U_PIXELS, dst->V_PIXELS };
    OMX_INT outstep[3] = { dst->Y_PITCH, dst->U_PITCH, dst->V_PITCH };
    OMXSize size = { SRC_WIDTH, SRC_HEIGHT };

    omxIPCS_CbYCrY422ToYCbCr420Rotate_U8_C2P3R (
        in, instep, out, outstep, size, OMX_IP_DISABLE);
}
VIDEO_FILTER_WRAPPER (UYVY_I420)

static void UYVY_YV12 (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in = src->p->p_pixels;
    OMX_INT instep = src->p->i_pitch;
    OMX_U8 *out[3] = { dst->Y_PIXELS, dst->V_PIXELS, dst->U_PIXELS };
    OMX_INT outstep[3] = { dst->Y_PITCH, dst->V_PITCH, dst->U_PITCH };
    OMXSize size = { SRC_WIDTH, SRC_HEIGHT };

    omxIPCS_CbYCrY422ToYCbCr420Rotate_U8_C2P3R (
        in, instep, out, outstep, size, OMX_IP_DISABLE);
}
VIDEO_FILTER_WRAPPER (UYVY_YV12)

/*** Helpers ***/
static int FixRV24 (video_format_t *fmt)
{
#ifndef WORDS_BIGENDIAN
    if (fmt->i_rmask == 0 && fmt->i_gmask == 0 && fmt->i_bmask == 0)
    {
        fmt->i_rmask = 0xff0000;
        fmt->i_gmask = 0x00ff00;
        fmt->i_bmask = 0x0000ff;
    }
    return (fmt->i_rmask == 0xff0000 && fmt->i_gmask == 0x00ff00
         && fmt->i_bmask == 0x0000ff) ? 0 : -1;
#else
    if (fmt->i_rmask == 0 && fmt->i_gmask == 0 && fmt->i_bmask == 0)
    {
        fmt->i_rmask = 0x0000ff;
        fmt->i_gmask = 0x00ff00;
        fmt->i_bmask = 0xff0000;
    }
    return (fmt->i_rmask == 0x0000ff && fmt->i_gmask == 0x00ff00
         && fmt->i_bmask == 0xff0000) ? 0 : -1;
#endif
}

static int FixRV16 (video_format_t *fmt)
{
#ifndef WORDS_BIGENDIAN
    if (fmt->i_rmask == 0 && fmt->i_gmask == 0 && fmt->i_bmask == 0)
    {
        fmt->i_rmask = 0xf800;
        fmt->i_gmask = 0x07e0;
        fmt->i_bmask = 0x001f;
    }
    return (fmt->i_rmask == 0xf800 && fmt->i_gmask == 0x07e0
         && fmt->i_bmask == 0x001f) ? 0 : -1;
#else
    (void) fmt;
    return -1;
#endif
}

static int FixRV15 (video_format_t *fmt)
{
#ifndef WORDS_BIGENDIAN
    if (fmt->i_rmask == 0 && fmt->i_gmask == 0 && fmt->i_bmask == 0)
    {
        fmt->i_rmask = 0x7c00;
        fmt->i_gmask = 0x03e0;
        fmt->i_bmask = 0x001f;
    }
    return (fmt->i_rmask == 0x7c00 && fmt->i_gmask == 0x03e0
         && fmt->i_bmask == 0x001f) ? 0 : -1;
#else
    (void) fmt;
    return -1;
#endif
}

static int FixRV12 (video_format_t *fmt)
{
#ifndef WORDS_BIGENDIAN
    if (fmt->i_rmask == 0 && fmt->i_gmask == 0 && fmt->i_bmask == 0)
    {
        fmt->i_rmask = 0x0f00;
        fmt->i_gmask = 0x00f0;
        fmt->i_bmask = 0x000f;
    }
    return (fmt->i_rmask == 0x0f00 && fmt->i_gmask == 0x00f0
         && fmt->i_bmask == 0x000f) ? 0 : -1;
#else
    if (fmt->i_rmask == 0 && fmt->i_gmask == 0 && fmt->i_bmask == 0)
    {
        fmt->i_rmask = 0x000f;
        fmt->i_gmask = 0xf000;
        fmt->i_bmask = 0x0f00;
    }
    return (fmt->i_rmask == 0x000f && fmt->i_gmask == 0xf000
         && fmt->i_bmask == 0x0f00) ? 0 : -1;
#endif
}

/*** Initialization ***/
static int Open (vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if ((filter->fmt_in.video.i_width != filter->fmt_out.video.i_width)
     || (filter->fmt_in.video.i_height != filter->fmt_out.video.i_height))
        return OpenScaler (obj);

    switch (filter->fmt_in.video.i_chroma)
    {
        case VLC_CODEC_I420:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_RGB16:
                    if (FixRV16 (&filter->fmt_out.video))
                        return VLC_EGENERIC;
                    filter->pf_video_filter = I420_RV16_Filter;
                    return VLC_SUCCESS;
            }
            break;

        case VLC_CODEC_YV12:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_RGB16:
                    if (FixRV16 (&filter->fmt_out.video))
                        return VLC_EGENERIC;
                    filter->pf_video_filter = YV12_RV16_Filter;
                    return VLC_SUCCESS;
            }
            break;

        case VLC_CODEC_I422:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I420:
                    filter->pf_video_filter = I422_I420_Filter;
                    return VLC_SUCCESS;
                case VLC_CODEC_YV12:
                    filter->pf_video_filter = I422_YV12_Filter;
                    return VLC_SUCCESS;
            }
            break;

        case VLC_CODEC_I444:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_RGB16:
                    if (FixRV16 (&filter->fmt_out.video))
                        return VLC_EGENERIC;
                    filter->pf_video_filter = I444_RV16_Filter;
                    return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case VLC_CODEC_YUYV:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_RGB24:
                    if (FixRV24 (&filter->fmt_out.video))
                        return VLC_EGENERIC;
                    filter->pf_video_filter = YUYV_RV24_Filter;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB16:
                    if (FixRV16 (&filter->fmt_out.video))
                        return VLC_EGENERIC;
                    filter->pf_video_filter = YUYV_RV16_Filter;
                    return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case VLC_CODEC_UYVY:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I420:
                    filter->pf_video_filter = UYVY_I420_Filter;
                    return VLC_SUCCESS;
                case VLC_CODEC_YV12:
                    filter->pf_video_filter = UYVY_YV12_Filter;
                    return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
    }
    /* fallback to scaler (conversion with 1:1 scale) */
    return OpenScaler (obj);
}

/* TODO: configurable interpolation */

#define XRR_MAX \
    (OMX_INT)(((float)((SRC_WIDTH  & ~1) - 1)) / ((DST_WIDTH  & ~1) - 1) * (1 << 16) + .5)
#define YRR_MAX \
  (OMX_INT)(((float)((SRC_HEIGHT & ~1) - 1)) / ((DST_HEIGHT & ~1) - 1) * (1 << 16) + .5)
#define CNV ((intptr_t)(filter->p_sys))

/*** Scaling from I420 ***/
static void I420_I420_Scale (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->U_PITCH, src->V_PITCH };
    OMXSize insize = { SRC_WIDTH, SRC_HEIGHT };
    OMX_U8 *out[3] = { dst->Y_PIXELS, dst->U_PIXELS, dst->V_PIXELS };
    OMX_INT outstep[3] = { dst->Y_PITCH, dst->U_PITCH, dst->V_PITCH };
    OMXSize outsize = { DST_WIDTH, DST_HEIGHT };

    omxIPCS_YCbCr420RszRot_U8_P3R (
        in, instep, insize, out, outstep, outsize,
        OMX_IP_NEAREST, OMX_IP_DISABLE, XRR_MAX, YRR_MAX);
}
VIDEO_FILTER_WRAPPER (I420_I420_Scale)

static void I420_YV12_Scale (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->U_PITCH, src->V_PITCH };
    OMXSize insize = { SRC_WIDTH, SRC_HEIGHT };
    OMX_U8 *out[3] = { dst->Y_PIXELS, dst->V_PIXELS, dst->U_PIXELS };
    OMX_INT outstep[3] = { dst->Y_PITCH, dst->V_PITCH, dst->U_PITCH };
    OMXSize outsize = { DST_WIDTH, DST_HEIGHT };

    omxIPCS_YCbCr420RszRot_U8_P3R (
        in, instep, insize, out, outstep, outsize,
        OMX_IP_NEAREST, OMX_IP_DISABLE, XRR_MAX, YRR_MAX);
}
VIDEO_FILTER_WRAPPER (I420_YV12_Scale)

static void I420_RGB_Scale (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->U_PITCH, src->V_PITCH };
    OMXSize insize = { SRC_WIDTH, SRC_HEIGHT };
    OMX_U8 *out = dst->p->p_pixels;
    OMX_INT outstep = dst->p->i_pitch;
    OMXSize outsize = { DST_WIDTH, DST_HEIGHT };

    omxIPCS_YCbCr420RszCscRotBGR_U8_P3C3R (
        in, instep, insize, out, outstep, outsize,
        CNV, OMX_IP_NEAREST, OMX_IP_DISABLE, XRR_MAX, YRR_MAX);
}
VIDEO_FILTER_WRAPPER (I420_RGB_Scale)

/*** Scaling from YV12 ***/
static void YV12_I420_Scale (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->V_PIXELS, src->U_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->V_PITCH, src->U_PITCH };
    OMXSize insize = { SRC_WIDTH, SRC_HEIGHT };
    OMX_U8 *out[3] = { dst->Y_PIXELS, dst->U_PIXELS, dst->V_PIXELS };
    OMX_INT outstep[3] = { dst->Y_PITCH, dst->U_PITCH, dst->V_PITCH };
    OMXSize outsize = { DST_WIDTH, DST_HEIGHT };

    omxIPCS_YCbCr420RszRot_U8_P3R (
        in, instep, insize, out, outstep, outsize,
        OMX_IP_NEAREST, OMX_IP_DISABLE, XRR_MAX, YRR_MAX);
}
VIDEO_FILTER_WRAPPER (YV12_I420_Scale)

static void YV12_YV12_Scale (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->V_PIXELS, src->U_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->V_PITCH, src->U_PITCH };
    OMXSize insize = { SRC_WIDTH, SRC_HEIGHT };
    OMX_U8 *out[3] = { dst->Y_PIXELS, dst->V_PIXELS, dst->U_PIXELS };
    OMX_INT outstep[3] = { dst->Y_PITCH, dst->V_PITCH, dst->U_PITCH };
    OMXSize outsize = { DST_WIDTH, DST_HEIGHT };

    omxIPCS_YCbCr420RszRot_U8_P3R (
        in, instep, insize, out, outstep, outsize,
        OMX_IP_NEAREST, OMX_IP_DISABLE, XRR_MAX, YRR_MAX);
}
VIDEO_FILTER_WRAPPER (YV12_YV12_Scale)

static void YV12_RGB_Scale (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->V_PIXELS, src->U_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->V_PITCH, src->U_PITCH };
    OMXSize insize = { SRC_WIDTH, SRC_HEIGHT };
    OMX_U8 *out = dst->p->p_pixels;
    OMX_INT outstep = dst->p->i_pitch;
    OMXSize outsize = { DST_WIDTH, DST_HEIGHT };

    omxIPCS_YCbCr420RszCscRotBGR_U8_P3C3R (
        in, instep, insize, out, outstep, outsize,
        CNV, OMX_IP_NEAREST, OMX_IP_DISABLE, XRR_MAX, YRR_MAX);
}
VIDEO_FILTER_WRAPPER (YV12_RGB_Scale)

/*** Scaling from I422 ***/
static void I422_I422_Scale (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->U_PITCH, src->V_PITCH };
    OMXSize insize = { SRC_WIDTH, SRC_HEIGHT };
    OMX_U8 *out[3] = { dst->Y_PIXELS, dst->U_PIXELS, dst->V_PIXELS };
    OMX_INT outstep[3] = { dst->Y_PITCH, dst->U_PITCH, dst->V_PITCH };
    OMXSize outsize = { DST_WIDTH, DST_HEIGHT };

    omxIPCS_YCbCr422RszRot_U8_P3R (
        in, instep, insize, out, outstep, outsize,
        OMX_IP_NEAREST, OMX_IP_DISABLE, XRR_MAX, YRR_MAX);
}
VIDEO_FILTER_WRAPPER (I422_I422_Scale)

static void I422_RGB_Scale (filter_t *filter, picture_t *src, picture_t *dst)
{
    const OMX_U8 *in[3] = { src->Y_PIXELS, src->U_PIXELS, src->V_PIXELS };
    OMX_INT instep[3] = { src->Y_PITCH, src->U_PITCH, src->V_PITCH };
    OMXSize insize = { SRC_WIDTH, SRC_HEIGHT };
    OMX_U8 *out = dst->p->p_pixels;
    OMX_INT outstep = dst->p->i_pitch;
    OMXSize outsize = { DST_WIDTH, DST_HEIGHT };

    omxIPCS_YCbCr422RszCscRotBGR_U8_P3C3R (
        in, instep, insize, out, outstep, outsize,
        CNV, OMX_IP_NEAREST, OMX_IP_DISABLE, XRR_MAX, YRR_MAX);
}
VIDEO_FILTER_WRAPPER (I422_RGB_Scale)

/*** Scaling initialization ***/
static int OpenScaler (vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    switch (filter->fmt_in.video.i_chroma)
    {
        case VLC_CODEC_I420:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I420:
                    filter->pf_video_filter = I420_I420_Scale_Filter;
                    return VLC_SUCCESS;
                case VLC_CODEC_YV12:
                    filter->pf_video_filter = I420_YV12_Scale_Filter;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB16:
                    if (FixRV16 (&filter->fmt_out.video))
                        return VLC_EGENERIC;
                    filter->pf_video_filter = I420_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR565;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB15:
                    if (FixRV15 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = I420_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR555;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB12:
                    if (FixRV12 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = I420_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR444;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB24:
                    if (FixRV24 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = I420_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR888;
                    return VLC_SUCCESS;
            }
            break;

        case VLC_CODEC_YV12:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I420:
                    filter->pf_video_filter = YV12_I420_Scale_Filter;
                    return VLC_SUCCESS;
                case VLC_CODEC_YV12:
                    filter->pf_video_filter = YV12_YV12_Scale_Filter;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB16:
                    if (FixRV16 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = YV12_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR565;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB15:
                    if (FixRV15 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = YV12_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR555;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB12:
                    if (FixRV12 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = YV12_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR444;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB24:
                    if (FixRV24 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = YV12_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR888;
                    return VLC_SUCCESS;
            }
            break;

        case VLC_CODEC_I422:
            switch (filter->fmt_out.video.i_chroma)
            {
                case VLC_CODEC_I422:
                    filter->pf_video_filter = I422_I422_Scale_Filter;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB16:
                    if (FixRV16 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = I422_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR565;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB15:
                    if (FixRV15 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = I422_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR555;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB12:
                    if (FixRV12 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = I422_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR444;
                    return VLC_SUCCESS;
                case VLC_CODEC_RGB24:
                    if (FixRV24 (&filter->fmt_out.video))
                        break;
                    filter->pf_video_filter = I422_RGB_Scale_Filter;
                    filter->p_sys = (void *)(intptr_t)OMX_IP_BGR888;
                    return VLC_SUCCESS;
            }
            break;
    }
    return VLC_EGENERIC;
}
