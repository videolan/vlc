/*****************************************************************************
 * mft_d3d.cpp : Media Foundation Transform audio/video decoder D3D helpers
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

#ifndef _MSC_VER // including mfapi with mingw-w64 is not clean for UWP yet
#include <process.h>
#include <winapifamily.h>
#undef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mft_d3d.h"
#include <mfapi.h>
#include <mfidl.h>

#include <cassert>

#include <vlc_common.h>
#include <vlc_codec.h>

using Microsoft::WRL::ComPtr;

vlc_mf_d3d::~vlc_mf_d3d()
{
    assert(dxgi_manager.Get() == nullptr);
}

void vlc_mf_d3d::Init()
{
#if _WIN32_WINNT < _WIN32_WINNT_WIN8
    HINSTANCE mfplat_dll = LoadLibraryExA("mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (mfplat_dll)
    {
        fptr_MFCreateDXGIDeviceManager = reinterpret_cast<decltype(fptr_MFCreateDXGIDeviceManager)>(
            GetProcAddress(mfplat_dll, "MFCreateDXGIDeviceManager") );
        // we still have the DLL automatically loaded after this
        FreeLibrary(mfplat_dll);
    }
#endif // Win8+
}

bool vlc_mf_d3d::CanUseD3D() const
{
#if _WIN32_WINNT < _WIN32_WINNT_WIN8
    return fptr_MFCreateDXGIDeviceManager != nullptr;
#else
    return true;
#endif
}

HRESULT vlc_mf_d3d::SetD3D(vlc_logger *logger, IGraphicsUnknown *device, ComPtr<IMFTransform> & mft)
{
    if (!CanUseD3D())
    {
        vlc_warning(logger, "D3D not supported");
        return E_HANDLE;
    }

    HRESULT hr;
#if _WIN32_WINNT < _WIN32_WINNT_WIN8
    hr = fptr_MFCreateDXGIDeviceManager(&dxgi_token, &dxgi_manager);
#else
    hr = MFCreateDXGIDeviceManager(&dxgi_token, &dxgi_manager);
#endif
    if (FAILED(hr))
        return hr;

#if defined(_GAMING_XBOX) || defined(_XBOX_ONE)
    hr = MFResetDXGIDeviceManagerX(dxgi_manager.Get(), device, dxgi_token);
#else // !_GAMING_XBOX && !_XBOX_ONE
    hr = dxgi_manager->ResetDevice(device, dxgi_token);
#endif // !_GAMING_XBOX && !_XBOX_ONE
    if (FAILED(hr))
        goto error;

    hr = dxgi_manager->OpenDeviceHandle(&d3d_handle);
    if (FAILED(hr))
        goto error;

    hr = mft->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)dxgi_manager.Get());
    if (SUCCEEDED(hr))
        return S_OK;

    // dxgi_manager->TestDevice(d3d_handle);
    // hr = device->GetDeviceRemovedReason();

error:
    if (d3d_handle != INVALID_HANDLE_VALUE)
    {
        dxgi_manager->CloseDeviceHandle(d3d_handle);
        d3d_handle = INVALID_HANDLE_VALUE;
    }
    dxgi_manager.Reset();
    return hr;
}

void vlc_mf_d3d::ReleaseD3D(ComPtr<IMFTransform> & mft)
{
    if (dxgi_manager.Get())
    {
        if (mft.Get())
        {
            // mft->SetInputType(input_stream_id, nullptr, 0);
            // mft->SetOutputType(output_stream_id, nullptr, 0);

            mft->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)0);
        }

        if (d3d_handle != INVALID_HANDLE_VALUE)
        {
            dxgi_manager->CloseDeviceHandle(d3d_handle);
            d3d_handle = INVALID_HANDLE_VALUE;
        }
        dxgi_manager.Reset();
    }
}

MFHW_d3d::MFHW_d3d()
{
    d3d.Init();
}

HRESULT MFHW_d3d::SetD3D(vlc_logger *logger, vlc_decoder_device & dec_dev, IGraphicsUnknown *device, ComPtr<IMFTransform> & mft)
{
    HRESULT hr = d3d.SetD3D(logger, device, mft);
    if (SUCCEEDED(hr))
    {
        this->dec_dev = &dec_dev;
        vlc_decoder_device_Hold(this->dec_dev);
    }
    return hr;
}

void MFHW_d3d::Release(ComPtr<IMFTransform> & mft)
{
    d3d.ReleaseD3D(mft);
    if (vctx_out != nullptr)
    {
        vlc_video_context_Release(vctx_out);
        vctx_out = nullptr;
    }
    if (dec_dev != nullptr)
    {
        vlc_decoder_device_Release(dec_dev);
        dec_dev = nullptr;
    }
}
