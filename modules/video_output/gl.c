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
    set_capability ("vout display", /*165*/0)
    set_callbacks (Open, Close)
    add_shortcut ("opengles2", "gles2")
    add_module ("gles2", "opengl es2", NULL,
                GLES2_TEXT, PROVIDER_LONGTEXT, true)

#elif USE_OPENGL_ES == 1
# define API VLC_OPENGL_ES
# define MODULE_VARNAME "gles"
    set_shortname (N_("OpenGL ES"))
    set_description (N_("OpenGL for Embedded Systems video output"))
    set_capability ("vout display", /*160*/0)
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
    set_capability ("vout display", /*170*/0)
    set_callbacks (Open, Close)
    add_shortcut ("opengl", "gl")
    add_module ("gl", "opengl", NULL,
                GL_TEXT, PROVIDER_LONGTEXT, true)
#endif
vlc_module_end ()

struct vout_display_sys_t
{
    vout_display_opengl_t *vgl;

    vout_window_t *window;
    vlc_gl_t *gl;
    picture_pool_t *pool;
};

/* Display callbacks */
static picture_pool_t *Pool (vout_display_t *, unsigned);
static void PictureRender (vout_display_t *, picture_t *, subpicture_t *);
static void PictureDisplay (vout_display_t *, picture_t *, subpicture_t *);
static int Control (vout_display_t *, int, va_list);

static vout_window_t *MakeWindow (vout_display_t *vd)
{
    vout_window_cfg_t wnd_cfg;

    memset (&wnd_cfg, 0, sizeof (wnd_cfg));

    /* Please keep this in sync with egl.c */
    /* <EGL/eglplatform.h> defines the list and order of platforms */
#if defined(_WIN32) || defined(__VC32__) \
 && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
    wnd_cfg.type = VOUT_WINDOW_TYPE_HWND;
#elif defined(__WINSCW__) || defined(__SYMBIAN32__)  /* Symbian */
# warning Symbian not supported.
#elif defined(WL_EGL_PLATFORM)
# error Wayland not supported.
#elif defined(__GBM__)
# error Glamor not supported.
#elif defined(ANDROID)
# error Android not supported.
#elif defined(__unix__) /* X11 */
    wnd_cfg.type = VOUT_WINDOW_TYPE_XID;
#else
# error Platform not recognized.
#endif
    wnd_cfg.x = var_InheritInteger (vd, "video-x");
    wnd_cfg.y = var_InheritInteger (vd, "video-y");
    wnd_cfg.width  = vd->cfg->display.width;
    wnd_cfg.height = vd->cfg->display.height;

    vout_window_t *wnd = vout_display_NewWindow (vd, &wnd_cfg);
    if (wnd == NULL)
        msg_Err (vd, "parent window not available");
    return wnd;
}

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

    sys->window = MakeWindow (vd);
    if (sys->window == NULL)
        goto error;

    sys->gl = vlc_gl_Create (sys->window, API, "$" MODULE_VARNAME);
    if (sys->gl == NULL)
        goto error;

    if (vlc_gl_MakeCurrent (sys->gl))
        goto error;

    /* Initialize video display */
    const vlc_fourcc_t *spu_chromas;
    sys->vgl = vout_display_opengl_New (&vd->fmt, &spu_chromas, sys->gl);
    if (!sys->vgl)
    {
        vlc_gl_ReleaseCurrent (sys->gl);
        goto error;
    }

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
    if (sys->window != NULL)
        vout_display_DeleteWindow (vd, sys->window);
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

    vout_display_opengl_Delete (sys->vgl);
    vlc_gl_ReleaseCurrent (sys->gl);

    vlc_gl_Destroy (sys->gl);
    vout_display_DeleteWindow (vd, sys->window);
    free (sys);
}

/**
 * Returns picture buffers
 */
static picture_pool_t *Pool (vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
        sys->pool = vout_display_opengl_GetPool (sys->vgl, count);
    return sys->pool;
}

static void PictureRender (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Prepare (sys->vgl, pic, subpicture);
}

static void PictureDisplay (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Display (sys->vgl, &vd->source);
    picture_Release (pic);
    (void)subpicture;
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
        assert(0);
#endif
      case VOUT_DISPLAY_CHANGE_FULLSCREEN:
      {
        const vout_display_cfg_t *cfg =
            va_arg (ap, const vout_display_cfg_t *);

        return vout_window_SetFullScreen (sys->window, cfg->is_fullscreen);
      }

      case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
      {
        unsigned state = va_arg (ap, unsigned);

        return vout_window_SetState (sys->window, state);
      }

      case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
      case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
      case VOUT_DISPLAY_CHANGE_ZOOM:
      {
        const vout_display_cfg_t *cfg = va_arg (ap, const vout_display_cfg_t *);
        const video_format_t *src = &vd->source;

        if (query == VOUT_DISPLAY_CHANGE_DISPLAY_SIZE)
        {
            bool force = false;

            force = va_arg (ap, int);
            if (force
             && (cfg->display.width  != vd->cfg->display.width
              || cfg->display.height != vd->cfg->display.height)
             && vout_window_SetSize (sys->window,
                                     cfg->display.width, cfg->display.height))
                return VLC_EGENERIC;
        }

        vout_display_place_t place;

        vout_display_PlacePicture (&place, src, cfg, false);
        glViewport (0, 0, place.width, place.height);
        return VLC_SUCCESS;
      }

      case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
      case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
      {
        const vout_display_cfg_t *cfg = vd->cfg;
        const video_format_t *src = va_arg (ap, const video_format_t *);
        vout_display_place_t place;

        vout_display_PlacePicture (&place, src, cfg, false);
        glViewport (0, 0, place.width, place.height);
        return VLC_SUCCESS;
      }

      case VOUT_DISPLAY_GET_OPENGL:
      {
        vlc_gl_t **pgl = va_arg (ap, vlc_gl_t **);

        *pgl = sys->gl;
        return VLC_SUCCESS;
      }

      default:
        msg_Err (vd, "Unknown request %d", query);
    }
    return VLC_EGENERIC;
}
