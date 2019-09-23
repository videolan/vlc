/*****************************************************************************
 * dxva2.c: Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
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

typedef picture_sys_d3d9_t VA_PICSYS;
#include "va_surface.h"

#include "directx_va.h"

static int Open(vlc_va_t *, AVCodecContext *, enum PixelFormat,
                const es_format_t *, void *);

vlc_module_begin()
    set_description(N_("DirectX Video Acceleration (DXVA) 2.0"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_va_callback(Open, 110)
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
} d3d9_format_t;
/* XXX Prefered format must come first */
static const d3d9_format_t d3d_formats[] = {
    { "YV12",   MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_YV12 },
    { "NV12",   MAKEFOURCC('N','V','1','2'),    VLC_CODEC_NV12 },
    //{ "IMC3",   MAKEFOURCC('I','M','C','3'),    VLC_CODEC_YV12 },
    { "P010",   MAKEFOURCC('P','0','1','0'),    VLC_CODEC_P010 },

    { NULL, 0, 0 }
};

static const d3d9_format_t *D3dFindFormat(D3DFORMAT format)
{
    for (unsigned i = 0; d3d_formats[i].name; i++) {
        if (d3d_formats[i].format == format)
            return &d3d_formats[i];
    }
    return NULL;
}

struct vlc_va_sys_t
{
    /* Direct3D */
    d3d9_handle_t          hd3d;
    d3d9_device_t          d3d_dev;

    /* DLL */
    HINSTANCE              dxva2_dll;

    /* Device manager */
    IDirect3DDeviceManager9  *devmng;
    HANDLE                   device;

    /* Video service */
    D3DFORMAT                    render;

    /* Video decoder */
    DXVA2_ConfigPictureDecode    cfg;
    GUID                         decoder_guid;
    IDirectXVideoDecoderService  *d3ddec;

    /* pool */
    va_pool_t           va_pool;
    IDirect3DSurface9   *hw_surface[MAX_SURFACE_COUNT];

    /* avcodec internals */
    struct dxva_context hw;
};


/* */
static int D3dCreateDevice(vlc_va_t *);
static void D3dDestroyDevice(vlc_va_t *);

static int DxGetInputList(vlc_va_t *, input_list_t *);
static int DxSetupOutput(vlc_va_t *, const GUID *, const video_format_t *);

static int DxCreateVideoDecoder(vlc_va_t *, int codec_id,
                                const video_format_t *, unsigned surface_count);
static void DxDestroyVideoDecoder(vlc_va_sys_t *);

static void SetupAVCodecContext(vlc_va_sys_t *sys, unsigned surfaces)
{
    sys->hw.cfg = &sys->cfg;
    sys->hw.surface_count = surfaces;
    sys->hw.surface = sys->hw_surface;

    if (IsEqualGUID(&sys->decoder_guid, &DXVA_Intel_H264_NoFGT_ClearVideo))
        sys->hw.workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;
}

static void d3d9_pic_context_destroy(struct picture_context_t *opaque)
{
    struct va_pic_context *pic_ctx = (struct va_pic_context*)opaque;
    if (pic_ctx->va_surface)
    {
        ReleaseD3D9PictureSys(&pic_ctx->picsys);
        va_surface_Release(pic_ctx->va_surface);
        free(pic_ctx);
    }
}

static struct va_pic_context *CreatePicContext(IDirect3DSurface9 *, IDirectXVideoDecoder *);

static struct picture_context_t *d3d9_pic_context_copy(struct picture_context_t *ctx)
{
    struct va_pic_context *src_ctx = (struct va_pic_context*)ctx;
    struct va_pic_context *pic_ctx = CreatePicContext(src_ctx->picsys.surface, src_ctx->picsys.decoder);
    if (unlikely(pic_ctx==NULL))
        return NULL;
    pic_ctx->va_surface = src_ctx->va_surface;
    va_surface_AddRef(pic_ctx->va_surface);
    return &pic_ctx->s;
}

static struct va_pic_context *CreatePicContext(IDirect3DSurface9 *surface, IDirectXVideoDecoder *decoder)
{
    struct va_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx==NULL))
        return NULL;
    pic_ctx->s.destroy = d3d9_pic_context_destroy;
    pic_ctx->s.copy    = d3d9_pic_context_copy;
    pic_ctx->picsys.surface = surface;
    pic_ctx->picsys.decoder = decoder;
    AcquireD3D9PictureSys(&pic_ctx->picsys);
    return pic_ctx;
}

