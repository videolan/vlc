/*****************************************************************************
 * dxa9.c : DXVA2 GPU surface conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2015 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_modules.h>

#include "d3d9_filters.h"

#include "../../video_chroma/copy.h"

#include <windows.h>
#include <d3d9.h>
#include "../../video_chroma/d3d9_fmt.h"

typedef struct
{
    /* GPU to CPU */
    copy_cache_t      cache;

    /* CPU to GPU */
    d3d9_handle_t     hd3d;
    d3d9_device_t     d3d_dev;
    filter_t          *filter;
    picture_t         *staging;
} filter_sys_t;

static bool GetLock(filter_t *p_filter, IDirect3DSurface9 *d3d,
                    D3DLOCKED_RECT *p_lock, D3DSURFACE_DESC *p_desc)
{
    if (unlikely(FAILED( IDirect3DSurface9_GetDesc(d3d, p_desc))))
        return false;

    /* */
    if (FAILED(IDirect3DSurface9_LockRect(d3d, p_lock, NULL, D3DLOCK_READONLY))) {
        msg_Err(p_filter, "Failed to lock surface");
        return false;
    }

    return true;
}

static void DXA9_YV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;
    picture_sys_t *p_sys = &((struct va_pic_context *)src->context)->picsys;

    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lock;
    if (!GetLock(p_filter, p_sys->surface, &lock, &desc))
        return;

    if (desc.Format == MAKEFOURCC('Y','V','1','2') ||
        desc.Format == MAKEFOURCC('I','M','C','3')) {

        if (dst->format.i_chroma == VLC_CODEC_I420)
            picture_SwapUV( dst );

        bool imc3 = desc.Format == MAKEFOURCC('I','M','C','3');
        size_t chroma_pitch = imc3 ? lock.Pitch : (lock.Pitch / 2);

        const size_t pitch[3] = {
            lock.Pitch,
            chroma_pitch,
            chroma_pitch,
        };

        const uint8_t *plane[3] = {
            (uint8_t*)lock.pBits,
            (uint8_t*)lock.pBits + pitch[0] * desc.Height,
            (uint8_t*)lock.pBits + pitch[0] * desc.Height
                                 + pitch[1] * desc.Height / 2,
        };

        if (imc3) {
            const uint8_t *V = plane[1];
            plane[1] = plane[2];
            plane[2] = V;
        }
        Copy420_P_to_P(dst, plane, pitch, src->format.i_height, p_copy_cache);

        if (dst->format.i_chroma == VLC_CODEC_I420)
            picture_SwapUV( dst );
    } else if (desc.Format == MAKEFOURCC('N','V','1','2')
            || desc.Format == MAKEFOURCC('P','0','1','0')) {
        const uint8_t *plane[2] = {
            lock.pBits,
            (uint8_t*)lock.pBits + lock.Pitch * desc.Height
        };
        const size_t  pitch[2] = {
            lock.Pitch,
            lock.Pitch,
        };
        if (desc.Format == MAKEFOURCC('N','V','1','2'))
            Copy420_SP_to_P(dst, plane, pitch,
                            __MIN(desc.Height, src->format.i_y_offset + src->format.i_visible_height),
                            p_copy_cache);
        else
            Copy420_16_SP_to_P(dst, plane, pitch,
                              __MIN(desc.Height, src->format.i_y_offset + src->format.i_visible_height),
                              6, p_copy_cache);

        if (dst->format.i_chroma != VLC_CODEC_I420 && dst->format.i_chroma != VLC_CODEC_I420_10L)
            picture_SwapUV(dst);
    } else {
        msg_Err(p_filter, "Unsupported DXA9 conversion from 0x%08X to YV12", desc.Format);
    }

    /* */
    IDirect3DSurface9_UnlockRect(p_sys->surface);
}

static void DXA9_NV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;
    picture_sys_t *p_sys = &((struct va_pic_context *)src->context)->picsys;

    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lock;
    if (!GetLock(p_filter, p_sys->surface, &lock, &desc))
        return;

    if (desc.Format == MAKEFOURCC('N','V','1','2')
     || desc.Format == MAKEFOURCC('P','0','1','0')) {
        const uint8_t *plane[2] = {
            lock.pBits,
            (uint8_t*)lock.pBits + lock.Pitch * desc.Height
        };
        size_t  pitch[2] = {
            lock.Pitch,
            lock.Pitch,
        };
        Copy420_SP_to_SP(dst, plane, pitch,
                         __MIN(desc.Height, src->format.i_y_offset + src->format.i_visible_height),
                         p_copy_cache);
    } else {
        msg_Err(p_filter, "Unsupported DXA9 conversion from 0x%08X to NV12", desc.Format);
    }

    /* */
    IDirect3DSurface9_UnlockRect(p_sys->surface);
}

