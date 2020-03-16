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

    if (priv->dx_render)
        IDirect3DSurface9_Release(priv->dx_render);

    free(priv);
}

static int
GLConvOpen(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *) obj;

    if (interop->fmt.i_chroma != VLC_CODEC_D3D9_OPAQUE
     && interop->fmt.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B)
        return VLC_EGENERIC;

    d3d9_video_context_t *vctx_sys = GetD3D9ContextPrivate( interop->vctx );
    d3d9_decoder_device_t *d3d9_decoder = GetD3D9OpaqueContext(interop->vctx);
    if ( vctx_sys == NULL || d3d9_decoder == NULL )
        return VLC_EGENERIC;

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
    priv->vt = vt;

    if (!d3d9_decoder->hd3d.use_ex)
    {
        msg_Warn(obj, "DX/GL interrop only working on d3d9x");
        goto error;
    }

    HRESULT hr;
    HANDLE shared_handle = NULL;
    hr = IDirect3DDevice9Ex_CreateRenderTarget(d3d9_decoder->d3ddev.devex,
                                               interop->fmt.i_visible_width,
                                               interop->fmt.i_visible_height,
                                               D3DFMT_X8R8G8B8,
                                               D3DMULTISAMPLE_NONE, 0, FALSE,
                                               &priv->dx_render, &shared_handle);
    if (FAILED(hr))
    {
        msg_Warn(obj, "IDirect3DDevice9Ex_CreateRenderTarget failed");
        goto error;
    }

   if (shared_handle)
        priv->vt.DXSetResourceShareHandleNV(priv->dx_render, shared_handle);

    priv->gl_handle_d3d = priv->vt.DXOpenDeviceNV(d3d9_decoder->d3ddev.dev);
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

    int ret = opengl_interop_init(interop, GL_TEXTURE_2D, VLC_CODEC_RGB32,
                                  COLOR_SPACE_UNDEF);
    if (ret != VLC_SUCCESS)
        goto error;

    return VLC_SUCCESS;

error:
    GLConvClose(obj);
    return VLC_EGENERIC;
}
