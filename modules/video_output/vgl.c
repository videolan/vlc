/*****************************************************************************
 * vgl.c: virtual LibVLC OpenGL extension
 *****************************************************************************
 * Copyright (c) 2018 VLC authors and VideoLAN
 *
 * Authors: Pierre Lamot <pierre@videolabs.io>
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
#include <vlc_opengl.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif

struct vout_display_sys_t
{
    libvlc_video_output_cleanup_cb cleanupCb;
    libvlc_video_output_setup_cb setupCb;
    libvlc_video_update_output_cb resizeCb;
    libvlc_video_swap_cb swapCb;
    libvlc_video_makeCurrent_cb makeCurrentCb;
    libvlc_video_getProcAddress_cb getProcAddressCb;

    void* opaque;
    unsigned width;
    unsigned height;
};


static void *OurGetProcAddress(vlc_gl_t *gl, const char *name)
{
    vout_display_sys_t *sys = gl->sys;
    return sys->getProcAddressCb(sys->opaque, name);
}

static int MakeCurrent(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;
    bool success = sys->makeCurrentCb(sys->opaque, true);
    return success ? VLC_SUCCESS : VLC_EGENERIC;
}

static void ReleaseCurrent(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;
    sys->makeCurrentCb(sys->opaque, false);
}

static void VglSwapBuffers(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;
    sys->swapCb(sys->opaque);
}

static void Resize(vlc_gl_t * gl, unsigned w, unsigned h)
{
    vout_display_sys_t *sys = gl->sys;
    if( sys->width == w && sys->height == h )
        return;

    MakeCurrent(gl);
    libvlc_video_render_cfg_t output_cfg = {
        w, h, 8, true,
        libvlc_video_colorspace_BT709, libvlc_video_primaries_BT709,
        libvlc_video_transfer_func_SRGB, NULL,
    };
    libvlc_video_output_cfg_t render_cfg;
    sys->resizeCb(sys->opaque, &output_cfg, &render_cfg);
    ReleaseCurrent(gl);
    assert(render_cfg.opengl_format == GL_RGBA);
    assert(render_cfg.full_range == true);
    assert(render_cfg.colorspace == libvlc_video_colorspace_BT709);
    assert(render_cfg.primaries  == libvlc_video_primaries_BT709);
    assert(render_cfg.transfer   == libvlc_video_transfer_func_SRGB);
    sys->width = w;
    sys->height = h;
}

static void Close(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;
    if( sys->cleanupCb )
        sys->cleanupCb(sys->opaque);
}


#define SET_CALLBACK_ADDR(var, varname) \
    do {                                                           \
        var = var_InheritAddress(gl, varname);                     \
        if( !var ) {                                               \
            msg_Err( gl, "%s address is missing", varname );       \
            return VLC_EGENERIC;                                   \
        }                                                          \
    } while( 0 )

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    vout_display_sys_t * sys;

    libvlc_video_engine_t engineType = var_InheritInteger( gl, "vout-cb-type" );
    if ( engineType != libvlc_video_engine_opengl &&
         engineType != libvlc_video_engine_gles2 )
        return VLC_EBADVAR;

    /* Allocate structure */
    gl->sys = sys = vlc_obj_calloc(VLC_OBJECT(gl), 1, sizeof(*sys));
    if( !sys )
        return VLC_ENOMEM;

    sys->opaque = var_InheritAddress(gl, "vout-cb-opaque");
    sys->setupCb = var_InheritAddress(gl, "vout-cb-setup");
    sys->cleanupCb = var_InheritAddress(gl, "vout-cb-cleanup");
    SET_CALLBACK_ADDR(sys->resizeCb, "vout-cb-update-output");
    SET_CALLBACK_ADDR(sys->swapCb, "vout-cb-swap");
    SET_CALLBACK_ADDR(sys->makeCurrentCb, "vout-cb-make-current");
    SET_CALLBACK_ADDR(sys->getProcAddressCb, "vout-cb-get-proc-address");

    gl->make_current = MakeCurrent;
    gl->release_current = ReleaseCurrent;
    gl->resize = Resize;
    gl->swap = VglSwapBuffers;
    gl->get_proc_address = OurGetProcAddress;
    gl->destroy = Close;

    if( sys->setupCb )
    {
        libvlc_video_setup_device_cfg_t setup_cfg = {};
        libvlc_video_setup_device_info_t configured_cfg;
        if( !sys->setupCb(&sys->opaque, &setup_cfg, &configured_cfg) )
        {
            msg_Err( gl, "user setup failed" );
            return VLC_EGENERIC;
        }
    }

    Resize(gl, width, height);
    return VLC_SUCCESS;
}

#undef SET_CALLBACK_ADDR

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin()
    set_shortname("GL texture")
    set_description("GL texture output")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

    set_capability("opengl", 0)
    set_callback(Open)
    add_shortcut("vglmem")

    add_submodule()
    set_capability("opengl es2", 0)
    set_callback(Open)
    add_shortcut("vglmem")
vlc_module_end()
