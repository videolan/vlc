/*****************************************************************************
 * direct3d11.cpp: Windows Direct3D11 video output module
 *****************************************************************************
 * Copyright (C) 2014-2021 VLC authors and VideoLAN
 *
 * Authors: Martell Malone <martellmalone@gmail.com>
 *          Steve Lhomme <robux4@gmail.com>
 *          Alexandre Janniaux <ajanni@videolabs.io>
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

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#include <cassert>
#include <math.h>
#include <new>

#include "../../video_chroma/d3d11_fmt.h"
#include "../../hw/nvdec/nvdec_fmt.h"

#include "d3d11_quad.h"
#include "d3d11_shaders.h"
#include "d3d11_scaler.h"
#include "d3d11_tonemap.h"
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#include "d3d11_swapchain.h"
#include "common.h"
#endif

#include "../../video_chroma/copy.h"

#include <dxgi1_6.h>

using Microsoft::WRL::ComPtr;

static int  Open(vout_display_t *,
                 video_format_t *, vlc_video_context *);
static void Close(vout_display_t *);

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
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_bool("direct3d11-hw-blending", true, HW_BLENDING_TEXT, HW_BLENDING_LONGTEXT)

    add_string("d3d11-upscale-mode", "linear", UPSCALE_MODE_TEXT, UPSCALE_MODE_LONGTEXT)
        change_string_list(ppsz_upscale_mode, ppsz_upscale_mode_text)

    add_string("d3d11-hdr-mode", "auto", HDR_MODE_TEXT, HDR_MODE_LONGTEXT)
        change_string_list(ppsz_hdr_mode, ppsz_hdr_mode_text)

    add_shortcut("direct3d11")
    set_callback_display(Open, 300)
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

typedef struct vout_display_sys_t
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    struct event_thread_t    *video_wnd = nullptr;
#endif
    bool                     place_changed = false;

    int                      log_level = 1;

    display_info_t           display = {};

    d3d11_device_t           *d3d_dev = NULL;
    d3d11_decoder_device_t   *local_d3d_dev = NULL; // when opened without a video context
    d3d11_quad_t             picQuad = {};

    d3d11_gpu_fence          fence = {};

    bool                     use_staging_texture = false;
    picture_sys_d3d11_t      stagingSys = {};
    plane_t                  stagingPlanes[PICTURE_PLANE_MAX];

    // NV12/P010 to RGB for D3D11 < 11.1
    struct {
        ComPtr<ID3D11VideoDevice>               d3dviddev;
        ComPtr<ID3D11VideoContext>              d3dvidctx;
        ComPtr<ID3D11VideoProcessorEnumerator>  enumerator;
        ComPtr<ID3D11VideoProcessor>            processor;
        ComPtr<ID3D11VideoProcessorOutputView>  outputView;
    } old_feature;

    video_projection_mode_t  projection_mode;
    d3d11_vertex_shader_t    projectionVShader = {};
    d3d11_vertex_shader_t    flatVShader = {};

    // SPU
    vlc_fourcc_t             pSubpictureChromas[2] = {};
    d3d11_quad_t             regionQuad = {};
    int                      d3dregion_count = 0;
    picture_t                **d3dregions = nullptr;

    /* outside rendering */
    void *outside_opaque = nullptr;
    libvlc_video_update_output_cb            updateOutputCb = nullptr;
    libvlc_video_swap_cb                     swapCb = nullptr;
    libvlc_video_makeCurrent_cb              startEndRenderingCb = nullptr;
    libvlc_video_frameMetadata_cb            sendMetadataCb = nullptr;
    libvlc_video_output_select_plane_cb      selectPlaneCb = nullptr;

    // upscaling
    enum d3d11_upscale       upscaleMode = upscale_LinearSampler;
    d3d11_scaler             *scaleProc = nullptr;
    vout_display_place_t     scalePlace;

    // HDR mode
    enum d3d11_hdr           hdrMode = hdr_Auto;
    d3d11_tonemapper         *tonemapProc = nullptr;
} vout_display_sys_t;

static void Prepare(vout_display_t *, picture_t *, const vlc_render_subpicture *, vlc_tick_t);
static void Display(vout_display_t *, picture_t *);

static int  Direct3D11Open (vout_display_t *, video_format_t *, vlc_video_context *);
static void Direct3D11Close(vout_display_t *);

static int SetupOutputFormat(vout_display_t *, video_format_t *, vlc_video_context *, video_format_t *quad);
static int  Direct3D11CreateFormatResources (vout_display_t *, const video_format_t *);
static int  Direct3D11CreateGenericResources(vout_display_t *);
static void Direct3D11DestroyResources(vout_display_t *);

static void Direct3D11DeleteRegions(int, picture_t **);
static int Direct3D11MapSubpicture(vout_display_t *, int *, picture_t ***, const vlc_render_subpicture *);

static int Control(vout_display_t *, int);


