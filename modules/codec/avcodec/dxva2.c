/*****************************************************************************
 * dxva2.c: Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_filter.h> /* need for the DXA9 to YV12 conversion */
#include <vlc_modules.h>
#include <vlc_plugin.h>

#include "directx_va.h"

#define DXVA2API_USE_BITFIELDS
#define COBJMACROS
#include <libavcodec/dxva2.h>

static int Open(vlc_va_t *, AVCodecContext *, enum PixelFormat,
                const es_format_t *, picture_sys_t *p_sys);
static void Close(vlc_va_t *, AVCodecContext *);

vlc_module_begin()
    set_description(N_("DirectX Video Acceleration (DXVA) 2.0"))
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

MS_GUID(IID_IDirectXVideoDecoderService, 0xfc51a551, 0xd5e7, 0x11d9, 0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);
MS_GUID(IID_IDirectXVideoAccelerationService, 0xfc51a550, 0xd5e7, 0x11d9, 0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);

DEFINE_GUID(DXVA2_NoEncrypt,                        0x1b81bed0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

DEFINE_GUID(DXVA_Intel_H264_NoFGT_ClearVideo,       0x604F8E68, 0x4951, 0x4c54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6);


/* */
typedef struct {
    const char   *name;
    D3DFORMAT    format;
    vlc_fourcc_t codec;
} d3d_format_t;
/* XXX Prefered format must come first */
static const d3d_format_t d3d_formats[] = {
    { "YV12",   MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_YV12 },
    { "NV12",   MAKEFOURCC('N','V','1','2'),    VLC_CODEC_NV12 },
    { "IMC3",   MAKEFOURCC('I','M','C','3'),    VLC_CODEC_YV12 },

    { NULL, 0, 0 }
};

static const d3d_format_t *D3dFindFormat(D3DFORMAT format)
{
    for (unsigned i = 0; d3d_formats[i].name; i++) {
        if (d3d_formats[i].format == format)
            return &d3d_formats[i];
    }
    return NULL;
}

struct vlc_va_sys_t
{
    directx_sys_t         dx_sys;

    /* DLL */
    HINSTANCE             hd3d9_dll;

    /* Direct3D */
    LPDIRECT3D9            d3dobj;
    D3DADAPTER_IDENTIFIER9 d3dai;

    /* Device manager */
    IDirect3DDeviceManager9  *devmng;
    HANDLE                   device;

    /* Video service */
    D3DFORMAT                    render;

    /* Video decoder */
    DXVA2_ConfigPictureDecode    cfg;

    /* Option conversion */
    filter_t                 *filter;

    /* avcodec internals */
    struct dxva_context hw;
};

struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
};

static picture_t *DxAllocPicture(vlc_va_t *, const video_format_t *, unsigned index);


/* */
static int D3dCreateDevice(vlc_va_t *);
static void D3dDestroyDevice(vlc_va_t *);
static char *DxDescribe(vlc_va_sys_t *);

static int D3dCreateDeviceManager(vlc_va_t *);
static void D3dDestroyDeviceManager(vlc_va_t *);

static int DxCreateVideoService(vlc_va_t *);
static void DxDestroyVideoService(vlc_va_t *);
static int DxGetInputList(vlc_va_t *, input_list_t *);
static int DxSetupOutput(vlc_va_t *, const GUID *, const video_format_t *);

static int DxCreateVideoDecoder(vlc_va_t *,
                                int codec_id, const video_format_t *, bool);
static void DxDestroyVideoDecoder(vlc_va_t *);
static int DxResetVideoDecoder(vlc_va_t *);
static void SetupAVCodecContext(vlc_va_t *);

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
    p_filter->fmt_in.i_codec  = p_filter->fmt_in.video.i_chroma  = VLC_CODEC_D3D9_OPAQUE;
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

/* */
static void Setup(vlc_va_t *va, vlc_fourcc_t *chroma)
{
    vlc_va_sys_t *sys = va->sys;

    *chroma = sys->filter == NULL ? VLC_CODEC_D3D9_OPAQUE : VLC_CODEC_YV12;
}

