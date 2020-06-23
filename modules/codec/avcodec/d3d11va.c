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

struct d3d11va_pic_context
{
    struct d3d11_pic_context  ctx;
    struct vlc_va_surface_t   *va_surface;
};

#define D3D11VA_PICCONTEXT_FROM_PICCTX(pic_ctx)  \
    container_of((pic_ctx), struct d3d11va_pic_context, ctx.s)

#include "directx_va.h"

static int Open(vlc_va_t *, AVCodecContext *, enum PixelFormat hwfmt, const AVPixFmtDescriptor *,
                const es_format_t *, vlc_decoder_device *, video_format_t *, vlc_video_context **);

vlc_module_begin()
    set_description(N_("Direct3D11 Video Acceleration"))
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

DEFINE_GUID(DXVA_Intel_H264_NoFGT_ClearVideo,       0x604F8E68, 0x4951, 0x4c54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6);

DEFINE_GUID(DXVA2_NoEncrypt,                        0x1b81bed0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

struct vlc_va_sys_t
{
    d3d11_device_t               *d3d_dev;

    vlc_video_context            *vctx;

    const d3d_format_t           *render_fmt;

    /* Video decoder */
    D3D11_VIDEO_DECODER_CONFIG   cfg;
    const directx_va_mode_t      *selected_decoder;
    ID3D11VideoDevice            *d3ddec;

    /* avcodec internals */
    struct AVD3D11VAContext      hw;

    /* pool */
    va_pool_t                     *va_pool;
    ID3D11VideoDecoderOutputView  *hw_surface[MAX_SURFACE_COUNT];
    ID3D11ShaderResourceView      *renderSrc[MAX_SURFACE_COUNT * D3D11_MAX_SHADER_VIEW];
};

/* */
static int D3dCreateDevice(vlc_va_t *);

static int DxGetInputList(vlc_va_t *, input_list_t *);
static int DxSetupOutput(vlc_va_t *, const directx_va_mode_t *, const video_format_t *);

static int DxCreateDecoderSurfaces(vlc_va_t *, int codec_id,
                                   const video_format_t *fmt, size_t surface_count);
static void DxDestroySurfaces(void *);

static void SetupAVCodecContext(void *opaque, AVCodecContext *avctx)
{
    vlc_va_sys_t *sys = opaque;
    sys->hw.cfg = &sys->cfg;
    sys->hw.surface = sys->hw_surface;
    sys->hw.context_mutex = sys->d3d_dev->context_mutex;
    sys->hw.workaround = sys->selected_decoder->workaround;
    avctx->hwaccel_context = &sys->hw;
}

static void d3d11va_pic_context_destroy(picture_context_t *ctx)
{
    struct d3d11va_pic_context *pic_ctx = D3D11VA_PICCONTEXT_FROM_PICCTX(ctx);
    struct vlc_va_surface_t *va_surface = pic_ctx->va_surface;
    static_assert(offsetof(struct d3d11va_pic_context, ctx.s) == 0,
        "Cast assumption failure");
    d3d11_pic_context_destroy(ctx);
    va_surface_Release(va_surface);
}

static picture_context_t *d3d11va_pic_context_copy(picture_context_t *ctx)
{
    struct d3d11va_pic_context *src_ctx = D3D11VA_PICCONTEXT_FROM_PICCTX(ctx);
    struct d3d11va_pic_context *pic_ctx = malloc(sizeof(*pic_ctx));
    if (unlikely(pic_ctx==NULL))
        return NULL;
    *pic_ctx = *src_ctx;
    vlc_video_context_Hold(pic_ctx->ctx.s.vctx);
    va_surface_AddRef(pic_ctx->va_surface);
    for (int i=0;i<D3D11_MAX_SHADER_VIEW; i++)
    {
        pic_ctx->ctx.picsys.resource[i]  = src_ctx->ctx.picsys.resource[i];
        pic_ctx->ctx.picsys.renderSrc[i] = src_ctx->ctx.picsys.renderSrc[i];
    }
    AcquireD3D11PictureSys(&pic_ctx->ctx.picsys);
    return &pic_ctx->ctx.s;
}

static struct d3d11va_pic_context *CreatePicContext(
                                                  UINT slice,
                                                  ID3D11ShaderResourceView *renderSrc[D3D11_MAX_SHADER_VIEW],
                                                  vlc_video_context *vctx)
{
    struct d3d11va_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx==NULL))
        return NULL;
    pic_ctx->ctx.s = (picture_context_t) {
        d3d11va_pic_context_destroy, d3d11va_pic_context_copy,
        vlc_video_context_Hold(vctx),
    };

    ID3D11Resource *p_resource;
    ID3D11ShaderResourceView_GetResource(renderSrc[0], &p_resource);

    D3D11_TEXTURE2D_DESC txDesc;
    ID3D11Texture2D_GetDesc((ID3D11Texture2D*)p_resource, &txDesc);

    pic_ctx->ctx.picsys.slice_index = slice;
    for (int i=0;i<D3D11_MAX_SHADER_VIEW; i++)
    {
        pic_ctx->ctx.picsys.resource[i] = p_resource;
        pic_ctx->ctx.picsys.renderSrc[i] = renderSrc[i];
    }
    AcquireD3D11PictureSys(&pic_ctx->ctx.picsys);
    ID3D11Resource_Release(p_resource);
    return pic_ctx;
}

