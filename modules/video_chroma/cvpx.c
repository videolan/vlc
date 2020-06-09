/*****************************************************************************
 * cvpx.c: core video buffer to picture converter
 *****************************************************************************
 * Copyright (C) 2015-2017 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          Thomas Guillem <thomas@gllm.fr>
 *          Victorien Le Couviour--Tuffet <victorien.lecouiour.tuffet@gmail.com>
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

#include <QuartzCore/QuartzCore.h>
#include <TargetConditionals.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_modules.h>
#include "../codec/vt_utils.h"
#include "../video_chroma/copy.h"

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

#if !TARGET_OS_IPHONE
static int Open_CVPX_to_CVPX(vlc_object_t *);
static void Close_CVPX_to_CVPX(vlc_object_t *);
#endif

typedef struct
{
    CVPixelBufferPoolRef pool;
    union
    {
        struct
        {
            video_format_t fmt;
            copy_cache_t cache;
        } sw;
#if !TARGET_OS_IPHONE
        VTPixelTransferSessionRef vttransfer;
#endif
    };
} filter_sys_t;

vlc_module_begin ()
    set_description("Conversions from/to CoreVideo buffers")
    set_capability("video converter", 10)
    set_callbacks(Open, Close)
#if !TARGET_OS_IPHONE
    add_submodule()
    set_description("Conversions between CoreVideo buffers")
    set_callbacks(Open_CVPX_to_CVPX, Close_CVPX_to_CVPX)
#endif
vlc_module_end ()


/********************************
 * CVPX to/from I420 conversion *
 ********************************/

static void Copy(filter_t *p_filter, picture_t *dst, picture_t *src,
                 unsigned height)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    const uint8_t *src_planes[3] = { src->p[0].p_pixels,
                                     src->p[1].p_pixels,
                                     src->p[2].p_pixels };
    const size_t src_pitches[3] = { src->p[0].i_pitch,
                                    src->p[1].i_pitch,
                                    src->p[2].i_pitch };

#define DO(x) \
    x(dst, src_planes, src_pitches, height, &p_sys->sw.cache)
#define DO_S(x, shift) \
    x(dst, src_planes, src_pitches, height, shift, &p_sys->sw.cache)
#define DO_P(x) \
    x(dst, src_planes[0], src_pitches[0], height, &p_sys->sw.cache)

    const vlc_fourcc_t infcc = src->format.i_chroma;
    const vlc_fourcc_t outfcc = dst->format.i_chroma;

    switch (infcc)
    {
        case VLC_CODEC_NV12:
            if (outfcc == VLC_CODEC_NV12)
                DO(Copy420_SP_to_SP);
            else
            {
                assert(outfcc == VLC_CODEC_I420);
                DO(Copy420_SP_to_P);
            }
            break;
        case VLC_CODEC_P010:
            if (outfcc == VLC_CODEC_P010)
                DO(Copy420_SP_to_SP);
            else
            {
                assert(dst->format.i_chroma == VLC_CODEC_I420_10L);
                DO_S(Copy420_16_SP_to_P, 6);
            }
            break;
        case VLC_CODEC_I420:
            if (outfcc == VLC_CODEC_I420)
                DO(Copy420_P_to_P);
            else
            {
                assert(outfcc == VLC_CODEC_NV12);
                DO(Copy420_P_to_SP);
            }
            break;
        case VLC_CODEC_I420_10L:
            assert(outfcc == VLC_CODEC_P010);
            DO_S(Copy420_16_P_to_SP, -6);
            break;
        case VLC_CODEC_UYVY:
            assert(outfcc == VLC_CODEC_UYVY);
            DO_P(CopyPacked);
            break;
        case VLC_CODEC_BGRA:
            assert(outfcc == VLC_CODEC_BGRA);
            DO_P(CopyPacked);
            break;
        default:
            vlc_assert_unreachable();
    }

#undef DO
#undef DO_S
#undef DO_P
}

static picture_t *CVPX_TO_SW_Filter(filter_t *p_filter, picture_t *src)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    CVPixelBufferRef cvpx = cvpxpic_get_ref(src);
    picture_t *src_sw =
        cvpxpic_create_mapped(&p_sys->sw.fmt, cvpx, p_filter->vctx_in, true);
    if (!src_sw)
    {
        picture_Release(src);
        return NULL;
    }

    picture_t *dst = filter_NewPicture(p_filter);
    if (!dst)
    {
        picture_Release(src_sw);
        picture_Release(src);
        return NULL;
    }

    size_t height = CVPixelBufferGetHeight(cvpx);
    Copy(p_filter, dst, src_sw, __MIN(height, dst->format.i_visible_height));

    picture_Release(src_sw);

    picture_CopyProperties(dst, src);
    picture_Release(src);

    return dst;
}