void SetupAVCodecContext(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &sys->dx_sys;

    sys->hw.decoder = (IDirectXVideoDecoder*) dx_sys->decoder;
    sys->hw.cfg = &sys->cfg;
    sys->hw.surface_count = dx_sys->surface_count;
    sys->hw.surface = (LPDIRECT3DSURFACE9*) dx_sys->hw_surface;

    if (IsEqualGUID(&dx_sys->input, &DXVA_Intel_H264_NoFGT_ClearVideo))
        sys->hw.workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;
}

static int Extract(vlc_va_t *va, picture_t *picture, uint8_t *data)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    LPDIRECT3DSURFACE9 d3d = (LPDIRECT3DSURFACE9)(uintptr_t)data;
    if (picture->format.i_chroma == VLC_CODEC_D3D9_OPAQUE)
    {
        picture_sys_t *p_sys = picture->p_sys;
        LPDIRECT3DSURFACE9 output = p_sys->surface;

        assert(d3d != output);
#ifndef NDEBUG
        LPDIRECT3DDEVICE9 srcDevice, dstDevice;
        IDirect3DSurface9_GetDevice(d3d, &srcDevice);
        IDirect3DSurface9_GetDevice(output, &dstDevice);
        assert(srcDevice == dstDevice);
#endif

        HRESULT hr;
        RECT visibleSource;
        visibleSource.left = 0;
        visibleSource.top = 0;
        visibleSource.right = picture->format.i_visible_width;
        visibleSource.bottom = picture->format.i_visible_height;
        hr = IDirect3DDevice9_StretchRect( (IDirect3DDevice9*) dx_sys->d3ddev, d3d, &visibleSource, output, &visibleSource, D3DTEXF_NONE);
        if (FAILED(hr)) {
            msg_Err(va, "Failed to copy the hw surface to the decoder surface (hr=0x%0lx)", hr );
            return VLC_EGENERIC;
        }
    }
    else if (va->sys->filter != NULL) {
        va->sys->filter->owner.sys = picture;
        vlc_va_surface_t *surface = picture->context;
        picture_Hold( surface->p_pic );
        va->sys->filter->pf_video_filter( va->sys->filter, surface->p_pic );
    } else {
        msg_Err(va, "Unsupported output picture format %08X", picture->format.i_chroma );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int CheckDevice(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    /* Check the device */
    HRESULT hr = IDirect3DDeviceManager9_TestDevice(sys->devmng, sys->device);
    if (hr == DXVA2_E_NEW_VIDEO_DEVICE) {
        if (DxResetVideoDecoder(va))
            return VLC_EGENERIC;
    } else if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_TestDevice %u", (unsigned)hr);
        return VLC_EGENERIC;
    }
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

    if (sys->hd3d9_dll)
        FreeLibrary(sys->hd3d9_dll);

    free((char *)va->description);
    free(sys);
}

