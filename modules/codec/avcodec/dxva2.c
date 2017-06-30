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
#include <vlc_plugin.h>

#define DXVA2API_USE_BITFIELDS
#define COBJMACROS
#include <libavcodec/dxva2.h>
#include "../../video_chroma/d3d9_fmt.h"

#define D3D_Device          IDirect3DDevice9
#define D3D_DecoderType     IDirectXVideoDecoder
#define D3D_DecoderDevice   IDirectXVideoDecoderService
#define D3D_DecoderSurface  IDirect3DSurface9
#include "directx_va.h"

static int Open(vlc_va_t *, AVCodecContext *, enum PixelFormat,
                const es_format_t *, picture_sys_t *p_sys);
static void Close(vlc_va_t *, void **);

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
    { "P010",   MAKEFOURCC('P','0','1','0'),    VLC_CODEC_P010 },

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

    /* avcodec internals */
    struct dxva_context hw;
};


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

static int DxCreateVideoDecoder(vlc_va_t *, int codec_id,
                                const video_format_t *, unsigned surface_count);
static void DxDestroyVideoDecoder(vlc_va_t *);
static int DxResetVideoDecoder(vlc_va_t *);
static void SetupAVCodecContext(vlc_va_t *);

void SetupAVCodecContext(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    directx_sys_t *dx_sys = &sys->dx_sys;

    sys->hw.decoder = dx_sys->decoder;
    sys->hw.cfg = &sys->cfg;
    sys->hw.surface_count = dx_sys->va_pool.surface_count;
    sys->hw.surface = dx_sys->hw_surface;

    if (IsEqualGUID(&dx_sys->input, &DXVA_Intel_H264_NoFGT_ClearVideo))
        sys->hw.workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;
}

static void d3d9_pic_context_destroy(struct picture_context_t *opaque)
{
    struct va_pic_context *pic_ctx = (struct va_pic_context*)opaque;
    if (pic_ctx->va_surface)
    {
        ReleasePictureSys(&pic_ctx->picsys);
        va_surface_Release(pic_ctx->va_surface);
        free(pic_ctx);
    }
}

static struct va_pic_context *CreatePicContext(IDirect3DSurface9 *);

static struct picture_context_t *d3d9_pic_context_copy(struct picture_context_t *ctx)
{
    struct va_pic_context *src_ctx = (struct va_pic_context*)ctx;
    struct va_pic_context *pic_ctx = CreatePicContext(src_ctx->picsys.surface);
    if (unlikely(pic_ctx==NULL))
        return NULL;
    pic_ctx->va_surface = src_ctx->va_surface;
    va_surface_AddRef(pic_ctx->va_surface);
    return &pic_ctx->s;
}

static struct va_pic_context *CreatePicContext(IDirect3DSurface9 *surface)
{
    struct va_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx==NULL))
        return NULL;
    pic_ctx->s.destroy = d3d9_pic_context_destroy;
    pic_ctx->s.copy    = d3d9_pic_context_copy;
    pic_ctx->picsys.surface = surface;
    AcquirePictureSys(&pic_ctx->picsys);
    return pic_ctx;
}

static struct va_pic_context* NewSurfacePicContext(vlc_va_t *va, int surface_index)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    struct va_pic_context *pic_ctx = CreatePicContext(dx_sys->hw_surface[surface_index]);
    if (unlikely(pic_ctx==NULL))
        return NULL;
    /* all the resources are acquired during surfaces init, and a second time in
     * CreatePicContext(), undo one of them otherwise we need an extra release
     * when the pool is emptied */
    ReleasePictureSys(&pic_ctx->picsys);
    return pic_ctx;
}

static int Get(vlc_va_t *va, picture_t *pic, uint8_t **data)
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

    int res = va_pool_Get(&sys->dx_sys.va_pool, pic);
    if (likely(res==VLC_SUCCESS))
        *data = (uint8_t*)((struct va_pic_context*)pic->context)->picsys.surface;
    return res;
}

