/*****************************************************************************
 * mft_d3d.h : Media Foundation Transform audio/video decoder D3D helpers
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

#ifndef VLC_MFT_D3D_H
#define VLC_MFT_D3D_H

#include <windows.h>

#include <wrl/client.h>

#include "../video_chroma/dxgi_fmt.h"

#if !defined(_GAMING_XBOX_SCARLETT) && !defined(_GAMING_XBOX_XBOXONE)
#define IGraphicsUnknown IUnknown
#endif // !_GAMING_XBOX_XBOXONE && !_GAMING_XBOX_XBOXONE

class IMFTransform;
class IMFDXGIDeviceManager;

struct vlc_logger;

class vlc_mf_d3d
{
public:
    ~vlc_mf_d3d();

    void Init();

    bool CanUseD3D() const;

    HRESULT SetD3D(vlc_logger *logger, IGraphicsUnknown *d3d_device, Microsoft::WRL::ComPtr<IMFTransform> &);

    void ReleaseD3D(Microsoft::WRL::ComPtr<IMFTransform> &);

private:
    UINT dxgi_token = 0;
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgi_manager;
    HANDLE d3d_handle = INVALID_HANDLE_VALUE;
#if _WIN32_WINNT < _WIN32_WINNT_WIN8
    HRESULT (WINAPI *fptr_MFCreateDXGIDeviceManager)(UINT *resetToken, IMFDXGIDeviceManager **ppDeviceManager) = nullptr;
#endif
};

#endif // VLC_MFT_D3D_H