static int Open(vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat pix_fmt,
                const es_format_t *fmt, picture_sys_t *p_sys)
{
    int err = VLC_EGENERIC;
    directx_sys_t *dx_sys;

    if (pix_fmt != AV_PIX_FMT_DXVA2_VLD)
        return VLC_EGENERIC;

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* Load dll*/
    sys->hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!sys->hd3d9_dll) {
        msg_Warn(va, "cannot load d3d9.dll");
        goto error;
    }

    dx_sys = &sys->dx_sys;

    dx_sys->pf_check_device            = CheckDevice;
    dx_sys->pf_create_device           = D3dCreateDevice;
    dx_sys->pf_destroy_device          = D3dDestroyDevice;
    dx_sys->pf_create_device_manager   = D3dCreateDeviceManager;
    dx_sys->pf_destroy_device_manager  = D3dDestroyDeviceManager;
    dx_sys->pf_create_video_service    = DxCreateVideoService;
    dx_sys->pf_destroy_video_service   = DxDestroyVideoService;
    dx_sys->pf_create_decoder_surfaces = DxCreateVideoDecoder;
    dx_sys->pf_destroy_surfaces        = DxDestroyVideoDecoder;
    dx_sys->pf_setup_avcodec_ctx       = SetupAVCodecContext;
    dx_sys->pf_get_input_list          = DxGetInputList;
    dx_sys->pf_setup_output            = DxSetupOutput;
    dx_sys->pf_alloc_surface_pic       = DxAllocPicture;
    dx_sys->psz_decoder_dll            = TEXT("DXVA2.DLL");

    va->sys = sys;

    dx_sys->d3ddev = NULL;
    if (p_sys!=NULL)
        IDirect3DSurface9_GetDevice(p_sys->surface, (IDirect3DDevice9**) &dx_sys->d3ddev );

    err = directx_va_Open(va, &sys->dx_sys, ctx, fmt, true);
    if (err!=VLC_SUCCESS)
        goto error;

    if (p_sys == NULL)
    {
        sys->filter = CreateFilter( VLC_OBJECT(va), fmt, VLC_CODEC_YV12);
        if (sys->filter == NULL)
            goto error;
    }

    err = directx_va_Setup(va, &sys->dx_sys, ctx);
    if (err != VLC_SUCCESS)
        goto error;

    ctx->hwaccel_context = &sys->hw;

    /* TODO print the hardware name/vendor for debugging purposes */
    va->description = DxDescribe(sys);
    va->setup   = Setup;
    va->get     = Get;
    va->release = directx_va_Release;
    va->extract = Extract;
    return VLC_SUCCESS;

error:
    Close(va, ctx);
    return VLC_EGENERIC;
}
/* */

/**
 * It creates a Direct3D device usable for DXVA 2
 */
static int D3dCreateDevice(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    if (sys->dx_sys.d3ddev) {
        msg_Dbg(va, "Reusing Direct3D9 device");
        IDirect3DDevice9_AddRef(sys->dx_sys.d3ddev);
        return VLC_SUCCESS;
    }

    /* */
    LPDIRECT3D9 (WINAPI *Create9)(UINT SDKVersion);
    Create9 = (void *)GetProcAddress(sys->hd3d9_dll, "Direct3DCreate9");
    if (!Create9) {
        msg_Err(va, "Cannot locate reference to Direct3DCreate9 ABI in DLL");
        return VLC_EGENERIC;
    }

    /* */
    LPDIRECT3D9 d3dobj;
    d3dobj = Create9(D3D_SDK_VERSION);
    if (!d3dobj) {
        msg_Err(va, "Direct3DCreate9 failed");
        return VLC_EGENERIC;
    }
    sys->d3dobj = d3dobj;

    /* */
    D3DADAPTER_IDENTIFIER9 *d3dai = &sys->d3dai;
    if (FAILED(IDirect3D9_GetAdapterIdentifier(sys->d3dobj,
                                               D3DADAPTER_DEFAULT, 0, d3dai))) {
        msg_Warn(va, "IDirect3D9_GetAdapterIdentifier failed");
        ZeroMemory(d3dai, sizeof(*d3dai));
    }

    /* */
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp.Windowed               = TRUE;
    d3dpp.hDeviceWindow          = NULL;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp.BackBufferCount        = 0;                  /* FIXME what to put here */
    d3dpp.BackBufferFormat       = D3DFMT_X8R8G8B8;    /* FIXME what to put here */
    d3dpp.BackBufferWidth        = 0;
    d3dpp.BackBufferHeight       = 0;
    d3dpp.EnableAutoDepthStencil = FALSE;

    /* Direct3D needs a HWND to create a device, even without using ::Present
    this HWND is used to alert Direct3D when there's a change of focus window.
    For now, use GetDesktopWindow, as it looks harmless */
    LPDIRECT3DDEVICE9 d3ddev;
    if (FAILED(IDirect3D9_CreateDevice(d3dobj, D3DADAPTER_DEFAULT,
                                       D3DDEVTYPE_HAL, GetDesktopWindow(),
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                       D3DCREATE_MULTITHREADED,
                                       &d3dpp, &d3ddev))) {
        msg_Err(va, "IDirect3D9_CreateDevice failed");
        return VLC_EGENERIC;
    }
    sys->dx_sys.d3ddev = (IUnknown*) d3ddev;

    return VLC_SUCCESS;
}

