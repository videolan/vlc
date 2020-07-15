/*****************************************************************************
 * direct3d9.c: Windows Direct3D9 video output module
 *****************************************************************************
 * Copyright (C) 2006-2014 VLC authors and VideoLAN
 *
 * Authors: Martell Malone <martellmalone@gmail.com>,
 *          Damien Fouilleul <damienf@videolan.org>,
 *          Sasha Koruga <skoruga@gmail.com>,
 *          Felix Abecassis <felix.abecassis@gmail.com>
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

/*****************************************************************************
 * Preamble:
 *
 * This plugin will use YUV surface if supported, using YUV will result in
 * the best video quality (hardware filtering when rescaling the picture)
 * and the fastest display as it requires less processing.
 *
 * If YUV overlay is not supported this plugin will use RGB offscreen video
 * surfaces that will be blitted onto the primary surface (display) to
 * effectively display the pictures.
 *
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_fs.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#include <windows.h>
#include <d3d9.h>
#ifdef HAVE_D3DX9EFFECT_H
#include <d3dx9effect.h>
#endif
#include "../../video_chroma/d3d9_fmt.h"
#include <dxvahd.h>

#include "common.h"
#include "builtin_shaders.h"
#include "../../video_chroma/copy.h"

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open(vout_display_t *, const vout_display_cfg_t *,
                 video_format_t *, vlc_video_context *);
static void Close(vout_display_t *);

#define DESKTOP_LONGTEXT N_(\
    "The desktop mode allows you to display the video on the desktop.")

#define HW_BLENDING_TEXT N_("Use hardware blending support")
#define HW_BLENDING_LONGTEXT N_(\
    "Try to use hardware acceleration for subtitle/OSD blending.")
#define HW_YUV_TEXT N_("Use hardware YUV->RGB conversions")
#define HW_YUV_LONGTEXT N_(\
    "Try to use hardware acceleration for YUV->RGB conversions. " \
    "This option doesn't have any effect when using overlays.")

#define PIXEL_SHADER_TEXT N_("Pixel Shader")
#define PIXEL_SHADER_LONGTEXT N_(\
        "Choose a pixel shader to apply.")
#define PIXEL_SHADER_FILE_TEXT N_("Path to HLSL file")
#define PIXEL_SHADER_FILE_LONGTEXT N_("Path to an HLSL file containing a single pixel shader.")
/* The latest option in the selection list: used for loading a shader file. */
#define SELECTED_SHADER_FILE N_("HLSL File")

#define D3D9_HELP N_("Recommended video output for Windows Vista and later versions")

vlc_module_begin ()
    set_shortname("Direct3D9")
    set_description(N_("Direct3D9 video output"))
    set_help(D3D9_HELP)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_bool("direct3d9-hw-blending", true, HW_BLENDING_TEXT, HW_BLENDING_LONGTEXT, true)
    add_bool("directx-hw-yuv", true, HW_YUV_TEXT, HW_YUV_LONGTEXT, true)

    add_string("direct3d9-shader", "", PIXEL_SHADER_TEXT, PIXEL_SHADER_LONGTEXT, true)
    add_loadfile("direct3d9-shader-file", NULL,
                 PIXEL_SHADER_FILE_TEXT, PIXEL_SHADER_FILE_LONGTEXT)

    add_bool("direct3d9-dxvahd", true, DXVAHD_TEXT, DXVAHD_LONGTEXT, true)

    add_shortcut("direct3d9", "direct3d")
    set_callback_display(Open, 280)
vlc_module_end ()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static const vlc_fourcc_t d3d_subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

typedef struct
{
    const char   *name;
    D3DFORMAT    format;    /* D3D format */
    vlc_fourcc_t fourcc;    /* VLC fourcc */
    uint32_t     rmask;
    uint32_t     gmask;
    uint32_t     bmask;
} d3d9_format_t;

struct vout_display_sys_t
{
    vout_display_sys_win32_t sys;       /* only use if sys.event is not NULL */
    display_win32_area_t     area;

    bool allow_hw_yuv;    /* Should we use hardware YUV->RGB conversions */

    // core objects
    d3d9_decoder_device_t  *d3d9_device;
    vlc_decoder_device     *dec_device; // if d3d9_decoder comes from a decoder device

    D3DFORMAT               BufferFormat;

    HINSTANCE               hxdll;      /* handle of the opened d3d9x dll */
    IDirect3DPixelShader9*  d3dx_shader;

    // scene objects
    IDirect3DTexture9       *sceneTexture;
    IDirect3DVertexBuffer9  *sceneVertexBuffer;
    D3DFORMAT               d3dregion_format;    /* Backbuffer output format */
    size_t                  d3dregion_count;    /* for subpictures */
    struct d3d_region_t     *d3dregion;         /* for subpictures */

    const d3d9_format_t     *sw_texture_fmt;  /* Rendering texture(s) format */
    IDirect3DSurface9       *dx_render;

    /* */
    bool                    reset_device;
    bool                    lost_not_ready;
    bool                    clear_scene;

    /* outside rendering */
    void *outside_opaque;
    libvlc_video_update_output_cb            updateOutputCb;
    libvlc_video_swap_cb                     swapCb;
    libvlc_video_makeCurrent_cb              startEndRenderingCb;

    /* range converter */
    struct {
        HMODULE                 dll;
        IDXVAHD_VideoProcessor *proc;
    } processor;
};

/* */
typedef struct
{
    FLOAT       x,y,z;      // vertex untransformed position
    FLOAT       rhw;        // eye distance
    FLOAT       tu, tv;     // texture relative coordinates
} CUSTOMVERTEX;
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_TEX1)

typedef struct d3d_region_t {
    D3DFORMAT          format; // for subpictures
    unsigned           width;  // for pixel shaders
    unsigned           height; // for pixel shaders
    CUSTOMVERTEX       vertex[4];
    IDirect3DTexture9  *texture;
} d3d_region_t;

/* */
static HINSTANCE Direct3D9LoadShaderLibrary(void)
{
    HINSTANCE instance = NULL;
    for (int i = 43; i > 23; --i) {
        WCHAR filename[16];
        _snwprintf(filename, ARRAY_SIZE(filename), TEXT("D3dx9_%d.dll"), i);
        instance = LoadLibrary(filename);
        if (instance)
            break;
    }
    return instance;
}

/**
 * Compute the vertex ordering needed to rotate the video. Without
 * rotation, the vertices of the rectangle are defined in a clockwise
 * order. This function computes a remapping of the coordinates to
 * implement the rotation, given fixed texture coordinates.
 * The unrotated order is the following:
 * 0--1
 * |  |
 * 3--2
 * For a 180 degrees rotation it should like this:
 * 2--3
 * |  |
 * 1--0
 * Vertex 0 should be assigned coordinates at index 2 from the
 * unrotated order and so on, thus yielding order: 2 3 0 1.
 */
static void orientationVertexOrder(video_orientation_t orientation, int vertex_order[static 4])
{
    switch (orientation) {
        case ORIENT_ROTATED_90:      /* ORIENT_RIGHT_TOP */
            vertex_order[0] = 1;
            vertex_order[1] = 2;
            vertex_order[2] = 3;
            vertex_order[3] = 0;
            break;
        case ORIENT_ROTATED_270:     /* ORIENT_LEFT_BOTTOM */
            vertex_order[0] = 3;
            vertex_order[1] = 0;
            vertex_order[2] = 1;
            vertex_order[3] = 2;
            break;
        case ORIENT_ROTATED_180:     /* ORIENT_BOTTOM_RIGHT */
            vertex_order[0] = 2;
            vertex_order[1] = 3;
            vertex_order[2] = 0;
            vertex_order[3] = 1;
            break;
        case ORIENT_TRANSPOSED:      /* ORIENT_LEFT_TOP */
            vertex_order[0] = 0;
            vertex_order[1] = 3;
            vertex_order[2] = 2;
            vertex_order[3] = 1;
            break;
        case ORIENT_HFLIPPED:        /* ORIENT_TOP_RIGHT */
            vertex_order[0] = 1;
            vertex_order[1] = 0;
            vertex_order[2] = 3;
            vertex_order[3] = 2;
            break;
        case ORIENT_VFLIPPED:        /* ORIENT_BOTTOM_LEFT */
            vertex_order[0] = 3;
            vertex_order[1] = 2;
            vertex_order[2] = 1;
            vertex_order[3] = 0;
            break;
        case ORIENT_ANTI_TRANSPOSED: /* ORIENT_RIGHT_BOTTOM */
            vertex_order[0] = 2;
            vertex_order[1] = 1;
            vertex_order[2] = 0;
            vertex_order[3] = 3;
            break;
       default:
            vertex_order[0] = 0;
            vertex_order[1] = 1;
            vertex_order[2] = 2;
            vertex_order[3] = 3;
            break;
    }
}

