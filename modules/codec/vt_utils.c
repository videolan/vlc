/*****************************************************************************
 * vt_utils.c: videotoolbox/cvpx utility functions
 *****************************************************************************
 * Copyright (C) 2017 VLC authors, VideoLAN and VideoLabs
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

#include <vlc_atomic.h>

#include "vt_utils.h"

CFMutableDictionaryRef
cfdict_create(CFIndex capacity)
{
    return CFDictionaryCreateMutable(kCFAllocatorDefault, capacity,
                                     &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
}

void
cfdict_set_int32(CFMutableDictionaryRef dict, CFStringRef key, int value)
{
    CFNumberRef number = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
    CFDictionarySetValue(dict, key, number);
    CFRelease(number);
}

struct cvpxpic_ctx
{
    picture_context_t s;
    CVPixelBufferRef cvpx;
    unsigned nb_fields;

    vlc_atomic_rc_t rc;
    void (*on_released_cb)(vlc_video_context *vctx, unsigned);
};

static void
cvpxpic_destroy_cb(picture_context_t *opaque)
{
    struct cvpxpic_ctx *ctx = (struct cvpxpic_ctx *)opaque;

    if (vlc_atomic_rc_dec(&ctx->rc))
    {
        CFRelease(ctx->cvpx);
        if (ctx->on_released_cb)
            ctx->on_released_cb(opaque->vctx, ctx->nb_fields);
        free(opaque);
    }
}

static picture_context_t *
cvpxpic_copy_cb(struct picture_context_t *opaque)
{
    struct cvpxpic_ctx *ctx = (struct cvpxpic_ctx *)opaque;
    vlc_atomic_rc_inc(&ctx->rc);
    vlc_video_context_Hold(opaque->vctx);
    return opaque;
}

static int
cvpxpic_attach_common(picture_t *p_pic, CVPixelBufferRef cvpx,
                      void (*pf_destroy)(picture_context_t *),
                      vlc_video_context *vctx,
                      void (*on_released_cb)(vlc_video_context *vctx, unsigned))
{
    struct cvpxpic_ctx *ctx = malloc(sizeof(struct cvpxpic_ctx));
    if (ctx == NULL)
    {
        picture_Release(p_pic);
        return VLC_ENOMEM;
    }
    ctx->s = (picture_context_t) {
        pf_destroy, cvpxpic_copy_cb, vctx,
    };
    ctx->cvpx = CVPixelBufferRetain(cvpx);
    ctx->nb_fields = p_pic->i_nb_fields;
    vlc_atomic_rc_init(&ctx->rc);

    assert(vctx);
    vlc_video_context_Hold(vctx);
    ctx->on_released_cb = on_released_cb;

    p_pic->context = &ctx->s;

    return VLC_SUCCESS;
}

int
cvpxpic_attach(picture_t *p_pic, CVPixelBufferRef cvpx, vlc_video_context *vctx,
               void (*on_released_cb)(vlc_video_context *vctx, unsigned))
{
    return cvpxpic_attach_common(p_pic, cvpx, cvpxpic_destroy_cb, vctx, on_released_cb);
}

CVPixelBufferRef
cvpxpic_get_ref(picture_t *pic)
{
    assert(pic->context != NULL);
    return ((struct cvpxpic_ctx *)pic->context)->cvpx;
}

static void
cvpxpic_destroy_mapped_ro_cb(picture_context_t *opaque)
{
    struct cvpxpic_ctx *ctx = (struct cvpxpic_ctx *)opaque;

    CVPixelBufferUnlockBaseAddress(ctx->cvpx, kCVPixelBufferLock_ReadOnly);
    cvpxpic_destroy_cb(opaque);
}

static void
cvpxpic_destroy_mapped_rw_cb(picture_context_t *opaque)
{
    struct cvpxpic_ctx *ctx = (struct cvpxpic_ctx *)opaque;

    CVPixelBufferUnlockBaseAddress(ctx->cvpx, 0);
    cvpxpic_destroy_cb(opaque);
}

picture_t *
cvpxpic_create_mapped(const video_format_t *fmt, CVPixelBufferRef cvpx,
                      vlc_video_context *vctx, bool readonly)

{
    unsigned planes_count;
    switch (fmt->i_chroma)
    {
        case VLC_CODEC_BGRA:
        case VLC_CODEC_UYVY: planes_count = 0; break;
        case VLC_CODEC_NV12:
        case VLC_CODEC_P010: planes_count = 2; break;
        case VLC_CODEC_I420: planes_count = 3; break;
        default: return NULL;
    }

    CVPixelBufferLockFlags lock = readonly ? kCVPixelBufferLock_ReadOnly : 0;
    CVPixelBufferLockBaseAddress(cvpx, lock);
    picture_resource_t rsc = { };

#ifndef NDEBUG
    assert(CVPixelBufferGetPlaneCount(cvpx) == planes_count);
#endif

    if (planes_count == 0)
    {
        rsc.p[0].p_pixels = CVPixelBufferGetBaseAddress(cvpx);
        rsc.p[0].i_lines = CVPixelBufferGetHeight(cvpx);
        rsc.p[0].i_pitch = CVPixelBufferGetBytesPerRow(cvpx);
    }
    else
    {
        for (unsigned i = 0; i < planes_count; ++i)
        {
            rsc.p[i].p_pixels = CVPixelBufferGetBaseAddressOfPlane(cvpx, i);
            rsc.p[i].i_lines = CVPixelBufferGetHeightOfPlane(cvpx, i);
            rsc.p[i].i_pitch = CVPixelBufferGetBytesPerRowOfPlane(cvpx, i);
        }
    }

    void (*pf_destroy)(picture_context_t *) = readonly ?
        cvpxpic_destroy_mapped_ro_cb : cvpxpic_destroy_mapped_rw_cb;

    picture_t *pic = picture_NewFromResource(fmt, &rsc);
    if (pic == NULL
     || cvpxpic_attach_common(pic, cvpx, pf_destroy, vctx, NULL) != VLC_SUCCESS)
    {
        CVPixelBufferUnlockBaseAddress(cvpx, lock);
        return NULL;
    }
    return pic;
}

picture_t *
cvpxpic_unmap(picture_t *mapped_pic)
{
    video_format_t fmt = mapped_pic->format;
    switch (fmt.i_chroma)
    {
        case VLC_CODEC_UYVY: fmt.i_chroma = VLC_CODEC_CVPX_UYVY; break;
        case VLC_CODEC_NV12: fmt.i_chroma = VLC_CODEC_CVPX_NV12; break;
        case VLC_CODEC_P010: fmt.i_chroma = VLC_CODEC_CVPX_P010; break;
        case VLC_CODEC_I420: fmt.i_chroma = VLC_CODEC_CVPX_I420; break;
        case VLC_CODEC_BGRA: fmt.i_chroma = VLC_CODEC_CVPX_BGRA; break;
        default:
            assert(!"invalid mapped_pic fmt");
            picture_Release(mapped_pic);
            return NULL;
    }
    assert(mapped_pic->context != NULL);

    picture_t *hw_pic = picture_NewFromFormat(&fmt);
    if (hw_pic == NULL)
    {
        picture_Release(mapped_pic);
        return NULL;
    }

    cvpxpic_attach(hw_pic, cvpxpic_get_ref(mapped_pic), NULL, NULL);
    picture_CopyProperties(hw_pic, mapped_pic);
    picture_Release(mapped_pic);
    return hw_pic;
}

CVPixelBufferPoolRef
cvpxpool_create(const video_format_t *fmt, unsigned count)
{
    int cvpx_format;
    switch (fmt->i_chroma)
    {
        case VLC_CODEC_CVPX_UYVY:
            cvpx_format = kCVPixelFormatType_422YpCbCr8;
            break;
        case VLC_CODEC_CVPX_NV12:
            cvpx_format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
            break;
        case VLC_CODEC_CVPX_I420:
            cvpx_format = kCVPixelFormatType_420YpCbCr8Planar;
            break;
        case VLC_CODEC_CVPX_BGRA:
            cvpx_format = kCVPixelFormatType_32BGRA;
            break;
        case VLC_CODEC_CVPX_P010:
            cvpx_format = 'x420'; /* kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange */
            break;
        default:
            return NULL;
    }

    /* destination pixel buffer attributes */
    CFMutableDictionaryRef cvpx_attrs_dict = cfdict_create(5);
    if (unlikely(cvpx_attrs_dict == NULL))
        return NULL;
    CFMutableDictionaryRef pool_dict = cfdict_create(2);
    if (unlikely(pool_dict == NULL))
    {
        CFRelease(cvpx_attrs_dict);
        return NULL;
    }

    CFMutableDictionaryRef io_dict = cfdict_create(0);
    if (unlikely(io_dict == NULL))
    {
        CFRelease(cvpx_attrs_dict);
        CFRelease(pool_dict);
        return NULL;
    }
    CFDictionarySetValue(cvpx_attrs_dict,
                         kCVPixelBufferIOSurfacePropertiesKey, io_dict);
    CFRelease(io_dict);

    cfdict_set_int32(cvpx_attrs_dict, kCVPixelBufferPixelFormatTypeKey,
                     cvpx_format);
    cfdict_set_int32(cvpx_attrs_dict, kCVPixelBufferWidthKey, fmt->i_visible_width);
    cfdict_set_int32(cvpx_attrs_dict, kCVPixelBufferHeightKey, fmt->i_visible_height);
    /* Required by CIFilter to render IOSurface */
    cfdict_set_int32(cvpx_attrs_dict, kCVPixelBufferBytesPerRowAlignmentKey, 16);

    cfdict_set_int32(pool_dict, kCVPixelBufferPoolMinimumBufferCountKey, count);
    cfdict_set_int32(pool_dict, kCVPixelBufferPoolMaximumBufferAgeKey, 0);

    CVPixelBufferPoolRef pool;
    CVReturn err =
        CVPixelBufferPoolCreate(NULL, pool_dict, cvpx_attrs_dict, &pool);
    CFRelease(pool_dict);
    CFRelease(cvpx_attrs_dict);
    if (err != kCVReturnSuccess)
        return NULL;

    CVPixelBufferRef cvpxs[count];
    for (unsigned i = 0; i < count; ++i)
    {
        err = CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &cvpxs[i]);
        if (err != kCVReturnSuccess)
        {
            CVPixelBufferPoolRelease(pool);
            pool = NULL;
            count = i;
            break;
        }
    }
    for (unsigned i = 0; i < count; ++i)
        CFRelease(cvpxs[i]);

    return pool;
}