/**
 * It releases a Direct3D device and its resources.
 */
static void D3dDestroyDevice(vlc_va_t *va)
{
    if (va->sys->d3dobj)
        IDirect3D9_Release(va->sys->d3dobj);
}
/**
 * It describes our Direct3D object
 */
static char *DxDescribe(vlc_va_sys_t *va)
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
    D3DADAPTER_IDENTIFIER9 *id = &va->d3dai;

    const char *vendor = "Unknown";
    for (int i = 0; vendors[i].id != 0; i++) {
        if (vendors[i].id == id->VendorId) {
            vendor = vendors[i].name;
            break;
        }
    }

    char *description;
    if (asprintf(&description, "DXVA2 (%.*s, vendor %lu(%s), device %lu, revision %lu)",
                 (int)sizeof(id->Description), id->Description,
                 id->VendorId, vendor, id->DeviceId, id->Revision) < 0)
        return NULL;
    return description;
}

/**
 * It creates a Direct3D device manager
 */
static int D3dCreateDeviceManager(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &va->sys->dx_sys;

    HRESULT (WINAPI *CreateDeviceManager9)(UINT *pResetToken,
                                           IDirect3DDeviceManager9 **);
    CreateDeviceManager9 =
      (void *)GetProcAddress(dx_sys->hdecoder_dll,
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
    sys->devmng = devmng;
    msg_Info(va, "obtained IDirect3DDeviceManager9");

    HRESULT hr = IDirect3DDeviceManager9_ResetDevice(devmng, (IDirect3DDevice9*) dx_sys->d3ddev, token);
    if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_ResetDevice failed: %08x", (unsigned)hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
/**
 * It destroys a Direct3D device manager
 */
static void D3dDestroyDeviceManager(vlc_va_t *va)
{
    if (va->sys->devmng)
        IDirect3DDeviceManager9_Release(va->sys->devmng);
}

/**
 * It creates a DirectX video service
 */
static int DxCreateVideoService(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &va->sys->dx_sys;

    HRESULT (WINAPI *CreateVideoService)(IDirect3DDevice9 *,
                                         REFIID riid,
                                         void **ppService);
    CreateVideoService =
      (void *)GetProcAddress(dx_sys->hdecoder_dll, "DXVA2CreateVideoService");

    if (!CreateVideoService) {
        msg_Err(va, "cannot load function");
        return 4;
    }
    msg_Info(va, "DXVA2CreateVideoService Success!");

    HRESULT hr;

    HANDLE device;
    hr = IDirect3DDeviceManager9_OpenDeviceHandle(sys->devmng, &device);
    if (FAILED(hr)) {
        msg_Err(va, "OpenDeviceHandle failed");
        return VLC_EGENERIC;
    }
    sys->device = device;

    void *pv;
    hr = IDirect3DDeviceManager9_GetVideoService(sys->devmng, device,
                                        &IID_IDirectXVideoDecoderService, &pv);
    if (FAILED(hr)) {
        msg_Err(va, "GetVideoService failed");
        return VLC_EGENERIC;
    }
    dx_sys->d3ddec = pv;

    return VLC_SUCCESS;
}

/**
 * It destroys a DirectX video service
 */
static void DxDestroyVideoService(vlc_va_t *va)
{
    if (va->sys->device)
        IDirect3DDeviceManager9_CloseDeviceHandle(va->sys->devmng, va->sys->device);
}

static void ReleaseInputList(input_list_t *p_list)
{
    CoTaskMemFree(p_list->list);
}

static int DxGetInputList(vlc_va_t *va, input_list_t *p_list)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    UINT input_count = 0;
    GUID *input_list = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderDeviceGuids((IDirectXVideoDecoderService*) dx_sys->d3ddec,
                                                                 &input_count,
                                                                 &input_list))) {
        msg_Err(va, "IDirectXVideoDecoderService_GetDecoderDeviceGuids failed");
        return VLC_EGENERIC;
    }

    p_list->count = input_count;
    p_list->list = input_list;
    p_list->pf_release = ReleaseInputList;
    return VLC_SUCCESS;
}