static struct va_pic_context* NewSurfacePicContext(vlc_va_t *va, int surface_index, vlc_va_surface_t *va_surface)
{
    vlc_va_sys_t *sys = va->sys;
    struct va_pic_context *pic_ctx = CreatePicContext(sys->hw_surface[surface_index], sys->hw.decoder);
    if (unlikely(pic_ctx==NULL))
        return NULL;
    /* all the resources are acquired during surfaces init, and a second time in
     * CreatePicContext(), undo one of them otherwise we need an extra release
     * when the pool is emptied */
    ReleaseD3D9PictureSys(&pic_ctx->picsys);
    pic_ctx->va_surface = va_surface;
    return pic_ctx;
}

static int Get(vlc_va_t *va, picture_t *pic, uint8_t **data)
{
    vlc_va_sys_t *sys = va->sys;

    /* Check the device */
    HRESULT hr = IDirect3DDeviceManager9_TestDevice(sys->devmng, sys->device);
    if (FAILED(hr)) {
        if (hr == DXVA2_E_NEW_VIDEO_DEVICE)
            msg_Warn(va, "New video device detected.");
        else
            msg_Err(va, "device not usable. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    picture_context_t *pic_ctx = va_pool_Get(&sys->va_pool);
    if (likely(pic_ctx==NULL))
        return VLC_ENOITEM;

    pic->context = pic_ctx;
    *data = (uint8_t*)((struct va_pic_context*)pic->context)->picsys.surface;
    return VLC_SUCCESS;
}

static void Close(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    va_pool_Close(va, &sys->va_pool);

    if (sys->dxva2_dll)
        FreeLibrary(sys->dxva2_dll);

    free(sys);
}

static const struct vlc_va_operations ops = { Get, Close, };

static int Open(vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat pix_fmt,
                const es_format_t *fmt, void *picsys)
{
    int err = VLC_EGENERIC;

    if (pix_fmt != AV_PIX_FMT_DXVA2_VLD)
        return VLC_EGENERIC;

    ctx->hwaccel_context = NULL;

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    /* Load dll*/
    picture_sys_d3d9_t *p_sys = picsys;
    if (p_sys!=NULL && p_sys->surface!=NULL)
    {
        IDirect3DDevice9 *device;
        if ( FAILED(IDirect3DSurface9_GetDevice( p_sys->surface, &device )) )
        {
            free( sys );
            return VLC_EGENERIC;
        }
        if ( D3D9_CreateExternal(&sys->hd3d, device) != VLC_SUCCESS ||
             FAILED(D3D9_CreateDeviceExternal( device, &sys->hd3d, &sys->d3d_dev)) )
        {
            IDirect3DDevice9_Release(device);
            free( sys );
            return VLC_EGENERIC;
        }
        D3DSURFACE_DESC src;
        if (SUCCEEDED(IDirect3DSurface9_GetDesc(p_sys->surface, &src)))
            sys->render = src.Format;
    }
    else if (D3D9_Create(va, &sys->hd3d) != VLC_SUCCESS) {
        msg_Warn(va, "cannot load d3d9.dll");
        free( sys );
        return VLC_EGENERIC;
    }

    /* Load dll*/
    sys->dxva2_dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (!sys->dxva2_dll) {
        msg_Warn(va, "cannot load DXVA2 decoder DLL");
        D3D9_Destroy( &sys->hd3d );
        free( sys );
        return VLC_EGENERIC;
    }

    static const struct va_pool_cfg pool_cfg = {
        D3dCreateDevice,
        D3dDestroyDevice,
        DxCreateVideoDecoder,
        DxDestroyVideoDecoder,
        SetupAVCodecContext,
        NewSurfacePicContext,
    };

    va->sys = sys;

    err = va_pool_Open(va, &pool_cfg, &sys->va_pool);
    if (err!=VLC_SUCCESS)
        goto error;

    video_format_t fmt_out;
    static const directx_sys_t dx_sys = { DxGetInputList, DxSetupOutput };
    err = directx_va_Setup(va, &dx_sys, ctx, fmt, 0, &fmt_out, &sys->hw.surface_count, &sys->decoder_guid);
    if (err != VLC_SUCCESS)
        goto error;

    err = va_pool_SetupDecoder(va, &sys->va_pool, ctx, &fmt_out, sys->hw.surface_count);
    if (err != VLC_SUCCESS)
        goto error;

    D3DADAPTER_IDENTIFIER9 d3dai;
    if (SUCCEEDED(IDirect3D9_GetAdapterIdentifier(sys->hd3d.obj,
                                               sys->d3d_dev.adapterId, 0, &d3dai))) {
        msg_Info(va, "Using DXVA2 (%.*s, vendor %s(%lx), device %lx, revision %lx)",
                    (int)sizeof(d3dai.Description), d3dai.Description,
                    DxgiVendorStr(d3dai.VendorId), d3dai.VendorId, d3dai.DeviceId, d3dai.Revision);
    }

    ctx->hwaccel_context = &sys->hw;

    va->ops = &ops;
    return VLC_SUCCESS;

error:
    Close(va);
    return VLC_EGENERIC;
}
/* */

/**
 * It creates a Direct3D device usable for DXVA 2
 */
static int D3dCreateDevice(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    if (!sys->d3d_dev.dev) {
        HRESULT hr = D3D9_CreateDevice(va, &sys->hd3d, -1, &sys->d3d_dev);
        if (FAILED(hr))
        {
            msg_Err(va, "IDirect3D9_CreateDevice failed");
            return VLC_EGENERIC;
        }
    } else {
        msg_Dbg(va, "Reusing Direct3D9 device");
    }

    HRESULT (WINAPI *CreateDeviceManager9)(UINT *pResetToken,
                                           IDirect3DDeviceManager9 **);
    CreateDeviceManager9 =
      (void *)GetProcAddress(sys->dxva2_dll,
                             "DXVA2CreateDirect3DDeviceManager9");

    if (!CreateDeviceManager9) {
        msg_Err(va, "cannot load function");
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "got CreateDeviceManager9");

    UINT token;
    if (FAILED(CreateDeviceManager9(&token, &sys->devmng))) {
        msg_Err(va, " OurDirect3DCreateDeviceManager9 failed");
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "obtained IDirect3DDeviceManager9");

    HRESULT hr = IDirect3DDeviceManager9_ResetDevice(sys->devmng, sys->d3d_dev.dev, token);
    if (FAILED(hr)) {
        msg_Err(va, "IDirect3DDeviceManager9_ResetDevice failed: 0x%lX)", hr);
        IDirect3DDeviceManager9_Release(sys->devmng);
        return VLC_EGENERIC;
    }

    hr = IDirect3DDeviceManager9_OpenDeviceHandle(sys->devmng, &sys->device);
    if (FAILED(hr)) {
        msg_Err(va, "OpenDeviceHandle failed");
        IDirect3DDeviceManager9_Release(sys->devmng);
        return VLC_EGENERIC;
    }

    void *pv;
    hr = IDirect3DDeviceManager9_GetVideoService(sys->devmng, sys->device,
                                        &IID_IDirectXVideoDecoderService, &pv);
    if (FAILED(hr)) {
        msg_Err(va, "GetVideoService failed");
        IDirect3DDeviceManager9_CloseDeviceHandle(sys->devmng, sys->device);
        IDirect3DDeviceManager9_Release(sys->devmng);
        return VLC_EGENERIC;
    }
    sys->d3ddec = pv;

    return VLC_SUCCESS;
}

/**
 * It releases a Direct3D device and its resources.
 */
static void D3dDestroyDevice(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    HRESULT hr = IDirect3DDeviceManager9_CloseDeviceHandle(sys->devmng, sys->device);
    if (FAILED(hr))
        msg_Warn(va, "Failed to release device handle 0x%p. (hr=0x%lX)", sys->device, hr);
    IDirectXVideoDecoderService_Release(sys->d3ddec);
    IDirect3DDeviceManager9_Release(sys->devmng);
    D3D9_ReleaseDevice(&sys->d3d_dev);
    D3D9_Destroy( &sys->hd3d );
}

static void ReleaseInputList(input_list_t *p_list)
{
    CoTaskMemFree(p_list->list);
}

static int DxGetInputList(vlc_va_t *va, input_list_t *p_list)
{
    vlc_va_sys_t *sys = va->sys;
    UINT input_count = 0;
    GUID *input_list = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderDeviceGuids(sys->d3ddec,
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
    vlc_va_sys_t *sys = va->sys;

    D3DADAPTER_IDENTIFIER9 identifier;
    HRESULT hr = IDirect3D9_GetAdapterIdentifier(sys->hd3d.obj, sys->d3d_dev.adapterId, 0, &identifier);
    if (FAILED(hr))
        return VLC_EGENERIC;

    UINT driverBuild = identifier.DriverVersion.LowPart & 0xFFFF;
    if (identifier.VendorId == GPU_MANUFACTURER_INTEL && (identifier.DriverVersion.LowPart >> 16) >= 100)
    {
        /* new Intel driver format */
        driverBuild += ((identifier.DriverVersion.LowPart >> 16) - 100) * 1000;
    }
    if (!directx_va_canUseDecoder(va, identifier.VendorId, identifier.DeviceId,
                                  input, driverBuild))
    {
        char* psz_decoder_name = directx_va_GetDecoderName(input);
        msg_Warn(va, "GPU blacklisted for %s codec", psz_decoder_name);
        free(psz_decoder_name);
        return VLC_EGENERIC;
    }

    int err = VLC_EGENERIC;
    UINT      output_count = 0;
    D3DFORMAT *output_list = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderRenderTargets(sys->d3ddec,
                                                                   input,
                                                                   &output_count,
                                                                   &output_list))) {
        msg_Err(va, "IDirectXVideoDecoderService_GetDecoderRenderTargets failed");
        return VLC_EGENERIC;
    }

    for (unsigned j = 0; j < output_count; j++) {
        const D3DFORMAT f = output_list[j];
        const d3d9_format_t *format = D3dFindFormat(f);
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
            const d3d9_format_t *format = &d3d_formats[j];

            /* */
            bool is_supported = false;
            for (unsigned k = 0; !is_supported && k < output_count; k++) {
                is_supported = format->format == output_list[k];
            }
            if (!is_supported)
                continue;
            if (pass == 0 && format->format != sys->render)
                continue;

            /* We have our solution */
            msg_Dbg(va, "Using decoder output '%s'", format->name);
            sys->render = format->format;
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
    vlc_va_sys_t *sys = va->sys;
    HRESULT hr;

    hr = IDirectXVideoDecoderService_CreateSurface(sys->d3ddec,
                                                         fmt->i_width,
                                                         fmt->i_height,
                                                         surface_count - 1,
                                                         sys->render,
                                                         D3DPOOL_DEFAULT,
                                                         0,
                                                         DXVA2_VideoDecoderRenderTarget,
                                                         sys->hw_surface,
                                                         NULL);
    if (FAILED(hr)) {
        msg_Err(va, "IDirectXVideoAccelerationService_CreateSurface %d failed (hr=0x%lX)", surface_count - 1, hr);
        return VLC_EGENERIC;
    }
    msg_Dbg(va, "IDirectXVideoAccelerationService_CreateSurface succeed with %d surfaces (%dx%d)",
            surface_count, fmt->i_width, fmt->i_height);

    IDirect3DSurface9 *tstCrash;
    hr = IDirectXVideoDecoderService_CreateSurface(sys->d3ddec,
                                                         fmt->i_width,
                                                         fmt->i_height,
                                                         0,
                                                         sys->render,
                                                         D3DPOOL_DEFAULT,
                                                         0,
                                                         DXVA2_VideoDecoderRenderTarget,
                                                         &tstCrash,
                                                         NULL);
    if (FAILED(hr)) {
        msg_Err(va, "extra buffer impossible, avoid a crash (hr=0x%lX)", hr);
        goto error;
    }
    IDirect3DSurface9_Release(tstCrash);

    /* */
    DXVA2_VideoDesc dsc;
    ZeroMemory(&dsc, sizeof(dsc));
    dsc.SampleWidth     = fmt->i_width;
    dsc.SampleHeight    = fmt->i_height;
    dsc.Format          = sys->render;
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
    hr = IDirectXVideoDecoderService_GetDecoderConfigurations(sys->d3ddec,
                                                              &sys->decoder_guid,
                                                              &dsc,
                                                              NULL,
                                                              &cfg_count,
                                                              &cfg_list);
    if (FAILED(hr)) {
        msg_Err(va, "IDirectXVideoDecoderService_GetDecoderConfigurations failed. (hr=0x%lX)", hr);
        goto error;
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
            sys->cfg = *cfg;
            cfg_score = score;
        }
    }
    CoTaskMemFree(cfg_list);
    if (cfg_score <= 0) {
        msg_Err(va, "Failed to find a supported decoder configuration");
        goto error;
    }

    /* Create the decoder */
    /* adds a reference on each decoder surface */
    if (FAILED(IDirectXVideoDecoderService_CreateVideoDecoder(sys->d3ddec,
                                                              &sys->decoder_guid,
                                                              &dsc,
                                                              &sys->cfg,
                                                              sys->hw_surface,
                                                              surface_count,
                                                              &sys->hw.decoder))) {
        msg_Err(va, "IDirectXVideoDecoderService_CreateVideoDecoder failed");
        goto error;
    }

    msg_Dbg(va, "IDirectXVideoDecoderService_CreateVideoDecoder succeed");
    return VLC_SUCCESS;
error:
    for (unsigned i = 0; i < surface_count; i++)
        IDirect3DSurface9_Release( sys->hw_surface[i] );
    return VLC_EGENERIC;
}

static void DxDestroyVideoDecoder(vlc_va_sys_t *sys)
{
    /* releases a reference on each decoder surface */
    if (sys->hw.decoder)
        IDirectXVideoDecoder_Release(sys->hw.decoder);
    for (unsigned i = 0; i < sys->va_pool.surface_count; i++)
        IDirect3DSurface9_Release(sys->hw_surface[i]);
}
