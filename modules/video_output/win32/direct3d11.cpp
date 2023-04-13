/*****************************************************************************
 * direct3d11.cpp: Windows Direct3D11 video output module
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

#include "d3d11_quad.h"
#include "d3d11_shaders.h"
#include "d3d11_swapchain.h"

#include "common.h"
#include "../../video_chroma/copy.h"

using Microsoft::WRL::ComPtr;

static int  Open(vout_display_t *,
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
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_bool("direct3d11-hw-blending", true, HW_BLENDING_TEXT, HW_BLENDING_LONGTEXT)

    add_shortcut("direct3d11")
    set_callback_display(Open, 300)
vlc_module_end ()

typedef struct vout_display_sys_t
{
    display_win32_area_t     area;

    int                      log_level;

    /* Sensors */
    void *p_sensors = NULL;

    display_info_t           display = {};

    d3d11_device_t           *d3d_dev = NULL;
    d3d11_decoder_device_t   *local_d3d_dev = NULL; // when opened without a video context
    d3d_shader_compiler_t    *shaders = nullptr;
    d3d11_quad_t             picQuad;

#ifdef HAVE_D3D11_4_H
    d3d11_gpu_fence          fence;
#endif

    picture_sys_d3d11_t      stagingSys;
    plane_t                  stagingPlanes[PICTURE_PLANE_MAX];

    d3d11_vertex_shader_t    projectionVShader;
    d3d11_vertex_shader_t    flatVShader;

    /* copy from the decoder pool into picSquad before display
     * Uses a Texture2D with slices rather than a Texture2DArray for the decoder */
    bool                     legacy_shader = false;

    // SPU
    vlc_fourcc_t             pSubpictureChromas[2];
    d3d11_quad_t             regionQuad;
    int                      d3dregion_count = 0;
    picture_t                **d3dregions = NULL;

    /* outside rendering */
    void *outside_opaque = NULL;
    libvlc_video_update_output_cb            updateOutputCb;
    libvlc_video_swap_cb                     swapCb;
    libvlc_video_makeCurrent_cb              startEndRenderingCb;
    libvlc_video_frameMetadata_cb            sendMetadataCb;
    libvlc_video_output_select_plane_cb      selectPlaneCb;
} vout_display_sys_t;

static void Prepare(vout_display_t *, picture_t *, subpicture_t *subpicture, vlc_tick_t);
static void Display(vout_display_t *, picture_t *);

static int  Direct3D11Open (vout_display_t *, video_format_t *, vlc_video_context *);
static void Direct3D11Close(vout_display_t *);

static int SetupOutputFormat(vout_display_t *, video_format_t *, vlc_video_context *);
static int  Direct3D11CreateFormatResources (vout_display_t *, const video_format_t *);
static int  Direct3D11CreateGenericResources(vout_display_t *);
static void Direct3D11DestroyResources(vout_display_t *);

static void Direct3D11DeleteRegions(int, picture_t **);
static int Direct3D11MapSubpicture(vout_display_t *, int *, picture_t ***, subpicture_t *);

static int Control(vout_display_t *, int);