static void  Direct3D9SetupVertices(CUSTOMVERTEX *vertices,
                                  const RECT *full_texture, const RECT *visible_texture,
                                  const RECT *rect_in_display,
                                  int alpha,
                                  video_orientation_t orientation)
{
    /* Vertices of the dst rectangle in the unrotated (clockwise) order. */
    const int vertices_coords[4][2] = {
        { rect_in_display->left,  rect_in_display->top    },
        { rect_in_display->right, rect_in_display->top    },
        { rect_in_display->right, rect_in_display->bottom },
        { rect_in_display->left,  rect_in_display->bottom },
    };

    /* Compute index remapping necessary to implement the rotation. */
    int vertex_order[4];
    orientationVertexOrder(orientation, vertex_order);

    for (int i = 0; i < 4; ++i) {
        vertices[i].x  = vertices_coords[vertex_order[i]][0];
        vertices[i].y  = vertices_coords[vertex_order[i]][1];
    }

    float texture_right  = (float)visible_texture->right / (float)full_texture->right;
    float texture_left   = (float)visible_texture->left  / (float)full_texture->right;
    float texture_top    = (float)visible_texture->top    / (float)full_texture->bottom;
    float texture_bottom = (float)visible_texture->bottom / (float)full_texture->bottom;

    vertices[0].tu = texture_left;
    vertices[0].tv = texture_top;

    vertices[1].tu = texture_right;
    vertices[1].tv = texture_top;

    vertices[2].tu = texture_right;
    vertices[2].tv = texture_bottom;

    vertices[3].tu = texture_left;
    vertices[3].tv = texture_bottom;

    for (int i = 0; i < 4; i++) {
        /* -0.5f is a "feature" of DirectX and it seems to apply to Direct3d also */
        /* http://www.sjbrown.co.uk/2003/05/01/fix-directx-rasterisation/ */
        vertices[i].x -= 0.5;
        vertices[i].y -= 0.5;

        vertices[i].z       = 0.0f;
        vertices[i].rhw     = 1.0f;
    }
}

/**
 * It copies picture surface into a texture and setup the associated d3d_region_t.
 */
static int Direct3D9ImportPicture(vout_display_t *vd,
                                 d3d_region_t *region,
                                 IDirect3DSurface9 *source)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;

    if (!source) {
        msg_Dbg(vd, "no surface to render?");
        return VLC_EGENERIC;
    }

    /* retrieve texture top-level surface */
    IDirect3DSurface9 *destination;
    hr = IDirect3DTexture9_GetSurfaceLevel(sys->sceneTexture, 0, &destination);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DTexture9_GetSurfaceLevel: 0x%lX", hr);
        return VLC_EGENERIC;
    }

    if (sys->processor.proc)
    {
        DXVAHD_STREAM_DATA inputStream = { 0 };
        inputStream.Enable = TRUE;
        inputStream.pInputSurface = source;
        hr = IDXVAHD_VideoProcessor_VideoProcessBltHD( sys->processor.proc, destination, 0, 1, &inputStream );
        if (FAILED(hr)) {
            D3DSURFACE_DESC srcDesc, dstDesc;
            IDirect3DSurface9_GetDesc(source, &srcDesc);
            IDirect3DSurface9_GetDesc(destination, &dstDesc);

            msg_Dbg(vd, "Failed VideoProcessBltHD src:%4.4s (%d) dst:%4.4s (%d) (hr=0x%lX)",
                    (const char*)&srcDesc.Format, srcDesc.Format,
                    (const char*)&dstDesc.Format, dstDesc.Format, hr);
        }
    }
    else
    {
        /* Copy picture surface into texture surface
         * color space conversion happen here */
        RECT source_visible_rect = {
            .left   = vd->source.i_x_offset,
            .right  = vd->source.i_x_offset + vd->source.i_visible_width,
            .top    = vd->source.i_y_offset,
            .bottom = vd->source.i_y_offset + vd->source.i_visible_height,
        };
        RECT texture_visible_rect = {
            .left   = 0,
            .right  = vd->source.i_visible_width,
            .top    = 0,
            .bottom = vd->source.i_visible_height,
        };
        // On nVidia & AMD, StretchRect will fail if the visible size isn't even.
        // When copying the entire buffer, the margin end up being blended in the actual picture
        // on nVidia (regardless of even/odd dimensions)
        if (texture_visible_rect.right & 1)
        {
            texture_visible_rect.right++;
            source_visible_rect.right++;
        }
        if (texture_visible_rect.bottom & 1)
        {
            texture_visible_rect.bottom++;
            source_visible_rect.bottom++;
        }

        hr = IDirect3DDevice9_StretchRect(sys->d3d9_device->d3ddev.dev, source, &source_visible_rect,
                                        destination, &texture_visible_rect,
                                        D3DTEXF_NONE);
        if (FAILED(hr)) {
            msg_Dbg(vd, "Failed StretchRect: source 0x%p. (hr=0x%lX)",
                    (LPVOID)source, hr);
        }
    }
    IDirect3DSurface9_Release(destination);
    if (FAILED(hr))
        return VLC_EGENERIC;

    /* */
    region->texture = sys->sceneTexture;
    RECT texture_rect = {
        .left   = 0,
        .right  = vd->source.i_width,
        .top    = 0,
        .bottom = vd->source.i_height,
    };
    RECT rect_in_display = {
        .left   = sys->area.place.x,
        .right  = sys->area.place.x + sys->area.place.width,
        .top    = sys->area.place.y,
        .bottom = sys->area.place.y + sys->area.place.height,
    };
    RECT texture_visible_rect = {
        .left   = 0,
        .right  = vd->source.i_visible_width,
        .top    = 0,
        .bottom = vd->source.i_visible_height,
    };
    Direct3D9SetupVertices(region->vertex, &texture_rect, &texture_visible_rect,
                           &rect_in_display, 255, vd->source.orientation);
    return VLC_SUCCESS;
}

static void Direct3D9DeleteRegions(size_t count, d3d_region_t *region)
{
    for (size_t i = 0; i < count; i++) {
        if (region[i].texture)
            IDirect3DTexture9_Release(region[i].texture);
    }
    free(region);
}

/**
 * It releases the scene resources.
 */
static void Direct3D9DestroyScene(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D9DeleteRegions(sys->d3dregion_count, sys->d3dregion);
    sys->d3dregion_count = 0;
    sys->d3dregion       = NULL;

    if (sys->sceneVertexBuffer)
    {
        IDirect3DVertexBuffer9_Release(sys->sceneVertexBuffer);
        sys->sceneVertexBuffer = NULL;
    }

    if (sys->sceneTexture)
    {
        IDirect3DTexture9_Release(sys->sceneTexture);
        sys->sceneTexture = NULL;
    }

    msg_Dbg(vd, "Direct3D9 scene released successfully");
}

static void Direct3D9DestroyShaders(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->d3dx_shader)
    {
        IDirect3DPixelShader9_Release(sys->d3dx_shader);
        sys->d3dx_shader = NULL;
    }
}

/**
 * It destroys the picture and scene resources.
 */
static void Direct3D9DestroyResources(vout_display_t *vd)
{
    Direct3D9DestroyScene(vd);
    if (vd->sys->dx_render)
    {
        IDirect3DSurface9_Release(vd->sys->dx_render);
        vd->sys->dx_render = NULL;
    }
    Direct3D9DestroyShaders(vd);
}

