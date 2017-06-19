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
#include "vlc_vaapi.h"

# define DEST_PICS_POOL_SZ 3

struct filter_sys_t
{
    VADisplay           dpy;
    picture_pool_t *    dest_pics;
    VASurfaceID *       va_surface_ids;
    copy_cache_t        cache;

    bool                derive_failed;
    bool                image_fallback_failed;
    VAImage             image_fallback;
};

static int              Open(vlc_object_t *obj);
static void             Close(vlc_object_t *obj);

vlc_module_begin()
    set_shortname(N_("VAAPI"))
    set_description(N_("VAAPI surface conversions"))
    set_capability("video converter", 10)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callbacks(Open, Close)
vlc_module_end()

static int CreateFallbackImage(filter_t *filter, picture_t *src_pic)
{
    filter_sys_t *const filter_sys = filter->p_sys;
    VADisplay va_dpy = filter_sys->dpy;
    int count = vaMaxNumImageFormats(va_dpy);

    VAImageFormat *fmts = malloc(count * sizeof (*fmts));
    if (unlikely(fmts == NULL))
        return VLC_ENOMEM;

    if (vaQueryImageFormats(va_dpy, fmts, &count))
    {
        free(fmts);
        return VLC_EGENERIC;
    }

    int i;
    for (i = 0; i < count; i++)
        if (fmts[i].fourcc == VA_FOURCC_NV12)
            break;

    int ret;
    if (fmts[i].fourcc == VA_FOURCC_NV12
     && !vlc_vaapi_CreateImage(VLC_OBJECT(filter), va_dpy, &fmts[i],
                               src_pic->format.i_width, src_pic->format.i_height,
                               &filter_sys->image_fallback))
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
    switch (src_img->format.fourcc)
    {
    case VA_FOURCC_NV12:
    {
        uint8_t *       src_planes[2] = { src_buf + src_img->offsets[0],
                                          src_buf + src_img->offsets[1] };
        size_t          src_pitches[2] = { src_img->pitches[0],
                                           src_img->pitches[1] };

        CopyFromNv12ToI420(dest, src_planes, src_pitches,
                           src_img->height, cache);
        break;
    }
    /* TODO
     * case VA_FOURCC_P010:
     *    break;
     */
    default:
        break;
    }
}

static picture_t *
DownloadSurface(filter_t *filter, picture_t *src_pic)
{
    filter_sys_t *const filter_sys = filter->p_sys;
    VADisplay           va_dpy;
    VAImage             src_img;
    void *              src_buf;

    if (filter_sys->dpy == NULL)
    {
        /* Get the instance here since the instance may be not created by the
         * decoder from Open() */
        va_dpy = filter_sys->dpy = vlc_vaapi_GetInstance();
        assert(filter_sys->dpy);
    }
    else
        va_dpy = filter->p_sys->dpy;

    picture_t *dest = filter_NewPicture(filter);
    if (!dest)
    {
        msg_Err(filter, "filter_NewPicture failed");
        goto ret;
    }

    VASurfaceID surface = vlc_vaapi_PicGetSurface(src_pic);
    if (vaSyncSurface(va_dpy, surface))
        goto error;

    if (filter_sys->derive_failed ||
        vlc_vaapi_DeriveImage(VLC_OBJECT(filter), va_dpy, surface, &src_img))
    {
        if (filter_sys->image_fallback_failed)
            goto error;
        if (!filter_sys->derive_failed)
        {
            filter_sys->derive_failed = true;
            if (CreateFallbackImage(filter, src_pic))
            {
                filter_sys->image_fallback_failed = true;
                goto error;
            }
        }

        if (vaGetImage(va_dpy, surface, 0, 0, src_pic->format.i_width,
                       src_pic->format.i_height,
                       filter_sys->image_fallback.image_id))
        {
            filter_sys->image_fallback_failed = true;
            goto error;
        }
        src_img = filter_sys->image_fallback;
    }

    if (vlc_vaapi_MapBuffer(VLC_OBJECT(filter), va_dpy, src_img.buf, &src_buf))
        goto error;

    FillPictureFromVAImage(dest, &src_img, src_buf, &filter->p_sys->cache);

    vlc_vaapi_UnmapBuffer(VLC_OBJECT(filter), va_dpy, src_img.buf);
    if (src_img.image_id != filter_sys->image_fallback.image_id)
        vlc_vaapi_DestroyImage(VLC_OBJECT(filter), va_dpy, src_img.image_id);

    picture_CopyProperties(dest, src_pic);
ret:
    picture_Release(src_pic);
    return dest;

error:
    picture_Release(dest);
    dest = NULL;
    goto ret;
}

