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

# undef WINAPI_FAMILY
# define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP

#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_charset.h>
#include <vlc_codec.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d11.h>
#include <libavcodec/d3d11va.h>

#include "../../video_chroma/d3d11_fmt.h"

#define D3D_Device          ID3D11Device
#define D3D_DecoderType     ID3D11VideoDecoder
#define D3D_DecoderDevice   ID3D11VideoDevice
#define D3D_DecoderSurface  ID3D11VideoDecoderOutputView
#include "directx_va.h"

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

#if VLC_WINSTORE_APP
#define pf_CreateDevice                 D3D11CreateDevice
#endif

/*
 * In this mode libavcodec doesn't need the whole array on texture on startup
 * So we get the surfaces from the decoder pool when needed. We don't need to
 * extract the decoded surface into the decoder picture anymore.
 */
#define D3D11_DIRECT_DECODE  LIBAVCODEC_VERSION_CHECK( 57, 30, 3, 72, 101 )

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

DEFINE_GUID(DXVA_Intel_H264_NoFGT_ClearVideo,       0x604F8E68, 0x4951, 0x4c54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6);

DEFINE_GUID(DXVA2_NoEncrypt,                        0x1b81bed0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

struct vlc_va_sys_t
{
    directx_sys_t                dx_sys;
    vlc_fourcc_t                 i_chroma;
    UINT                         totalTextureSlices;

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HINSTANCE                    dxgidebug_dll;
#endif

    /* Video service */
    ID3D11VideoContext           *d3dvidctx;
    DXGI_FORMAT                  render;

    ID3D11DeviceContext          *d3dctx;
    HANDLE                       context_mutex;

    /* pool */
    bool                         b_extern_pool;
    picture_t                    *extern_pics[MAX_SURFACE_COUNT];

    /* Video decoder */
    D3D11_VIDEO_DECODER_CONFIG   cfg;

    /* avcodec internals */
    struct AVD3D11VAContext      hw;
};

/* */
static int D3dCreateDevice(vlc_va_t *);
static void D3dDestroyDevice(vlc_va_t *);
static char *DxDescribe(directx_sys_t *);

static int DxCreateVideoService(vlc_va_t *);
static void DxDestroyVideoService(vlc_va_t *);
static int DxGetInputList(vlc_va_t *, input_list_t *);
static int DxSetupOutput(vlc_va_t *, const GUID *, const video_format_t *);

static int DxCreateDecoderSurfaces(vlc_va_t *, int codec_id, const video_format_t *fmt);
static void DxDestroySurfaces(vlc_va_t *);
static void SetupAVCodecContext(vlc_va_t *);

static picture_t *DxAllocPicture(vlc_va_t *, const video_format_t *, unsigned index);

/* */
static void Setup(vlc_va_t *va, vlc_fourcc_t *chroma)
{
    *chroma = va->sys->i_chroma;
}

void SetupAVCodecContext(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &sys->dx_sys;

    sys->hw.video_context = sys->d3dvidctx;
    sys->hw.decoder = dx_sys->decoder;
    sys->hw.cfg = &sys->cfg;
    sys->hw.surface_count = dx_sys->surface_count;
    sys->hw.surface = dx_sys->hw_surface;
    sys->hw.context_mutex = sys->context_mutex;

    if (IsEqualGUID(&dx_sys->input, &DXVA_Intel_H264_NoFGT_ClearVideo))
        sys->hw.workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;
}

static int Extract(vlc_va_t *va, picture_t *output, uint8_t *data)
{
    vlc_va_surface_t *surface = output->p_sys->va_surface;
    int ret = VLC_SUCCESS;

    picture_sys_t *p_sys_out = surface ? surface->p_pic->p_sys : output->p_sys;

    assert(p_sys_out->texture[KNOWN_DXGI_INDEX] != NULL);

#if D3D11_DIRECT_DECODE
    VLC_UNUSED(va); VLC_UNUSED(data);
#else
    {
        vlc_va_sys_t *sys = va->sys;
        ID3D11VideoDecoderOutputView *src = (ID3D11VideoDecoderOutputView*)(uintptr_t)data;
        picture_sys_t *p_sys_in = surface->p_pic->p_sys;
        assert(p_sys_in->decoder == src);

        if( sys->context_mutex != INVALID_HANDLE_VALUE ) {
            WaitForSingleObjectEx( sys->context_mutex, INFINITE, FALSE );
        }

        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
        ID3D11VideoDecoderOutputView_GetDesc( src, &viewDesc );

        D3D11_TEXTURE2D_DESC dstDesc;
        ID3D11Texture2D_GetDesc( p_sys_out->texture[KNOWN_DXGI_INDEX], &dstDesc);

        /* copy decoder slice to surface */
        D3D11_BOX copyBox = {
            .right = dstDesc.Width, .bottom = dstDesc.Height, .back = 1,
        };
        ID3D11DeviceContext_CopySubresourceRegion(sys->d3dctx, p_sys_out->resource[KNOWN_DXGI_INDEX],
                                                  p_sys_out->slice_index, 0, 0, 0,
                                                  p_sys_in->resource[KNOWN_DXGI_INDEX],
                                                  viewDesc.Texture2D.ArraySlice,
                                                  &copyBox);
        if( sys->context_mutex  != INVALID_HANDLE_VALUE ) {
            ReleaseMutex( sys->context_mutex );
        }
    }
#endif
    if (!output->p_sys && surface->p_pic && surface->p_pic->p_sys)
        /* HW decoding in a software pipe, pass the texture to upstream filters */
        output->p_sys = surface->p_pic->p_sys;

    return ret;
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
#if D3D11_DIRECT_DECODE
    picture_sys_t *p_sys = pic->p_sys;
    if (p_sys == NULL)
    {
        assert(!va->sys->b_extern_pool);
        vlc_va_surface_t *va_surface = directx_va_Get(va, &va->sys->dx_sys);
        if (!va_surface)
            return VLC_EGENERIC;
        *data = va_surface->decoderSurface;
        pic->p_sys = va_surface->p_pic->p_sys;
        pic->p_sys->va_surface = va_surface;
        return VLC_SUCCESS;
    }

    if (p_sys->decoder == NULL)
    {
        HRESULT hr;

        directx_sys_t *dx_sys = &va->sys->dx_sys;

        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
        ZeroMemory(&viewDesc, sizeof(viewDesc));
        viewDesc.DecodeProfile = dx_sys->input;
        viewDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.ArraySlice = p_sys->slice_index;

        hr = ID3D11VideoDevice_CreateVideoDecoderOutputView( dx_sys->d3ddec,
                                                             p_sys->resource[KNOWN_DXGI_INDEX],
                                                             &viewDesc,
                                                             &p_sys->decoder );
        if (FAILED(hr)) {
            msg_Warn(va, "CreateVideoDecoderOutputView %d failed. (hr=0x%0lx)", p_sys->slice_index, hr);
            D3D11_TEXTURE2D_DESC texDesc;
            ID3D11Texture2D_GetDesc(p_sys->texture[KNOWN_DXGI_INDEX], &texDesc);
            assert(texDesc.BindFlags & D3D11_BIND_DECODER);
            p_sys->decoder = NULL;
        }
    }
    if (p_sys->decoder == NULL)
        return VLC_EGENERIC;
    *data = p_sys->decoder;
    return VLC_SUCCESS;
#endif
    vlc_va_surface_t *va_surface = directx_va_Get(va, &va->sys->dx_sys);
    if (!va_surface)
        return VLC_EGENERIC;
    *data = va_surface->decoderSurface;
    pic->p_sys = va_surface->p_pic->p_sys;
    pic->p_sys->va_surface = va_surface;
    return VLC_SUCCESS;
}

static void Close(vlc_va_t *va, AVCodecContext *ctx)
{
    vlc_va_sys_t *sys = va->sys;

    (void) ctx;

    directx_va_Close(va, &sys->dx_sys);

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    if (sys->dxgidebug_dll)
        FreeLibrary(sys->dxgidebug_dll);
#endif

    free((char *)va->description);
    free(sys);
}

static vlc_fourcc_t d3d11va_fourcc(enum PixelFormat swfmt)
{
    switch (swfmt)
    {
        case AV_PIX_FMT_YUV420P10LE:
            return VLC_CODEC_D3D11_OPAQUE_10B;
        default:
            return VLC_CODEC_D3D11_OPAQUE;
    }
}

static void ReleasePic(void *opaque, uint8_t *data)
{
    (void)data;
    picture_t *pic = opaque;
    directx_va_Release(pic->p_sys->va_surface);
    pic->p_sys->va_surface = NULL;
    picture_Release(pic);
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
        void *d3dvidctx = NULL;
        HRESULT hr = ID3D11DeviceContext_QueryInterface(p_sys->context, &IID_ID3D11VideoContext, &d3dvidctx);
        if (FAILED(hr)) {
           msg_Err(va, "Could not Query ID3D11VideoContext Interface from the picture. (hr=0x%lX)", hr);
        } else {
            ID3D11DeviceContext_GetDevice( p_sys->context, &dx_sys->d3ddev );
            HANDLE context_lock = INVALID_HANDLE_VALUE;
            UINT dataSize = sizeof(context_lock);
            hr = ID3D11Device_GetPrivateData(dx_sys->d3ddev, &GUID_CONTEXT_MUTEX, &dataSize, &context_lock);
            if (FAILED(hr))
                msg_Warn(va, "No mutex found to lock the decoder");
            sys->context_mutex = context_lock;

            sys->d3dctx = p_sys->context;
            sys->d3dvidctx = d3dvidctx;

            assert(p_sys->texture[KNOWN_DXGI_INDEX] != NULL);
            D3D11_TEXTURE2D_DESC dstDesc;
            ID3D11Texture2D_GetDesc( p_sys->texture[KNOWN_DXGI_INDEX], &dstDesc);
            sys->render = dstDesc.Format;
            va->sys->totalTextureSlices = dstDesc.ArraySize;
        }
    }

    sys->i_chroma = d3d11va_fourcc(ctx->sw_pix_fmt);

#if VLC_WINSTORE_APP
    err = directx_va_Open(va, &sys->dx_sys, ctx, fmt, false);
#else
    err = directx_va_Open(va, &sys->dx_sys, ctx, fmt, dx_sys->d3ddev == NULL || va->sys->d3dctx == NULL);
#endif
    if (err!=VLC_SUCCESS)
        goto error;

    err = directx_va_Setup(va, &sys->dx_sys, ctx);
    if (err != VLC_SUCCESS)
        goto error;

    ctx->hwaccel_context = &sys->hw;

    /* TODO print the hardware name/vendor for debugging purposes */
    va->description = DxDescribe(dx_sys);
    va->setup   = Setup;
    va->get     = Get;
#if D3D11_DIRECT_DECODE
    va->release = sys->b_extern_pool ? NULL : ReleasePic;
#else
    va->release = ReleasePic;
#endif
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
        return VLC_SUCCESS;
    }

