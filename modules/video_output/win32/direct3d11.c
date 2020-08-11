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

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601 // _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0601 // _WIN32_WINNT_WIN7
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#include <assert.h>
#include <math.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d11.h>

/* avoided until we can pass ISwapchainPanel without c++/cx mode
# include <windows.ui.xaml.media.dxinterop.h> */

#include "../../video_chroma/d3d11_fmt.h"

#include "d3d11_quad.h"
#include "d3d11_shaders.h"
#include "d3d11_swapchain.h"

#include "common.h"
#include "../../video_chroma/copy.h"

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
    add_integer("winrt-swapchain",     0x0, NULL, NULL, true) /* IDXGISwapChain1*     */
#endif

    add_shortcut("direct3d11")
    set_callback_display(Open, 300)
vlc_module_end ()

struct vout_display_sys_t
{
    vout_display_sys_win32_t sys;       /* only use if sys.event is not NULL */
    display_win32_area_t     area;

    /* Sensors */
    void *p_sensors;

    display_info_t           display;

    d3d11_device_t           *d3d_dev;
    d3d11_decoder_device_t   *local_d3d_dev; // when opened without a video context
    d3d11_shaders_t          shaders;
    d3d_quad_t               picQuad;

    ID3D11Query              *prepareWait;

    picture_sys_d3d11_t      stagingSys;
    plane_t                  stagingPlanes[PICTURE_PLANE_MAX];

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
    libvlc_video_update_output_cb            updateOutputCb;
    libvlc_video_swap_cb                     swapCb;
    libvlc_video_makeCurrent_cb              startEndRenderingCb;
    libvlc_video_frameMetadata_cb            sendMetadataCb;
    libvlc_video_output_select_plane_cb      selectPlaneCb;
};

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

static void SetQuadVSProjection(vout_display_t *, d3d_quad_t *, const vlc_viewpoint_t *);
static void UpdatePicQuadPosition(vout_display_t *);

static int Control(vout_display_t *, int, va_list);