static int UpdateDisplayFormat(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    libvlc_video_render_cfg_t cfg;

    cfg.width  = vd->cfg->display.width;
    cfg.height = vd->cfg->display.height;

    if (sys->hdrMode == hdr_Always || sys->hdrMode == hdr_Fake)
    {
        // force a fake HDR source
        // corresponds to DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
        cfg.bitdepth = 10;
        cfg.full_range = true;
        cfg.primaries  = libvlc_video_primaries_BT2020;
        cfg.colorspace = libvlc_video_colorspace_BT2020;
        cfg.transfer   = libvlc_video_transfer_func_PQ;
    }
    else if (sys->hdrMode == hdr_Never)
    {
        // corresponds to DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709
        cfg.bitdepth = 8;
        cfg.full_range = true;
        cfg.primaries  = libvlc_video_primaries_BT709;
        cfg.colorspace = libvlc_video_colorspace_BT709;
        cfg.transfer   = libvlc_video_transfer_func_BT709;
    }
    else
    {
    switch (fmt->i_chroma)
    {
    case VLC_CODEC_D3D11_OPAQUE:
    case VLC_CODEC_D3D11_OPAQUE_ALPHA:
        cfg.bitdepth = 8;
        break;
    case VLC_CODEC_D3D11_OPAQUE_RGBA:
    case VLC_CODEC_D3D11_OPAQUE_BGRA:
        cfg.bitdepth = 8;
        break;
    case VLC_CODEC_RGBA10LE:
    case VLC_CODEC_D3D11_OPAQUE_10B:
        cfg.bitdepth = 10;
        break;
    default:
        {
            const auto *p_format = vlc_fourcc_GetChromaDescription(fmt->i_chroma);
            if (p_format == NULL)
            {
                cfg.bitdepth = 8;
            }
            else
            {
                cfg.bitdepth = p_format->pixel_bits == 0 ? 8 : p_format->pixel_bits /
                                                               (p_format->plane_count==1 ? p_format->pixel_size : 1);
            }
        }
        break;
    }
    cfg.full_range = sys->picQuad.quad_fmt.color_range == COLOR_RANGE_FULL ||
                     /* the YUV->RGB conversion already output full range */
                     is_d3d11_opaque(sys->picQuad.quad_fmt.i_chroma) ||
                     vlc_fourcc_IsYUV(sys->picQuad.quad_fmt.i_chroma);
    cfg.primaries  = (libvlc_video_color_primaries_t) sys->picQuad.quad_fmt.primaries;
    cfg.colorspace = (libvlc_video_color_space_t)     sys->picQuad.quad_fmt.space;
    cfg.transfer   = (libvlc_video_transfer_func_t)   sys->picQuad.quad_fmt.transfer;
    }

    libvlc_video_output_cfg_t out;
    if (!sys->updateOutputCb( sys->outside_opaque, &cfg, &out ))
    {
        msg_Err(vd, "Failed to set format %dx%d %d bits on output", cfg.width, cfg.height, cfg.bitdepth);
        return VLC_EGENERIC;
    }

    if (sys->upscaleMode == upscale_VideoProcessor || sys->upscaleMode == upscale_SuperResolution)
    {
        D3D11_UpscalerUpdate(VLC_OBJECT(vd), sys->scaleProc, sys->d3d_dev,
                             vd->source, &sys->picQuad.quad_fmt,
                             vd->cfg->display.width, vd->cfg->display.height,
                             vd->place);

        if (D3D11_UpscalerUsed(sys->scaleProc))
        {
            D3D11_UpscalerGetSize(sys->scaleProc, &sys->picQuad.quad_fmt.i_width, &sys->picQuad.quad_fmt.i_height);

            sys->picQuad.quad_fmt.i_x_offset       = 0;
            sys->picQuad.quad_fmt.i_y_offset       = 0;
            sys->picQuad.quad_fmt.i_visible_width  = sys->picQuad.quad_fmt.i_width;
            sys->picQuad.quad_fmt.i_visible_height = sys->picQuad.quad_fmt.i_height;

            sys->picQuad.generic.i_width = sys->picQuad.quad_fmt.i_width;
            sys->picQuad.generic.i_height = sys->picQuad.quad_fmt.i_height;

            vout_display_place_t before_place = sys->scalePlace;
            sys->scalePlace.x = 0;
            sys->scalePlace.y = 0;
            sys->scalePlace.width = sys->picQuad.quad_fmt.i_width;
            sys->scalePlace.height = sys->picQuad.quad_fmt.i_height;
            sys->place_changed |= !vout_display_PlaceEquals(&before_place, &sys->scalePlace);
        }
    }

    display_info_t new_display = { };

    new_display.pixelFormat = D3D11_RenderFormat((DXGI_FORMAT)out.dxgi_format, DXGI_FORMAT_UNKNOWN, false);
    if (unlikely(new_display.pixelFormat == NULL))
    {
        msg_Err(vd, "Could not find the output format.");
        return VLC_EGENERIC;
    }

    new_display.color     = (video_color_space_t)     out.colorspace;
    new_display.transfer  = (video_transfer_func_t)   out.transfer;
    new_display.primaries = (video_color_primaries_t) out.primaries;
    new_display.b_full_range = out.full_range;
    new_display.orientation = (video_orientation_t) out.orientation;

    /* guestimate the display peak luminance */
    switch (new_display.transfer)
    {
    case TRANSFER_FUNC_LINEAR:
    case TRANSFER_FUNC_SRGB:
        new_display.luminance_peak = DEFAULT_SRGB_BRIGHTNESS;
        break;
    case TRANSFER_FUNC_SMPTE_ST2084:
        new_display.luminance_peak = MAX_PQ_BRIGHTNESS;
        break;
    default:
        new_display.luminance_peak = DEFAULT_SRGB_BRIGHTNESS;
        break;
    }

    if ( sys->display.pixelFormat == NULL ||
         ( sys->display.pixelFormat    != new_display.pixelFormat ||
           sys->display.luminance_peak != new_display.luminance_peak ||
           sys->display.color          != new_display.color ||
           sys->display.transfer       != new_display.transfer ||
           sys->display.primaries      != new_display.primaries ||
           sys->display.b_full_range   != new_display.b_full_range ||
           sys->display.orientation    != new_display.orientation ))
    {
        sys->display = new_display;
        /* TODO release the pixel shaders if the format changed */
        if (Direct3D11CreateFormatResources(vd, fmt)) {
            msg_Err(vd, "Failed to allocate format resources");
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static void UpdateSize(vout_display_t *vd)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    msg_Dbg(vd, "Detected size change %dx%d", vd->cfg->display.width,
            vd->cfg->display.height);

    UpdateDisplayFormat(vd, vd->fmt);

    const vout_display_place_t *quad_place;
    if (sys->scaleProc && D3D11_UpscalerUsed(sys->scaleProc))
        quad_place = &sys->scalePlace;
    else
        quad_place = vd->place;
    sys->picQuad.UpdateViewport( quad_place, sys->display.pixelFormat );

    d3d11_device_lock( sys->d3d_dev );

    D3D11_UpdateQuadPosition(vd, sys->d3d_dev, &sys->picQuad,
                             video_format_GetTransform(vd->source->orientation, sys->display.orientation));

    D3D11_UpdateViewpoint( vd, sys->d3d_dev, &sys->picQuad, &vd->cfg->viewpoint,
                          (float) vd->cfg->display.width / vd->cfg->display.height );

    d3d11_device_unlock( sys->d3d_dev );

    sys->place_changed = false;

#ifndef NDEBUG
    msg_Dbg( vd, "picQuad position (%.02f,%.02f) %.02fx%.02f",
             sys->picQuad.cropViewport[0].TopLeftX, sys->picQuad.cropViewport[0].TopLeftY,
             sys->picQuad.cropViewport[0].Width, sys->picQuad.cropViewport[0].Height );
#endif
}

static int SetViewpoint(vout_display_t *vd, const vlc_viewpoint_t *viewpoint)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    if ( sys->picQuad.viewpointShaderConstant.Get() )
    {
        d3d11_device_lock( sys->d3d_dev );
        D3D11_UpdateViewpoint( vd, sys->d3d_dev, &sys->picQuad, viewpoint,
                                (float) vd->cfg->display.width / vd->cfg->display.height );
        d3d11_device_unlock( sys->d3d_dev );
    }
    return VLC_SUCCESS;
}

static int UpdateStaging(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    if (!sys->use_staging_texture)
        return VLC_SUCCESS;

    /* we need a staging texture */
    ID3D11Texture2D *textures[DXGI_MAX_SHADER_VIEW] = {0};
    video_format_t texture_fmt = *vd->source;
    texture_fmt.i_width  = sys->picQuad.generic.i_width;
    texture_fmt.i_height = sys->picQuad.generic.i_height;
    if (!is_d3d11_opaque(fmt->i_chroma))
    {
        texture_fmt.i_chroma = sys->picQuad.generic.textureFormat->fourcc;
    }

    if (AllocateTextures(vd, sys->d3d_dev, sys->picQuad.generic.textureFormat, &texture_fmt,
                            false, textures, sys->stagingPlanes))
    {
        msg_Err(vd, "Failed to allocate the staging texture");
        return VLC_EGENERIC;
    }

    if (D3D11_AllocateResourceView(vlc_object_logger(vd), sys->d3d_dev->d3ddevice, sys->picQuad.generic.textureFormat,
                                textures, 0, sys->stagingSys.renderSrc))
    {
        msg_Err(vd, "Failed to allocate the staging shader view");
        return VLC_EGENERIC;
    }

    for (unsigned plane = 0; plane < DXGI_MAX_SHADER_VIEW; plane++)
        sys->stagingSys.texture[plane] = textures[plane];

    if (sys->old_feature.processor)
    {
        HRESULT hr;
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc;
        outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outDesc.Texture2D.MipSlice = 0;

        hr = sys->old_feature.d3dviddev->CreateVideoProcessorOutputView(
                                                                textures[0],
                                                                sys->old_feature.enumerator.Get(),
                                                                &outDesc,
                                                                sys->old_feature.outputView.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            msg_Dbg(vd,"Failed to create processor output. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static enum d3d11_hdr HdrModeFromString(vlc_logger *logger, const char *psz_hdr)
{
    if (strcmp("auto", psz_hdr) == 0)
        return hdr_Auto;
    if (strcmp("never", psz_hdr) == 0)
        return hdr_Never;
    if (strcmp("always", psz_hdr) == 0)
        return hdr_Always;
    if (strcmp("generate", psz_hdr) == 0)
        return hdr_Fake;

    vlc_warning(logger, "unknown HDR mode %s, using auto mode", psz_hdr);
    return hdr_Auto;
}

static void InitTonemapProcessor(vout_display_t *vd, const video_format_t *fmt_in)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    if (sys->hdrMode != hdr_Fake)
        return;

    { // check the main display is in HDR mode
    HRESULT hr;

    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    ComPtr<IDXGIOutput> dxgiOutput;
    ComPtr<IDXGIOutput6> dxgiOutput6;
    DXGI_OUTPUT_DESC1 desc1;

    hr = CreateDXGIFactory1(IID_GRAPHICS_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr))
        goto error;
    UINT adapter_index = 0;
    hr = factory->EnumAdapters(adapter_index, adapter.GetAddressOf());
    if (FAILED(hr))
        goto error;
    UINT output_index = 0;
    hr = adapter->EnumOutputs(output_index, dxgiOutput.GetAddressOf());
    if (FAILED(hr))
        goto error;
    hr = dxgiOutput.As(&dxgiOutput6);
    if (FAILED(hr))
        goto error;
    hr = dxgiOutput6->GetDesc1(&desc1);
    if (FAILED(hr))
        goto error;
    if (desc1.ColorSpace != DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
    {
        msg_Warn(vd, "not an HDR display");
        goto error;
    }
    }

    sys->tonemapProc = D3D11_TonemapperCreate(VLC_OBJECT(vd), sys->d3d_dev, fmt_in);
    if (sys->tonemapProc != NULL)
    {
        msg_Dbg(vd, "Using tonemapper");
        return;
    }

error:
    sys->hdrMode = hdr_Auto;
    msg_Dbg(vd, "failed to create the tone mapper, using default HDR mode");
}

static int ChangeSourceProjection(vout_display_t *vd, video_projection_mode_t projection)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    sys->projection_mode = projection;
    return Direct3D11CreateFormatResources(vd, vd->source);
}

static const auto ops = []{
    struct vlc_display_operations ops {};
    ops.close = Close;
    ops.prepare = Prepare;
    ops.display = Display;
    ops.control = Control;
    ops.set_viewpoint = SetViewpoint;
    ops.change_source_projection = ChangeSourceProjection;
    return ops;
}();

static int Open(vout_display_t *vd,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys = new (std::nothrow) vout_display_sys_t();
    if (!sys)
        return VLC_ENOMEM;
    vd->sys = sys;

    char *psz_hdr = var_InheritString(vd, "d3d11-hdr-mode");
    sys->hdrMode = HdrModeFromString(vlc_object_logger(vd), psz_hdr);
    free(psz_hdr);

    d3d11_decoder_device_t *dev_sys = NULL;

    sys->outside_opaque = var_InheritAddress( vd, "vout-cb-opaque" );
    sys->updateOutputCb      = (libvlc_video_update_output_cb)var_InheritAddress( vd, "vout-cb-update-output" );
    sys->swapCb              = (libvlc_video_swap_cb)var_InheritAddress( vd, "vout-cb-swap" );
    sys->startEndRenderingCb = (libvlc_video_makeCurrent_cb)var_InheritAddress( vd, "vout-cb-make-current" );
    sys->sendMetadataCb      = (libvlc_video_frameMetadata_cb)var_InheritAddress( vd, "vout-cb-metadata" );
    sys->selectPlaneCb       = (libvlc_video_output_select_plane_cb)var_InheritAddress( vd, "vout-cb-select-plane" );

    dev_sys = GetD3D11OpaqueContext( context );
    if ( dev_sys == NULL )
    {
        // No d3d11 device, we create one
        sys->local_d3d_dev = D3D11_CreateDevice(vd, NULL, false, vd->obj.force);
        if (sys->local_d3d_dev == NULL) {
            msg_Err(vd, "Could not Create the D3D11 device.");
            goto error;
        }
        dev_sys = sys->local_d3d_dev;
    }
    sys->d3d_dev = &dev_sys->d3d_dev;
    sys->projection_mode = vd->cfg->projection;

    InitTonemapProcessor(vd, vd->source);

    if ( sys->swapCb == NULL || sys->startEndRenderingCb == NULL || sys->updateOutputCb == NULL )
    {
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        msg_Err(vd, "UWP apps needs to set an external rendering target");
        goto error;
#else // WINAPI_PARTITION_DESKTOP
        if (vd->cfg->window->type == VLC_WINDOW_TYPE_HWND)
        {
            if (CommonWindowInit(vd, &sys->video_wnd,
                       sys->projection_mode != PROJECTION_MODE_RECTANGULAR))
                goto error;
        }

        /* use our internal swapchain callbacks */
        dxgi_swapchain *swap = nullptr;
        if (vd->cfg->window->type == VLC_WINDOW_TYPE_DCOMP)
            swap = DXGI_CreateLocalSwapchainHandleDComp(VLC_OBJECT(vd),
                                                        vd->cfg->window->display.dcomp_device,
                                                        vd->cfg->window->handle.dcomp_visual);
        else
            swap = DXGI_CreateLocalSwapchainHandleHwnd(VLC_OBJECT(vd), CommonVideoHWND(sys->video_wnd));
        if (unlikely(swap == NULL))
            goto error;

        sys->outside_opaque = D3D11_CreateLocalSwapchain(VLC_OBJECT(vd), sys->d3d_dev, swap,
                                                         sys->hdrMode != hdr_Never && sys->hdrMode != hdr_Always);
        if (unlikely(sys->outside_opaque == NULL))
        {
            DXGI_LocalSwapchainCleanupDevice(swap);
            goto error;
        }

        sys->updateOutputCb      = D3D11_LocalSwapchainUpdateOutput;
        sys->swapCb              = D3D11_LocalSwapchainSwap;
        sys->startEndRenderingCb = D3D11_LocalSwapchainStartEndRendering;
        sys->sendMetadataCb      = D3D11_LocalSwapchainSetMetadata;
        sys->selectPlaneCb       = D3D11_LocalSwapchainSelectPlane;
#endif // WINAPI_PARTITION_DESKTOP
    }

    if (Direct3D11Open(vd, fmtp, context)) {
        msg_Err(vd, "Direct3D11 could not be opened");
        goto error;
    }

    vlc_window_SetTitle(vd->cfg->window, VOUT_TITLE " (Direct3D11 output)");
    msg_Dbg(vd, "Direct3D11 display adapter successfully initialized");

    if (var_InheritBool(vd, "direct3d11-hw-blending") &&
        sys->regionQuad.generic.textureFormat != NULL)
    {
        sys->pSubpictureChromas[0] = sys->regionQuad.generic.textureFormat->fourcc;
        sys->pSubpictureChromas[1] = 0;
        vd->info.subpicture_chromas = sys->pSubpictureChromas;
    }
    else
        vd->info.subpicture_chromas = NULL;

    vd->ops = &ops;

    msg_Dbg(vd, "Direct3D11 Open Succeeded");

    return VLC_SUCCESS;

error:
    Close(vd);
    return VLC_EGENERIC;
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    CommonWindowClean(sys->video_wnd);
#endif
    Direct3D11Close(vd);
    delete sys;
}
static int Control(vout_display_t *vd, int query)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    bool use_scaler = false;
    if (sys->upscaleMode == upscale_VideoProcessor || sys->upscaleMode == upscale_SuperResolution)
    {
        D3D11_UpscalerUpdate(VLC_OBJECT(vd), sys->scaleProc, sys->d3d_dev,
                             vd->source, &sys->picQuad.quad_fmt,
                             vd->cfg->display.width, vd->cfg->display.height,
                             vd->place);

        if (sys->scaleProc && D3D11_UpscalerUsed(sys->scaleProc))
        {
            D3D11_UpscalerGetSize(sys->scaleProc, &sys->picQuad.quad_fmt.i_width, &sys->picQuad.quad_fmt.i_height);

            sys->picQuad.quad_fmt.i_x_offset       = 0;
            sys->picQuad.quad_fmt.i_y_offset       = 0;
            sys->picQuad.quad_fmt.i_visible_width  = sys->picQuad.quad_fmt.i_width;
            sys->picQuad.quad_fmt.i_visible_height = sys->picQuad.quad_fmt.i_height;

            sys->picQuad.generic.i_width = sys->picQuad.quad_fmt.i_width;
            sys->picQuad.generic.i_height = sys->picQuad.quad_fmt.i_height;

            use_scaler = true;
        }
    }

    if (!use_scaler)
    {
        sys->picQuad.quad_fmt.i_sar_num        = vd->source->i_sar_num;
        sys->picQuad.quad_fmt.i_sar_den        = vd->source->i_sar_den;
        sys->picQuad.quad_fmt.i_x_offset       = vd->source->i_x_offset;
        sys->picQuad.quad_fmt.i_y_offset       = vd->source->i_y_offset;
        sys->picQuad.quad_fmt.i_visible_width  = vd->source->i_visible_width;
        sys->picQuad.quad_fmt.i_visible_height = vd->source->i_visible_height;
    }

    switch (query) {
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        CommonDisplaySizeChanged(sys->video_wnd);
#endif /* WINAPI_PARTITION_DESKTOP */
        break;
    case VOUT_DISPLAY_CHANGE_SOURCE_PLACE:
        sys->place_changed = true;
        // fallthrough
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    if (use_scaler)
    {
        vout_display_place_t before_place = sys->scalePlace;
        sys->scalePlace.x = 0;
        sys->scalePlace.y = 0;
        sys->scalePlace.width = sys->picQuad.quad_fmt.i_width;
        sys->scalePlace.height = sys->picQuad.quad_fmt.i_height;
        sys->place_changed |= !vout_display_PlaceEquals(&before_place, &sys->scalePlace);
        break;
    }
    }

    if ( sys->place_changed )
    {
        UpdateSize(vd);
    }

    return VLC_SUCCESS;
}

static bool SelectRenderPlane(void *opaque, size_t plane, ID3D11RenderTargetView **targetView)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(opaque);
    if (!sys->selectPlaneCb)
    {
        *targetView = NULL;
        return plane == 0; // we only support one packed RGBA plane by default
    }
    return sys->selectPlaneCb(sys->outside_opaque, plane, (void*)targetView);
}

static int assert_ProcessorInput(vout_display_t *vd, picture_sys_d3d11_t *p_sys_src)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    if (!p_sys_src->processorInput)
    {
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
        inDesc.FourCC = 0;
        inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inDesc.Texture2D.MipSlice = 0;
        inDesc.Texture2D.ArraySlice = p_sys_src->slice_index;

        HRESULT hr;

        hr = sys->old_feature.d3dviddev->CreateVideoProcessorInputView(
                                                             p_sys_src->resource[KNOWN_DXGI_INDEX],
                                                             sys->old_feature.enumerator.Get(),
                                                             &inDesc,
                                                             &p_sys_src->processorInput);
        if (FAILED(hr))
        {
#ifndef NDEBUG
            msg_Dbg(vd,"Failed to create processor input for slice %d. (hr=0x%lX)", p_sys_src->slice_index, hr);
#endif
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}


static void PreparePicture(vout_display_t *vd, picture_t *picture,
                           const vlc_render_subpicture *subpicture,
                           vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    if (sys->picQuad.generic.textureFormat->formatTexture == DXGI_FORMAT_UNKNOWN)
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        int i;
        HRESULT hr;

        assert(sys->use_staging_texture);

        bool b_mapped = true;
        for (i = 0; i < picture->i_planes; i++) {
            hr = sys->d3d_dev->d3dcontext->Map(sys->stagingSys.resource[i],
                                         0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            if( unlikely(FAILED(hr)) )
            {
                while (i-- > 0)
                    sys->d3d_dev->d3dcontext->Unmap(sys->stagingSys.resource[i], 0);
                b_mapped = false;
                break;
            }
            sys->stagingPlanes[i].i_pitch = mappedResource.RowPitch;
            sys->stagingPlanes[i].p_pixels = static_cast<uint8_t*>(mappedResource.pData);
        }

        if (b_mapped)
        {
            for (i = 0; i < picture->i_planes; i++)
                plane_CopyPixels(&sys->stagingPlanes[i], &picture->p[i]);

            for (i = 0; i < picture->i_planes; i++)
                sys->d3d_dev->d3dcontext->Unmap(sys->stagingSys.resource[i], 0);
        }
    }
    else if (!is_d3d11_opaque(picture->format.i_chroma))
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr;

        assert(sys->use_staging_texture);

        hr = sys->d3d_dev->d3dcontext->Map(sys->stagingSys.resource[0],
                                        0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if( unlikely(FAILED(hr)) )
            msg_Err(vd, "Failed to map the %4.4s staging picture. (hr=0x%lX)", (const char*)&picture->format.i_chroma, hr);
        else
        {
            uint8_t *buf = static_cast<uint8_t*>(mappedResource.pData);
            for (int i = 0; i < picture->i_planes; i++)
            {
                sys->stagingPlanes[i].i_pitch = mappedResource.RowPitch;
                sys->stagingPlanes[i].p_pixels = buf;

                plane_CopyPixels(&sys->stagingPlanes[i], &picture->p[i]);

                buf += sys->stagingPlanes[i].i_pitch * sys->stagingPlanes[i].i_lines;
            }

            sys->d3d_dev->d3dcontext->Unmap(sys->stagingSys.resource[0], 0);
        }
    }
    else
    {
        picture_sys_d3d11_t *p_sys = ActiveD3D11PictureSys(picture);

        D3D11_TEXTURE2D_DESC srcDesc;
        p_sys->texture[KNOWN_DXGI_INDEX]->GetDesc(&srcDesc);

        if (sys->tonemapProc)
        {
            if (FAILED(D3D11_TonemapperProcess(VLC_OBJECT(vd), sys->tonemapProc, p_sys)))
                return;
        }
        else if (sys->scaleProc && D3D11_UpscalerUsed(sys->scaleProc))
        {
            if (D3D11_UpscalerScale(VLC_OBJECT(vd), sys->scaleProc, p_sys) != VLC_SUCCESS)
                return;
            uint32_t witdh, height;
            D3D11_UpscalerGetSize(sys->scaleProc, &witdh, &height);
            srcDesc.Width  = witdh;
            srcDesc.Height = height;
        }
        else if (sys->use_staging_texture)
        {
            if (sys->old_feature.processor)
            {
                if (assert_ProcessorInput(vd, p_sys) != VLC_SUCCESS)
                {
                    msg_Err(vd, "fail to create upscaler input");
                }
                else
                {
                    D3D11_VIDEO_PROCESSOR_STREAM stream{};
                    stream.Enable = TRUE;
                    stream.pInputSurface = p_sys->processorInput;

                    sys->old_feature.d3dvidctx->VideoProcessorBlt(sys->old_feature.processor.Get(),
                                                                  sys->old_feature.outputView.Get(),
                                                                  0, 1, &stream);
                }
            }
            else
            {
                D3D11_TEXTURE2D_DESC texDesc;
                sys->stagingSys.texture[0]->GetDesc(&texDesc);
                D3D11_BOX box;
                box.top = 0;
                box.bottom = __MIN(srcDesc.Height, texDesc.Height);
                box.left = 0;
                box.right = __MIN(srcDesc.Width, texDesc.Width);
                box.back = 1;
                sys->d3d_dev->d3dcontext->CopySubresourceRegion(
                                                        sys->stagingSys.resource[KNOWN_DXGI_INDEX],
                                                        0, 0, 0, 0,
                                                        p_sys->resource[KNOWN_DXGI_INDEX],
                                                        p_sys->slice_index, &box);
            }
        }
        else
        {
            if (srcDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
            {
                /* for performance reason we don't want to allocate this during
                 * display, do it preferably when creating the texture */
                assert(p_sys->renderSrc[0]!=NULL);
            }
            if ( sys->picQuad.generic.i_height != srcDesc.Height ||
                 sys->picQuad.generic.i_width  != srcDesc.Width )
            {
                /* the decoder produced different sizes than the vout, we need to
                 * adjust the vertex */
                sys->picQuad.quad_fmt.i_width  = srcDesc.Width;
                sys->picQuad.quad_fmt.i_height = srcDesc.Height;
                sys->picQuad.generic.i_width  = sys->picQuad.quad_fmt.i_width;
                sys->picQuad.generic.i_height = sys->picQuad.quad_fmt.i_height;

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
        D3D11_UpdateQuadLuminanceScale(vd, sys->d3d_dev, &sys->picQuad, (float)sys->display.luminance_peak / D3D_GetFormatLuminance(VLC_OBJECT(vd), &picture->format));
    }

    /* Render the quad */
    ID3D11ShaderResourceView **renderSrc;
    ID3D11ShaderResourceView *SRV[DXGI_MAX_SHADER_VIEW];
    if (sys->scaleProc && D3D11_UpscalerUsed(sys->scaleProc))
    {
        D3D11_UpscalerGetSRV(sys->scaleProc, SRV);
        renderSrc = SRV;
    }
    else if (sys->use_staging_texture)
        renderSrc = sys->stagingSys.renderSrc;
    else {
        picture_sys_d3d11_t *p_sys = ActiveD3D11PictureSys(picture);
        if (sys->tonemapProc)
            p_sys = D3D11_TonemapperGetOutput(sys->tonemapProc);
        renderSrc = p_sys->renderSrc;
    }
    D3D11_RenderQuad(sys->d3d_dev, &sys->picQuad,
                     sys->projection_mode == PROJECTION_MODE_RECTANGULAR ? &sys->flatVShader : &sys->projectionVShader,
                     renderSrc, SelectRenderPlane, sys);

    if (subpicture) {
        // draw the additional vertices
        for (int i = 0; i < sys->d3dregion_count; ++i) {
            if (sys->d3dregions[i])
            {
                d3d11_quad_t *quad = (d3d11_quad_t *) sys->d3dregions[i]->p_sys;
                D3D11_RenderQuad(sys->d3d_dev, quad, &sys->flatVShader,
                                 quad->picSys.renderSrc, SelectRenderPlane, sys);
            }
        }
    }

    if (sys->log_level >= 4)
    {
        vlc_tick_t render_start = vlc_tick_now();
        D3D11_WaitFence(sys->fence);
        msg_Dbg(vd, "waited %" PRId64 " ms for the render fence", MS_FROM_VLC_TICK(vlc_tick_now() - render_start));
    }
    else
    {
        D3D11_WaitFence(sys->fence);
    }
}

static void Prepare(vout_display_t *vd, picture_t *picture,
                    const vlc_render_subpicture *subpicture, vlc_tick_t date)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    d3d11_device_lock( sys->d3d_dev );
    if ( sys->startEndRenderingCb( sys->outside_opaque, true ))
    {
        if ( sys->sendMetadataCb && picture->format.mastering.max_luminance )
        {
            libvlc_video_frame_hdr10_metadata_t hdr10;
            hdr10.GreenPrimary[0] = picture->format.mastering.primaries[0];
            hdr10.GreenPrimary[1] = picture->format.mastering.primaries[1];
            hdr10.BluePrimary[0]  = picture->format.mastering.primaries[2];
            hdr10.BluePrimary[1]  = picture->format.mastering.primaries[3];
            hdr10.RedPrimary[0]   = picture->format.mastering.primaries[4];
            hdr10.RedPrimary[1]   = picture->format.mastering.primaries[5];
            hdr10.WhitePoint[0]   = picture->format.mastering.white_point[0];
            hdr10.WhitePoint[1]   = picture->format.mastering.white_point[1];
            hdr10.MinMasteringLuminance = picture->format.mastering.min_luminance;
            hdr10.MaxMasteringLuminance = picture->format.mastering.max_luminance;
            hdr10.MaxContentLightLevel = picture->format.lighting.MaxCLL;
            hdr10.MaxFrameAverageLightLevel = picture->format.lighting.MaxFALL;

            sys->sendMetadataCb( sys->outside_opaque, libvlc_video_metadata_frame_hdr10, &hdr10 );
        }

        PreparePicture(vd, picture, subpicture, date);

        sys->startEndRenderingCb( sys->outside_opaque, false );
    }
    d3d11_device_unlock( sys->d3d_dev );
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    VLC_UNUSED(picture);

    d3d11_device_lock( sys->d3d_dev );
    sys->swapCb(sys->outside_opaque);
    d3d11_device_unlock( sys->d3d_dev );
}

static const d3d_format_t *GetDirectRenderingFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    return FindD3D11Format( vd, sys->d3d_dev, i_src_chroma, DXGI_RGB_FORMAT|DXGI_YUV_FORMAT, 0, 0, 0, 0,
                            DXGI_CHROMA_CPU, supportFlags );
}

static const d3d_format_t *GetDisplayFormatByDepth(vout_display_t *vd, uint8_t bit_depth,
                                                   uint8_t widthDenominator,
                                                   uint8_t heightDenominator,
                                                   uint8_t alpha_bits,
                                                   bool from_processor,
                                                   int rgb_yuv)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    if (from_processor)
    {
        supportFlags |= D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
        if (alpha_bits == 0)
            alpha_bits = 1; // allow the video processor to alpha opaque alpha
    }
    const d3d_format_t *res;
    res = FindD3D11Format( vd, sys->d3d_dev, 0, rgb_yuv,
                            bit_depth, widthDenominator+1, heightDenominator+1, alpha_bits,
                            from_processor ? DXGI_CHROMA_GPU : DXGI_CHROMA_CPU,
                            supportFlags );
    if (res == nullptr)
    {
        msg_Dbg(vd, "No display format for %u-bit %u:%u%s%s%s", bit_depth, widthDenominator, heightDenominator,
                                                          rgb_yuv & DXGI_YUV_FORMAT ? " YUV" : "",
                                                          rgb_yuv & DXGI_RGB_FORMAT ? " RGB" : "",
                                                          from_processor ? " supporting video processor" : "");
    }
    return res;
}

static const d3d_format_t *GetBlendableFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_BLENDABLE;
    return FindD3D11Format( vd, sys->d3d_dev, i_src_chroma, DXGI_RGB_FORMAT|DXGI_YUV_FORMAT, 0, 0, 0, 8, DXGI_CHROMA_CPU, supportFlags );
}

static void InitScaleProcessor(vout_display_t *vd)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    if (sys->upscaleMode != upscale_VideoProcessor && sys->upscaleMode != upscale_SuperResolution)
        return;

    sys->scaleProc = D3D11_UpscalerCreate(VLC_OBJECT(vd), sys->d3d_dev, sys->picQuad.quad_fmt.i_chroma,
                                          sys->upscaleMode == upscale_SuperResolution, &sys->picQuad.generic.textureFormat);
    if (sys->scaleProc == NULL)
    {
        msg_Dbg(vd, "forcing linear sampler");
        sys->upscaleMode = upscale_LinearSampler;
    }

    msg_Dbg(vd, "Using %s scaler", ppsz_upscale_mode_text[sys->upscaleMode]);
}

