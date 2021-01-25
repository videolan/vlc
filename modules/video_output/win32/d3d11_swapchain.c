/*****************************************************************************
 * d3d11_swapchain.c: Direct3D11 swapchain handled by the display module
 *****************************************************************************
 * Copyright (C) 2014-2019 VLC authors and VideoLAN
 *
 * Authors: Martell Malone <martellmalone@gmail.com>
 *          Steve Lhomme <robux4@gmail.com>
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

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#include <assert.h>

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601 // _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0601 // _WIN32_WINNT_WIN7
#endif

#include <windows.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d11.h>

#include "dxgi_swapchain.h"
#include "d3d11_swapchain.h"
#include "d3d11_shaders.h"

#ifdef HAVE_DCOMP_H
#  include "dcomp_wrapper.h"
#endif

struct d3d11_local_swapchain
{
    struct dxgi_swapchain *sys;

    vlc_object_t           *obj;
    d3d11_device_t         *d3d_dev;

    ID3D11RenderTargetView *swapchainTargetView[DXGI_MAX_RENDER_TARGET];
};

DEFINE_GUID(GUID_SWAPCHAIN_WIDTH,  0xf1b59347, 0x1643, 0x411a, 0xad, 0x6b, 0xc7, 0x80, 0x17, 0x7a, 0x06, 0xb6);
DEFINE_GUID(GUID_SWAPCHAIN_HEIGHT, 0x6ea976a0, 0x9d60, 0x4bb7, 0xa5, 0xa9, 0x7d, 0xd1, 0x18, 0x7f, 0xc9, 0xbd);

static bool UpdateSwapchain( struct d3d11_local_swapchain *display, const libvlc_video_render_cfg_t *cfg )
{
    HRESULT hr;

    D3D11_TEXTURE2D_DESC dsc = { 0 };
    uint8_t bitsPerChannel = 0;

    if ( display->swapchainTargetView[0] ) {
        ID3D11Resource *res = NULL;
        ID3D11RenderTargetView_GetResource( display->swapchainTargetView[0], &res );
        if ( res )
        {
            ID3D11Texture2D_GetDesc( (ID3D11Texture2D*) res, &dsc );
            ID3D11Resource_Release( res );
        }
        assert(DXGI_GetPixelFormat(display->sys)->formatTexture == dsc.Format);
        bitsPerChannel = DXGI_GetPixelFormat(display->sys)->bitsPerChannel;
    }

    if ( dsc.Width == cfg->width && dsc.Height == cfg->height && cfg->bitdepth <= bitsPerChannel )
        /* TODO also check the colorimetry */
        return true; /* nothing changed */

    for ( size_t i = 0; i < ARRAY_SIZE( display->swapchainTargetView ); i++ )
    {
        if ( display->swapchainTargetView[i] ) {
            ID3D11RenderTargetView_Release( display->swapchainTargetView[i] );
            display->swapchainTargetView[i] = NULL;
        }
    }

    const d3d_format_t *newPixelFormat = NULL;
#if VLC_WINSTORE_APP
    IDXGISwapChain1 *dxgiswapChain = DXGI_GetSwapChain1(display->sys);
    if (dxgiswapChain == NULL)
        dxgiswapChain = (void*)(uintptr_t)var_InheritInteger(display->obj, "winrt-swapchain");
    if (dxgiswapChain != NULL)
    {
        DXGI_SWAP_CHAIN_DESC1 scd;
        if (SUCCEEDED(IDXGISwapChain1_GetDesc1(dxgiswapChain, &scd)))
        {
            for (const d3d_format_t *output_format = DxgiGetRenderFormatList();
                 output_format->name != NULL; ++output_format)
            {
                if (output_format->formatTexture == scd.Format &&
                    !is_d3d11_opaque(output_format->fourcc))
                {
                    newPixelFormat = output_format;
                    break;
                }
            }
        }
    }
#else /* !VLC_WINSTORE_APP */
    /* favor RGB formats first */
    newPixelFormat = FindD3D11Format( display->obj, display->d3d_dev, 0, D3D11_RGB_FORMAT,
                                      cfg->bitdepth > 8 ? 10 : 8,
                                      0, 0,
                                      D3D11_CHROMA_CPU, D3D11_FORMAT_SUPPORT_DISPLAY );
    if (unlikely(newPixelFormat == NULL))
        newPixelFormat = FindD3D11Format( display->obj, display->d3d_dev, 0, D3D11_YUV_FORMAT,
                                          cfg->bitdepth > 8 ? 10 : 8,
                                          0, 0,
                                          D3D11_CHROMA_CPU, D3D11_FORMAT_SUPPORT_DISPLAY );