static int UpdateDisplayFormat(vout_display_t *vd, libvlc_video_output_cfg_t *out,
                               const video_format_t *input_fmt)
{
    vout_display_sys_t *sys = vd->sys;
    display_info_t new_display = { 0 };

    for (const d3d_format_t *output_format = GetRenderFormatList();
         output_format->name != NULL; ++output_format)
    {
        if (output_format->formatTexture == (DXGI_FORMAT)out->dxgi_format &&
            !is_d3d11_opaque(output_format->fourcc))
        {
            new_display.pixelFormat = output_format;
            break;
        }
    }
    if (unlikely(new_display.pixelFormat == NULL))
    {
        msg_Err(vd, "Could not find the output format.");
        return VLC_EGENERIC;
    }

    new_display.color     = (video_color_space_t)     out->colorspace;
    new_display.transfer  = (video_transfer_func_t)   out->transfer;
    new_display.primaries = (video_color_primaries_t) out->primaries;
    new_display.b_full_range = out->full_range;

    /* guestimate the display peak luminance */
    switch (out->transfer)
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
           sys->display.b_full_range   != new_display.b_full_range ))
    {
        sys->display = new_display;
        /* TODO release the pixel shaders if the format changed */
        if (Direct3D11CreateFormatResources(vd, input_fmt)) {
            msg_Err(vd, "Failed to allocate format resources");
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static int QueryDisplayFormat(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    libvlc_video_render_cfg_t cfg;

    cfg.width  = sys->area.vdcfg.display.width;
    cfg.height = sys->area.vdcfg.display.height;

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

    return UpdateDisplayFormat(vd, &out, fmt);
}

static void UpdateSize(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    msg_Dbg(vd, "Detected size change %dx%d", sys->area.place.width,
            sys->area.place.height);

    QueryDisplayFormat(vd, &vd->fmt);

    d3d11_device_lock( sys->d3d_dev );

    UpdatePicQuadPosition(vd);

    RECT source_rect = {
        .left   = vd->source.i_x_offset,
        .right  = vd->source.i_x_offset + vd->source.i_visible_width,
        .top    = vd->source.i_y_offset,
        .bottom = vd->source.i_y_offset + vd->source.i_visible_height,
    };
    D3D11_UpdateQuadPosition(vd, sys->d3d_dev, &sys->picQuad, &source_rect,
                             vd->source.orientation);

    d3d11_device_unlock( sys->d3d_dev );
}

static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys = vd->sys = vlc_obj_calloc(VLC_OBJECT(vd), 1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    int ret = D3D11_InitShaders(VLC_OBJECT(vd), &sys->shaders);
    if (ret != VLC_SUCCESS)
        goto error;

    CommonInit(vd, &sys->area, cfg);

    sys->outside_opaque = var_InheritAddress( vd, "vout-cb-opaque" );
    sys->updateOutputCb      = var_InheritAddress( vd, "vout-cb-update-output" );
    sys->swapCb              = var_InheritAddress( vd, "vout-cb-swap" );
    sys->startEndRenderingCb = var_InheritAddress( vd, "vout-cb-make-current" );
    sys->sendMetadataCb      = var_InheritAddress( vd, "vout-cb-metadata" );
    sys->selectPlaneCb       = var_InheritAddress( vd, "vout-cb-select-plane" );

    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueContext( context );
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
#if !VLC_WINSTORE_APP
        if (cfg->window->type == VOUT_WINDOW_TYPE_HWND)
        {
            if (CommonWindowInit(vd, &sys->area, &sys->sys,
                       vd->source.projection_mode != PROJECTION_MODE_RECTANGULAR))
                goto error;
        }

#endif /* !VLC_WINSTORE_APP */

        /* use our internal swapchain callbacks */
#ifdef HAVE_DCOMP_H
        if (cfg->window->type == VOUT_WINDOW_TYPE_DCOMP)
            sys->outside_opaque      = CreateLocalSwapchainHandleDComp(VLC_OBJECT(vd), cfg->window->display.dcomp_device, cfg->window->handle.dcomp_visual, sys->d3d_dev);
        else
#endif
            sys->outside_opaque      = CreateLocalSwapchainHandleHwnd(VLC_OBJECT(vd), sys->sys.hvideownd, sys->d3d_dev);
        if (unlikely(sys->outside_opaque == NULL))
            goto error;
        sys->updateOutputCb      = LocalSwapchainUpdateOutput;
        sys->swapCb              = LocalSwapchainSwap;
        sys->startEndRenderingCb = LocalSwapchainStartEndRendering;
        sys->sendMetadataCb      = LocalSwapchainSetMetadata;
        sys->selectPlaneCb       = LocalSwapchainSelectPlane;
    }

#if !VLC_WINSTORE_APP
    if (vd->source.projection_mode != PROJECTION_MODE_RECTANGULAR && sys->sys.hvideownd)
        sys->p_sensors = HookWindowsSensors(vd, sys->sys.hvideownd);
#endif // !VLC_WINSTORE_APP

    if (Direct3D11Open(vd, fmtp, context)) {
        msg_Err(vd, "Direct3D11 could not be opened");
        goto error;
    }

    vout_window_SetTitle(sys->area.vdcfg.window, VOUT_TITLE " (Direct3D11 output)");
    msg_Dbg(vd, "Direct3D11 display adapter successfully initialized");

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

    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->close = Close;

    msg_Dbg(vd, "Direct3D11 Open Succeeded");

    return VLC_SUCCESS;

error:
    Close(vd);
    return VLC_EGENERIC;
}

static void Close(vout_display_t *vd)
{
    D3D11_ReleaseShaders(&vd->sys->shaders);
    Direct3D11Close(vd);
#if !VLC_WINSTORE_APP
    UnhookWindowsSensors(vd->sys->p_sensors);
    CommonWindowClean(VLC_OBJECT(vd), &vd->sys->sys);
#endif
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
    hr = ID3D11DeviceContext_Map(sys->d3d_dev->d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        VS_PROJECTION_CONST *dst_data = mapped.pData;
        getZoomMatrix(SPHERE_RADIUS * f_z, dst_data->Zoom);
        getProjectionMatrix(f_sar, f_fovy, dst_data->Projection);

        vlc_viewpoint_t vp;
        vlc_viewpoint_reverse(&vp, p_vp);
        vlc_viewpoint_to_4x4(&vp, dst_data->View);
    }
    ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, (ID3D11Resource *)quad->pVertexShaderConstants, 0);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;
    int res = CommonControl( vd, &sys->area, &sys->sys, query, args );

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

static bool SelectRenderPlane(void *opaque, size_t plane)
{
    vout_display_sys_t *sys = opaque;
    return sys->selectPlaneCb(sys->outside_opaque, plane);
}

static void PreparePicture(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    d3d11_device_lock( sys->d3d_dev );

    if (sys->picQuad.textureFormat->formatTexture == DXGI_FORMAT_UNKNOWN)
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        int i;
        HRESULT hr;

        bool b_mapped = true;
        for (i = 0; i < picture->i_planes; i++) {
            hr = ID3D11DeviceContext_Map(sys->d3d_dev->d3dcontext, sys->stagingSys.resource[i],
                                         0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            if( unlikely(FAILED(hr)) )
            {
                while (i-- > 0)
                    ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, sys->stagingSys.resource[i], 0);
                b_mapped = false;
                break;
            }
            sys->stagingPlanes[i].i_pitch = mappedResource.RowPitch;
            sys->stagingPlanes[i].p_pixels = mappedResource.pData;
        }

        if (b_mapped)
        {
            for (i = 0; i < picture->i_planes; i++)
                plane_CopyPixels(&sys->stagingPlanes[i], &picture->p[i]);

            for (i = 0; i < picture->i_planes; i++)
                ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, sys->stagingSys.resource[i], 0);
        }
    }
    else if (!is_d3d11_opaque(picture->format.i_chroma))
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr;

        hr = ID3D11DeviceContext_Map(sys->d3d_dev->d3dcontext, sys->stagingSys.resource[0],
                                        0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if( unlikely(FAILED(hr)) )
            msg_Err(vd, "Failed to map the %4.4s staging picture. (hr=0x%lX)", (const char*)&picture->format.i_chroma, hr);
        else
        {
            uint8_t *buf = mappedResource.pData;
            for (int i = 0; i < picture->i_planes; i++)
            {
                sys->stagingPlanes[i].i_pitch = mappedResource.RowPitch;
                sys->stagingPlanes[i].p_pixels = buf;

                plane_CopyPixels(&sys->stagingPlanes[i], &picture->p[i]);

                buf += sys->stagingPlanes[i].i_pitch * sys->stagingPlanes[i].i_lines;
            }

            ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, sys->stagingSys.resource[0], 0);
        }
    }
    else
    {
        picture_sys_d3d11_t *p_sys = ActiveD3D11PictureSys(picture);

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
            ID3D11DeviceContext_CopySubresourceRegion(sys->d3d_dev->d3dcontext,
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
                sys->picQuad.i_height = texDesc.Height;
                sys->picQuad.i_width = texDesc.Width;

                CommonPlacePicture(vd, &sys->area, &sys->sys);
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
        D3D11_UpdateQuadLuminanceScale(vd, sys->d3d_dev, &sys->picQuad, (float)sys->display.luminance_peak / GetFormatLuminance(VLC_OBJECT(vd), &picture->format));
    }

    /* Render the quad */
    ID3D11ShaderResourceView **renderSrc;
    if (!is_d3d11_opaque(picture->format.i_chroma) || sys->legacy_shader)
        renderSrc = sys->stagingSys.renderSrc;
    else {
        picture_sys_d3d11_t *p_sys = ActiveD3D11PictureSys(picture);
        renderSrc = p_sys->renderSrc;
    }
    D3D11_RenderQuad(sys->d3d_dev, &sys->picQuad,
                     vd->source.projection_mode == PROJECTION_MODE_RECTANGULAR ? &sys->flatVShader : &sys->projectionVShader,
                     renderSrc, SelectRenderPlane, sys);

    if (subpicture) {
        // draw the additional vertices
        for (int i = 0; i < sys->d3dregion_count; ++i) {
            if (sys->d3dregions[i])
            {
                d3d_quad_t *quad = (d3d_quad_t *) sys->d3dregions[i]->p_sys;
                D3D11_RenderQuad(sys->d3d_dev, quad, &sys->flatVShader,
                                 quad->picSys.renderSrc, SelectRenderPlane, sys);
            }
        }
    }

    if (sys->prepareWait)
    {
        int maxWait = 10;
        ID3D11DeviceContext_End(sys->d3d_dev->d3dcontext, (ID3D11Asynchronous*)sys->prepareWait);

        while (S_FALSE == ID3D11DeviceContext_GetData(sys->d3d_dev->d3dcontext,
                                                      (ID3D11Asynchronous*)sys->prepareWait, NULL, 0, 0)
               && --maxWait)
            SleepEx(2, TRUE);
    }

    d3d11_device_unlock( sys->d3d_dev );
}

