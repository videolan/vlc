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

#if _WIN32_WINNT < 0x0601 // _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include <assert.h>
#include <math.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d11.h>
#ifdef HAVE_DXGI1_6_H
# include <dxgi1_6.h>
#else
# include <dxgi1_5.h>
#endif
#ifdef HAVE_D3D11_4_H
#include <d3d11_4.h>
#endif

/* avoided until we can pass ISwapchainPanel without c++/cx mode
# include <windows.ui.xaml.media.dxinterop.h> */

#include "../../video_chroma/d3d11_fmt.h"
#include "d3d11_quad.h"
#include "d3d11_shaders.h"
#include "d3d11_scaler.h"
#include "d3d11_tonemap.h"

#include "common.h"

DEFINE_GUID(GUID_SWAPCHAIN_WIDTH,  0xf1b59347, 0x1643, 0x411a, 0xad, 0x6b, 0xc7, 0x80, 0x17, 0x7a, 0x06, 0xb6);
DEFINE_GUID(GUID_SWAPCHAIN_HEIGHT, 0x6ea976a0, 0x9d60, 0x4bb7, 0xa5, 0xa9, 0x7d, 0xd1, 0x18, 0x7f, 0xc9, 0xbd);

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

#define D3D11_HELP N_("Recommended video output for Windows 8 and later versions")
#define HW_BLENDING_TEXT N_("Use hardware blending support")
#define HW_BLENDING_LONGTEXT N_(\
    "Try to use hardware acceleration for subtitle/OSD blending.")

#define UPSCALE_MODE_TEXT N_("Video Upscaling Mode")
#define UPSCALE_MODE_LONGTEXT N_("Select the upscaling mode for video.")

static const char *const ppsz_upscale_mode[] = {
    "linear", "point", "processor", "super" };
static const char *const ppsz_upscale_mode_text[] = {
    N_("Linear Sampler"), N_("Point Sampler"), N_("Video Processor"), N_("Super Resolution") };

#define HDR_MODE_TEXT N_("HDR Output Mode")
#define HDR_MODE_LONGTEXT N_("Use HDR output even if the source is SDR.")

static const char *const ppsz_hdr_mode[] = {
    "auto", "never", "always", "generate" };
static const char *const ppsz_hdr_mode_text[] = {
    N_("Auto"), N_("Never out HDR"), N_("Always output HDR"), N_("Generate HDR from SDR") };

vlc_module_begin ()
    set_shortname("Direct3D11")
    set_description(N_("Direct3D11 video output"))
    set_help(D3D11_HELP)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_bool("direct3d11-hw-blending", true, HW_BLENDING_TEXT, HW_BLENDING_LONGTEXT, true)

    add_integer("winrt-d3dcontext",    0x0, NULL, NULL, true) /* ID3D11DeviceContext* */
        change_volatile()
    add_integer("winrt-swapchain",     0x0, NULL, NULL, true) /* IDXGISwapChain1*     */
        change_volatile()

    add_string("d3d11-upscale-mode", "linear", UPSCALE_MODE_TEXT, UPSCALE_MODE_LONGTEXT, false)
        change_string_list(ppsz_upscale_mode, ppsz_upscale_mode_text)

    add_string("d3d11-hdr-mode", "auto", HDR_MODE_TEXT, HDR_MODE_LONGTEXT, false)
        change_string_list(ppsz_hdr_mode, ppsz_hdr_mode_text)

    set_capability("vout display", 300)
    add_shortcut("direct3d11")
    set_callbacks(Open, Close)
vlc_module_end ()

enum d3d11_upscale
{
    upscale_LinearSampler,
    upscale_PointSampler,
    upscale_VideoProcessor,
    upscale_SuperResolution,
};

enum d3d11_hdr
{
    hdr_Auto,
    hdr_Never,
    hdr_Always,
    hdr_Fake,
};

struct vout_display_sys_t
{
    vout_display_sys_win32_t sys;
    video_format_t           pool_fmt;
    const d3d_format_t       *pool_d3dfmt;

    int                      log_level;

    display_info_t           display;

    HINSTANCE                hdxgi_dll;        /* handle of the opened dxgi dll */
    d3d11_handle_t           hd3d;
    IDXGISwapChain1          *dxgiswapChain;   /* DXGI 1.2 swap chain */
    IDXGISwapChain4          *dxgiswapChain4;  /* DXGI 1.5 for HDR */
    DXGI_HDR_METADATA_HDR10  hdr10;
    d3d11_device_t           d3d_dev;
    d3d_quad_t               picQuad;
    video_format_t           quad_fmt;

#ifdef HAVE_D3D11_4_H
    ID3D11Fence              *d3dRenderFence;
    ID3D11DeviceContext4     *d3dcontext4;
    UINT64                   renderFence;
    HANDLE                   renderFinished;
#endif

    picture_sys_t            stagingSys;

    ID3D11RenderTargetView   *d3drenderTargetView;
    ID3D11DepthStencilView   *d3ddepthStencilView;

    ID3D11InputLayout         *pVertexLayout;
    ID3D11VertexShader        *flatVSShader;
    ID3D11VertexShader        *projectionVSShader;

    /* copy from the decoder pool into picSquad before display
     * Uses a Texture2D with slices rather than a Texture2DArray for the decoder */
    bool                     legacy_shader;

    // SPU
    vlc_fourcc_t             pSubpictureChromas[2];
    ID3D11PixelShader        *pSPUPixelShader;
    const d3d_format_t       *d3dregion_format;
    int                      d3dregion_count;
    picture_t                **d3dregions;

    // upscaling
    enum d3d11_upscale       upscaleMode;
    struct d3d11_scaler      *scaleProc;

    // HDR mode
    enum d3d11_hdr           hdrMode;
    struct d3d11_tonemapper  *tonemapProc;
};

#define RECTWidth(r)   (int)((r).right - (r).left)
#define RECTHeight(r)  (int)((r).bottom - (r).top)

static picture_pool_t *Pool(vout_display_t *, unsigned);

static void Prepare(vout_display_t *, picture_t *, subpicture_t *subpicture);
static void Display(vout_display_t *, picture_t *, subpicture_t *subpicture);

static void Direct3D11Destroy(vout_display_t *);

static int  Direct3D11Open (vout_display_t *, bool external_device);
static void Direct3D11Close(vout_display_t *);

static int SetupOutputFormat(vout_display_t *, video_format_t *decoder, video_format_t *quad);
static int  Direct3D11CreateFormatResources (vout_display_t *, const video_format_t *);
static int  Direct3D11CreateGenericResources(vout_display_t *);
static void Direct3D11DestroyResources(vout_display_t *);

static void DestroyDisplayPoolPicture(picture_t *);
static void Direct3D11DeleteRegions(int, picture_t **);
static int Direct3D11MapSubpicture(vout_display_t *, int *, picture_t ***, subpicture_t *);

static void SetQuadVSProjection(vout_display_t *, d3d_quad_t *, const vlc_viewpoint_t *);
static void UpdatePicQuadPosition(vout_display_t *);

static int Control(vout_display_t *, int, va_list);
static void Manage(vout_display_t *vd);

static int Direct3D11MapPoolTexture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr;

    ID3D11Device *dev;
    ID3D11DeviceContext_GetDevice(p_sys->context, &dev);
    ID3D11Device_Release(dev);

#ifndef NDEBUG
    D3D11_TEXTURE2D_DESC dsc;
    ID3D11Texture2D_GetDesc(p_sys->texture[KNOWN_DXGI_INDEX], &dsc);
    assert(dsc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE);
    assert(dsc.Usage & D3D11_USAGE_DYNAMIC);
#endif

    hr = ID3D11DeviceContext_Map(p_sys->context, p_sys->resource[KNOWN_DXGI_INDEX], p_sys->slice_index, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if( FAILED(hr) )
    {
        return VLC_EGENERIC;
    }
    return CommonUpdatePicture(picture, NULL, mappedResource.pData, mappedResource.RowPitch);
}

static void Direct3D11UnmapPoolTexture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    ID3D11DeviceContext_Unmap(p_sys->context, p_sys->resource[KNOWN_DXGI_INDEX], 0);
}

static bool GetWinRTSize(const vout_display_sys_win32_t *p_sys, UINT *w, UINT *h)
{
    const vout_display_sys_t *sys = (const vout_display_sys_t *)p_sys;
    uint32_t i_width;
    uint32_t i_height;
    UINT dataSize = sizeof(i_width);
    HRESULT hr = IDXGISwapChain_GetPrivateData(sys->dxgiswapChain, &GUID_SWAPCHAIN_WIDTH, &dataSize, &i_width);
    if (FAILED(hr)) {
        return false;
    }
    dataSize = sizeof(i_height);
    hr = IDXGISwapChain_GetPrivateData(sys->dxgiswapChain, &GUID_SWAPCHAIN_HEIGHT, &dataSize, &i_height);
    if (FAILED(hr)) {
        return false;
    }
    *w = i_width;
    *h = i_height;
    return true;
}

