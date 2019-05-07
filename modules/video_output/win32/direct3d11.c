/*****************************************************************************
 * direct3d11.c: Windows Direct3D11 video output module
 *****************************************************************************
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include <assert.h>
#include <math.h>

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

/* avoided until we can pass ISwapchainPanel without c++/cx mode
# include <windows.ui.xaml.media.dxinterop.h> */

#include "../../video_chroma/d3d11_fmt.h"
#include "d3d11_quad.h"
#include "d3d11_shaders.h"
#include "d3d_render.h"

#include "common.h"
#include "../video_chroma/copy.h"

DEFINE_GUID(GUID_SWAPCHAIN_WIDTH,  0xf1b59347, 0x1643, 0x411a, 0xad, 0x6b, 0xc7, 0x80, 0x17, 0x7a, 0x06, 0xb6);
DEFINE_GUID(GUID_SWAPCHAIN_HEIGHT, 0x6ea976a0, 0x9d60, 0x4bb7, 0xa5, 0xa9, 0x7d, 0xd1, 0x18, 0x7f, 0xc9, 0xbd);

static int  Open(vout_display_t *, const vout_display_cfg_t *,
                 video_format_t *, vlc_video_context *);
static void Close(vout_display_t *);

#define D3D11_HELP N_("Recommended video output for Windows 8 and later versions")
#define HW_BLENDING_TEXT N_("Use hardware blending support")
#define HW_BLENDING_LONGTEXT N_(\
    "Try to use hardware acceleration for subtitle/OSD blending.")

vlc_module_begin ()
    set_shortname("Direct3D11")
    set_description(N_("Direct3D11 video output"))
    set_help(D3D11_HELP)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_bool("direct3d11-hw-blending", true, HW_BLENDING_TEXT, HW_BLENDING_LONGTEXT, true)

#if VLC_WINSTORE_APP
    add_integer("winrt-d3dcontext",    0x0, NULL, NULL, true); /* ID3D11DeviceContext* */
    add_integer("winrt-swapchain",     0x0, NULL, NULL, true); /* IDXGISwapChain1*     */
#endif

    set_capability("vout display", 300)
    add_shortcut("direct3d11")
    set_callbacks(Open, Close)
vlc_module_end ()

struct d3d11_local_swapchain
{
    vlc_object_t           *obj;
    d3d11_device_t         d3d_dev;

    HWND                   swapchainHwnd;
    IDXGISwapChain1        *dxgiswapChain;   /* DXGI 1.2 swap chain */
    IDXGISwapChain4        *dxgiswapChain4;  /* DXGI 1.5 for HDR metadata */

    ID3D11RenderTargetView *swapchainTargetView[D3D11_MAX_RENDER_TARGET];
};

struct vout_display_sys_t
{
    vout_display_sys_win32_t sys;       /* only use if sys.event is not NULL */
    display_win32_area_t     area;

    /* Sensors */
    void *p_sensors;

    display_info_t           display;

    d3d11_handle_t           hd3d;
    d3d11_device_t           d3d_dev;
    d3d_quad_t               picQuad;

    ID3D11Query              *prepareWait;

    picture_sys_t            stagingSys;
    picture_pool_t           *pool; /* hardware decoding pool */

    struct d3d11_local_swapchain internal_swapchain; /* TODO do not access fields directly */

    d3d_vshader_t            projectionVShader;
    d3d_vshader_t            flatVShader;

    /* copy from the decoder pool into picSquad before display
     * Uses a Texture2D with slices rather than a Texture2DArray for the decoder */
    bool                     legacy_shader;

    // SPU
    vlc_fourcc_t             pSubpictureChromas[2];
    d3d_quad_t               regionQuad;
    int                      d3dregion_count;
    picture_t                **d3dregions;

    /* outside rendering */
    void *outside_opaque;
    d3d_device_setup_cb    setupDeviceCb;
    d3d_device_cleanup_cb  cleanupDeviceCb;
    d3d_update_output_cb   updateOutputCb;
    d3d_swap_cb            swapCb;
    d3d_start_end_rendering_cb startEndRenderingCb;
};

static picture_pool_t *Pool(vout_display_t *, unsigned);

static void Prepare(vout_display_t *, picture_t *, subpicture_t *subpicture, vlc_tick_t);
static void Display(vout_display_t *, picture_t *);

static void Direct3D11Destroy(vout_display_t *);

static int  Direct3D11Open (vout_display_t *, video_format_t *);
static void Direct3D11Close(vout_display_t *);

static int SetupOutputFormat(vout_display_t *, video_format_t *);
static int  Direct3D11CreateFormatResources (vout_display_t *, const video_format_t *);
static int  Direct3D11CreateGenericResources(vout_display_t *);
static void Direct3D11DestroyResources(vout_display_t *);

static void Direct3D11DestroyPool(vout_display_t *);

static void DestroyDisplayPoolPicture(picture_t *);
static void Direct3D11DeleteRegions(int, picture_t **);
static int Direct3D11MapSubpicture(vout_display_t *, int *, picture_t ***, subpicture_t *);

static void SetQuadVSProjection(vout_display_t *, d3d_quad_t *, const vlc_viewpoint_t *);
static void UpdatePicQuadPosition(vout_display_t *);

static int Control(vout_display_t *, int, va_list);

static HRESULT UpdateBackBuffer(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    const struct direct3d_cfg_t cfg = {
        .width  = sys->area.vdcfg.display.width,
        .height = sys->area.vdcfg.display.height
    };
    struct output_cfg_t out;
    if (!sys->updateOutputCb( sys->outside_opaque, &cfg, &out ))
        return E_FAIL;

    return S_OK;
}

static void UpdateSize(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    msg_Dbg(vd, "Detected size change %dx%d", sys->area.place.width,
            sys->area.place.height);

    UpdateBackBuffer(vd);

    d3d11_device_lock( &sys->d3d_dev );

    UpdatePicQuadPosition(vd);

    RECT source_rect = {
        .left   = vd->source.i_x_offset,
        .right  = vd->source.i_x_offset + vd->source.i_visible_width,
        .top    = vd->source.i_y_offset,
        .bottom = vd->source.i_y_offset + vd->source.i_visible_height,
    };
    D3D11_UpdateQuadPosition(vd, &sys->d3d_dev, &sys->picQuad, &source_rect,
                             vd->source.orientation);

    d3d11_device_unlock( &sys->d3d_dev );
}

#if !VLC_WINSTORE_APP
static void FillSwapChainDesc(vout_display_t *vd, UINT width, UINT height, DXGI_SWAP_CHAIN_DESC1 *out)
{
    ZeroMemory(out, sizeof(*out));
    out->BufferCount = 3;
    out->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    out->SampleDesc.Count = 1;
    out->SampleDesc.Quality = 0;
    out->Width = width;
    out->Height = height;
    out->Format = vd->sys->display.pixelFormat->formatTexture;
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

static int SetupWindowedOutput(vout_display_t *vd, UINT width, UINT height)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->internal_swapchain.swapchainHwnd == NULL)
    {
        msg_Err(vd, "missing a HWND to create the swapchain");
        return VLC_EGENERIC;
    }

    DXGI_SWAP_CHAIN_DESC1 scd;
    HRESULT hr;

    IDXGIFactory2 *dxgifactory;
    sys->display.pixelFormat = FindD3D11Format( vd, &sys->d3d_dev, 0, true,
                                                vd->source.i_chroma==VLC_CODEC_D3D11_OPAQUE_10B ? 10 : 8,
                                                0, 0,
                                                false, D3D11_FORMAT_SUPPORT_DISPLAY );
    if (unlikely(sys->display.pixelFormat == NULL))
        sys->display.pixelFormat = FindD3D11Format( vd, &sys->d3d_dev, 0, false,
                                                    vd->source.i_chroma==VLC_CODEC_D3D11_OPAQUE_10B ? 10 : 8,
                                                    0, 0,
                                                    false, D3D11_FORMAT_SUPPORT_DISPLAY );
    if (unlikely(sys->display.pixelFormat == NULL)) {
        msg_Err(vd, "Could not get the SwapChain format.");
        return VLC_EGENERIC;
    }

    FillSwapChainDesc(vd, width, height, &scd);

    IDXGIAdapter *dxgiadapter = D3D11DeviceAdapter(sys->d3d_dev.d3ddevice);
    if (unlikely(dxgiadapter==NULL)) {
       msg_Err(vd, "Could not get the DXGI Adapter");
       return VLC_EGENERIC;
    }

    hr = IDXGIAdapter_GetParent(dxgiadapter, &IID_IDXGIFactory2, (void **)&dxgifactory);
    IDXGIAdapter_Release(dxgiadapter);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the DXGI Factory. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgifactory, (IUnknown *)sys->d3d_dev.d3ddevice,
                                              sys->internal_swapchain.swapchainHwnd, &scd,
                                              NULL, NULL, &sys->internal_swapchain.dxgiswapChain);
    if (hr == DXGI_ERROR_INVALID_CALL && scd.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
    {
        msg_Warn(vd, "10 bits swapchain failed, try 8 bits");
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgifactory, (IUnknown *)sys->d3d_dev.d3ddevice,
                                                  sys->internal_swapchain.swapchainHwnd, &scd,
                                                  NULL, NULL, &sys->internal_swapchain.dxgiswapChain);
    }
    IDXGIFactory2_Release(dxgifactory);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the SwapChain. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    IDXGISwapChain_QueryInterface( sys->internal_swapchain.dxgiswapChain, &IID_IDXGISwapChain4, (void **)&sys->internal_swapchain.dxgiswapChain4);
    return VLC_SUCCESS;
}
#endif /* !VLC_WINSTORE_APP */