static picture_context_t* NewSurfacePicContext(vlc_va_t *va, vlc_va_surface_t *va_surface)
{
    vlc_va_sys_t *sys = va->sys;
    ID3D11VideoDecoderOutputView *surface = sys->hw_surface[va_surface_GetIndex(va_surface)];
    ID3D11ShaderResourceView *resourceView[D3D11_MAX_SHADER_VIEW];

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ID3D11VideoDecoderOutputView_GetDesc(surface, &viewDesc);

    for (size_t i=0; i<D3D11_MAX_SHADER_VIEW; i++)
        resourceView[i] = sys->renderSrc[viewDesc.Texture2D.ArraySlice*D3D11_MAX_SHADER_VIEW + i];

    struct d3d11va_pic_context *pic_ctx = CreatePicContext(
                                                  viewDesc.Texture2D.ArraySlice,
                                                  resourceView, sys->vctx);
    if (unlikely(pic_ctx==NULL))
        return NULL;
    pic_ctx->va_surface = va_surface;
    return &pic_ctx->ctx.s;
}

static int Get(vlc_va_t *va, picture_t *pic, uint8_t **data)
{
    vlc_va_sys_t *sys = va->sys;
    vlc_va_surface_t *va_surface = va_pool_Get(sys->va_pool);
    if (unlikely(va_surface == NULL))
        return VLC_ENOITEM;
    pic->context = NewSurfacePicContext(va, va_surface);
    if (unlikely(pic->context == NULL))
    {
        va_surface_Release(va_surface);
        return VLC_ENOITEM;
    }
    data[3] = (uint8_t*)sys->hw_surface[va_surface_GetIndex(va_surface)];
    return VLC_SUCCESS;
}

static void Close(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    if (sys->vctx)
        vlc_video_context_Release(sys->vctx);

    if (sys->va_pool)
        va_pool_Close(sys->va_pool);
}

static const struct vlc_va_operations ops = { Get, Close, };