static void DestroyPicture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    ReleasePictureSys( p_sys );
    free(p_sys);
}

static void DeleteFilter( filter_t * p_filter )
{
    if( p_filter->p_module )
        module_unneed( p_filter, p_filter->p_module );

    es_format_Clean( &p_filter->fmt_in );
    es_format_Clean( &p_filter->fmt_out );

    vlc_object_delete(p_filter);
}

static picture_t *NewBuffer(filter_t *p_filter)
{
    filter_t *p_parent = p_filter->owner.sys;
    filter_sys_t *p_sys = p_parent->p_sys;
    return p_sys->staging;
}

static filter_t *CreateFilter( vlc_object_t *p_this, const es_format_t *p_fmt_in,
                               vlc_fourcc_t dst_chroma )
{
    filter_t *p_filter;

    p_filter = vlc_object_create( p_this, sizeof(filter_t) );
    if (unlikely(p_filter == NULL))
        return NULL;

    static const struct filter_video_callbacks cbs = { NewBuffer };
    p_filter->b_allow_fmt_out_change = false;
    p_filter->owner.video = &cbs;
    p_filter->owner.sys = p_this;

    es_format_InitFromVideo( &p_filter->fmt_in,  &p_fmt_in->video );
    es_format_InitFromVideo( &p_filter->fmt_out, &p_fmt_in->video );
    p_filter->fmt_out.i_codec = p_filter->fmt_out.video.i_chroma = dst_chroma;
    p_filter->p_module = module_need( p_filter, "video converter", NULL, false );

    if( !p_filter->p_module )
    {
        msg_Dbg( p_filter, "no video converter found" );
        DeleteFilter( p_filter );
        return NULL;
    }

    return p_filter;
}

struct d3d_pic_context
{
    picture_context_t s;
};

static void d3d9_pic_context_destroy(struct picture_context_t *ctx)
{
    struct va_pic_context *pic_ctx = (struct va_pic_context*)ctx;
    ReleasePictureSys(&pic_ctx->picsys);
    free(pic_ctx);
}

static struct picture_context_t *d3d9_pic_context_copy(struct picture_context_t *ctx)
{
    struct va_pic_context *src_ctx = (struct va_pic_context*)ctx;
    struct va_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx==NULL))
        return NULL;
    pic_ctx->s.destroy = d3d9_pic_context_destroy;
    pic_ctx->s.copy    = d3d9_pic_context_copy;
    pic_ctx->picsys = src_ctx->picsys;
    AcquirePictureSys(&pic_ctx->picsys);
    return &pic_ctx->s;
}

static void YV12_D3D9(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = p_filter->p_sys;
    picture_sys_t *p_sys = dst->p_sys;
    picture_sys_t *p_staging_sys = sys->staging->p_sys;

    D3DSURFACE_DESC texDesc;
    IDirect3DSurface9_GetDesc( p_sys->surface, &texDesc);

    D3DLOCKED_RECT d3drect;
    HRESULT hr = IDirect3DSurface9_LockRect(p_staging_sys->surface, &d3drect, NULL, 0);
    if (FAILED(hr))
        return;

    picture_UpdatePlanes(sys->staging, d3drect.pBits, d3drect.Pitch);

    picture_Hold( src );

    sys->filter->pf_video_filter(sys->filter, src);

    IDirect3DSurface9_UnlockRect(p_staging_sys->surface);

    RECT visibleSource = {
        .right = dst->format.i_width, .bottom = dst->format.i_height,
    };
    IDirect3DDevice9_StretchRect( sys->d3d_dev.dev,
                                  p_staging_sys->surface, &visibleSource,
                                  p_sys->surface, &visibleSource,
                                  D3DTEXF_NONE );

    if (dst->context == NULL)
    {
        struct va_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
        if (likely(pic_ctx))
        {
            pic_ctx->s.destroy = d3d9_pic_context_destroy;
            pic_ctx->s.copy    = d3d9_pic_context_copy;
            pic_ctx->picsys = *p_sys;
            AcquirePictureSys(&pic_ctx->picsys);
            dst->context = &pic_ctx->s;
        }
    }
}

VIDEO_FILTER_WRAPPER (DXA9_YV12)
VIDEO_FILTER_WRAPPER (DXA9_NV12)
VIDEO_FILTER_WRAPPER (YV12_D3D9)

