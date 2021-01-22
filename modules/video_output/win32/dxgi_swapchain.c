/*****************************************************************************
 * dxgi_swapchain.c: DXGI swapchain handled by the display module
 *****************************************************************************
 * Copyright (C) 2014-2021 VLC authors and VideoLAN
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

#include <assert.h>

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601 // _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0601 // _WIN32_WINNT_WIN7
#endif

#include <vlc_es.h>

#define COBJMACROS

#ifdef HAVE_DCOMP_H
#  include "dcomp_wrapper.h"
#endif

#include <initguid.h>
#include "dxgi_swapchain.h"

#ifdef HAVE_DXGI1_6_H
# include <dxgi1_6.h>
#endif

#include "../../video_chroma/dxgi_fmt.h"

typedef enum video_color_axis {
    COLOR_AXIS_RGB,
    COLOR_AXIS_YCBCR,
} video_color_axis;

typedef enum swapchain_surface_type {
    SWAPCHAIN_SURFACE_HWND,
    SWAPCHAIN_SURFACE_DCOMP,
} swapchain_surface_type;

typedef struct {
    DXGI_COLOR_SPACE_TYPE   dxgi;
    const char              *name;
    video_color_axis        axis;
    video_color_primaries_t primaries;
    video_transfer_func_t   transfer;
    video_color_space_t     color;
    bool                    b_full_range;
} dxgi_color_space;

struct dxgi_swapchain
{
    vlc_object_t           *obj;

    const d3d_format_t     *pixelFormat;
    const dxgi_color_space *colorspace;

    swapchain_surface_type  swapchainSurfaceType;
    union {
#if !VLC_WINSTORE_APP
        HWND                hwnd;
#endif /* !VLC_WINSTORE_APP */
#ifdef HAVE_DCOMP_H
        struct {
            void*           device; // IDCompositionDevice
            void*           visual; // IDCompositionVisual
        } dcomp;
#endif // HAVE_DCOMP_H
    } swapchainSurface;

    IDXGISwapChain1        *dxgiswapChain;   /* DXGI 1.2 swap chain */
    IDXGISwapChain4        *dxgiswapChain4;  /* DXGI 1.5 for HDR metadata */
    bool                    send_metadata;
    DXGI_HDR_METADATA_HDR10 hdr10;

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

void DXGI_SelectSwapchainColorspace(struct dxgi_swapchain *display, const libvlc_video_render_cfg_t *cfg)
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
    display->send_metadata = color_spaces[best].transfer == (video_transfer_func_t) cfg->transfer &&
                             color_spaces[best].primaries == (video_color_primaries_t) cfg->primaries &&
                             color_spaces[best].color == (video_color_space_t) cfg->colorspace;
    if (dxgiswapChain3)
        IDXGISwapChain3_Release(dxgiswapChain3);
}

