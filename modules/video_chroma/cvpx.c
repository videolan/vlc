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

#include <QuartzCore/QuartzCore.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <TargetConditionals.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_modules.h>
#include "../codec/vt_utils.h"

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

#if !TARGET_OS_IPHONE
static int Open_CVPX_to_CVPX(vlc_object_t *);
static void Close_CVPX_to_CVPX(vlc_object_t *);
#endif

struct filter_sys_t
{
    CVPixelBufferPoolRef pool;
    union
    {
        filter_t *p_sw_filter;
#if !TARGET_OS_IPHONE
        VTPixelTransferSessionRef vttransfer;
#endif
    };
};

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

static picture_t *CVPX_TO_SW_Filter(filter_t *p_filter, picture_t *src)
{
    picture_t *src_sw =
        cvpxpic_create_mapped(&p_filter->fmt_out.video, cvpxpic_get_ref(src),
                              true);
    if (!src_sw)
    {
        picture_Release(src);
        return NULL;
    }
    picture_CopyProperties(src_sw, src);
    picture_Release(src);
    return src_sw;
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
        cvpxpic_create_mapped(&p_filter->fmt_in.video, cvpx, false);
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

    /* Copy pixels to the CVPX backed picture. Don't use picture_CopyPixels()
     * since we want to handle the context ourself. */
    for( int i = 0; i < src->i_planes ; i++ )
        plane_CopyPixels( mapped_dst->p+i, src->p+i );

    /* Attach the CVPX to a new opaque picture */
    cvpxpic_attach(dst, cvpxpic_get_ref(mapped_dst));

    /* Unlock and unmap the dst picture */
    picture_Release(mapped_dst);

    picture_CopyProperties(dst, src);
    picture_Release(src);
    return dst;
}


static picture_t *CVPX_TO_I420_Filter(filter_t *p_filter, picture_t *src)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    filter_t *p_sw_filter = p_sys->p_sw_filter;
    assert(p_sw_filter != NULL);
    picture_t *dst = NULL;

    picture_t *src_sw =
        cvpxpic_create_mapped(&p_sw_filter->fmt_in.video, cvpxpic_get_ref(src),
                              true);

    if (!src_sw)
    {
        picture_Release(src);
        return NULL;
    }
    picture_CopyProperties(src_sw, src);
    picture_Release(src);

    dst = p_sw_filter->pf_video_filter(p_sw_filter, src_sw);

    return dst;
}

static picture_t *SW_buffer_new(filter_t *p_filter)
{
    return picture_NewFromFormat( &p_filter->fmt_out.video );
}

static picture_t *CVPX_buffer_new(filter_t *p_sw_filter)
{
    filter_t *p_filter = p_sw_filter->owner.sys;
    filter_sys_t *p_sys = p_filter->p_sys;

    CVPixelBufferRef cvpx = cvpxpool_new_cvpx(p_sys->pool);
    if (cvpx == NULL)
        return NULL;

    picture_t *pic =
        cvpxpic_create_mapped(&p_sw_filter->fmt_out.video, cvpx, false);
    CFRelease(cvpx);
    return pic;
}

static picture_t *I420_TO_CVPX_Filter(filter_t *p_filter, picture_t *src)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    filter_t *p_sw_filter = p_sys->p_sw_filter;

    picture_t *sw_dst = p_sw_filter->pf_video_filter(p_sw_filter, src);
    if (sw_dst == NULL)
        return NULL;

    return cvpxpic_unmap(sw_dst);
}

static void Close(vlc_object_t *obj)
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;

    if (p_sys->p_sw_filter != NULL)
    {
        module_unneed(p_sys->p_sw_filter, p_sys->p_sw_filter->p_module);
        vlc_object_release(p_sys->p_sw_filter);
    }

    if (p_sys->pool != NULL)
        CVPixelBufferPoolRelease(p_sys->pool);
    free(p_sys);
}