static int Direct3D11Open(vout_display_t *vd, video_format_t *fmtp, vlc_video_context *vctx)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    video_format_t fmt;

    video_format_Copy(&fmt, vd->source);
    video_format_Copy(&sys->picQuad.quad_fmt, &fmt);
    int err = SetupOutputFormat(vd, &fmt, vctx, &sys->picQuad.quad_fmt);
    if (err != VLC_SUCCESS)
    {
        if (!is_d3d11_opaque(vd->source->i_chroma)
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            && vd->obj.force
#endif
                )
        {
            vlc_fourcc_t *list = vlc_fourcc_GetFallback(vd->source->i_chroma);
            if (list != NULL)
            {
                for (unsigned i = 0; list[i] != 0; i++) {
                    if (list[i] == vd->source->i_chroma)
                        continue;
                    fmt.i_chroma = list[i];
                    err = SetupOutputFormat(vd, &fmt, nullptr, &sys->picQuad.quad_fmt);
                    if (err == VLC_SUCCESS)
                        break;
                }
                free(list);
            }
        }
        if (err != VLC_SUCCESS)
        {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            if ( sys->swapCb == D3D11_LocalSwapchainSwap && sys->outside_opaque )
            {
                D3D11_LocalSwapchainCleanupDevice( sys->outside_opaque );
                sys->outside_opaque = nullptr;
            }
#endif // WINAPI_PARTITION_DESKTOP
            return err;
        }
    }

    if (!is_d3d11_opaque(fmt.i_chroma))
    {
        sys->picQuad.quad_fmt.i_chroma = sys->picQuad.generic.textureFormat->fourcc;
    }

    /* adjust the decoder sizes to have proper padding */
    if ( sys->picQuad.generic.textureFormat->heightDenominator != 1 ||
         sys->picQuad.generic.textureFormat->widthDenominator != 1 )
    {
        sys->picQuad.quad_fmt.i_width  = (sys->picQuad.quad_fmt.i_width  + 0x01) & ~0x01;
        sys->picQuad.quad_fmt.i_height = (sys->picQuad.quad_fmt.i_height + 0x01) & ~0x01;
    }
    sys->picQuad.generic.i_width  = sys->picQuad.quad_fmt.i_width;
    sys->picQuad.generic.i_height = sys->picQuad.quad_fmt.i_height;

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

    InitScaleProcessor(vd);

    sys->scalePlace.x = 0;
    sys->scalePlace.y = 0;
    sys->scalePlace.width = vd->cfg->display.width;
    sys->scalePlace.height = vd->cfg->display.height;
    sys->place_changed = true;

    err = UpdateDisplayFormat(vd, &sys->picQuad.quad_fmt);
    if (err != VLC_SUCCESS) {
        msg_Err(vd, "Could not update the backbuffer");
        return err;
    }

    if (Direct3D11CreateGenericResources(vd)) {
        msg_Err(vd, "Failed to allocate resources");
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        if ( sys->swapCb == D3D11_LocalSwapchainSwap && sys->outside_opaque )
        {
            D3D11_LocalSwapchainCleanupDevice( sys->outside_opaque );
            sys->outside_opaque = nullptr;
        }
#endif // WINAPI_PARTITION_DESKTOP
        return VLC_EGENERIC;
    }

    video_format_Clean(fmtp);
    *fmtp = fmt;

    sys->log_level = var_InheritInteger(vd, "verbose");

    return VLC_SUCCESS;
}