#if !VLC_WINSTORE_APP
static void FillSwapChainDesc(struct dxgi_swapchain *display, UINT width, UINT height, DXGI_SWAP_CHAIN_DESC1 *out)
{
    ZeroMemory(out, sizeof(*out));
    out->BufferCount = DXGI_SWAP_FRAME_COUNT;
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

static void DXGI_CreateSwapchainHwnd(struct dxgi_swapchain *display,
                              IDXGIAdapter *dxgiadapter, IUnknown *pFactoryDevice,
                              UINT width, UINT height)
{
    vlc_assert(display->swapchainSurfaceType == SWAPCHAIN_SURFACE_HWND);
    if (display->swapchainSurface.hwnd == NULL)
    {
        msg_Err(display->obj, "missing a HWND to create the swapchain");
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 scd;
    FillSwapChainDesc(display, width, height, &scd);

    IDXGIFactory2 *dxgifactory;
    HRESULT hr = IDXGIAdapter_GetParent(dxgiadapter, &IID_IDXGIFactory2, (void **)&dxgifactory);
    if (FAILED(hr)) {
        msg_Err(display->obj, "Could not get the DXGI Factory. (hr=0x%lX)", hr);
        return;
    }

    hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgifactory, pFactoryDevice,
                                              display->swapchainSurface.hwnd, &scd,
                                              NULL, NULL, &display->dxgiswapChain);

    if (hr == DXGI_ERROR_INVALID_CALL && scd.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
    {
        msg_Warn(display->obj, "10 bits swapchain failed, try 8 bits");
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgifactory, pFactoryDevice,
                                                  display->swapchainSurface.hwnd, &scd,
                                                  NULL, NULL, &display->dxgiswapChain);
    }
    IDXGIFactory2_Release(dxgifactory);
    if (FAILED(hr)) {
        msg_Err(display->obj, "Could not create the SwapChain. (hr=0x%lX)", hr);
    }
}

#ifdef HAVE_DCOMP_H
static void DXGI_CreateSwapchainDComp(struct dxgi_swapchain *display,
                               IDXGIAdapter *dxgiadapter, IUnknown *pFactoryDevice,
                               UINT width, UINT height)
{
    vlc_assert(display->swapchainSurfaceType == SWAPCHAIN_SURFACE_DCOMP);
    if (display->swapchainSurface.dcomp.device == NULL || display->swapchainSurface.dcomp.visual == NULL)
    {
        msg_Err(display->obj, "missing a HWND to create the swapchain");
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 scd;
    FillSwapChainDesc(display, width, height, &scd);
    ZeroMemory(&scd, sizeof(scd));
    scd.BufferCount = 3;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Width = width;
    scd.Height = height;
    scd.Format = display->pixelFormat->formatTexture;
    scd.Scaling = DXGI_SCALING_STRETCH;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    IDXGIFactory2 *dxgifactory;
    HRESULT hr = IDXGIAdapter_GetParent(dxgiadapter, &IID_IDXGIFactory2, (void **)&dxgifactory);
    if (FAILED(hr)) {
        msg_Err(display->obj, "Could not get the DXGI Factory. (hr=0x%lX)", hr);
        return;
    }

    hr = IDXGIFactory2_CreateSwapChainForComposition(dxgifactory, pFactoryDevice,
                                                    &scd, NULL, &display->dxgiswapChain);
    if (hr == DXGI_ERROR_INVALID_CALL && scd.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
    {
        msg_Warn(display->obj, "10 bits swapchain failed, try 8 bits");
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        hr = IDXGIFactory2_CreateSwapChainForComposition(dxgifactory, pFactoryDevice,
                                                        &scd, NULL, &display->dxgiswapChain);
    }
    IDXGIFactory2_Release(dxgifactory);
    if (SUCCEEDED(hr)) {
        IDCompositionVisual_SetContent(display->swapchainSurface.dcomp.visual, (IUnknown *)display->dxgiswapChain);
        IDCompositionDevice_Commit(display->swapchainSurface.dcomp.device);
    }
    if (FAILED(hr)) {
        msg_Err(display->obj, "Could not create the SwapChain. (hr=0x%lX)", hr);
    }
}
#endif /* HAVE_DCOMP_H */

#endif /* !VLC_WINSTORE_APP */

void DXGI_LocalSwapchainSwap( struct dxgi_swapchain *display )
{
    DXGI_PRESENT_PARAMETERS presentParams = { 0 };

    HRESULT hr = IDXGISwapChain1_Present1( display->dxgiswapChain, 0, 0, &presentParams );
    if ( hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET )
    {
        /* TODO device lost */
        msg_Err( display->obj, "SwapChain Present failed. (hr=0x%lX)", hr );
    }
}

void DXGI_LocalSwapchainSetMetadata( struct dxgi_swapchain *display, libvlc_video_metadata_type_t type, const void *metadata )
{
    assert(type == libvlc_video_metadata_frame_hdr10);
    if ( type == libvlc_video_metadata_frame_hdr10 && metadata &&
         display->send_metadata && display->dxgiswapChain4 )
    {
        const libvlc_video_frame_hdr10_metadata_t *p_hdr10 = metadata;
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
        if (memcmp(&display->hdr10, &hdr10, sizeof(hdr10)))
        {
            memcpy(&display->hdr10, &hdr10, sizeof(hdr10));
            IDXGISwapChain4_SetHDRMetaData( display->dxgiswapChain4, DXGI_HDR_METADATA_TYPE_HDR10,
                                            sizeof( &display->hdr10 ), &display->hdr10 );
        }
    }
}

struct dxgi_swapchain *DXGI_CreateLocalSwapchainHandleHwnd(vlc_object_t *o, HWND hwnd)
{
    struct dxgi_swapchain *display = vlc_obj_calloc(o, 1, sizeof(*display));
    if (unlikely(display == NULL))
        return NULL;

    display->obj = o;
#if !VLC_WINSTORE_APP
    display->swapchainSurfaceType = SWAPCHAIN_SURFACE_HWND;
    display->swapchainSurface.hwnd = hwnd;
#else // VLC_WINSTORE_APP
    VLC_UNUSED(hwnd);
#endif // VLC_WINSTORE_APP

    return display;
}

#ifdef HAVE_DCOMP_H
struct dxgi_swapchain *DXGI_CreateLocalSwapchainHandleDComp(vlc_object_t *o, void* dcompDevice, void* dcompVisual)
{
    struct dxgi_swapchain *display = vlc_obj_calloc(o, 1, sizeof(*display));
    if (unlikely(display == NULL))
        return NULL;

    display->obj = o;
    display->swapchainSurfaceType = SWAPCHAIN_SURFACE_DCOMP;
    display->swapchainSurface.dcomp.device = dcompDevice;
    display->swapchainSurface.dcomp.visual = dcompVisual;

    return display;
}
#endif

void DXGI_LocalSwapchainCleanupDevice( struct dxgi_swapchain *display )
{
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
}

void DXGI_SwapchainUpdateOutput( struct dxgi_swapchain *display, libvlc_video_output_cfg_t *out )
{
    out->dxgi_format    = display->pixelFormat->formatTexture;
    out->full_range     = display->colorspace->b_full_range;
    out->colorspace     = (libvlc_video_color_space_t)     display->colorspace->color;
    out->primaries      = (libvlc_video_color_primaries_t) display->colorspace->primaries;
    out->transfer       = (libvlc_video_transfer_func_t)   display->colorspace->transfer;
}

bool DXGI_UpdateSwapChain( struct dxgi_swapchain *display, IDXGIAdapter *dxgiadapter,
                           IUnknown *pFactoryDevice,
                           const d3d_format_t *newPixelFormat, const libvlc_video_render_cfg_t *cfg )
{
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

#ifdef HAVE_DCOMP_H
        if (display->swapchainSurfaceType == SWAPCHAIN_SURFACE_DCOMP)
            DXGI_CreateSwapchainDComp(display, dxgiadapter, pFactoryDevice,
                                      cfg->width, cfg->height);
        else // SWAPCHAIN_TARGET_HWND
#endif
            DXGI_CreateSwapchainHwnd(display, dxgiadapter, pFactoryDevice,
                                     cfg->width, cfg->height);

    }
#else /* VLC_WINSTORE_APP */
    if ( display->dxgiswapChain == NULL )
    {
        display->dxgiswapChain = (void*)(uintptr_t)var_InheritInteger(display->obj, "winrt-swapchain");
    }
#endif /* VLC_WINSTORE_APP */
    if (display->dxgiswapChain == NULL)
        return false;

    /* TODO detect is the size is the same as the output and switch to fullscreen mode */
    HRESULT hr;
    hr = IDXGISwapChain_ResizeBuffers( display->dxgiswapChain, 0, cfg->width, cfg->height,
                                        DXGI_FORMAT_UNKNOWN, 0 );
    if ( FAILED( hr ) ) {
        msg_Err( display->obj, "Failed to resize the backbuffer. (hr=0x%lX)", hr );
        return false;
    }

    DXGI_SelectSwapchainColorspace(display, cfg);
    return true;
}

IDXGISwapChain1 *DXGI_GetSwapChain1( struct dxgi_swapchain *display )
{
    return display->dxgiswapChain;
}

IDXGISwapChain4 *DXGI_GetSwapChain4( struct dxgi_swapchain *display )
{
    return display->dxgiswapChain4;
}

const d3d_format_t  *DXGI_GetPixelFormat( struct dxgi_swapchain *display )
{
    return display->pixelFormat;
}