#if !VLC_WINSTORE_APP
    /* */
    PFN_D3D11_CREATE_DEVICE pf_CreateDevice;
    pf_CreateDevice = (void *)GetProcAddress(dx_sys->hdecoder_dll, "D3D11CreateDevice");
    if (!pf_CreateDevice) {
        msg_Err(va, "Cannot locate reference to D3D11CreateDevice ABI in DLL");
        return VLC_EGENERIC;
    }
#endif

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
    dx_sys->d3ddev = d3ddev;
    va->sys->d3dctx = d3dctx;

    void *d3dvidctx = NULL;
    hr = ID3D11DeviceContext_QueryInterface(d3dctx, &IID_ID3D11VideoContext, &d3dvidctx);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoContext Interface. (hr=0x%lX)", hr);
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
        { 0x4D4F4351, "Qualcomm" },
        { 0, "" }
    };

    IDXGIAdapter *p_adapter = D3D11DeviceAdapter(dx_sys->d3ddev);
    if (!p_adapter) {
       return NULL;
    }

    char *description = NULL;
    DXGI_ADAPTER_DESC adapterDesc;
    if (SUCCEEDED(IDXGIAdapter_GetDesc(p_adapter, &adapterDesc))) {
        const char *vendor = "Unknown";
        for (int i = 0; vendors[i].id != 0; i++) {
            if (vendors[i].id == adapterDesc.VendorId) {
                vendor = vendors[i].name;
                break;
            }
        }

        char *utfdesc = FromWide(adapterDesc.Description);
        if (likely(utfdesc!=NULL))
        {
            if (asprintf(&description, "D3D11VA (%s, vendor %u(%s), device %u, revision %u)",
                         utfdesc,
                         adapterDesc.VendorId, vendor, adapterDesc.DeviceId, adapterDesc.Revision) < 0)
                description = NULL;
            free(utfdesc);
        }
    }

    IDXGIAdapter_Release(p_adapter);
    return description;
}

