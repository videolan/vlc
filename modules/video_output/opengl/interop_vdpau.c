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
#include <vlc_window.h>
#include <vlc_xlib.h>
#include <vlc_codec.h>
#include <vlc_plugin.h>

#include "gl_api.h"
#include "gl_util.h"
#include "../../hw/vdpau/vlc_vdpau.h"
#include "interop.h"

#define INTEROP_CALL(sys, fct, ...) \
    ((sys)->gl.fct(__VA_ARGS__)); \
    { \
        GLenum ret = (sys)->gl.GetError(); \
        if (ret != GL_NO_ERROR) \
        { \
            msg_Err(interop->gl, #fct " failed: 0x%x", ret); \
            return VLC_EGENERIC; \
        } \
    }

typedef struct {
    vlc_decoder_device *dec_device;
    struct {
        PFNGLGETERRORPROC GetError;
        PFNGLVDPAUINITNVPROC                     VDPAUInitNV;
        PFNGLVDPAUFININVPROC                     VDPAUFiniNV;
        PFNGLVDPAUREGISTEROUTPUTSURFACENVPROC    VDPAURegisterOutputSurfaceNV;
        PFNGLVDPAUISSURFACENVPROC                VDPAUIsSurfaceNV;
        PFNGLVDPAUUNREGISTERSURFACENVPROC        VDPAUUnregisterSurfaceNV;
        PFNGLVDPAUGETSURFACEIVNVPROC             VDPAUGetSurfaceivNV;
        PFNGLVDPAUSURFACEACCESSNVPROC            VDPAUSurfaceAccessNV;
        PFNGLVDPAUMAPSURFACESNVPROC              VDPAUMapSurfacesNV;
        PFNGLVDPAUUNMAPSURFACESNVPROC            VDPAUUnmapSurfacesNV;
    } gl;
} converter_sys_t;

static int
tc_vdpau_gl_update(const struct vlc_gl_interop *interop, uint32_t textures[],
                   int32_t const tex_widths[], int32_t const tex_heights[],
                   picture_t *pic, size_t const plane_offsets[])
{
    VLC_UNUSED(tex_widths);
    VLC_UNUSED(tex_heights);
    VLC_UNUSED(plane_offsets);

    vlc_vdp_output_surface_t *p_sys = pic->p_sys;
    converter_sys_t *convsys = interop->priv;
    GLvdpauSurfaceNV gl_nv_surface = p_sys->gl_nv_surface;

    static_assert (sizeof (gl_nv_surface) <= sizeof (p_sys->gl_nv_surface),
                   "Type too small");

    if (gl_nv_surface)
    {
        assert(convsys->gl.VDPAUIsSurfaceNV(gl_nv_surface) == GL_TRUE);

        GLint state;
        GLsizei num_val;
        INTEROP_CALL(convsys, VDPAUGetSurfaceivNV, gl_nv_surface,
                     GL_SURFACE_STATE_NV, 1, &num_val, &state);
        assert(num_val == 1); assert(state == GL_SURFACE_MAPPED_NV);

        INTEROP_CALL(convsys, VDPAUUnmapSurfacesNV, 1, &gl_nv_surface);
        INTEROP_CALL(convsys, VDPAUUnregisterSurfaceNV, gl_nv_surface);
    }

    gl_nv_surface =
        INTEROP_CALL(convsys, VDPAURegisterOutputSurfaceNV,
                     (void *)(size_t)p_sys->surface,
                     GL_TEXTURE_2D, interop->tex_count, textures);
    INTEROP_CALL(convsys, VDPAUSurfaceAccessNV, gl_nv_surface, GL_READ_ONLY);
    INTEROP_CALL(convsys, VDPAUMapSurfacesNV, 1, &gl_nv_surface);

    p_sys->gl_nv_surface = gl_nv_surface;
    return VLC_SUCCESS;
}

static void
Close(struct vlc_gl_interop *interop)
{
    converter_sys_t *sys = interop->priv;

    sys->gl.VDPAUFiniNV();
    assert(sys->gl.GetError() == GL_NO_ERROR);
    vlc_decoder_device *dec_device = sys->dec_device;
    vlc_decoder_device_Release(dec_device);
}

static int
Open(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *) obj;
    if (interop->vctx == NULL)
        return VLC_EGENERIC;

    struct vlc_gl_extension_vt extension_vt;
    vlc_gl_LoadExtensionFunctions(interop->gl, &extension_vt);

    vlc_decoder_device *dec_device = vlc_video_context_HoldDevice(interop->vctx);
    if (GetVDPAUOpaqueDevice(dec_device) == NULL
     || interop->fmt_in.i_chroma != VLC_CODEC_VDPAU_VIDEO
     || !vlc_gl_HasExtension(&extension_vt, "GL_NV_vdpau_interop"))
    {
        vlc_decoder_device_Release(dec_device);
        return VLC_EGENERIC;
    }

    converter_sys_t *sys = vlc_obj_malloc(VLC_OBJECT(interop), sizeof(*sys));
    if (unlikely(sys == NULL))
    {
        vlc_decoder_device_Release(dec_device);
        return VLC_ENOMEM;
    }
    sys->dec_device = dec_device;
    sys->gl.GetError = vlc_gl_GetProcAddress(interop->gl, "glGetError");
    assert(sys->gl.GetError != NULL);

    /* Request to change the input chroma to the core */
    interop->fmt_in.i_chroma = VLC_CODEC_VDPAU_OUTPUT;

    VdpDevice device;
    vdpau_decoder_device_t *vdpau_dev = GetVDPAUOpaqueDevice(dec_device);
    vdp_t *vdp = vdpau_dev->vdp;
    device = vdpau_dev->device;

    void *vdp_gpa;
    if (vdp_get_proc_address(vdp, device,
                             VDP_FUNC_ID_GET_PROC_ADDRESS, &vdp_gpa)
        != VDP_STATUS_OK)
    {
        vlc_decoder_device_Release(dec_device);
        return VLC_EGENERIC;
    }

#define SAFE_GPA(fct) \
    sys->gl.fct = vlc_gl_GetProcAddress(interop->gl, "gl" #fct); \
    if (sys->gl.fct == NULL) \
    { \
        vlc_decoder_device_Release(dec_device); \
        return VLC_EGENERIC; \
    }
    SAFE_GPA(VDPAUInitNV);
    SAFE_GPA(VDPAUFiniNV);
    SAFE_GPA(VDPAURegisterOutputSurfaceNV);
    SAFE_GPA(VDPAUIsSurfaceNV);
    SAFE_GPA(VDPAUUnregisterSurfaceNV);
    SAFE_GPA(VDPAUGetSurfaceivNV);
    SAFE_GPA(VDPAUSurfaceAccessNV);
    SAFE_GPA(VDPAUMapSurfacesNV);
    SAFE_GPA(VDPAUUnmapSurfacesNV);
#undef SAFE_GPA

    INTEROP_CALL(sys, VDPAUInitNV, (void *)(uintptr_t)device, vdp_gpa);

    /* The pictures are uploaded upside-down */
    video_format_TransformBy(&interop->fmt_out, TRANSFORM_VFLIP);

    interop->tex_target = GL_TEXTURE_2D;
    interop->fmt_out.i_chroma = VLC_CODEC_RGB32;
    interop->fmt_out.space = COLOR_SPACE_UNDEF;

    interop->tex_count = 1;
    interop->texs[0] = (struct vlc_gl_tex_cfg) {
        .w = {1, 1},
        .h = {1, 1},
        .internal = GL_RGBA,
        .format = GL_RGBA,
        .type = GL_UNSIGNED_BYTE,
    };

    static const struct vlc_gl_interop_ops ops = {
        .update_textures = tc_vdpau_gl_update,
        .close = Close,
    };
    interop->ops = &ops;
    interop->priv = sys;

    return VLC_SUCCESS;
}


vlc_module_begin ()
    set_description("VDPAU OpenGL surface converter")
    set_capability("glinterop", 2)
    set_callback(Open)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("vdpau")
vlc_module_end ()