#endif /* !VLC_WINSTORE_APP */
    if (unlikely(newPixelFormat == NULL)) {
        msg_Err(display->obj, "Could not get the SwapChain format.");
        return false;
    }

    IDXGIDevice *pDXGIDevice = NULL;
    hr = ID3D11Device_QueryInterface(display->d3d_dev->d3ddevice, &IID_IDXGIDevice, (void **)&pDXGIDevice);
    if (FAILED(hr)) {
        return false;
    }
    IDXGIAdapter *dxgiadapter;
    hr = IDXGIDevice_GetAdapter(pDXGIDevice, &dxgiadapter);
    IDXGIDevice_Release(pDXGIDevice);
    if (FAILED(hr)) {
        return false;
    }

    if (!DXGI_UpdateSwapChain( display->sys, dxgiadapter, (IUnknown*) display->d3d_dev->d3ddevice, newPixelFormat, cfg ))
    {
        IDXGIAdapter_Release(dxgiadapter);
        return false;
    }
    IDXGIAdapter_Release(dxgiadapter);

    ID3D11Resource* pBackBuffer;
    hr = IDXGISwapChain1_GetBuffer( DXGI_GetSwapChain1(display->sys), 0, &IID_ID3D11Resource, (LPVOID *) &pBackBuffer );
    if ( FAILED( hr ) ) {
        msg_Err( display->obj, "Could not get the backbuffer for the Swapchain. (hr=0x%lX)", hr );
        return false;
    }

    hr = D3D11_CreateRenderTargets( display->d3d_dev, pBackBuffer,
                                    DXGI_GetPixelFormat(display->sys), display->swapchainTargetView );
    ID3D11Resource_Release( pBackBuffer );
    if ( FAILED( hr ) ) {
        msg_Err( display->obj, "Failed to create the target view. (hr=0x%lX)", hr );
        return false;
    }

    D3D11_ClearRenderTargets( display->d3d_dev, DXGI_GetPixelFormat(display->sys), display->swapchainTargetView );

    return true;
}

void D3D11_LocalSwapchainCleanupDevice( void *opaque )
{
    struct d3d11_local_swapchain *display = opaque;
    for (size_t i=0; i < ARRAY_SIZE(display->swapchainTargetView); i++)
    {
        if (display->swapchainTargetView[i]) {
            ID3D11RenderTargetView_Release(display->swapchainTargetView[i]);
            display->swapchainTargetView[i] = NULL;
        }
    }
    DXGI_LocalSwapchainCleanupDevice(display->sys);
}

bool D3D11_LocalSwapchainUpdateOutput( void *opaque, const libvlc_video_render_cfg_t *cfg, libvlc_video_output_cfg_t *out )
{
    struct d3d11_local_swapchain *display = opaque;
    if ( !UpdateSwapchain( display, cfg ) )
        return false;
    DXGI_SwapchainUpdateOutput(display->sys, out);
    return true;
}

void D3D11_LocalSwapchainSwap( void *opaque )
{
    struct d3d11_local_swapchain *display = opaque;
    DXGI_LocalSwapchainSwap( display->sys );
}

void D3D11_LocalSwapchainSetMetadata( void *opaque, libvlc_video_metadata_type_t type, const void *metadata )
{
    struct d3d11_local_swapchain *display = opaque;
    DXGI_LocalSwapchainSetMetadata( display->sys, type, metadata );
}

bool D3D11_LocalSwapchainWinstoreSize( void *opaque, uint32_t *width, uint32_t *height )
{
#if VLC_WINSTORE_APP
    struct d3d11_local_swapchain *display = opaque;
    /* legacy UWP mode, the width/height was set in GUID_SWAPCHAIN_WIDTH/HEIGHT */
    UINT dataSize = sizeof(*width);
    HRESULT hr = IDXGISwapChain1_GetPrivateData(DXGI_GetSwapChain1(display->sys), &GUID_SWAPCHAIN_WIDTH, &dataSize, width);
    if (SUCCEEDED(hr)) {
        dataSize = sizeof(*height);
        hr = IDXGISwapChain1_GetPrivateData(DXGI_GetSwapChain1(display->sys), &GUID_SWAPCHAIN_HEIGHT, &dataSize, height);
        return SUCCEEDED(hr);
    }
#else
    VLC_UNUSED(opaque); VLC_UNUSED(width); VLC_UNUSED(height);
#endif
    return false;
}

bool D3D11_LocalSwapchainStartEndRendering( void *opaque, bool enter )
{
    struct d3d11_local_swapchain *display = opaque;

    if ( enter )
        D3D11_ClearRenderTargets( display->d3d_dev, DXGI_GetPixelFormat(display->sys), display->swapchainTargetView );

    return true;
}

bool D3D11_LocalSwapchainSelectPlane( void *opaque, size_t plane )
{
    struct d3d11_local_swapchain *display = opaque;
    if (!display->swapchainTargetView[plane])
        return false;
    ID3D11DeviceContext_OMSetRenderTargets(display->d3d_dev->d3dcontext, 1,
                                            &display->swapchainTargetView[plane], NULL);
    return true;
}

void *D3D11_CreateLocalSwapchainHandleHwnd(vlc_object_t *o, HWND hwnd, d3d11_device_t *d3d_dev)
{
    struct d3d11_local_swapchain *display = vlc_obj_calloc(o, 1, sizeof(*display));
    if (unlikely(display == NULL))
        return NULL;

    display->sys = DXGI_CreateLocalSwapchainHandleHwnd(o, hwnd);
    if (unlikely(display->sys == NULL))
        return NULL;

    display->obj = o;
    display->d3d_dev = d3d_dev;

    return display;
}

#ifdef HAVE_DCOMP_H
void *D3D11_CreateLocalSwapchainHandleDComp(vlc_object_t *o, void* dcompDevice, void* dcompVisual, d3d11_device_t *d3d_dev)
{
    struct d3d11_local_swapchain *display = vlc_obj_calloc(o, 1, sizeof(*display));
    if (unlikely(display == NULL))
        return NULL;

    display->sys = DXGI_CreateLocalSwapchainHandleDComp(o, dcompDevice, dcompVisual);
    if (unlikely(display->sys == NULL))
        return NULL;

    display->obj = o;
    display->d3d_dev = d3d_dev;

    return display;
}
#endif