static void Prepare(vout_display_t *vd, picture_t *picture,
                    subpicture_t *subpicture, vlc_tick_t date)
{
    vout_display_sys_t *sys = vd->sys;

    VLC_UNUSED(date);

    d3d11_device_lock( sys->d3d_dev );
#if VLC_WINSTORE_APP
    if ( sys->swapCb == LocalSwapchainSwap )
    {
        /* legacy UWP mode, the width/height was set in GUID_SWAPCHAIN_WIDTH/HEIGHT */
        uint32_t i_width;
        uint32_t i_height;
        if (LocalSwapchainWinstoreSize( sys->outside_opaque, &i_width, &i_height ))
        {
            if (i_width != sys->area.vdcfg.display.width || i_height != sys->area.vdcfg.display.height)
                vout_display_SetSize(vd, i_width, i_height);
        }
    }
#endif
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

        PreparePicture(vd, picture, subpicture);

        sys->startEndRenderingCb( sys->outside_opaque, false );
    }
    d3d11_device_unlock( sys->d3d_dev );
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(picture);

    d3d11_device_lock( sys->d3d_dev );
    sys->swapCb(sys->outside_opaque);
    d3d11_device_unlock( sys->d3d_dev );
}

static const d3d_format_t *GetDirectRenderingFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    if (is_d3d11_opaque(i_src_chroma))
        supportFlags |= D3D11_FORMAT_SUPPORT_DECODER_OUTPUT;
    return FindD3D11Format( vd, vd->sys->d3d_dev, i_src_chroma, D3D11_RGB_FORMAT|D3D11_YUV_FORMAT, 0, 0, 0,
                            is_d3d11_opaque(i_src_chroma) ? D3D11_CHROMA_GPU : D3D11_CHROMA_CPU, supportFlags );
}

