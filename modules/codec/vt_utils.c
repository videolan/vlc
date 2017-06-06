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
};

static void
cvpxpic_destroy_cb(picture_context_t *opaque)
{
    struct cvpxpic_ctx *ctx = (struct cvpxpic_ctx *)opaque;

    CFRelease(ctx->cvpx);
    free(opaque);
}

static picture_context_t *
cvpxpic_copy_cb(struct picture_context_t *opaque)
{
    struct cvpxpic_ctx *src_ctx = (struct cvpxpic_ctx *)opaque;
    struct cvpxpic_ctx *dst_ctx = malloc(sizeof(struct cvpxpic_ctx));
    if (dst_ctx == NULL)
        return NULL;
    *dst_ctx = *src_ctx;
    dst_ctx->cvpx = CVPixelBufferRetain(dst_ctx->cvpx);
    return &dst_ctx->s;
}

int
cvpxpic_attach(picture_t *p_pic, CVPixelBufferRef cvpx)
{
    /* will be freed by the vout */
    struct cvpxpic_ctx *ctx = malloc(sizeof(struct cvpxpic_ctx));
    if (ctx == NULL)
    {
        picture_Release(p_pic);
        return VLC_ENOMEM;
    }
    ctx->s.destroy = cvpxpic_destroy_cb;
    ctx->s.copy = cvpxpic_copy_cb;
    ctx->cvpx = CVPixelBufferRetain(cvpx);
    p_pic->context = &ctx->s;

    return VLC_SUCCESS;
}

CVPixelBufferRef
cvpxpic_get_ref(picture_t *pic)
{
    assert(pic->context != NULL);
    return ((struct cvpxpic_ctx *)pic->context)->cvpx;
}

static void
cvpxpic_destroy_mapped_ro_cb(picture_t *pic)
{
    CVPixelBufferRef cvpx = (void *) pic->p_sys;
    CVPixelBufferUnlockBaseAddress(cvpx, kCVPixelBufferLock_ReadOnly);
    CFRelease(cvpx);
}

static void
cvpxpic_destroy_mapped_rw_cb(picture_t *pic)
{
    CVPixelBufferRef cvpx = (void *) pic->p_sys;
    CVPixelBufferUnlockBaseAddress(cvpx, 0);
    CFRelease(cvpx);
}

picture_t *
cvpxpic_create_mapped(const video_format_t *fmt, CVPixelBufferRef cvpx,
                      bool readonly)
{
    unsigned planes_count;
    switch (fmt->i_chroma)
    {
        case VLC_CODEC_BGRA:
        case VLC_CODEC_UYVY: planes_count = 0; break;
        case VLC_CODEC_NV12: planes_count = 2; break;
        case VLC_CODEC_I420: planes_count = 3; break;
        default: return NULL;
    }

    CVPixelBufferLockFlags lock = readonly ? kCVPixelBufferLock_ReadOnly : 0;
    CVPixelBufferLockBaseAddress(cvpx, lock);
    picture_resource_t rsc = {
        .p_sys = (void *)cvpx,
        .pf_destroy = readonly ? cvpxpic_destroy_mapped_ro_cb
                               : cvpxpic_destroy_mapped_rw_cb,
    };

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

    picture_t *pic = picture_NewFromResource(fmt, &rsc);
    if (pic == NULL)
    {
        CVPixelBufferUnlockBaseAddress(cvpx, lock);
        CFRelease(cvpx);
        return NULL;
    }
    CVPixelBufferRetain(cvpx);
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
        case VLC_CODEC_I420: fmt.i_chroma = VLC_CODEC_CVPX_I420; break;
        case VLC_CODEC_BGRA: fmt.i_chroma = VLC_CODEC_CVPX_BGRA; break;
        default:
            assert(!"invalid mapped_pic fmt");
            picture_Release(mapped_pic);
            return NULL;
    }
    assert(mapped_pic->p_sys != NULL);

    picture_t *hw_pic = picture_NewFromFormat(&fmt);
    if (hw_pic == NULL)
    {
        picture_Release(mapped_pic);
        return NULL;
    }

    cvpxpic_attach(hw_pic, CVPixelBufferRetain((void *)mapped_pic->p_sys));
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

#if !TARGET_OS_IPHONE
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
#else
    CFDictionarySetValue(cvpx_attrs_dict,
                         kCVPixelBufferOpenGLESCompatibilityKey, kCFBooleanTrue);
#endif

    cfdict_set_int32(cvpx_attrs_dict, kCVPixelBufferBytesPerRowAlignmentKey,
                     fmt->i_width);
    cfdict_set_int32(cvpx_attrs_dict, kCVPixelBufferPixelFormatTypeKey,
                     cvpx_format);
    cfdict_set_int32(cvpx_attrs_dict, kCVPixelBufferWidthKey, fmt->i_width);
    cfdict_set_int32(cvpx_attrs_dict, kCVPixelBufferHeightKey, fmt->i_height);

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
cvpxpool_get_cvpx(CVPixelBufferPoolRef pool)
{
    CVPixelBufferRef cvpx;
    CVReturn err = CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &cvpx);

    if (err != kCVReturnSuccess)
        return NULL;

    return cvpx;
}