static int UpdateOutput(vout_display_t *vd, const video_format_t *fmt,
                        libvlc_video_output_cfg_t *out)
{
    vout_display_sys_t *sys = vd->sys;
    libvlc_video_render_cfg_t cfg;
    cfg.width  = sys->area.vdcfg.display.width;
    cfg.height = sys->area.vdcfg.display.height;

    switch (fmt->i_chroma)
    {
    case VLC_CODEC_D3D9_OPAQUE:
        cfg.bitdepth = 8;
        break;
    case VLC_CODEC_D3D9_OPAQUE_10B:
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

    cfg.full_range = fmt->color_range == COLOR_RANGE_FULL;
    cfg.primaries  = fmt->primaries;
    cfg.colorspace = fmt->space;
    cfg.transfer   = fmt->transfer;

    cfg.device = sys->d3d9_device->d3ddev.dev;

    libvlc_video_output_cfg_t local_out;
    if (out == NULL)
        out = &local_out;
    if (!sys->updateOutputCb( sys->outside_opaque, &cfg, out ))
    {
        msg_Err(vd, "Failed to set the external render size");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/**
 * It allocates and initializes the resources needed to render the scene.
 */
static int Direct3D9CreateScene(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    IDirect3DDevice9        *d3ddev = sys->d3d9_device->d3ddev.dev;
    HRESULT hr;

    UINT width  = fmt->i_visible_width;
    UINT height = fmt->i_visible_height;
    // On nVidia & AMD, StretchRect will fail if the visible size isn't even.
    // When copying the entire buffer, the margin end up being blended in the actual picture
    // on nVidia (regardless of even/odd dimensions)
    if (height & 1) height++;
    if (width  & 1) width++;

    /*
     * Create a texture for use when rendering a scene
     * for performance reason, texture format is identical to backbuffer
     * which would usually be a RGB format
     */
    hr = IDirect3DDevice9_CreateTexture(d3ddev,
                                        width,
                                        height,
                                        1,
                                        D3DUSAGE_RENDERTARGET,
                                        sys->BufferFormat,
                                        D3DPOOL_DEFAULT,
                                        &sys->sceneTexture,
                                        NULL);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to create texture. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }

#ifndef NDEBUG
    msg_Dbg(vd, "Direct3D created texture: %ix%i",
                fmt->i_width, fmt->i_height);
#endif

    /*
    ** Create a vertex buffer for use when rendering scene
    */
    hr = IDirect3DDevice9_CreateVertexBuffer(d3ddev,
                                             sizeof(CUSTOMVERTEX)*4,
                                             D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                             D3DFVF_CUSTOMVERTEX,
                                             D3DPOOL_DEFAULT,
                                             &sys->sceneVertexBuffer,
                                             NULL);
    if (FAILED(hr)) {
        msg_Err(vd, "Failed to create vertex buffer. (hr=0x%lX)", hr);
        IDirect3DTexture9_Release(sys->sceneTexture);
        sys->sceneTexture = NULL;
        return VLC_EGENERIC;
    }

    // we use FVF instead of vertex shader
    hr = IDirect3DDevice9_SetFVF(d3ddev, D3DFVF_CUSTOMVERTEX);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed SetFVF: 0x%lX", hr);
        return -1;
    }

    /* */
    sys->d3dregion_count = 0;
    sys->d3dregion       = NULL;

    sys->clear_scene = true;

    // Texture coordinates outside the range [0.0, 1.0] are set
    // to the texture color at 0.0 or 1.0, respectively.
    IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    // Set linear filtering quality
    if (sys->d3d9_device->d3ddev.caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) {
        //msg_Dbg(vd, "Using D3DTEXF_LINEAR for minification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    } else {
        //msg_Dbg(vd, "Using D3DTEXF_POINT for minification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    }
    if (sys->d3d9_device->d3ddev.caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR) {
        //msg_Dbg(vd, "Using D3DTEXF_LINEAR for magnification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    } else {
        //msg_Dbg(vd, "Using D3DTEXF_POINT for magnification");
        IDirect3DDevice9_SetSamplerState(d3ddev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    }

    // set maximum ambient light
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_AMBIENT, D3DCOLOR_XRGB(255,255,255));

    // Turn off culling
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_CULLMODE, D3DCULL_NONE);

    // Turn off the zbuffer
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ZENABLE, D3DZB_FALSE);

    // Turn off lights
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_LIGHTING, FALSE);

    // Enable dithering
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_DITHERENABLE, TRUE);

    // disable stencil
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_STENCILENABLE, FALSE);

    // manage blending
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

    if (sys->d3d9_device->d3ddev.caps.AlphaCmpCaps & D3DPCMPCAPS_GREATER) {
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHATESTENABLE,TRUE);
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHAREF, 0x00);
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHAFUNC,D3DCMP_GREATER);
    }

    // Set texture states
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_COLORARG1,D3DTA_TEXTURE);

    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(d3ddev, 0, D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);

    msg_Dbg(vd, "Direct3D9 scene created successfully");

    return VLC_SUCCESS;
}

