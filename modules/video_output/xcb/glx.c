/**
 * @file glx.c
 * @brief GLX video output module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2004 VLC authors and VideoLAN
 * Copyright © 2009 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include <xcb/xcb.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>
#include "../opengl.h"

#include "events.h"

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("GLX"))
    set_description (N_("OpenGL GLX video output (XCB)"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout display", 250)
    set_callbacks (Open, Close)

    add_shortcut ("xcb-glx", "glx", "opengl", "xid")
vlc_module_end ()

struct vout_display_sys_t
{
    xcb_connection_t *conn; /**< XCB connection */
    vlc_gl_t *gl;

    xcb_cursor_t cursor; /* blank cursor */
    bool visible; /* whether to draw */

    vout_display_opengl_t *vgl;
    picture_pool_t *pool; /* picture pool */
};

static picture_pool_t *Pool (vout_display_t *, unsigned);
static void PictureRender (vout_display_t *, picture_t *, subpicture_t *);
static void PictureDisplay (vout_display_t *, picture_t *, subpicture_t *);
static int Control (vout_display_t *, int, va_list);
static void Manage (vout_display_t *);

/**
 * Probe the X server.
 */
static int Open (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = malloc (sizeof (*sys));

    if (sys == NULL)
        return VLC_ENOMEM;

    sys->vgl = NULL;
    sys->pool = NULL;

    /* Get window, connect to X server (via XCB) */
    xcb_connection_t *conn;
    const xcb_screen_t *scr;
    vout_window_t *surface;

    surface = XCB_parent_Create (vd, &conn, &scr);
    if (surface == NULL)
    {
        free (sys);
        return VLC_EGENERIC;
    }

    sys->conn = conn;
    sys->gl = vlc_gl_Create (surface, VLC_OPENGL, "glx");
    if (sys->gl == NULL)
        goto error;

    const vlc_fourcc_t *spu_chromas;

    if (vlc_gl_MakeCurrent (sys->gl))
        goto error;

    sys->vgl = vout_display_opengl_New (&vd->fmt, &spu_chromas, sys->gl);
    vlc_gl_ReleaseCurrent (sys->gl);
    if (sys->vgl == NULL)
        goto error;

    sys->cursor = XCB_cursor_Create (conn, scr);
    sys->visible = false;

    /* Setup vout_display_t once everything is fine */
    vd->sys = sys;
    vd->info.has_pictures_invalid = false;
    vd->info.has_event_thread = true;
    vd->info.subpicture_chromas = spu_chromas;
    vd->pool = Pool;
    vd->prepare = PictureRender;
    vd->display = PictureDisplay;
    vd->control = Control;
    vd->manage = Manage;

    return VLC_SUCCESS;

error:
    if (sys->gl != NULL)
        vlc_gl_Destroy (sys->gl);
    xcb_disconnect (sys->conn);
    vout_display_DeleteWindow (vd, surface);
    free (sys);
    return VLC_EGENERIC;
}


/**
 * Disconnect from the X server.
 */
static void Close (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = vd->sys;
    vlc_gl_t *gl = sys->gl;
    vout_window_t *surface = gl->surface;

    vlc_gl_MakeCurrent (gl);
    vout_display_opengl_Delete (sys->vgl);
    vlc_gl_ReleaseCurrent (gl);
    vlc_gl_Destroy (gl);

    /* show the default cursor */
    xcb_change_window_attributes (sys->conn, surface->handle.xid,
                               XCB_CW_CURSOR, &(uint32_t) { XCB_CURSOR_NONE });
    xcb_flush (sys->conn);
    xcb_disconnect (sys->conn);

    vout_display_DeleteWindow (vd, surface);
    free (sys);
}

/**
 * Return a direct buffer
 */
static picture_pool_t *Pool (vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
    {
        vlc_gl_MakeCurrent (sys->gl);
        sys->pool = vout_display_opengl_GetPool (sys->vgl, requested_count);
        vlc_gl_ReleaseCurrent (sys->gl);
    }
    return sys->pool;
}

static void PictureRender (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    vlc_gl_MakeCurrent (sys->gl);
    vout_display_opengl_Prepare (sys->vgl, pic, subpicture);
    vlc_gl_ReleaseCurrent (sys->gl);
}

static void PictureDisplay (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    vlc_gl_MakeCurrent (sys->gl);
    vout_display_opengl_Display (sys->vgl, &vd->source);
    vlc_gl_ReleaseCurrent (sys->gl);

    picture_Release (pic);
    if (subpicture)
        subpicture_Delete(subpicture);
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    {
        const vout_display_cfg_t *cfg;
        const video_format_t *source;

        if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT
         || query == VOUT_DISPLAY_CHANGE_SOURCE_CROP)
        {
            source = (const video_format_t *)va_arg (ap, const video_format_t *);
            cfg = vd->cfg;
        }
        else
        {
            source = &vd->source;
            cfg = (const vout_display_cfg_t*)va_arg (ap, const vout_display_cfg_t *);
        }

        vout_display_place_t place;
        vout_display_PlacePicture (&place, source, cfg, false);

        vlc_gl_MakeCurrent (sys->gl);
        glViewport (place.x, place.y, place.width, place.height);
        vlc_gl_ReleaseCurrent (sys->gl);
        return VLC_SUCCESS;
    }

    /* Hide the mouse. It will be send when
     * vout_display_t::info.b_hide_mouse is false */
    case VOUT_DISPLAY_HIDE_MOUSE:
        xcb_change_window_attributes (sys->conn, sys->gl->surface->handle.xid,
                                    XCB_CW_CURSOR, &(uint32_t){ sys->cursor });
        xcb_flush (sys->conn);
        return VLC_SUCCESS;

    case VOUT_DISPLAY_RESET_PICTURES:
        vlc_assert_unreachable ();
    default:
        msg_Err (vd, "Unknown request in XCB vout display");
        return VLC_EGENERIC;
    }
}

static void Manage (vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    XCB_Manage (vd, sys->conn, &sys->visible);
}
