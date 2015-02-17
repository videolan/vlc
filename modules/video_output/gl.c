/**
 * @file gl.c
 * @brief OpenGL video output module
 */
/*****************************************************************************
 * Copyright © 2010-2011 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>
#include "opengl.h"

/* Plugin callbacks */
static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

#define GL_TEXT N_("OpenGL extension")
#define GLES2_TEXT N_("OpenGL ES 2 extension")
#define GLES_TEXT N_("OpenGL ES extension")
#define PROVIDER_LONGTEXT N_( \
    "Extension through which to use the Open Graphics Library (OpenGL).")

vlc_module_begin ()
#if USE_OPENGL_ES == 2
# define API VLC_OPENGL_ES2
# define MODULE_VARNAME "gles2"
    set_shortname (N_("OpenGL ES2"))
    set_description (N_("OpenGL for Embedded Systems 2 video output"))
    set_capability ("vout display", /*265*/0)
    set_callbacks (Open, Close)
    add_shortcut ("opengles2", "gles2")
    add_module ("gles2", "opengl es2", NULL,
                GLES2_TEXT, PROVIDER_LONGTEXT, true)

#elif USE_OPENGL_ES == 1
# define API VLC_OPENGL_ES
# define MODULE_VARNAME "gles"
    set_shortname (N_("OpenGL ES"))
    set_description (N_("OpenGL for Embedded Systems video output"))
    set_capability ("vout display", /*260*/0)
    set_callbacks (Open, Close)
    add_shortcut ("opengles", "gles")
    add_module ("gles", "opengl es", NULL,
                GLES_TEXT, PROVIDER_LONGTEXT, true)
#else
# define API VLC_OPENGL
# define MODULE_VARNAME "gl"
    set_shortname (N_("OpenGL"))
    set_description (N_("OpenGL video output (experimental)"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout display", /*270*/0)
    set_callbacks (Open, Close)
    add_shortcut ("opengl", "gl")
    add_module ("gl", "opengl", NULL,
                GL_TEXT, PROVIDER_LONGTEXT, true)
#endif
vlc_module_end ()

struct vout_display_sys_t
{
    vout_display_opengl_t *vgl;
    vlc_gl_t *gl;
    picture_pool_t *pool;
};

/* Display callbacks */
static picture_pool_t *Pool (vout_display_t *, unsigned);
static void PictureRender (vout_display_t *, picture_t *, subpicture_t *);
static void PictureDisplay (vout_display_t *, picture_t *, subpicture_t *);
static int Control (vout_display_t *, int, va_list);

/**
 * Allocates a surface and an OpenGL context for video output.
 */
static int Open (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->gl = NULL;
    sys->pool = NULL;

    vout_window_t *surface = vout_display_NewWindow (vd, VOUT_WINDOW_TYPE_INVALID);
    if (surface == NULL)
    {
        msg_Err (vd, "parent window not available");
        goto error;
    }

    sys->gl = vlc_gl_Create (surface, API, "$" MODULE_VARNAME);
    if (sys->gl == NULL)
        goto error;

    vlc_gl_Resize (sys->gl, vd->cfg->display.width, vd->cfg->display.height);

    /* Initialize video display */
    const vlc_fourcc_t *spu_chromas;

    if (vlc_gl_MakeCurrent (sys->gl))
        goto error;

    sys->vgl = vout_display_opengl_New (&vd->fmt, &spu_chromas, sys->gl);
    vlc_gl_ReleaseCurrent (sys->gl);

    if (sys->vgl == NULL)
        goto error;

    vd->sys = sys;
    vd->info.has_pictures_invalid = false;
    vd->info.has_event_thread = false;
    vd->info.subpicture_chromas = spu_chromas;
    vd->pool = Pool;
    vd->prepare = PictureRender;
    vd->display = PictureDisplay;
    vd->control = Control;
    vd->manage = NULL;
    return VLC_SUCCESS;

error:
    if (sys->gl != NULL)
        vlc_gl_Destroy (sys->gl);
    if (surface != NULL)
        vout_display_DeleteWindow (vd, surface);
    free (sys);
    return VLC_EGENERIC;
}

/**
 * Destroys the OpenGL context.
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
    vout_display_DeleteWindow (vd, surface);
    free (sys);
}

/**
 * Returns picture buffers
 */
static picture_pool_t *Pool (vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
    {
        vlc_gl_MakeCurrent (sys->gl);
        sys->pool = vout_display_opengl_GetPool (sys->vgl, count);
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
    (void) subpicture;
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
      case VOUT_DISPLAY_HIDE_MOUSE: /* FIXME TODO */
        break;
#ifndef NDEBUG
      case VOUT_DISPLAY_RESET_PICTURES: // not needed
        vlc_assert_unreachable();
#endif

      case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
      case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
      case VOUT_DISPLAY_CHANGE_ZOOM:
      {
        const vout_display_cfg_t *c = va_arg (ap, const vout_display_cfg_t *);
        const video_format_t *src = &vd->source;
        vout_display_place_t place;

        vout_display_PlacePicture (&place, src, c, false);
        vlc_gl_Resize (sys->gl, place.width, place.height);
        vlc_gl_MakeCurrent (sys->gl);
        glViewport (place.x, place.y, place.width, place.height);
        vlc_gl_ReleaseCurrent (sys->gl);
        return VLC_SUCCESS;
      }

      case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
      case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
      {
        const vout_display_cfg_t *cfg = vd->cfg;
        const video_format_t *src = va_arg (ap, const video_format_t *);
        vout_display_place_t place;

        vout_display_PlacePicture (&place, src, cfg, false);
        vlc_gl_MakeCurrent (sys->gl);
        glViewport (place.x, place.y, place.width, place.height);
        vlc_gl_ReleaseCurrent (sys->gl);
        return VLC_SUCCESS;
      }
      default:
        msg_Err (vd, "Unknown request %d", query);
    }
    return VLC_EGENERIC;
}