static const d3d_format_t *GetDirectDecoderFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_DECODER_OUTPUT;
    return FindD3D11Format( vd, vd->sys->d3d_dev, i_src_chroma, D3D11_RGB_FORMAT|D3D11_YUV_FORMAT, 0, 0, 0,
                            D3D11_CHROMA_GPU, supportFlags );
}

static const d3d_format_t *GetDisplayFormatByDepth(vout_display_t *vd, uint8_t bit_depth,
                                                   uint8_t widthDenominator,
                                                   uint8_t heightDenominator,
                                                   bool from_processor,
                                                   int rgb_yuv)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD;
    if (from_processor)
        supportFlags |= D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
    return FindD3D11Format( vd, vd->sys->d3d_dev, 0, rgb_yuv,
                            bit_depth, widthDenominator+1, heightDenominator+1,
                            D3D11_CHROMA_CPU, supportFlags );
}

static const d3d_format_t *GetBlendableFormat(vout_display_t *vd, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_BLENDABLE;
    return FindD3D11Format( vd, vd->sys->d3d_dev, i_src_chroma, D3D11_RGB_FORMAT|D3D11_YUV_FORMAT, 0, 0, 0, D3D11_CHROMA_CPU, supportFlags );
}

static int Direct3D11Open(vout_display_t *vd, video_format_t *fmtp, vlc_video_context *vctx)
{
    vout_display_sys_t *sys = vd->sys;
    video_format_t fmt;
    video_format_Copy(&fmt, &vd->source);
    int err = SetupOutputFormat(vd, &fmt, vctx);
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
                err = SetupOutputFormat(vd, &fmt, NULL);
                if (err == VLC_SUCCESS)
                    break;
            }
        }
        if (err != VLC_SUCCESS)
        {
            if ( sys->swapCb == LocalSwapchainSwap )
                LocalSwapchainCleanupDevice( sys->outside_opaque );
            return err;
        }
    }

    /* adjust the decoder sizes to have proper padding */
    sys->picQuad.i_width  = fmt.i_width;
    sys->picQuad.i_height = fmt.i_height;
    if ( sys->picQuad.textureFormat->formatTexture != DXGI_FORMAT_R8G8B8A8_UNORM &&
         sys->picQuad.textureFormat->formatTexture != DXGI_FORMAT_B5G6R5_UNORM )
    {
        sys->picQuad.i_width  = (sys->picQuad.i_width  + 0x01) & ~0x01;
        sys->picQuad.i_height = (sys->picQuad.i_height + 0x01) & ~0x01;
    }

    CommonPlacePicture(vd, &sys->area, &sys->sys);

    err = QueryDisplayFormat(vd, &fmt);
    if (err != VLC_SUCCESS) {
        msg_Err(vd, "Could not update the backbuffer");
        return err;
    }

    if (Direct3D11CreateGenericResources(vd)) {
        msg_Err(vd, "Failed to allocate resources");
        if ( sys->swapCb == LocalSwapchainSwap )
            LocalSwapchainCleanupDevice( sys->outside_opaque );
        return VLC_EGENERIC;
    }

    video_format_Clean(fmtp);
    *fmtp = fmt;

    return VLC_SUCCESS;
}