static int UpdateDisplayFormat(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    libvlc_video_render_cfg_t cfg;

    cfg.width  = vd->cfg->display.width;
    cfg.height = vd->cfg->display.height;

    switch (fmt->i_chroma)
    {
    case VLC_CODEC_D3D11_OPAQUE:
        cfg.bitdepth = 8;
        break;
    case VLC_CODEC_D3D11_OPAQUE_RGBA:
    case VLC_CODEC_D3D11_OPAQUE_BGRA:
        cfg.bitdepth = 8;
        break;
    case VLC_CODEC_D3D11_OPAQUE_10B:
        cfg.bitdepth = 10;
        break;
    default:
        {
            const vlc_chroma_description_t *p_format = vlc_fourcc_GetChromaDescription(fmt->i_chroma);
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
    cfg.full_range = fmt->color_range == COLOR_RANGE_FULL ||
                     /* the YUV->RGB conversion already output full range */
                     is_d3d11_opaque(fmt->i_chroma) ||
                     vlc_fourcc_IsYUV(fmt->i_chroma);
    cfg.primaries  = (libvlc_video_color_primaries_t) fmt->primaries;
    cfg.colorspace = (libvlc_video_color_space_t)     fmt->space;
    cfg.transfer   = (libvlc_video_transfer_func_t)   fmt->transfer;

    libvlc_video_output_cfg_t out;
    if (!sys->updateOutputCb( sys->outside_opaque, &cfg, &out ))
    {
        msg_Err(vd, "Failed to set format %dx%d %d bits on output", cfg.width, cfg.height, cfg.bitdepth);
        return VLC_EGENERIC;
    }

    display_info_t new_display = { };

    new_display.pixelFormat = D3D11_RenderFormat((DXGI_FORMAT)out.dxgi_format, false);
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
    msg_Dbg(vd, "Detected size change %dx%d", sys->area.place.width,
            sys->area.place.height);

    UpdateDisplayFormat(vd, vd->fmt);

    RECT rect_dst;
    rect_dst.left   = sys->area.place.x;
    rect_dst.right  = sys->area.place.x + sys->area.place.width;
    rect_dst.top    = sys->area.place.y;
    rect_dst.bottom = sys->area.place.y + sys->area.place.height;

    sys->picQuad.UpdateViewport( &rect_dst, sys->display.pixelFormat );

    RECT source_rect;
    source_rect.left   = vd->source->i_x_offset;
    source_rect.right  = vd->source->i_x_offset + vd->source->i_visible_width;
    source_rect.top    = vd->source->i_y_offset;
    source_rect.bottom = vd->source->i_y_offset + vd->source->i_visible_height;
    d3d11_device_lock( sys->d3d_dev );

    D3D11_UpdateQuadPosition(vd, sys->d3d_dev, &sys->picQuad, &source_rect,
                             video_format_GetTransform(vd->source->orientation, sys->display.orientation));

    D3D11_UpdateViewpoint( vd, sys->d3d_dev, &sys->picQuad, &vd->cfg->viewpoint,
                          (float) vd->cfg->display.width / vd->cfg->display.height );

    d3d11_device_unlock( sys->d3d_dev );

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
#ifdef HAVE_ID3D11VIDEODECODER
    if (sys->legacy_shader)
    {
        /* we need a staging texture */
        ID3D11Texture2D *textures[DXGI_MAX_SHADER_VIEW] = {0};
        video_format_t texture_fmt = *vd->source;
        texture_fmt.i_width  = sys->picQuad.generic.i_width;
        texture_fmt.i_height = sys->picQuad.generic.i_height;
        if (!is_d3d11_opaque(fmt->i_chroma))
            texture_fmt.i_chroma = sys->picQuad.generic.textureFormat->fourcc;

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
    }
#endif
    return VLC_SUCCESS;
}

static const auto ops = []{
    struct vlc_display_operations ops {};
    ops.close = Close;
    ops.prepare = Prepare;
    ops.display = Display;
    ops.control = Control;
    ops.set_viewpoint = SetViewpoint;
    return ops;
}();

static int Open(vout_display_t *vd,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys = new (std::nothrow) vout_display_sys_t();
    if (!sys)
        return VLC_ENOMEM;
    vd->sys = sys;

    d3d11_decoder_device_t *dev_sys = NULL;

    int ret = D3D_CreateShaderCompiler(VLC_OBJECT(vd), &sys->shaders);
    if (ret != VLC_SUCCESS)
        goto error;

    CommonInit(&sys->area);

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

    if ( sys->swapCb == NULL || sys->startEndRenderingCb == NULL || sys->updateOutputCb == NULL )
    {
#ifndef VLC_WINSTORE_APP
        if (vd->cfg->window->type == VLC_WINDOW_TYPE_HWND)
        {
            if (CommonWindowInit(vd, &sys->area,
                       vd->source->projection_mode != PROJECTION_MODE_RECTANGULAR))
                goto error;
        }

#endif /* !VLC_WINSTORE_APP */

        /* use our internal swapchain callbacks */
#if defined(HAVE_DCOMP_H) && !defined(VLC_WINSTORE_APP)
        if (vd->cfg->window->type == VLC_WINDOW_TYPE_DCOMP)
            sys->outside_opaque =
                D3D11_CreateLocalSwapchainHandleDComp(VLC_OBJECT(vd),
                                                      vd->cfg->window->display.dcomp_device,
                                                      vd->cfg->window->handle.dcomp_visual, sys->d3d_dev);
        else
#endif
            sys->outside_opaque      = D3D11_CreateLocalSwapchainHandleHwnd(VLC_OBJECT(vd), CommonVideoHWND(&sys->area), sys->d3d_dev);
        if (unlikely(sys->outside_opaque == NULL))
            goto error;
        sys->updateOutputCb      = D3D11_LocalSwapchainUpdateOutput;
        sys->swapCb              = D3D11_LocalSwapchainSwap;
        sys->startEndRenderingCb = D3D11_LocalSwapchainStartEndRendering;
        sys->sendMetadataCb      = D3D11_LocalSwapchainSetMetadata;
        sys->selectPlaneCb       = D3D11_LocalSwapchainSelectPlane;
    }

#ifndef VLC_WINSTORE_APP
    if (vd->source->projection_mode != PROJECTION_MODE_RECTANGULAR && CommonVideoHWND(&sys->area))
        sys->p_sensors = HookWindowsSensors(vd, CommonVideoHWND(&sys->area));
#endif // !VLC_WINSTORE_APP

    if (Direct3D11Open(vd, fmtp, context)) {
        msg_Err(vd, "Direct3D11 could not be opened");
        goto error;
    }

    vlc_window_SetTitle(vd->cfg->window, VOUT_TITLE " (Direct3D11 output)");
    msg_Dbg(vd, "Direct3D11 display adapter successfully initialized");

    vd->info.can_scale_spu        = true;

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
    D3D_ReleaseShaderCompiler(sys->shaders);
#ifndef VLC_WINSTORE_APP
    UnhookWindowsSensors(sys->p_sensors);
    CommonWindowClean(&sys->area);
#endif
    Direct3D11Close(vd);
    delete sys;
}
static int Control(vout_display_t *vd, int query)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    CommonControl( vd, &sys->area, query );

    if ( sys->area.place_changed )
    {
        UpdateSize(vd);
        sys->area.place_changed =false;
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

static void PreparePicture(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture,
                           vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    if (sys->picQuad.generic.textureFormat->formatTexture == DXGI_FORMAT_UNKNOWN)
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        int i;
        HRESULT hr;

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

        if (sys->legacy_shader) {
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
                sys->picQuad.generic.i_height = srcDesc.Height;
                sys->picQuad.generic.i_width  = srcDesc.Width;

                CommonPlacePicture(vd, &sys->area);
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
    if (sys->legacy_shader)
        renderSrc = sys->stagingSys.renderSrc;
    else {
        picture_sys_d3d11_t *p_sys = ActiveD3D11PictureSys(picture);
        renderSrc = p_sys->renderSrc;
    }
    D3D11_RenderQuad(sys->d3d_dev, &sys->picQuad,
                     vd->source->projection_mode == PROJECTION_MODE_RECTANGULAR ? &sys->flatVShader : &sys->projectionVShader,
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

#ifdef HAVE_D3D11_4_H
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
#endif
}

static void Prepare(vout_display_t *vd, picture_t *picture,
                    subpicture_t *subpicture, vlc_tick_t date)
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
    if (is_d3d11_opaque(i_src_chroma))
        supportFlags |= D3D11_FORMAT_SUPPORT_DECODER_OUTPUT;
    return FindD3D11Format( vd, sys->d3d_dev, i_src_chroma, DXGI_RGB_FORMAT|DXGI_YUV_FORMAT, 0, 0, 0,
                            is_d3d11_opaque(i_src_chroma) ? DXGI_CHROMA_GPU : DXGI_CHROMA_CPU, supportFlags );
}

static const d3d_format_t *GetDirectDecoderFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    UINT supportFlags = D3D11_FORMAT_SUPPORT_DECODER_OUTPUT;
    return FindD3D11Format( vd, sys->d3d_dev, i_src_chroma, DXGI_RGB_FORMAT|DXGI_YUV_FORMAT, 0, 0, 0,
                            DXGI_CHROMA_GPU, supportFlags );
}

static const d3d_format_t *GetDisplayFormatByDepth(vout_display_t *vd, uint8_t bit_depth,
                                                   uint8_t widthDenominator,
                                                   uint8_t heightDenominator,
                                                   bool from_processor,
                                                   int rgb_yuv)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    if (from_processor)
        supportFlags |= D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
    return FindD3D11Format( vd, sys->d3d_dev, 0, rgb_yuv,
                            bit_depth, widthDenominator+1, heightDenominator+1,
                            DXGI_CHROMA_CPU, supportFlags );
}

static const d3d_format_t *GetBlendableFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_BLENDABLE;
    return FindD3D11Format( vd, sys->d3d_dev, i_src_chroma, DXGI_RGB_FORMAT|DXGI_YUV_FORMAT, 0, 0, 0, DXGI_CHROMA_CPU, supportFlags );
}

static int Direct3D11Open(vout_display_t *vd, video_format_t *fmtp, vlc_video_context *vctx)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    video_format_t fmt;
    video_format_Copy(&fmt, vd->source);
    int err = SetupOutputFormat(vd, &fmt, vctx);
    if (err != VLC_SUCCESS)
    {
        if (!is_d3d11_opaque(vd->source->i_chroma)
#ifndef VLC_WINSTORE_APP
            && vd->obj.force
#endif
                )
        {
            const vlc_fourcc_t *list = vlc_fourcc_IsYUV(vd->source->i_chroma) ?
                        vlc_fourcc_GetYUVFallback(vd->source->i_chroma) :
                        vlc_fourcc_GetRGBFallback(vd->source->i_chroma);
            for (unsigned i = 0; list[i] != 0; i++) {
                fmt.i_chroma = list[i];
                if (fmt.i_chroma == vd->source->i_chroma)
                    continue;
                err = SetupOutputFormat(vd, &fmt, NULL);
                if (err == VLC_SUCCESS)
                    break;
            }
        }
        if (err != VLC_SUCCESS)
        {
            if ( sys->swapCb == D3D11_LocalSwapchainSwap )
                D3D11_LocalSwapchainCleanupDevice( sys->outside_opaque );
            return err;
        }
    }

    /* adjust the decoder sizes to have proper padding */
    sys->picQuad.generic.i_width  = fmt.i_width;
    sys->picQuad.generic.i_height = fmt.i_height;
    if ( sys->picQuad.generic.textureFormat->formatTexture != DXGI_FORMAT_R8G8B8A8_UNORM &&
         sys->picQuad.generic.textureFormat->formatTexture != DXGI_FORMAT_B5G6R5_UNORM )
    {
        sys->picQuad.generic.i_width  = (sys->picQuad.generic.i_width  + 0x01) & ~0x01;
        sys->picQuad.generic.i_height = (sys->picQuad.generic.i_height + 0x01) & ~0x01;
    }

    CommonPlacePicture(vd, &sys->area);

    err = UpdateDisplayFormat(vd, &fmt);
    if (err != VLC_SUCCESS) {
        msg_Err(vd, "Could not update the backbuffer");
        return err;
    }

    if (Direct3D11CreateGenericResources(vd)) {
        msg_Err(vd, "Failed to allocate resources");
        if ( sys->swapCb == D3D11_LocalSwapchainSwap )
            D3D11_LocalSwapchainCleanupDevice( sys->outside_opaque );
        return VLC_EGENERIC;
    }

    video_format_Clean(fmtp);
    *fmtp = fmt;

    sys->log_level = var_InheritInteger(vd, "verbose");

    return VLC_SUCCESS;
}