static const d3d_format_t *SelectOutputFormat(vout_display_t *vd, const video_format_t *fmt, vlc_video_context *vctx,
                                              bool allow_processor)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    const d3d_format_t *res = nullptr;

    d3d11_video_context_t *vtcx_sys = GetD3D11ContextPrivate(vctx);
    if (vtcx_sys != NULL)
    {
        if (D3D11_DeviceSupportsFormat( sys->d3d_dev, vtcx_sys->format, D3D11_FORMAT_SUPPORT_SHADER_LOAD ))
        {
            res = D3D11_RenderFormat(vtcx_sys->format, vtcx_sys->secondary ,true);
            if (likely(res != nullptr))
                return res;
            msg_Dbg(vd, "Unsupported rendering texture format %s/%s", DxgiFormatToStr(vtcx_sys->format), DxgiFormatToStr(vtcx_sys->secondary));
        }
        else
        {
            msg_Dbg(vd, "Texture format %s not supported by shaders", DxgiFormatToStr(vtcx_sys->format));
            if (!D3D11_DeviceSupportsFormat( sys->d3d_dev, vtcx_sys->format, D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT ))
            {
                msg_Dbg(vd, "Texture format %s not supported in video processor", DxgiFormatToStr(vtcx_sys->format));
            }
            else
            {
                allow_processor = true;
            }
        }
    }
    else
    {
        // look for the requested pixel format first
        res = GetDirectRenderingFormat(vd, fmt->i_chroma);
        if (res != nullptr)
            return res;
    }

    msg_Dbg(vd, "Direct rendering not usable for %4.4s", (char*)&fmt->i_chroma);

    // look for any pixel format that we can handle with enough pixels per channel
    uint8_t bits_per_channel;
    uint8_t widthDenominator, heightDenominator;
    uint8_t alpha_bits = 0;
    vlc_fourcc_t cpu_chroma;
    if (is_d3d11_opaque(fmt->i_chroma))
        cpu_chroma = DxgiFormatFourcc(vtcx_sys->format);
    else if (is_nvdec_opaque(fmt->i_chroma))
        cpu_chroma = NVDECToVlcChroma(fmt->i_chroma);
    else
        cpu_chroma = fmt->i_chroma;

    const auto *p_format = vlc_fourcc_GetChromaDescription(cpu_chroma);
    if (unlikely(p_format == NULL || p_format->plane_count == 0))
    {
        bits_per_channel = 8;
        widthDenominator = heightDenominator = 2;
    }
    else
    {
        bits_per_channel = p_format->pixel_bits /
                            (p_format->plane_count==1 ? p_format->pixel_size : 1);
        widthDenominator = heightDenominator = 1;
        for (size_t i=0; i<p_format->plane_count; i++)
        {
            if (widthDenominator < p_format->p[i].w.den)
                widthDenominator = p_format->p[i].w.den;
            if (heightDenominator < p_format->p[i].h.den)
                heightDenominator = p_format->p[1].h.den;
        }

        switch (cpu_chroma) // FIXME get this info from the core
        {
        case VLC_CODEC_YUVA:
        case VLC_CODEC_YUV422A:
        case VLC_CODEC_YUV420A:
        case VLC_CODEC_VUYA:
        case VLC_CODEC_RGBA:
        case VLC_CODEC_ARGB:
        case VLC_CODEC_BGRA:
        case VLC_CODEC_ABGR:
        case VLC_CODEC_D3D11_OPAQUE_RGBA:
        case VLC_CODEC_D3D11_OPAQUE_BGRA:
        case VLC_CODEC_D3D11_OPAQUE_ALPHA:
            alpha_bits = 8;
            break;
        case VLC_CODEC_YUVA_444_10L:
        case VLC_CODEC_YUVA_444_10B:
            alpha_bits = 10;
            break;
        case VLC_CODEC_RGBA10LE:
            bits_per_channel = 10;
            alpha_bits = 2;
            break;
        case VLC_CODEC_YUVA_444_12L:
        case VLC_CODEC_YUVA_444_12B:
            alpha_bits = 12;
            break;
        case VLC_CODEC_RGBA64:
            alpha_bits = 16;
            break;
        }
    }

    bool is_rgb = !vlc_fourcc_IsYUV(fmt->i_chroma);
    res = GetDisplayFormatByDepth(vd, bits_per_channel,
                                  widthDenominator, heightDenominator, alpha_bits,
                                  allow_processor,
                                  is_rgb ? DXGI_RGB_FORMAT : DXGI_YUV_FORMAT);
    if (res != nullptr)
        return res;
    // check RGB instead of YUV (and vice versa)
    res = GetDisplayFormatByDepth(vd, bits_per_channel,
                                  widthDenominator, heightDenominator, alpha_bits,
                                  allow_processor,
                                  is_rgb ? DXGI_YUV_FORMAT : DXGI_RGB_FORMAT);
    if (res != nullptr)
        return res;

    // look for any pixel format that we can handle
    if (is_d3d11_opaque(fmt->i_chroma))
    {
        res = GetDisplayFormatByDepth(vd, 0, 0, 0, 0, true, DXGI_YUV_FORMAT|DXGI_RGB_FORMAT);
        if (res != nullptr)
            return res;

        if (!vd->obj.force)
            // the source is hardware decoded but can't be handled
            // let other display modules deal with it
            return nullptr;
        // fallthrough
    }
    // any format, even CPU ones
    return GetDisplayFormatByDepth(vd, 0, 0, 0, 0, false, DXGI_YUV_FORMAT|DXGI_RGB_FORMAT);
}