static int OpenCoreW(vout_display_t *vd)
{
    IDXGISwapChain1* dxgiswapChain  = (void*)var_InheritInteger(vd, "winrt-swapchain");
    if (!dxgiswapChain)
        return VLC_EGENERIC;
    ID3D11DeviceContext* d3dcontext = (void*)var_InheritInteger(vd, "winrt-d3dcontext");
    if (!d3dcontext)
        return VLC_EGENERIC;
    ID3D11Device* d3ddevice = NULL;
    ID3D11DeviceContext_GetDevice(d3dcontext, &d3ddevice);
    if (!d3ddevice)
        return VLC_EGENERIC;

    vout_display_sys_t *sys = vd->sys;
    sys->dxgiswapChain = dxgiswapChain;
    sys->d3d_dev.d3ddevice     = d3ddevice;
    sys->d3d_dev.d3dcontext    = d3dcontext;
    sys->d3d_dev.feature_level = ID3D11Device_GetFeatureLevel(sys->d3d_dev.d3ddevice );
    IDXGISwapChain_AddRef     (sys->dxgiswapChain);
    ID3D11DeviceContext_AddRef(sys->d3d_dev.d3dcontext);

    sys->sys.pf_GetWindowSize = GetWinRTSize;

    return VLC_SUCCESS;
}

static unsigned int GetPictureWidth(const vout_display_t *vd)
{
    return vd->sys->picQuad.i_width;
}

static unsigned int GetPictureHeight(const vout_display_t *vd)
{
    return vd->sys->picQuad.i_height;
}

static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;

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

    vout_display_sys_t *sys = vd->sys = calloc(1, sizeof(vout_display_sys_t));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    int ret = D3D11_Create(vd, &sys->hd3d, true);
    if (unlikely(ret != VLC_SUCCESS))
        goto error;

    ret = OpenCoreW(vd);
    bool external_device = ret == VLC_SUCCESS;
#if VLC_WINSTORE_APP
    if (!external_device)
        return ret;
#endif

    if (external_device)
        sys->sys.src_fmt = &vd->source;
    else if (CommonInit(vd) != VLC_SUCCESS)
        goto error;

    vd->sys->sys.pf_GetPictureWidth  = GetPictureWidth;
    vd->sys->sys.pf_GetPictureHeight = GetPictureHeight;

    if (Direct3D11Open(vd, external_device)) {
        msg_Err(vd, "Direct3D11 could not be opened");
        goto error;
    }

#if !VLC_WINSTORE_APP
    if (!external_device)
        EventThreadUpdateTitle(vd->sys->sys.event, VOUT_TITLE " (Direct3D11 output)");
#endif
    msg_Dbg(vd, "Direct3D11 device adapter successfully initialized");

    vd->info.has_double_click     = true;
    vd->info.has_pictures_invalid = vd->info.is_slow;

    if (var_InheritBool(vd, "direct3d11-hw-blending") &&
        vd->sys->d3dregion_format != NULL)
    {
        vd->sys->pSubpictureChromas[0] = vd->sys->d3dregion_format->fourcc;
        vd->sys->pSubpictureChromas[1] = 0;
        vd->info.subpicture_chromas = vd->sys->pSubpictureChromas;
    }
    else
        vd->info.subpicture_chromas = NULL;

    vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    msg_Dbg(vd, "Direct3D11 Open Succeeded");

    return VLC_SUCCESS;

error:
    Close(object);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *object)
{
    vout_display_t * vd = (vout_display_t *)object;

    Direct3D11Close(vd);
    CommonClean(vd);
    Direct3D11Destroy(vd);
    free(vd->sys);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned pool_size)
{
    /* compensate for extra hardware decoding pulling extra pictures from our pool */
    pool_size += 2;

    vout_display_sys_t *sys = vd->sys;
    picture_t **pictures = NULL;
    picture_t *picture;
    unsigned  picture_count = 0;

    if (sys->sys.pool)
        return sys->sys.pool;

    if (vd->info.is_slow)
        pool_size = 1;

    if (D3D11_SetupQuad( vd, &sys->d3d_dev, &sys->quad_fmt, &sys->picQuad, &sys->display, &sys->sys.rect_src_clipped,
                   vd->fmt.projection_mode == PROJECTION_MODE_RECTANGULAR ? sys->flatVSShader : sys->projectionVSShader,
                   sys->pVertexLayout,
                   sys->quad_fmt.projection_mode, vd->fmt.orientation ) != VLC_SUCCESS) {
        msg_Err(vd, "Could not Create the main quad picture.");
        return NULL;
    }

    if ( vd->fmt.projection_mode == PROJECTION_MODE_EQUIRECTANGULAR ||
         vd->fmt.projection_mode == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD )
        SetQuadVSProjection( vd, &sys->picQuad, &vd->cfg->viewpoint );

    if (!vd->info.is_slow) {
        HRESULT           hr;
        ID3D10Multithread *pMultithread;
        hr = ID3D11Device_QueryInterface( sys->d3d_dev.d3ddevice, &IID_ID3D10Multithread, (void **)&pMultithread);
        if (SUCCEEDED(hr)) {
            ID3D10Multithread_SetMultithreadProtected(pMultithread, TRUE);
            ID3D10Multithread_Release(pMultithread);
        }
    }

    if (sys->pool_d3dfmt->formatTexture == DXGI_FORMAT_UNKNOWN)
        sys->sys.pool = picture_pool_NewFromFormat( &sys->pool_fmt, pool_size );
    else
    {
        ID3D11Texture2D  *textures[pool_size * D3D11_MAX_SHADER_VIEW];
        memset(textures, 0, sizeof(textures));
        unsigned slices = pool_size;
        if (!CanUseVoutPool(&sys->d3d_dev, pool_size))
            /* only provide enough for the filters, we can still do direct rendering */
            slices = __MIN(slices, 6);

        if (AllocateTextures(vd, &sys->d3d_dev, sys->pool_d3dfmt, &sys->pool_fmt, slices, textures))
            goto error;

        pictures = calloc(pool_size, sizeof(*pictures));
        if (!pictures)
            goto error;

        for (picture_count = 0; picture_count < pool_size; picture_count++) {
            picture_sys_t *picsys = calloc(1, sizeof(*picsys));
            if (unlikely(picsys == NULL))
                goto error;

            for (unsigned plane = 0; plane < D3D11_MAX_SHADER_VIEW; plane++)
            {
                if (picture_count < slices)
                    picsys->texture[plane] =textures[picture_count * D3D11_MAX_SHADER_VIEW + plane];
                else if (textures[plane])
                {
                    picsys->texture[plane] = textures[plane];
                    ID3D11Texture2D_AddRef(picsys->texture[plane]);
                }
            }

            picsys->slice_index = picture_count < slices ? picture_count : 0;
            picsys->formatTexture = sys->pool_d3dfmt->formatTexture;
            picsys->context = sys->d3d_dev.d3dcontext;

            picture_resource_t resource = {
                .p_sys = picsys,
                .pf_destroy = DestroyDisplayPoolPicture,
            };

            picture = picture_NewFromResource(&sys->pool_fmt, &resource);
            if (unlikely(picture == NULL)) {
                free(picsys);
                msg_Err( vd, "Failed to create picture %d in the pool.", picture_count );
                goto error;
            }

            pictures[picture_count] = picture;
            /* each picture_t holds a ref to the context and release it on Destroy */
            ID3D11DeviceContext_AddRef(picsys->context);
        }

#ifdef HAVE_ID3D11VIDEODECODER
        if (is_d3d11_opaque(sys->pool_fmt.i_chroma) && !sys->legacy_shader)
#endif
        {
            sys->picQuad.resourceCount = DxgiResourceCount(sys->pool_d3dfmt);
            for (picture_count = 0; picture_count < slices; picture_count++) {
                if (!pictures[picture_count]->p_sys->texture[0])
                    continue;
                if (D3D11_AllocateShaderView(vd, sys->d3d_dev.d3ddevice, sys->pool_d3dfmt,
                                       pictures[picture_count]->p_sys->texture, picture_count,
                                       pictures[picture_count]->p_sys->resourceView))
                    goto error;
            }
        }

        picture_pool_configuration_t pool_cfg = {
            .picture       = pictures,
            .picture_count = pool_size,
        };
        if (vd->info.is_slow && !is_d3d11_opaque(sys->pool_fmt.i_chroma)) {
            pool_cfg.lock          = Direct3D11MapPoolTexture;
            //pool_cfg.unlock        = Direct3D11UnmapPoolTexture;
        }
        sys->sys.pool = picture_pool_NewExtended( &pool_cfg );
    }

error:
    if (sys->sys.pool == NULL) {
        picture_pool_configuration_t pool_cfg = {
            .picture_count = 0,
        };
        if (pictures) {
            msg_Dbg(vd, "Failed to create the picture d3d11 pool");
            for (unsigned i=0;i<picture_count; ++i)
                picture_Release(pictures[i]);
            free(pictures);
        }

        /* create an empty pool to avoid crashing */
        sys->sys.pool = picture_pool_NewExtended( &pool_cfg );
    } else {
        msg_Dbg(vd, "D3D11 pool succeed with %d surfaces (%dx%d) context 0x%p",
                pool_size, sys->pool_fmt.i_width, sys->pool_fmt.i_height, sys->d3d_dev.d3dcontext);
    }
    return sys->sys.pool;
}