static void Close(vlc_va_t *va, void **ctx)
{
    vlc_va_sys_t *sys = va->sys;

    (void) ctx;

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

    ctx->hwaccel_context = NULL;

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

    dx_sys->va_pool.pf_create_device           = D3dCreateDevice;
    dx_sys->va_pool.pf_destroy_device          = D3dDestroyDevice;
    dx_sys->va_pool.pf_create_device_manager   = D3dCreateDeviceManager;
    dx_sys->va_pool.pf_destroy_device_manager  = D3dDestroyDeviceManager;
    dx_sys->va_pool.pf_create_video_service    = DxCreateVideoService;
    dx_sys->va_pool.pf_destroy_video_service   = DxDestroyVideoService;
    dx_sys->va_pool.pf_create_decoder_surfaces = DxCreateVideoDecoder;
    dx_sys->va_pool.pf_destroy_surfaces        = DxDestroyVideoDecoder;
    dx_sys->va_pool.pf_setup_avcodec_ctx       = SetupAVCodecContext;
    dx_sys->va_pool.pf_new_surface_context     = NewSurfacePicContext;
    dx_sys->pf_get_input_list          = DxGetInputList;
    dx_sys->pf_setup_output            = DxSetupOutput;
    dx_sys->psz_decoder_dll            = TEXT("DXVA2.DLL");

    va->sys = sys;

    dx_sys->d3ddev = NULL;
    if (p_sys!=NULL)
    {
        D3DSURFACE_DESC src;
        if (SUCCEEDED(IDirect3DSurface9_GetDesc(p_sys->surface, &src)))
            sys->render = src.Format;
        IDirect3DSurface9_GetDevice(p_sys->surface, &dx_sys->d3ddev );
    }

    err = directx_va_Open(va, &sys->dx_sys, true);
    if (err!=VLC_SUCCESS)
        goto error;

    err = directx_va_Setup(va, &sys->dx_sys, ctx, fmt);
    if (err != VLC_SUCCESS)
        goto error;

    ctx->hwaccel_context = &sys->hw;

    /* TODO print the hardware name/vendor for debugging purposes */
    va->description = DxDescribe(sys);
    va->get     = Get;
    return VLC_SUCCESS;

error:
    Close(va, NULL);
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
    sys->dx_sys.d3ddev = d3ddev;

    return VLC_SUCCESS;
}

/**
 * It releases a Direct3D device and its resources.
 */
