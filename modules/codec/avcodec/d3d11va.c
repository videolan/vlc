/*****************************************************************************
 * d3d11va.c: Direct3D11 Video Acceleration decoder
 *****************************************************************************
 * Copyright © 2009 Geoffroy Couprie
 * Copyright © 2009 Laurent Aimar
 * Copyright © 2015 Steve Lhomme
 * Copyright © 2015 VideoLabs
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/**
  * See https://msdn.microsoft.com/en-us/library/windows/desktop/hh162912%28v=vs.85%29.aspx
  **/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_charset.h>
#include <vlc_filter.h>
#include <vlc_modules.h>

#include "directx_va.h"

#define COBJMACROS
#include <libavcodec/d3d11va.h>

#include "../../video_chroma/copy.h"

static int Open(vlc_va_t *, AVCodecContext *, enum PixelFormat,
                const es_format_t *, picture_sys_t *p_sys);
static void Close(vlc_va_t *, AVCodecContext *);

vlc_module_begin()
    set_description(N_("Direct3D11 Video Acceleration"))
    set_capability("hw decoder", 0)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callbacks(Open, Close)
vlc_module_end()

#include <initguid.h> /* must be last included to not redefine existing GUIDs */

/* dxva2api.h GUIDs: http://msdn.microsoft.com/en-us/library/windows/desktop/ms697067(v=vs100).aspx
 * assume that they are declared in dxva2api.h */
#define MS_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)

#ifdef __MINGW32__
# include <_mingw.h>

# if !defined(__MINGW64_VERSION_MAJOR)
#  undef MS_GUID
#  define MS_GUID DEFINE_GUID /* dxva2api.h fails to declare those, redefine as static */
#  define DXVA2_E_NEW_VIDEO_DEVICE MAKE_HRESULT(1, 4, 4097)
# else
#  include <dxva.h>
# endif

#endif /* __MINGW32__ */

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
# include <dxgidebug.h>
#endif

DEFINE_GUID(IID_ID3D11VideoDevice,   0x10EC4D5B, 0x975A, 0x4689, 0xB9, 0xE4, 0xD0, 0xAA, 0xC3, 0x0F, 0xE3, 0x33);
DEFINE_GUID(IID_ID3D11VideoContext,  0x61F21C45, 0x3C0E, 0x4a74, 0x9C, 0xEA, 0x67, 0x10, 0x0D, 0x9A, 0xD5, 0xE4);
DEFINE_GUID(IID_IDXGIDevice,         0x54ec77fa, 0x1377, 0x44e6, 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c);
DEFINE_GUID(IID_ID3D10Multithread,   0x9b7e4e00, 0x342c, 0x4106, 0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0);

DEFINE_GUID(DXVA_Intel_H264_NoFGT_ClearVideo,       0x604F8E68, 0x4951, 0x4c54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6);

struct vlc_va_sys_t
{
    directx_sys_t                dx_sys;

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HINSTANCE                    dxgidebug_dll;
#endif

    /* Video service */
    ID3D11VideoContext           *d3dvidctx;
    DXGI_FORMAT                  render;

    ID3D11DeviceContext          *d3dctx;

    /* Video decoder */
    D3D11_VIDEO_DECODER_CONFIG   cfg;

    /* Option conversion */
    filter_t                     *filter;

    /* avcodec internals */
    struct AVD3D11VAContext      hw;
};

/* VLC_CODEC_D3D11_OPAQUE */
struct picture_sys_t
{
    ID3D11VideoDecoderOutputView  *decoder; /* may be NULL for pictures from the pool */
    ID3D11Texture2D               *texture;
    ID3D11DeviceContext           *context;
};

/* */
static int D3dCreateDevice(vlc_va_t *);
static void D3dDestroyDevice(vlc_va_t *);
static char *DxDescribe(directx_sys_t *);

static int D3dCreateDeviceManager(vlc_va_t *);
static void D3dDestroyDeviceManager(vlc_va_t *);

static int DxCreateVideoService(vlc_va_t *);
static void DxDestroyVideoService(vlc_va_t *);
static int DxGetInputList(vlc_va_t *, input_list_t *);
static int DxSetupOutput(vlc_va_t *, const GUID *);