static int Open(vlc_object_t *obj)
{
    filter_t *p_filter = (filter_t *)obj;

    if (p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
        || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width)
        return VLC_EGENERIC;

#define CASE_CVPX_INPUT(x) \
    case VLC_CODEC_CVPX_##x: \
        if (p_filter->fmt_out.video.i_chroma == VLC_CODEC_##x) { \
            p_filter->pf_video_filter = CVPX_TO_SW_Filter; \
        } else if (p_filter->fmt_out.video.i_chroma == VLC_CODEC_I420) {\
            p_filter->pf_video_filter = CVPX_TO_I420_Filter; \
            i_sw_filter_in_chroma = VLC_CODEC_##x; \
            i_sw_filter_out_chroma = VLC_CODEC_I420; \
            sw_filter_owner.video.buffer_new = SW_buffer_new; \
        } else return VLC_EGENERIC; \
        b_need_pool = false;

#define CASE_CVPX_OUTPUT(x) \
    case VLC_CODEC_CVPX_##x: \
        if (p_filter->fmt_in.video.i_chroma == VLC_CODEC_##x) { \
            p_filter->pf_video_filter = SW_TO_CVPX_Filter; \
        } else if (p_filter->fmt_in.video.i_chroma == VLC_CODEC_I420) {\
            p_filter->pf_video_filter = I420_TO_CVPX_Filter; \
            i_sw_filter_in_chroma = VLC_CODEC_I420; \
            i_sw_filter_out_chroma = VLC_CODEC_##x; \
            sw_filter_owner.sys = p_filter; \
            sw_filter_owner.video.buffer_new = CVPX_buffer_new; \
        } else return VLC_EGENERIC; \
        b_need_pool = true;

    bool b_need_pool;
    vlc_fourcc_t i_sw_filter_in_chroma = 0, i_sw_filter_out_chroma = 0;
    filter_owner_t sw_filter_owner = {};
    switch (p_filter->fmt_in.video.i_chroma)
    {
        CASE_CVPX_INPUT(NV12)
            break;
        CASE_CVPX_INPUT(P010)
            break;
        CASE_CVPX_INPUT(UYVY)
            break;
        CASE_CVPX_INPUT(I420)
            break;
        CASE_CVPX_INPUT(BGRA)
            break;
        default:
            switch (p_filter->fmt_out.video.i_chroma)
            {
                CASE_CVPX_OUTPUT(NV12)
                    break;
                CASE_CVPX_OUTPUT(P010)
                    break;
                CASE_CVPX_OUTPUT(UYVY)
                    break;
                CASE_CVPX_OUTPUT(I420)
                    break;
                CASE_CVPX_OUTPUT(BGRA)
                    break;
                default:
                    return VLC_EGENERIC;
            }
    }

    filter_sys_t *p_sys = p_filter->p_sys = malloc(sizeof(filter_sys_t));

    if (unlikely(!p_sys))
        return VLC_ENOMEM;

    p_sys->p_sw_filter = NULL;
    p_sys->pool = NULL;

    if (b_need_pool
     && (p_sys->pool = cvpxpool_create(&p_filter->fmt_out.video, 3)) == NULL)
        goto error;

    if (i_sw_filter_in_chroma != 0)
    {
        filter_t *p_sw_filter = vlc_object_create(p_filter, sizeof(filter_t));
        if (unlikely(p_sw_filter == NULL))
            goto error;

        p_sw_filter->fmt_in = p_filter->fmt_in;
        p_sw_filter->fmt_out = p_filter->fmt_out;
        p_sw_filter->fmt_in.i_codec = p_sw_filter->fmt_in.video.i_chroma
                                    = i_sw_filter_in_chroma;
        p_sw_filter->fmt_out.i_codec = p_sw_filter->fmt_out.video.i_chroma
                                     = i_sw_filter_out_chroma;

        p_sw_filter->owner = sw_filter_owner;
        p_sw_filter->p_module = module_need(p_sw_filter, "video converter",
                                            NULL, false);
        if (p_sw_filter->p_module == NULL)
        {
            vlc_object_release(p_sw_filter);
            goto error;
        }
        p_sys->p_sw_filter = p_sw_filter;
    }

    return VLC_SUCCESS;

error:
    Close(obj);
    return VLC_EGENERIC;
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

    if (VTPixelTransferSessionTransferImage(filter->p_sys->vttransfer,
                                            src_cvpx, dst_cvpx) != noErr)
    {
        picture_Release(dst);
        picture_Release(src);
        return NULL;
    }

    cvpxpic_attach(dst, dst_cvpx);

    picture_CopyProperties(dst, src);
    picture_Release(src);
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
    free(filter->p_sys);
}

#endif