static int SetupOutputFormat(vout_display_t *vd, video_format_t *fmt, vlc_video_context *vctx)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    d3d11_video_context_t *vtcx_sys = GetD3D11ContextPrivate(vctx);
    if (vtcx_sys != NULL &&
        D3D11_DeviceSupportsFormat( sys->d3d_dev, vtcx_sys->format, D3D11_FORMAT_SUPPORT_SHADER_LOAD ))
    {
        sys->picQuad.generic.textureFormat = D3D11_RenderFormat(vtcx_sys->format ,true);
    }

    // look for the requested pixel format first
    if ( !sys->picQuad.generic.textureFormat )
        sys->picQuad.generic.textureFormat = GetDirectRenderingFormat(vd, fmt->i_chroma);

    // look for any pixel format that we can handle with enough pixels per channel
    const d3d_format_t *decoder_format = nullptr;
    if ( !sys->picQuad.generic.textureFormat )
    {
        uint8_t bits_per_channel;
        uint8_t widthDenominator, heightDenominator;
        switch (fmt->i_chroma)
        {
        case VLC_CODEC_D3D11_OPAQUE:
        case VLC_CODEC_NVDEC_OPAQUE:
            bits_per_channel = 8;
            widthDenominator = heightDenominator = 2;
            break;
        case VLC_CODEC_D3D11_OPAQUE_RGBA:
        case VLC_CODEC_D3D11_OPAQUE_BGRA:
            bits_per_channel = 8;
            widthDenominator = heightDenominator = 1;
            break;
        case VLC_CODEC_D3D11_OPAQUE_10B:
        case VLC_CODEC_NVDEC_OPAQUE_10B:
            bits_per_channel = 10;
            widthDenominator = heightDenominator = 2;
            break;
        case VLC_CODEC_NVDEC_OPAQUE_16B:
            bits_per_channel = 16;
            widthDenominator = heightDenominator = 2;
            break;
        case VLC_CODEC_NVDEC_OPAQUE_444:
            bits_per_channel = 8;
            widthDenominator = heightDenominator = 1;
            break;
        case VLC_CODEC_NVDEC_OPAQUE_444_16B:
            bits_per_channel = 16;
            widthDenominator = heightDenominator = 1;
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

        bool is_rgb = !vlc_fourcc_IsYUV(fmt->i_chroma);
        sys->picQuad.generic.textureFormat = GetDisplayFormatByDepth(vd, bits_per_channel,
                                                             widthDenominator, heightDenominator,
                                                             decoder_format!=nullptr,
                                                             is_rgb ? DXGI_RGB_FORMAT : DXGI_YUV_FORMAT);
        if (!sys->picQuad.generic.textureFormat)
            sys->picQuad.generic.textureFormat = GetDisplayFormatByDepth(vd, bits_per_channel,
                                                                 widthDenominator, heightDenominator,
                                                                 decoder_format!=nullptr,
                                                                 is_rgb ? DXGI_YUV_FORMAT : DXGI_RGB_FORMAT);
    }

    // look for any pixel format that we can handle
    if ( !sys->picQuad.generic.textureFormat )
        sys->picQuad.generic.textureFormat = GetDisplayFormatByDepth(vd, 0, 0, 0, false, DXGI_YUV_FORMAT|DXGI_RGB_FORMAT);

    if ( !sys->picQuad.generic.textureFormat )
    {
       msg_Err(vd, "Could not get a suitable texture pixel format");
       return VLC_EGENERIC;
    }

    msg_Dbg( vd, "Using pixel format %s for chroma %4.4s", sys->picQuad.generic.textureFormat->name,
                 (char *)&fmt->i_chroma );

    fmt->i_chroma = decoder_format ? decoder_format->fourcc : sys->picQuad.generic.textureFormat->fourcc;
    DxgiFormatMask( sys->picQuad.generic.textureFormat->formatTexture, fmt );

    /* check the region pixel format */
    sys->regionQuad.generic.textureFormat = GetBlendableFormat(vd, VLC_CODEC_RGBA);
    if (!sys->regionQuad.generic.textureFormat)
        sys->regionQuad.generic.textureFormat = GetBlendableFormat(vd, VLC_CODEC_BGRA);

    return VLC_SUCCESS;
}