#ifdef HAVE_D3DX9EFFECT_H
static int Direct3D9CompileShader(vout_display_t *vd, const char *shader_source, size_t source_length)
{
    vout_display_sys_t *sys = vd->sys;

    HRESULT (WINAPI * OurD3DXCompileShader)(
            LPCSTR pSrcData,
            UINT srcDataLen,
            const D3DXMACRO *pDefines,
            LPD3DXINCLUDE pInclude,
            LPCSTR pFunctionName,
            LPCSTR pProfile,
            DWORD Flags,
            LPD3DXBUFFER *ppShader,
            LPD3DXBUFFER *ppErrorMsgs,
            LPD3DXCONSTANTTABLE *ppConstantTable);

    OurD3DXCompileShader = (void*)GetProcAddress(sys->hxdll, "D3DXCompileShader");
    if (!OurD3DXCompileShader) {
        msg_Warn(vd, "Cannot locate reference to D3DXCompileShader; pixel shading will be disabled");
        return VLC_EGENERIC;
    }

    LPD3DXBUFFER error_msgs = NULL;
    LPD3DXBUFFER compiled_shader = NULL;

    DWORD shader_flags = 0;
    HRESULT hr = OurD3DXCompileShader(shader_source, source_length, NULL, NULL,
                "main", "ps_3_0", shader_flags, &compiled_shader, &error_msgs, NULL);

    if (FAILED(hr)) {
        msg_Warn(vd, "D3DXCompileShader Error (hr=0x%lX)", hr);
        if (error_msgs) {
            msg_Warn(vd, "HLSL Compilation Error: %s", (char*)ID3DXBuffer_GetBufferPointer(error_msgs));
            ID3DXBuffer_Release(error_msgs);
    }
        return VLC_EGENERIC;
    }

    hr = IDirect3DDevice9_CreatePixelShader(sys->d3d9_device->d3ddev.dev,
            ID3DXBuffer_GetBufferPointer(compiled_shader),
            &sys->d3dx_shader);

    if (compiled_shader)
        ID3DXBuffer_Release(compiled_shader);
    if (error_msgs)
        ID3DXBuffer_Release(error_msgs);

    if (FAILED(hr)) {
        msg_Warn(vd, "IDirect3DDevice9_CreatePixelShader error (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
#else
# define Direct3D9CompileShader(a,b,c)  VLC_EGENERIC
#endif

#define MAX_SHADER_FILE_SIZE  (1024*1024)

static int Direct3D9CreateShaders(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->hxdll)
        return VLC_EGENERIC;

    /* Find which shader was selected in the list. */
    char *selected_shader = var_InheritString(vd, "direct3d9-shader");
    if (!selected_shader)
        return VLC_SUCCESS; /* Nothing to do */

    const char *shader_source_builtin = NULL;
    char *shader_source_file = NULL;
    FILE *fs = NULL;

    for (size_t i = 0; i < BUILTIN_SHADERS_COUNT; ++i) {
        if (!strcmp(selected_shader, builtin_shaders[i].name)) {
            shader_source_builtin = builtin_shaders[i].code;
            break;
        }
    }

    if (shader_source_builtin) {
        /* A builtin shader was selected. */
        int err = Direct3D9CompileShader(vd, shader_source_builtin, strlen(shader_source_builtin));
        if (err)
            goto error;
    } else {
        if (strcmp(selected_shader, SELECTED_SHADER_FILE))
            goto error; /* Unrecognized entry in the list. */
        /* The source code of the shader needs to be read from a file. */
        char *filepath = var_InheritString(vd, "direct3d9-shader-file");
        if (!filepath || !*filepath)
        {
            free(filepath);
            goto error;
        }
        /* Open file, find its size with fseek/ftell and read its content in a buffer. */
        fs = vlc_fopen(filepath, "rb");
        if (!fs)
            goto error;
        int ret = fseek(fs, 0, SEEK_END);
        if (ret == -1)
            goto error;
        long length = ftell(fs);
        if (length == -1 || length >= MAX_SHADER_FILE_SIZE)
            goto error;
        rewind(fs);
        shader_source_file = vlc_alloc(length, sizeof(*shader_source_file));
        if (!shader_source_file)
            goto error;
        ret = fread(shader_source_file, length, 1, fs);
        if (ret != 1)
            goto error;
        ret = Direct3D9CompileShader(vd, shader_source_file, length);
        if (ret)
            goto error;
    }

    free(selected_shader);
    free(shader_source_file);
    fclose(fs);

    return VLC_SUCCESS;

error:
    Direct3D9DestroyShaders(vd);
    free(selected_shader);
    free(shader_source_file);
    if (fs)
        fclose(fs);
    return VLC_EGENERIC;
}

/**
 * It creates the picture and scene resources.
 */
static int Direct3D9CreateResources(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;

    if (Direct3D9CreateScene(vd, fmt)) {
        msg_Err(vd, "Direct3D scene initialization failed !");
        return VLC_EGENERIC;
    }
    if (Direct3D9CreateShaders(vd)) {
        /* Failing to initialize shaders is not fatal. */
        msg_Warn(vd, "Direct3D shaders initialization failed !");
    }

    sys->d3dregion_format = D3DFMT_UNKNOWN;
    for (int i = 0; i < 2; i++) {
        D3DFORMAT dfmt = i == 0 ? D3DFMT_A8B8G8R8 : D3DFMT_A8R8G8B8;
        if (SUCCEEDED(IDirect3D9_CheckDeviceFormat(sys->d3d9_device->hd3d.obj,
                                                   sys->d3d9_device->d3ddev.adapterId,
                                                   D3DDEVTYPE_HAL,
                                                   sys->BufferFormat,
                                                   D3DUSAGE_DYNAMIC,
                                                   D3DRTYPE_TEXTURE,
                                                   dfmt))) {
            sys->d3dregion_format = dfmt;
            break;
        }
    }

    if( !is_d3d9_opaque( fmt->i_chroma ) )
    {
        HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(sys->d3d9_device->d3ddev.dev,
                                                          fmt->i_width,
                                                          fmt->i_height,
                                                          sys->sw_texture_fmt->format,
                                                          D3DPOOL_DEFAULT,
                                                          &sys->dx_render,
                                                          NULL);
        if (FAILED(hr)) {
           msg_Err(vd, "Failed to allocate offscreen surface (hr=0x%lX)", hr);
           return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

/**
 * It reset the Direct3D9 device and its resources.
 */
static int Direct3D9Reset(vout_display_t *vd, const video_format_t *fmtp)
{
    vout_display_sys_t *sys = vd->sys;

    int res = D3D9_ResetDevice( VLC_OBJECT(vd), sys->d3d9_device );
    if (res != VLC_SUCCESS)
        return res;

    /* release all D3D objects */
    Direct3D9DestroyResources(vd);

    if (UpdateOutput(vd, fmtp, NULL) != VLC_SUCCESS)
        return VLC_EGENERIC;

    /* re-create them */
    if (Direct3D9CreateResources(vd, fmtp)) {
        msg_Dbg(vd, "Direct3D9CreateResources failed !");
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void Direct3D9ImportSubpicture(vout_display_t *vd,
                                     size_t *count_ptr, d3d_region_t **region,
                                     subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    size_t count = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
        count++;

    *count_ptr = count;
    *region    = calloc(count, sizeof(**region));
    if (*region == NULL) {
        *count_ptr = 0;
        return;
    }

    int i = 0;
    for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next, i++) {
        d3d_region_t *d3dr = &(*region)[i];
        HRESULT hr;

        d3dr->texture = NULL;
        for (size_t j = 0; j < sys->d3dregion_count; j++) {
            d3d_region_t *cache = &sys->d3dregion[j];
            if (cache->texture &&
                cache->format == sys->d3dregion_format &&
                cache->width  == r->fmt.i_width &&
                cache->height == r->fmt.i_height) {
                *d3dr = *cache;
                memset(cache, 0, sizeof(*cache));
                break;
            }
        }
        if (!d3dr->texture) {
            d3dr->format = sys->d3dregion_format;
            d3dr->width  = r->fmt.i_width;
            d3dr->height = r->fmt.i_height;
            hr = IDirect3DDevice9_CreateTexture(sys->d3d9_device->d3ddev.dev,
                                                d3dr->width, d3dr->height,
                                                1,
                                                D3DUSAGE_DYNAMIC,
                                                d3dr->format,
                                                D3DPOOL_DEFAULT,
                                                &d3dr->texture,
                                                NULL);
            if (FAILED(hr)) {
                d3dr->texture = NULL;
                msg_Err(vd, "Failed to create %dx%d texture for OSD (hr=0x%lX)",
                        d3dr->width, d3dr->height, hr);
                continue;
            }
#ifndef NDEBUG
            msg_Dbg(vd, "Created %dx%d texture for OSD",
                    r->fmt.i_width, r->fmt.i_height);
#endif
        }

        D3DLOCKED_RECT lock;
        hr = IDirect3DTexture9_LockRect(d3dr->texture, 0, &lock, NULL, 0);
        if (SUCCEEDED(hr)) {
            uint8_t  *dst_data   = lock.pBits;
            int       dst_pitch  = lock.Pitch;
            uint8_t  *src_data   = r->p_picture->p->p_pixels;
            int       src_pitch  = r->p_picture->p->i_pitch;

            if (d3dr->format == D3DFMT_A8B8G8R8) {
                if (dst_pitch == r->p_picture->p->i_pitch) {
                    memcpy(dst_data, src_data, r->fmt.i_height * dst_pitch);
                } else {
                    int copy_pitch = __MIN(dst_pitch, r->p_picture->p->i_pitch);
                    for (unsigned y = 0; y < r->fmt.i_height; y++) {
                        memcpy(&dst_data[y * dst_pitch], &src_data[y * src_pitch], copy_pitch);
                    }
                }
            } else {
                int copy_pitch = __MIN(dst_pitch, r->p_picture->p->i_pitch);
                for (unsigned y = 0; y < r->fmt.i_height; y++) {
                    for (int x = 0; x < copy_pitch; x += 4) {
                        dst_data[y * dst_pitch + x + 0] = src_data[y * src_pitch + x + 2];
                        dst_data[y * dst_pitch + x + 1] = src_data[y * src_pitch + x + 1];
                        dst_data[y * dst_pitch + x + 2] = src_data[y * src_pitch + x + 0];
                        dst_data[y * dst_pitch + x + 3] = src_data[y * src_pitch + x + 3];
                    }
                }
            }
            hr = IDirect3DTexture9_UnlockRect(d3dr->texture, 0);
            if (FAILED(hr))
                msg_Err(vd, "Failed to unlock the texture");
        } else {
            msg_Err(vd, "Failed to lock the texture");
        }

        /* Map the subpicture to sys->sys.sys.place */
        const float scale_w = (float)(sys->area.place.width)  / subpicture->i_original_picture_width;
        const float scale_h = (float)(sys->area.place.height) / subpicture->i_original_picture_height;

        RECT rect_in_display;
        rect_in_display.left   =            scale_w * r->i_x,
        rect_in_display.right  = rect_in_display.left + scale_w * r->fmt.i_visible_width,
        rect_in_display.top    =            scale_h * r->i_y,
        rect_in_display.bottom = rect_in_display.top  + scale_h * r->fmt.i_visible_height;

        rect_in_display.left   += sys->area.place.x;
        rect_in_display.right  += sys->area.place.x;
        rect_in_display.top    += sys->area.place.y;
        rect_in_display.bottom += sys->area.place.y;

        RECT texture_rect;
        texture_rect.left   = 0;
        texture_rect.right  = r->fmt.i_width;
        texture_rect.top    = 0;
        texture_rect.bottom = r->fmt.i_height;

        RECT texture_visible_rect;
        texture_visible_rect.left   = r->fmt.i_x_offset;
        texture_visible_rect.right  = r->fmt.i_x_offset + r->fmt.i_visible_width;
        texture_visible_rect.top    = r->fmt.i_y_offset;
        texture_visible_rect.bottom = r->fmt.i_y_offset + r->fmt.i_visible_height;

        Direct3D9SetupVertices(d3dr->vertex, &texture_rect, &texture_visible_rect,
                              &rect_in_display, subpicture->i_alpha * r->i_alpha / 255, ORIENT_NORMAL);
    }
}

static int Direct3D9RenderRegion(vout_display_t *vd,
                                const d3d_region_t *region,
                                bool use_pixel_shader)
{
    vout_display_sys_t *sys = vd->sys;

    IDirect3DDevice9 *d3ddev = vd->sys->d3d9_device->d3ddev.dev;

    HRESULT hr;

    /* Import vertices */
    void *vertex;
    hr = IDirect3DVertexBuffer9_Lock(sys->sceneVertexBuffer, 0, 0, &vertex, D3DLOCK_DISCARD);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DVertexBuffer9_Lock: 0x%lX", hr);
        return -1;
    }
    memcpy(vertex, region->vertex, sizeof(region->vertex));
    hr = IDirect3DVertexBuffer9_Unlock(sys->sceneVertexBuffer);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed IDirect3DVertexBuffer9_Unlock: 0x%lX", hr);
        return -1;
    }

    // Render the vertex buffer contents
    hr = IDirect3DDevice9_SetStreamSource(d3ddev, 0, sys->sceneVertexBuffer, 0, sizeof(CUSTOMVERTEX));
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed SetStreamSource: 0x%lX", hr);
        return -1;
    }

    // Setup our texture. Using textures introduces the texture stage states,
    // which govern how textures get blended together (in the case of multiple
    // textures) and lighting information. In this case, we are modulating
    // (blending) our texture with the diffuse color of the vertices.
    hr = IDirect3DDevice9_SetTexture(d3ddev, 0, (IDirect3DBaseTexture9*)region->texture);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed SetTexture: 0x%lX", hr);
        return -1;
    }

    if (sys->d3dx_shader) {
        if (use_pixel_shader)
        {
            hr = IDirect3DDevice9_SetPixelShader(d3ddev, sys->d3dx_shader);
            float shader_data[4] = { region->width, region->height, 0, 0 };
            hr = IDirect3DDevice9_SetPixelShaderConstantF(d3ddev, 0, shader_data, 1);
            if (FAILED(hr)) {
                msg_Dbg(vd, "Failed SetPixelShaderConstantF: 0x%lX", hr);
                return -1;
            }
        }
        else /* Disable any existing pixel shader. */
            hr = IDirect3DDevice9_SetPixelShader(d3ddev, NULL);
        if (FAILED(hr)) {
            msg_Dbg(vd, "Failed SetPixelShader: 0x%lX", hr);
            return -1;
        }
    }

    // draw rectangle
    hr = IDirect3DDevice9_DrawPrimitive(d3ddev, D3DPT_TRIANGLEFAN, 0, 2);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed DrawPrimitive: 0x%lX", hr);
        return -1;
    }
    return 0;
}

/**
 * It renders the scene.
 *
 * This function is intented for higher end 3D cards, with pixel shader support
 * and at least 64 MiB of video RAM.
 */
static void Direct3D9RenderScene(vout_display_t *vd,
                                d3d_region_t *picture,
                                size_t subpicture_count,
                                d3d_region_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    IDirect3DDevice9 *d3ddev = sys->d3d9_device->d3ddev.dev;
    HRESULT hr;

    if (sys->startEndRenderingCb && !sys->startEndRenderingCb( sys->outside_opaque, true ))
        return;

    if (sys->clear_scene) {
        /* Clear the backbuffer and the zbuffer */
        hr = IDirect3DDevice9_Clear(d3ddev, 0, NULL, D3DCLEAR_TARGET,
                                  D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        if (FAILED(hr)) {
            msg_Dbg(vd, "Failed Clear: 0x%lX", hr);
            return;
        }
        sys->clear_scene = false;
    }

    hr = IDirect3DDevice9_BeginScene(d3ddev);
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed BeginScene: 0x%lX", hr);
        return;
    }

    Direct3D9RenderRegion(vd, picture, true);

    if (subpicture_count)
    {
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, TRUE);
        for (size_t i = 0; i < subpicture_count; i++) {
            d3d_region_t *r = &subpicture[i];
            if (r->texture)
                Direct3D9RenderRegion(vd, r, false);
        }
        IDirect3DDevice9_SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, FALSE);
    }

    hr = IDirect3DDevice9_EndScene(d3ddev);
    if (FAILED(hr))
        msg_Dbg(vd, "Failed EndScene: 0x%lX", hr);

    if (sys->startEndRenderingCb)
        sys->startEndRenderingCb( sys->outside_opaque, false );
}