static int Open(vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat hwfmt, const AVPixFmtDescriptor *desc,
                const es_format_t *fmt_in, vlc_decoder_device *dec_device,
                video_format_t *fmt_out, vlc_video_context **vtcx_out)
{
    int err = VLC_EGENERIC;

    ctx->hwaccel_context = NULL;

    if ( hwfmt != AV_PIX_FMT_D3D11VA_VLD )
        return VLC_EGENERIC;

    d3d11_decoder_device_t *devsys = GetD3D11OpaqueDevice( dec_device );
    if ( devsys == NULL )
        return VLC_EGENERIC;

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    va->sys = sys;

    sys->render_fmt = NULL;
    sys->d3d_dev = &devsys->d3d_dev;
    if (sys->d3d_dev->context_mutex == INVALID_HANDLE_VALUE)
        msg_Warn(va, "No mutex found to lock the decoder");

    struct va_pool_cfg pool_cfg = {
        D3dCreateDevice,
        DxDestroySurfaces,
        DxCreateDecoderSurfaces,
        SetupAVCodecContext,
        sys,
    };

    sys->va_pool = va_pool_Create(va, &pool_cfg);
    if (sys->va_pool == NULL)
    {
        err = VLC_EGENERIC;
        goto error;
    }

    video_format_t final_fmt = *fmt_out;
    static const directx_sys_t dx_sys = { DxGetInputList, DxSetupOutput };
    sys->selected_decoder = directx_va_Setup(va, &dx_sys, ctx, desc, fmt_in, isXboxHardware(sys->d3d_dev),
                                             &final_fmt, &sys->hw.surface_count);
    if (sys->selected_decoder == NULL)
    {
        err = VLC_EGENERIC;
        goto error;
    }

    final_fmt.i_chroma = sys->render_fmt->fourcc;
    err = va_pool_SetupDecoder(va, sys->va_pool, ctx, &final_fmt, sys->hw.surface_count);
    if (err != VLC_SUCCESS)
        goto error;

    msg_Info(va, "Using D3D11VA (%ls, vendor %x(%s), device %x, revision %x)",
                sys->d3d_dev->adapterDesc.Description,
                sys->d3d_dev->adapterDesc.VendorId, DxgiVendorStr(sys->d3d_dev->adapterDesc.VendorId),
                sys->d3d_dev->adapterDesc.DeviceId, sys->d3d_dev->adapterDesc.Revision);

    sys->vctx = D3D11CreateVideoContext(dec_device, sys->render_fmt->formatTexture);
    if (sys->vctx == NULL)
    {
        msg_Dbg(va, "no video context");
        err = VLC_EGENERIC;
        goto error;
    }

    va->ops = &ops;
    *fmt_out = final_fmt;
    *vtcx_out = sys->vctx;
    return VLC_SUCCESS;

error:
    Close(va);
    return err;
}

/**
 * It creates a Direct3D device usable for decoding
 */
