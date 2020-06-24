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

#include <windows.h>
#include <d3d9.h>
#include "../../video_chroma/d3d9_fmt.h"
#include <dxvahd.h>

#include "../opengl/interop.h"
#include "../opengl/renderer.h"
#include <GL/glew.h>
#include <GL/wglew.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  GLConvOpen(vlc_object_t *);
static void GLConvClose(vlc_object_t *);

vlc_module_begin ()
    set_shortname("dxva2")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description("DXVA2 surface converter")
    set_capability("glinterop", 1)
    set_callbacks(GLConvOpen, GLConvClose)

    add_bool("direct3d9-dxvahd", true, DXVAHD_TEXT, DXVAHD_LONGTEXT, true)
vlc_module_end ()

struct wgl_vt {
    PFNWGLDXSETRESOURCESHAREHANDLENVPROC DXSetResourceShareHandleNV;
    PFNWGLDXOPENDEVICENVPROC             DXOpenDeviceNV;
    PFNWGLDXCLOSEDEVICENVPROC            DXCloseDeviceNV;
    PFNWGLDXREGISTEROBJECTNVPROC         DXRegisterObjectNV;
    PFNWGLDXUNREGISTEROBJECTNVPROC       DXUnregisterObjectNV;
    PFNWGLDXLOCKOBJECTSNVPROC            DXLockObjectsNV;
    PFNWGLDXUNLOCKOBJECTSNVPROC          DXUnlockObjectsNV;
};
struct glpriv
{
    struct wgl_vt vt;
    HANDLE gl_handle_d3d;
    HANDLE gl_render;
    IDirect3DSurface9 *dx_render;

    D3DFORMAT OutputFormat;

    /* range converter */
    struct {
        HMODULE                 dll;
        IDXVAHD_VideoProcessor *proc;
    } processor;
};