CVPixelBufferRef
cvpxpool_new_cvpx(CVPixelBufferPoolRef pool)
{
    CVPixelBufferRef cvpx;
    CVReturn err = CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &cvpx);

    if (err != kCVReturnSuccess)
        return NULL;

    return cvpx;
}

struct cvpx_video_context
{
    const struct vlc_video_context_operations *ops;
    enum cvpx_video_context_type type;
    uint8_t private[];
};

static void
cvpx_video_context_Destroy(void *priv)
{
    struct cvpx_video_context *cvpx_vctx = priv;
    if (cvpx_vctx->ops->destroy)
        cvpx_vctx->ops->destroy(&cvpx_vctx->private);
}

vlc_video_context *
vlc_video_context_CreateCVPX(vlc_decoder_device *device,
                              enum cvpx_video_context_type type, size_t type_size,
                              const struct vlc_video_context_operations *ops)
{
    static const struct vlc_video_context_operations vctx_ops =
    {
        cvpx_video_context_Destroy,
    };
    vlc_video_context *vctx =
        vlc_video_context_Create(device, VLC_VIDEO_CONTEXT_CVPX,
                                 sizeof(struct cvpx_video_context) + type_size,
                                 &vctx_ops);
    if (!vctx)
        return NULL;
    struct cvpx_video_context *cvpx_vctx =
        vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_CVPX);
    assert(cvpx_vctx != NULL);
    cvpx_vctx->type = type;
    cvpx_vctx->ops = ops;

    return vctx;
}

void *
vlc_video_context_GetCVPXPrivate(vlc_video_context *vctx,
                                 enum cvpx_video_context_type type)
{
    struct cvpx_video_context *cvpx_vctx =
        vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_CVPX);

    if (cvpx_vctx && cvpx_vctx->type == type)
        return &cvpx_vctx->private;
    return NULL;
}