static void Direct3D11Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    Direct3D11DestroyResources(vd);

    if ( sys->swapCb == D3D11_LocalSwapchainSwap )
        D3D11_LocalSwapchainCleanupDevice( sys->outside_opaque );

    if (sys->d3d_dev && sys->d3d_dev == &sys->local_d3d_dev->d3d_dev)
        D3D11_ReleaseDevice( sys->local_d3d_dev );

    msg_Dbg(vd, "Direct3D11 display adapter closed");
}

static bool CanUseTextureArray(vout_display_t *vd)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

#ifndef HAVE_ID3D11VIDEODECODER
    (void) vd;
    return false;
#else
    // 15.200.1062.1004 is wrong - 2015/08/03 - 15.7.1 WHQL
    // 21.19.144.1281 is wrong   -
    // 22.19.165.3 is good       - 2017/05/04 - ReLive Edition 17.5.1
    const auto WDDM_os = []{
        struct wddm_version wddm = {};
        wddm.wddm         = 21;  // starting with drivers designed for W10 Anniversary Update
        return wddm;
    }();
    if (D3D11CheckDriverVersion(sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM_os) != VLC_SUCCESS)
    {
        msg_Dbg(vd, "AMD driver too old, fallback to legacy shader mode");
        return false;
    }

    // xx.xx.1000.xxx drivers can't happen here for WDDM > 2.0
    const auto WDDM_build = []{
        struct wddm_version wddm = {};
        wddm.revision      = 162;
        return wddm;
    }();
    if (D3D11CheckDriverVersion(sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM_build) != VLC_SUCCESS)
    {
        msg_Dbg(vd, "Bogus AMD driver detected, fallback to legacy shader mode");
        return false;
    }

    return true;
