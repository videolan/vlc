/*****************************************************************************
 * mft_d3d11.h : Media Foundation Transform audio/video decoder D3D11 helpers
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Author: Steve Lhomme <slhomme@videolabs.io>
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

#ifndef VLC_MFT_D3D11_H
#define VLC_MFT_D3D11_H

#include <vlc_common.h>
#include <vlc_codec.h>
#include "../video_chroma/d3d11_fmt.h"
#include "mft_d3d.h"

class MFHW_d3d11 : public MFHW_d3d
{
public:
    virtual ~MFHW_d3d11() = default;

    HRESULT SetD3D(vlc_logger *, vlc_decoder_device &, Microsoft::WRL::ComPtr<IMFTransform> &) final;
    void Release(Microsoft::WRL::ComPtr<IMFTransform> &) final;
    picture_context_t *CreatePicContext(struct vlc_logger *, Microsoft::WRL::ComPtr<IMFDXGIBuffer> &,
                                        vlc_mft_ref *) final;

    HRESULT SetupVideoContext(vlc_logger *, Microsoft::WRL::ComPtr<IMFDXGIBuffer> &, es_format_t & fmt_out) final;

private:
    const d3d_format_t *cfg = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> cached_tex;
    ID3D11ShaderResourceView *cachedSRV[32][DXGI_MAX_SHADER_VIEW] = {{nullptr}};
};

#endif // VLC_MFT_D3D11_H
