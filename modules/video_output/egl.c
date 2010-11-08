/**
 * @file egl.c
 * @brief EGL video output module
 */
/*****************************************************************************
 * Copyright © 2010 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <EGL/egl.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_vout_opengl.h>
#include "opengl.h"

#if USE_OPENGL_ES
# define VLC_API_NAME "OpenGL_ES"
# define VLC_API EGL_OPENGL_ES_API
# if USE_OPENGL_ES == 2
#  define VLC_RENDERABLE_BIT EGL_OPENGL_ES2_BIT
# else
#  define VLC_RENDERABLE_BIT EGL_OPENGL_ES_BIT
# endif
#else
# define VLC_API_NAME "OpenGL"
# define VLC_API EGL_OPENGL_API
# define VLC_RENDERABLE_BIT EGL_OPENGL_BIT
#endif

#ifdef __unix__
# include <dlfcn.h>
#endif

/* Plugin callbacks */
static int Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("EGL"))
    set_description (N_("EGL video output"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout display", 0)
    set_callbacks (Open, Close)
vlc_module_end ()

struct vout_display_sys_t
{
    EGLDisplay display;
    EGLSurface surface;

    vout_opengl_t gl;
    vout_display_opengl_t vgl;

    picture_pool_t *pool;
    vout_window_t *window;
};

/* Display callbacks */
static picture_pool_t *Pool (vout_display_t *, unsigned);
static void PictureRender (vout_display_t *, picture_t *);
static void PictureDisplay (vout_display_t *, picture_t *);
static int Control (vout_display_t *, int, va_list);
static void Manage (vout_display_t *);
/* OpenGL callbacks */
static void SwapBuffers (vout_opengl_t *gl);

static bool CheckAPI (EGLDisplay dpy, const char *api)
{
    const char *apis = eglQueryString (dpy, EGL_CLIENT_APIS);
    size_t apilen = strlen (api);

    /* Cannot use strtok_r() on constant string... */
    do
    {
        if (!strncmp (apis, api, apilen)
          && (memchr (" ", apis[apilen], 2) != NULL))
            return true;

        apis = strchr (apis, ' ');
    }
    while (apis != NULL);

    return false;
}

static vout_window_t *MakeWindow (vout_display_t *vd, EGLNativeWindowType *id)
{
    vout_window_cfg_t wnd_cfg;

    memset (&wnd_cfg, 0, sizeof (wnd_cfg));
#if defined (WIN32)
    wnd_cfg.type = VOUT_WINDOW_TYPE_HWND;
#elif defined (__unix__)
    wnd_cfg.type = VOUT_WINDOW_TYPE_XID;
#else
# error Unknown native window type!
#endif
    wnd_cfg.x = var_InheritInteger (vd, "video-x");
    wnd_cfg.y = var_InheritInteger (vd, "video-y");
    wnd_cfg.width  = vd->cfg->display.width;
    wnd_cfg.height = vd->cfg->display.height;

    vout_window_t *wnd = vout_display_NewWindow (vd, &wnd_cfg);
    if (wnd != NULL)
#if defined (WIN32)
        *id = wnd->handle.hwnd;
#elif defined (__unix__)
        *id = wnd->handle.xid;
#endif
    else
        msg_Err (vd, "parent window not available");
    return wnd;
}

/**
 * Probe EGL display availability
 */
static int Open (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;

    /* Initialize EGL display */
    /* TODO: support various display types */
    EGLDisplay dpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY)
        return VLC_EGENERIC;

    vout_display_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    vd->sys = sys;
    sys->display = dpy;
    sys->gl.sys = NULL;
    sys->pool = NULL;

    /* XXX Explicit hack!
     * Mesa EGL plugins (as of version 7.8.2) are not properly linked to
     * libEGL.so even though they import some of its symbols. This is
     * typically not a problem. Unfortunately, LibVLC loads plugins as
     * RTLD_LOCAL so that they do not pollute the namespace. Then the
     * libEGL symbols are not visible to EGL plugins, and the run-time
     * linker exits the whole process. */
#ifdef __unix__
    dlopen ("libEGL.so", RTLD_GLOBAL|RTLD_LAZY);
#endif

    EGLint major, minor;
    if (eglInitialize (dpy, &major, &minor) != EGL_TRUE)
    {
        /* No need to call eglTerminate() in this case */
        free (sys);
        return VLC_EGENERIC;
    }

    if (major != 1)
        goto abort;
#if USE_OPENGL_ES == 2
    if (minor < 3) /* Well well, this wouldn't compile with 1.2 anyway */
        goto abort;
#elif USE_OPENGL_ES == 0
    if (minor < 4)
        goto abort;
#endif

    if (!CheckAPI (dpy, VLC_API_NAME))
        goto abort;

    msg_Dbg (obj, "EGL version %s by %s", eglQueryString (dpy, EGL_VERSION),
             eglQueryString (dpy, EGL_VENDOR));
    {
        const char *ext = eglQueryString (dpy, EGL_EXTENSIONS);
        if (*ext)
            msg_Dbg (obj, " extensions: %s", ext);
    }

    static const EGLint conf_attr[] = {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE, 5,
        EGL_RENDERABLE_TYPE, VLC_RENDERABLE_BIT,
        EGL_NONE
    };
    EGLConfig cfgv[1];
    EGLint cfgc;

    if (eglChooseConfig (dpy, conf_attr, cfgv, 1, &cfgc) != EGL_TRUE
     || cfgc == 0)
        goto abort;

    /* Create a drawing surface */
    EGLNativeWindowType win;
    sys->window = MakeWindow (vd, &win);
    if (sys->window == NULL)
        goto abort;

    EGLSurface surface = eglCreateWindowSurface (dpy, cfgv[0], win, NULL);
    if (surface == EGL_NO_SURFACE)
    {
        msg_Err (obj, "cannot create EGL window surface");
        goto error;
    }
    sys->surface = surface;

    if (eglBindAPI (VLC_API) != EGL_TRUE)
    {
        msg_Err (obj, "cannot bind EGL API");
        goto error;
    }

    static const EGLint ctx_attr[] = {
#if USE_OPENGL_ES
        EGL_CONTEXT_CLIENT_VERSION, USE_OPENGL_ES,
#endif
        EGL_NONE
    };   

    EGLContext ctx = eglCreateContext (dpy, cfgv[0], EGL_NO_CONTEXT,
                                       ctx_attr);
    if (ctx == EGL_NO_CONTEXT)
    {
        msg_Err (obj, "cannot create EGL context");
        goto error;
    }

    if (eglMakeCurrent (dpy, surface, surface, ctx) != EGL_TRUE)
        goto error;

    /* Initialize OpenGL callbacks */
    sys->gl.lock = NULL;
    sys->gl.unlock = NULL;
    sys->gl.swap = SwapBuffers;
    sys->gl.sys = sys;

    if (vout_display_opengl_Init (&sys->vgl, &vd->fmt, &sys->gl))
        goto error;

    /* Initialize video display */
    vd->info.has_pictures_invalid = false;
    vd->info.has_event_thread = false;
    vd->pool = Pool;
    vd->prepare = PictureRender;
    vd->display = PictureDisplay;
    vd->control = Control;
    vd->manage = Manage;

    return VLC_SUCCESS;
error:
    vout_display_DeleteWindow (vd, sys->window);
abort:
    eglTerminate (dpy);
    free (sys);
    return VLC_EGENERIC;
}


/**
 * Destrisconnect from the X server.
 */
static void Close (vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = vd->sys;
    EGLDisplay dpy = sys->display;

    if (sys->gl.sys != NULL)
        vout_display_opengl_Clean (&sys->vgl);
    eglTerminate (dpy);
    vout_display_DeleteWindow (vd, sys->window);
    free (sys);
}

static void SwapBuffers (vout_opengl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;

    eglSwapBuffers (sys->display, sys->surface);
}

/**
 * Return a direct buffer
 */
static picture_pool_t *Pool (vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
        sys->pool = vout_display_opengl_GetPool (&sys->vgl);
    (void) count;
    return sys->pool;
}

static void PictureRender (vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Prepare (&sys->vgl, pic);
}

static void PictureDisplay (vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Display (&sys->vgl, &vd->source);
    picture_Release (pic);
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
      case VOUT_DISPLAY_HIDE_MOUSE:
      case VOUT_DISPLAY_RESET_PICTURES: // not needed?
          break;

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
        vout_opengl_t **gl = va_arg (ap, vout_opengl_t **);

        *gl = &sys->gl;
        return VLC_SUCCESS;
      }

      default:
        msg_Err (vd, "Unknown request %d", query);
    }
    return VLC_EGENERIC;
}

static void Manage (vout_display_t *vd)
{
    //vout_display_sys_t *sys = vd->sys;
    (void) vd;
}