static bool UpdateSwapchain( void *opaque, const struct direct3d_cfg_t *cfg )
{
    vout_display_t *vd = opaque;
    vout_display_sys_t *sys = vd->sys;
    ID3D11Texture2D* pBackBuffer;
    HRESULT hr;

    D3D11_TEXTURE2D_DESC dsc = { 0 };

    if ( sys->internal_swapchain.swapchainTargetView[0] ) {
        ID3D11Resource *res = NULL;
        ID3D11RenderTargetView_GetResource( sys->internal_swapchain.swapchainTargetView[0], &res );
        if ( res )
        {
            ID3D11Texture2D_GetDesc( (ID3D11Texture2D*) res, &dsc );
            ID3D11Resource_Release( res );
        }
    }

    if ( dsc.Width == cfg->width && dsc.Height == cfg->height )
        return true; /* nothing changed */

    for ( size_t i = 0; i < ARRAY_SIZE( sys->internal_swapchain.swapchainTargetView ); i++ )
    {
        if ( sys->internal_swapchain.swapchainTargetView[i] ) {
            ID3D11RenderTargetView_Release( sys->internal_swapchain.swapchainTargetView[i] );
            sys->internal_swapchain.swapchainTargetView[i] = NULL;
        }
    }

    /* TODO detect is the size is the same as the output and switch to fullscreen mode */
    hr = IDXGISwapChain_ResizeBuffers( sys->internal_swapchain.dxgiswapChain, 0, cfg->width, cfg->height,
                                       DXGI_FORMAT_UNKNOWN, 0 );
    if ( FAILED( hr ) ) {
        msg_Err( vd, "Failed to resize the backbuffer. (hr=0x%lX)", hr );
        return false;
    }

    hr = IDXGISwapChain_GetBuffer( sys->internal_swapchain.dxgiswapChain, 0, &IID_ID3D11Texture2D, (LPVOID *) &pBackBuffer );
    if ( FAILED( hr ) ) {
        msg_Err( vd, "Could not get the backbuffer for the Swapchain. (hr=0x%lX)", hr );
        return false;
    }

    hr = D3D11_CreateRenderTargets( &sys->d3d_dev, (ID3D11Resource *) pBackBuffer,
                                    sys->display.pixelFormat, sys->internal_swapchain.swapchainTargetView );
    ID3D11Texture2D_Release( pBackBuffer );
    if ( FAILED( hr ) ) {
        msg_Err( vd, "Failed to create the target view. (hr=0x%lX)", hr );
        return false;
    }

    D3D11_ClearRenderTargets( &sys->d3d_dev, sys->display.pixelFormat, sys->internal_swapchain.swapchainTargetView );

    return true;
}

static bool LocalSwapchainSetupDevice( void *opaque, const struct device_cfg_t *cfg, struct device_setup_t *out )
{
    vout_display_t *vd = opaque;
    HRESULT hr;
#if VLC_WINSTORE_APP
    ID3D11DeviceContext *legacy_ctx = var_InheritInteger( vd, "winrt-d3dcontext" ); /* LEGACY */
    if ( legacy_ctx == NULL )
        hr = E_FAIL;
    else
        hr = D3D11_CreateDeviceExternal( vd,
                                         legacy_ctx,
                                         cfg->hardware_decoding,
                                         &sys->internal_swapchain.d3d_dev );
#else /* !VLC_WINSTORE_APP */
    vout_display_sys_t *sys = vd->sys;
    hr = D3D11_CreateDevice( vd, &sys->hd3d, NULL,
                             cfg->hardware_decoding,
                             &sys->internal_swapchain.d3d_dev );
#endif /* !VLC_WINSTORE_APP */
    if ( FAILED( hr ) )
        return false;
    out->device_context = sys->internal_swapchain.d3d_dev.d3dcontext;
    return true;
}

static void LocalSwapchainCleanupDevice( void *opaque )
{
    vout_display_t *vd = opaque;
    vout_display_sys_t *sys = vd->sys;
    D3D11_ReleaseDevice( &sys->internal_swapchain.d3d_dev );
}

static void LocalSwapchainSwap( void *opaque )
{
    vout_display_t *vd = opaque;
    vout_display_sys_t *sys = vd->sys;

    DXGI_PRESENT_PARAMETERS presentParams = { 0 };

    HRESULT hr = IDXGISwapChain1_Present1( sys->internal_swapchain.dxgiswapChain, 0, 0, &presentParams );
    if ( hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET )
    {
        /* TODO device lost */
        msg_Err( vd, "SwapChain Present failed. (hr=0x%lX)", hr );
    }
}