static HRESULT SetupInternalConverter(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    auto d3d_dev = sys->d3d_dev;

    HRESULT hr;
    hr = d3d_dev->d3ddevice->QueryInterface(IID_GRAPHICS_PPV_ARGS(&sys->old_feature.d3dviddev));
    if (unlikely(FAILED(hr)))
    {
        msg_Err(vd, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
        return hr;
    }

    hr = d3d_dev->d3dcontext->QueryInterface(IID_GRAPHICS_PPV_ARGS(&sys->old_feature.d3dvidctx));
    if (unlikely(FAILED(hr)))
    {
        msg_Err(vd, "Could not Query ID3D11VideoContext Interface. (hr=0x%lX)", hr);
        return hr;
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc{};
    processorDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    processorDesc.InputFrameRate = {
        fmt->i_frame_rate, fmt->i_frame_rate_base,
    };
    processorDesc.InputWidth   = fmt->i_width;
    processorDesc.InputHeight  = fmt->i_height;
    processorDesc.OutputWidth  = fmt->i_width;
    processorDesc.OutputHeight = fmt->i_height;
    processorDesc.OutputFrameRate = {
        fmt->i_frame_rate, fmt->i_frame_rate_base,
    };
    processorDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
    hr = sys->old_feature.d3dviddev->CreateVideoProcessorEnumerator(&processorDesc, &sys->old_feature.enumerator);
    if (FAILED(hr))
    {
        msg_Dbg(vd, "Can't get a video processor for the video (error 0x%lx).", hr);
        return hr;
    }

    hr = sys->old_feature.d3dviddev->CreateVideoProcessor(sys->old_feature.enumerator.Get(), 0,
                                                        &sys->old_feature.processor);
    if (FAILED(hr))
    {
        msg_Dbg(vd, "failed to create the processor (error 0x%lx).", hr);
        return hr;
    }

    return S_OK;
}

static int SetupOutputFormat(vout_display_t *vd, video_format_t *fmt, vlc_video_context *vctx, video_format_t *quad_fmt)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    const d3d_format_t *decoder_format = nullptr;

    if (sys->hdrMode == hdr_Fake)
    {
        // force a fake HDR source
        // corresponds to DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
        vctx                         = nullptr; // TODO create an internal one from the tonemapper
        quad_fmt->i_chroma           = VLC_CODEC_RGBA10LE;
        quad_fmt->primaries          = COLOR_PRIMARIES_BT2020;
        quad_fmt->transfer           = TRANSFER_FUNC_SMPTE_ST2084;
        quad_fmt->space              = COLOR_SPACE_BT2020;
        quad_fmt->color_range        = COLOR_RANGE_FULL;
    }
    sys->picQuad.generic.textureFormat = SelectOutputFormat(vd, quad_fmt, vctx, false);
    if ( !sys->picQuad.generic.textureFormat )
    {
       msg_Err(vd, "Could not get a suitable texture pixel format");
       return VLC_EGENERIC;
    }

    if (vctx)
    {
        d3d11_video_context_t *vtcx_sys = GetD3D11ContextPrivate(vctx);
        if (vtcx_sys && sys->picQuad.generic.textureFormat->formatTexture != vtcx_sys->format)
        {
            HRESULT hr;
            // check the input format can be used as input of a VideoProcessor
            decoder_format = FindD3D11Format( vd, sys->d3d_dev, fmt->i_chroma, DXGI_RGB_FORMAT|DXGI_YUV_FORMAT, 0, 0, 0, 0,
                                              DXGI_CHROMA_GPU, D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT );
            hr = SetupInternalConverter(vd, fmt);
            if (FAILED(hr))
            {
                msg_Err(vd, "Failed to initialize internal converter. (hr=0x%lX)", hr);
                return VLC_EGENERIC;
            }
        }
    }

    msg_Dbg( vd, "Using pixel format %s for chroma %4.4s", sys->picQuad.generic.textureFormat->name,
                 (char *)&fmt->i_chroma );

    fmt->i_chroma = decoder_format ? decoder_format->fourcc : sys->picQuad.generic.textureFormat->fourcc;

    /* select the subpicture region pixel format */
    sys->regionQuad.generic.textureFormat = GetBlendableFormat(vd, VLC_CODEC_RGBA);
    if (!sys->regionQuad.generic.textureFormat)
        sys->regionQuad.generic.textureFormat = GetBlendableFormat(vd, VLC_CODEC_BGRA);

    return VLC_SUCCESS;
}

static void Direct3D11Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    Direct3D11DestroyResources(vd);

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    if ( sys->swapCb == D3D11_LocalSwapchainSwap && sys->outside_opaque )
    {
        D3D11_LocalSwapchainCleanupDevice( sys->outside_opaque );
        sys->outside_opaque = nullptr;
    }
#endif // WINAPI_PARTITION_DESKTOP

    if (sys->d3d_dev && sys->d3d_dev == &sys->local_d3d_dev->d3d_dev)
        D3D11_ReleaseDevice( sys->local_d3d_dev );

    msg_Dbg(vd, "Direct3D11 display adapter closed");
}