static int SetupOutputFormat(vout_display_t *vd, video_format_t *fmt, vlc_video_context *vctx)
{
    vout_display_sys_t *sys = vd->sys;

    d3d11_video_context_t *vtcx_sys = GetD3D11ContextPrivate(vctx);
    if (vtcx_sys != NULL &&
        DeviceSupportsFormat( vd->sys->d3d_dev->d3ddevice, vtcx_sys->format, D3D11_FORMAT_SUPPORT_SHADER_LOAD ))
    {
        for (const d3d_format_t *output_format = GetRenderFormatList();
            output_format->name != NULL; ++output_format)
        {
            if (output_format->formatTexture == vtcx_sys->format &&
                    is_d3d11_opaque(output_format->fourcc))
            {
                sys->picQuad.textureFormat = output_format;
                break;
            }
        }
    }

    // look for the requested pixel format first
    if ( !sys->picQuad.textureFormat )
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
        else
            decoder_format = sys->picQuad.textureFormat;

        bool is_rgb = !vlc_fourcc_IsYUV(fmt->i_chroma);
        sys->picQuad.textureFormat = GetDisplayFormatByDepth(vd, bits_per_channel,
                                                             widthDenominator, heightDenominator,
                                                             decoder_format!=NULL,
                                                             is_rgb ? D3D11_RGB_FORMAT : D3D11_YUV_FORMAT);
        if (!sys->picQuad.textureFormat)
            sys->picQuad.textureFormat = GetDisplayFormatByDepth(vd, bits_per_channel,
                                                                 widthDenominator, heightDenominator,
                                                                 decoder_format!=NULL,
                                                                 is_rgb ? D3D11_YUV_FORMAT : D3D11_RGB_FORMAT);
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

    return VLC_SUCCESS;
}

static void Direct3D11Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D11DestroyResources(vd);

    if ( sys->swapCb == LocalSwapchainSwap )
        LocalSwapchainCleanupDevice( sys->outside_opaque );

    if (sys->d3d_dev && sys->d3d_dev == &sys->local_d3d_dev->d3d_dev)
        D3D11_ReleaseDevice( sys->local_d3d_dev );

    msg_Dbg(vd, "Direct3D11 display adapter closed");
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
    // 15.200.1062.1004 is wrong - 2015/08/03 - 15.7.1 WHQL
    // 21.19.144.1281 is wrong   -
    // 22.19.165.3 is good       - 2017/05/04 - ReLive Edition 17.5.1
    struct wddm_version WDDM_os = {
        .wddm         = 21,  // starting with drivers designed for W10 Anniversary Update
    };
    if (D3D11CheckDriverVersion(vd->sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM_os) != VLC_SUCCESS)
    {
        msg_Dbg(vd, "AMD driver too old, fallback to legacy shader mode");
        return false;
    }

    // xx.xx.1000.xxx drivers can't happen here for WDDM > 2.0
    struct wddm_version WDDM_build = {
        .revision     = 162,
    };
    if (D3D11CheckDriverVersion(vd->sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM_build) != VLC_SUCCESS)
    {
        msg_Dbg(vd, "Bogus AMD driver detected, fallback to legacy shader mode");
        return false;
    }

    return true;
#endif
}

