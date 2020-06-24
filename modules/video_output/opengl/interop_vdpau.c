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
#include <vlc_codec.h>
#include <vlc_plugin.h>

#include "gl_api.h"
#include "../../hw/vdpau/vlc_vdpau.h"
#include "internal.h"
#include "interop.h"

#define INTEROP_CALL(fct, ...) \
    _##fct(__VA_ARGS__); \
    { \
        GLenum ret = interop->vt->GetError(); \
        if (ret != GL_NO_ERROR) \
        { \
            msg_Err(interop->gl, #fct " failed: 0x%x", ret); \
            return VLC_EGENERIC; \
        } \
    }

static PFNGLVDPAUINITNVPROC                     _glVDPAUInitNV;
static PFNGLVDPAUFININVPROC                     _glVDPAUFiniNV;
static PFNGLVDPAUREGISTEROUTPUTSURFACENVPROC    _glVDPAURegisterOutputSurfaceNV;
static PFNGLVDPAUISSURFACENVPROC                _glVDPAUIsSurfaceNV;
static PFNGLVDPAUUNREGISTERSURFACENVPROC        _glVDPAUUnregisterSurfaceNV;
static PFNGLVDPAUGETSURFACEIVNVPROC             _glVDPAUGetSurfaceivNV;
static PFNGLVDPAUSURFACEACCESSNVPROC            _glVDPAUSurfaceAccessNV;
static PFNGLVDPAUMAPSURFACESNVPROC              _glVDPAUMapSurfacesNV;
static PFNGLVDPAUUNMAPSURFACESNVPROC            _glVDPAUUnmapSurfacesNV;

typedef struct {
    vlc_decoder_device *dec_device;
} converter_sys_t;

static int
tc_vdpau_gl_update(const struct vlc_gl_interop *interop, GLuint textures[],
                   GLsizei const tex_widths[], GLsizei const tex_heights[],
                   picture_t *pic, size_t const plane_offsets[])
{
    VLC_UNUSED(tex_widths);
    VLC_UNUSED(tex_heights);
    VLC_UNUSED(plane_offsets);

    vlc_vdp_output_surface_t *p_sys = pic->p_sys;
    GLvdpauSurfaceNV gl_nv_surface = p_sys->gl_nv_surface;

    static_assert (sizeof (gl_nv_surface) <= sizeof (p_sys->gl_nv_surface),
                   "Type too small");

    if (gl_nv_surface)
    {
        assert(_glVDPAUIsSurfaceNV(gl_nv_surface) == GL_TRUE);

        GLint state;
        GLsizei num_val;
        INTEROP_CALL(glVDPAUGetSurfaceivNV, gl_nv_surface,
                     GL_SURFACE_STATE_NV, 1, &num_val, &state);
        assert(num_val == 1); assert(state == GL_SURFACE_MAPPED_NV);

        INTEROP_CALL(glVDPAUUnmapSurfacesNV, 1, &gl_nv_surface);
        INTEROP_CALL(glVDPAUUnregisterSurfaceNV, gl_nv_surface);
    }

    gl_nv_surface =
        INTEROP_CALL(glVDPAURegisterOutputSurfaceNV,
                     (void *)(size_t)p_sys->surface,
                     GL_TEXTURE_2D, interop->tex_count, textures);
    INTEROP_CALL(glVDPAUSurfaceAccessNV, gl_nv_surface, GL_READ_ONLY);
    INTEROP_CALL(glVDPAUMapSurfacesNV, 1, &gl_nv_surface);

    p_sys->gl_nv_surface = gl_nv_surface;
    return VLC_SUCCESS;
}

static void
Close(struct vlc_gl_interop *interop)
{
    _glVDPAUFiniNV(); assert(interop->vt->GetError() == GL_NO_ERROR);
    converter_sys_t *sys = interop->priv;
    vlc_decoder_device *dec_device = sys->dec_device;
    vlc_decoder_device_Release(dec_device);
}

static int
Open(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *) obj;
    if (interop->vctx == NULL)
        return VLC_EGENERIC;
    vlc_decoder_device *dec_device = vlc_video_context_HoldDevice(interop->vctx);
    if (GetVDPAUOpaqueDevice(dec_device) == NULL
     || (interop->fmt_in.i_chroma != VLC_CODEC_VDPAU_VIDEO_420
      && interop->fmt_in.i_chroma != VLC_CODEC_VDPAU_VIDEO_422
      && interop->fmt_in.i_chroma != VLC_CODEC_VDPAU_VIDEO_444)
     || !vlc_gl_StrHasToken(interop->api->extensions, "GL_NV_vdpau_interop")
     || interop->gl->surface->type != VOUT_WINDOW_TYPE_XID)
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
    _##fct = vlc_gl_GetProcAddress(interop->gl, #fct); \
    if (!_##fct) \
    { \
        vlc_decoder_device_Release(dec_device); \
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

    INTEROP_CALL(glVDPAUInitNV, (void *)(uintptr_t)device, vdp_gpa);

    /* The pictures are uploaded upside-down */
    video_format_TransformBy(&interop->fmt_out, TRANSFORM_VFLIP);

    int ret = opengl_interop_init(interop, GL_TEXTURE_2D, VLC_CODEC_RGB32,
                                  COLOR_SPACE_UNDEF);
    if (ret != VLC_SUCCESS)
    {
        Close(interop);
        return VLC_EGENERIC;
    }

    static const struct vlc_gl_interop_ops ops = {
        .update_textures = tc_vdpau_gl_update,
        .close = Close,
    };
    interop->ops = &ops;
    interop->priv = sys;

    return VLC_SUCCESS;
}

static void
DecoderDeviceClose(vlc_decoder_device *device)
{
    vdpau_decoder_device_t *vdpau_dev = GetVDPAUOpaqueDevice(device);
    vdp_release_x11(vdpau_dev->vdp);
}

static const struct vlc_decoder_device_operations dev_ops = {
    .close = DecoderDeviceClose,
};

static int
DecoderDeviceOpen(vlc_decoder_device *device, vout_window_t *window)
{
    if (!window || !vlc_xlib_init(VLC_OBJECT(window)))
        return VLC_EGENERIC;

    vdpau_decoder_device_t *sys = vlc_obj_malloc(VLC_OBJECT(device), sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    if (vdp_get_x11(window->display.x11, -1,
                    &sys->vdp, &sys->device) != VDP_STATUS_OK)
    {
        vlc_obj_free(VLC_OBJECT(device), sys);
        return VLC_EGENERIC;
    }

    device->ops = &dev_ops;
    device->type = VLC_DECODER_DEVICE_VDPAU;
    device->opaque = sys;
    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_description("VDPAU OpenGL surface converter")
    set_capability("glinterop", 2)
    set_callback(Open)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("vdpau")
    add_submodule()
        set_callback_dec_device(DecoderDeviceOpen, 3)
vlc_module_end ()