static int D3dCreateDevice(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    HRESULT hr;

    assert(sys->d3d_dev->d3ddevice && sys->d3d_dev->d3dcontext);
    void *d3dvidctx = NULL;
    hr = ID3D11DeviceContext_QueryInterface(sys->d3d_dev->d3dcontext, &IID_ID3D11VideoContext, &d3dvidctx);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoContext Interface. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    sys->hw.video_context = d3dvidctx;

    void *d3dviddev = NULL;
    hr = ID3D11Device_QueryInterface(sys->d3d_dev->d3ddevice, &IID_ID3D11VideoDevice, &d3dviddev);
    if (FAILED(hr)) {
       msg_Err(va, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       ID3D11VideoContext_Release(sys->hw.video_context);
       return VLC_EGENERIC;
    }
    sys->d3ddec = d3dviddev;

    return VLC_SUCCESS;
}

static void ReleaseInputList(input_list_t *p_list)
{
    free(p_list->list);
}

static int DxGetInputList(vlc_va_t *va, input_list_t *p_list)
{
    vlc_va_sys_t *sys = va->sys;
    HRESULT hr;

    UINT input_count = ID3D11VideoDevice_GetVideoDecoderProfileCount(sys->d3ddec);

    p_list->count = input_count;
    p_list->list = calloc(input_count, sizeof(*p_list->list));
    if (unlikely(p_list->list == NULL)) {
        return VLC_ENOMEM;
    }
    p_list->pf_release = ReleaseInputList;

    for (unsigned i = 0; i < input_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderProfile(sys->d3ddec, i, &p_list->list[i]);
        if (FAILED(hr))
        {
            msg_Err(va, "GetVideoDecoderProfile %d failed. (hr=0x%lX)", i, hr);
            ReleaseInputList(p_list);
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static const d3d_format_t *D3D11_FindDXGIFormat(DXGI_FORMAT dxgi)
{
    for (const d3d_format_t *output_format = GetRenderFormatList();
         output_format->name != NULL; ++output_format)
    {
        if (output_format->formatTexture == dxgi &&
                is_d3d11_opaque(output_format->fourcc))
        {
            return output_format;
        }
    }
    return NULL;
}

static int DxSetupOutput(vlc_va_t *va, const directx_va_mode_t *mode, const video_format_t *fmt)
{
    vlc_va_sys_t *sys = va->sys;
    HRESULT hr;

#ifndef NDEBUG
    BOOL bSupported = false;
    for (int format = 0; format < 188; format++) {
        hr = ID3D11VideoDevice_CheckVideoDecoderFormat(sys->d3ddec, mode->guid, format, &bSupported);
        if (SUCCEEDED(hr) && bSupported)
            msg_Dbg(va, "format %s is supported for output", DxgiFormatToStr(format));
    }
#endif

    if (!directx_va_canUseDecoder(va, sys->d3d_dev->adapterDesc.VendorId, sys->d3d_dev->adapterDesc.DeviceId,
                                  mode->guid, sys->d3d_dev->WDDM.build))
    {
        msg_Warn(va, "GPU blocklisted for %s codec", mode->name);
        return VLC_EGENERIC;
    }

    const d3d_format_t *processorInput[4];
    int idx = 0;
    const d3d_format_t *decoder_format;
    UINT supportFlags = D3D11_FORMAT_SUPPORT_DECODER_OUTPUT | D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    decoder_format = FindD3D11Format( va, va->sys->d3d_dev, 0, D3D11_RGB_FORMAT|D3D11_YUV_FORMAT,
                                      mode->bit_depth, mode->log2_chroma_h+1, mode->log2_chroma_w+1,
                                      D3D11_CHROMA_GPU, supportFlags );
    if (decoder_format == NULL)
        decoder_format = FindD3D11Format( va, va->sys->d3d_dev, 0, D3D11_RGB_FORMAT|D3D11_YUV_FORMAT,
                                        mode->bit_depth, 0, 0, D3D11_CHROMA_GPU, supportFlags );
    if (decoder_format == NULL && mode->bit_depth > 10)
        decoder_format = FindD3D11Format( va, va->sys->d3d_dev, 0, D3D11_RGB_FORMAT|D3D11_YUV_FORMAT,
                                        10, 0, 0, D3D11_CHROMA_GPU, supportFlags );
    if (decoder_format == NULL)
        decoder_format = FindD3D11Format( va, va->sys->d3d_dev, 0, D3D11_RGB_FORMAT|D3D11_YUV_FORMAT,
                                        0, 0, 0, D3D11_CHROMA_GPU, supportFlags );
    if (decoder_format != NULL)
    {
        msg_Dbg(va, "favor decoder format %s", decoder_format->name);
        processorInput[idx++] = decoder_format;
    }

    if (decoder_format == NULL || decoder_format->formatTexture != DXGI_FORMAT_NV12)
        processorInput[idx++] = D3D11_FindDXGIFormat(DXGI_FORMAT_NV12);
    processorInput[idx++] = D3D11_FindDXGIFormat(DXGI_FORMAT_420_OPAQUE);
    processorInput[idx++] = NULL;

    /* */
    for (idx = 0; processorInput[idx] != NULL; ++idx)
    {
        BOOL is_supported = false;
        hr = ID3D11VideoDevice_CheckVideoDecoderFormat(sys->d3ddec, mode->guid, processorInput[idx]->formatTexture, &is_supported);
        if (SUCCEEDED(hr) && is_supported)
            msg_Dbg(va, "%s output is supported for decoder %s.", DxgiFormatToStr(processorInput[idx]->formatTexture), mode->name);
        else
        {
            msg_Dbg(va, "Can't get a decoder output format %s for decoder %s.", DxgiFormatToStr(processorInput[idx]->formatTexture), mode->name);
            continue;
        }

       // check if we can create render texture of that format
       // check the decoder can output to that format
       if ( !DeviceSupportsFormat(sys->d3d_dev->d3ddevice, processorInput[idx]->formatTexture,
                                  D3D11_FORMAT_SUPPORT_SHADER_LOAD) )
       {
#ifndef ID3D11VideoContext_VideoProcessorBlt
           msg_Dbg(va, "Format %s needs a processor but is not supported",
                   DxgiFormatToStr(processorInput[idx]->formatTexture));
#else
           if ( !DeviceSupportsFormat(sys->d3d_dev->d3ddevice, processorInput[idx]->formatTexture,
                                      D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT) )
           {
               msg_Dbg(va, "Format %s needs a processor but is not available",
                       DxgiFormatToStr(processorInput[idx]->formatTexture));
               continue;
           }
#endif
        }

        D3D11_VIDEO_DECODER_DESC decoderDesc;
        ZeroMemory(&decoderDesc, sizeof(decoderDesc));
        decoderDesc.Guid = *mode->guid;
        decoderDesc.SampleWidth = fmt->i_width;
        decoderDesc.SampleHeight = fmt->i_height;
        decoderDesc.OutputFormat = processorInput[idx]->formatTexture;

        UINT cfg_count = 0;
        hr = ID3D11VideoDevice_GetVideoDecoderConfigCount( sys->d3ddec, &decoderDesc, &cfg_count );
        if (FAILED(hr))
        {
            msg_Err( va, "Failed to get configuration for decoder %s. (hr=0x%lX)", mode->name, hr );
            continue;
        }
        if (cfg_count == 0) {
            msg_Err( va, "No decoder configuration possible for %s %dx%d",
                     DxgiFormatToStr(decoderDesc.OutputFormat),
                     decoderDesc.SampleWidth, decoderDesc.SampleHeight );
            continue;
        }

        msg_Dbg(va, "Using output format %s for decoder %s", DxgiFormatToStr(processorInput[idx]->formatTexture), mode->name);
        sys->render_fmt = processorInput[idx];
        return VLC_SUCCESS;
    }

    msg_Dbg(va, "Output format from picture source not supported.");
    return VLC_EGENERIC;
}

static bool CanUseDecoderPadding(const vlc_va_sys_t *sys)
{
    /* Qualcomm hardware has issues with textures and pixels that should not be
    * part of the decoded area */
    return sys->d3d_dev->adapterDesc.VendorId != GPU_MANUFACTURER_QUALCOMM;
}

/**
 * It creates a Direct3D11 decoder using the given video format
 */
static int DxCreateDecoderSurfaces(vlc_va_t *va, int codec_id,
                                   const video_format_t *fmt, size_t surface_count)
{
    vlc_va_sys_t *sys = va->sys;
    HRESULT hr;

    ID3D10Multithread *pMultithread;
    hr = ID3D11Device_QueryInterface( sys->d3d_dev->d3ddevice, &IID_ID3D10Multithread, (void **)&pMultithread);
    if (SUCCEEDED(hr)) {
        ID3D10Multithread_SetMultithreadProtected(pMultithread, TRUE);
        ID3D10Multithread_Release(pMultithread);
    }

#if VLC_WINSTORE_APP
    /* On the Xbox 1/S, any decoding of H264 with one dimension over 2304
     * crashes totally the device */
    if (codec_id == AV_CODEC_ID_H264 &&
        (fmt->i_width > 2304 || fmt->i_height > 2304) &&
        isXboxHardware(sys->d3d_dev))
    {
        msg_Warn(va, "%dx%d resolution not supported by your hardware", fmt->i_width, fmt->i_height);
        return VLC_EGENERIC;
    }
#endif

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ZeroMemory(&viewDesc, sizeof(viewDesc));
    viewDesc.DecodeProfile = *sys->selected_decoder->guid;
    viewDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = fmt->i_width;
    texDesc.Height = fmt->i_height;
    texDesc.MipLevels = 1;
    texDesc.Format = sys->render_fmt->formatTexture;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0;
    texDesc.ArraySize = surface_count;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DECODER;
    texDesc.CPUAccessFlags = 0;

    if (DeviceSupportsFormat(sys->d3d_dev->d3ddevice, texDesc.Format, D3D11_FORMAT_SUPPORT_SHADER_LOAD))
        texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D *p_texture;
    hr = ID3D11Device_CreateTexture2D( sys->d3d_dev->d3ddevice, &texDesc, NULL, &p_texture );
    if (FAILED(hr)) {
        msg_Err(va, "CreateTexture2D %zu failed. (hr=0x%lX)", surface_count, hr);
        return VLC_EGENERIC;
    }

    unsigned surface_idx;
    for (surface_idx = 0; surface_idx < surface_count; surface_idx++) {
        viewDesc.Texture2D.ArraySlice = surface_idx;

        hr = ID3D11VideoDevice_CreateVideoDecoderOutputView( sys->d3ddec,
                                                                (ID3D11Resource*) p_texture,
                                                                &viewDesc,
                                                                &sys->hw_surface[surface_idx] );
        if (FAILED(hr)) {
            msg_Err(va, "CreateVideoDecoderOutputView %d failed. (hr=0x%lX)", surface_idx, hr);
            ID3D11Texture2D_Release(p_texture);
            return VLC_EGENERIC;
        }

        if (texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
        {
            ID3D11Texture2D *textures[D3D11_MAX_SHADER_VIEW] = {p_texture, p_texture, p_texture};
            D3D11_AllocateResourceView(va, sys->d3d_dev->d3ddevice, sys->render_fmt, textures, surface_idx,
                                &sys->renderSrc[surface_idx * D3D11_MAX_SHADER_VIEW]);
        }
    }
    ID3D11Texture2D_Release(p_texture);
    msg_Dbg(va, "ID3D11VideoDecoderOutputView succeed with %zu surfaces (%dx%d)",
            surface_count, fmt->i_width, fmt->i_height);

    D3D11_VIDEO_DECODER_DESC decoderDesc;
    ZeroMemory(&decoderDesc, sizeof(decoderDesc));
    decoderDesc.Guid = *sys->selected_decoder->guid;
    decoderDesc.SampleWidth = fmt->i_width;
    decoderDesc.SampleHeight = fmt->i_height;
    decoderDesc.OutputFormat = sys->render_fmt->formatTexture;

    UINT cfg_count;
    hr = ID3D11VideoDevice_GetVideoDecoderConfigCount( sys->d3ddec, &decoderDesc, &cfg_count );
    if (FAILED(hr)) {
        msg_Err(va, "GetVideoDecoderConfigCount failed. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    /* List all configurations available for the decoder */
    D3D11_VIDEO_DECODER_CONFIG cfg_list[cfg_count];
    for (unsigned i = 0; i < cfg_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderConfig( sys->d3ddec, &decoderDesc, i, &cfg_list[i] );
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
    hr = ID3D11VideoDevice_CreateVideoDecoder( sys->d3ddec, &decoderDesc, &sys->cfg, &decoder );
    if (FAILED(hr)) {
        msg_Err(va, "ID3D11VideoDevice_CreateVideoDecoder failed. (hr=0x%lX)", hr);
        sys->hw.decoder = NULL;
        return VLC_EGENERIC;
    }
    sys->hw.decoder = decoder;

    msg_Dbg(va, "DxCreateDecoderSurfaces succeed");
    return VLC_SUCCESS;
}

static void DxDestroySurfaces(void *opaque)
{
    vlc_va_sys_t *sys = opaque;
    if (sys->hw.decoder)
        ID3D11VideoDecoder_Release(sys->hw.decoder);
    if (sys->hw_surface[0]) {
        for (unsigned i = 0; i < sys->hw.surface_count; i++)
        {
            for (int j = 0; j < D3D11_MAX_SHADER_VIEW; j++)
            {
                if (sys->renderSrc[i*D3D11_MAX_SHADER_VIEW + j])
                    ID3D11ShaderResourceView_Release(sys->renderSrc[i*D3D11_MAX_SHADER_VIEW + j]);
            }
            ID3D11VideoDecoderOutputView_Release( sys->hw_surface[i] );
        }
    }

    if (sys->d3ddec)
        ID3D11VideoDevice_Release(sys->d3ddec);
    if (sys->hw.video_context)
        ID3D11VideoContext_Release(sys->hw.video_context);

    free(sys);
}