static int DxCreateDecoderSurfaces(vlc_va_t *, int codec_id, const video_format_t *fmt, bool b_threading);
static void DxDestroySurfaces(vlc_va_t *);
static void SetupAVCodecContext(vlc_va_t *);

static picture_t *DxAllocPicture(vlc_va_t *, const video_format_t *, unsigned index);

/* */
static int Setup(vlc_va_t *va, AVCodecContext *avctx, vlc_fourcc_t *chroma)
{
    vlc_va_sys_t *sys = va->sys;
    if (directx_va_Setup(va, &sys->dx_sys, avctx, chroma)!=VLC_SUCCESS)
        return VLC_EGENERIC;

    avctx->hwaccel_context = &sys->hw;
    *chroma = sys->filter == NULL ? VLC_CODEC_D3D11_OPAQUE : VLC_CODEC_YV12;

    return VLC_SUCCESS;
}

void SetupAVCodecContext(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &sys->dx_sys;

    sys->hw.video_context = sys->d3dvidctx;
    sys->hw.decoder = (ID3D11VideoDecoder*) dx_sys->decoder;
    sys->hw.cfg = &sys->cfg;
    sys->hw.surface_count = dx_sys->surface_count;
    sys->hw.surface = (ID3D11VideoDecoderOutputView**) dx_sys->hw_surface;

    if (IsEqualGUID(&dx_sys->input, &DXVA_Intel_H264_NoFGT_ClearVideo))
        sys->hw.workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;
}

static void DeleteFilter( filter_t * p_filter )
{
    if( p_filter->p_module )
        module_unneed( p_filter, p_filter->p_module );

    es_format_Clean( &p_filter->fmt_in );
    es_format_Clean( &p_filter->fmt_out );

    vlc_object_release( p_filter );
}

static picture_t *video_new_buffer(filter_t *p_filter)
{
    return p_filter->owner.sys;
}

static filter_t *CreateFilter( vlc_object_t *p_this, const es_format_t *p_fmt_in,
                               vlc_fourcc_t fmt_out )
{
    filter_t *p_filter;

    p_filter = vlc_object_create( p_this, sizeof(filter_t) );
    if (unlikely(p_filter == NULL))
        return NULL;

    p_filter->owner.video.buffer_new = (picture_t *(*)(filter_t *))video_new_buffer;

    es_format_InitFromVideo( &p_filter->fmt_in,  &p_fmt_in->video );
    es_format_InitFromVideo( &p_filter->fmt_out, &p_fmt_in->video );
    p_filter->fmt_in.i_codec  = p_filter->fmt_in.video.i_chroma  = VLC_CODEC_D3D11_OPAQUE;
    p_filter->fmt_out.i_codec = p_filter->fmt_out.video.i_chroma = fmt_out;
    p_filter->p_module = module_need( p_filter, "video filter2", NULL, false );

    if( !p_filter->p_module )
    {
        msg_Dbg( p_filter, "no video filter found" );
        DeleteFilter( p_filter );
        return NULL;
    }

    return p_filter;
}