static int SetupWindowLessOutput(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    DXGI_FORMAT windowlessFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
#if VLC_WINSTORE_APP
    DXGI_SWAP_CHAIN_DESC1 scd;
    IDXGISwapChain1* dxgiswapChain  = var_InheritInteger(vd, "winrt-swapchain");
    if (!dxgiswapChain)
        return VLC_EGENERIC;

    sys->internal_swapchain.dxgiswapChain = dxgiswapChain;
    IDXGISwapChain_AddRef(sys->internal_swapchain.dxgiswapChain);

    if (FAILED(IDXGISwapChain1_GetDesc(dxgiswapChain, &scd)))
        return VLC_EGENERIC;

    windowlessFormat = scd.Format;
#endif /* VLC_WINSTORE_APP */
    for (const d3d_format_t *output_format = GetRenderFormatList();
         output_format->name != NULL; ++output_format)
    {
        if (output_format->formatTexture == windowlessFormat &&
            !is_d3d11_opaque(output_format->fourcc))
        {
            sys->display.pixelFormat = output_format;
            break;
        }
    }
    if (unlikely(sys->display.pixelFormat == NULL)) {
        msg_Err(vd, "Could not setup the output format.");
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static bool LocalSwapchainUpdateOutput( void *opaque, const struct direct3d_cfg_t *cfg, struct output_cfg_t *out )
{
    vout_display_t *vd = opaque;
    if ( !UpdateSwapchain( vd, cfg ) )
        return -1;
    out->surface_format = vd->sys->display.pixelFormat->formatTexture;
    return true;
}

static bool LocalSwapchainStartEndRendering( void *opaque, bool enter )
{
    vout_display_t *vd = opaque;

    if ( enter )
    {
        vout_display_sys_t *sys = vd->sys;

        D3D11_ClearRenderTargets( &sys->d3d_dev, sys->display.pixelFormat, sys->internal_swapchain.swapchainTargetView );
    }
    return true;
}

static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
#if !VLC_WINSTORE_APP
    /* Allow using D3D11 automatically starting from Windows 8.1 */
    if (!vd->obj.force)
    {
        bool isWin81OrGreater = false;
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
        if (likely(hKernel32 != NULL))
            isWin81OrGreater = GetProcAddress(hKernel32, "IsProcessCritical") != NULL;
        if (!isWin81OrGreater)
            return VLC_EGENERIC;
    }
#endif

    vout_display_sys_t *sys = vd->sys = vlc_obj_calloc(VLC_OBJECT(vd), 1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    int ret = D3D11_Create(vd, &sys->hd3d, true);
    if (ret != VLC_SUCCESS)
        return ret;

    bool uses_external_callbacks = true;
    if (sys->swapCb == NULL || sys->startEndRenderingCb == NULL || sys->updateOutputCb == NULL)
    {
        sys->internal_swapchain.obj = VLC_OBJECT(vd);
        sys->outside_opaque = vd;
        sys->setupDeviceCb       = LocalSwapchainSetupDevice;
        sys->cleanupDeviceCb     = LocalSwapchainCleanupDevice;
        sys->updateOutputCb      = LocalSwapchainUpdateOutput;
        sys->swapCb              = LocalSwapchainSwap;
        sys->startEndRenderingCb = LocalSwapchainStartEndRendering;
        uses_external_callbacks = false;
    }

    CommonInit(vd, &sys->area, cfg);
    if ( !uses_external_callbacks )
    {
#if VLC_WINSTORE_APP
        /* LEGACY, the d3dcontext and swapchain were given by the host app */
        if (var_InheritInteger(vd, "winrt-d3dcontext") == 0)
        {
            msg_Err(vd, "missing direct3d context for winstore");
            goto error;
        }
#else /* !VLC_WINSTORE_APP */
        if (CommonWindowInit(VLC_OBJECT(vd), &sys->area, &sys->sys,
                       vd->source.projection_mode != PROJECTION_MODE_RECTANGULAR))
            goto error;
        sys->internal_swapchain.swapchainHwnd = sys->sys.hvideownd;
#endif /* !VLC_WINSTORE_APP */
    }
    else
    {
        CommonPlacePicture(VLC_OBJECT(vd), &sys->area, &sys->sys);
    }

    if (vd->source.projection_mode != PROJECTION_MODE_RECTANGULAR && sys->sys.hvideownd)
        sys->p_sensors = HookWindowsSensors(vd, sys->sys.hvideownd);

    if (Direct3D11Open(vd, fmtp)) {
        msg_Err(vd, "Direct3D11 could not be opened");
        goto error;
    }

    vout_window_SetTitle(sys->area.vdcfg.window, VOUT_TITLE " (Direct3D11 output)");
    msg_Dbg(vd, "Direct3D11 device adapter successfully initialized");

    vd->info.can_scale_spu        = true;

    if (var_InheritBool(vd, "direct3d11-hw-blending") &&
        sys->regionQuad.textureFormat != NULL)
    {
        sys->pSubpictureChromas[0] = sys->regionQuad.textureFormat->fourcc;
        sys->pSubpictureChromas[1] = 0;
        vd->info.subpicture_chromas = sys->pSubpictureChromas;
    }
    else
        vd->info.subpicture_chromas = NULL;

    if (is_d3d11_opaque(vd->fmt.i_chroma))
        vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;

    msg_Dbg(vd, "Direct3D11 Open Succeeded");

    return VLC_SUCCESS;

error:
    Close(vd);
    return VLC_EGENERIC;
}

static void Close(vout_display_t *vd)
{
    Direct3D11Close(vd);
    UnhookWindowsSensors(vd->sys->p_sensors);
#if !VLC_WINSTORE_APP
    CommonWindowClean(VLC_OBJECT(vd), &vd->sys->sys);
#endif
    Direct3D11Destroy(vd);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned pool_size)
{
    /* compensate for extra hardware decoding pulling extra pictures from our pool */
    pool_size += 2;

    vout_display_sys_t *sys = vd->sys;
    picture_t **pictures = NULL;
    picture_t *picture;
    unsigned  picture_count = 0;

    if (sys->pool)
        return sys->pool;

    ID3D11Texture2D  *textures[pool_size * D3D11_MAX_SHADER_VIEW];
    memset(textures, 0, sizeof(textures));
    unsigned slices = pool_size;
    if (!CanUseVoutPool(&sys->d3d_dev, pool_size))
        /* only provide enough for the filters, we can still do direct rendering */
        slices = __MIN(slices, 6);

    if (AllocateTextures(vd, &sys->d3d_dev, sys->picQuad.textureFormat, &sys->area.texture_source, slices, textures))
        goto error;

    pictures = calloc(pool_size, sizeof(*pictures));
    if (!pictures)
        goto error;

    for (picture_count = 0; picture_count < pool_size; picture_count++) {
        picture_sys_t *picsys = calloc(1, sizeof(*picsys));
        if (unlikely(picsys == NULL))
            goto error;

        for (unsigned plane = 0; plane < D3D11_MAX_SHADER_VIEW; plane++)
            picsys->texture[plane] = textures[picture_count * D3D11_MAX_SHADER_VIEW + plane];

        picture_resource_t resource = {
            .p_sys = picsys,
            .pf_destroy = DestroyDisplayPoolPicture,
        };

        picture = picture_NewFromResource(&sys->area.texture_source, &resource);
        if (unlikely(picture == NULL)) {
            free(picsys);
            msg_Err( vd, "Failed to create picture %d in the pool.", picture_count );
            goto error;
        }

        pictures[picture_count] = picture;
        picsys->slice_index = picture_count;
        picsys->formatTexture = sys->picQuad.textureFormat->formatTexture;
        /* each picture_t holds a ref to the context and release it on Destroy */
        picsys->context = sys->d3d_dev.d3dcontext;
        ID3D11DeviceContext_AddRef(sys->d3d_dev.d3dcontext);
    }

#ifdef HAVE_ID3D11VIDEODECODER
    if (!sys->legacy_shader)
#endif
    {
        for (picture_count = 0; picture_count < pool_size; picture_count++) {
            picture_sys_t *p_sys = pictures[picture_count]->p_sys;
            if (!p_sys->texture[0])
                continue;
            if (D3D11_AllocateResourceView(vd, sys->d3d_dev.d3ddevice, sys->picQuad.textureFormat,
                                         p_sys->texture, picture_count,
                                         p_sys->renderSrc))
                goto error;
        }
    }

    sys->pool = picture_pool_New( pool_size, pictures );

error:
    if (sys->pool == NULL) {
        if (pictures) {
            msg_Dbg(vd, "Failed to create the picture d3d11 pool");
            for (unsigned i=0;i<picture_count; ++i)
                picture_Release(pictures[i]);
            free(pictures);
        }

        /* create an empty pool to avoid crashing */
        sys->pool = picture_pool_New( 0, NULL );
    } else {
        msg_Dbg(vd, "D3D11 pool succeed with %d surfaces (%dx%d) context 0x%p",
                pool_size, sys->area.texture_source.i_width, sys->area.texture_source.i_height, sys->d3d_dev.d3dcontext);
    }
    return sys->pool;
}

static void DestroyDisplayPoolPicture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    ReleasePictureSys( p_sys );
    free(p_sys);
}

static void getZoomMatrix(float zoom, FLOAT matrix[static 16]) {

    const FLOAT m[] = {
        /* x   y     z     w */
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, zoom, 1.0f
    };

    memcpy(matrix, m, sizeof(m));
}

/* perspective matrix see https://www.opengl.org/sdk/docs/man2/xhtml/gluPerspective.xml */
static void getProjectionMatrix(float sar, float fovy, FLOAT matrix[static 16]) {

    float zFar  = 1000;
    float zNear = 0.01;

    float f = 1.f / tanf(fovy / 2.f);

    const FLOAT m[] = {
        f / sar, 0.f,                   0.f,                0.f,
        0.f,     f,                     0.f,                0.f,
        0.f,     0.f,     (zNear + zFar) / (zNear - zFar), -1.f,
        0.f,     0.f, (2 * zNear * zFar) / (zNear - zFar),  0.f};

     memcpy(matrix, m, sizeof(m));
}

static float UpdateFOVy(float f_fovx, float f_sar)
{
    return 2 * atanf(tanf(f_fovx / 2) / f_sar);
}

static float UpdateZ(float f_fovx, float f_fovy)
{
    /* Do trigonometry to calculate the minimal z value
     * that will allow us to zoom out without seeing the outside of the
     * sphere (black borders). */
    float tan_fovx_2 = tanf(f_fovx / 2);
    float tan_fovy_2 = tanf(f_fovy / 2);
    float z_min = - SPHERE_RADIUS / sinf(atanf(sqrtf(
                    tan_fovx_2 * tan_fovx_2 + tan_fovy_2 * tan_fovy_2)));

    /* The FOV value above which z is dynamically calculated. */
    const float z_thresh = 90.f;

    float f_z;
    if (f_fovx <= z_thresh * M_PI / 180)
        f_z = 0;
    else
    {
        float f = z_min / ((FIELD_OF_VIEW_DEGREES_MAX - z_thresh) * M_PI / 180);
        f_z = f * f_fovx - f * z_thresh * M_PI / 180;
        if (f_z < z_min)
            f_z = z_min;
    }
    return f_z;
}

static void SetQuadVSProjection(vout_display_t *vd, d3d_quad_t *quad, const vlc_viewpoint_t *p_vp)
{
    vout_display_sys_t *sys = vd->sys;
    if (!quad->pVertexShaderConstants)
        return;

    // Convert degree into radian
    float f_fovx = p_vp->fov * (float)M_PI / 180.f;
    if ( f_fovx > FIELD_OF_VIEW_DEGREES_MAX * M_PI / 180 + 0.001f ||
         f_fovx < -0.001f )
        return;

    float f_sar = (float) sys->area.vdcfg.display.width / sys->area.vdcfg.display.height;
    float f_fovy = UpdateFOVy(f_fovx, f_sar);
    float f_z = UpdateZ(f_fovx, f_fovy);

    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        VS_PROJECTION_CONST *dst_data = mapped.pData;
        getZoomMatrix(SPHERE_RADIUS * f_z, dst_data->Zoom);
        getProjectionMatrix(f_sar, f_fovy, dst_data->Projection);

        vlc_viewpoint_t vp;
        vlc_viewpoint_reverse(&vp, p_vp);
        vlc_viewpoint_to_4x4(&vp, dst_data->View);
    }
    ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;
    int res = CommonControl( VLC_OBJECT(vd), &sys->area, &sys->sys, query, args );

    if (query == VOUT_DISPLAY_CHANGE_VIEWPOINT)
    {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t*);
        if ( sys->picQuad.pVertexShaderConstants )
        {
            SetQuadVSProjection( vd, &sys->picQuad, &cfg->viewpoint );
            res = VLC_SUCCESS;
        }
    }

    if ( sys->area.place_changed )
    {
        UpdateSize(vd);
        sys->area.place_changed =false;
    }

    return res;
}

