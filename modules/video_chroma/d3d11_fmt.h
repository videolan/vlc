/*****************************************************************************
 * d3d11_fmt.h : D3D11 helper calls
 *****************************************************************************
 * Copyright © 2017 VLC authors, VideoLAN and VideoLabs
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

#ifndef VLC_VIDEOCHROMA_D3D11_FMT_H_
#define VLC_VIDEOCHROMA_D3D11_FMT_H_

#include <vlc_picture.h>

#include <d3d11.h>
#include <assert.h>

#include "dxgi_fmt.h"

DEFINE_GUID(GUID_CONTEXT_MUTEX, 0x472e8835, 0x3f8e, 0x4f93, 0xa0, 0xcb, 0x25, 0x79, 0x77, 0x6c, 0xed, 0x86);

/* owned by the vout for VLC_CODEC_D3D11_OPAQUE */
struct picture_sys_t
{
    ID3D11VideoDecoderOutputView  *decoder; /* may be NULL for pictures from the pool */
    union {
        ID3D11Texture2D           *texture[D3D11_MAX_SHADER_VIEW];
        ID3D11Resource            *resource[D3D11_MAX_SHADER_VIEW];
    };
    ID3D11DeviceContext           *context;
    unsigned                      slice_index;
    ID3D11VideoProcessorInputView  *processorInput;  /* when used as processor input */
    ID3D11VideoProcessorOutputView *processorOutput; /* when used as processor output */
    ID3D11ShaderResourceView      *resourceView[D3D11_MAX_SHADER_VIEW];
    DXGI_FORMAT                   formatTexture;
    bool                          mapped;
};

#include "../codec/avcodec/va_surface.h"

static inline picture_sys_t *ActivePictureSys(picture_t *p_pic)
{
    struct va_pic_context *pic_ctx = (struct va_pic_context*)p_pic->context;
    return pic_ctx ? &pic_ctx->picsys : p_pic->p_sys;
}

/* index to use for texture/resource that use a known DXGI format
 * (ie not DXGI_FORMAT_UNKNWON) */
#define KNOWN_DXGI_INDEX   0

static inline void AcquirePictureSys(picture_sys_t *p_sys)
{
    for (int i=0; i<D3D11_MAX_SHADER_VIEW; i++) {
        if (p_sys->resourceView[i])
            ID3D11ShaderResourceView_AddRef(p_sys->resourceView[i]);
        if (p_sys->texture[i])
            ID3D11Texture2D_AddRef(p_sys->texture[i]);
    }
    if (p_sys->context)
        ID3D11DeviceContext_AddRef(p_sys->context);
    if (p_sys->decoder)
        ID3D11VideoDecoderOutputView_AddRef(p_sys->decoder);
    if (p_sys->processorInput)
        ID3D11VideoProcessorInputView_AddRef(p_sys->processorInput);
    if (p_sys->processorOutput)
        ID3D11VideoProcessorOutputView_AddRef(p_sys->processorOutput);
}

static inline void ReleasePictureSys(picture_sys_t *p_sys)
{
    for (int i=0; i<D3D11_MAX_SHADER_VIEW; i++) {
        if (p_sys->resourceView[i])
            ID3D11ShaderResourceView_Release(p_sys->resourceView[i]);
        if (p_sys->texture[i])
            ID3D11Texture2D_Release(p_sys->texture[i]);
    }
    if (p_sys->context)
        ID3D11DeviceContext_Release(p_sys->context);
    if (p_sys->decoder)
        ID3D11VideoDecoderOutputView_Release(p_sys->decoder);
    if (p_sys->processorInput)
        ID3D11VideoProcessorInputView_Release(p_sys->processorInput);
    if (p_sys->processorOutput)
        ID3D11VideoProcessorOutputView_Release(p_sys->processorOutput);
}

/* map texture planes to resource views */
static inline int AllocateShaderView(vlc_object_t *obj, ID3D11Device *d3ddevice,
                              const d3d_format_t *format,
                              ID3D11Texture2D *p_texture[D3D11_MAX_SHADER_VIEW], UINT slice_index,
                              ID3D11ShaderResourceView *resourceView[D3D11_MAX_SHADER_VIEW])
{
    HRESULT hr;
    int i;
    D3D11_SHADER_RESOURCE_VIEW_DESC resviewDesc = { 0 };
    D3D11_TEXTURE2D_DESC texDesc;
    ID3D11Texture2D_GetDesc(p_texture[0], &texDesc);
    assert(texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE);

    if (texDesc.ArraySize == 1)
    {
        resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        resviewDesc.Texture2D.MipLevels = 1;
    }
    else
    {
        resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        resviewDesc.Texture2DArray.MipLevels = -1;
        resviewDesc.Texture2DArray.ArraySize = 1;
        resviewDesc.Texture2DArray.FirstArraySlice = slice_index;
    }
    for (i=0; i<D3D11_MAX_SHADER_VIEW; i++)
    {
        resviewDesc.Format = format->resourceFormat[i];
        if (resviewDesc.Format == DXGI_FORMAT_UNKNOWN)
            resourceView[i] = NULL;
        else
        {
            hr = ID3D11Device_CreateShaderResourceView(d3ddevice, (ID3D11Resource*)p_texture[i], &resviewDesc, &resourceView[i]);
            if (FAILED(hr)) {
                msg_Err(obj, "Could not Create the Texture ResourceView %d slice %d. (hr=0x%lX)", i, slice_index, hr);
                break;
            }
        }
    }

    if (i != D3D11_MAX_SHADER_VIEW)
    {
        while (--i >= 0)
        {
            ID3D11ShaderResourceView_Release(resourceView[i]);
            resourceView[i] = NULL;
        }
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#endif /* include-guard */