static inline void
FillVAImageFromPicture(VAImage *dest_img, uint8_t *dest_buf,
                       picture_t *dest_pic, picture_t *src,
                       copy_cache_t *cache)
{
    switch (src->format.i_chroma)
    {
    case VLC_CODEC_I420:
    {
        uint8_t *       src_planes[3] = { src->p[Y_PLANE].p_pixels,
                                          src->p[U_PLANE].p_pixels,
                                          src->p[V_PLANE].p_pixels };
        size_t          src_pitches[3] = { src->p[Y_PLANE].i_pitch,
                                           src->p[U_PLANE].i_pitch,
                                           src->p[V_PLANE].i_pitch };
        void *const     tmp[2] = { dest_pic->p[0].p_pixels,
                                   dest_pic->p[1].p_pixels };

        dest_pic->p[0].p_pixels = dest_buf + dest_img->offsets[0];
        dest_pic->p[1].p_pixels = dest_buf + dest_img->offsets[1];
        dest_pic->p[0].i_pitch = dest_img->pitches[0];
        dest_pic->p[1].i_pitch = dest_img->pitches[1];

        CopyFromI420ToNv12(dest_pic, src_planes, src_pitches,
                           src->format.i_height, cache);

        dest_pic->p[0].p_pixels = tmp[0];
        dest_pic->p[1].p_pixels = tmp[1];

        break;
    }
    case VLC_CODEC_I420_10L || VLC_CODEC_I420_10B:
        break;
    default:
        break;
    }
}

static picture_t *
UploadSurface(filter_t *filter, picture_t *src)
{
    VADisplay const va_dpy = filter->p_sys->dpy;
    VAImage         dest_img;
    void *          dest_buf;
    picture_t *     dest_pic = picture_pool_Wait(filter->p_sys->dest_pics);

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
                           src, &filter->p_sys->cache);

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

static int Open(vlc_object_t *obj)
{
    filter_t *const     filter = (filter_t *)obj;
    filter_sys_t *      filter_sys;
    bool                is_upload;

    if (filter->fmt_in.video.orientation != filter->fmt_out.video.orientation)
        return VLC_EGENERIC;

    if (filter->fmt_in.video.i_chroma == VLC_CODEC_VAAPI_420 &&
        (filter->fmt_out.video.i_chroma == VLC_CODEC_I420 ||
         filter->fmt_out.video.i_chroma == VLC_CODEC_I420_10L ||
         filter->fmt_out.video.i_chroma == VLC_CODEC_I420_10B))
    {
        is_upload = false;
        filter->pf_video_filter = DownloadSurface;
    }
    else if ((filter->fmt_in.video.i_chroma == VLC_CODEC_I420 ||
              filter->fmt_in.video.i_chroma == VLC_CODEC_I420_10L ||
              filter->fmt_in.video.i_chroma == VLC_CODEC_I420_10B) &&
             filter->fmt_out.video.i_chroma == VLC_CODEC_VAAPI_420)
    {
        is_upload = true;
        filter->pf_video_filter = UploadSurface;
    }
    else
        return VLC_EGENERIC;

    if (!(filter_sys = calloc(1, sizeof(filter_sys_t))))
    {
        msg_Err(obj, "unable to allocate memory");
        return VLC_ENOMEM;
    }
    filter_sys->derive_failed = false;
    filter_sys->image_fallback_failed = false;
    filter_sys->image_fallback.image_id = VA_INVALID_ID;

    if (is_upload)
    {
        filter_sys->dpy = vlc_vaapi_GetInstance();
        if (filter_sys->dpy == NULL)
        {
            free(filter_sys);
            return VLC_EGENERIC;
        }

        filter_sys->dest_pics =
            vlc_vaapi_PoolNew(obj, filter_sys->dpy, DEST_PICS_POOL_SZ,
                              &filter_sys->va_surface_ids,
                              &filter->fmt_out.video, VA_RT_FORMAT_YUV420,
                              VA_FOURCC_NV12);
        if (!filter_sys->dest_pics)
        {
            vlc_vaapi_ReleaseInstance(filter_sys->dpy);
            free(filter_sys);
            return VLC_EGENERIC;
        }
    }
    else
    {
        /* Don't fetch the vaapi instance since it may be not created yet at
         * this point (in case of cpu rendering) */
        filter_sys->dest_pics = NULL;
    }

    if (CopyInitCache(&filter_sys->cache, filter->fmt_in.video.i_width))
    {
        if (is_upload)
        {
            picture_pool_Release(filter_sys->dest_pics);
            vlc_vaapi_ReleaseInstance(filter_sys->dpy);
        }
        free(filter_sys);
        return VLC_EGENERIC;
    }

    filter->p_sys = filter_sys;

    return VLC_SUCCESS;
}

static void
Close(vlc_object_t *obj)
{
    filter_sys_t *const filter_sys = ((filter_t *)obj)->p_sys;

    if (filter_sys->image_fallback.image_id != VA_INVALID_ID)
        vlc_vaapi_DestroyImage(obj, filter_sys->dpy,
                               filter_sys->image_fallback.image_id);
    if (filter_sys->dest_pics)
        picture_pool_Release(filter_sys->dest_pics);
    if (filter_sys->dpy != NULL)
        vlc_vaapi_ReleaseInstance(filter_sys->dpy);
    CopyCleanCache(&filter_sys->cache);

    free(filter_sys);
}
