/*****************************************************************************
 * chroma.c: VLC picture to VAAPI surface or vice versa
 *****************************************************************************
 * Copyright (C) 2017 VLC authors, VideoLAN and VideoLabs
 *
 * Author: Victorien Le Couviour--Tuffet <victorien.lecouviour.tuffet@gmail.com>
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

#include <assert.h>

#include <va/va.h>

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_plugin.h>

#include "../../video_chroma/copy.h"
#include "filters.h"

# define DEST_PICS_POOL_SZ 3

typedef struct
{
    VADisplay           dpy;
    picture_pool_t *    dest_pics;
    VASurfaceID *       va_surface_ids;
    copy_cache_t        cache;

    bool                derive_failed;
    bool                image_fallback_failed;
} filter_sys_t;

static int CreateFallbackImage(filter_t *filter, picture_t *src_pic,
                               VADisplay va_dpy, VAImage *image_fallback)
{
    int count = vaMaxNumImageFormats(va_dpy);

    VAImageFormat *fmts = vlc_alloc(count, sizeof (*fmts));
    if (unlikely(fmts == NULL))
        return VLC_ENOMEM;

    if (vaQueryImageFormats(va_dpy, fmts, &count))
    {
        free(fmts);
        return VLC_EGENERIC;
    }

    int i;
    for (i = 0; i < count; i++)
        if (fmts[i].fourcc == VA_FOURCC_NV12
         || fmts[i].fourcc == VA_FOURCC_P010)
            break;

    int ret;
    if ((fmts[i].fourcc == VA_FOURCC_NV12 || fmts[i].fourcc == VA_FOURCC_P010)
     && !vlc_vaapi_CreateImage(VLC_OBJECT(filter), va_dpy, &fmts[i],
                               src_pic->format.i_width, src_pic->format.i_height,
                               image_fallback))
        ret = VLC_SUCCESS;
    else
        ret = VLC_EGENERIC;

    free(fmts);

    return ret;
}

static inline void
FillPictureFromVAImage(picture_t *dest,
                       VAImage *src_img, uint8_t *src_buf, copy_cache_t *cache)
{
    const uint8_t * src_planes[2] = { src_buf + src_img->offsets[0],
                                      src_buf + src_img->offsets[1] };
    const size_t    src_pitches[2] = { src_img->pitches[0],
                                       src_img->pitches[1] };

    switch (src_img->format.fourcc)
    {
    case VA_FOURCC_NV12:
    {
        assert(dest->format.i_chroma == VLC_CODEC_I420);
        Copy420_SP_to_P(dest, src_planes, src_pitches, src_img->height, cache);
        break;
    }
    case VA_FOURCC_P010:
        switch (dest->format.i_chroma)
        {
            case VLC_CODEC_P010:
                Copy420_SP_to_SP(dest, src_planes, src_pitches, src_img->height,
                                 cache);
                break;
            case VLC_CODEC_I420_10L:
                Copy420_16_SP_to_P(dest, src_planes, src_pitches,
                                   src_img->height, 6, cache);
                break;
            default:
                vlc_assert_unreachable();
        }
        break;
    default:
        vlc_assert_unreachable();
        break;
    }
}

static picture_t *
DownloadSurface(filter_t *filter, picture_t *src_pic)
{
    filter_sys_t *const filter_sys = filter->p_sys;
    VADisplay           va_dpy = vlc_vaapi_PicGetDisplay(src_pic);
    VAImage             src_img;
    void *              src_buf;

    picture_t *dest = filter_NewPicture(filter);
    if (!dest)
    {
        msg_Err(filter, "filter_NewPicture failed");
        goto ret;
    }

    VAImageID image_fallback_id = VA_INVALID_ID;
    VASurfaceID surface = vlc_vaapi_PicGetSurface(src_pic);
    if (vaSyncSurface(va_dpy, surface))
        goto error;

    if (filter_sys->derive_failed ||
        vlc_vaapi_DeriveImage(VLC_OBJECT(filter), va_dpy, surface, &src_img))
    {
        if (filter_sys->image_fallback_failed)
            goto error;

        filter_sys->derive_failed = true;

        VAImage image_fallback;
        if (CreateFallbackImage(filter, src_pic, va_dpy, &image_fallback))
        {
            filter_sys->image_fallback_failed = true;
            goto error;
        }
        image_fallback_id = image_fallback.image_id;

        if (vaGetImage(va_dpy, surface, 0, 0, src_pic->format.i_width,
                       src_pic->format.i_height, image_fallback_id))
        {
            filter_sys->image_fallback_failed = true;
            goto error;
        }
        src_img = image_fallback;
    }

    if (vlc_vaapi_MapBuffer(VLC_OBJECT(filter), va_dpy, src_img.buf, &src_buf))
        goto error;

    FillPictureFromVAImage(dest, &src_img, src_buf, &filter_sys->cache);

    vlc_vaapi_UnmapBuffer(VLC_OBJECT(filter), va_dpy, src_img.buf);
    vlc_vaapi_DestroyImage(VLC_OBJECT(filter), va_dpy, src_img.image_id);

    picture_CopyProperties(dest, src_pic);
ret:
    picture_Release(src_pic);
    return dest;

error:
    if (image_fallback_id != VA_INVALID_ID)
        vlc_vaapi_DestroyImage(VLC_OBJECT(filter), va_dpy, image_fallback_id);

    picture_Release(dest);
    dest = NULL;
    goto ret;
}

static inline void
FillVAImageFromPicture(VAImage *dest_img, uint8_t *dest_buf,
                       picture_t *dest_pic, picture_t *src,
                       copy_cache_t *cache)
{
    const uint8_t * src_planes[3] = { src->p[Y_PLANE].p_pixels,
                                      src->p[U_PLANE].p_pixels,
                                      src->p[V_PLANE].p_pixels };
    const size_t    src_pitches[3] = { src->p[Y_PLANE].i_pitch,
                                       src->p[U_PLANE].i_pitch,
                                       src->p[V_PLANE].i_pitch };
    void *const     tmp[2] = { dest_pic->p[0].p_pixels,
                               dest_pic->p[1].p_pixels };

    dest_pic->p[0].p_pixels = dest_buf + dest_img->offsets[0];
    dest_pic->p[1].p_pixels = dest_buf + dest_img->offsets[1];
    dest_pic->p[0].i_pitch = dest_img->pitches[0];
    dest_pic->p[1].i_pitch = dest_img->pitches[1];

    switch (src->format.i_chroma)
    {
    case VLC_CODEC_I420:
        assert(dest_pic->format.i_chroma == VLC_CODEC_VAAPI_420);
        Copy420_P_to_SP(dest_pic, src_planes, src_pitches,
                        src->format.i_height, cache);

        break;
    case VLC_CODEC_I420_10L:
        assert(dest_pic->format.i_chroma == VLC_CODEC_VAAPI_420_10BPP);
        Copy420_16_P_to_SP(dest_pic, src_planes, src_pitches,
                           src->format.i_height, -6, cache);
        break;
    case VLC_CODEC_P010:
    {
        assert(dest_pic->format.i_chroma == VLC_CODEC_VAAPI_420_10BPP);
        Copy420_SP_to_SP(dest_pic,  src_planes, src_pitches,
                         src->format.i_height, cache);
        break;
    }
    default:
        vlc_assert_unreachable();
    }

    dest_pic->p[0].p_pixels = tmp[0];
    dest_pic->p[1].p_pixels = tmp[1];
}

static picture_t *
UploadSurface(filter_t *filter, picture_t *src)
{
    filter_sys_t   *p_sys = filter->p_sys;
    VADisplay const va_dpy = p_sys->dpy;
    VAImage         dest_img;
    void *          dest_buf;
    picture_t *     dest_pic = picture_pool_Wait(p_sys->dest_pics);

    if (!dest_pic)
    {
        msg_Err(filter, "cannot retrieve picture from the dest pics pool");
        goto ret;
    }
    vlc_vaapi_PicAttachContext(dest_pic);
    picture_CopyProperties(dest_pic, src);

    if (vlc_vaapi_DeriveImage(VLC_OBJECT(filter), va_dpy,
                              vlc_vaapi_PicGetSurface(dest_pic), &dest_img)
        || vlc_vaapi_MapBuffer(VLC_OBJECT(filter), va_dpy,
                               dest_img.buf, &dest_buf))
        goto error;

    FillVAImageFromPicture(&dest_img, dest_buf, dest_pic,
                           src, &p_sys->cache);

    if (vlc_vaapi_UnmapBuffer(VLC_OBJECT(filter), va_dpy, dest_img.buf)
        || vlc_vaapi_DestroyImage(VLC_OBJECT(filter),
                                  va_dpy, dest_img.image_id))
        goto error;

ret:
    picture_Release(src);
    return dest_pic;

error:
    picture_Release(dest_pic);
    dest_pic = NULL;
    goto ret;
}

static int CheckFmt(const video_format_t *in, const video_format_t *out,
                    bool *upload, uint8_t *pixel_bytes)
{
    *pixel_bytes = 1;
    *upload = false;
    switch (in->i_chroma)
    {
        case VLC_CODEC_VAAPI_420:
            if (out->i_chroma == VLC_CODEC_I420)
                return VLC_SUCCESS;
            break;
        case VLC_CODEC_VAAPI_420_10BPP:
            if (out->i_chroma == VLC_CODEC_P010
             || out->i_chroma == VLC_CODEC_I420_10L)
            {
                *pixel_bytes = 2;
                return VLC_SUCCESS;
            }
            break;
    }

    *upload = true;
    switch (out->i_chroma)
    {
        case VLC_CODEC_VAAPI_420:
            if (in->i_chroma == VLC_CODEC_I420)
                return VLC_SUCCESS;
            break;
        case VLC_CODEC_VAAPI_420_10BPP:
            if (in->i_chroma == VLC_CODEC_P010
             || in->i_chroma == VLC_CODEC_I420_10L)
            {
                *pixel_bytes = 2;
                return VLC_SUCCESS;
            }
            break;
    }
    return VLC_EGENERIC;
}

int
vlc_vaapi_OpenChroma(vlc_object_t *obj)
{
    filter_t *const     filter = (filter_t *)obj;
    filter_sys_t *      filter_sys;

    if (filter->fmt_in.video.i_height != filter->fmt_out.video.i_height
     || filter->fmt_in.video.i_width != filter->fmt_out.video.i_width
     || filter->fmt_in.video.orientation != filter->fmt_out.video.orientation)
        return VLC_EGENERIC;

    bool is_upload;
    uint8_t pixel_bytes;
    if (CheckFmt(&filter->fmt_in.video, &filter->fmt_out.video, &is_upload,
                 &pixel_bytes))
        return VLC_EGENERIC;

    filter->pf_video_filter = is_upload ? UploadSurface : DownloadSurface;

    if (!(filter_sys = calloc(1, sizeof(filter_sys_t))))
    {
        msg_Err(obj, "unable to allocate memory");
        return VLC_ENOMEM;
    }
    filter_sys->derive_failed = false;
    filter_sys->image_fallback_failed = false;
    if (is_upload)
    {
        vlc_decoder_device *dec_device = filter_HoldDecoderDeviceType( filter, VLC_DECODER_DEVICE_VAAPI );
        if (dec_device == NULL)
        {
            free(filter_sys);
            return VLC_EGENERIC;
        }

        filter->vctx_out = vlc_video_context_Create( dec_device, VLC_VIDEO_CONTEXT_VAAPI, 0, NULL );
        vlc_decoder_device_Release(dec_device);
        if (!filter->vctx_out)
        {
            free(filter_sys);
            return VLC_EGENERIC;
        }

        filter_sys->dpy = dec_device->opaque;

        filter_sys->dest_pics =
            vlc_vaapi_PoolNew(obj, filter->vctx_out, filter_sys->dpy,
                              DEST_PICS_POOL_SZ, &filter_sys->va_surface_ids,
                              &filter->fmt_out.video);
        if (!filter_sys->dest_pics)
        {
            vlc_video_context_Release(filter->vctx_out);
            filter->vctx_out = NULL;
            free(filter_sys);
            return VLC_EGENERIC;
        }
    }
    else
    {
        /* Don't fetch the vaapi instance since it may be not created yet at
         * this point (in case of cpu rendering) */
        filter_sys->dpy = NULL;
        filter_sys->dest_pics = NULL;
    }

    if (CopyInitCache(&filter_sys->cache, filter->fmt_in.video.i_width
                      * pixel_bytes))
    {
        if (is_upload)
        {
            picture_pool_Release(filter_sys->dest_pics);
            vlc_video_context_Release(filter->vctx_out);
            filter->vctx_out = NULL;
        }
        free(filter_sys);
        return VLC_EGENERIC;
    }

    filter->p_sys = filter_sys;
    msg_Warn(obj, "Using SW chroma filter for %dx%d %4.4s -> %4.4s",
             filter->fmt_in.video.i_width,
             filter->fmt_in.video.i_height,
             (const char *) &filter->fmt_in.video.i_chroma,
             (const char *) &filter->fmt_out.video.i_chroma);

    return VLC_SUCCESS;
}

void
vlc_vaapi_CloseChroma(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *const filter_sys = filter->p_sys;

    if (filter_sys->dest_pics)
        picture_pool_Release(filter_sys->dest_pics);
    CopyCleanCache(&filter_sys->cache);
    if (filter->vctx_out)
        vlc_video_context_Release(filter->vctx_out);

    free(filter_sys);
}
