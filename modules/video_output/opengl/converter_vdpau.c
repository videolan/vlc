/*****************************************************************************
 * converter_vdpau.c: OpenGL VDPAU opaque converter
 *****************************************************************************
 * Copyright (C) 2017 VLC authors, VideoLAN and VideoLabs
 *
 * Author: Victorien Le Couviour--Tuffet <victorien.lecouviour.tuffet@gmail.com>
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

#include <GL/gl.h>
#include <GL/glext.h>

#include <vlc_common.h>
#include <vlc_vout_window.h>
#include <vlc_xlib.h>

#include "../../hw/vdpau/vlc_vdpau.h"
#include "internal.h"

#define INTEROP_CALL(fct, ...) \
    _##fct(__VA_ARGS__); \
    { \
        GLenum ret = tc->vt->GetError(); \
        if (ret != GL_NO_ERROR) \
        { \
            msg_Err(tc->gl, #fct " failed: 0x%x\n", ret); \
            return VLC_EGENERIC; \
        } \
    }

struct  priv
{
    vdp_t *vdp;
    VdpDevice vdp_device;
};

static PFNGLVDPAUINITNVPROC                     _glVDPAUInitNV;
static PFNGLVDPAUFININVPROC                     _glVDPAUFiniNV;
static PFNGLVDPAUREGISTEROUTPUTSURFACENVPROC    _glVDPAURegisterOutputSurfaceNV;
static PFNGLVDPAUISSURFACENVPROC                _glVDPAUIsSurfaceNV;
static PFNGLVDPAUUNREGISTERSURFACENVPROC        _glVDPAUUnregisterSurfaceNV;
static PFNGLVDPAUGETSURFACEIVNVPROC             _glVDPAUGetSurfaceivNV;
static PFNGLVDPAUSURFACEACCESSNVPROC            _glVDPAUSurfaceAccessNV;
static PFNGLVDPAUMAPSURFACESNVPROC              _glVDPAUMapSurfacesNV;
static PFNGLVDPAUUNMAPSURFACESNVPROC            _glVDPAUUnmapSurfacesNV;

static void
pool_pic_destroy_cb(picture_t *pic)
{
    vdp_output_surface_destroy(pic->p_sys->vdp, pic->p_sys->surface);
    vdp_release_x11(pic->p_sys->vdp);
    free(pic->p_sys);
    free(pic);
}

static picture_pool_t *
tc_vdpau_gl_get_pool(opengl_tex_converter_t const *tc,
                     unsigned int requested_count)
{
    struct priv *priv = tc->priv;
    picture_t *pics[requested_count];

    unsigned int i;
    for (i = 0; i < requested_count; ++i)
    {
        VdpOutputSurface surface;

        VdpStatus st;
        if ((st = vdp_output_surface_create(priv->vdp, priv->vdp_device,
                                            VDP_RGBA_FORMAT_B8G8R8A8,
                                            tc->fmt.i_visible_width,
                                            tc->fmt.i_visible_height,
                                            &surface)) != VDP_STATUS_OK)
            goto error;

        picture_sys_t *picsys = calloc(1, sizeof(*picsys));
        if (!picsys)
            goto error;

        picsys->vdp = vdp_hold_x11(priv->vdp, NULL);
        picsys->device = priv->vdp_device;
        picsys->surface = surface;

        picture_resource_t rsc = { .p_sys = picsys,
                                   .pf_destroy = pool_pic_destroy_cb };

        pics[i] = picture_NewFromResource(&tc->fmt, &rsc);
        if (!pics[i])
            goto error;
    }

    picture_pool_t *pool = picture_pool_New(requested_count, pics);
    if (!pool)
        goto error;

    return pool;

error:
    while (i--)
        picture_Release(pics[i]);
    return NULL;
}

static int
tc_vdpau_gl_update(opengl_tex_converter_t const *tc, GLuint textures[],
                   GLsizei const tex_widths[], GLsizei const tex_heights[],
                   picture_t *pic, size_t const plane_offsets[])
{
    VLC_UNUSED(tex_widths);
    VLC_UNUSED(tex_heights);
    VLC_UNUSED(plane_offsets);

    GLvdpauSurfaceNV *p_gl_nv_surface =
        (GLvdpauSurfaceNV *)&pic->p_sys->gl_nv_surface;

    if (*p_gl_nv_surface)
    {
        assert(_glVDPAUIsSurfaceNV(*p_gl_nv_surface) == GL_TRUE);

        GLint state;
        GLsizei num_val;
        INTEROP_CALL(glVDPAUGetSurfaceivNV, *p_gl_nv_surface,
                     GL_SURFACE_STATE_NV, 1, &num_val, &state);
        assert(num_val == 1); assert(state == GL_SURFACE_MAPPED_NV);

        INTEROP_CALL(glVDPAUUnmapSurfacesNV, 1, p_gl_nv_surface);
        INTEROP_CALL(glVDPAUUnregisterSurfaceNV, *p_gl_nv_surface);
    }

    *p_gl_nv_surface =
        INTEROP_CALL(glVDPAURegisterOutputSurfaceNV,
                     (void *)(size_t)pic->p_sys->surface,
                     GL_TEXTURE_2D, tc->tex_count, textures);
    INTEROP_CALL(glVDPAUSurfaceAccessNV, *p_gl_nv_surface, GL_READ_ONLY);
    INTEROP_CALL(glVDPAUMapSurfacesNV, 1, p_gl_nv_surface);

    return VLC_SUCCESS;
}

static void
Close(vlc_object_t *obj)
{
    opengl_tex_converter_t *tc = (void *)obj;
    _glVDPAUFiniNV(); assert(tc->vt->GetError() == GL_NO_ERROR);
    vdp_release_x11(((struct priv *)tc->priv)->vdp);
    free(tc->priv);
}

static int
Open(vlc_object_t *obj)
{
    opengl_tex_converter_t *tc = (void *) obj;
    if ((tc->fmt.i_chroma != VLC_CODEC_VDPAU_VIDEO_420 &&
         tc->fmt.i_chroma != VLC_CODEC_VDPAU_VIDEO_422 &&
         tc->fmt.i_chroma != VLC_CODEC_VDPAU_VIDEO_444) ||
        !HasExtension(tc->glexts, "GL_NV_vdpau_interop") ||
        tc->gl->surface->type != VOUT_WINDOW_TYPE_XID)
        return VLC_EGENERIC;

    tc->fmt.i_chroma = VLC_CODEC_VDPAU_OUTPUT;

    if (!vlc_xlib_init(VLC_OBJECT(tc->gl)))
        return VLC_EGENERIC;

    struct priv *priv = calloc(1, sizeof(*priv));
    if (!priv)
        return VLC_EGENERIC;
    tc->priv = priv;

    if (vdp_get_x11(tc->gl->surface->display.x11, -1,
                    &priv->vdp, &priv->vdp_device) != VDP_STATUS_OK)
    {
        free(priv);
        return VLC_EGENERIC;
    }

    void *vdp_gpa;
    if (vdp_get_proc_address(priv->vdp, priv->vdp_device,
                             VDP_FUNC_ID_GET_PROC_ADDRESS, &vdp_gpa)
        != VDP_STATUS_OK)
    {
        vdp_release_x11(priv->vdp);
        free(priv);
        return VLC_EGENERIC;
    }

#define SAFE_GPA(fct) \
    _##fct = vlc_gl_GetProcAddress(tc->gl, #fct); \
    if (!_##fct) \
    { \
        vdp_release_x11(priv->vdp); \
        free(priv); \
        return VLC_EGENERIC; \
    }
    SAFE_GPA(glVDPAUInitNV);
    SAFE_GPA(glVDPAUFiniNV);
    SAFE_GPA(glVDPAURegisterOutputSurfaceNV);
    SAFE_GPA(glVDPAUIsSurfaceNV);
    SAFE_GPA(glVDPAUUnregisterSurfaceNV);
    SAFE_GPA(glVDPAUGetSurfaceivNV);
    SAFE_GPA(glVDPAUSurfaceAccessNV);
    SAFE_GPA(glVDPAUMapSurfacesNV);
    SAFE_GPA(glVDPAUUnmapSurfacesNV);
#undef SAFE_GPA

    INTEROP_CALL(glVDPAUInitNV, (void *)(size_t)priv->vdp_device, vdp_gpa);

    tc->fshader = opengl_fragment_shader_init(tc, GL_TEXTURE_2D,
                                              VLC_CODEC_RGB32,
                                              COLOR_SPACE_UNDEF);
    if (!tc->fshader)
    {
        Close(obj);
        return VLC_EGENERIC;
    }

    tc->pf_get_pool = tc_vdpau_gl_get_pool;
    tc->pf_update = tc_vdpau_gl_update;

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_description("VDPAU OpenGL surface converter")
    set_capability("glconv", 2)
    set_callbacks(Open, Close)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("vdpau")
vlc_module_end ()