int D3D9OpenConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;

    if ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE &&
         p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    uint8_t pixel_bytes = 1;
    switch( p_filter->fmt_out.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = DXA9_YV12_Filter;
        break;
    case VLC_CODEC_I420_10L:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = DXA9_YV12_Filter;
        pixel_bytes = 2;
        break;
    case VLC_CODEC_NV12:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = DXA9_NV12_Filter;
        break;
    case VLC_CODEC_P010:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = DXA9_NV12_Filter;
        pixel_bytes = 2;
        break;
    default:
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = calloc(1, sizeof(filter_sys_t));
    if (!p_sys)
         return VLC_ENOMEM;

    if (CopyInitCache(&p_sys->cache, p_filter->fmt_in.video.i_width * pixel_bytes))
    {
        free(p_sys);
        return VLC_ENOMEM;
    }

    if (unlikely(D3D9_Create( p_filter, &p_sys->hd3d ) != VLC_SUCCESS)) {
        msg_Warn(p_filter, "cannot load d3d9.dll, aborting");
        CopyCleanCache(&p_sys->cache);
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_filter->p_sys = p_sys;
    return VLC_SUCCESS;
}

int D3D9OpenCPUConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    int err = VLC_EGENERIC;
    IDirect3DSurface9 *texture = NULL;
    filter_t *p_cpu_filter = NULL;
    picture_t *p_dst = NULL;
    video_format_t fmt_staging;

    if ( p_filter->fmt_out.video.i_chroma != VLC_CODEC_D3D9_OPAQUE
      && p_filter->fmt_out.video.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    switch( p_filter->fmt_in.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
    case VLC_CODEC_I420_10L:
    case VLC_CODEC_P010:
        p_filter->pf_video_filter = YV12_D3D9_Filter;
        break;
    default:
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = calloc(1, sizeof(filter_sys_t));
    if (!p_sys)
         return VLC_ENOMEM;

    video_format_Init(&fmt_staging, 0);
    if (unlikely(D3D9_Create( p_filter, &p_sys->hd3d ) != VLC_SUCCESS)) {
        msg_Warn(p_filter, "cannot load d3d9.dll, aborting");
        free(p_sys);
        return VLC_EGENERIC;
    }

    D3DSURFACE_DESC texDesc;
    D3D9_FilterHoldInstance(p_filter, &p_sys->d3d_dev, &texDesc);
    if (!p_sys->d3d_dev.dev)
    {
        msg_Dbg(p_filter, "Filter without a context");
        goto done;
    }
    if (texDesc.Format == 0)
        goto done;

    if ( p_filter->fmt_in.video.i_chroma != texDesc.Format )
    {
        picture_resource_t res;
        res.pf_destroy = DestroyPicture;
        picture_sys_t *res_sys = calloc(1, sizeof(picture_sys_t));
        if (res_sys == NULL) {
            err = VLC_ENOMEM;
            goto done;
        }
        res.p_sys = res_sys;

        video_format_Copy(&fmt_staging, &p_filter->fmt_out.video);
        fmt_staging.i_chroma = texDesc.Format;
        fmt_staging.i_height = texDesc.Height;
        fmt_staging.i_width  = texDesc.Width;

        p_dst = picture_NewFromResource(&fmt_staging, &res);
        if (p_dst == NULL) {
            msg_Err(p_filter, "Failed to map create the temporary picture.");
            goto done;
        }
        picture_Setup(p_dst, &p_dst->format);

        HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(p_sys->d3d_dev.dev,
                                                          p_dst->format.i_width,
                                                          p_dst->format.i_height,
                                                          texDesc.Format,
                                                          D3DPOOL_DEFAULT,
                                                          &texture,
                                                          NULL);
        if (FAILED(hr)) {
            msg_Err(p_filter, "Failed to create a %4.4s staging texture to extract surface pixels (hr=0x%0lx)", (char *)texDesc.Format, hr );
            goto done;
        }
        res_sys->surface = texture;
        IDirect3DSurface9_AddRef(texture);

        p_cpu_filter = CreateFilter(VLC_OBJECT(p_filter), &p_filter->fmt_in, p_dst->format.i_chroma);
        if (!p_cpu_filter)
            goto done;
    }

    p_sys->filter    = p_cpu_filter;
    p_sys->staging   = p_dst;
    p_filter->p_sys = p_sys;
    err = VLC_SUCCESS;

done:
    video_format_Clean(&fmt_staging);
    if (err != VLC_SUCCESS)
    {
        if (texture)
            IDirect3DSurface9_Release(texture);
        D3D9_FilterReleaseInstance(&p_sys->d3d_dev);
        D3D9_Destroy( &p_sys->hd3d );
        free(p_sys);
    }
    return err;
}

void D3D9CloseConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;
    CopyCleanCache( &p_sys->cache );
    D3D9_Destroy( &p_sys->hd3d );
    free( p_sys );
    p_filter->p_sys = NULL;
}

void D3D9CloseCPUConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;
    DeleteFilter(p_sys->filter);
    picture_Release(p_sys->staging);
    D3D9_FilterReleaseInstance(&p_sys->d3d_dev);
    D3D9_Destroy( &p_sys->hd3d );
    free( p_sys );
    p_filter->p_sys = NULL;
}