static void PreparePicture(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->picQuad.textureFormat->formatTexture == DXGI_FORMAT_UNKNOWN || !is_d3d11_opaque(picture->format.i_chroma))
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        D3D11_TEXTURE2D_DESC texDesc;
        int i;
        HRESULT hr;
        plane_t planes[PICTURE_PLANE_MAX];

        bool b_mapped = true;
        for (i = 0; i < picture->i_planes; i++) {
            hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, sys->stagingSys.resource[i],
                                         0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            if( unlikely(FAILED(hr)) )
            {
                while (i-- > 0)
                    ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, sys->stagingSys.resource[i], 0);
                b_mapped = false;
                break;
            }
            ID3D11Texture2D_GetDesc(sys->stagingSys.texture[i], &texDesc);
            planes[i].i_lines = texDesc.Height;
            planes[i].i_pitch = mappedResource.RowPitch;
            planes[i].p_pixels = mappedResource.pData;

            planes[i].i_visible_lines = picture->p[i].i_visible_lines;
            planes[i].i_visible_pitch = picture->p[i].i_visible_pitch;
        }

        if (b_mapped)
        {
            for (i = 0; i < picture->i_planes; i++)
                plane_CopyPixels(&planes[i], &picture->p[i]);

            for (i = 0; i < picture->i_planes; i++)
                ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, sys->stagingSys.resource[i], 0);
        }
    }
    else
    {
        picture_sys_t *p_sys = ActivePictureSys(picture);

        d3d11_device_lock( &sys->d3d_dev );

        if (sys->legacy_shader) {
            D3D11_TEXTURE2D_DESC srcDesc,texDesc;
            ID3D11Texture2D_GetDesc(p_sys->texture[KNOWN_DXGI_INDEX], &srcDesc);
            ID3D11Texture2D_GetDesc(sys->stagingSys.texture[0], &texDesc);
            D3D11_BOX box = {
                .top = 0,
                .bottom = __MIN(srcDesc.Height, texDesc.Height),
                .left = 0,
                .right = __MIN(srcDesc.Width, texDesc.Width),
                .back = 1,
            };
            ID3D11DeviceContext_CopySubresourceRegion(sys->d3d_dev.d3dcontext,
                                                      sys->stagingSys.resource[KNOWN_DXGI_INDEX],
                                                      0, 0, 0, 0,
                                                      p_sys->resource[KNOWN_DXGI_INDEX],
                                                      p_sys->slice_index, &box);
        }
        else
        {
            D3D11_TEXTURE2D_DESC texDesc;
            ID3D11Texture2D_GetDesc(p_sys->texture[0], &texDesc);
            if (texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
            {
                /* for performance reason we don't want to allocate this during
                 * display, do it preferrably when creating the texture */
                assert(p_sys->renderSrc[0]!=NULL);
            }
            if ( sys->picQuad.i_height != texDesc.Height ||
                 sys->picQuad.i_width != texDesc.Width )
            {
                /* the decoder produced different sizes than the vout, we need to
                 * adjust the vertex */
                sys->area.texture_source.i_width  = sys->picQuad.i_height = texDesc.Height;
                sys->area.texture_source.i_height = sys->picQuad.i_width = texDesc.Width;

                CommonPlacePicture(VLC_OBJECT(vd), &sys->area, &sys->sys);
                UpdateSize(vd);
            }
        }
    }

    if (subpicture) {
        int subpicture_region_count    = 0;
        picture_t **subpicture_regions = NULL;
        Direct3D11MapSubpicture(vd, &subpicture_region_count, &subpicture_regions, subpicture);
        Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
        sys->d3dregion_count = subpicture_region_count;
        sys->d3dregions      = subpicture_regions;
    }

    if (picture->format.mastering.max_luminance)
    {
        D3D11_UpdateQuadLuminanceScale(vd, &sys->d3d_dev, &sys->picQuad, GetFormatLuminance(VLC_OBJECT(vd), &picture->format) / (float)sys->display.luminance_peak);

        if (sys->internal_swapchain.dxgiswapChain4)
        {
            DXGI_HDR_METADATA_HDR10 hdr10 = {0};
            hdr10.GreenPrimary[0] = picture->format.mastering.primaries[0];
            hdr10.GreenPrimary[1] = picture->format.mastering.primaries[1];
            hdr10.BluePrimary[0]  = picture->format.mastering.primaries[2];
            hdr10.BluePrimary[1]  = picture->format.mastering.primaries[3];
            hdr10.RedPrimary[0]   = picture->format.mastering.primaries[4];
            hdr10.RedPrimary[1]   = picture->format.mastering.primaries[5];
            hdr10.WhitePoint[0] = picture->format.mastering.white_point[0];
            hdr10.WhitePoint[1] = picture->format.mastering.white_point[1];
            hdr10.MinMasteringLuminance = picture->format.mastering.min_luminance;
            hdr10.MaxMasteringLuminance = picture->format.mastering.max_luminance;
            hdr10.MaxContentLightLevel = picture->format.lighting.MaxCLL;
            hdr10.MaxFrameAverageLightLevel = picture->format.lighting.MaxFALL;
            IDXGISwapChain4_SetHDRMetaData(sys->internal_swapchain.dxgiswapChain4, DXGI_HDR_METADATA_TYPE_HDR10, sizeof(hdr10), &hdr10);
        }
    }

    /* Render the quad */
    ID3D11ShaderResourceView **renderSrc;
    if (!is_d3d11_opaque(picture->format.i_chroma) || sys->legacy_shader)
        renderSrc = sys->stagingSys.renderSrc;
    else {
        picture_sys_t *p_sys = ActivePictureSys(picture);
        renderSrc = p_sys->renderSrc;
    }
    D3D11_RenderQuad(&sys->d3d_dev, &sys->picQuad,
                     vd->source.projection_mode == PROJECTION_MODE_RECTANGULAR ? &sys->flatVShader : &sys->projectionVShader,
                     renderSrc,
                     sys->internal_swapchain.swapchainTargetView); /* NULL with external rendering */

    if (subpicture) {
        // draw the additional vertices
        for (int i = 0; i < sys->d3dregion_count; ++i) {
            if (sys->d3dregions[i])
            {
                d3d_quad_t *quad = (d3d_quad_t *) sys->d3dregions[i]->p_sys;
                D3D11_RenderQuad(&sys->d3d_dev, quad, &sys->flatVShader,
                                 quad->picSys.renderSrc,
                                 sys->internal_swapchain.swapchainTargetView); /* NULL with external rendering */
            }
        }
    }

    if (sys->prepareWait)
    {
        int maxWait = 10;
        ID3D11DeviceContext_End(sys->d3d_dev.d3dcontext, (ID3D11Asynchronous*)sys->prepareWait);

        while (S_FALSE == ID3D11DeviceContext_GetData(sys->d3d_dev.d3dcontext,
                                                      (ID3D11Asynchronous*)sys->prepareWait, NULL, 0, 0)
               && --maxWait)
            SleepEx(2, TRUE);
    }

    if (is_d3d11_opaque(picture->format.i_chroma) && sys->picQuad.textureFormat->formatTexture != DXGI_FORMAT_UNKNOWN)
        d3d11_device_unlock( &sys->d3d_dev );
}

