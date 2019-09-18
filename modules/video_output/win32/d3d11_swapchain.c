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

#include <windows.h>
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif

#define COBJMACROS
#include <initguid.h>
#include <d3d11.h>
#ifdef HAVE_DXGI1_6_H
# include <dxgi1_6.h>
#else
# include <dxgi1_5.h>
#endif

#include "d3d11_swapchain.h"
#include "d3d11_shaders.h"
#include "../../video_chroma/d3d9_fmt.h"

typedef enum video_color_axis {
    COLOR_AXIS_RGB,
    COLOR_AXIS_YCBCR,
} video_color_axis;

typedef struct {
    DXGI_COLOR_SPACE_TYPE   dxgi;
    const char              *name;
    video_color_axis        axis;
    video_color_primaries_t primaries;
    video_transfer_func_t   transfer;
    video_color_space_t     color;
    bool                    b_full_range;
} dxgi_color_space;

struct d3d11_local_swapchain
{
    vlc_object_t           *obj;
    d3d11_device_t         d3d_dev;

    const d3d_format_t     *pixelFormat;
    const dxgi_color_space *colorspace;

#if !VLC_WINSTORE_APP
    HWND                   swapchainHwnd;
#endif /* !VLC_WINSTORE_APP */
    IDXGISwapChain1        *dxgiswapChain;   /* DXGI 1.2 swap chain */
    IDXGISwapChain4        *dxgiswapChain4;  /* DXGI 1.5 for HDR metadata */

    ID3D11RenderTargetView *swapchainTargetView[D3D11_MAX_RENDER_TARGET];

    bool                   logged_capabilities;
};

DEFINE_GUID(GUID_SWAPCHAIN_WIDTH,  0xf1b59347, 0x1643, 0x411a, 0xad, 0x6b, 0xc7, 0x80, 0x17, 0x7a, 0x06, 0xb6);
DEFINE_GUID(GUID_SWAPCHAIN_HEIGHT, 0x6ea976a0, 0x9d60, 0x4bb7, 0xa5, 0xa9, 0x7d, 0xd1, 0x18, 0x7f, 0xc9, 0xbd);

#define DXGI_COLOR_RANGE_FULL   1 /* 0-255 */
#define DXGI_COLOR_RANGE_STUDIO 0 /* 16-235 */

#define TRANSFER_FUNC_10    TRANSFER_FUNC_LINEAR
#define TRANSFER_FUNC_22    TRANSFER_FUNC_SRGB
#define TRANSFER_FUNC_2084  TRANSFER_FUNC_SMPTE_ST2084

#define COLOR_PRIMARIES_BT601  COLOR_PRIMARIES_BT601_525