static void Prepare(vout_display_t *vd, picture_t *picture,
                    subpicture_t *subpicture, vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;

    /* Position Change */
    if (sys->area.place_changed) {
#if 0 /* need that when bicubic filter is available */
        RECT rect;
        UINT width, height;

        GetClientRect(p_sys->sys.hvideownd, &rect);
        width  = RECTWidth(rect);
        height = RECTHeight(rect);

        if (width != p_sys->pp.BackBufferWidth || height != p_sys->pp.BackBufferHeight)
        {
            msg_Dbg(vd, "resizing device back buffers to (%lux%lu)", width, height);
            // need to reset D3D device to resize back buffer
            if (VLC_SUCCESS != Direct3D9ResetDevice(vd, width, height))
                return VLC_EGENERIC;
        }
#endif
        UpdateOutput(vd, &vd->fmt, NULL);

        sys->clear_scene = true;
        sys->area.place_changed = false;
    }

    d3d9_device_t *p_d3d9_dev = &sys->d3d9_device->d3ddev;

    /* check if device is still available */
    HRESULT hr = IDirect3DDevice9_TestCooperativeLevel(p_d3d9_dev->dev);
    if (FAILED(hr)) {
        if (hr == D3DERR_DEVICENOTRESET && !sys->reset_device) {
            if (!is_d3d9_opaque(picture->format.i_chroma))
                vout_display_SendEventPicturesInvalid(vd);
            sys->reset_device = true;
            sys->lost_not_ready = false;
        }
        if (hr == D3DERR_DEVICELOST && !sys->lost_not_ready) {
            /* Device is lost but not yet ready for reset. */
            sys->lost_not_ready = true;
        }
        return;
    }

    /* FIXME it is a bit ugly, we need the surface to be unlocked for
     * rendering.
     *  The clean way would be to release the picture (and ensure that
     * the vout doesn't keep a reference). But because of the vout
     * wrapper, we can't */
    IDirect3DSurface9 *surface;

    if ( !is_d3d9_opaque(picture->format.i_chroma) )
    {
        D3DLOCKED_RECT d3drect;
        surface = sys->dx_render;
        HRESULT hr = IDirect3DSurface9_LockRect(surface, &d3drect, NULL, 0);
        if (unlikely(FAILED(hr))) {
            msg_Err(vd, "failed to lock surface");
            return;
        }

        picture_t fake_pic = *picture;
        picture_UpdatePlanes(&fake_pic, d3drect.pBits, d3drect.Pitch);
        picture_CopyPixels(&fake_pic, picture);
        IDirect3DSurface9_UnlockRect(surface);
    }
    else
    {
        const picture_sys_d3d9_t *picsys = ActiveD3D9PictureSys(picture);
        surface = picsys->surface;
        if (picsys->surface != surface)
        {
            D3DSURFACE_DESC srcDesc, dstDesc;
            IDirect3DSurface9_GetDesc(picsys->surface, &srcDesc);
            IDirect3DSurface9_GetDesc(surface, &dstDesc);
            if ( srcDesc.Width == dstDesc.Width && srcDesc.Height == dstDesc.Height )
                surface = picsys->surface;
            else
            {
                HRESULT hr;
                RECT visibleSource;
                visibleSource.left = 0;
                visibleSource.top = 0;
                visibleSource.right = picture->format.i_visible_width;
                visibleSource.bottom = picture->format.i_visible_height;

                hr = IDirect3DDevice9_StretchRect( p_d3d9_dev->dev, picsys->surface, &visibleSource, surface, &visibleSource, D3DTEXF_NONE);
                if (FAILED(hr)) {
                    msg_Err(vd, "Failed to copy the hw surface to the decoder surface (hr=0x%lX)", hr );
                }
            }
        }
    }

    d3d_region_t picture_region;
    if (!Direct3D9ImportPicture(vd, &picture_region, surface)) {
        picture_region.width = picture->format.i_visible_width;
        picture_region.height = picture->format.i_visible_height;
        size_t subpicture_region_count     = 0;
        d3d_region_t *subpicture_region = NULL;
        if (subpicture)
            Direct3D9ImportSubpicture(vd, &subpicture_region_count, &subpicture_region,
                                     subpicture);

        Direct3D9RenderScene(vd, &picture_region,
                            subpicture_region_count, subpicture_region);

        Direct3D9DeleteRegions(sys->d3dregion_count, sys->d3dregion);
        sys->d3dregion_count = subpicture_region_count;
        sys->d3dregion       = subpicture_region;
    }
}

static void Swap(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    const d3d9_device_t *p_d3d9_dev = &sys->d3d9_device->d3ddev;

    // Present the back buffer contents to the display
    // No stretching should happen here !
    RECT src = {
        .left   = 0,
        .right  = sys->area.vdcfg.display.width,
        .top    = 0,
        .bottom = sys->area.vdcfg.display.height
    };

    HRESULT hr;
    if (sys->d3d9_device->hd3d.use_ex) {
        hr = IDirect3DDevice9Ex_PresentEx(p_d3d9_dev->devex, &src, &src, sys->sys.hvideownd, NULL, 0);
    } else {
        hr = IDirect3DDevice9_Present(p_d3d9_dev->dev, &src, &src, sys->sys.hvideownd, NULL);
    }
    if (FAILED(hr)) {
        msg_Dbg(vd, "Failed Present: 0x%lX", hr);
    }
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->lost_not_ready)
        return;

    sys->swapCb( sys->outside_opaque );
}

/**
 * It releases an instance of Direct3D9
 */
static void Direct3D9Destroy(vout_display_sys_t *sys)
{
    if (sys->processor.proc)
    {
        IDXVAHD_VideoProcessor_Release(sys->processor.proc);
        FreeLibrary(sys->processor.dll);
    }
    if (sys->dec_device)
        vlc_decoder_device_Release(sys->dec_device);
    else if (sys->d3d9_device)
    {
        D3D9_ReleaseDevice(sys->d3d9_device);
    }
    if (sys->hxdll)
    {
        FreeLibrary(sys->hxdll);
        sys->hxdll = NULL;
    }
}