static picture_t *SW_TO_CVPX_Filter(filter_t *p_filter, picture_t *src)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    CVPixelBufferRef cvpx = cvpxpool_new_cvpx(p_sys->pool);
    if (cvpx == NULL)
    {
        picture_Release(src);
        return NULL;
    }

    /* Allocate a CPVX backed picture mapped for read/write */
    picture_t *mapped_dst =
        cvpxpic_create_mapped(&p_sys->sw.fmt, cvpx, p_filter->vctx_out, false);
    CFRelease(cvpx);
    if (!mapped_dst)
    {
        picture_Release(src);
        return NULL;
    }

    /* Allocate a CVPX picture without any context */
    picture_t *dst = picture_NewFromFormat(&p_filter->fmt_out.video);
    if (!dst)
    {
        picture_Release(src);
        picture_Release(mapped_dst);
        return NULL;
    }

    size_t height = CVPixelBufferGetHeight(cvpx);
    Copy(p_filter, mapped_dst, src, __MIN(height, src->format.i_visible_height));

    /* Attach the CVPX to a new opaque picture */
    cvpxpic_attach(dst, cvpxpic_get_ref(mapped_dst), p_filter->vctx_out, NULL);

    /* Unlock and unmap the dst picture */
    picture_Release(mapped_dst);

    picture_CopyProperties(dst, src);
    picture_Release(src);
    return dst;
}

static void Close(vlc_object_t *obj)
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;

    if (p_sys->pool != NULL)
        CVPixelBufferPoolRelease(p_sys->pool);

    CopyCleanCache(&p_sys->sw.cache);
    if (p_filter->vctx_out)
        vlc_video_context_Release(p_filter->vctx_out);
    free(p_sys);
}

