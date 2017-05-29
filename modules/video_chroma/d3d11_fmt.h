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

#include <d3d11.h>

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
    ID3D11VideoProcessorInputView *processorInput; /* when used as processor input */
    ID3D11ShaderResourceView      *resourceView[D3D11_MAX_SHADER_VIEW];
    DXGI_FORMAT                   formatTexture;
    void                          *va_surface;
};

/* index to use for texture/resource that use a known DXGI format
 * (ie not DXGI_FORMAT_UNKNWON) */
#define KNOWN_DXGI_INDEX   0

static inline void ReleasePictureSys(picture_sys_t *p_sys)
{
    for (int i=0; i<D3D11_MAX_SHADER_VIEW; i++) {
        if (p_sys->resourceView[i]) {
            ID3D11ShaderResourceView_Release(p_sys->resourceView[i]);
            p_sys->resourceView[i] = NULL;
        }
        if (p_sys->texture[i]) {
            ID3D11Texture2D_Release(p_sys->texture[i]);
            p_sys->texture[i] = NULL;
        }
    }
    if (p_sys->context) {
        ID3D11DeviceContext_Release(p_sys->context);
        p_sys->context = NULL;
    }
    if (p_sys->decoder) {
        ID3D11VideoDecoderOutputView_Release(p_sys->decoder);
        p_sys->decoder = NULL;
    }
    if (p_sys->processorInput) {
        ID3D11VideoProcessorInputView_Release(p_sys->processorInput);
        p_sys->processorInput = NULL;
    }
}

#endif /* include-guard */