static void Prepare(vout_display_t *vd, picture_t *picture,
                    subpicture_t *subpicture, vlc_tick_t date)
{
    vout_display_sys_t *sys = vd->sys;

    VLC_UNUSED(date);
#if VLC_WINSTORE_APP
    /* legacy UWP mode, the width/height was set in GUID_SWAPCHAIN_WIDTH/HEIGHT */
    uint32_t i_width;
    uint32_t i_height;
    UINT dataSize = sizeof(i_width);
    HRESULT hr = IDXGISwapChain_GetPrivateData(sys->internal_swapchain.dxgiswapChain, &GUID_SWAPCHAIN_WIDTH, &dataSize, &i_width);
    if (SUCCEEDED(hr)) {
        dataSize = sizeof(i_height);
        hr = IDXGISwapChain_GetPrivateData(sys->internal_swapchain.dxgiswapChain, &GUID_SWAPCHAIN_HEIGHT, &dataSize, &i_height);
        if (SUCCEEDED(hr)) {
            if (i_width != sys->area.vdcfg.display.width || i_height != sys->area.vdcfg.display.height)
            {
                vout_display_SetSize(vd, i_width, i_height);
            }
        }
    }
#endif

    if (sys->startEndRenderingCb(sys->outside_opaque, true))
    {
        PreparePicture(vd, picture, subpicture);

        sys->startEndRenderingCb(sys->outside_opaque, false);
    }
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(picture);

    d3d11_device_lock( &sys->d3d_dev );
    sys->swapCb(sys->outside_opaque);
    d3d11_device_unlock( &sys->d3d_dev );
}

static void Direct3D11Destroy(vout_display_t *vd)
{
#if !VLC_WINSTORE_APP
    D3D11_Destroy( &vd->sys->hd3d );
#endif
}

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

static void D3D11SetColorSpace(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    int best = -1;
    int score, best_score = 0;
    UINT support;
    IDXGISwapChain3 *dxgiswapChain3 = NULL;
    sys->display.colorspace = &color_spaces[0];
    if (sys->sys.event == NULL) /* TODO support external colourspace handling */
        goto done;

    hr = IDXGISwapChain_QueryInterface( sys->internal_swapchain.dxgiswapChain, &IID_IDXGISwapChain3, (void **)&dxgiswapChain3);
    if (FAILED(hr)) {
        msg_Warn(vd, "could not get a IDXGISwapChain3");
        goto done;
    }

    bool src_full_range = vd->source.color_range == COLOR_RANGE_FULL ||
                          /* the YUV->RGB conversion already output full range */
                          is_d3d11_opaque(vd->source.i_chroma) ||
                          vlc_fourcc_IsYUV(vd->source.i_chroma);

    /* pick the best output based on color support and transfer */
    /* TODO support YUV output later */
    for (int i=0; color_spaces[i].name; ++i)
    {
        hr = IDXGISwapChain3_CheckColorSpaceSupport(dxgiswapChain3, color_spaces[i].dxgi, &support);
        if (SUCCEEDED(hr) && support) {
            msg_Dbg(vd, "supports colorspace %s", color_spaces[i].name);
            score = 0;
            if (color_spaces[i].primaries == vd->source.primaries)
                score++;
            if (color_spaces[i].color == vd->source.space)
                score += 2; /* we don't want to translate color spaces */
            if (color_spaces[i].transfer == vd->source.transfer ||
                /* favor 2084 output for HLG source */
                (color_spaces[i].transfer == TRANSFER_FUNC_SMPTE_ST2084 && vd->source.transfer == TRANSFER_FUNC_HLG))
                score++;
            if (color_spaces[i].b_full_range == src_full_range)
                score++;
            if (score > best_score || (score && best == -1)) {
                best = i;
                best_score = score;
            }
        }
    }

    if (best == -1)
    {
        best = 0;
        msg_Warn(vd, "no matching colorspace found force %s", color_spaces[best].name);
    }

#ifdef HAVE_DXGI1_6_H
    IDXGIOutput *dxgiOutput = NULL;

    if (SUCCEEDED(IDXGISwapChain_GetContainingOutput( sys->internal_swapchain.dxgiswapChain, &dxgiOutput )))
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
                            msg_Warn(vd, "Can't handle conversion to screen format %s", color_spaces[i].name);
                        else
                        {
                            best = i;
                            csp = &color_spaces[i];
                        }
                        break;
                    }
                }

                msg_Dbg(vd, "Output max luminance: %.1f, colorspace %s, bits per pixel %d", desc1.MaxFullFrameLuminance, csp?csp->name:"unknown", desc1.BitsPerColor);
                //sys->display.luminance_peak = desc1.MaxFullFrameLuminance;
            }
            IDXGIOutput6_Release( dxgiOutput6 );
        }
        IDXGIOutput_Release( dxgiOutput );
    }
#endif

    hr = IDXGISwapChain3_SetColorSpace1(dxgiswapChain3, color_spaces[best].dxgi);
    if (SUCCEEDED(hr))
    {
        sys->display.colorspace = &color_spaces[best];
        msg_Dbg(vd, "using colorspace %s", sys->display.colorspace->name);
    }
    else
        msg_Err(vd, "Failed to set colorspace %s. (hr=0x%lX)", sys->display.colorspace->name, hr);
done:
    /* guestimate the display peak luminance */
    switch (sys->display.colorspace->transfer)
    {
    case TRANSFER_FUNC_LINEAR:
    case TRANSFER_FUNC_SRGB:
        sys->display.luminance_peak = DEFAULT_SRGB_BRIGHTNESS;
        break;
    case TRANSFER_FUNC_SMPTE_ST2084:
        sys->display.luminance_peak = MAX_PQ_BRIGHTNESS;
        break;
    /* there is no other output transfer on Windows */
    default:
        vlc_assert_unreachable();
    }

    if (dxgiswapChain3)
        IDXGISwapChain3_Release(dxgiswapChain3);
}

static const d3d_format_t *GetDirectRenderingFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    if (is_d3d11_opaque(i_src_chroma))
        supportFlags |= D3D11_FORMAT_SUPPORT_DECODER_OUTPUT;
    return FindD3D11Format( vd, &vd->sys->d3d_dev, i_src_chroma, false, 0, 0, 0, is_d3d11_opaque(i_src_chroma), supportFlags );
}

static const d3d_format_t *GetDirectDecoderFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_DECODER_OUTPUT;
    return FindD3D11Format( vd, &vd->sys->d3d_dev, i_src_chroma, false, 0, 0, 0, is_d3d11_opaque(i_src_chroma), supportFlags );
}

static const d3d_format_t *GetDisplayFormatByDepth(vout_display_t *vd, uint8_t bit_depth,
                                                   uint8_t widthDenominator,
                                                   uint8_t heightDenominator,
                                                   bool from_processor,
                                                   bool rgb_only)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    if (from_processor)
        supportFlags |= D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
    return FindD3D11Format( vd, &vd->sys->d3d_dev, 0, rgb_only,
                            bit_depth, widthDenominator, heightDenominator,
                            false, supportFlags );
}

static const d3d_format_t *GetBlendableFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_BLENDABLE;
    return FindD3D11Format( vd, &vd->sys->d3d_dev, i_src_chroma, false, 0, 0, 0, false, supportFlags );
}

static int Direct3D11Open(vout_display_t *vd, video_format_t *fmtp)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr = E_FAIL;
    int ret = VLC_EGENERIC;

    struct device_cfg_t cfg = {
        .hardware_decoding = is_d3d11_opaque( vd->source.i_chroma ) 
    };
    struct device_setup_t out;
    ID3D11DeviceContext *d3d11_ctx = NULL;
    if ( sys->setupDeviceCb( sys->outside_opaque, &cfg, &out ) )
        d3d11_ctx = out.device_context;
    if ( d3d11_ctx == NULL )
    {
        msg_Err(vd, "Missing external ID3D11DeviceContext");
        return VLC_EGENERIC;
    }
    hr = D3D11_CreateDeviceExternal(vd, d3d11_ctx,
                                    is_d3d11_opaque(vd->source.i_chroma),
                                    &sys->d3d_dev);
    if (FAILED(hr)) {
        msg_Err(vd, "Could not Create the D3D11 device. (hr=0x%lX)", hr);
        if ( sys->cleanupDeviceCb )
            sys->cleanupDeviceCb( sys->outside_opaque );
        return VLC_EGENERIC;
    }

    if (sys->sys.event == NULL)
        ret = SetupWindowLessOutput(vd);