static int Open(vlc_object_t *obj)
{
    filter_t *p_filter = (filter_t *)obj;

    if (p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
        || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width)
        return VLC_EGENERIC;

    video_format_t sw_fmt;
#define CASE_CVPX_INPUT(x, i420_fcc) \
    case VLC_CODEC_CVPX_##x: \
        sw_fmt = p_filter->fmt_out.video; \
        if (p_filter->fmt_out.video.i_chroma == VLC_CODEC_##x) \
            p_filter->pf_video_filter = CVPX_TO_SW_Filter; \
        else if (i420_fcc != 0 && p_filter->fmt_out.video.i_chroma == i420_fcc) { \
            p_filter->pf_video_filter = CVPX_TO_SW_Filter; \
            sw_fmt.i_chroma = VLC_CODEC_##x; \
        } else return VLC_EGENERIC; \

#define CASE_CVPX_OUTPUT(x, i420_fcc) \
    case VLC_CODEC_CVPX_##x: \
        sw_fmt = p_filter->fmt_in.video; \
        if (p_filter->fmt_in.video.i_chroma == VLC_CODEC_##x) { \
            p_filter->pf_video_filter = SW_TO_CVPX_Filter; \
        } \
        else if (i420_fcc != 0 && p_filter->fmt_in.video.i_chroma == i420_fcc) { \
            p_filter->pf_video_filter = SW_TO_CVPX_Filter; \
            sw_fmt.i_chroma = VLC_CODEC_##x; \
        } else return VLC_EGENERIC; \
        b_need_pool = true;

    bool b_need_pool = false;
    unsigned i_cache_pixel_bytes = 1;
    switch (p_filter->fmt_in.video.i_chroma)
    {
        CASE_CVPX_INPUT(NV12, VLC_CODEC_I420)
            break;
        CASE_CVPX_INPUT(P010, VLC_CODEC_I420_10L)
            i_cache_pixel_bytes = 2;
            break;
        CASE_CVPX_INPUT(UYVY, 0)
            break;
        CASE_CVPX_INPUT(I420, 0)
            break;
        CASE_CVPX_INPUT(BGRA, 0)
            break;
        default:
            switch (p_filter->fmt_out.video.i_chroma)
            {
                CASE_CVPX_OUTPUT(NV12, VLC_CODEC_I420)
                    break;
                CASE_CVPX_OUTPUT(P010, VLC_CODEC_I420_10L)
                    i_cache_pixel_bytes = 2;
                    break;
                CASE_CVPX_OUTPUT(UYVY, 0)
                    break;
                CASE_CVPX_OUTPUT(I420, 0)
                    break;
                CASE_CVPX_OUTPUT(BGRA, 0)
                    break;
                default:
                    return VLC_EGENERIC;
            }
    }

    filter_sys_t *p_sys = p_filter->p_sys = malloc(sizeof(filter_sys_t));

    if (unlikely(!p_sys))
        return VLC_ENOMEM;

    p_sys->pool = NULL;
    p_sys->sw.fmt = sw_fmt;

    unsigned i_cache_width = p_filter->fmt_in.video.i_width * i_cache_pixel_bytes;
    int ret = CopyInitCache(&p_sys->sw.cache, i_cache_width);
    if (ret != VLC_SUCCESS)
        goto error;

    if (b_need_pool)
    {
        vlc_decoder_device *dec_dev =
            filter_HoldDecoderDeviceType(p_filter,
                                         VLC_DECODER_DEVICE_VIDEOTOOLBOX);
        if (dec_dev == NULL)
        {
            msg_Err(p_filter, "Missing decoder device");
            goto error;
        }
        const static struct vlc_video_context_operations vt_vctx_ops = {
            NULL,
        };
        p_filter->vctx_out =
            vlc_video_context_CreateCVPX(dec_dev, CVPX_VIDEO_CONTEXT_DEFAULT,
                                         0, &vt_vctx_ops);
        vlc_decoder_device_Release(dec_dev);
        if (!p_filter->vctx_out)
            goto error;

        p_sys->pool = cvpxpool_create(&p_filter->fmt_out.video, 3);
        if (p_sys->pool == NULL)
            goto error;
    }
    else
    {
        if (p_filter->vctx_in == NULL ||
            vlc_video_context_GetType(p_filter->vctx_in) != VLC_VIDEO_CONTEXT_CVPX)
            return VLC_EGENERIC;
    }

    p_filter->fmt_out.i_codec = p_filter->fmt_out.video.i_chroma;
    return VLC_SUCCESS;
error:
    Close(obj);
    return ret;
#undef CASE_CVPX_INPUT
#undef CASE_CVPX_OUTPUT
}

/***************************
 * CVPX to CVPX conversion *
 ***************************/

#if !TARGET_OS_IPHONE

static picture_t *
Filter(filter_t *filter, picture_t *src)
{
    filter_sys_t *p_sys = filter->p_sys;

    CVPixelBufferRef src_cvpx = cvpxpic_get_ref(src);
    assert(src_cvpx);

    picture_t *dst = filter_NewPicture(filter);
    if (!dst)
    {
        picture_Release(src);
        return NULL;
    }

    CVPixelBufferRef dst_cvpx = cvpxpool_new_cvpx(p_sys->pool);
    if (dst_cvpx == NULL)
    {
        picture_Release(src);
        picture_Release(dst);
        return NULL;
    }

    if (VTPixelTransferSessionTransferImage(p_sys->vttransfer,
                                            src_cvpx, dst_cvpx) != noErr)
    {
        picture_Release(dst);
        picture_Release(src);
        CVPixelBufferRelease(dst_cvpx);
        return NULL;
    }

    cvpxpic_attach(dst, dst_cvpx, filter->vctx_out, NULL);

    picture_CopyProperties(dst, src);
    picture_Release(src);
    CVPixelBufferRelease(dst_cvpx);
    return dst;
}

static vlc_fourcc_t const supported_chromas[] = { VLC_CODEC_CVPX_BGRA,
                                                  VLC_CODEC_CVPX_I420,
                                                  VLC_CODEC_CVPX_NV12,
                                                  VLC_CODEC_CVPX_P010,
                                                  VLC_CODEC_CVPX_UYVY };

static int
Open_CVPX_to_CVPX(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    /* Avoid conversion to self if we're not resizing */
    if (filter->fmt_in.video.i_chroma == filter->fmt_out.video.i_chroma &&
        filter->fmt_in.video.i_visible_width == filter->fmt_out.video.i_visible_width &&
        filter->fmt_in.video.i_visible_height == filter->fmt_out.video.i_visible_height)
        return VLC_EGENERIC;

    if (filter->vctx_in == NULL ||
        vlc_video_context_GetType(filter->vctx_in) != VLC_VIDEO_CONTEXT_CVPX)
        return VLC_EGENERIC;

    unsigned int i;
#define CHECK_CHROMA(fourcc) \
    i = 0; \
    while (i < ARRAY_SIZE(supported_chromas) && \
           fourcc != supported_chromas[i]) \
        ++i; \
    if (i == ARRAY_SIZE(supported_chromas)) \
        return VLC_EGENERIC; \

    CHECK_CHROMA(filter->fmt_in.video.i_chroma)
    CHECK_CHROMA(filter->fmt_out.video.i_chroma)
#undef CHECK_CHROMA

    filter_sys_t *p_sys  = filter->p_sys = calloc(1, sizeof(filter_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    if (VTPixelTransferSessionCreate(NULL, &p_sys->vttransfer)
        != noErr)
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    if ((p_sys->pool = cvpxpool_create(&filter->fmt_out.video, 3)) == NULL)
    {
        VTPixelTransferSessionInvalidate(p_sys->vttransfer);
        CFRelease(p_sys->vttransfer);
        free(p_sys);
        return VLC_EGENERIC;
    }

    filter->pf_video_filter = Filter;
    filter->vctx_out = vlc_video_context_Hold(filter->vctx_in);
    filter->fmt_out.i_codec = filter->fmt_out.video.i_chroma;
    return VLC_SUCCESS;
}

static void
Close_CVPX_to_CVPX(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *p_sys = filter->p_sys;

    VTPixelTransferSessionInvalidate(p_sys->vttransfer);
    CFRelease(p_sys->vttransfer);
    CVPixelBufferPoolRelease(p_sys->pool);
    vlc_video_context_Release(filter->vctx_out);
    free(filter->p_sys);
}

#endif
