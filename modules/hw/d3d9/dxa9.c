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
    picture_sys_d3d9_t *p_sys = ActiveD3D9PictureSys(src);

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
    picture_sys_d3d9_t *p_sys = ActiveD3D9PictureSys(src);

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

static vlc_decoder_device * HoldD3D9DecoderDevice(vlc_object_t *o, void *sys)
{
    VLC_UNUSED(o);
    filter_t *p_this = sys;
    return filter_HoldDecoderDevice(p_this);
}

static filter_t *CreateFilter( filter_t *p_this, const es_format_t *p_fmt_in,
                               vlc_fourcc_t dst_chroma )
{
    filter_t *p_filter;

    p_filter = vlc_object_create( p_this, sizeof(filter_t) );
    if (unlikely(p_filter == NULL))
        return NULL;

    static const struct filter_video_callbacks cbs = { NewBuffer, HoldD3D9DecoderDevice };
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

static void YV12_D3D9(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = p_filter->p_sys;
    picture_sys_d3d9_t *p_dst_sys = ActiveD3D9PictureSys(dst);
    if (sys->filter == NULL)
    {
        D3DLOCKED_RECT d3drect;
        HRESULT hr = IDirect3DSurface9_LockRect(p_dst_sys->surface, &d3drect, NULL, 0);
        if (FAILED(hr))
            return;

        picture_context_t *dst_pic_ctx = dst->context;
        dst->context = NULL; // some CPU filters won't like the mix of CPU/GPU

        picture_UpdatePlanes(dst, d3drect.pBits, d3drect.Pitch);

        picture_CopyPixels(dst, src);

        dst->context = dst_pic_ctx;

        IDirect3DSurface9_UnlockRect(p_dst_sys->surface);
    }
    else
    {
        picture_sys_d3d9_t *p_staging_sys = ActiveD3D9PictureSys(sys->staging);

        D3DLOCKED_RECT d3drect;
        HRESULT hr = IDirect3DSurface9_LockRect(p_staging_sys->surface, &d3drect, NULL, 0);
        if (FAILED(hr))
            return;

        picture_UpdatePlanes(sys->staging, d3drect.pBits, d3drect.Pitch);
        picture_context_t *staging_pic_ctx = sys->staging->context;
        sys->staging->context = NULL; // some CPU filters won't like the mix of CPU/GPU

        picture_Hold( src );

        sys->filter->pf_video_filter(sys->filter, src);

        sys->staging->context = staging_pic_ctx;

        IDirect3DSurface9_UnlockRect(p_staging_sys->surface);

        d3d9_decoder_device_t *d3d9_decoder = GetD3D9OpaqueContext(p_filter->vctx_out);

        RECT visibleSource = {
            .right = dst->format.i_width, .bottom = dst->format.i_height,
        };
        IDirect3DDevice9_StretchRect( d3d9_decoder->d3ddev.dev,
                                    p_staging_sys->surface, &visibleSource,
                                    p_dst_sys->surface, &visibleSource,
                                    D3DTEXF_NONE );
    }
    // stop pretending this is a CPU picture
    dst->format.i_chroma = p_filter->fmt_out.video.i_chroma;
    dst->i_planes = 0;
}

static picture_t *AllocateCPUtoGPUTexture(filter_t *p_filter)
{
    IDirect3DSurface9 *texture = NULL;
    video_format_t fmt_staging;
    d3d9_decoder_device_t *devsys = GetD3D9OpaqueContext(p_filter->vctx_out);

    static const D3DFORMAT outputFormats8[] = {
        MAKEFOURCC('I','4','2','0'),
        MAKEFOURCC('Y','V','1','2'),
        MAKEFOURCC('N','V','1','2'),
        D3DFMT_UNKNOWN
    };
    static const D3DFORMAT outputFormats10[] = {
        MAKEFOURCC('P','0','1','0'),
        MAKEFOURCC('I','4','2','0'),
        MAKEFOURCC('Y','V','1','2'),
        MAKEFOURCC('N','V','1','2'),
        D3DFMT_UNKNOWN
    };

    D3DFORMAT format = D3DFMT_UNKNOWN;
    const D3DFORMAT *list;
    switch( p_filter->fmt_in.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
        list = outputFormats8;
        break;
    case VLC_CODEC_I420_10L:
    case VLC_CODEC_P010:
        list = outputFormats10;
        break;
    default:
        vlc_assert_unreachable();
    }
    while (*list != D3DFMT_UNKNOWN)
    {
        HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(devsys->d3ddev.dev,
                                                          p_filter->fmt_out.video.i_width,
                                                          p_filter->fmt_out.video.i_height,
                                                          *list,
                                                          D3DPOOL_DEFAULT,
                                                          &texture,
                                                          NULL);
        if (SUCCEEDED(hr)) {
            format = *list;
            msg_Dbg(p_filter, "using pixel format %4.4s", (char*)&format);
            break;
        }
        list++;
    }
    if (format == D3DFMT_UNKNOWN)
    {
        msg_Err(p_filter, "Failed to find a usable pixel format");
        goto done;
    }

    struct d3d9_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx == NULL))
        goto done;

    pic_ctx->s = (picture_context_t) {
        d3d9_pic_context_destroy, d3d9_pic_context_copy,
        vlc_video_context_Hold(p_filter->vctx_out),
    };

    video_format_Copy(&fmt_staging, &p_filter->fmt_out.video);
    fmt_staging.i_chroma = format;

    picture_resource_t dummy_res = {};
    picture_t *p_dst = picture_NewFromResource(&fmt_staging, &dummy_res);
    if (p_dst == NULL) {
        msg_Err(p_filter, "Failed to map create the temporary picture.");
        goto done;
    }
    pic_ctx->picsys.surface = texture;
    p_dst->context = &pic_ctx->s;
    return p_dst;