#if !VLC_WINSTORE_APP
    else
        ret = SetupWindowedOutput(vd, sys->area.vdcfg.display.width, sys->area.vdcfg.display.height);
#endif /* !VLC_WINSTORE_APP */
    if (ret != VLC_SUCCESS)
        return ret;

    D3D11SetColorSpace(vd);

    video_format_t fmt;
    video_format_Copy(&fmt, &vd->source);
    int err = SetupOutputFormat(vd, &fmt);
    if (err != VLC_SUCCESS)
    {
        if (!is_d3d11_opaque(vd->source.i_chroma)
#if !VLC_WINSTORE_APP
            && vd->obj.force
#endif
                )
        {
            const vlc_fourcc_t *list = vlc_fourcc_IsYUV(vd->source.i_chroma) ?
                        vlc_fourcc_GetYUVFallback(vd->source.i_chroma) :
                        vlc_fourcc_GetRGBFallback(vd->source.i_chroma);
            for (unsigned i = 0; list[i] != 0; i++) {
                fmt.i_chroma = list[i];
                if (fmt.i_chroma == vd->source.i_chroma)
                    continue;
                err = SetupOutputFormat(vd, &fmt);
                if (err == VLC_SUCCESS)
                    break;
            }
        }
        if (err != VLC_SUCCESS)
        {
            if ( sys->cleanupDeviceCb )
                sys->cleanupDeviceCb( sys->outside_opaque );
            return err;
        }
    }

    if (Direct3D11CreateGenericResources(vd)) {
        msg_Err(vd, "Failed to allocate resources");
        if ( sys->cleanupDeviceCb )
            sys->cleanupDeviceCb( sys->outside_opaque );
        return VLC_EGENERIC;
    }

    video_format_Clean(fmtp);
    *fmtp = fmt;

    return VLC_SUCCESS;
}