static void DestroyDisplayPoolPicture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    ReleasePictureSys( p_sys );
    free(p_sys);
    free(picture);
}

#if !VLC_WINSTORE_APP
static void FillSwapChainDesc(vout_display_t *vd, DXGI_SWAP_CHAIN_DESC1 *out)
{
    vout_display_sys_t *sys = vd->sys;
    ZeroMemory(out, sizeof(*out));
    out->BufferCount = 3;
    out->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    out->SampleDesc.Count = 1;
    out->SampleDesc.Quality = 0;
    out->Width = vd->source.i_visible_width;
    out->Height = vd->source.i_visible_height;
    out->Format = DXGI_FORMAT_R8G8B8A8_UNORM; /* TODO: use DXGI_FORMAT_NV12 */
    if (sys->hdrMode == hdr_Always || sys->hdrMode == hdr_Fake)
        out->Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    else if (sys->hdrMode == hdr_Auto)
    {
        if ( vd->source.i_chroma == VLC_CODEC_D3D11_OPAQUE_10B ||
             vd->source.transfer == TRANSFER_FUNC_SMPTE_ST2084 ||
             vd->source.transfer == TRANSFER_FUNC_HLG)
            out->Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    }
    //out->Flags = 512; // DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO;

    bool isWin10OrGreater = false;
    HMODULE hKernelBase = GetModuleHandle(TEXT("kernelbase.dll"));
    if (likely(hKernelBase != NULL))
    {
        isWin10OrGreater = GetProcAddress(hKernelBase, "VirtualAllocFromApp") != NULL;
        FreeLibrary(hKernelBase);
    }
    if (isWin10OrGreater)
        out->SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    else
    {
        bool isWin81OrGreater = false;
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
        if (likely(hKernel32 != NULL))
            isWin81OrGreater = GetProcAddress(hKernel32, "IsProcessCritical") != NULL;
        if (isWin81OrGreater)
            out->SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        else
        {
            out->SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            out->BufferCount = 1;
        }
    }
}
#endif

static int UpdateSamplers(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    D3D11_SAMPLER_DESC sampDesc;
    memset(&sampDesc, 0, sizeof(sampDesc));
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    d3d11_device_lock(&sys->d3d_dev);

    ID3D11SamplerState *d3dsampState[2];
    if (sys->upscaleMode == upscale_PointSampler)
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    else
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    hr = ID3D11Device_CreateSamplerState(sys->d3d_dev.d3ddevice, &sampDesc, &d3dsampState[0]);
    if (FAILED(hr)) {
        msg_Err(vd, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
        d3d11_device_unlock(&sys->d3d_dev);
        return VLC_EGENERIC;
    }

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = ID3D11Device_CreateSamplerState(sys->d3d_dev.d3ddevice, &sampDesc, &d3dsampState[1]);
    if (FAILED(hr)) {
        msg_Err(vd, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
        ID3D11SamplerState_Release(d3dsampState[0]);
        d3d11_device_unlock(&sys->d3d_dev);
        return VLC_EGENERIC;
    }

    ID3D11DeviceContext_PSSetSamplers(sys->d3d_dev.d3dcontext, 0, 2, d3dsampState);
    ID3D11SamplerState_Release(d3dsampState[0]);
    ID3D11SamplerState_Release(d3dsampState[1]);

    d3d11_device_unlock(&sys->d3d_dev);
    return VLC_SUCCESS;
}

static HRESULT UpdateBackBuffer(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    ID3D11Texture2D* pDepthStencil;
    ID3D11Texture2D* pBackBuffer;
    UINT window_width, window_height;
    bool proc_upscale = sys->upscaleMode == upscale_VideoProcessor || sys->upscaleMode == upscale_SuperResolution;
    if ((sys->sys.pf_GetWindowSize != GetWinRTSize && !proc_upscale)
    || !sys->sys.pf_GetWindowSize(&sys->sys, &window_width, &window_height))
    {
        window_width  = RECTWidth(sys->sys.rect_dest_clipped);
        window_height = RECTHeight(sys->sys.rect_dest_clipped);
    }

    if (proc_upscale)
    {
        vout_display_cfg_t cfg = *vd->cfg;
        cfg.display.width = window_width;
        cfg.display.height = window_height;
        D3D11_UpscalerUpdate(VLC_OBJECT(vd), sys->scaleProc, &sys->d3d_dev,
                             &sys->pool_fmt, &sys->quad_fmt, &cfg);
    }

    D3D11_TEXTURE2D_DESC dsc = { 0 };

    if (sys->d3drenderTargetView) {
        ID3D11Resource *res = NULL;
        ID3D11RenderTargetView_GetResource(sys->d3drenderTargetView, &res);
        if (res)
        {
            ID3D11Texture2D_GetDesc((ID3D11Texture2D*) res, &dsc);
            ID3D11Resource_Release(res);
        }
    }

    if (dsc.Width == window_width && dsc.Height == window_height)
        return S_OK; /* nothing changed */

    if (sys->d3drenderTargetView) {
        ID3D11RenderTargetView_Release(sys->d3drenderTargetView);
        sys->d3drenderTargetView = NULL;
    }
    if (sys->d3ddepthStencilView) {
        ID3D11DepthStencilView_Release(sys->d3ddepthStencilView);
        sys->d3ddepthStencilView = NULL;
    }

    /* TODO detect is the size is the same as the output and switch to fullscreen mode */
    hr = IDXGISwapChain_ResizeBuffers(sys->dxgiswapChain, 0, window_width, window_height,
        DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
       msg_Err(vd, "Failed to resize the backbuffer. (hr=0x%lX)", hr);
       return hr;
    }

    hr = IDXGISwapChain_GetBuffer(sys->dxgiswapChain, 0, &IID_ID3D11Texture2D, (LPVOID *)&pBackBuffer);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not get the backbuffer for the Swapchain. (hr=0x%lX)", hr);
       return hr;
    }

    hr = ID3D11Device_CreateRenderTargetView(sys->d3d_dev.d3ddevice, (ID3D11Resource *)pBackBuffer, NULL, &sys->d3drenderTargetView);
    ID3D11Texture2D_Release(pBackBuffer);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to create the target view. (hr=0x%lX)", hr);
        return hr;
    }

    D3D11_TEXTURE2D_DESC deptTexDesc;
    memset(&deptTexDesc, 0,sizeof(deptTexDesc));
    deptTexDesc.ArraySize = 1;
    deptTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    deptTexDesc.CPUAccessFlags = 0;
    deptTexDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    deptTexDesc.Width = window_width;
    deptTexDesc.Height = window_height;
    deptTexDesc.MipLevels = 1;
    deptTexDesc.MiscFlags = 0;
    deptTexDesc.SampleDesc.Count = 1;
    deptTexDesc.SampleDesc.Quality = 0;
    deptTexDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = ID3D11Device_CreateTexture2D(sys->d3d_dev.d3ddevice, &deptTexDesc, NULL, &pDepthStencil);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the depth stencil texture. (hr=0x%lX)", hr);
       return hr;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc;
    memset(&depthViewDesc, 0, sizeof(depthViewDesc));

    depthViewDesc.Format = deptTexDesc.Format;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Texture2D.MipSlice = 0;

    hr = ID3D11Device_CreateDepthStencilView(sys->d3d_dev.d3ddevice, (ID3D11Resource *)pDepthStencil, &depthViewDesc, &sys->d3ddepthStencilView);
    ID3D11Texture2D_Release(pDepthStencil);

    if (FAILED(hr)) {
       msg_Err(vd, "Could not create the depth stencil view. (hr=0x%lX)", hr);
       return hr;
    }

    return S_OK;
}