#endif
}

static bool BogusZeroCopy(const vout_display_t *vd)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);

    if (sys->d3d_dev->adapterDesc.VendorId != GPU_MANUFACTURER_AMD)
        return false;

    switch (sys->d3d_dev->adapterDesc.DeviceId)
    {
    case 0x687F: // RX Vega 56/64
    case 0x6863: // RX Vega Frontier Edition
    case 0x15DD: // RX Vega 8/11 (Ryzen iGPU)
    {
        const auto WDDM = []{
            struct wddm_version wddm = {};
            wddm.revision     = 14011; // 18.10.2 - 2018/06/11
            return wddm;
        }();
        return D3D11CheckDriverVersion(sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM) != VLC_SUCCESS;
    }
    default:
        return false;
    }
}

/* TODO : handle errors better
   TODO : separate out into smaller functions like createshaders */
static int Direct3D11CreateFormatResources(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    HRESULT hr;

    sys->legacy_shader = sys->d3d_dev->feature_level < D3D_FEATURE_LEVEL_10_0 || !CanUseTextureArray(vd) ||
            BogusZeroCopy(vd) || !is_d3d11_opaque(fmt->i_chroma);

    d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET] = { };
    hr = D3D11_CompilePixelShaderBlob(vd, sys->shaders, sys->d3d_dev,
                                  &sys->display, fmt->transfer,
                                  fmt->color_range == COLOR_RANGE_FULL,
                                  &sys->picQuad, pPSBlob);
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to compile the pixel shader. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    hr = D3D11_SetQuadPixelShader(VLC_OBJECT(vd), sys->d3d_dev, false,
                                  &sys->picQuad, pPSBlob);
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to set the pixel shader. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    if (D3D11_AllocateQuad(vd, sys->d3d_dev, vd->source->projection_mode, &sys->picQuad) != VLC_SUCCESS)
    {
        msg_Err(vd, "Could not allocate quad buffers.");
       return VLC_EGENERIC;
    }

    if (D3D11_SetupQuad( vd, sys->d3d_dev, vd->source, &sys->picQuad, &sys->display) != VLC_SUCCESS)
    {
        msg_Err(vd, "Could not Create the main quad picture.");
        return VLC_EGENERIC;
    }

    RECT source_rect;
    source_rect.left   = fmt->i_x_offset;
    source_rect.right  = fmt->i_x_offset + fmt->i_visible_width;
    source_rect.top    = fmt->i_y_offset;
    source_rect.bottom = fmt->i_y_offset + fmt->i_visible_height;
    if (!D3D11_UpdateQuadPosition(vd, sys->d3d_dev, &sys->picQuad, &source_rect,
                                  video_format_GetTransform(vd->source->orientation, sys->display.orientation)))
    {
        msg_Err(vd, "Could not set quad picture position.");
        return VLC_EGENERIC;
    }

    if ( vd->source->projection_mode == PROJECTION_MODE_EQUIRECTANGULAR ||
         vd->source->projection_mode == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD )
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