static void D3dDestroyDevice(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    if (va->sys->d3dobj)
        IDirect3D9_Release(va->sys->d3dobj);
    if (dx_sys->d3ddev)
        IDirect3DDevice9_Release(dx_sys->d3ddev);
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

    HRESULT hr = IDirect3DDeviceManager9_ResetDevice(devmng, dx_sys->d3ddev, token);
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
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    if (va->sys->device)
        IDirect3DDeviceManager9_CloseDeviceHandle(va->sys->devmng, va->sys->device);
    if (dx_sys->d3ddec)
        IDirectXVideoDecoderService_Release(dx_sys->d3ddec);
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
    if (FAILED(IDirectXVideoDecoderService_GetDecoderDeviceGuids(dx_sys->d3ddec,
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
    if (FAILED(IDirectXVideoDecoderService_GetDecoderRenderTargets(va->sys->dx_sys.d3ddec,
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
    for (unsigned pass = 0; pass < 2 && err != VLC_SUCCESS; ++pass)
    {
        for (unsigned j = 0; d3d_formats[j].name; j++) {
            const d3d_format_t *format = &d3d_formats[j];

            /* */
            bool is_supported = false;
            for (unsigned k = 0; !is_supported && k < output_count; k++) {
                is_supported = format->format == output_list[k];
            }
            if (!is_supported)
                continue;
            if (pass == 0 && format->format != va->sys->render)
                continue;

            /* We have our solution */
            msg_Dbg(va, "Using decoder output '%s'", format->name);
            va->sys->render = format->format;
            err = VLC_SUCCESS;
            break;
        }
    }
    CoTaskMemFree(output_list);
    return err;
}

/**
 * It creates a DXVA2 decoder using the given video format
 */
static int DxCreateVideoDecoder(vlc_va_t *va, int codec_id,
                                const video_format_t *fmt, unsigned surface_count)
{
    vlc_va_sys_t *p_sys = va->sys;
    directx_sys_t *sys = &va->sys->dx_sys;
    HRESULT hr;

    hr = IDirectXVideoDecoderService_CreateSurface(sys->d3ddec,
                                                         fmt->i_width,
                                                         fmt->i_height,
                                                         surface_count - 1,
                                                         p_sys->render,
                                                         D3DPOOL_DEFAULT,
                                                         0,
                                                         DXVA2_VideoDecoderRenderTarget,
                                                         sys->hw_surface,
                                                         NULL);
    if (FAILED(hr)) {
        msg_Err(va, "IDirectXVideoAccelerationService_CreateSurface %d failed (hr=0x%0lx)", surface_count - 1, hr);
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "IDirectXVideoAccelerationService_CreateSurface succeed with %d surfaces (%dx%d)",
            surface_count, fmt->i_width, fmt->i_height);

    IDirect3DSurface9 *tstCrash;
    hr = IDirectXVideoDecoderService_CreateSurface(sys->d3ddec,
                                                         fmt->i_width,
                                                         fmt->i_height,
                                                         0,
                                                         p_sys->render,
                                                         D3DPOOL_DEFAULT,
                                                         0,
                                                         DXVA2_VideoDecoderRenderTarget,
                                                         &tstCrash,
                                                         NULL);
    if (FAILED(hr)) {
        msg_Err(va, "extra buffer impossible, avoid a crash (hr=0x%0lx)", hr);
        for (unsigned i = 0; i < surface_count; i++)
            IDirect3DSurface9_Release( sys->hw_surface[i] );
        return VLC_EGENERIC;
    }
    IDirect3DSurface9_Release(tstCrash);

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
    if (FAILED(IDirectXVideoDecoderService_GetDecoderConfigurations(sys->d3ddec,
                                                                    &sys->input,
                                                                    &dsc,
                                                                    NULL,
                                                                    &cfg_count,
                                                                    &cfg_list))) {
        msg_Err(va, "IDirectXVideoDecoderService_GetDecoderConfigurations failed");
        for (unsigned i = 0; i < surface_count; i++)
            IDirect3DSurface9_Release( sys->hw_surface[i] );
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
    if (FAILED(IDirectXVideoDecoderService_CreateVideoDecoder(sys->d3ddec,
                                                              &sys->input,
                                                              &dsc,
                                                              &p_sys->cfg,
                                                              sys->hw_surface,
                                                              surface_count,
                                                              &decoder))) {
        msg_Err(va, "IDirectXVideoDecoderService_CreateVideoDecoder failed");
        for (unsigned i = 0; i < surface_count; i++)
            IDirect3DSurface9_Release( sys->hw_surface[i] );
        return VLC_EGENERIC;
    }
    sys->decoder = decoder;

    msg_Dbg(va, "IDirectXVideoDecoderService_CreateVideoDecoder succeed");
    return VLC_SUCCESS;
}

static void DxDestroyVideoDecoder(vlc_va_t *va)
{
    directx_sys_t *dx_sys = &va->sys->dx_sys;
    if (dx_sys->decoder)
    {
        IDirectXVideoDecoder_Release(dx_sys->decoder);
        dx_sys->decoder = NULL;
        for (unsigned i = 0; i < dx_sys->va_pool.surface_count; i++)
            IDirect3DSurface9_Release(dx_sys->hw_surface[i]);
    }
}

static int DxResetVideoDecoder(vlc_va_t *va)
{
    msg_Err(va, "DxResetVideoDecoder unimplemented");
    return VLC_EGENERIC;
}