static int DxSetupOutput(vlc_va_t *va, const GUID *input, const video_format_t *fmt)
{
    VLC_UNUSED(fmt);
    int err = VLC_EGENERIC;
    UINT      output_count = 0;
    D3DFORMAT *output_list = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderRenderTargets((IDirectXVideoDecoderService*) va->sys->dx_sys.d3ddec,
                                                                   input,
                                                                   &output_count,
                                                                   &output_list))) {
        msg_Err(va, "IDirectXVideoDecoderService_GetDecoderRenderTargets failed");
        return VLC_EGENERIC;
    }

    for (unsigned j = 0; j < output_count; j++) {
        const D3DFORMAT f = output_list[j];
        const d3d_format_t *format = D3dFindFormat(f);
        if (format) {
            msg_Dbg(va, "%s is supported for output", format->name);
        } else {
            msg_Dbg(va, "%d is supported for output (%4.4s)", f, (const char*)&f);
        }
    }

    /* */
    for (unsigned j = 0; d3d_formats[j].name; j++) {
        const d3d_format_t *format = &d3d_formats[j];

        /* */
        bool is_supported = false;
        for (unsigned k = 0; !is_supported && k < output_count; k++) {
            is_supported = format->format == output_list[k];
        }
        if (!is_supported)
            continue;

        /* We have our solution */
        msg_Dbg(va, "Using decoder output '%s'", format->name);
        va->sys->render = format->format;
        err = VLC_SUCCESS;
        break;
    }
    CoTaskMemFree(output_list);
    return err;
}

/**
 * It creates a DXVA2 decoder using the given video format
 */