static int Extract(vlc_va_t *va, picture_t *output, uint8_t *data)
{
    vlc_va_sys_t *sys = va->sys;
    ID3D11VideoDecoderOutputView *src = (ID3D11VideoDecoderOutputView*)(uintptr_t)data;
    vlc_va_surface_t *surface = output->context;

    if (output->format.i_chroma == VLC_CODEC_D3D11_OPAQUE)
    {
        /* copy decoder slice to surface */
        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
        picture_sys_t *p_sys_out = output->p_sys;
        picture_sys_t *p_sys_in = surface->p_pic->p_sys;

        assert(p_sys_out->texture != NULL);
        assert(p_sys_in->decoder == src);

        ID3D11VideoDecoderOutputView_GetDesc( src, &viewDesc );
        ID3D11DeviceContext_CopySubresourceRegion(sys->d3dctx, (ID3D11Resource*) p_sys_out->texture,
                                                  0, 0, 0, 0,
                                                  (ID3D11Resource*) p_sys_in->texture,
                                                  viewDesc.Texture2D.ArraySlice,
                                                  NULL);
    }
    else if (output->format.i_chroma == VLC_CODEC_YV12) {
        va->sys->filter->owner.sys = output;
        picture_Hold( surface->p_pic );
        va->sys->filter->pf_video_filter( va->sys->filter, surface->p_pic );
    } else {
        msg_Err(va, "Unsupported output picture format %08X", output->format.i_chroma );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int CheckDevice(vlc_va_t *va)
{
    VLC_UNUSED(va);
#ifdef TODO
    /* Check the device */
    /* see MFCreateDXGIDeviceManager in mfplat.dll, not avail in Win7 */
    HRESULT hr = IDirect3DDeviceManager9_TestDevice(sys->devmng, sys->device);
    if (hr == DXVA2_E_NEW_VIDEO_DEVICE) {
        if (DxResetVideoDecoder(va))
            return VLC_EGENERIC;
    } else if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_TestDevice %u", (unsigned)hr);
        return VLC_EGENERIC;
    }
#endif
    return VLC_SUCCESS;
}

static int Get(vlc_va_t *va, picture_t *pic, uint8_t **data)
{
    return directx_va_Get(va, &va->sys->dx_sys, pic, data);
}

static void Close(vlc_va_t *va, AVCodecContext *ctx)
{
    vlc_va_sys_t *sys = va->sys;

    (void) ctx;

    if (sys->filter)
    {
        DeleteFilter( sys->filter );
        sys->filter = NULL;
    }

    directx_va_Close(va, &sys->dx_sys);

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    if (sys->dxgidebug_dll)
        FreeLibrary(sys->dxgidebug_dll);
#endif

    free((char *)va->description);
    free(sys);
}

static int Open(vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat pix_fmt,
                const es_format_t *fmt, picture_sys_t *p_sys)
{
    int err = VLC_EGENERIC;
    directx_sys_t *dx_sys;

    if (pix_fmt != AV_PIX_FMT_D3D11VA_VLD)
        return VLC_EGENERIC;

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    sys->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
#endif

    dx_sys = &sys->dx_sys;

    dx_sys->pf_check_device            = CheckDevice;
    dx_sys->pf_create_device           = D3dCreateDevice;
    dx_sys->pf_destroy_device          = D3dDestroyDevice;
    dx_sys->pf_create_device_manager   = D3dCreateDeviceManager;
    dx_sys->pf_destroy_device_manager  = D3dDestroyDeviceManager;
    dx_sys->pf_create_video_service    = DxCreateVideoService;
    dx_sys->pf_destroy_video_service   = DxDestroyVideoService;
    dx_sys->pf_create_decoder_surfaces = DxCreateDecoderSurfaces;
    dx_sys->pf_destroy_surfaces        = DxDestroySurfaces;
    dx_sys->pf_setup_avcodec_ctx       = SetupAVCodecContext;
    dx_sys->pf_get_input_list          = DxGetInputList;
    dx_sys->pf_setup_output            = DxSetupOutput;
    dx_sys->pf_alloc_surface_pic       = DxAllocPicture;
    dx_sys->psz_decoder_dll            = TEXT("D3D11.DLL");

    va->sys = sys;

    dx_sys->d3ddev = NULL;
    va->sys->render = DXGI_FORMAT_UNKNOWN;
    if ( p_sys != NULL && p_sys->context != NULL ) {
        ID3D11VideoContext *d3dvidctx = NULL;
        HRESULT hr = ID3D11DeviceContext_QueryInterface(p_sys->context, &IID_ID3D11VideoContext, (void **)&d3dvidctx);
        if (FAILED(hr)) {
           msg_Err(va, "Could not Query ID3D11VideoDevice Interface from the picture. (hr=0x%lX)", hr);
        } else {
            ID3D11DeviceContext_GetDevice( p_sys->context, (ID3D11Device**) &dx_sys->d3ddev );
            sys->d3dctx = p_sys->context;
            sys->d3dvidctx = d3dvidctx;

            assert(p_sys->texture != NULL);
            D3D11_TEXTURE2D_DESC dstDesc;
            ID3D11Texture2D_GetDesc( (ID3D11Texture2D*) p_sys->texture, &dstDesc);
            sys->render = dstDesc.Format;
        }
    }

    err = directx_va_Open(va, &sys->dx_sys, ctx, fmt, dx_sys->d3ddev==NULL || va->sys->d3dctx==NULL);
    if (err!=VLC_SUCCESS)
        goto error;

    if (p_sys == NULL)
    {
        sys->filter = CreateFilter( VLC_OBJECT(va), fmt, VLC_CODEC_YV12);
        if (sys->filter == NULL)
            goto error;
    }

    /* TODO print the hardware name/vendor for debugging purposes */
    va->description = DxDescribe(dx_sys);
    va->setup   = Setup;
    va->get     = Get;
    va->release = directx_va_Release;
    va->extract = Extract;

    return VLC_SUCCESS;

error:
    Close(va, ctx);
    return err;
}

/**
 * It creates a Direct3D device usable for decoding
 */
static int D3dCreateDevice(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    HRESULT hr;

    if (dx_sys->d3ddev && va->sys->d3dctx) {
        msg_Dbg(va, "Reusing Direct3D11 device");
        ID3D11DeviceContext_AddRef(va->sys->d3dctx);
        ID3D11Device_AddRef(dx_sys->d3ddev);
        return VLC_SUCCESS;
    }

    /* */
    PFN_D3D11_CREATE_DEVICE pf_CreateDevice;
    pf_CreateDevice = (void *)GetProcAddress(dx_sys->hdecoder_dll, "D3D11CreateDevice");
    if (!pf_CreateDevice) {
        msg_Err(va, "Cannot locate reference to D3D11CreateDevice ABI in DLL");
        return VLC_EGENERIC;
    }

    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#if !defined(NDEBUG) //&& defined(_MSC_VER)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    /* */
    ID3D11Device *d3ddev;
    ID3D11DeviceContext *d3dctx;
    hr = pf_CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                 creationFlags, NULL, 0,
                                 D3D11_SDK_VERSION, &d3ddev, NULL, &d3dctx);
    if (FAILED(hr)) {
        msg_Err(va, "D3D11CreateDevice failed. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    dx_sys->d3ddev = (IUnknown*) d3ddev;
    va->sys->d3dctx = d3dctx;

    ID3D11VideoContext *d3dvidctx = NULL;
    hr = ID3D11DeviceContext_QueryInterface(d3dctx, &IID_ID3D11VideoContext, (void **)&d3dvidctx);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    va->sys->d3dvidctx = d3dvidctx;

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HRESULT (WINAPI  * pf_DXGIGetDebugInterface)(const GUID *riid, void **ppDebug);
    if (va->sys->dxgidebug_dll) {
        pf_DXGIGetDebugInterface = (void *)GetProcAddress(va->sys->dxgidebug_dll, "DXGIGetDebugInterface");
        if (pf_DXGIGetDebugInterface) {
            IDXGIDebug *pDXGIDebug = NULL;
            hr = pf_DXGIGetDebugInterface(&IID_IDXGIDebug, (void**)&pDXGIDebug);
            if (SUCCEEDED(hr) && pDXGIDebug) {
                hr = IDXGIDebug_ReportLiveObjects(pDXGIDebug, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
            }
        }
    }
#endif

    return VLC_SUCCESS;
}

/**
 * It releases a Direct3D device and its resources.
 */
static void D3dDestroyDevice(vlc_va_t *va)
{
    if (va->sys->d3dvidctx)
        ID3D11VideoContext_Release(va->sys->d3dvidctx);
    if (va->sys->d3dctx)
        ID3D11DeviceContext_Release(va->sys->d3dctx);
}
/**
 * It describes our Direct3D object
 */
static char *DxDescribe(directx_sys_t *dx_sys)
{
    static const struct {
        unsigned id;
        char     name[32];
    } vendors [] = {
        { 0x1002, "ATI" },
        { 0x10DE, "NVIDIA" },
        { 0x1106, "VIA" },
        { 0x8086, "Intel" },
        { 0x5333, "S3 Graphics" },
        { 0, "" }
    };

    IDXGIDevice *pDXGIDevice = NULL;
    HRESULT hr = ID3D11Device_QueryInterface( (ID3D11Device*) dx_sys->d3ddev, &IID_IDXGIDevice, (void **)&pDXGIDevice);
    if (FAILED(hr)) {
       return NULL;
    }

    IDXGIAdapter *p_adapter;
    hr = IDXGIDevice_GetAdapter(pDXGIDevice, &p_adapter);
    if (FAILED(hr)) {
        IDXGIDevice_Release(pDXGIDevice);
       return NULL;
    }

    DXGI_ADAPTER_DESC adapterDesc;
    if (SUCCEEDED(IDXGIAdapter_GetDesc(p_adapter, &adapterDesc))) {
        const char *vendor = "Unknown";
        for (int i = 0; vendors[i].id != 0; i++) {
            if (vendors[i].id == adapterDesc.VendorId) {
                vendor = vendors[i].name;
                break;
            }
        }

        char *description;
        if (asprintf(&description, "D3D11VA (%s, vendor %u(%s), device %u, revision %u)",
                     FromWide(adapterDesc.Description),
                     adapterDesc.VendorId, vendor, adapterDesc.DeviceId, adapterDesc.Revision) < 0)
            return NULL;
        IDXGIAdapter_Release(p_adapter);
        IDXGIDevice_Release(pDXGIDevice);
        return description;
    }

    IDXGIAdapter_Release(p_adapter);
    IDXGIDevice_Release(pDXGIDevice);
    return NULL;
}

/**
 * It creates a Direct3D device manager
 */
static int D3dCreateDeviceManager(vlc_va_t *va)
{
    VLC_UNUSED(va);
#if 0
    vlc_va_sys_t *sys = va->sys;

    HRESULT (WINAPI *CreateDeviceManager9)(UINT *pResetToken,
                                           IDirect3DDeviceManager9 **);
    CreateDeviceManager9 =
      (void *)GetProcAddress(sys->hdxva2_dll,
                             "DXVA2CreateDirect3DDeviceManager9");

    if (!CreateDeviceManager9) {
        msg_Err(va, "cannot load function");
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "OurDirect3DCreateDeviceManager9 Success!");

    UINT token;
    IDirect3DDeviceManager9 *devmng;
    if (FAILED(CreateDeviceManager9(&token, &devmng))) {
        msg_Err(va, " OurDirect3DCreateDeviceManager9 failed");
        return VLC_EGENERIC;
    }
    sys->token  = token;
    sys->devmng = devmng;
    msg_Info(va, "obtained IDirect3DDeviceManager9");

    HRESULT hr = IDirect3DDeviceManager9_ResetDevice(devmng, (ID3D11Device*) dx_sys->d3ddev, token);
    if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_ResetDevice failed: %08x", (unsigned)hr);
        return VLC_EGENERIC;
    }
#endif
    return VLC_SUCCESS;
}
/**
 * It destroys a Direct3D device manager
 */
static void D3dDestroyDeviceManager(vlc_va_t *va)
{
    VLC_UNUSED(va);
#if 0
    if (va->devmng)
        IDirect3DDeviceManager9_Release(va->devmng);
#endif
}

/**
 * It creates a DirectX video service
 */
static int DxCreateVideoService(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;

    ID3D11VideoDevice *d3dviddev = NULL;
    HRESULT hr = ID3D11Device_QueryInterface( (ID3D11Device*) dx_sys->d3ddev, &IID_ID3D11VideoDevice, (void **)&d3dviddev);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    dx_sys->d3ddec = (IUnknown*) d3dviddev;

    return VLC_SUCCESS;
}

/**
 * It destroys a DirectX video service
 */
static void DxDestroyVideoService(vlc_va_t *va)
{
    VLC_UNUSED(va);
}

static void ReleaseInputList(input_list_t *p_list)
{
    free(p_list->list);
}

static int DxGetInputList(vlc_va_t *va, input_list_t *p_list)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    HRESULT hr;

    UINT input_count = ID3D11VideoDevice_GetVideoDecoderProfileCount((ID3D11VideoDevice*) dx_sys->d3ddec);

    p_list->count = input_count;
    p_list->list = calloc(input_count, sizeof(*p_list->list));
    if (unlikely(p_list->list == NULL)) {
        return VLC_ENOMEM;
    }
    p_list->pf_release = ReleaseInputList;

    for (unsigned i = 0; i < input_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderProfile((ID3D11VideoDevice*) dx_sys->d3ddec, i, &p_list->list[i]);
        if (FAILED(hr))
        {
            msg_Err(va, "GetVideoDecoderProfile %d failed. (hr=0x%lX)", i, hr);
            ReleaseInputList(p_list);
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static int DxSetupOutput(vlc_va_t *va, const GUID *input)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    HRESULT hr;

    /* */
    BOOL is_supported = false;
    hr = ID3D11VideoDevice_CheckVideoDecoderFormat((ID3D11VideoDevice*) dx_sys->d3ddec, input, DXGI_FORMAT_NV12, &is_supported);
    if (SUCCEEDED(hr) && is_supported)
        msg_Dbg(va, "NV12 is supported for output");

    if ( va->sys->render != DXGI_FORMAT_UNKNOWN )
    {
        is_supported = false;
        hr = ID3D11VideoDevice_CheckVideoDecoderFormat((ID3D11VideoDevice*) dx_sys->d3ddec, input, va->sys->render, &is_supported);
        if (SUCCEEDED(hr) && is_supported)
        {
            /* We have our solution */
            msg_Dbg(va, "Using decoder output from picture source.");
            return VLC_SUCCESS;
        }
        msg_Dbg(va, "Output format from picture source not supported.");
        return VLC_EGENERIC;
    }
    else
    {
        /* */
        is_supported = false;
        hr = ID3D11VideoDevice_CheckVideoDecoderFormat((ID3D11VideoDevice*) dx_sys->d3ddec, input, DXGI_FORMAT_NV12, &is_supported);
        if (SUCCEEDED(hr) && is_supported)
        {
            /* We have our solution */
            msg_Dbg(va, "Using decoder output NV12");
            va->sys->render = DXGI_FORMAT_NV12;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

/**
 * It creates a Direct3D11 decoder using the given video format
 */
static int DxCreateDecoderSurfaces(vlc_va_t *va, int codec_id, const video_format_t *fmt, bool b_threading)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    HRESULT hr;

    ID3D10Multithread *pMultithread;
    hr = ID3D11Device_QueryInterface( (ID3D11Device*) dx_sys->d3ddev, &IID_ID3D10Multithread, (void **)&pMultithread);
    if (SUCCEEDED(hr)) {
        ID3D10Multithread_SetMultithreadProtected(pMultithread, b_threading && dx_sys->thread_count > 1);
        ID3D10Multithread_Release(pMultithread);
    }

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = dx_sys->surface_width;
    texDesc.Height = dx_sys->surface_height;
    texDesc.MipLevels = 1;
    texDesc.Format = sys->render;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0;
    texDesc.ArraySize = dx_sys->surface_count;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DECODER;
    texDesc.CPUAccessFlags = 0;

    ID3D11Texture2D *p_texture;
    hr = ID3D11Device_CreateTexture2D( (ID3D11Device*) dx_sys->d3ddev, &texDesc, NULL, &p_texture );
    if (FAILED(hr)) {
        msg_Err(va, "CreateTexture2D %d failed. (hr=0x%0lx)", dx_sys->surface_count, hr);
        return VLC_EGENERIC;
    }

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ZeroMemory(&viewDesc, sizeof(viewDesc));
    viewDesc.DecodeProfile = dx_sys->input;
    viewDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;

    int surface_count = dx_sys->surface_count;
    for (dx_sys->surface_count = 0; dx_sys->surface_count < surface_count; dx_sys->surface_count++) {
        viewDesc.Texture2D.ArraySlice = dx_sys->surface_count;

        hr = ID3D11VideoDevice_CreateVideoDecoderOutputView( (ID3D11VideoDevice*) dx_sys->d3ddec,
                                                             (ID3D11Resource*) p_texture,
                                                             &viewDesc,
                                                             (ID3D11VideoDecoderOutputView**) &dx_sys->hw_surface[dx_sys->surface_count] );
        if (FAILED(hr)) {
            msg_Err(va, "CreateVideoDecoderOutputView %d failed. (hr=0x%0lx)", dx_sys->surface_count, hr);
            ID3D11Texture2D_Release(p_texture);
            return VLC_EGENERIC;
        }
    }
    msg_Dbg(va, "ID3D11VideoDecoderOutputView succeed with %d surfaces (%dx%d)",
            dx_sys->surface_count, dx_sys->surface_width, dx_sys->surface_height);

    D3D11_VIDEO_DECODER_DESC decoderDesc;
    ZeroMemory(&decoderDesc, sizeof(decoderDesc));
    decoderDesc.Guid = dx_sys->input;
    decoderDesc.SampleWidth = fmt->i_width;
    decoderDesc.SampleHeight = fmt->i_height;
    decoderDesc.OutputFormat = sys->render;

    UINT cfg_count;
    hr = ID3D11VideoDevice_GetVideoDecoderConfigCount( (ID3D11VideoDevice*) dx_sys->d3ddec, &decoderDesc, &cfg_count );
    if (FAILED(hr)) {
        msg_Err(va, "GetVideoDecoderConfigCount failed. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    /* List all configurations available for the decoder */
    D3D11_VIDEO_DECODER_CONFIG cfg_list[cfg_count];
    for (unsigned i = 0; i < cfg_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderConfig( (ID3D11VideoDevice*) dx_sys->d3ddec, &decoderDesc, i, &cfg_list[i] );
        if (FAILED(hr)) {
            msg_Err(va, "GetVideoDecoderConfig failed. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    msg_Dbg(va, "we got %d decoder configurations", cfg_count);

    /* Select the best decoder configuration */
    int cfg_score = 0;
    for (unsigned i = 0; i < cfg_count; i++) {
        const D3D11_VIDEO_DECODER_CONFIG *cfg = &cfg_list[i];

        /* */
        msg_Dbg(va, "configuration[%d] ConfigBitstreamRaw %d",
                i, cfg->ConfigBitstreamRaw);

        /* */
        int score;
        if (cfg->ConfigBitstreamRaw == 1)
            score = 1;
        else if (codec_id == AV_CODEC_ID_H264 && cfg->ConfigBitstreamRaw == 2)
            score = 2;
        else
            continue;
        if (IsEqualGUID(&cfg->guidConfigBitstreamEncryption, &DXVA_NoEncrypt))
            score += 16;

        if (cfg_score < score) {
            sys->cfg = *cfg;
            cfg_score = score;
        }
    }
    if (cfg_score <= 0) {
        msg_Err(va, "Failed to find a supported decoder configuration");
        return VLC_EGENERIC;
    }

    /* Create the decoder */
    ID3D11VideoDecoder *decoder;
    hr = ID3D11VideoDevice_CreateVideoDecoder( (ID3D11VideoDevice*) dx_sys->d3ddec, &decoderDesc, &sys->cfg, &decoder );
    if (FAILED(hr)) {
        msg_Err(va, "ID3D11VideoDevice_CreateVideoDecoder failed. (hr=0x%lX)", hr);
        dx_sys->decoder = NULL;
        return VLC_EGENERIC;
    }
    dx_sys->decoder = (IUnknown*) decoder;

    msg_Dbg(va, "DxCreateDecoderSurfaces succeed");
    return VLC_SUCCESS;
}

static void DxDestroySurfaces(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    if (dx_sys->surface_count) {
        ID3D11Resource *p_texture;
        ID3D11VideoDecoderOutputView_GetResource( (ID3D11VideoDecoderOutputView*) dx_sys->hw_surface[0], &p_texture );
        ID3D11Resource_Release(p_texture);
        ID3D11Resource_Release(p_texture);
    }
}

static void DestroyPicture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    ID3D11Texture2D_Release( p_sys->texture );

    free(p_sys);
    free(picture);
}

static picture_t *DxAllocPicture(vlc_va_t *va, const video_format_t *fmt, unsigned index)
{
    video_format_t src_fmt = *fmt;
    src_fmt.i_chroma = VLC_CODEC_D3D11_OPAQUE;
    picture_sys_t *pic_sys = calloc(1, sizeof(*pic_sys));
    if (unlikely(pic_sys == NULL))
        return NULL;

    pic_sys->decoder  = (ID3D11VideoDecoderOutputView*) va->sys->dx_sys.hw_surface[index];
    ID3D11VideoDecoderOutputView_GetResource(pic_sys->decoder, (ID3D11Resource**) &pic_sys->texture);
    pic_sys->context  = va->sys->d3dctx;

    picture_resource_t res = {
        .p_sys      = pic_sys,
        .pf_destroy = DestroyPicture,
    };
    picture_t *pic = picture_NewFromResource(&src_fmt, &res);
    if (unlikely(pic == NULL))
    {
        free(pic_sys);
        return NULL;
    }
    return pic;
}