done:
    return NULL;
}

VIDEO_FILTER_WRAPPER (DXA9_YV12)
VIDEO_FILTER_WRAPPER (DXA9_NV12)

static picture_t *YV12_D3D9_Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic = AllocateCPUtoGPUTexture( p_filter );
    if( p_outpic )
    {
        YV12_D3D9( p_filter, p_pic, p_outpic );
        picture_CopyProperties( p_outpic, p_pic );
    }
    picture_Release( p_pic );
    return p_outpic;
}

int D3D9OpenConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;

    if ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE &&
         p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B )
        return VLC_EGENERIC;
    if ( GetD3D9ContextPrivate(p_filter->vctx_in) == NULL )
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

    p_filter->p_sys = p_sys;
    return VLC_SUCCESS;
}

int D3D9OpenCPUConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    int err = VLC_EGENERIC;
    picture_t *p_dst = NULL;

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

    vlc_decoder_device *dec_device = filter_HoldDecoderDeviceType( p_filter, VLC_DECODER_DEVICE_DXVA2 );
    if (dec_device == NULL)
    {
        msg_Err(p_filter, "Missing decoder device");
        return VLC_EGENERIC;
    }
    d3d9_decoder_device_t *devsys = GetD3D9OpaqueDevice(dec_device);
    if (devsys == NULL)
    {
        msg_Err(p_filter, "Incompatible decoder device %d", dec_device->type);
        vlc_decoder_device_Release(dec_device);
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = calloc(1, sizeof(filter_sys_t));
    if (!p_sys)
    {
        vlc_decoder_device_Release(dec_device);
        return VLC_ENOMEM;
    }

    p_filter->vctx_out = vlc_video_context_Create( dec_device, VLC_VIDEO_CONTEXT_DXVA2, sizeof(d3d9_video_context_t),
                                                   &d3d9_vctx_ops );
    vlc_decoder_device_Release(dec_device);

    if ( p_filter->vctx_out == NULL )
    {
        msg_Err(p_filter, "Failed to create the video context");
        goto done;
    }

    p_dst = AllocateCPUtoGPUTexture(p_filter);
    if (p_dst == NULL)
        goto done;
    d3d9_video_context_t *vctx_sys = GetD3D9ContextPrivate( p_filter->vctx_out );
    vctx_sys->format = p_dst->format.i_chroma;

    if ( p_filter->fmt_in.video.i_chroma != p_dst->format.i_chroma )
    {
        p_sys->filter = CreateFilter(p_filter, &p_filter->fmt_in, p_dst->format.i_chroma);
        if (!p_sys->filter)
            goto done;
        p_sys->staging = p_dst;
    }
    else
    {
        picture_Release(p_dst);
    }

    p_filter->p_sys = p_sys;
    err = VLC_SUCCESS;

done:
    if (err != VLC_SUCCESS)
    {
        vlc_video_context_Release(p_filter->vctx_out);
        free(p_sys);
    }
    return err;
}

void D3D9CloseConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;
    CopyCleanCache( &p_sys->cache );
    free( p_sys );
    p_filter->p_sys = NULL;
}

void D3D9CloseCPUConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;
    DeleteFilter(p_sys->filter);
    if (p_sys->staging)
        picture_Release(p_sys->staging);
    vlc_video_context_Release(p_filter->vctx_out);
    free( p_sys );
    p_filter->p_sys = NULL;
}