static int SetupOutputFormat(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    // look for the requested pixel format first
    sys->picQuad.textureFormat = GetDirectRenderingFormat(vd, fmt->i_chroma);

    // look for any pixel format that we can handle with enough pixels per channel
    const d3d_format_t *decoder_format = NULL;
    if ( !sys->picQuad.textureFormat )
    {
        uint8_t bits_per_channel;
        uint8_t widthDenominator, heightDenominator;
        switch (fmt->i_chroma)
        {
        case VLC_CODEC_D3D11_OPAQUE:
            bits_per_channel = 8;
            widthDenominator = heightDenominator = 2;
            break;
        case VLC_CODEC_D3D11_OPAQUE_RGBA:
        case VLC_CODEC_D3D11_OPAQUE_BGRA:
            bits_per_channel = 8;
            widthDenominator = heightDenominator = 1;
            break;
        case VLC_CODEC_D3D11_OPAQUE_10B:
            bits_per_channel = 10;
            widthDenominator = heightDenominator = 2;
            break;
        default:
            {
                const vlc_chroma_description_t *p_format = vlc_fourcc_GetChromaDescription(fmt->i_chroma);
                if (p_format == NULL)
                {
                    bits_per_channel = 8;
                    widthDenominator = heightDenominator = 2;
                }
                else
                {
                    bits_per_channel = p_format->pixel_bits == 0 ? 8 : p_format->pixel_bits /
                                                                   (p_format->plane_count==1 ? p_format->pixel_size : 1);
                    widthDenominator = heightDenominator = 1;
                    for (size_t i=0; i<p_format->plane_count; i++)
                    {
                        if (widthDenominator < p_format->p[i].w.den)
                            widthDenominator = p_format->p[i].w.den;
                        if (heightDenominator < p_format->p[i].h.den)
                            heightDenominator = p_format->p[1].h.den;
                    }
                }
            }
            break;
        }

        /* look for a decoder format that can be decoded but not used in shaders */
        if ( is_d3d11_opaque(fmt->i_chroma) )
            decoder_format = GetDirectDecoderFormat(vd, fmt->i_chroma);
        else
            decoder_format = sys->picQuad.textureFormat;

        bool is_rgb = !vlc_fourcc_IsYUV(fmt->i_chroma);
        sys->picQuad.textureFormat = GetDisplayFormatByDepth(vd, bits_per_channel,
                                                             widthDenominator, heightDenominator,
                                                             decoder_format!=NULL, is_rgb);
        if (!sys->picQuad.textureFormat && is_rgb)
            sys->picQuad.textureFormat = GetDisplayFormatByDepth(vd, bits_per_channel,
                                                                 widthDenominator, heightDenominator,
                                                                 decoder_format!=NULL, false);
    }

    // look for any pixel format that we can handle
    if ( !sys->picQuad.textureFormat )
        sys->picQuad.textureFormat = GetDisplayFormatByDepth(vd, 0, 0, 0, false, false);

    if ( !sys->picQuad.textureFormat )
    {
       msg_Err(vd, "Could not get a suitable texture pixel format");
       return VLC_EGENERIC;
    }

    msg_Dbg( vd, "Using pixel format %s for chroma %4.4s", sys->picQuad.textureFormat->name,
                 (char *)&fmt->i_chroma );

    fmt->i_chroma = decoder_format ? decoder_format->fourcc : sys->picQuad.textureFormat->fourcc;
    DxgiFormatMask( sys->picQuad.textureFormat->formatTexture, fmt );

    /* check the region pixel format */
    sys->regionQuad.textureFormat = GetBlendableFormat(vd, VLC_CODEC_RGBA);
    if (!sys->regionQuad.textureFormat)
        sys->regionQuad.textureFormat = GetBlendableFormat(vd, VLC_CODEC_BGRA);

    if (Direct3D11CreateFormatResources(vd, fmt)) {
        msg_Err(vd, "Failed to allocate format resources");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void Direct3D11Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyResources(vd);

    if (sys->internal_swapchain.dxgiswapChain4)
    {
        IDXGISwapChain4_Release(sys->internal_swapchain.dxgiswapChain4);
        sys->internal_swapchain.dxgiswapChain4 = NULL;
    }
    if (sys->internal_swapchain.dxgiswapChain)
    {
        IDXGISwapChain_Release(sys->internal_swapchain.dxgiswapChain);
        sys->internal_swapchain.dxgiswapChain = NULL;
    }

    D3D11_ReleaseDevice( &sys->d3d_dev );

    if ( sys->cleanupDeviceCb )
        sys->cleanupDeviceCb( sys->outside_opaque );

    msg_Dbg(vd, "Direct3D11 device adapter closed");
}

static void UpdatePicQuadPosition(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    RECT rect_dst = {
        .left   = sys->area.place.x,
        .right  = sys->area.place.x + sys->area.place.width,
        .top    = sys->area.place.y,
        .bottom = sys->area.place.y + sys->area.place.height
    };

    D3D11_UpdateViewport( &sys->picQuad, &rect_dst, sys->display.pixelFormat );

    SetQuadVSProjection(vd, &sys->picQuad, &sys->area.vdcfg.viewpoint);

#ifndef NDEBUG
    msg_Dbg( vd, "picQuad position (%.02f,%.02f) %.02fx%.02f",
             sys->picQuad.cropViewport[0].TopLeftX, sys->picQuad.cropViewport[0].TopLeftY,
             sys->picQuad.cropViewport[0].Width, sys->picQuad.cropViewport[0].Height );
#endif
}

static bool CanUseTextureArray(vout_display_t *vd)
{
#ifndef HAVE_ID3D11VIDEODECODER
    (void) vd;
    return false;
#else
    struct wddm_version WDDM = {
        .revision     = 162, // 17.5.1 - 2017/05/04
    };
    if (D3D11CheckDriverVersion(&vd->sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM) == VLC_SUCCESS)
        return true;

    msg_Dbg(vd, "fallback to legacy shader mode for old AMD drivers");
    return false;
#endif
}

static bool BogusZeroCopy(vout_display_t *vd)
{
    IDXGIAdapter *p_adapter = D3D11DeviceAdapter(vd->sys->d3d_dev.d3ddevice);
    if (!p_adapter)
        return false;

    DXGI_ADAPTER_DESC adapterDesc;
    if (FAILED(IDXGIAdapter_GetDesc(p_adapter, &adapterDesc)))
        return false;
    IDXGIAdapter_Release(p_adapter);

    if (adapterDesc.VendorId != GPU_MANUFACTURER_AMD)
        return false;

    switch (adapterDesc.DeviceId)
    {
    case 0x687F: // RX Vega 56/64
    case 0x6863: // RX Vega Frontier Edition
    case 0x15DD: // RX Vega 8/11 (Ryzen iGPU)
    {
        struct wddm_version WDDM = {
            .revision     = 14011, // 18.10.2 - 2018/06/11
        };
        return D3D11CheckDriverVersion(&vd->sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM) != VLC_SUCCESS;
    }
    default:
        return false;
    }
}

/* TODO : handle errors better
   TODO : seperate out into smaller functions like createshaders */
static int Direct3D11CreateFormatResources(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    sys->legacy_shader = sys->d3d_dev.feature_level < D3D_FEATURE_LEVEL_10_0 || !CanUseTextureArray(vd) ||
            BogusZeroCopy(vd);

    hr = D3D11_CompilePixelShader(vd, &sys->hd3d, sys->legacy_shader, &sys->d3d_dev,
                                  &sys->display, fmt->transfer, fmt->primaries,
                                  fmt->color_range == COLOR_RANGE_FULL,
                                  &sys->picQuad);
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to create the pixel shader. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    sys->picQuad.i_width  = fmt->i_width;
    sys->picQuad.i_height = fmt->i_height;
    if (!sys->legacy_shader && is_d3d11_opaque(fmt->i_chroma))
    {
        sys->picQuad.i_width  = (sys->picQuad.i_width  + 0x7F) & ~0x7F;
        sys->picQuad.i_height = (sys->picQuad.i_height + 0x7F) & ~0x7F;
    }
    else
    if ( sys->picQuad.textureFormat->formatTexture != DXGI_FORMAT_R8G8B8A8_UNORM &&
         sys->picQuad.textureFormat->formatTexture != DXGI_FORMAT_B5G6R5_UNORM )
    {
        sys->picQuad.i_width  = (sys->picQuad.i_width  + 0x01) & ~0x01;
        sys->picQuad.i_height = (sys->picQuad.i_height + 0x01) & ~0x01;
    }

    sys->area.texture_source.i_width  = sys->picQuad.i_width;
    sys->area.texture_source.i_height = sys->picQuad.i_height;

    CommonPlacePicture(VLC_OBJECT(vd), &sys->area, &sys->sys);

    if (D3D11_AllocateQuad(vd, &sys->d3d_dev, vd->source.projection_mode, &sys->picQuad) != VLC_SUCCESS)
    {
        msg_Err(vd, "Could not allocate quad buffers.");
       return VLC_EGENERIC;
    }

    RECT source_rect = {
        .left   = vd->source.i_x_offset,
        .right  = vd->source.i_x_offset + vd->source.i_visible_width,
        .top    = vd->source.i_y_offset,
        .bottom = vd->source.i_y_offset + vd->source.i_visible_height,
    };
    if (D3D11_SetupQuad( vd, &sys->d3d_dev, &sys->area.texture_source, &sys->picQuad, &sys->display,
                         &source_rect,
                         vd->source.orientation ) != VLC_SUCCESS) {
        msg_Err(vd, "Could not Create the main quad picture.");
        return VLC_EGENERIC;
    }

    if ( vd->source.projection_mode == PROJECTION_MODE_EQUIRECTANGULAR ||
         vd->source.projection_mode == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD )
        SetQuadVSProjection( vd, &sys->picQuad, &sys->area.vdcfg.viewpoint );

    if (is_d3d11_opaque(fmt->i_chroma)) {
        ID3D10Multithread *pMultithread;
        hr = ID3D11Device_QueryInterface( sys->d3d_dev.d3ddevice, &IID_ID3D10Multithread, (void **)&pMultithread);
        if (SUCCEEDED(hr)) {
            ID3D10Multithread_SetMultithreadProtected(pMultithread, TRUE);
            ID3D10Multithread_Release(pMultithread);
        }
    }

#ifdef HAVE_ID3D11VIDEODECODER
    if (!is_d3d11_opaque(fmt->i_chroma) || sys->legacy_shader)
    {
        /* we need a staging texture */
        ID3D11Texture2D *textures[D3D11_MAX_SHADER_VIEW] = {0};

        if (AllocateTextures(vd, &sys->d3d_dev, sys->picQuad.textureFormat, &sys->area.texture_source, 1, textures))
        {
            msg_Err(vd, "Failed to allocate the staging texture");
            return VLC_EGENERIC;
        }

        if (D3D11_AllocateResourceView(vd, sys->d3d_dev.d3ddevice, sys->picQuad.textureFormat,
                                     textures, 0, sys->stagingSys.renderSrc))
        {
            msg_Err(vd, "Failed to allocate the staging shader view");
            return VLC_EGENERIC;
        }

        for (unsigned plane = 0; plane < D3D11_MAX_SHADER_VIEW; plane++)
            sys->stagingSys.texture[plane] = textures[plane];
    }
#endif

    vd->info.is_slow = false;
    return VLC_SUCCESS;
}

static int Direct3D11CreateGenericResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    D3D11_QUERY_DESC query = { 0 };
    query.Query = D3D11_QUERY_EVENT;
    hr = ID3D11Device_CreateQuery(sys->d3d_dev.d3ddevice, &query, &sys->prepareWait);

    ID3D11BlendState *pSpuBlendState;
    D3D11_BLEND_DESC spuBlendDesc = { 0 };
    spuBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    spuBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    /* output colors */
    spuBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    spuBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; /* keep source intact */
    spuBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; /* RGB colors + inverse alpha (255 is full opaque) */
    /* output alpha  */
    spuBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    spuBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; /* keep source intact */
    spuBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; /* discard */

    hr = ID3D11Device_CreateBlendState(sys->d3d_dev.d3ddevice, &spuBlendDesc, &pSpuBlendState);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create SPU blend state. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    ID3D11DeviceContext_OMSetBlendState(sys->d3d_dev.d3dcontext, pSpuBlendState, NULL, 0xFFFFFFFF);
    ID3D11BlendState_Release(pSpuBlendState);

    /* disable depth testing as we're only doing 2D
     * see https://msdn.microsoft.com/en-us/library/windows/desktop/bb205074%28v=vs.85%29.aspx
     * see http://rastertek.com/dx11tut11.html
    */
    D3D11_DEPTH_STENCIL_DESC stencilDesc;
    ZeroMemory(&stencilDesc, sizeof(stencilDesc));

    ID3D11DepthStencilState *pDepthStencilState;
    hr = ID3D11Device_CreateDepthStencilState(sys->d3d_dev.d3ddevice, &stencilDesc, &pDepthStencilState );
    if (SUCCEEDED(hr)) {
        ID3D11DeviceContext_OMSetDepthStencilState(sys->d3d_dev.d3dcontext, pDepthStencilState, 0);
        ID3D11DepthStencilState_Release(pDepthStencilState);
    }

    hr = UpdateBackBuffer(vd);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not update the backbuffer. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }

    if (sys->regionQuad.textureFormat != NULL)
    {
        hr = D3D11_CompilePixelShader(vd, &sys->hd3d, sys->legacy_shader, &sys->d3d_dev,
                                      &sys->display, TRANSFER_FUNC_SRGB, COLOR_PRIMARIES_SRGB, true,
                                      &sys->regionQuad);
        if (FAILED(hr))
        {
            D3D11_ReleasePixelShader(&sys->picQuad);
            msg_Err(vd, "Failed to create the SPU pixel shader. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    hr = D3D11_CompileFlatVertexShader(vd, &sys->hd3d, &sys->d3d_dev, &sys->flatVShader);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the vertex input layout. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    hr = D3D11_CompileProjectionVertexShader(vd, &sys->hd3d, &sys->d3d_dev, &sys->projectionVShader);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the projection vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    UpdatePicQuadPosition(vd);

    msg_Dbg(vd, "Direct3D11 resources created");
    return VLC_SUCCESS;
}

static void Direct3D11DestroyPool(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
    {
        picture_pool_Release(sys->pool);
        sys->pool = NULL;
    }
}

static void Direct3D11DestroyResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyPool(vd);

    D3D11_ReleaseQuad(&sys->picQuad);
    Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
    sys->d3dregion_count = 0;

    ReleasePictureSys(&sys->stagingSys);

    D3D11_ReleaseVertexShader(&sys->flatVShader);
    D3D11_ReleaseVertexShader(&sys->projectionVShader);

    D3D11_ReleasePixelShader(&sys->regionQuad);
    for (size_t i=0; i < ARRAY_SIZE(sys->internal_swapchain.swapchainTargetView); i++)
    {
        if (sys->internal_swapchain.swapchainTargetView[i]) {
            ID3D11RenderTargetView_Release(sys->internal_swapchain.swapchainTargetView[i]);
            sys->internal_swapchain.swapchainTargetView[i] = NULL;
        }
    }
    if (sys->prepareWait)
    {
        ID3D11Query_Release(sys->prepareWait);
        sys->prepareWait = NULL;
    }

    msg_Dbg(vd, "Direct3D11 resources destroyed");
}

static void Direct3D11DeleteRegions(int count, picture_t **region)
{
    for (int i = 0; i < count; ++i) {
        if (region[i]) {
            picture_Release(region[i]);
        }
    }
    free(region);
}

static void DestroyPictureQuad(picture_t *p_picture)
{
    D3D11_ReleaseQuad( (d3d_quad_t *) p_picture->p_sys );
}

static int Direct3D11MapSubpicture(vout_display_t *vd, int *subpicture_region_count,
                                   picture_t ***region, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    D3D11_TEXTURE2D_DESC texDesc;
    HRESULT hr;
    int err;

    if (sys->regionQuad.textureFormat == NULL)
        return VLC_EGENERIC;

    int count = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
        count++;

    *region = calloc(count, sizeof(picture_t *));
    if (unlikely(*region==NULL))
        return VLC_ENOMEM;
    *subpicture_region_count = count;

    int i = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next, i++) {
        if (!r->fmt.i_visible_width || !r->fmt.i_visible_height)
            continue; // won't render anything, keep the cache for later

        for (int j = 0; j < sys->d3dregion_count; j++) {
            picture_t *cache = sys->d3dregions[j];
            if (cache != NULL && ((d3d_quad_t *) cache->p_sys)->picSys.texture[KNOWN_DXGI_INDEX]) {
                ID3D11Texture2D_GetDesc( ((d3d_quad_t *) cache->p_sys)->picSys.texture[KNOWN_DXGI_INDEX], &texDesc );
                if (texDesc.Format == sys->regionQuad.textureFormat->formatTexture &&
                    texDesc.Width  == r->p_picture->format.i_width &&
                    texDesc.Height == r->p_picture->format.i_height) {
                    (*region)[i] = cache;
                    memset(&sys->d3dregions[j], 0, sizeof(cache)); // do not reuse this cached value a second time
                    break;
                }
            }
        }

        RECT output;
        output.left   = r->fmt.i_x_offset;
        output.right  = r->fmt.i_x_offset + r->fmt.i_visible_width;
        output.top    = r->fmt.i_y_offset;
        output.bottom = r->fmt.i_y_offset + r->fmt.i_visible_height;

        picture_t *quad_picture = (*region)[i];
        if (quad_picture == NULL) {
            d3d_quad_t *d3dquad = calloc(1, sizeof(*d3dquad));
            if (unlikely(d3dquad==NULL)) {
                continue;
            }
            if (AllocateTextures(vd, &sys->d3d_dev, sys->regionQuad.textureFormat, &r->p_picture->format, 1, d3dquad->picSys.texture)) {
                msg_Err(vd, "Failed to allocate %dx%d texture for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                for (int j=0; j<D3D11_MAX_SHADER_VIEW; j++)
                    if (d3dquad->picSys.texture[j])
                        ID3D11Texture2D_Release(d3dquad->picSys.texture[j]);
                free(d3dquad);
                continue;
            }

            if (D3D11_AllocateResourceView(vd, sys->d3d_dev.d3ddevice, sys->regionQuad.textureFormat,
                                           d3dquad->picSys.texture, 0,
                                           d3dquad->picSys.renderSrc)) {
                msg_Err(vd, "Failed to create %dx%d shader view for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                free(d3dquad);
                continue;
            }
            d3dquad->i_width    = r->fmt.i_width;
            d3dquad->i_height   = r->fmt.i_height;

            d3dquad->textureFormat = sys->regionQuad.textureFormat;
            err = D3D11_AllocateQuad(vd, &sys->d3d_dev, PROJECTION_MODE_RECTANGULAR, d3dquad);
            if (err != VLC_SUCCESS)
            {
                msg_Err(vd, "Failed to allocate %dx%d quad for OSD",
                             r->fmt.i_visible_width, r->fmt.i_visible_height);
                free(d3dquad);
                continue;
            }

            err = D3D11_SetupQuad( vd, &sys->d3d_dev, &r->fmt, d3dquad, &sys->display, &output,
                                   ORIENT_NORMAL );
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to setup %dx%d quad for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                free(d3dquad);
                continue;
            }
            picture_resource_t picres = {
                .p_sys      = (picture_sys_t *) d3dquad,
                .pf_destroy = DestroyPictureQuad,
            };
            (*region)[i] = picture_NewFromResource(&r->p_picture->format, &picres);
            if ((*region)[i] == NULL) {
                msg_Err(vd, "Failed to create %dx%d picture for OSD",
                        r->fmt.i_width, r->fmt.i_height);
                D3D11_ReleaseQuad(d3dquad);
                continue;
            }
            quad_picture = (*region)[i];
            for (size_t j=0; j<D3D11_MAX_SHADER_VIEW; j++)
            {
                /* TODO use something more accurate if we have different formats */
                if (sys->regionQuad.d3dpixelShader[j])
                {
                    d3dquad->d3dpixelShader[j] = sys->regionQuad.d3dpixelShader[j];
                    ID3D11PixelShader_AddRef(d3dquad->d3dpixelShader[j]);
                }
            }
        } else {
            D3D11_UpdateQuadPosition(vd, &sys->d3d_dev, (d3d_quad_t *) quad_picture->p_sys, &output, ORIENT_NORMAL);
        }

        hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, ((d3d_quad_t *) quad_picture->p_sys)->picSys.resource[KNOWN_DXGI_INDEX], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if( SUCCEEDED(hr) ) {
            err = picture_UpdatePlanes(quad_picture, mappedResource.pData, mappedResource.RowPitch);
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to set the buffer on the SPU picture" );
                ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, ((d3d_quad_t *) quad_picture->p_sys)->picSys.resource[KNOWN_DXGI_INDEX], 0);
                picture_Release(quad_picture);
                if ((*region)[i] == quad_picture)
                    (*region)[i] = NULL;
                continue;
            }

            picture_CopyPixels(quad_picture, r->p_picture);

            ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, ((d3d_quad_t *) quad_picture->p_sys)->picSys.resource[KNOWN_DXGI_INDEX], 0);
        } else {
            msg_Err(vd, "Failed to map the SPU texture (hr=0x%lX)", hr );
            picture_Release(quad_picture);
            if ((*region)[i] == quad_picture)
                (*region)[i] = NULL;
            continue;
        }

        d3d_quad_t *quad = (d3d_quad_t *) quad_picture->p_sys;

        RECT spuViewport;
        spuViewport.left   = (FLOAT) r->i_x * sys->area.place.width  / subpicture->i_original_picture_width;
        spuViewport.top    = (FLOAT) r->i_y * sys->area.place.height / subpicture->i_original_picture_height;
        spuViewport.right  = (FLOAT) (r->i_x + r->fmt.i_visible_width)  * sys->area.place.width  / subpicture->i_original_picture_width;
        spuViewport.bottom = (FLOAT) (r->i_y + r->fmt.i_visible_height) * sys->area.place.height / subpicture->i_original_picture_height;

        if (r->zoom_h.num != 0 && r->zoom_h.den != 0)
        {
            spuViewport.left   = (FLOAT) spuViewport.left   * r->zoom_h.num / r->zoom_h.den;
            spuViewport.right  = (FLOAT) spuViewport.right  * r->zoom_h.num / r->zoom_h.den;
        }
        if (r->zoom_v.num != 0 && r->zoom_v.den != 0)
        {
            spuViewport.top    = (FLOAT) spuViewport.top    * r->zoom_v.num / r->zoom_v.den;
            spuViewport.bottom = (FLOAT) spuViewport.bottom * r->zoom_v.num / r->zoom_v.den;
        }

        /* move the SPU inside the video area */
        spuViewport.left   += sys->area.place.x;
        spuViewport.right  += sys->area.place.x;
        spuViewport.top    += sys->area.place.y;
        spuViewport.bottom += sys->area.place.y;

        D3D11_UpdateViewport( quad, &spuViewport, sys->display.pixelFormat );

        D3D11_UpdateQuadOpacity(vd, &sys->d3d_dev, quad, r->i_alpha / 255.0f );
    }
    return VLC_SUCCESS;
}