#ifdef HAVE_D3D11_4_H
    hr = D3D11_InitFence(*sys->d3d_dev, sys->fence);
    if (SUCCEEDED(hr))
    {
        msg_Dbg(vd, "using GPU render fence");
    }
#endif

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
        hr = D3D11_CompilePixelShaderBlob(vd, sys->shaders, sys->d3d_dev,
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
    hr = D3D11_CompileVertexShaderBlob(VLC_OBJECT(vd), sys->shaders, sys->d3d_dev, true, &VSBlob);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to compile the flat vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    hr = D3D11_CreateVertexShader(vd, &VSBlob, sys->d3d_dev, &sys->flatVShader);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the vertex input layout. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }


    hr = D3D11_CompileVertexShaderBlob(VLC_OBJECT(vd), sys->shaders, sys->d3d_dev, false, &VSBlob);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to compile the 360 vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }
    hr = D3D11_CreateVertexShader(vd, &VSBlob, sys->d3d_dev, &sys->projectionVShader);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the projection vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    RECT rect_dst;
    rect_dst.left   = sys->area.place.x;
    rect_dst.right  = sys->area.place.x + sys->area.place.width;
    rect_dst.top    = sys->area.place.y;
    rect_dst.bottom = sys->area.place.y + sys->area.place.height;

    sys->picQuad.UpdateViewport( &rect_dst, sys->display.pixelFormat );

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

    sys->picQuad.Reset();
    Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
    sys->d3dregion_count = 0;
    sys->regionQuad.Reset();

    ReleaseD3D11PictureSys(&sys->stagingSys);

    D3D11_ReleaseVertexShader(&sys->flatVShader);
    D3D11_ReleaseVertexShader(&sys->projectionVShader);