/**
 * It creates a DirectX video service
 */
static int DxCreateVideoService(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;

    void *d3dviddev = NULL;
    HRESULT hr = ID3D11Device_QueryInterface(dx_sys->d3ddev, &IID_ID3D11VideoDevice, &d3dviddev);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    dx_sys->d3ddec = d3dviddev;

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

    UINT input_count = ID3D11VideoDevice_GetVideoDecoderProfileCount(dx_sys->d3ddec);

    p_list->count = input_count;
    p_list->list = calloc(input_count, sizeof(*p_list->list));
    if (unlikely(p_list->list == NULL)) {
        return VLC_ENOMEM;
    }
    p_list->pf_release = ReleaseInputList;

    for (unsigned i = 0; i < input_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderProfile(dx_sys->d3ddec, i, &p_list->list[i]);
        if (FAILED(hr))
        {
            msg_Err(va, "GetVideoDecoderProfile %d failed. (hr=0x%lX)", i, hr);
            ReleaseInputList(p_list);
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static int DxSetupOutput(vlc_va_t *va, const GUID *input, const video_format_t *fmt)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    HRESULT hr;

#ifndef NDEBUG
    BOOL bSupported = false;
    for (int format = 0; format < 188; format++) {
        hr = ID3D11VideoDevice_CheckVideoDecoderFormat(dx_sys->d3ddec, input, format, &bSupported);
        if (SUCCEEDED(hr) && bSupported)
            msg_Dbg(va, "format %s is supported for output", DxgiFormatToStr(format));
    }
#endif

    DXGI_FORMAT processorInput[4];
    int idx = 0;
    if ( va->sys->render != DXGI_FORMAT_UNKNOWN )
        processorInput[idx++] = va->sys->render;
    processorInput[idx++] = DXGI_FORMAT_NV12;
    processorInput[idx++] = DXGI_FORMAT_420_OPAQUE;
    processorInput[idx++] = DXGI_FORMAT_UNKNOWN;

    char *psz_decoder_name = directx_va_GetDecoderName(input);

    /* */
    for (idx = 0; processorInput[idx] != DXGI_FORMAT_UNKNOWN; ++idx)
    {
        BOOL is_supported = false;
        hr = ID3D11VideoDevice_CheckVideoDecoderFormat(dx_sys->d3ddec, input, processorInput[idx], &is_supported);
        if (SUCCEEDED(hr) && is_supported)
            msg_Dbg(va, "%s output is supported for decoder %s.", DxgiFormatToStr(processorInput[idx]), psz_decoder_name);
        else
        {
            msg_Dbg(va, "Can't get a decoder output format %s for decoder %s.", DxgiFormatToStr(processorInput[idx]), psz_decoder_name);
            continue;
        }

       // check if we can create render texture of that format
       // check the decoder can output to that format
       if ( !DeviceSupportsFormat(dx_sys->d3ddev, processorInput[idx],
                                  D3D11_FORMAT_SUPPORT_SHADER_LOAD) )
       {
#ifndef ID3D11VideoContext_VideoProcessorBlt
           msg_Dbg(va, "Format %s needs a processor but is not supported",
                   DxgiFormatToStr(processorInput[idx]));
#else
           if ( !DeviceSupportsFormat(dx_sys->d3ddev, processorInput[idx],
                                      D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT) )
           {
               msg_Dbg(va, "Format %s needs a processor but is not available",
                       DxgiFormatToStr(processorInput[idx]));
               continue;
           }
#endif
        }

        D3D11_VIDEO_DECODER_DESC decoderDesc;
        ZeroMemory(&decoderDesc, sizeof(decoderDesc));
        decoderDesc.Guid = *input;
        decoderDesc.SampleWidth = fmt->i_width;
        decoderDesc.SampleHeight = fmt->i_height;
        decoderDesc.OutputFormat = processorInput[idx];

        UINT cfg_count = 0;
        hr = ID3D11VideoDevice_GetVideoDecoderConfigCount( dx_sys->d3ddec, &decoderDesc, &cfg_count );
        if (FAILED(hr))
        {
            msg_Err( va, "Failed to get configuration for decoder %s. (hr=0x%lX)", psz_decoder_name, hr );
            continue;
        }
        if (cfg_count == 0) {
            msg_Err( va, "No decoder configuration possible for %s %dx%d",
                     DxgiFormatToStr(decoderDesc.OutputFormat),
                     decoderDesc.SampleWidth, decoderDesc.SampleHeight );
            continue;
        }

        msg_Dbg(va, "Using output format %s for decoder %s", DxgiFormatToStr(processorInput[idx]), psz_decoder_name);
        if ( va->sys->render == processorInput[idx] )
        {
            /* NVIDIA cards crash when calling CreateVideoDecoderOutputView
             * on more than 30 slices */
            if (va->sys->totalTextureSlices <= 30 || !isNvidiaHardware(dx_sys->d3ddev))
                va->sys->b_extern_pool = true;
            else
                msg_Warn( va, "NVIDIA GPU with too many slices (%d) detected, use internal pool",
                          va->sys->totalTextureSlices );
        }
        va->sys->render = processorInput[idx];
        free(psz_decoder_name);
        return VLC_SUCCESS;
    }
    free(psz_decoder_name);

    msg_Dbg(va, "Output format from picture source not supported.");
    return VLC_EGENERIC;
}

/**
 * It creates a Direct3D11 decoder using the given video format
 */
static int DxCreateDecoderSurfaces(vlc_va_t *va, int codec_id, const video_format_t *fmt)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    HRESULT hr;

    ID3D10Multithread *pMultithread;
    hr = ID3D11Device_QueryInterface( dx_sys->d3ddev, &IID_ID3D10Multithread, (void **)&pMultithread);
    if (SUCCEEDED(hr)) {
        ID3D10Multithread_SetMultithreadProtected(pMultithread, TRUE);
        ID3D10Multithread_Release(pMultithread);
    }

#if VLC_WINSTORE_APP
    /* On the Xbox 1/S, any decoding of H264 with one dimension over 2304
     * crashes totally the device */
    if (codec_id == AV_CODEC_ID_H264 &&
        (dx_sys->surface_width > 2304 || dx_sys->surface_height > 2304) &&
        isXboxHardware(dx_sys->d3ddev))
    {
        msg_Warn(va, "%dx%d resolution not supported by your hardware", dx_sys->surface_width, dx_sys->surface_height);
        dx_sys->surface_count = 0;
        return VLC_EGENERIC;
    }
#endif

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ZeroMemory(&viewDesc, sizeof(viewDesc));
    viewDesc.DecodeProfile = dx_sys->input;
    viewDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;

    if (sys->b_extern_pool)
    {
#if D3D11_DIRECT_DECODE
        dx_sys->surface_count = 0;
#else
        size_t surface_idx;
        for (surface_idx = 0; surface_idx < dx_sys->surface_count; surface_idx++) {
            picture_t *pic = decoder_NewPicture( (decoder_t*) va->obj.parent );
            sys->extern_pics[surface_idx] = pic;
            dx_sys->hw_surface[surface_idx] = NULL;
            if (pic==NULL)
            {
                msg_Warn(va, "not enough decoder pictures %d out of %d", surface_idx, dx_sys->surface_count);
                sys->b_extern_pool = false;
                break;
            }

            D3D11_TEXTURE2D_DESC texDesc;
            ID3D11Texture2D_GetDesc(pic->p_sys->texture[KNOWN_DXGI_INDEX], &texDesc);
            if (texDesc.ArraySize < dx_sys->surface_count)
            {
                msg_Warn(va, "not enough decoding slices in the texture (%d/%d)",
                         texDesc.ArraySize, dx_sys->surface_count);
                sys->b_extern_pool = false;
                break;
            }
            assert(texDesc.Format == sys->render);
            assert(texDesc.BindFlags & D3D11_BIND_DECODER);

#if !LIBAVCODEC_VERSION_CHECK( 57, 27, 2, 61, 102 )
            if (pic->p_sys->slice_index != surface_idx)
            {
                msg_Warn(va, "d3d11va requires decoding slices to be the first in the texture (%d/%d)",
                         pic->p_sys->slice_index, surface_idx);
                sys->b_extern_pool = false;
                break;
            }
#endif

            viewDesc.Texture2D.ArraySlice = pic->p_sys->slice_index;
            hr = ID3D11VideoDevice_CreateVideoDecoderOutputView( dx_sys->d3ddec,
                                                                 pic->p_sys->resource[KNOWN_DXGI_INDEX],
                                                                 &viewDesc,
                                                                 &pic->p_sys->decoder );
            if (FAILED(hr)) {
                msg_Warn(va, "CreateVideoDecoderOutputView %d failed. (hr=0x%0lx)", surface_idx, hr);
                sys->b_extern_pool = false;
                break;
            }
            sys->dx_sys.hw_surface[surface_idx] = pic->p_sys->decoder;
        }

        if (!sys->b_extern_pool)
        {
            for (size_t i = 0; i < surface_idx; ++i)
            {
                if (dx_sys->hw_surface[i])
                {
                    ID3D11VideoDecoderOutputView_Release(dx_sys->hw_surface[i]);
                    dx_sys->hw_surface[i] = NULL;
                }
                if (sys->extern_pics[i])
                {
                    sys->extern_pics[i]->p_sys->decoder = NULL;
                    picture_Release(sys->extern_pics[i]);
                    sys->extern_pics[i] = NULL;
                }
            }
        }
        else
#endif
            msg_Dbg(va, "using external surface pool");
    }

    if (!sys->b_extern_pool)
    {
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
        hr = ID3D11Device_CreateTexture2D( dx_sys->d3ddev, &texDesc, NULL, &p_texture );
        if (FAILED(hr)) {
            msg_Err(va, "CreateTexture2D %d failed. (hr=0x%0lx)", dx_sys->surface_count, hr);
            dx_sys->surface_count = 0;
            return VLC_EGENERIC;
        }

        int surface_count = dx_sys->surface_count;
        for (dx_sys->surface_count = 0; dx_sys->surface_count < surface_count; dx_sys->surface_count++) {
            sys->extern_pics[dx_sys->surface_count] = NULL;
            viewDesc.Texture2D.ArraySlice = dx_sys->surface_count;

            hr = ID3D11VideoDevice_CreateVideoDecoderOutputView( dx_sys->d3ddec,
                                                                 (ID3D11Resource*) p_texture,
                                                                 &viewDesc,
                                                                 &dx_sys->hw_surface[dx_sys->surface_count] );
            if (FAILED(hr)) {
                msg_Err(va, "CreateVideoDecoderOutputView %d failed. (hr=0x%0lx)", dx_sys->surface_count, hr);
                ID3D11Texture2D_Release(p_texture);
                return VLC_EGENERIC;
            }
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
    hr = ID3D11VideoDevice_GetVideoDecoderConfigCount( dx_sys->d3ddec, &decoderDesc, &cfg_count );
    if (FAILED(hr)) {
        msg_Err(va, "GetVideoDecoderConfigCount failed. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    /* List all configurations available for the decoder */
    D3D11_VIDEO_DECODER_CONFIG cfg_list[cfg_count];
    for (unsigned i = 0; i < cfg_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderConfig( dx_sys->d3ddec, &decoderDesc, i, &cfg_list[i] );
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
        if (IsEqualGUID(&cfg->guidConfigBitstreamEncryption, &DXVA2_NoEncrypt))
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
    hr = ID3D11VideoDevice_CreateVideoDecoder( dx_sys->d3ddec, &decoderDesc, &sys->cfg, &decoder );
    if (FAILED(hr)) {
        msg_Err(va, "ID3D11VideoDevice_CreateVideoDecoder failed. (hr=0x%lX)", hr);
        dx_sys->decoder = NULL;
        return VLC_EGENERIC;
    }
    dx_sys->decoder = decoder;

    msg_Dbg(va, "DxCreateDecoderSurfaces succeed");
    return VLC_SUCCESS;
}

static void DxDestroySurfaces(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    if (dx_sys->surface_count && !va->sys->b_extern_pool) {
        ID3D11Resource *p_texture;
        ID3D11VideoDecoderOutputView_GetResource( dx_sys->hw_surface[0], &p_texture );
        ID3D11Resource_Release(p_texture);
        ID3D11Resource_Release(p_texture);
    }
}

static void DestroyPicture(picture_t *picture)
{
    picture_sys_t *p_sys = picture->p_sys;
    ReleasePictureSys(p_sys);
    free(p_sys);
    free(picture);
}

static picture_t *DxAllocPicture(vlc_va_t *va, const video_format_t *fmt, unsigned index)
{
    vlc_va_sys_t *sys = va->sys;
    if (sys->b_extern_pool)
        return sys->extern_pics[index];

    video_format_t src_fmt = *fmt;
    src_fmt.i_chroma = sys->i_chroma;
    picture_sys_t *pic_sys = calloc(1, sizeof(*pic_sys));
    if (unlikely(pic_sys == NULL))
        return NULL;

    pic_sys->decoder = sys->dx_sys.hw_surface[index];
    ID3D11VideoDecoderOutputView_GetResource(pic_sys->decoder, &pic_sys->resource[KNOWN_DXGI_INDEX]);
    pic_sys->context  = sys->d3dctx;

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

