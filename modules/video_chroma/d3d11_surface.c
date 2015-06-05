/*****************************************************************************
 * d3d11_surface.c : D3D11 GPU surface conversion module for vlc
 *****************************************************************************
 * Copyright © 2015 VLC authors, VideoLAN and VideoLabs
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
#include <vlc_plugin.h>
#include <vlc_filter.h>

#include "copy.h"

static int  OpenConverter( vlc_object_t * );
static void CloseConverter( vlc_object_t * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Conversions from D3D11 to YUV") )
    set_capability( "video filter2", 10 )
    set_callbacks( OpenConverter, CloseConverter )
vlc_module_end ()

#include <windows.h>
#define COBJMACROS
#include <d3d11.h>

/* VLC_CODEC_D3D11_OPAQUE */
struct picture_sys_t
{
    ID3D11VideoDecoderOutputView  *decoder; /* may be NULL for pictures from the pool */
    ID3D11Texture2D               *texture;
    ID3D11DeviceContext           *context;
    HINSTANCE                     hd3d11_dll; /* TODO */
};

struct filter_sys_t {
    copy_cache_t     cache;
    ID3D11Texture2D  *staging;
    vlc_mutex_t      staging_lock;
};

static int assert_staging(filter_t *p_filter, picture_sys_t *p_sys)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    HRESULT hr;

    if (sys->staging)
        goto ok;

    D3D11_TEXTURE2D_DESC texDesc;
    ID3D11Texture2D_GetDesc( p_sys->texture, &texDesc);

    texDesc.MipLevels = 1;
    //texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0;
    texDesc.ArraySize = 1;
    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    texDesc.BindFlags = 0;

    ID3D11Device *p_device;
    ID3D11DeviceContext_GetDevice(p_sys->context, &p_device);
    sys->staging = NULL;
    hr = ID3D11Device_CreateTexture2D( p_device, &texDesc, NULL, &sys->staging);
    ID3D11Device_Release(p_device);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to create a staging texture to extract surface pixels (hr=0x%0lx)", hr );
        return VLC_EGENERIC;
    }
ok:
    return VLC_SUCCESS;
}

static void D3D11_YUY2(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    picture_sys_t *p_sys = src->p_sys;

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;

    vlc_mutex_lock(&sys->staging_lock);
    if (assert_staging(p_filter, p_sys) != VLC_SUCCESS)
    {
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ID3D11VideoDecoderOutputView_GetDesc( p_sys->decoder, &viewDesc );

    ID3D11DeviceContext_CopySubresourceRegion(p_sys->context, (ID3D11Resource*) sys->staging,
                                              0, 0, 0, 0,
                                              (ID3D11Resource*) p_sys->texture, viewDesc.Texture2D.ArraySlice,
                                              NULL);

    HRESULT hr = ID3D11DeviceContext_Map(p_sys->context, (ID3D11Resource*) sys->staging,
                                         0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to map source surface. (hr=0x%0lx)", hr);
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    ID3D11Texture2D_GetDesc(sys->staging, &desc);

    if (desc.Format == DXGI_FORMAT_YUY2) {
        size_t chroma_pitch = (lock.RowPitch / 2);

        size_t pitch[3] = {
            lock.RowPitch,
            chroma_pitch,
            chroma_pitch,
        };

        uint8_t *plane[3] = {
            (uint8_t*)lock.pData,
            (uint8_t*)lock.pData + pitch[0] * src->format.i_height,
            (uint8_t*)lock.pData + pitch[0] * src->format.i_height
                                 + pitch[1] * src->format.i_height / 2,
        };

        CopyFromYv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_height, &sys->cache);
    } else if (desc.Format == DXGI_FORMAT_NV12) {
        uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * src->format.i_height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        CopyFromNv12(dst, plane, pitch, src->format.i_width,
                     src->format.i_height, &sys->cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to YV12", desc.Format);
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    /* */
    ID3D11DeviceContext_Unmap(p_sys->context, (ID3D11Resource*)sys->staging, 0);
    vlc_mutex_unlock(&sys->staging_lock);
}

static void D3D11_NV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    picture_sys_t *p_sys = src->p_sys;

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;

    vlc_mutex_lock(&sys->staging_lock);
    if (assert_staging(p_filter, p_sys) != VLC_SUCCESS)
    {
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ID3D11VideoDecoderOutputView_GetDesc( p_sys->decoder, &viewDesc );

    ID3D11DeviceContext_CopySubresourceRegion(p_sys->context, (ID3D11Resource*) sys->staging,
                                              0, 0, 0, 0,
                                              (ID3D11Resource*) p_sys->texture, viewDesc.Texture2D.ArraySlice,
                                              NULL);

    HRESULT hr = ID3D11DeviceContext_Map(p_sys->context, (ID3D11Resource*) sys->staging,
                                         0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to map source surface. (hr=0x%0lx)", hr);
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    ID3D11Texture2D_GetDesc(sys->staging, &desc);

    if (desc.Format == DXGI_FORMAT_NV12) {
        uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * src->format.i_height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        CopyFromNv12ToNv12(dst, plane, pitch, src->format.i_width,
                           src->format.i_height, &sys->cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to NV12", desc.Format);
    }

    /* */
    ID3D11DeviceContext_Unmap(p_sys->context, (ID3D11Resource*)sys->staging, 0);
    vlc_mutex_unlock(&sys->staging_lock);
}

VIDEO_FILTER_WRAPPER (D3D11_NV12)
VIDEO_FILTER_WRAPPER (D3D11_YUY2)

static int OpenConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    if ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    switch( p_filter->fmt_out.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
        p_filter->pf_video_filter = D3D11_YUY2_Filter;
        break;
    case VLC_CODEC_NV12:
        p_filter->pf_video_filter = D3D11_NV12_Filter;
        break;
    default:
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = calloc(1, sizeof(filter_sys_t));
    if (!p_sys)
         return VLC_ENOMEM;
    CopyInitCache(&p_sys->cache, p_filter->fmt_in.video.i_width );
    vlc_mutex_init(&p_sys->staging_lock);
    p_filter->p_sys = p_sys;

    return VLC_SUCCESS;
}

static void CloseConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = (filter_sys_t*) p_filter->p_sys;
    CopyCleanCache(&p_sys->cache);
    vlc_mutex_destroy(&p_sys->staging_lock);
    if (p_sys->staging)
        ID3D11Texture2D_Release(p_sys->staging);
    free( p_sys );
    p_filter->p_sys = NULL;
}