#ifdef HAVE_D3D11_4_H
    D3D11_ReleaseFence(sys->fence);
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
    d3d11_quad_t *quad = static_cast<d3d11_quad_t *>(p_picture->p_sys);
    delete quad;
}

static int Direct3D11MapSubpicture(vout_display_t *vd, int *subpicture_region_count,
                                   picture_t ***region, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t *>(vd->sys);
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    D3D11_TEXTURE2D_DESC texDesc;
    HRESULT hr;
    int err;

    if (sys->regionQuad.generic.textureFormat == NULL)
        return VLC_EGENERIC;

    int count = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
        count++;

    *region = static_cast<picture_t**>(calloc(count, sizeof(picture_t *)));
    if (unlikely(*region==NULL))
        return VLC_ENOMEM;
    *subpicture_region_count = count;

    int i = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next, i++) {
        if (!r->fmt.i_visible_width || !r->fmt.i_visible_height)
            continue; // won't render anything, keep the cache for later

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
            quad = static_cast<d3d11_quad_t*>(quad_picture->p_sys);
        else
        {
            d3d11_quad_t *d3dquad = new (std::nothrow) d3d11_quad_t;
            if (unlikely(d3dquad==NULL)) {
                continue;
            }
            quad = d3dquad;
            if (AllocateTextures(vd, sys->d3d_dev, sys->regionQuad.generic.textureFormat, &r->p_picture->format,
                                 false, d3dquad->picSys.texture, NULL)) {
                msg_Err(vd, "Failed to allocate %dx%d texture for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                for (int j=0; j<DXGI_MAX_SHADER_VIEW; j++)
                    if (d3dquad->picSys.texture[j])
                        d3dquad->picSys.texture[j]->Release();
                delete d3dquad;
                continue;
            }

            if (D3D11_AllocateResourceView(vlc_object_logger(vd), sys->d3d_dev->d3ddevice, sys->regionQuad.generic.textureFormat,
                                           d3dquad->picSys.texture, 0,
                                           d3dquad->picSys.renderSrc)) {
                msg_Err(vd, "Failed to create %dx%d shader view for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                delete d3dquad;
                continue;
            }
            d3dquad->generic.i_width  = r->fmt.i_width;
            d3dquad->generic.i_height = r->fmt.i_height;

            d3dquad->generic.textureFormat = sys->regionQuad.generic.textureFormat;
            err = D3D11_AllocateQuad(vd, sys->d3d_dev, PROJECTION_MODE_RECTANGULAR, d3dquad);
            if (err != VLC_SUCCESS)
            {
                msg_Err(vd, "Failed to allocate %dx%d quad for OSD",
                             r->fmt.i_visible_width, r->fmt.i_visible_height);
                delete d3dquad;
                continue;
            }

            err = D3D11_SetupQuad( vd, sys->d3d_dev, &r->fmt, d3dquad, &sys->display );
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to setup %dx%d quad for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                delete d3dquad;
                continue;
            }
            const auto picres = [](picture_sys_d3d11_t * p_sys){
                picture_resource_t res = {};
                res.p_sys      = p_sys;
                res.pf_destroy = DestroyPictureQuad;
                return res;
            }((picture_sys_d3d11_t *) d3dquad);
            (*region)[i] = picture_NewFromResource(&r->p_picture->format, &picres);
            if ((*region)[i] == NULL) {
                msg_Err(vd, "Failed to create %dx%d picture for OSD",
                        r->fmt.i_width, r->fmt.i_height);
                d3dquad->Reset();
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
            err = picture_UpdatePlanes(quad_picture, static_cast<uint8_t*>(mappedResource.pData), mappedResource.RowPitch);
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to set the buffer on the SPU picture" );
                sys->d3d_dev->d3dcontext->Unmap(quad->picSys.resource[KNOWN_DXGI_INDEX], 0);
                picture_Release(quad_picture);
                if ((*region)[i] == quad_picture)
                    (*region)[i] = NULL;
                continue;
            }

            picture_CopyPixels(quad_picture, r->p_picture);

            sys->d3d_dev->d3dcontext->Unmap(quad->picSys.resource[KNOWN_DXGI_INDEX], 0);
        } else {
            msg_Err(vd, "Failed to map the SPU texture (hr=0x%lX)", hr );
            picture_Release(quad_picture);
            if ((*region)[i] == quad_picture)
                (*region)[i] = NULL;
            continue;
        }

        RECT output;
        output.left   = r->fmt.i_x_offset;
        output.right  = r->fmt.i_x_offset + r->fmt.i_visible_width;
        output.top    = r->fmt.i_y_offset;
        output.bottom = r->fmt.i_y_offset + r->fmt.i_visible_height;

        D3D11_UpdateQuadPosition(vd, sys->d3d_dev, quad, &output,
            video_format_GetTransform(ORIENT_NORMAL, sys->display.orientation));

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

        quad->UpdateViewport( &spuViewport, sys->display.pixelFormat );

        D3D11_UpdateQuadOpacity(vd, sys->d3d_dev, quad, r->i_alpha / 255.0f );
    }
    return VLC_SUCCESS;
}