static const dxgi_color_space color_spaces[] = {
#define DXGIMAP(AXIS, RANGE, GAMMA, SITTING, PRIMARIES) \
    { DXGI_COLOR_SPACE_##AXIS##_##RANGE##_G##GAMMA##_##SITTING##_P##PRIMARIES, \
      #AXIS " Rec." #PRIMARIES " gamma:" #GAMMA " range:" #RANGE, \
      COLOR_AXIS_##AXIS, COLOR_PRIMARIES_BT##PRIMARIES, TRANSFER_FUNC_##GAMMA, \
      COLOR_SPACE_BT##PRIMARIES, DXGI_COLOR_RANGE_##RANGE},

    DXGIMAP(RGB,   FULL,     22,    NONE,   709)
    DXGIMAP(YCBCR, STUDIO,   22,    LEFT,   601)
    DXGIMAP(YCBCR, FULL,     22,    LEFT,   601)
    DXGIMAP(RGB,   FULL,     10,    NONE,   709)
    DXGIMAP(RGB,   STUDIO,   22,    NONE,   709)
    DXGIMAP(YCBCR, STUDIO,   22,    LEFT,   709)
    DXGIMAP(YCBCR, FULL,     22,    LEFT,   709)
    DXGIMAP(RGB,   STUDIO,   22,    NONE,  2020)
    DXGIMAP(YCBCR, STUDIO,   22,    LEFT,  2020)
    DXGIMAP(YCBCR, FULL,     22,    LEFT,  2020)
    DXGIMAP(YCBCR, STUDIO,   22, TOPLEFT,  2020)
    DXGIMAP(RGB,   FULL,     22,    NONE,  2020)
    DXGIMAP(RGB,   FULL,   2084,    NONE,  2020)
    DXGIMAP(YCBCR, STUDIO, 2084,    LEFT,  2020)
    DXGIMAP(RGB,   STUDIO, 2084,    NONE,  2020)
    DXGIMAP(YCBCR, STUDIO, 2084, TOPLEFT,  2020)
    DXGIMAP(YCBCR, STUDIO,  HLG, TOPLEFT,  2020)
    DXGIMAP(YCBCR, FULL,    HLG, TOPLEFT,  2020)
    /*DXGIMAP(YCBCR, FULL,     22,    NONE,  2020, 601)*/
    {DXGI_COLOR_SPACE_RESERVED, NULL, 0, 0, 0, 0, 0},
#undef DXGIMAP
};

#ifdef HAVE_DXGI1_6_H
static bool canHandleConversion(const dxgi_color_space *src, const dxgi_color_space *dst)
{
    if (src == dst)
        return true;
    if (src->primaries == COLOR_PRIMARIES_BT2020)
        return true; /* we can convert BT2020 to 2020 or 709 */
    if (dst->transfer == TRANSFER_FUNC_BT709)
        return true; /* we can handle anything to 709 */
    return false; /* let Windows do the rest */
}
#endif

static void SelectSwapchainColorspace(struct d3d11_local_swapchain *display, const libvlc_video_direct3d_cfg_t *cfg)
{
    HRESULT hr;
    int best = 0;
    int score, best_score = 0;
    UINT support;
    IDXGISwapChain3 *dxgiswapChain3 = NULL;
    hr = IDXGISwapChain_QueryInterface( display->dxgiswapChain, &IID_IDXGISwapChain3, (void **)&dxgiswapChain3);
    if (FAILED(hr)) {
        msg_Warn(display->obj, "could not get a IDXGISwapChain3");
        goto done;
    }

    /* pick the best output based on color support and transfer */
    /* TODO support YUV output later */
    best = -1;
    for (int i=0; color_spaces[i].name; ++i)
    {
        hr = IDXGISwapChain3_CheckColorSpaceSupport(dxgiswapChain3, color_spaces[i].dxgi, &support);
        if (SUCCEEDED(hr) && support) {
            if (!display->logged_capabilities)
                msg_Dbg(display->obj, "supports colorspace %s", color_spaces[i].name);
            score = 0;
            if (color_spaces[i].primaries == (video_color_primaries_t) cfg->primaries)
                score++;
            if (color_spaces[i].color == (video_color_space_t) cfg->colorspace)
                score += 2; /* we don't want to translate color spaces */
            if (color_spaces[i].transfer == (video_transfer_func_t) cfg->transfer ||
                /* favor 2084 output for HLG source */
                (color_spaces[i].transfer == TRANSFER_FUNC_SMPTE_ST2084 && cfg->transfer == TRANSFER_FUNC_HLG))
                score++;
            if (color_spaces[i].b_full_range == cfg->full_range)
                score++;
            if (score > best_score || (score && best == -1)) {
                best = i;
                best_score = score;
            }
        }
    }
    display->logged_capabilities = true;

    if (best == -1)
    {
        best = 0;
        msg_Warn(display->obj, "no matching colorspace found force %s", color_spaces[best].name);
    }

    IDXGISwapChain_QueryInterface( display->dxgiswapChain, &IID_IDXGISwapChain4, (void **)&display->dxgiswapChain4);

#ifdef HAVE_DXGI1_6_H
    IDXGIOutput *dxgiOutput = NULL;

    if (SUCCEEDED(IDXGISwapChain_GetContainingOutput( display->dxgiswapChain, &dxgiOutput )))
    {
        IDXGIOutput6 *dxgiOutput6 = NULL;
        if (SUCCEEDED(IDXGIOutput_QueryInterface( dxgiOutput, &IID_IDXGIOutput6, (void **)&dxgiOutput6 )))
        {
            DXGI_OUTPUT_DESC1 desc1;
            if (SUCCEEDED(IDXGIOutput6_GetDesc1( dxgiOutput6, &desc1 )))
            {
                const dxgi_color_space *csp = NULL;
                for (int i=0; color_spaces[i].name; ++i)
                {
                    if (color_spaces[i].dxgi == desc1.ColorSpace)
                    {
                        if (!canHandleConversion(&color_spaces[best], &color_spaces[i]))
                            msg_Warn(display->obj, "Can't handle conversion to screen format %s", color_spaces[i].name);
                        else
                        {
                            best = i;
                            csp = &color_spaces[i];
                        }
                        break;
                    }
                }

                msg_Dbg(display->obj, "Output max luminance: %.1f, colorspace %s, bits per pixel %d", desc1.MaxFullFrameLuminance, csp?csp->name:"unknown", desc1.BitsPerColor);
                //sys->display.luminance_peak = desc1.MaxFullFrameLuminance;
            }
            IDXGIOutput6_Release( dxgiOutput6 );
        }
        IDXGIOutput_Release( dxgiOutput );
    }
#endif

    hr = IDXGISwapChain3_SetColorSpace1(dxgiswapChain3, color_spaces[best].dxgi);
    if (SUCCEEDED(hr))
        msg_Dbg(display->obj, "using colorspace %s", color_spaces[best].name);
    else
        msg_Err(display->obj, "Failed to set colorspace %s. (hr=0x%lX)", color_spaces[best].name, hr);
done:
    display->colorspace = &color_spaces[best];
    if (dxgiswapChain3)
        IDXGISwapChain3_Release(dxgiswapChain3);
}

#if !VLC_WINSTORE_APP
static void FillSwapChainDesc(struct d3d11_local_swapchain *display, UINT width, UINT height, DXGI_SWAP_CHAIN_DESC1 *out)
{
    ZeroMemory(out, sizeof(*out));
    out->BufferCount = 3;
    out->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    out->SampleDesc.Count = 1;
    out->SampleDesc.Quality = 0;
    out->Width = width;
    out->Height = height;
    out->Format = display->pixelFormat->formatTexture;
    //out->Flags = 512; // DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO;

    bool isWin10OrGreater = false;
    HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
    if (likely(hKernel32 != NULL))
        isWin10OrGreater = GetProcAddress(hKernel32, "GetSystemCpuSetInformation") != NULL;
    if (isWin10OrGreater)
        out->SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    else
    {
        bool isWin80OrGreater = false;
        if (likely(hKernel32 != NULL))
            isWin80OrGreater = GetProcAddress(hKernel32, "CheckTokenCapability") != NULL;
        if (isWin80OrGreater)
            out->SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        else
        {
            out->SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            out->BufferCount = 1;
        }
    }
}

static void CreateSwapchain(struct d3d11_local_swapchain *display, UINT width, UINT height)
{
    if (display->swapchainHwnd == NULL)
    {
        msg_Err(display->obj, "missing a HWND to create the swapchain");
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 scd;
    FillSwapChainDesc(display, width, height, &scd);

    IDXGIAdapter *dxgiadapter = D3D11DeviceAdapter(display->d3d_dev.d3ddevice);
    if (unlikely(dxgiadapter==NULL)) {
        msg_Err(display->obj, "Could not get the DXGI Adapter");
        return;
    }

    IDXGIFactory2 *dxgifactory;
    HRESULT hr = IDXGIAdapter_GetParent(dxgiadapter, &IID_IDXGIFactory2, (void **)&dxgifactory);
    IDXGIAdapter_Release(dxgiadapter);
    if (FAILED(hr)) {
        msg_Err(display->obj, "Could not get the DXGI Factory. (hr=0x%lX)", hr);
        return;
    }

    hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgifactory, (IUnknown *)display->d3d_dev.d3ddevice,
                                              display->swapchainHwnd, &scd,
                                              NULL, NULL, &display->dxgiswapChain);
    if (hr == DXGI_ERROR_INVALID_CALL && scd.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
    {
        msg_Warn(display->obj, "10 bits swapchain failed, try 8 bits");
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgifactory, (IUnknown *)display->d3d_dev.d3ddevice,
                                                  display->swapchainHwnd, &scd,
                                                  NULL, NULL, &display->dxgiswapChain);
    }
    IDXGIFactory2_Release(dxgifactory);
    if (FAILED(hr)) {
        msg_Err(display->obj, "Could not create the SwapChain. (hr=0x%lX)", hr);
    }
}
#endif /* !VLC_WINSTORE_APP */

static bool UpdateSwapchain( struct d3d11_local_swapchain *display, const libvlc_video_direct3d_cfg_t *cfg )
{
    ID3D11Texture2D* pBackBuffer;
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
        assert(display->pixelFormat->formatTexture == dsc.Format);
        bitsPerChannel = display->pixelFormat->bitsPerChannel;
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
    display->dxgiswapChain = var_InheritInteger(display->obj, "winrt-swapchain");
    if (display->dxgiswapChain != NULL)
    {
        DXGI_SWAP_CHAIN_DESC1 scd;
        if (SUCCEEDED(IDXGISwapChain1_GetDesc(display->dxgiswapChain, &scd)))
        {
            for (const d3d_format_t *output_format = GetRenderFormatList();
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
    newPixelFormat = FindD3D11Format( display->obj, &display->d3d_dev, 0, true,
                                      cfg->bitdepth > 8 ? 10 : 8,
                                      0, 0,
                                      false, D3D11_FORMAT_SUPPORT_DISPLAY );
    if (unlikely(newPixelFormat == NULL))
        newPixelFormat = FindD3D11Format( display->obj, &display->d3d_dev, 0, false,
                                          cfg->bitdepth > 8 ? 10 : 8,
                                          0, 0,
                                          false, D3D11_FORMAT_SUPPORT_DISPLAY );
#endif /* !VLC_WINSTORE_APP */
    if (unlikely(newPixelFormat == NULL)) {
        msg_Err(display->obj, "Could not get the SwapChain format.");
        return false;
    }

#if !VLC_WINSTORE_APP
    if (display->dxgiswapChain != NULL && display->pixelFormat != newPixelFormat)
    {
        // the pixel format changed, we need a new swapchain
        IDXGISwapChain_Release(display->dxgiswapChain);
        display->dxgiswapChain = NULL;
        display->logged_capabilities = false;
    }

    if ( display->dxgiswapChain == NULL )
    {
        display->pixelFormat = newPixelFormat;
        CreateSwapchain(display, cfg->width, cfg->height);

        if (display->dxgiswapChain == NULL)
            return false;
    }
    else
#endif /* !VLC_WINSTORE_APP */
    {
        /* TODO detect is the size is the same as the output and switch to fullscreen mode */
        hr = IDXGISwapChain_ResizeBuffers( display->dxgiswapChain, 0, cfg->width, cfg->height,
                                           DXGI_FORMAT_UNKNOWN, 0 );
        if ( FAILED( hr ) ) {
            msg_Err( display->obj, "Failed to resize the backbuffer. (hr=0x%lX)", hr );
            return false;
        }
    }

    hr = IDXGISwapChain_GetBuffer( display->dxgiswapChain, 0, &IID_ID3D11Texture2D, (LPVOID *) &pBackBuffer );
    if ( FAILED( hr ) ) {
        msg_Err( display->obj, "Could not get the backbuffer for the Swapchain. (hr=0x%lX)", hr );
        return false;
    }

    hr = D3D11_CreateRenderTargets( &display->d3d_dev, (ID3D11Resource *) pBackBuffer,
                                    display->pixelFormat, display->swapchainTargetView );
    ID3D11Texture2D_Release( pBackBuffer );
    if ( FAILED( hr ) ) {
        msg_Err( display->obj, "Failed to create the target view. (hr=0x%lX)", hr );
        return false;
    }

    D3D11_ClearRenderTargets( &display->d3d_dev, display->pixelFormat, display->swapchainTargetView );

    SelectSwapchainColorspace(display, cfg);

    return true;
}

void LocalSwapchainCleanupDevice( void *opaque )
{
    struct d3d11_local_swapchain *display = opaque;
    for (size_t i=0; i < ARRAY_SIZE(display->swapchainTargetView); i++)
    {
        if (display->swapchainTargetView[i]) {
            ID3D11RenderTargetView_Release(display->swapchainTargetView[i]);
            display->swapchainTargetView[i] = NULL;
        }
    }
    if (display->dxgiswapChain4)
    {
        IDXGISwapChain4_Release(display->dxgiswapChain4);
        display->dxgiswapChain4 = NULL;
    }
    if (display->dxgiswapChain)
    {
        IDXGISwapChain_Release(display->dxgiswapChain);
        display->dxgiswapChain = NULL;
    }

    D3D11_ReleaseDevice( &display->d3d_dev );
}

void LocalSwapchainSwap( void *opaque )
{
    struct d3d11_local_swapchain *display = opaque;
    DXGI_PRESENT_PARAMETERS presentParams = { 0 };

    HRESULT hr = IDXGISwapChain1_Present1( display->dxgiswapChain, 0, 0, &presentParams );
    if ( hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET )
    {
        /* TODO device lost */
        msg_Err( display->obj, "SwapChain Present failed. (hr=0x%lX)", hr );
    }
}

bool LocalSwapchainUpdateOutput( void *opaque, const libvlc_video_direct3d_cfg_t *cfg, libvlc_video_output_cfg_t *out )
{
    struct d3d11_local_swapchain *display = opaque;
    if ( !UpdateSwapchain( display, cfg ) )
        return false;
    out->surface_format = display->pixelFormat->formatTexture;
    out->full_range     = display->colorspace->b_full_range;
    out->colorspace     = (libvlc_video_color_space_t)     display->colorspace->color;
    out->primaries      = (libvlc_video_color_primaries_t) display->colorspace->primaries;
    out->transfer       = (libvlc_video_transfer_func_t)   display->colorspace->transfer;
    return true;
}

bool LocalSwapchainStartEndRendering( void *opaque, bool enter, const libvlc_video_direct3d_hdr10_metadata_t *p_hdr10 )
{
    struct d3d11_local_swapchain *display = opaque;

    if ( enter )
    {
#if VLC_WINSTORE_APP
        /* legacy UWP mode, the width/height was set in GUID_SWAPCHAIN_WIDTH/HEIGHT */
        uint32_t i_width;
        uint32_t i_height;
        UINT dataSize = sizeof(i_width);
        HRESULT hr = IDXGISwapChain_GetPrivateData(display->dxgiswapChain, &GUID_SWAPCHAIN_WIDTH, &dataSize, &i_width);
        if (SUCCEEDED(hr)) {
            dataSize = sizeof(i_height);
            hr = IDXGISwapChain_GetPrivateData(display->dxgiswapChain, &GUID_SWAPCHAIN_HEIGHT, &dataSize, &i_height);
            if (SUCCEEDED(hr)) {
                if (i_width != sys->area.vdcfg.display.width || i_height != sys->area.vdcfg.display.height)
                {
                    vout_display_SetSize(vd, i_width, i_height);
                }
            }
        }
#endif

        if ( display->dxgiswapChain4 && p_hdr10 != NULL )
        {
            DXGI_HDR_METADATA_HDR10 hdr10 = { 0 };
            hdr10.GreenPrimary[0] = p_hdr10->GreenPrimary[0];
            hdr10.GreenPrimary[1] = p_hdr10->GreenPrimary[1];
            hdr10.BluePrimary[0] = p_hdr10->BluePrimary[0];
            hdr10.BluePrimary[1] = p_hdr10->BluePrimary[1];
            hdr10.RedPrimary[0] = p_hdr10->RedPrimary[0];
            hdr10.RedPrimary[1] = p_hdr10->RedPrimary[1];
            hdr10.WhitePoint[0] = p_hdr10->WhitePoint[0];
            hdr10.WhitePoint[1] = p_hdr10->WhitePoint[1];
            hdr10.MinMasteringLuminance = p_hdr10->MinMasteringLuminance;
            hdr10.MaxMasteringLuminance = p_hdr10->MaxMasteringLuminance;
            hdr10.MaxContentLightLevel = p_hdr10->MaxContentLightLevel;
            hdr10.MaxFrameAverageLightLevel = p_hdr10->MaxFrameAverageLightLevel;
            IDXGISwapChain4_SetHDRMetaData( display->dxgiswapChain4, DXGI_HDR_METADATA_TYPE_HDR10, sizeof( hdr10 ), &hdr10 );
        }

        D3D11_ClearRenderTargets( &display->d3d_dev, display->pixelFormat, display->swapchainTargetView );
    }
    return true;
}

bool LocalSwapchainSelectPlane( void *opaque, size_t plane )
{
    struct d3d11_local_swapchain *display = opaque;
    if (!display->swapchainTargetView[plane])
        return false;
    ID3D11DeviceContext_OMSetRenderTargets(display->d3d_dev.d3dcontext, 1,
                                            &display->swapchainTargetView[plane], NULL);
    return true;
}

void *CreateLocalSwapchainHandle(vlc_object_t *o, HWND hwnd, ID3D11DeviceContext *d3d11_ctx)
{
    struct d3d11_local_swapchain *display = vlc_obj_calloc(o, 1, sizeof(*display));
    if (unlikely(display == NULL))
        return NULL;

    display->obj = o;
#if !VLC_WINSTORE_APP
    display->swapchainHwnd = hwnd;
#endif /* !VLC_WINSTORE_APP */
    D3D11_CreateDeviceExternal( o, d3d11_ctx, false /* is_d3d11_opaque(vd->source.i_chroma) */, &display->d3d_dev);

    return display;
}