static int
GLConvUpdate(const struct vlc_gl_interop *interop, GLuint *textures,
             const GLsizei *tex_width, const GLsizei *tex_height,
             picture_t *pic, const size_t *plane_offset)
{
    VLC_UNUSED(textures); VLC_UNUSED(tex_width); VLC_UNUSED(tex_height); VLC_UNUSED(plane_offset);
    struct glpriv *priv = interop->priv;
    HRESULT hr;

    picture_sys_d3d9_t *picsys = ActiveD3D9PictureSys(pic);
    if (unlikely(!picsys || !priv->gl_render))
        return VLC_EGENERIC;

    if (!priv->vt.DXUnlockObjectsNV(priv->gl_handle_d3d, 1, &priv->gl_render))
    {
        msg_Warn(interop->gl, "DXUnlockObjectsNV failed");
        return VLC_EGENERIC;
    }

    d3d9_decoder_device_t *d3d9_decoder = GetD3D9OpaqueContext(interop->vctx);

    if (priv->processor.proc)
    {
        DXVAHD_STREAM_DATA inputStream = { 0 };
        inputStream.Enable = TRUE;
        inputStream.pInputSurface = picsys->surface;
        hr = IDXVAHD_VideoProcessor_VideoProcessBltHD( priv->processor.proc, priv->dx_render, 0, 1, &inputStream );
        if (FAILED(hr)) {
            D3DSURFACE_DESC srcDesc, dstDesc;
            IDirect3DSurface9_GetDesc(picsys->surface, &srcDesc);
            IDirect3DSurface9_GetDesc(priv->dx_render, &dstDesc);

            msg_Dbg(interop->gl, "Failed VideoProcessBltHD src:%4.4s (%d) dst:%4.4s (%d) (hr=0x%lX)",
                    (const char*)&srcDesc.Format, srcDesc.Format,
                    (const char*)&dstDesc.Format, dstDesc.Format, hr);
            return VLC_EGENERIC;
        }
    }
    else
    {
        const RECT rect = {
            .left = 0,
            .top = 0,
            .right = pic->format.i_visible_width,
            .bottom = pic->format.i_visible_height
        };
        hr = IDirect3DDevice9Ex_StretchRect(d3d9_decoder->d3ddev.devex, picsys->surface,
                                            &rect, priv->dx_render, NULL, D3DTEXF_NONE);
        if (FAILED(hr))
        {
            msg_Warn(interop->gl, "IDirect3DDevice9Ex_StretchRect failed. (0x%lX)", hr);
            return VLC_EGENERIC;
        }
    }

    if (!priv->vt.DXLockObjectsNV(priv->gl_handle_d3d, 1, &priv->gl_render))
    {
        msg_Warn(interop->gl, "DXLockObjectsNV failed");
        priv->vt.DXUnregisterObjectNV(priv->gl_handle_d3d, priv->gl_render);
        priv->gl_render = NULL;
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int
GLConvAllocateTextures(const struct vlc_gl_interop *interop, GLuint *textures,
                       const GLsizei *tex_width, const GLsizei *tex_height)
{
    VLC_UNUSED(tex_width); VLC_UNUSED(tex_height);
    struct glpriv *priv = interop->priv;

    priv->gl_render =
        priv->vt.DXRegisterObjectNV(priv->gl_handle_d3d, priv->dx_render,
                                    textures[0], GL_TEXTURE_2D, WGL_ACCESS_WRITE_DISCARD_NV);
    if (!priv->gl_render)
    {
        msg_Warn(interop->gl, "DXRegisterObjectNV failed: %lu", GetLastError());
        return VLC_EGENERIC;
    }

    if (!priv->vt.DXLockObjectsNV(priv->gl_handle_d3d, 1, &priv->gl_render))
    {
        msg_Warn(interop->gl, "DXLockObjectsNV failed");
        priv->vt.DXUnregisterObjectNV(priv->gl_handle_d3d, priv->gl_render);
        priv->gl_render = NULL;
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void
GLConvClose(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *)obj;
    struct glpriv *priv = interop->priv;

    if (priv->gl_handle_d3d)
    {
        if (priv->gl_render)
        {
            priv->vt.DXUnlockObjectsNV(priv->gl_handle_d3d, 1, &priv->gl_render);
            priv->vt.DXUnregisterObjectNV(priv->gl_handle_d3d, priv->gl_render);
        }

        priv->vt.DXCloseDeviceNV(priv->gl_handle_d3d);
    }
    if (priv->processor.proc)
    {
        IDXVAHD_VideoProcessor_Release(priv->processor.proc);
        FreeLibrary(priv->processor.dll);
    }

    if (priv->dx_render)
        IDirect3DSurface9_Release(priv->dx_render);

    free(priv);
}

static void SetupProcessorInput(struct vlc_gl_interop *interop, const video_format_t *fmt, D3DFORMAT src_format)
{
    struct glpriv *sys = interop->priv;
    HRESULT hr;
    DXVAHD_STREAM_STATE_D3DFORMAT_DATA d3dformat = { src_format };
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
        .left   = interop->fmt_in.i_x_offset,
        .right  = interop->fmt_in.i_x_offset + interop->fmt_in.i_visible_width,
        .top    = interop->fmt_in.i_y_offset,
        .bottom = interop->fmt_in.i_y_offset + interop->fmt_in.i_visible_height,
    };;
    hr = IDXVAHD_VideoProcessor_SetVideoProcessStreamState( sys->processor.proc, 0, DXVAHD_STREAM_STATE_SOURCE_RECT, sizeof(srcRect), &srcRect );

    DXVAHD_BLT_STATE_TARGET_RECT_DATA dstRect;
    dstRect.Enable = TRUE;
    dstRect.TargetRect = (RECT) {
        .left   = 0,
        .right  = interop->fmt_in.i_visible_width,
        .top    = 0,
        .bottom = interop->fmt_in.i_visible_height,
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

static int InitRangeProcessor(struct vlc_gl_interop *interop, IDirect3DDevice9Ex *devex, D3DFORMAT src_format)
{
    struct glpriv *sys = interop->priv;

    HRESULT hr;

    sys->processor.dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (unlikely(!sys->processor.dll))
    {
        msg_Err(interop, "Failed to load DXVA2.DLL");
        return VLC_EGENERIC;
    }

    D3DFORMAT *formatsList = NULL;
    DXVAHD_VPCAPS *capsList = NULL;
    IDXVAHD_Device *hd_device = NULL;

    HRESULT (WINAPI *CreateDevice)(IDirect3DDevice9Ex *,const DXVAHD_CONTENT_DESC *,DXVAHD_DEVICE_USAGE,PDXVAHDSW_Plugin,IDXVAHD_Device **);
    CreateDevice = (void *)GetProcAddress(sys->processor.dll, "DXVAHD_CreateDevice");
    if (CreateDevice == NULL)
    {
        msg_Err(interop, "Can't create HD device (not Windows 7+)");
        goto error;
    }

    DXVAHD_CONTENT_DESC desc;
    desc.InputFrameFormat = DXVAHD_FRAME_FORMAT_PROGRESSIVE;
    GetFrameRate( &desc.InputFrameRate, &interop->fmt_in );
    desc.InputWidth       = interop->fmt_in.i_visible_width;
    desc.InputHeight      = interop->fmt_in.i_visible_height;
    desc.OutputFrameRate  = desc.InputFrameRate;
    desc.OutputWidth      = interop->fmt_in.i_visible_width;
    desc.OutputHeight     = interop->fmt_in.i_visible_height;

    hr = CreateDevice(devex, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &hd_device);
    if (FAILED(hr))
    {
        msg_Dbg(interop, "Failed to create the device (error 0x%lX)", hr);
        goto error;
    }

    DXVAHD_VPDEVCAPS devcaps = { 0 };
    hr = IDXVAHD_Device_GetVideoProcessorDeviceCaps( hd_device, &devcaps );
    if (unlikely(FAILED(hr)))
    {
        msg_Err(interop, "Failed to get the device capabilities (error 0x%lX)", hr);
        goto error;
    }
    if (devcaps.VideoProcessorCount == 0)
    {
        msg_Warn(interop, "No good video processor found for range conversion");
        goto error;
    }

    formatsList = malloc(devcaps.InputFormatCount * sizeof(*formatsList));
    if (unlikely(formatsList == NULL))
    {
        msg_Dbg(interop, "Failed to allocate %u input formats", devcaps.InputFormatCount);
        goto error;
    }

    hr = IDXVAHD_Device_GetVideoProcessorInputFormats( hd_device, devcaps.InputFormatCount, formatsList);
    UINT i;
    for (i=0; i<devcaps.InputFormatCount; i++)
    {
        if (formatsList[i] == src_format)
            break;
    }
    if (i == devcaps.InputFormatCount)
    {
        msg_Warn(interop, "Input format %4.4s not supported for range conversion", (const char*)&src_format);
        goto error;
    }

    free(formatsList);
    formatsList = malloc(devcaps.OutputFormatCount * sizeof(*formatsList));
    if (unlikely(formatsList == NULL))
    {
        msg_Dbg(interop, "Failed to allocate %u output formats", devcaps.OutputFormatCount);
        goto error;
    }

    hr = IDXVAHD_Device_GetVideoProcessorOutputFormats( hd_device, devcaps.OutputFormatCount, formatsList);
    for (i=0; i<devcaps.OutputFormatCount; i++)
    {
        if (formatsList[i] == sys->OutputFormat)
            break;
    }
    if (i == devcaps.OutputFormatCount)
    {
        msg_Warn(interop, "Output format %d not supported for range conversion", sys->OutputFormat);
        goto error;
    }

    capsList = malloc(devcaps.VideoProcessorCount * sizeof(*capsList));
    if (unlikely(capsList == NULL))
    {
        msg_Dbg(interop, "Failed to allocate %u video processors", devcaps.VideoProcessorCount);
        goto error;
    }
    hr = IDXVAHD_Device_GetVideoProcessorCaps( hd_device, devcaps.VideoProcessorCount, capsList);
    if (FAILED(hr))
    {
        msg_Dbg(interop, "Failed to get the processor caps (error 0x%lX)", hr);
        goto error;
    }

    hr = IDXVAHD_Device_CreateVideoProcessor( hd_device, &capsList->VPGuid, &sys->processor.proc );
    if (FAILED(hr))
    {
        msg_Dbg(interop, "Failed to create the processor (error 0x%lX)", hr);
        goto error;
    }
    IDXVAHD_Device_Release( hd_device );

    SetupProcessorInput(interop, &interop->fmt_in, src_format);

    DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE_DATA colorspace;
    colorspace.Usage = 0; // playback
    colorspace.RGB_Range = true ? 0 : 1;
    colorspace.YCbCr_xvYCC = true ? 1 : 0;
    colorspace.YCbCr_Matrix = false ? 0 : 1;
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

static int
GLConvOpen(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *) obj;

    if (interop->fmt_in.i_chroma != VLC_CODEC_D3D9_OPAQUE
     && interop->fmt_in.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B)
        return VLC_EGENERIC;

    d3d9_video_context_t *vctx_sys = GetD3D9ContextPrivate( interop->vctx );
    d3d9_decoder_device_t *d3d9_decoder = GetD3D9OpaqueContext(interop->vctx);
    if ( vctx_sys == NULL || d3d9_decoder == NULL )
        return VLC_EGENERIC;

    if (!d3d9_decoder->hd3d.use_ex)
    {
        msg_Warn(obj, "DX/GL interrop only working on d3d9x");
        return VLC_EGENERIC;
    }

    if (interop->gl->ext != VLC_GL_EXT_WGL || !interop->gl->wgl.getExtensionsString)
        return VLC_EGENERIC;

    const char *wglExt = interop->gl->wgl.getExtensionsString(interop->gl);

    if (wglExt == NULL || !vlc_gl_StrHasToken(wglExt, "WGL_NV_DX_interop"))
        return VLC_EGENERIC;

    struct wgl_vt vt;
#define LOAD_EXT(name, type) do { \
    vt.name = (type) vlc_gl_GetProcAddress(interop->gl, "wgl" #name); \
    if (!vt.name) { \
        msg_Warn(obj, "'wgl " #name "' could not be loaded"); \
        return VLC_EGENERIC; \
    } \
} while(0)

    LOAD_EXT(DXSetResourceShareHandleNV, PFNWGLDXSETRESOURCESHAREHANDLENVPROC);
    LOAD_EXT(DXOpenDeviceNV, PFNWGLDXOPENDEVICENVPROC);
    LOAD_EXT(DXCloseDeviceNV, PFNWGLDXCLOSEDEVICENVPROC);
    LOAD_EXT(DXRegisterObjectNV, PFNWGLDXREGISTEROBJECTNVPROC);
    LOAD_EXT(DXUnregisterObjectNV, PFNWGLDXUNREGISTEROBJECTNVPROC);
    LOAD_EXT(DXLockObjectsNV, PFNWGLDXLOCKOBJECTSNVPROC);
    LOAD_EXT(DXUnlockObjectsNV, PFNWGLDXUNLOCKOBJECTSNVPROC);

    struct glpriv *priv = calloc(1, sizeof(struct glpriv));
    if (!priv)
        return VLC_ENOMEM;
    interop->priv = priv;
    priv->OutputFormat = D3DFMT_X8R8G8B8;
    priv->vt = vt;

    HRESULT hr;
    bool force_dxva_hd = var_InheritBool(interop, "direct3d9-dxvahd");
    if (force_dxva_hd || (interop->fmt_in.color_range != COLOR_RANGE_FULL &&
                          d3d9_decoder->d3ddev.identifier.VendorId == GPU_MANUFACTURER_NVIDIA))
    {
        // NVIDIA bug, YUV to RGB internal conversion in StretchRect always converts from limited to limited range
        msg_Dbg(interop->gl, "init DXVA-HD processor from %4.4s to RGB", (const char*)&vctx_sys->format);
        int err = InitRangeProcessor(interop, d3d9_decoder->d3ddev.devex, vctx_sys->format);
        if (err != VLC_SUCCESS)
            force_dxva_hd = false;
    }
    if (!force_dxva_hd)
    {
        // test whether device can perform color-conversion from that format to target format
        hr = IDirect3D9_CheckDeviceFormatConversion(d3d9_decoder->hd3d.obj,
                                                    d3d9_decoder->d3ddev.adapterId,
                                                    D3DDEVTYPE_HAL,
                                                    vctx_sys->format, priv->OutputFormat);
        if (FAILED(hr))
        {
            msg_Dbg(interop->gl, "Unsupported conversion from %4.4s to RGB", (const char*)&vctx_sys->format );
            goto error;
        }
        msg_Dbg(interop->gl, "using StrecthRect from %4.4s to RGB", (const char*)&vctx_sys->format );
    }

    HANDLE shared_handle = NULL;
    hr = IDirect3DDevice9Ex_CreateRenderTarget(d3d9_decoder->d3ddev.devex,
                                               interop->fmt_in.i_visible_width,
                                               interop->fmt_in.i_visible_height,
                                               priv->OutputFormat,
                                               D3DMULTISAMPLE_NONE, 0, FALSE,
                                               &priv->dx_render, &shared_handle);
    if (FAILED(hr))
    {
        msg_Warn(obj, "IDirect3DDevice9Ex_CreateRenderTarget failed");
        goto error;
    }

   if (shared_handle)
        priv->vt.DXSetResourceShareHandleNV(priv->dx_render, shared_handle);

    priv->gl_handle_d3d = priv->vt.DXOpenDeviceNV(d3d9_decoder->d3ddev.devex);
    if (!priv->gl_handle_d3d)
    {
        msg_Warn(obj, "DXOpenDeviceNV failed: %lu", GetLastError());
        goto error;
    }

    static const struct vlc_gl_interop_ops ops = {
        .allocate_textures = GLConvAllocateTextures,
        .update_textures = GLConvUpdate,
    };
    interop->ops = &ops;

    /* The pictures are uploaded upside-down */
    video_format_TransformBy(&interop->fmt_out, TRANSFORM_VFLIP);

    int ret = opengl_interop_init(interop, GL_TEXTURE_2D, VLC_CODEC_RGB32,
                                  COLOR_SPACE_UNDEF);
    if (ret != VLC_SUCCESS)
        goto error;

    return VLC_SUCCESS;

error:
    GLConvClose(obj);
    return VLC_EGENERIC;
}