/* rotation around the Z axis */
static void getZRotMatrix(float theta, FLOAT matrix[static 16])
{
    float st, ct;

    sincosf(theta, &st, &ct);

    const FLOAT m[] = {
    /*  x    y    z    w */
        ct,  -st, 0.f, 0.f,
        st,  ct,  0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

/* rotation around the Y axis */
static void getYRotMatrix(float theta, FLOAT matrix[static 16])
{
    float st, ct;

    sincosf(theta, &st, &ct);

    const FLOAT m[] = {
    /*  x    y    z    w */
        ct,  0.f, -st, 0.f,
        0.f, 1.f, 0.f, 0.f,
        st,  0.f, ct,  0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

/* rotation around the X axis */
static void getXRotMatrix(float phi, FLOAT matrix[static 16])
{
    float sp, cp;

    sincosf(phi, &sp, &cp);

    const FLOAT m[] = {
    /*  x    y    z    w */
        1.f, 0.f, 0.f, 0.f,
        0.f, cp,  sp,  0.f,
        0.f, -sp, cp,  0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
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
    if (!quad->pVertexShaderConstants)
        return;

#define RAD(d) ((float) ((d) * M_PI / 180.f))
    float f_fovx = RAD(p_vp->fov);
    if ( f_fovx > FIELD_OF_VIEW_DEGREES_MAX * M_PI / 180 + 0.001f ||
         f_fovx < -0.001f )
        return;

    float f_sar = (float) vd->cfg->display.width / vd->cfg->display.height;
    float f_teta = RAD(p_vp->yaw) - (float) M_PI_2;
    float f_phi  = RAD(p_vp->pitch);
    float f_roll = RAD(p_vp->roll);
    float f_fovy = UpdateFOVy(f_fovx, f_sar);
    float f_z = UpdateZ(f_fovx, f_fovy);

    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        VS_PROJECTION_CONST *dst_data = mapped.pData;
        getXRotMatrix(f_phi, dst_data->RotX);
        getYRotMatrix(f_teta,   dst_data->RotY);
        getZRotMatrix(f_roll,  dst_data->RotZ);
        getZoomMatrix(SPHERE_RADIUS * f_z, dst_data->View);
        getProjectionMatrix(f_sar, f_fovy, dst_data->Projection);
    }
    ID3D11DeviceContext_Unmap(sys->d3d_dev.d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0);
#undef RAD
}

static void UpdateSize(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    msg_Dbg(vd, "Detected size change %dx%d", RECTWidth(sys->sys.rect_dest_clipped),
            RECTHeight(sys->sys.rect_dest_clipped));

    UpdateBackBuffer(vd);

    d3d11_device_lock( &sys->d3d_dev );

    UpdatePicQuadPosition(vd);

    D3D11_UpdateQuadPosition(vd, &sys->d3d_dev, &sys->picQuad, &sys->sys.rect_src_clipped,
                             vd->fmt.orientation);

    d3d11_device_unlock( &sys->d3d_dev );
}

static inline bool RectEquals(const RECT *r1, const RECT *r2)
{
    return r1->bottom == r2->bottom && r1->top == r2->top &&
           r1->left == r2->left && r1->right == r2->right;
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;
    RECT before_src_clipped  = sys->sys.rect_src_clipped;
    RECT before_dest_clipped = sys->sys.rect_dest_clipped;
    RECT before_dest         = sys->sys.rect_dest;

    if (sys->upscaleMode == upscale_VideoProcessor || sys->upscaleMode == upscale_SuperResolution)
        switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            {
                // update the source cropping
                UINT window_width, window_height;
                if (!sys->sys.pf_GetWindowSize(&sys->sys, &window_width, &window_height))
                {
                    window_width  = RECTWidth(sys->sys.rect_dest_clipped);
                    window_height = RECTHeight(sys->sys.rect_dest_clipped);
                }
                vout_display_cfg_t cfg = *vd->cfg;
                cfg.display.width = window_width;
                cfg.display.height = window_height;
                D3D11_UpscalerUpdate(VLC_OBJECT(vd), sys->scaleProc, &sys->d3d_dev, &vd->source,
                                     &sys->quad_fmt, &cfg);
            }
            break;
        }

    int res = CommonControl( vd, query, args );

    if (query == VOUT_DISPLAY_CHANGE_VIEWPOINT)
    {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t*);
        if ( sys->picQuad.pVertexShaderConstants )
        {
            SetQuadVSProjection( vd, &sys->picQuad, &cfg->viewpoint );
            res = VLC_SUCCESS;
        }
    }

    if (!RectEquals(&before_src_clipped,  &sys->sys.rect_src_clipped) ||
        !RectEquals(&before_dest_clipped, &sys->sys.rect_dest_clipped) ||
        !RectEquals(&before_dest,         &sys->sys.rect_dest) )
    {
        UpdateSize(vd);
    }

    return res;
}

static void Manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    RECT before_src_clipped  = sys->sys.rect_src_clipped;
    RECT before_dest_clipped = sys->sys.rect_dest_clipped;
    RECT before_dest         = sys->sys.rect_dest;

    CommonManage(vd);

    if (!RectEquals(&before_src_clipped, &sys->sys.rect_src_clipped) ||
        !RectEquals(&before_dest_clipped, &sys->sys.rect_dest_clipped) ||
        !RectEquals(&before_dest, &sys->sys.rect_dest))
    {
        UpdateSize(vd);
    }
}

static void CallUpdateRects(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    if (sys->scaleProc && D3D11_UpscalerUsed(sys->scaleProc))
    {
        D3D11_UpscalerGetSize(sys->scaleProc, &sys->quad_fmt.i_width, &sys->quad_fmt.i_height);

        sys->quad_fmt.i_x_offset       = 0;
        sys->quad_fmt.i_y_offset       = 0;
        sys->quad_fmt.i_visible_width  = sys->quad_fmt.i_width;
        sys->quad_fmt.i_visible_height = sys->quad_fmt.i_height;

        sys->picQuad.i_width = sys->quad_fmt.i_width;
        sys->picQuad.i_height = sys->quad_fmt.i_height;

        UpdateRects(vd, NULL, true);
        UpdateSize(vd);
    }
    else
    {
        UpdateRects(vd, NULL, true);
    }
}

static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->picQuad.formatInfo->formatTexture == DXGI_FORMAT_UNKNOWN)
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

        D3D11_TEXTURE2D_DESC srcDesc;
        ID3D11Texture2D_GetDesc(p_sys->texture[KNOWN_DXGI_INDEX], &srcDesc);

        if (is_d3d11_opaque(picture->format.i_chroma))
            d3d11_device_lock( &sys->d3d_dev );

        if (sys->tonemapProc)
        {
            if (FAILED(D3D11_TonemapperProcess(VLC_OBJECT(vd), sys->tonemapProc, p_sys)))
                return;
            p_sys = D3D11_TonemapperGetOutput(sys->tonemapProc);
        }
        if (sys->scaleProc && D3D11_UpscalerUsed(sys->scaleProc))
        {
            if (D3D11_UpscalerScale(VLC_OBJECT(vd), sys->scaleProc, p_sys) != VLC_SUCCESS)
                return;
            uint32_t witdh, height;
            D3D11_UpscalerGetSize(sys->scaleProc, &witdh, &height);
            srcDesc.Width  = witdh;
            srcDesc.Height = height;
        }

        if (!is_d3d11_opaque(picture->format.i_chroma) || sys->legacy_shader) {
            D3D11_TEXTURE2D_DESC texDesc;
            if (!is_d3d11_opaque(picture->format.i_chroma))
                Direct3D11UnmapPoolTexture(picture);
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
            if (srcDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
            {
                /* for performance reason we don't want to allocate this during
                 * display, do it preferrably when creating the texture */
                assert(p_sys->resourceView[0]!=NULL);
            }
            if ( sys->picQuad.i_height != srcDesc.Height ||
                 sys->picQuad.i_width != srcDesc.Width )
            {
                /* the decoder produced different sizes than the vout, we need to
                 * adjust the vertex */
                sys->quad_fmt.i_width = srcDesc.Width;
                sys->quad_fmt.i_height = srcDesc.Height;
                sys->picQuad.i_width = sys->quad_fmt.i_width;
                sys->picQuad.i_height = sys->quad_fmt.i_height;

                CallUpdateRects(vd);
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
        D3D11_UpdateQuadLuminanceScale(vd, &sys->d3d_dev, &sys->picQuad, (float)sys->display.luminance_peak / GetFormatLuminance(VLC_OBJECT(vd), &picture->format));

        if (sys->dxgiswapChain4)
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
            if (memcmp(&sys->hdr10, &hdr10, sizeof(hdr10)))
            {
                memcpy(&sys->hdr10, &hdr10, sizeof(hdr10));
                IDXGISwapChain4_SetHDRMetaData(sys->dxgiswapChain4, DXGI_HDR_METADATA_TYPE_HDR10, sizeof(hdr10), &hdr10);
            }
        }
    }

    FLOAT blackRGBA[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    ID3D11DeviceContext_ClearRenderTargetView(sys->d3d_dev.d3dcontext, sys->d3drenderTargetView, blackRGBA);

    /* no ID3D11Device operations should come here */

    ID3D11DeviceContext_ClearDepthStencilView(sys->d3d_dev.d3dcontext, sys->d3ddepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    /* Render the quad */
    if (!is_d3d11_opaque(picture->format.i_chroma) || sys->legacy_shader)
        D3D11_RenderQuad(&sys->d3d_dev, &sys->picQuad, sys->stagingSys.resourceView, sys->d3drenderTargetView);
    else if (sys->scaleProc && D3D11_UpscalerUsed(sys->scaleProc))
    {
        ID3D11ShaderResourceView *SRV[D3D11_MAX_SHADER_VIEW];
        D3D11_UpscalerGetSRV(sys->scaleProc, SRV);
        D3D11_RenderQuad(&sys->d3d_dev, &sys->picQuad, SRV, sys->d3drenderTargetView);
    }
    else
    {
        picture_sys_t *p_sys;
        if (sys->tonemapProc)
            p_sys = D3D11_TonemapperGetOutput(sys->tonemapProc);
        else
            p_sys = ActivePictureSys(picture);
        D3D11_RenderQuad(&sys->d3d_dev, &sys->picQuad, p_sys->resourceView, sys->d3drenderTargetView);
    }

    if (subpicture) {
        // draw the additional vertices
        for (int i = 0; i < sys->d3dregion_count; ++i) {
            if (sys->d3dregions[i])
            {
                d3d_quad_t *quad = (d3d_quad_t *) sys->d3dregions[i]->p_sys;
                D3D11_RenderQuad(&sys->d3d_dev, quad, quad->picSys.resourceView, sys->d3drenderTargetView);
            }
        }
    }

#ifdef HAVE_D3D11_4_H
    if (sys->d3dcontext4)
    {
        vlc_tick_t render_start;
        if (sys->log_level >= 4)
            render_start = mdate();
        if (sys->renderFence == UINT64_MAX)
            sys->renderFence = 0;
        else
            sys->renderFence++;

        ResetEvent(sys->renderFinished);
        ID3D11Fence_SetEventOnCompletion(sys->d3dRenderFence, sys->renderFence, sys->renderFinished);
        ID3D11DeviceContext4_Signal(sys->d3dcontext4, sys->d3dRenderFence, sys->renderFence);

        WaitForSingleObject(sys->renderFinished, INFINITE);
        if (sys->log_level >= 4)
            msg_Dbg(vd, "waited %" PRId64 " ms for the render fence",
                    (mdate() - render_start) * 1000 / CLOCK_FREQ);
    }
#endif

    if (is_d3d11_opaque(picture->format.i_chroma))
        d3d11_device_unlock( &sys->d3d_dev );
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    DXGI_PRESENT_PARAMETERS presentParams;
    memset(&presentParams, 0, sizeof(presentParams));
    d3d11_device_lock( &sys->d3d_dev );
    HRESULT hr = IDXGISwapChain1_Present1(sys->dxgiswapChain, 0, 0, &presentParams);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        /* TODO device lost */
        msg_Err(vd, "SwapChain Present failed. (hr=0x%lX)", hr);
    }

    d3d11_device_unlock( &sys->d3d_dev );

    picture_Release(picture);
    if (subpicture)
        subpicture_Delete(subpicture);

    CommonDisplay(vd);
}

static void Direct3D11Destroy(vout_display_t *vd)
{
#if !VLC_WINSTORE_APP
    vout_display_sys_t *sys = vd->sys;
    sys->hdxgi_dll = NULL;
    D3D11_Destroy( &vd->sys->hd3d );
#endif
}

#define COLOR_RANGE_FULL   1 /* 0-255 */
#define COLOR_RANGE_STUDIO 0 /* 16-235 */

#define TRANSFER_FUNC_10    TRANSFER_FUNC_LINEAR
#define TRANSFER_FUNC_22    TRANSFER_FUNC_SRGB
#define TRANSFER_FUNC_2084  TRANSFER_FUNC_SMPTE_ST2084

#define COLOR_PRIMARIES_BT601  COLOR_PRIMARIES_BT601_525

static const dxgi_color_space color_spaces[] = {
#define DXGIMAP(AXIS, RANGE, GAMMA, SITTING, PRIMARIES) \
    { DXGI_COLOR_SPACE_##AXIS##_##RANGE##_G##GAMMA##_##SITTING##_P##PRIMARIES, \
      #AXIS " Rec." #PRIMARIES " gamma:" #GAMMA " range:" #RANGE, \
      COLOR_AXIS_##AXIS, COLOR_PRIMARIES_BT##PRIMARIES, TRANSFER_FUNC_##GAMMA, \
      COLOR_SPACE_BT##PRIMARIES, COLOR_RANGE_##RANGE},

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
    IDXGIOutput *dxgiOutput = NULL;
    IDXGISwapChain3 *dxgiswapChain3 = NULL;
    sys->display.colorspace = &color_spaces[0];

    hr = IDXGISwapChain_QueryInterface( sys->dxgiswapChain, &IID_IDXGISwapChain3, (void **)&dxgiswapChain3);
    if (FAILED(hr)) {
        msg_Warn(vd, "could not get a IDXGISwapChain3");
        goto done;
    }

    video_format_t match_source = vd->source;
    if (sys->hdrMode == hdr_Never)
    {
        match_source.primaries = COLOR_PRIMARIES_BT709;
        match_source.transfer  = TRANSFER_FUNC_BT709;
        match_source.space     = COLOR_SPACE_BT709;
    }
    else if (sys->hdrMode == hdr_Always || sys->hdrMode == hdr_Fake)
    {
        match_source.primaries = COLOR_PRIMARIES_BT2020;
        match_source.transfer  = TRANSFER_FUNC_SMPTE_ST2084;
        match_source.space     = COLOR_SPACE_BT2020;
        if (sys->hdrMode == hdr_Fake) // the video processor keeps the source range
            match_source.i_chroma  = VLC_CODEC_RGBA10;
    }

    bool src_full_range = match_source.b_color_range_full ||
                          /* the YUV->RGB conversion already output full range */
                          is_d3d11_opaque(match_source.i_chroma) ||
                          vlc_fourcc_IsYUV(match_source.i_chroma);

    /* pick the best output based on color support and transfer */
    /* TODO support YUV output later */
    for (int i=0; color_spaces[i].name; ++i)
    {
        hr = IDXGISwapChain3_CheckColorSpaceSupport(dxgiswapChain3, color_spaces[i].dxgi, &support);
        if (SUCCEEDED(hr) && support) {
            msg_Dbg(vd, "supports colorspace %s", color_spaces[i].name);
            score = 0;
            if (color_spaces[i].primaries == match_source.primaries)
                score++;
            if (color_spaces[i].color == match_source.space)
                score += 2; /* we don't want to translate color spaces */
            if (color_spaces[i].transfer == match_source.transfer ||
                /* favor 2084 output for HLG source */
                (color_spaces[i].transfer == TRANSFER_FUNC_SMPTE_ST2084 && match_source.transfer == TRANSFER_FUNC_HLG))
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
    if (sys->hdrMode == hdr_Auto || sys->hdrMode == hdr_Fake) // match the screen
    if (SUCCEEDED(IDXGISwapChain_GetContainingOutput( sys->dxgiswapChain, &dxgiOutput )))
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

static bool CanUseTextureArray(vout_display_t *vd)
{
#ifndef HAVE_ID3D11VIDEODECODER
    (void) vd;
    return false;
#else
    // 15.200.1062.1004 is wrong - 2015/08/03 - 15.7.1 WHQL
    // 21.19.144.1281 is wrong   -
    // 22.19.165.3 is good       - 2017/05/04 - ReLive Edition 17.5.1
    struct wddm_version WDDM_os = {
        .wddm         = 21,  // starting with drivers designed for W10 Anniversary Update
    };
    if (D3D11CheckDriverVersion(&vd->sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM_os) != VLC_SUCCESS)
    {
        msg_Dbg(vd, "AMD driver too old, fallback to legacy shader mode");
        return false;
    }

    // xx.xx.1000.xxx drivers can't happen here for WDDM > 2.0
    struct wddm_version WDDM_build = {
        .revision     = 162,
    };
    if (D3D11CheckDriverVersion(&vd->sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM_build) != VLC_SUCCESS)
    {
        msg_Dbg(vd, "Bogus AMD driver detected, fallback to legacy shader mode");
        return false;
    }

    return true;
#endif
}

static bool BogusZeroCopy(const vout_display_t *vd)
{
    if (vd->sys->d3d_dev.adapterDesc.VendorId != GPU_MANUFACTURER_AMD)
        return false;

    switch (vd->sys->d3d_dev.adapterDesc.DeviceId)
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

static enum d3d11_hdr HdrModeFromString(vlc_object_t *logger, const char *psz_hdr)
{
    if (strcmp("auto", psz_hdr) == 0)
        return hdr_Auto;
    if (strcmp("never", psz_hdr) == 0)
        return hdr_Never;
    if (strcmp("always", psz_hdr) == 0)
        return hdr_Always;
    if (strcmp("generate", psz_hdr) == 0)
        return hdr_Fake;

    msg_Warn(logger, "unknown HDR mode %s, using auto mode", psz_hdr);
    return hdr_Auto;
}

static void InitTonemapProcessor(vout_display_t *vd, const video_format_t *fmt_in)
{
    vout_display_sys_t *sys = vd->sys;
    if (sys->hdrMode != hdr_Fake)
        return;

    sys->tonemapProc = D3D11_TonemapperCreate(VLC_OBJECT(vd), &sys->d3d_dev, fmt_in);
    if (sys->tonemapProc == NULL)
    {
        sys->hdrMode = hdr_Auto;
        msg_Dbg(vd, "failed to create the tone mapper, using default HDR mode");
        return;
    }

    msg_Dbg(vd, "Using tonemapper");
}

static void InitScaleProcessor(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    if (sys->upscaleMode != upscale_VideoProcessor && sys->upscaleMode != upscale_SuperResolution)
        return;

    sys->scaleProc = D3D11_UpscalerCreate(VLC_OBJECT(vd), &sys->d3d_dev, sys->quad_fmt.i_chroma,
                                          sys->upscaleMode == upscale_SuperResolution,
                                          &sys->picQuad.formatInfo);
    if (sys->scaleProc == NULL)
        sys->upscaleMode = upscale_LinearSampler;

    msg_Dbg(vd, "Using %s scaler with %s output", ppsz_upscale_mode_text[sys->upscaleMode],
        sys->picQuad.formatInfo->name);
}

static int Direct3D11Open(vout_display_t *vd, bool external_device)
{
    vout_display_sys_t *sys = vd->sys;
    IDXGIFactory2 *dxgifactory;

    char *psz_hdr = var_InheritString(vd, "d3d11-hdr-mode");
    sys->hdrMode = HdrModeFromString(VLC_OBJECT(vd), psz_hdr);
    free(psz_hdr);

    if (!external_device)
    {
#if !VLC_WINSTORE_APP
        HRESULT hr = S_OK;

        DXGI_SWAP_CHAIN_DESC1 scd;

        hr = D3D11_CreateDevice(vd, &sys->hd3d,
                                is_d3d11_opaque(vd->source.i_chroma),
                                &sys->d3d_dev);
        if (FAILED(hr)) {
        msg_Err(vd, "Could not Create the D3D11 device. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
        }

        IDXGIAdapter *dxgiadapter = D3D11DeviceAdapter(sys->d3d_dev.d3ddevice);
        if (unlikely(dxgiadapter==NULL)) {
        msg_Err(vd, "Could not get the DXGI Adapter");
        return VLC_EGENERIC;
        }

        InitTonemapProcessor(vd, &vd->source);

        hr = IDXGIAdapter_GetParent(dxgiadapter, &IID_IDXGIFactory2, (void **)&dxgifactory);
        IDXGIAdapter_Release(dxgiadapter);
        if (FAILED(hr)) {
        msg_Err(vd, "Could not get the DXGI Factory. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
        }

        FillSwapChainDesc(vd, &scd);

        hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgifactory, (IUnknown *)sys->d3d_dev.d3ddevice,
                                                sys->sys.hvideownd, &scd, NULL, NULL, &sys->dxgiswapChain);
        if (hr == DXGI_ERROR_INVALID_CALL && scd.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
        {
            msg_Warn(vd, "10 bits swapchain failed, try 8 bits");
            scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgifactory, (IUnknown *)sys->d3d_dev.d3ddevice,
                                                    sys->sys.hvideownd, &scd, NULL, NULL, &sys->dxgiswapChain);
        }
        IDXGIFactory2_Release(dxgifactory);
        if (FAILED(hr)) {
        msg_Err(vd, "Could not create the SwapChain. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
        }
#endif
    }

    IDXGISwapChain_QueryInterface( sys->dxgiswapChain, &IID_IDXGISwapChain4, (void **)&sys->dxgiswapChain4);

    D3D11SetColorSpace(vd);

    char *psz_upscale = var_InheritString(vd, "d3d11-upscale-mode");
    if (strcmp("linear", psz_upscale) == 0)
        sys->upscaleMode = upscale_LinearSampler;
    else if (strcmp("point", psz_upscale) == 0)
        sys->upscaleMode = upscale_PointSampler;
    else if (strcmp("processor", psz_upscale) == 0)
        sys->upscaleMode = upscale_VideoProcessor;
    else if (strcmp("super", psz_upscale) == 0)
        sys->upscaleMode = upscale_SuperResolution;
    else
    {
        msg_Warn(vd, "unknown upscale mode %s, using linear sampler", psz_upscale);
        sys->upscaleMode = upscale_LinearSampler;
    }
    free(psz_upscale);

    video_format_Copy(&sys->pool_fmt, &vd->source);
    video_format_Copy(&sys->quad_fmt, &vd->source);
    int err = SetupOutputFormat(vd, &sys->pool_fmt, &sys->quad_fmt);
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
                sys->pool_fmt.i_chroma = list[i];
                if (sys->pool_fmt.i_chroma == vd->source.i_chroma)
                    continue;
                err = SetupOutputFormat(vd, &sys->pool_fmt, &sys->quad_fmt);
                if (err == VLC_SUCCESS)
                    break;
            }
        }
        if (err != VLC_SUCCESS)
            return err;
    }

    if (sys->upscaleMode == upscale_VideoProcessor || sys->upscaleMode == upscale_SuperResolution)
        sys->sys.src_fmt = &sys->quad_fmt;

    if ( sys->picQuad.formatInfo->formatTexture != DXGI_FORMAT_R8G8B8A8_UNORM &&
         sys->picQuad.formatInfo->formatTexture != DXGI_FORMAT_B5G6R5_UNORM )
    {
        sys->pool_fmt.i_width  = (sys->pool_fmt.i_width  + 0x01) & ~0x01;
        sys->pool_fmt.i_height = (sys->pool_fmt.i_height + 0x01) & ~0x01;
    }

    if (Direct3D11CreateGenericResources(vd)) {
        msg_Err(vd, "Failed to allocate resources");
        return VLC_EGENERIC;
    }

    video_format_Clean(&vd->fmt);
    vd->fmt = sys->pool_fmt;

    sys->log_level = var_InheritInteger(vd, "verbose");

    return VLC_SUCCESS;
}

static const d3d_format_t *SelectClosestOutput(vout_display_t *vd, vlc_fourcc_t i_chroma,
                                               bool from_processor)
{
    const d3d_format_t *res = NULL;

    // look for any pixel format that we can handle with enough pixels per channel
    uint8_t bits_per_channel;
    uint8_t widthDenominator, heightDenominator;
    switch (i_chroma)
    {
    case VLC_CODEC_D3D11_OPAQUE:
        bits_per_channel = 8;
        widthDenominator = heightDenominator = 2;
        break;
    case VLC_CODEC_D3D11_OPAQUE_10B:
        bits_per_channel = 10;
        widthDenominator = heightDenominator = 2;
        break;
    default:
        {
            const vlc_chroma_description_t *p_format = vlc_fourcc_GetChromaDescription(i_chroma);
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

    bool is_rgb = !vlc_fourcc_IsYUV(i_chroma);
    res = GetDisplayFormatByDepth(vd, bits_per_channel,
                                  widthDenominator,
                                  heightDenominator,
                                  from_processor, is_rgb);
    if (res != NULL)
        return res;
    if (is_rgb)
        res = GetDisplayFormatByDepth(vd, bits_per_channel,
                                      widthDenominator,
                                      heightDenominator,
                                      from_processor, false);
    if (res != NULL)
        return res;

    // look for any pixel format that we can handle
    res = GetDisplayFormatByDepth(vd, 0, 0, 0, false, false);
    return res;
}

static const d3d_format_t *SelectOutputFormat(vout_display_t *vd, const video_format_t *fmt,
                                              const d3d_format_t ** decoder_format)
{
    const d3d_format_t *res = NULL;
    // look for the requested pixel format first
    res = GetDirectRenderingFormat(vd, fmt->i_chroma);
    if (res != NULL)
        return res;

    /* look for a decoder format that can be decoded but not used in shaders */
    if ( is_d3d11_opaque(fmt->i_chroma) )
        *decoder_format = GetDirectDecoderFormat(vd, fmt->i_chroma);

    res = SelectClosestOutput(vd, fmt->i_chroma, *decoder_format!=NULL);

    return res;
}

static int SetupOutputFormat(vout_display_t *vd, video_format_t *fmt, video_format_t *quad_fmt)
{
    vout_display_sys_t *sys = vd->sys;

    // look for the requested pixel format first
    const d3d_format_t *decoder_format = NULL;
    if (sys->hdrMode == hdr_Fake)
    {
        quad_fmt->i_chroma           = VLC_CODEC_RGBA10;
        quad_fmt->primaries          = sys->display.colorspace->primaries;
        quad_fmt->transfer           = sys->display.colorspace->transfer;
        quad_fmt->space              = sys->display.colorspace->color;
        quad_fmt->b_color_range_full = sys->display.colorspace->b_full_range;

        // request an input format that can be input of a VideoProcessor
        UINT supportFlags = D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT;
        if (is_d3d11_opaque(fmt->i_chroma))
            supportFlags |= D3D11_FORMAT_SUPPORT_DECODER_OUTPUT;
        decoder_format = FindD3D11Format( vd, &sys->d3d_dev, fmt->i_chroma, false, 0, 0, 0,
                                          is_d3d11_opaque(fmt->i_chroma), supportFlags );
    }
    sys->picQuad.formatInfo = SelectOutputFormat(vd, quad_fmt, &decoder_format);

    if ( !sys->picQuad.formatInfo )
    {
       msg_Err(vd, "Could not get a suitable texture pixel format");
       return VLC_EGENERIC;
    }
    sys->pool_d3dfmt = decoder_format ? decoder_format : sys->picQuad.formatInfo;

    msg_Dbg( vd, "Using pixel format %s for chroma %4.4s", sys->pool_d3dfmt->name,
                 (char *)&fmt->i_chroma );
    fmt->i_chroma = sys->pool_d3dfmt->fourcc;
    DxgiFormatMask( sys->picQuad.formatInfo->formatTexture, fmt );

    /* check the region pixel format */
    sys->d3dregion_format = GetBlendableFormat(vd, VLC_CODEC_RGBA);
    if (!sys->d3dregion_format)
        sys->d3dregion_format = GetBlendableFormat(vd, VLC_CODEC_BGRA);

    InitScaleProcessor(vd);

    sys->legacy_shader = sys->d3d_dev.feature_level < D3D_FEATURE_LEVEL_10_0 ||
            (sys->scaleProc == NULL && !CanUseTextureArray(vd)) ||
            BogusZeroCopy(vd);

    if (Direct3D11CreateFormatResources(vd, quad_fmt)) {
        msg_Err(vd, "Failed to allocate format resources");
        return VLC_EGENERIC;
    }
    vd->info.is_slow = !is_d3d11_opaque(fmt->i_chroma) && sys->picQuad.formatInfo->formatTexture != DXGI_FORMAT_UNKNOWN;

    return VLC_SUCCESS;
}

static void Direct3D11Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyResources(vd);
    if (sys->dxgiswapChain4)
    {
        IDXGISwapChain_Release(sys->dxgiswapChain4);
        sys->dxgiswapChain4 = NULL;
    }
    if (sys->dxgiswapChain)
    {
        IDXGISwapChain_Release(sys->dxgiswapChain);
        sys->dxgiswapChain = NULL;
    }

    D3D11_ReleaseDevice( &sys->d3d_dev );

    msg_Dbg(vd, "Direct3D11 device adapter closed");
}

static void UpdatePicQuadPosition(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    sys->picQuad.cropViewport.Width    = RECTWidth(sys->sys.rect_dest_clipped);
    sys->picQuad.cropViewport.Height   = RECTHeight(sys->sys.rect_dest_clipped);
    sys->picQuad.cropViewport.TopLeftX = sys->sys.rect_dest_clipped.left;
    sys->picQuad.cropViewport.TopLeftY = sys->sys.rect_dest_clipped.top;

    sys->picQuad.cropViewport.MinDepth = 0.0f;
    sys->picQuad.cropViewport.MaxDepth = 1.0f;

    SetQuadVSProjection(vd, &sys->picQuad, &vd->cfg->viewpoint);

#ifndef NDEBUG
    msg_Dbg(vd, "picQuad position (%.02f,%.02f) %.02fx%.02f", sys->picQuad.cropViewport.TopLeftX, sys->picQuad.cropViewport.TopLeftY, sys->picQuad.cropViewport.Width, sys->picQuad.cropViewport.Height );
#endif
}

/* TODO : handle errors better
   TODO : seperate out into smaller functions like createshaders */
static int Direct3D11CreateFormatResources(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    hr = D3D11_CompilePixelShader(vd, &sys->hd3d, sys->legacy_shader, &sys->d3d_dev,
                                  sys->picQuad.formatInfo, &sys->display, fmt->transfer, fmt->primaries,
                                  fmt->b_color_range_full,
                                  &sys->picQuad.d3dpixelShader);
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to create the pixel shader. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    sys->picQuad.i_width  = fmt->i_width;
    sys->picQuad.i_height = fmt->i_height;

    CallUpdateRects(vd);

#ifdef HAVE_ID3D11VIDEODECODER
    if (!is_d3d11_opaque(fmt->i_chroma) || sys->legacy_shader)
    {
        /* we need a staging texture */
        ID3D11Texture2D *textures[D3D11_MAX_SHADER_VIEW] = {0};
        video_format_t surface_fmt = *fmt;
        surface_fmt.i_width  = sys->picQuad.i_width;
        surface_fmt.i_height = sys->picQuad.i_height;

        if (AllocateTextures(vd, &sys->d3d_dev, sys->picQuad.formatInfo, &surface_fmt, 1, textures))
        {
            msg_Err(vd, "Failed to allocate the staging texture");
            return VLC_EGENERIC;
        }

        sys->picQuad.resourceCount = DxgiResourceCount(sys->picQuad.formatInfo);
        if (D3D11_AllocateShaderView(vd, sys->d3d_dev.d3ddevice, sys->picQuad.formatInfo,
                                     textures, 0, sys->stagingSys.resourceView))
        {
            msg_Err(vd, "Failed to allocate the staging shader view");
            return VLC_EGENERIC;
        }

        for (unsigned plane = 0; plane < D3D11_MAX_SHADER_VIEW; plane++)
            sys->stagingSys.texture[plane] = textures[plane];
    }
#endif

    return VLC_SUCCESS;
}

#ifdef HAVE_D3D11_4_H
static HRESULT InitRenderFence(vout_display_sys_t *sys)
{
    HRESULT hr;
    sys->renderFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (unlikely(sys->renderFinished == NULL))
        return S_FALSE;
    ID3D11Device5 *d3ddev5 = NULL;
    hr = ID3D11DeviceContext_QueryInterface(sys->d3d_dev.d3dcontext, &IID_ID3D11DeviceContext4, (void**)&sys->d3dcontext4);
    if (FAILED(hr))
        goto error;
    hr = ID3D11Device_QueryInterface(sys->d3d_dev.d3ddevice, &IID_ID3D11Device5, (void**)&d3ddev5);
    if (FAILED(hr))
        goto error;
    hr = ID3D11Device5_CreateFence(d3ddev5, sys->renderFence, D3D11_FENCE_FLAG_NONE, &IID_ID3D11Fence, (void**)&sys->d3dRenderFence);
    if (FAILED(hr))
        goto error;
    ID3D11Device5_Release(d3ddev5);
    return hr;
error:
    if (d3ddev5)
        ID3D11Device5_Release(d3ddev5);
    if (sys->d3dRenderFence)
    {
        ID3D11Fence_Release(sys->d3dRenderFence);
        sys->d3dRenderFence = NULL;
    }
    if (sys->d3dcontext4)
    {
        ID3D11DeviceContext4_Release(sys->d3dcontext4);
        sys->d3dcontext4 = NULL;
    }
    CloseHandle(sys->renderFinished);
    return hr;
}
#endif // HAVE_D3D11_4_H

static int Direct3D11CreateGenericResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

#ifdef HAVE_D3D11_4_H
    hr = InitRenderFence(sys);
    if (SUCCEEDED(hr))
    {
        msg_Dbg(vd, "using GPU render fence");
    }
#endif

    ID3D11BlendState *pSpuBlendState;
    D3D11_BLEND_DESC spuBlendDesc = { 0 };
    spuBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    spuBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    spuBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    spuBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

    spuBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    spuBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    spuBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    spuBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    spuBlendDesc.RenderTarget[1].BlendEnable = TRUE;
    spuBlendDesc.RenderTarget[1].SrcBlend = D3D11_BLEND_ONE;
    spuBlendDesc.RenderTarget[1].DestBlend = D3D11_BLEND_ZERO;
    spuBlendDesc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;

    spuBlendDesc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ONE;
    spuBlendDesc.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_ZERO;
    spuBlendDesc.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    spuBlendDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
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

    CallUpdateRects(vd);

    if (sys->d3dregion_format != NULL)
    {
        hr = D3D11_CompilePixelShader(vd, &sys->hd3d, sys->legacy_shader, &sys->d3d_dev,
                                      sys->d3dregion_format, &sys->display, TRANSFER_FUNC_SRGB, COLOR_PRIMARIES_SRGB, true, &sys->pSPUPixelShader);
        if (FAILED(hr))
        {
            if (sys->picQuad.d3dpixelShader)
            {
                ID3D11PixelShader_Release(sys->picQuad.d3dpixelShader);
                sys->picQuad.d3dpixelShader = NULL;
            }
            msg_Err(vd, "Failed to create the SPU pixel shader. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    ID3DBlob *pVSBlob = D3D11_CompileShader(vd, &sys->hd3d, &sys->d3d_dev, globVertexShaderFlat, false);
    if (!pVSBlob)
        return VLC_EGENERIC;

    hr = ID3D11Device_CreateVertexShader(sys->d3d_dev.d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), NULL, &sys->flatVSShader);

    if(FAILED(hr)) {
      ID3D10Blob_Release(pVSBlob);
      msg_Err(vd, "Failed to create the flat vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = ID3D11Device_CreateInputLayout(sys->d3d_dev.d3ddevice, layout, 2, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), &sys->pVertexLayout);

    ID3D10Blob_Release(pVSBlob);

    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the vertex input layout. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    pVSBlob = D3D11_CompileShader(vd, &sys->hd3d, &sys->d3d_dev, globVertexShaderProjection, false);
    if (!pVSBlob)
        return VLC_EGENERIC;

    hr = ID3D11Device_CreateVertexShader(sys->d3d_dev.d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), NULL, &sys->projectionVSShader);

    if(FAILED(hr)) {
      ID3D10Blob_Release(pVSBlob);
      msg_Err(vd, "Failed to create the projection vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    ID3D10Blob_Release(pVSBlob);

    UpdatePicQuadPosition(vd);

    UpdateSamplers(vd);

    msg_Dbg(vd, "Direct3D11 resources created");
    return VLC_SUCCESS;
}

static void Direct3D11DestroyPool(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->sys.pool)
        picture_pool_Release(sys->sys.pool);
    sys->sys.pool = NULL;
    video_format_Clean(&sys->pool_fmt);
}

static void Direct3D11DestroyResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyPool(vd);

    D3D11_ReleaseQuad(&sys->picQuad);
    Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
    sys->d3dregion_count = 0;

    ReleasePictureSys(&sys->stagingSys);

    if (sys->tonemapProc != NULL)
    {
        D3D11_TonemapperDestroy(sys->tonemapProc);
        sys->tonemapProc = NULL;
    }
    if (sys->scaleProc != NULL)
    {
        D3D11_UpscalerDestroy(sys->scaleProc);
        sys->scaleProc = NULL;
    }
    video_format_Clean(&sys->quad_fmt);

    if (sys->pVertexLayout)
    {
        ID3D11InputLayout_Release(sys->pVertexLayout);
        sys->pVertexLayout = NULL;
    }
    if (sys->flatVSShader)
    {
        ID3D11VertexShader_Release(sys->flatVSShader);
        sys->flatVSShader = NULL;
    }
    if (sys->projectionVSShader)
    {
        ID3D11VertexShader_Release(sys->projectionVSShader);
        sys->projectionVSShader = NULL;
    }
    if (sys->d3drenderTargetView)
    {
        ID3D11RenderTargetView_Release(sys->d3drenderTargetView);
        sys->d3drenderTargetView = NULL;
    }
    if (sys->d3ddepthStencilView)
    {
        ID3D11DepthStencilView_Release(sys->d3ddepthStencilView);
        sys->d3ddepthStencilView = NULL;
    }
    if (sys->pSPUPixelShader)
    {
        ID3D11PixelShader_Release(sys->pSPUPixelShader);
        sys->pSPUPixelShader = NULL;
    }
    if (sys->picQuad.d3dpixelShader)
    {
        ID3D11PixelShader_Release(sys->picQuad.d3dpixelShader);
        sys->picQuad.d3dpixelShader = NULL;
    }
#ifdef HAVE_D3D11_4_H
    if (sys->d3dcontext4)
    {
        ID3D11Fence_Release(sys->d3dRenderFence);
        sys->d3dRenderFence = NULL;
        ID3D11DeviceContext4_Release(sys->d3dcontext4);
        sys->d3dcontext4 = NULL;
        CloseHandle(sys->renderFinished);
        sys->renderFinished = NULL;
    }
#endif

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
    free( p_picture );
}

static int Direct3D11MapSubpicture(vout_display_t *vd, int *subpicture_region_count,
                                   picture_t ***region, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    D3D11_TEXTURE2D_DESC texDesc;
    HRESULT hr;
    int err;

    if (sys->d3dregion_format == NULL)
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
                if (texDesc.Format == sys->d3dregion_format->formatTexture &&
                    texDesc.Width  == r->p_picture->format.i_width &&
                    texDesc.Height == r->p_picture->format.i_height) {
                    (*region)[i] = cache;
                    memset(&sys->d3dregions[j], 0, sizeof(cache)); // do not reuse this cached value
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
            if (AllocateTextures(vd, &sys->d3d_dev, sys->d3dregion_format, &r->p_picture->format, 1, d3dquad->picSys.texture)) {
                msg_Err(vd, "Failed to allocate %dx%d texture for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                for (int j=0; j<D3D11_MAX_SHADER_VIEW; j++)
                    if (d3dquad->picSys.texture[j])
                        ID3D11Texture2D_Release(d3dquad->picSys.texture[j]);
                free(d3dquad);
                continue;
            }

            if (D3D11_AllocateShaderView(vd, sys->d3d_dev.d3ddevice, sys->d3dregion_format,
                                         d3dquad->picSys.texture, 0,
                                         d3dquad->picSys.resourceView)) {
                msg_Err(vd, "Failed to create %dx%d shader view for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                free(d3dquad);
                continue;
            }
            d3dquad->i_width    = r->fmt.i_width;
            d3dquad->i_height   = r->fmt.i_height;

            d3dquad->formatInfo = sys->d3dregion_format;
            err = D3D11_SetupQuad( vd, &sys->d3d_dev, &r->fmt, d3dquad, &sys->display, &output,
                             sys->flatVSShader, sys->pVertexLayout, PROJECTION_MODE_RECTANGULAR, ORIENT_NORMAL );
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to create %dx%d quad for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                free(d3dquad);
                continue;
            }
            d3dquad->d3dpixelShader = sys->pSPUPixelShader;
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
        } else {
            D3D11_UpdateQuadPosition(vd, &sys->d3d_dev, (d3d_quad_t *) quad_picture->p_sys, &output, ORIENT_NORMAL);
        }

        hr = ID3D11DeviceContext_Map(sys->d3d_dev.d3dcontext, ((d3d_quad_t *) quad_picture->p_sys)->picSys.resource[KNOWN_DXGI_INDEX], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if( SUCCEEDED(hr) ) {
            err = CommonUpdatePicture(quad_picture, NULL, mappedResource.pData, mappedResource.RowPitch);
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

        vout_display_cfg_t place_cfg = *vd->cfg;
        if (sys->sys.rect_display.right && sys->sys.rect_display.bottom)
        {
            place_cfg.display.width  = sys->sys.rect_display.right;
            place_cfg.display.height = sys->sys.rect_display.bottom;
        } else {
            place_cfg.display.width  = RECTWidth(sys->sys.rect_dest_clipped);
            place_cfg.display.height = RECTHeight(sys->sys.rect_dest_clipped);
        }

        vout_display_place_t place;
        vout_display_PlacePicture(&place, &vd->source, &place_cfg, false);

        quad->cropViewport.Width =  (FLOAT) r->fmt.i_visible_width  * place.width  / subpicture->i_original_picture_width;
        quad->cropViewport.Height = (FLOAT) r->fmt.i_visible_height * place.height / subpicture->i_original_picture_height;
        quad->cropViewport.MinDepth = 0.0f;
        quad->cropViewport.MaxDepth = 1.0f;
        quad->cropViewport.TopLeftX = place.x + (FLOAT) r->i_x * place.width  / subpicture->i_original_picture_width;
        quad->cropViewport.TopLeftY = place.y + (FLOAT) r->i_y * place.height / subpicture->i_original_picture_height;

        D3D11_UpdateQuadOpacity(vd, &sys->d3d_dev, quad, r->i_alpha / 255.0f );
    }
    return VLC_SUCCESS;
}