static int DxCreateVideoDecoder(vlc_va_t *va, int codec_id, const video_format_t *fmt, bool b_threading)
{
    VLC_UNUSED(b_threading);

    vlc_va_sys_t *p_sys = va->sys;
    directx_sys_t *sys = &va->sys->dx_sys;

    if (FAILED(IDirectXVideoDecoderService_CreateSurface((IDirectXVideoDecoderService*) sys->d3ddec,
                                                         sys->surface_width,
                                                         sys->surface_height,
                                                         sys->surface_count - 1,
                                                         p_sys->render,
                                                         D3DPOOL_DEFAULT,
                                                         0,
                                                         DXVA2_VideoDecoderRenderTarget,
                                                         (LPDIRECT3DSURFACE9*) sys->hw_surface,
                                                         NULL))) {
        msg_Err(va, "IDirectXVideoAccelerationService_CreateSurface failed");
        sys->surface_count = 0;
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "IDirectXVideoAccelerationService_CreateSurface succeed with %d surfaces (%dx%d)",
            sys->surface_count, sys->surface_width, sys->surface_height);

    /* */
    DXVA2_VideoDesc dsc;
    ZeroMemory(&dsc, sizeof(dsc));
    dsc.SampleWidth     = fmt->i_width;
    dsc.SampleHeight    = fmt->i_height;
    dsc.Format          = p_sys->render;
    if (fmt->i_frame_rate > 0 && fmt->i_frame_rate_base > 0) {
        dsc.InputSampleFreq.Numerator   = fmt->i_frame_rate;
        dsc.InputSampleFreq.Denominator = fmt->i_frame_rate_base;
    } else {
        dsc.InputSampleFreq.Numerator   = 0;
        dsc.InputSampleFreq.Denominator = 0;
    }
    dsc.OutputFrameFreq = dsc.InputSampleFreq;
    dsc.UABProtectionLevel = FALSE;
    dsc.Reserved = 0;

    /* FIXME I am unsure we can let unknown everywhere */
    DXVA2_ExtendedFormat *ext = &dsc.SampleFormat;
    ext->SampleFormat = 0;//DXVA2_SampleUnknown;
    ext->VideoChromaSubsampling = 0;//DXVA2_VideoChromaSubsampling_Unknown;
    ext->NominalRange = 0;//DXVA2_NominalRange_Unknown;
    ext->VideoTransferMatrix = 0;//DXVA2_VideoTransferMatrix_Unknown;
    ext->VideoLighting = 0;//DXVA2_VideoLighting_Unknown;
    ext->VideoPrimaries = 0;//DXVA2_VideoPrimaries_Unknown;
    ext->VideoTransferFunction = 0;//DXVA2_VideoTransFunc_Unknown;

    /* List all configurations available for the decoder */
    UINT                      cfg_count = 0;
    DXVA2_ConfigPictureDecode *cfg_list = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderConfigurations((IDirectXVideoDecoderService*) sys->d3ddec,
                                                                    &sys->input,
                                                                    &dsc,
                                                                    NULL,
                                                                    &cfg_count,
                                                                    &cfg_list))) {
        msg_Err(va, "IDirectXVideoDecoderService_GetDecoderConfigurations failed");
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "we got %d decoder configurations", cfg_count);

    /* Select the best decoder configuration */
    int cfg_score = 0;
    for (unsigned i = 0; i < cfg_count; i++) {
        const DXVA2_ConfigPictureDecode *cfg = &cfg_list[i];

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
            p_sys->cfg = *cfg;
            cfg_score = score;
        }
    }
    CoTaskMemFree(cfg_list);
    if (cfg_score <= 0) {
        msg_Err(va, "Failed to find a supported decoder configuration");
        return VLC_EGENERIC;
    }

    /* Create the decoder */
    IDirectXVideoDecoder *decoder;
    if (FAILED(IDirectXVideoDecoderService_CreateVideoDecoder((IDirectXVideoDecoderService*) sys->d3ddec,
                                                              &sys->input,
                                                              &dsc,
                                                              &p_sys->cfg,
                                                              (LPDIRECT3DSURFACE9*) sys->hw_surface,
                                                              sys->surface_count,
                                                              &decoder))) {
        msg_Err(va, "IDirectXVideoDecoderService_CreateVideoDecoder failed");
        return VLC_EGENERIC;
    }
    sys->decoder = (IUnknown*) decoder;

    msg_Dbg(va, "IDirectXVideoDecoderService_CreateVideoDecoder succeed");
    return VLC_SUCCESS;
}

static void DxDestroyVideoDecoder(vlc_va_t *va)
{
    VLC_UNUSED(va);
}

static int DxResetVideoDecoder(vlc_va_t *va)
{
    msg_Err(va, "DxResetVideoDecoder unimplemented");
    return VLC_EGENERIC;
}

static picture_t *DxAllocPicture(vlc_va_t *va, const video_format_t *fmt, unsigned index)
{
    video_format_t src_fmt = *fmt;
    src_fmt.i_chroma = VLC_CODEC_D3D9_OPAQUE;
    picture_sys_t *pic_sys = calloc(1, sizeof(*pic_sys));
    if (unlikely(pic_sys == NULL))
        return NULL;
    pic_sys->surface = (LPDIRECT3DSURFACE9) va->sys->dx_sys.hw_surface[index];

    picture_resource_t res = {
        .p_sys = pic_sys,
    };
    picture_t *pic = picture_NewFromResource(&src_fmt, &res);
    if (unlikely(pic == NULL))
    {
        free(pic_sys);
        return NULL;
    }
    return pic;
}