static bool BogusZeroCopy(const vout_display_t *vd)
{
    if (vd->sys->d3d_dev->adapterDesc.VendorId != GPU_MANUFACTURER_AMD)
        return false;

    switch (vd->sys->d3d_dev->adapterDesc.DeviceId)
    {
    case 0x687F: // RX Vega 56/64
    case 0x6863: // RX Vega Frontier Edition
    case 0x15DD: // RX Vega 8/11 (Ryzen iGPU)
    {
        struct wddm_version WDDM = {
            .revision     = 14011, // 18.10.2 - 2018/06/11
        };
        return D3D11CheckDriverVersion(vd->sys->d3d_dev, GPU_MANUFACTURER_AMD, &WDDM) != VLC_SUCCESS;
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

    sys->legacy_shader = sys->d3d_dev->feature_level < D3D_FEATURE_LEVEL_10_0 || !CanUseTextureArray(vd) ||
            BogusZeroCopy(vd);

    hr = D3D11_CompilePixelShader(vd, &sys->shaders, sys->legacy_shader, sys->d3d_dev,
                                  &sys->display, fmt->transfer, fmt->primaries,
                                  fmt->color_range == COLOR_RANGE_FULL,
                                  &sys->picQuad);
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to create the pixel shader. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

    if (D3D11_AllocateQuad(vd, sys->d3d_dev, vd->source.projection_mode, &sys->picQuad) != VLC_SUCCESS)
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
    if (D3D11_SetupQuad( vd, sys->d3d_dev, &vd->source, &sys->picQuad, &sys->display,
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
        hr = ID3D11Device_QueryInterface( sys->d3d_dev->d3ddevice, &IID_ID3D10Multithread, (void **)&pMultithread);
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
        video_format_t texture_fmt = vd->source;
        texture_fmt.i_width  = sys->picQuad.i_width;
        texture_fmt.i_height = sys->picQuad.i_height;
        if (!is_d3d11_opaque(fmt->i_chroma))
            texture_fmt.i_chroma = sys->picQuad.textureFormat->fourcc;

        if (AllocateTextures(vd, sys->d3d_dev, sys->picQuad.textureFormat, &texture_fmt, textures, sys->stagingPlanes))
        {
            msg_Err(vd, "Failed to allocate the staging texture");
            return VLC_EGENERIC;
        }

        if (D3D11_AllocateResourceView(vd, sys->d3d_dev->d3ddevice, sys->picQuad.textureFormat,
                                     textures, 0, sys->stagingSys.renderSrc))
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

static int Direct3D11CreateGenericResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    D3D11_QUERY_DESC query = { 0 };
    query.Query = D3D11_QUERY_EVENT;
    hr = ID3D11Device_CreateQuery(sys->d3d_dev->d3ddevice, &query, &sys->prepareWait);

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

    hr = ID3D11Device_CreateBlendState(sys->d3d_dev->d3ddevice, &spuBlendDesc, &pSpuBlendState);
    if (FAILED(hr)) {
       msg_Err(vd, "Could not create SPU blend state. (hr=0x%lX)", hr);
       return VLC_EGENERIC;
    }
    ID3D11DeviceContext_OMSetBlendState(sys->d3d_dev->d3dcontext, pSpuBlendState, NULL, 0xFFFFFFFF);
    ID3D11BlendState_Release(pSpuBlendState);

    /* disable depth testing as we're only doing 2D
     * see https://msdn.microsoft.com/en-us/library/windows/desktop/bb205074%28v=vs.85%29.aspx
     * see http://rastertek.com/dx11tut11.html
    */
    D3D11_DEPTH_STENCIL_DESC stencilDesc;
    ZeroMemory(&stencilDesc, sizeof(stencilDesc));

    ID3D11DepthStencilState *pDepthStencilState;
    hr = ID3D11Device_CreateDepthStencilState(sys->d3d_dev->d3ddevice, &stencilDesc, &pDepthStencilState );
    if (SUCCEEDED(hr)) {
        ID3D11DeviceContext_OMSetDepthStencilState(sys->d3d_dev->d3dcontext, pDepthStencilState, 0);
        ID3D11DepthStencilState_Release(pDepthStencilState);
    }

    if (sys->regionQuad.textureFormat != NULL)
    {
        hr = D3D11_CompilePixelShader(vd, &sys->shaders, sys->legacy_shader, sys->d3d_dev,
                                      &sys->display, TRANSFER_FUNC_SRGB, COLOR_PRIMARIES_SRGB, true,
                                      &sys->regionQuad);
        if (FAILED(hr))
        {
            D3D11_ReleasePixelShader(&sys->picQuad);
            msg_Err(vd, "Failed to create the SPU pixel shader. (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    hr = D3D11_CompileFlatVertexShader(vd, &sys->shaders, sys->d3d_dev, &sys->flatVShader);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the vertex input layout. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    hr = D3D11_CompileProjectionVertexShader(vd, &sys->shaders, sys->d3d_dev, &sys->projectionVShader);
    if(FAILED(hr)) {
      msg_Err(vd, "Failed to create the projection vertex shader. (hr=0x%lX)", hr);
      return VLC_EGENERIC;
    }

    UpdatePicQuadPosition(vd);

    msg_Dbg(vd, "Direct3D11 resources created");
    return VLC_SUCCESS;
}

static void Direct3D11DestroyResources(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    D3D11_ReleaseQuad(&sys->picQuad);
    Direct3D11DeleteRegions(sys->d3dregion_count, sys->d3dregions);
    sys->d3dregion_count = 0;
    D3D11_ReleaseQuad(&sys->regionQuad);

    ReleaseD3D11PictureSys(&sys->stagingSys);

    D3D11_ReleaseVertexShader(&sys->flatVShader);
    D3D11_ReleaseVertexShader(&sys->projectionVShader);

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
            if (AllocateTextures(vd, sys->d3d_dev, sys->regionQuad.textureFormat, &r->p_picture->format, d3dquad->picSys.texture, NULL)) {
                msg_Err(vd, "Failed to allocate %dx%d texture for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                for (int j=0; j<D3D11_MAX_SHADER_VIEW; j++)
                    if (d3dquad->picSys.texture[j])
                        ID3D11Texture2D_Release(d3dquad->picSys.texture[j]);
                free(d3dquad);
                continue;
            }

            if (D3D11_AllocateResourceView(vd, sys->d3d_dev->d3ddevice, sys->regionQuad.textureFormat,
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
            err = D3D11_AllocateQuad(vd, sys->d3d_dev, PROJECTION_MODE_RECTANGULAR, d3dquad);
            if (err != VLC_SUCCESS)
            {
                msg_Err(vd, "Failed to allocate %dx%d quad for OSD",
                             r->fmt.i_visible_width, r->fmt.i_visible_height);
                free(d3dquad);
                continue;
            }

            err = D3D11_SetupQuad( vd, sys->d3d_dev, &r->fmt, d3dquad, &sys->display, &output,
                                   ORIENT_NORMAL );
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to setup %dx%d quad for OSD",
                        r->fmt.i_visible_width, r->fmt.i_visible_height);
                free(d3dquad);
                continue;
            }
            picture_resource_t picres = {
                .p_sys      = (picture_sys_d3d11_t *) d3dquad,
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
            D3D11_UpdateQuadPosition(vd, sys->d3d_dev, (d3d_quad_t *) quad_picture->p_sys, &output, ORIENT_NORMAL);
        }

        hr = ID3D11DeviceContext_Map(sys->d3d_dev->d3dcontext, ((d3d_quad_t *) quad_picture->p_sys)->picSys.resource[KNOWN_DXGI_INDEX], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if( SUCCEEDED(hr) ) {
            err = picture_UpdatePlanes(quad_picture, mappedResource.pData, mappedResource.RowPitch);
            if (err != VLC_SUCCESS) {
                msg_Err(vd, "Failed to set the buffer on the SPU picture" );
                ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, ((d3d_quad_t *) quad_picture->p_sys)->picSys.resource[KNOWN_DXGI_INDEX], 0);
                picture_Release(quad_picture);
                if ((*region)[i] == quad_picture)
                    (*region)[i] = NULL;
                continue;
            }

            picture_CopyPixels(quad_picture, r->p_picture);

            ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, ((d3d_quad_t *) quad_picture->p_sys)->picSys.resource[KNOWN_DXGI_INDEX], 0);
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

        D3D11_UpdateQuadOpacity(vd, sys->d3d_dev, quad, r->i_alpha / 255.0f );
    }
    return VLC_SUCCESS;
}