/* TODO : handle errors better
   TODO : separate out into smaller functions like createshaders */
static int Direct3D11CreateFormatResources(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    HRESULT hr;

    if (sys->tonemapProc == NULL && !is_d3d11_opaque(fmt->i_chroma))
        // CPU source copied in the staging texture(s)
        sys->use_staging_texture = true;
    else if (sys->old_feature.d3dviddev)
        // use a staging texture to do chroma conversion
        sys->use_staging_texture = true;
    else
        sys->use_staging_texture = false;

    d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET] = { };
    hr = D3D11_CompilePixelShaderBlob(vd, sys->d3d_dev,
                                  &sys->display, fmt->transfer,
                                  fmt->color_range == COLOR_RANGE_FULL,
                                  &sys->picQuad, pPSBlob);
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to compile the pixel shader. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    hr = D3D11_SetQuadPixelShader(VLC_OBJECT(vd), sys->d3d_dev, sys->upscaleMode != upscale_LinearSampler,
                                  &sys->picQuad, pPSBlob);
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to set the pixel shader. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    if (D3D11_AllocateQuad(vd, sys->d3d_dev, sys->projection_mode, &sys->picQuad) != VLC_SUCCESS)
    {
        msg_Err(vd, "Could not allocate quad buffers.");
       return VLC_EGENERIC;
    }

    if (D3D11_SetupQuad( vd, sys->d3d_dev, &sys->picQuad.quad_fmt, &sys->picQuad, &sys->display) != VLC_SUCCESS)
    {
        msg_Err(vd, "Could not Create the main quad picture.");
        return VLC_EGENERIC;
    }

    if (!D3D11_UpdateQuadPosition(vd, sys->d3d_dev, &sys->picQuad,
                                  video_format_GetTransform(sys->picQuad.quad_fmt.orientation, sys->display.orientation)))
    {
        msg_Err(vd, "Could not set quad picture position.");
        return VLC_EGENERIC;
    }

    if ( sys->projection_mode == PROJECTION_MODE_EQUIRECTANGULAR ||
         sys->projection_mode == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD )
        D3D11_UpdateViewpoint( vd, sys->d3d_dev, &sys->picQuad, &vd->cfg->viewpoint,
                               (float) vd->cfg->display.width / vd->cfg->display.height );

    if (is_d3d11_opaque(fmt->i_chroma)) {
        ComPtr<ID3D10Multithread> pMultithread;
        hr = sys->d3d_dev->d3ddevice->QueryInterface(IID_GRAPHICS_PPV_ARGS(&pMultithread));
        if (SUCCEEDED(hr))
            pMultithread->SetMultithreadProtected(TRUE);
    }

    return UpdateStaging(vd, fmt);
}