/**
 * It tests if the conversion from src to dst is supported.
 */
static int Direct3D9CheckConversion(vout_display_t *vd, D3DFORMAT src)
{
    vout_display_sys_t *sys = vd->sys;
    IDirect3D9 *d3dobj = sys->d3d9_device->hd3d.obj;
    D3DFORMAT dst = sys->BufferFormat;
    HRESULT hr;

    /* test whether device can create a surface of that format */
    hr = IDirect3D9_CheckDeviceFormat(d3dobj, sys->d3d9_device->d3ddev.adapterId,
                                      D3DDEVTYPE_HAL, dst, 0,
                                      D3DRTYPE_SURFACE, src);
    if (!SUCCEEDED(hr)) {
        if (D3DERR_NOTAVAILABLE != hr)
            msg_Err(vd, "Could not query adapter supported formats. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static const d3d9_format_t d3d_formats[] = {
    /* YV12 is always used for planar 420, the planes are then swapped in Lock() */
    { "YV12",       MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_YV12,  0,0,0 },
    { "YV12",       MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_I420,  0,0,0 },
    { "YV12",       MAKEFOURCC('Y','V','1','2'),    VLC_CODEC_J420,  0,0,0 },
    { "NV12",       MAKEFOURCC('N','V','1','2'),    VLC_CODEC_NV12,  0,0,0 },
    { "DXA9",       MAKEFOURCC('N','V','1','2'),    VLC_CODEC_D3D9_OPAQUE,  0,0,0 },
    { "DXA9_422",   MAKEFOURCC('Y','U','Y','2'),    VLC_CODEC_D3D9_OPAQUE,  0,0,0 },
    { "DXA9_444",   MAKEFOURCC('A','Y','U','V'),    VLC_CODEC_D3D9_OPAQUE,  0,0,0 },
    { "DXA9_10",    MAKEFOURCC('P','0','1','0'),    VLC_CODEC_D3D9_OPAQUE_10B, 0,0,0 },
    { "DXA9_10_422", MAKEFOURCC('Y','2','1','0'),   VLC_CODEC_D3D9_OPAQUE_10B, 0,0,0 },
    { "DXA9_10_444", MAKEFOURCC('Y','4','1','0'),   VLC_CODEC_D3D9_OPAQUE_10B, 0,0,0 },
    { "UYVY",       D3DFMT_UYVY,    VLC_CODEC_UYVY,  0,0,0 },
    { "YUY2",       D3DFMT_YUY2,    VLC_CODEC_YUYV,  0,0,0 },
    { "X8R8G8B8",   D3DFMT_X8R8G8B8,VLC_CODEC_RGB32, 0xff0000, 0x00ff00, 0x0000ff },
    { "A8R8G8B8",   D3DFMT_A8R8G8B8,VLC_CODEC_RGB32, 0xff0000, 0x00ff00, 0x0000ff },
    { "8G8B8",      D3DFMT_R8G8B8,  VLC_CODEC_RGB24, 0xff0000, 0x00ff00, 0x0000ff },
    { "R5G6B5",     D3DFMT_R5G6B5,  VLC_CODEC_RGB16, 0x1f<<11, 0x3f<<5,  0x1f<<0 },
    { "X1R5G5B5",   D3DFMT_X1R5G5B5,VLC_CODEC_RGB15, 0x1f<<10, 0x1f<<5,  0x1f<<0 },

    { NULL, 0, 0, 0,0,0}
};

static const d3d9_format_t *FindBufferFormat(vout_display_t *vd, D3DFORMAT fmt)
{
    for (unsigned j = 0; d3d_formats[j].name; j++) {
        const d3d9_format_t *format = &d3d_formats[j];

        if (format->format != fmt)
            continue;

        return format;
    }
    return NULL;
}

/**
 * It returns the format (closest to chroma) that can be converted to target */
static const d3d9_format_t *Direct3DFindFormat(vout_display_t *vd, const video_format_t *fmt, vlc_video_context *vctx)
{
    vout_display_sys_t *sys = vd->sys;
    bool hardware_scale_ok = !(fmt->i_visible_width & 1) && !(fmt->i_visible_height & 1);
    if( !hardware_scale_ok )
        msg_Warn( vd, "Disabling hardware chroma conversion due to odd dimensions" );

    for (unsigned pass = 0; pass < 2; pass++) {
        const vlc_fourcc_t *list;
        const vlc_fourcc_t dxva_chroma[] = {fmt->i_chroma, 0};
        D3DFORMAT decoder_format = D3DFMT_UNKNOWN;

        if (pass == 0 && is_d3d9_opaque(fmt->i_chroma))
        {
            d3d9_video_context_t *vctx_sys = GetD3D9ContextPrivate(vctx);
            assert(vctx_sys != NULL);
            list = dxva_chroma;
            decoder_format = vctx_sys->format;
            msg_Dbg(vd, "favor decoder format: %4.4s (%d)", (const char*)&decoder_format, decoder_format);
        }
        else if (pass == 0 && hardware_scale_ok && sys->allow_hw_yuv && vlc_fourcc_IsYUV(fmt->i_chroma))
            list = vlc_fourcc_GetYUVFallback(fmt->i_chroma);
        else if (pass == 1)
            list = vlc_fourcc_GetRGBFallback(fmt->i_chroma);
        else
            continue;

        for (unsigned i = 0; list[i] != 0; i++) {
            for (unsigned j = 0; d3d_formats[j].name; j++) {
                const d3d9_format_t *format = &d3d_formats[j];

                if (format->fourcc != list[i])
                    continue;
                if (decoder_format != D3DFMT_UNKNOWN && format->format != decoder_format)
                    continue; // not the Hardware format we prefer

                msg_Dbg(vd, "trying surface pixel format: %s", format->name);
                if (!Direct3D9CheckConversion(vd, format->format))
                    return format;
            }
        }
    }
    return NULL;
}

static void SetupProcessorInput(vout_display_t *vd, const video_format_t *fmt, const d3d9_format_t *d3dfmt)
{
    vout_display_sys_t *sys = vd->sys;
    HRESULT hr;
    DXVAHD_STREAM_STATE_D3DFORMAT_DATA d3dformat = { d3dfmt->format };
    hr = IDXVAHD_VideoProcessor_SetVideoProcessStreamState( sys->processor.proc, 0, DXVAHD_STREAM_STATE_D3DFORMAT, sizeof(d3dformat), &d3dformat );

    DXVAHD_STREAM_STATE_FRAME_FORMAT_DATA frame_format = { DXVAHD_FRAME_FORMAT_PROGRESSIVE };
    hr = IDXVAHD_VideoProcessor_SetVideoProcessStreamState( sys->processor.proc, 0, DXVAHD_STREAM_STATE_FRAME_FORMAT, sizeof(frame_format), &frame_format );

    DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE_DATA colorspace = { 0 };
    colorspace.RGB_Range = fmt->color_range == COLOR_RANGE_FULL ? 0 : 1;
    colorspace.YCbCr_xvYCC = fmt->color_range == COLOR_RANGE_FULL ? 1 : 0;
    colorspace.YCbCr_Matrix = fmt->space == COLOR_SPACE_BT601 ? 0 : 1;
    hr = IDXVAHD_VideoProcessor_SetVideoProcessStreamState( sys->processor.proc, 0, DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE, sizeof(colorspace), &colorspace );

    DXVAHD_STREAM_STATE_SOURCE_RECT_DATA srcRect;
    srcRect.Enable = TRUE;
    srcRect.SourceRect = (RECT) {
        .left   = vd->source.i_x_offset,
        .right  = vd->source.i_x_offset + vd->source.i_visible_width,
        .top    = vd->source.i_y_offset,
        .bottom = vd->source.i_y_offset + vd->source.i_visible_height,
    };;
    hr = IDXVAHD_VideoProcessor_SetVideoProcessStreamState( sys->processor.proc, 0, DXVAHD_STREAM_STATE_SOURCE_RECT, sizeof(srcRect), &srcRect );

    DXVAHD_BLT_STATE_TARGET_RECT_DATA dstRect;
    dstRect.Enable = TRUE;
    dstRect.TargetRect = (RECT) {
        .left   = 0,
        .right  = vd->source.i_visible_width,
        .top    = 0,
        .bottom = vd->source.i_visible_height,
    };
    hr = IDXVAHD_VideoProcessor_SetVideoProcessBltState( sys->processor.proc, DXVAHD_BLT_STATE_TARGET_RECT, sizeof(dstRect), &dstRect);
}

static void GetFrameRate(DXVAHD_RATIONAL *r, const video_format_t *fmt)
{
    if (fmt->i_frame_rate && fmt->i_frame_rate_base)
    {
        r->Numerator   = fmt->i_frame_rate;
        r->Denominator = fmt->i_frame_rate_base;
    }
    else
    {
        r->Numerator   = 0;
        r->Denominator = 0;
    }
}

static int InitRangeProcessor(vout_display_t *vd, const d3d9_format_t *d3dfmt,
                              const libvlc_video_output_cfg_t *render_out)
{
    vout_display_sys_t *sys = vd->sys;

    HRESULT hr;

    sys->processor.dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (unlikely(!sys->processor.dll))
    {
        msg_Err(vd, "Failed to load DXVA2.DLL");
        return VLC_EGENERIC;
    }

    D3DFORMAT *formatsList = NULL;
    DXVAHD_VPCAPS *capsList = NULL;
    IDXVAHD_Device *hd_device = NULL;

    HRESULT (WINAPI *CreateDevice)(IDirect3DDevice9Ex *,const DXVAHD_CONTENT_DESC *,DXVAHD_DEVICE_USAGE,PDXVAHDSW_Plugin,IDXVAHD_Device **);
    CreateDevice = (void *)GetProcAddress(sys->processor.dll, "DXVAHD_CreateDevice");
    if (CreateDevice == NULL)
    {
        msg_Err(vd, "Can't create HD device (not Windows 7+)");
        goto error;
    }

    DXVAHD_CONTENT_DESC desc;
    desc.InputFrameFormat = DXVAHD_FRAME_FORMAT_PROGRESSIVE;
    GetFrameRate( &desc.InputFrameRate, &vd->source );
    desc.InputWidth       = vd->source.i_visible_width;
    desc.InputHeight      = vd->source.i_visible_height;
    desc.OutputFrameRate  = desc.InputFrameRate;
    desc.OutputWidth      = vd->source.i_visible_width;
    desc.OutputHeight     = vd->source.i_visible_height;

    hr = CreateDevice(sys->d3d9_device->d3ddev.devex, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &hd_device);
    if (FAILED(hr))
    {
        msg_Dbg(vd, "Failed to create the device (error 0x%lX)", hr);
        goto error;
    }

    DXVAHD_VPDEVCAPS devcaps = { 0 };
    hr = IDXVAHD_Device_GetVideoProcessorDeviceCaps( hd_device, &devcaps );
    if (unlikely(FAILED(hr)))
    {
        msg_Err(vd, "Failed to get the device capabilities (error 0x%lX)", hr);
        goto error;
    }
    if (devcaps.VideoProcessorCount == 0)
    {
        msg_Warn(vd, "No good video processor found for range conversion");
        goto error;
    }

    formatsList = malloc(devcaps.InputFormatCount * sizeof(*formatsList));
    if (unlikely(formatsList == NULL))
    {
        msg_Dbg(vd, "Failed to allocate %u input formats", devcaps.InputFormatCount);
        goto error;
    }

    hr = IDXVAHD_Device_GetVideoProcessorInputFormats( hd_device, devcaps.InputFormatCount, formatsList);
    UINT i;
    for (i=0; i<devcaps.InputFormatCount; i++)
    {
        if (formatsList[i] == d3dfmt->format)
            break;
    }
    if (i == devcaps.InputFormatCount)
    {
        msg_Warn(vd, "Input format %s not supported for range conversion", d3dfmt->name);
        goto error;
    }

    free(formatsList);
    formatsList = malloc(devcaps.OutputFormatCount * sizeof(*formatsList));
    if (unlikely(formatsList == NULL))
    {
        msg_Dbg(vd, "Failed to allocate %u output formats", devcaps.OutputFormatCount);
        goto error;
    }

    hr = IDXVAHD_Device_GetVideoProcessorOutputFormats( hd_device, devcaps.OutputFormatCount, formatsList);
    for (i=0; i<devcaps.OutputFormatCount; i++)
    {
        if (formatsList[i] == sys->BufferFormat)
            break;
    }
    if (i == devcaps.OutputFormatCount)
    {
        msg_Warn(vd, "Output format %d not supported for range conversion", sys->BufferFormat);
        goto error;
    }

    capsList = malloc(devcaps.VideoProcessorCount * sizeof(*capsList));
    if (unlikely(capsList == NULL))
    {
        msg_Dbg(vd, "Failed to allocate %u video processors", devcaps.VideoProcessorCount);
        goto error;
    }
    hr = IDXVAHD_Device_GetVideoProcessorCaps( hd_device, devcaps.VideoProcessorCount, capsList);
    if (FAILED(hr))
    {
        msg_Dbg(vd, "Failed to get the processor caps (error 0x%lX)", hr);
        goto error;
    }

    hr = IDXVAHD_Device_CreateVideoProcessor( hd_device, &capsList->VPGuid, &sys->processor.proc );
    if (FAILED(hr))
    {
        msg_Dbg(vd, "Failed to create the processor (error 0x%lX)", hr);
        goto error;
    }
    IDXVAHD_Device_Release( hd_device );

    SetupProcessorInput(vd, &vd->source, d3dfmt);

    DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE_DATA colorspace;
    colorspace.Usage = 0; // playback
    colorspace.RGB_Range = render_out->full_range ? 0 : 1;
    colorspace.YCbCr_xvYCC = render_out->full_range ? 1 : 0;
    colorspace.YCbCr_Matrix = render_out->colorspace == libvlc_video_colorspace_BT601 ? 0 : 1;
    hr = IDXVAHD_VideoProcessor_SetVideoProcessBltState( sys->processor.proc, DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE, sizeof(colorspace), &colorspace);

    return VLC_SUCCESS;

error:
    free(capsList);
    free(formatsList);
    if (hd_device)
        IDXVAHD_Device_Release(hd_device);
    FreeLibrary(sys->processor.dll);
    return VLC_EGENERIC;
}

/**
 * It creates a Direct3D9 device and the associated resources.
 */
static int Direct3D9Open(vout_display_t *vd, video_format_t *fmt, vlc_video_context *vctx)
{
    vout_display_sys_t *sys = vd->sys;

    libvlc_video_output_cfg_t render_out;
    if (UpdateOutput(vd, &vd->source, &render_out) != VLC_SUCCESS)
        return VLC_EGENERIC;

    sys->BufferFormat = render_out.d3d9_format;
    const d3d9_format_t *dst_format = FindBufferFormat(vd, sys->BufferFormat);
    if (unlikely(!dst_format))
        msg_Warn(vd, "Unknown back buffer format 0x%X", sys->BufferFormat);

    /* Find the appropriate D3DFORMAT for the render chroma, the format will be the closest to
     * the requested chroma which is usable by the hardware in an offscreen surface, as they
     * typically support more formats than textures */
    const d3d9_format_t *d3dfmt = Direct3DFindFormat(vd, &vd->source, vctx);
    if (!d3dfmt) {
        msg_Err(vd, "unsupported source pixel format %4.4s", &vd->source.i_chroma);
        goto error;
    }
    msg_Dbg(vd, "found input surface format %s for source %4.4s", d3dfmt->name, (const char *)&vd->source.i_chroma);

    bool force_dxva_hd = var_InheritBool(vd, "direct3d9-dxvahd");
    if (force_dxva_hd || (dst_format && vd->source.color_range != COLOR_RANGE_FULL && dst_format->rmask && !d3dfmt->rmask &&
                          sys->d3d9_device->d3ddev.identifier.VendorId == GPU_MANUFACTURER_NVIDIA))
    {
        // NVIDIA bug, YUV to RGB internal conversion in StretchRect always converts from limited to limited range
        msg_Dbg(vd, "init DXVA-HD processor from %s to %s", d3dfmt->name, dst_format?dst_format->name:"unknown");
        int err = InitRangeProcessor( vd, d3dfmt, &render_out );
        if (err != VLC_SUCCESS)
            force_dxva_hd = false;
    }
    if (!force_dxva_hd)
    {
        // test whether device can perform color-conversion from that format to target format
        HRESULT hr = IDirect3D9_CheckDeviceFormatConversion(sys->d3d9_device->hd3d.obj,
                                                    sys->d3d9_device->d3ddev.adapterId,
                                                    D3DDEVTYPE_HAL,
                                                    d3dfmt->format, sys->BufferFormat);
        if (FAILED(hr))
        {
            msg_Dbg(vd, "Unsupported conversion from %s to %s", d3dfmt->name, dst_format?dst_format->name:"unknown" );
            goto error;
        }
        msg_Dbg(vd, "using StrecthRect from %s to %s", d3dfmt->name, dst_format?dst_format->name:"unknown" );
    }

    /* */
    *fmt = vd->source;
    fmt->i_chroma = d3dfmt->fourcc;
    fmt->i_rmask  = d3dfmt->rmask;
    fmt->i_gmask  = d3dfmt->gmask;
    fmt->i_bmask  = d3dfmt->bmask;
    sys->sw_texture_fmt = d3dfmt;

    if (Direct3D9CreateResources(vd, fmt)) {
        msg_Err(vd, "Failed to allocate resources");
        goto error;
    }

    /* Change the window title bar text */
    vout_window_SetTitle(sys->area.vdcfg.window, VOUT_TITLE " (Direct3D9 output)");

    msg_Dbg(vd, "Direct3D9 display adapter successfully initialized");
    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

/**
 * It releases the Direct3D9 device and its resources.
 */
static void Direct3D9Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    Direct3D9DestroyResources(vd);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
    case VOUT_DISPLAY_RESET_PICTURES:
    {
        /* FIXME what to do here in case of failure */
        if (sys->reset_device) {
            va_arg(args, const vout_display_cfg_t *);
            const video_format_t *fmt = va_arg(args, video_format_t *);
            if (Direct3D9Reset(vd, fmt)) {
                msg_Err(vd, "Failed to reset device");
                return VLC_EGENERIC;
            }
            sys->reset_device = false;
        }
        return VLC_SUCCESS;
    }
    default:
        return CommonControl(vd, &sys->area, &sys->sys, query, args);
    }
}

typedef struct
{
    char **values;
    char **descs;
    size_t count;
} enum_context_t;

static void ListShaders(enum_context_t *ctx)
{
    size_t num_shaders = BUILTIN_SHADERS_COUNT;
    ctx->values = xrealloc(ctx->values, (ctx->count + num_shaders + 1) * sizeof(char *));
    ctx->descs = xrealloc(ctx->descs, (ctx->count + num_shaders + 1) * sizeof(char *));
    for (size_t i = 0; i < num_shaders; ++i) {
        ctx->values[ctx->count] = strdup(builtin_shaders[i].name);
        ctx->descs[ctx->count] = strdup(builtin_shaders[i].name);
        ctx->count++;
    }
    ctx->values[ctx->count] = strdup(SELECTED_SHADER_FILE);
    ctx->descs[ctx->count] = strdup(SELECTED_SHADER_FILE);
    ctx->count++;
}

/* Populate the list of available shader techniques in the options */
static int FindShadersCallback(const char *name, char ***values, char ***descs)
{
    VLC_UNUSED(name);

    enum_context_t ctx = { NULL, NULL, 0 };

    ListShaders(&ctx);

    *values = ctx.values;
    *descs = ctx.descs;
    return ctx.count;

}

VLC_CONFIG_STRING_ENUM(FindShadersCallback)

static bool LocalSwapchainUpdateOutput( void *opaque, const libvlc_video_render_cfg_t *cfg, libvlc_video_output_cfg_t *out )
{
    vout_display_t *vd = opaque;
    vout_display_sys_t *sys = vd->sys;

    D3DDISPLAYMODE d3ddm;
    HRESULT hr = IDirect3D9_GetAdapterDisplayMode(sys->d3d9_device->hd3d.obj, sys->d3d9_device->d3ddev.adapterId, &d3ddm);
    if (unlikely(FAILED(hr)))
        return false;

    out->d3d9_format = d3ddm.Format;
    out->full_range  = true;
    out->colorspace  = libvlc_video_colorspace_BT709;
    out->primaries   = libvlc_video_primaries_BT709;
    out->transfer    = libvlc_video_transfer_func_SRGB;

    return true;
}

static void LocalSwapchainSwap( void *opaque )
{
    vout_display_t *vd = opaque;
    Swap( vd );
}

/**
 * It creates a Direct3D vout display.
 */
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys;

    if ( !vd->obj.force && vd->source.projection_mode != PROJECTION_MODE_RECTANGULAR)
        return VLC_EGENERIC; /* let a module who can handle it do it */

    if ( !vd->obj.force && vd->source.mastering.max_luminance != 0)
        return VLC_EGENERIC; /* let a module who can handle it do it */

    /* do not use D3D9 on XP unless forced */
    if (!vd->obj.force)
    {
        bool isVistaOrGreater = false;
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
        if (likely(hKernel32 != NULL))
            isVistaOrGreater = GetProcAddress(hKernel32, "EnumResourceLanguagesExW") != NULL;
        if (!isVistaOrGreater)
            return VLC_EGENERIC;
    }

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    CommonInit(vd, &sys->area, cfg);

    sys->outside_opaque = var_InheritAddress( vd, "vout-cb-opaque" );
    sys->updateOutputCb      = var_InheritAddress( vd, "vout-cb-update-output" );
    sys->swapCb              = var_InheritAddress( vd, "vout-cb-swap" );
    sys->startEndRenderingCb = var_InheritAddress( vd, "vout-cb-make-current" );

    if ( sys->swapCb == NULL || sys->startEndRenderingCb == NULL || sys->updateOutputCb == NULL )
    {
        /* use our own callbacks, since there isn't any external ones */
        if (CommonWindowInit(vd, &sys->area, &sys->sys, false))
            goto error;

        sys->outside_opaque = vd;
        sys->updateOutputCb      = LocalSwapchainUpdateOutput;
        sys->swapCb              = LocalSwapchainSwap;
        sys->startEndRenderingCb = NULL;
    }

    sys->dec_device = context ? vlc_video_context_HoldDevice(context) : NULL;
    sys->d3d9_device = GetD3D9OpaqueDevice(sys->dec_device);
    if ( sys->d3d9_device == NULL )
    {
        if (sys->dec_device)
        {
            vlc_decoder_device_Release(sys->dec_device);
            sys->dec_device = NULL;
        }

        sys->d3d9_device = D3D9_CreateDevice( vd );
        if (sys->d3d9_device == NULL) {
            msg_Err( vd, "D3D9 Creation failed!" );
            goto error;
        }
    }


    if ( vd->source.i_visible_width  > sys->d3d9_device->d3ddev.caps.MaxTextureWidth ||
         vd->source.i_visible_height > sys->d3d9_device->d3ddev.caps.MaxTextureHeight )
    {
        msg_Err(vd, "Textures too large %ux%u max possible: %lx%lx",
                vd->source.i_visible_width, vd->source.i_visible_height,
                sys->d3d9_device->d3ddev.caps.MaxTextureWidth,
                sys->d3d9_device->d3ddev.caps.MaxTextureHeight);
        goto error;
    }

    if (sys->swapCb == LocalSwapchainSwap)
        CommonPlacePicture(vd, &sys->area, &sys->sys);

    sys->hxdll = Direct3D9LoadShaderLibrary();
    if (!sys->hxdll)
        msg_Warn(vd, "cannot load Direct3D9 Shader Library; HLSL pixel shading will be disabled.");

    sys->reset_device = false;
    sys->lost_not_ready = false;
    sys->allow_hw_yuv = var_CreateGetBool(vd, "directx-hw-yuv");

    /* */
    video_format_t fmt;
    if (Direct3D9Open(vd, &fmt, context)) {
        msg_Err(vd, "Direct3D9 could not be opened");
        goto error;
    }

    /* Setup vout_display now that everything is fine */
    if (var_InheritBool(vd, "direct3d9-hw-blending") &&
        sys->d3dregion_format != D3DFMT_UNKNOWN &&
        (sys->d3d9_device->d3ddev.caps.SrcBlendCaps  & D3DPBLENDCAPS_SRCALPHA) &&
        (sys->d3d9_device->d3ddev.caps.DestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA) &&
        (sys->d3d9_device->d3ddev.caps.TextureCaps   & D3DPTEXTURECAPS_ALPHA) &&
        (sys->d3d9_device->d3ddev.caps.TextureOpCaps & D3DTEXOPCAPS_SELECTARG1) &&
        (sys->d3d9_device->d3ddev.caps.TextureOpCaps & D3DTEXOPCAPS_MODULATE))
        vd->info.subpicture_chromas = d3d_subpicture_chromas;
    else
        vd->info.subpicture_chromas = NULL;

    video_format_Clean(fmtp);
    video_format_Copy(fmtp, &fmt);

    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->close = Close;

    return VLC_SUCCESS;
error:
    Direct3D9Close(vd);
    CommonWindowClean(VLC_OBJECT(vd), &sys->sys);
    Direct3D9Destroy(sys);
    free(vd->sys);
    return VLC_EGENERIC;
}

/**
 * It destroyes a Direct3D vout display.
 */
static void Close(vout_display_t *vd)
{
    Direct3D9Close(vd);

    CommonWindowClean(VLC_OBJECT(vd), &vd->sys->sys);

    Direct3D9Destroy(vd->sys);

    free(vd->sys);
}