static int Direct3D11CreateGenericResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    HRESULT hr;

    hr = D3D11_InitFence(*sys->d3d_dev, sys->fence);
    if (SUCCEEDED(hr))
    {
        msg_Dbg(vd, "using GPU render fence");
    }

    ComPtr<ID3D11BlendState> pSpuBlendState;
    D3D11_BLEND_DESC spuBlendDesc = { };
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

    hr = sys->d3d_dev->d3ddevice->CreateBlendState(&spuBlendDesc, &pSpuBlendState);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create SPU blend state. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    sys->d3d_dev->d3dcontext->OMSetBlendState(pSpuBlendState.Get(), NULL, 0xFFFFFFFF);

    /* disable depth testing as we're only doing 2D
     * see https://msdn.microsoft.com/en-us/library/windows/desktop/bb205074%28v=vs.85%29.aspx
     * see http://rastertek.com/dx11tut11.html
    */
    D3D11_DEPTH_STENCIL_DESC stencilDesc = { };

    ComPtr<ID3D11DepthStencilState> pDepthStencilState;
    hr = sys->d3d_dev->d3ddevice->CreateDepthStencilState(&stencilDesc, &pDepthStencilState );
    if (SUCCEEDED(hr))
        sys->d3d_dev->d3dcontext->OMSetDepthStencilState(pDepthStencilState.Get(), 0);

    if (sys->regionQuad.generic.textureFormat != NULL)
    {
        d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET] = { };
        hr = D3D11_CompilePixelShaderBlob(vd, sys->d3d_dev,
                                      &sys->display, TRANSFER_FUNC_SRGB, true,
                                      &sys->regionQuad, pPSBlob);
        if (FAILED(hr))
        {
            msg_Err(vd, "Failed to create the SPU pixel shader. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
        hr = D3D11_SetQuadPixelShader(VLC_OBJECT(vd), sys->d3d_dev, true,
                                      &sys->regionQuad, pPSBlob);
        if (FAILED(hr))
        {
            msg_Err(vd, "Failed to create the SPU pixel shader. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    d3d_shader_blob VSBlob = { };
    hr = D3D11_CompileVertexShaderBlob(VLC_OBJECT(vd), sys->d3d_dev, true, &VSBlob);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to compile the flat vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    hr = D3D11_CreateVertexShader(vd, &VSBlob, sys->d3d_dev, &sys->flatVShader);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the vertex input layout. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }


    hr = D3D11_CompileVertexShaderBlob(VLC_OBJECT(vd), sys->d3d_dev, false, &VSBlob);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to compile the 360 vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    hr = D3D11_CreateVertexShader(vd, &VSBlob, sys->d3d_dev, &sys->projectionVShader);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the projection vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    const vout_display_place_t *quad_place;
    if (sys->scaleProc && D3D11_UpscalerUsed(sys->scaleProc))
        quad_place = &sys->scalePlace;
    else
        quad_place = vd->place;
    sys->picQuad.UpdateViewport( quad_place, sys->display.pixelFormat );

#ifndef NDEBUG
    msg_Dbg( vd, "picQuad position (%.02f,%.02f) %.02fx%.02f",
             sys->picQuad.cropViewport[0].TopLeftX, sys->picQuad.cropViewport[0].TopLeftY,
             sys->picQuad.cropViewport[0].Width, sys->picQuad.cropViewport[0].Height );
#endif

    D3D11_UpdateViewpoint( vd, sys->d3d_dev, &sys->picQuad, &vd->cfg->viewpoint,
                          (float) vd->cfg->display.width / vd->cfg->display.height );

    msg_Dbg(vd, "Direct3D11 resources created");
    return VLC_SUCCESS;
}

static void Direct3D11DestroyResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    if (sys->tonemapProc != NULL)
    {
        D3D11_TonemapperDestroy(sys->tonemapProc);
        sys->tonemapProc = NULL;
    }
    if (sys->scaleProc != nullptr)
    {
        D3D11_UpscalerDestroy(sys->scaleProc);
        sys->scaleProc = nullptr;
    }

    sys->picQuad.Reset();
    Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
    sys->d3dregion_count = 0;
    sys->regionQuad.Reset();

    ReleaseD3D11PictureSys(&sys->stagingSys);

    D3D11_ReleaseVertexShader(&sys->flatVShader);
    D3D11_ReleaseVertexShader(&sys->projectionVShader);

    D3D11_ReleaseFence(sys->fence);

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
    d3d11_quad_t *quad = static_cast<d3d11_quad_t *>(p_picture->p_sys);
    delete quad;
}

static int Direct3D11MapSubpicture(vout_display_t *vd, int *subpicture_region_count,
                                   picture_t ***region, const vlc_render_subpicture *subpicture)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    D3D11_TEXTURE2D_DESC texDesc;
    HRESULT hr;
    int err;

    if (sys->regionQuad.generic.textureFormat == NULL)
        return VLC_EGENERIC;

    size_t count = subpicture->regions.size;
    const struct subpicture_region_rendered *r;

    *region = static_cast<picture_t**>(calloc(count, sizeof(picture_t *)));
    if (unlikely(*region==NULL))
        return VLC_ENOMEM;
    *subpicture_region_count = count;

    int i = 0;
    vlc_vector_foreach(r, &subpicture->regions) {
        if (!r->place.width || !r->place.height)
        {
            i++;
            continue; // won't render anything, keep the cache for later
        }

        for (int j = 0; j < sys->d3dregion_count; j++) {
            picture_t *cache = sys->d3dregions[j];
            d3d11_quad_t *cache_quad = cache ? static_cast<d3d11_quad_t *>(cache->p_sys) : nullptr;
            if (cache_quad != nullptr && cache_quad->picSys.texture[KNOWN_DXGI_INDEX]) {
                cache_quad->picSys.texture[KNOWN_DXGI_INDEX]->GetDesc(&texDesc );
                if (texDesc.Format == sys->regionQuad.generic.textureFormat->formatTexture &&
                    texDesc.Width  == r->p_picture->format.i_width &&
                    texDesc.Height == r->p_picture->format.i_height) {
                    (*region)[i] = cache;
                    memset(&sys->d3dregions[j], 0, sizeof(cache)); // do not reuse this cached value a second time
                    break;
                }
            }
        }

        picture_t *quad_picture = (*region)[i];
        d3d11_quad_t *quad;
        if (quad_picture != NULL)
        {
            quad = static_cast<d3d11_quad_t*>(quad_picture->p_sys);

            video_format_Clean(&quad->quad_fmt);
            video_format_Copy(&quad->quad_fmt, &r->p_picture->format);
        }
        else
        {
            d3d11_quad_t *d3dquad = new (std::nothrow) d3d11_quad_t;
            if (unlikely(d3dquad==NULL)) {
                i++;
                continue;
            }
            quad = d3dquad;
            if (AllocateTextures(vd, sys->d3d_dev, sys->regionQuad.generic.textureFormat, &r->p_picture->format,
                                 false, d3dquad->picSys.texture, NULL)) {
                msg_Err(vd, "Failed to allocate %dx%d texture for OSD",
                        r->p_picture->format.i_width, r->p_picture->format.i_height);
                for (int j=0; j<DXGI_MAX_SHADER_VIEW; j++)
                    if (d3dquad->picSys.texture[j])
                        d3dquad->picSys.texture[j]->Release();
                delete d3dquad;
                i++;
                continue;
            }

            if (D3D11_AllocateResourceView(vlc_object_logger(vd), sys->d3d_dev->d3ddevice, sys->regionQuad.generic.textureFormat,
                                           d3dquad->picSys.texture, 0,
                                           d3dquad->picSys.renderSrc)) {
                msg_Err(vd, "Failed to create %dx%d shader view for OSD",
                        r->p_picture->format.i_width, r->p_picture->format.i_height);
                delete d3dquad;
                i++;
                continue;
            }
            video_format_Copy(&d3dquad->quad_fmt, &r->p_picture->format);
            d3dquad->generic.i_width  = r->p_picture->format.i_width;
            d3dquad->generic.i_height = r->p_picture->format.i_height;

            d3dquad->generic.textureFormat = sys->regionQuad.generic.textureFormat;
            err = D3D11_AllocateQuad(vd, sys->d3d_dev, PROJECTION_MODE_RECTANGULAR, d3dquad);
            if (err != VLC_SUCCESS)
            {
                msg_Err(vd, "Failed to allocate %dx%d quad for OSD",
                             r->p_picture->format.i_width, r->p_picture->format.i_height);
                delete d3dquad;
                i++;
                continue;
            }

            err = D3D11_SetupQuad( vd, sys->d3d_dev, &r->p_picture->format, d3dquad, &sys->display );
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to setup %dx%d quad for OSD",
                        r->p_picture->format.i_width, r->p_picture->format.i_height);
                delete d3dquad;
                i++;
                continue;
            }
            const auto picres = [](picture_sys_d3d11_t * p_sys){
                picture_resource_t res = {};
                res.p_sys      = p_sys;
                res.pf_destroy = DestroyPictureQuad;
                return res;
            }((picture_sys_d3d11_t *) d3dquad);
            video_format_t no_crop = r->p_picture->format;
            no_crop.i_x_offset = no_crop.i_y_offset = 0;
            no_crop.i_visible_width = no_crop.i_width;
            no_crop.i_visible_height = no_crop.i_height;
            (*region)[i] = picture_NewFromResource(&no_crop, &picres);
            if ((*region)[i] == NULL) {
                msg_Err(vd, "Failed to create %dx%d picture for OSD",
                        r->p_picture->format.i_width, r->p_picture->format.i_height);
                d3dquad->Reset();
                i++;
                continue;
            }
            quad_picture = (*region)[i];
            for (size_t j=0; j<ARRAY_SIZE(sys->regionQuad.d3dpixelShader); j++)
            {
                /* TODO use something more accurate if we have different formats */
                if (sys->regionQuad.d3dpixelShader[j])
                {
                    d3dquad->d3dpixelShader[j] = sys->regionQuad.d3dpixelShader[j];
                }
            }
        }

        hr = sys->d3d_dev->d3dcontext->Map(quad->picSys.resource[KNOWN_DXGI_INDEX], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if( SUCCEEDED(hr) ) {
            picture_UpdatePlanes(quad_picture, static_cast<uint8_t*>(mappedResource.pData), mappedResource.RowPitch);

            picture_CopyPixels(quad_picture, r->p_picture);

            sys->d3d_dev->d3dcontext->Unmap(quad->picSys.resource[KNOWN_DXGI_INDEX], 0);
        } else {
            msg_Err(vd, "Failed to map the SPU texture (hr=0x%lX)", hr );
            picture_Release(quad_picture);
            if ((*region)[i] == quad_picture)
                (*region)[i] = NULL;
            i++;
            continue;
        }

        D3D11_UpdateQuadPosition(vd, sys->d3d_dev, quad,
            video_format_GetTransform(ORIENT_NORMAL, sys->display.orientation));

        quad->UpdateViewport( &r->place, sys->display.pixelFormat );

        D3D11_UpdateQuadOpacity(vd, sys->d3d_dev, quad, r->i_alpha / 255.0f );
        i++;
    }
    return VLC_SUCCESS;
}
