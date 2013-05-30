/**
 * @file egl.c
 * @brief EGL OpenGL extension module
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
#include <EGL/egl.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>
#include <vlc_vout_window.h>
#ifdef __unix__
# include <vlc_xlib.h>
#endif

/* Plugin callbacks */
static int OpenGLES2 (vlc_object_t *);
static int OpenGLES (vlc_object_t *);
static int OpenGL (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("EGL"))
    set_description (N_("EGL extension for OpenGL"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("opengl", 50)
    set_callbacks (OpenGL, Close)

    add_submodule ()
    set_capability ("opengl es2", 50)
    set_callbacks (OpenGLES2, Close)

    add_submodule ()
    set_capability ("opengl es", 50)
    set_callbacks (OpenGLES, Close)

vlc_module_end ()

typedef struct vlc_gl_sys_t
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
} vlc_gl_sys_t;

/* OpenGL callbacks */
static int MakeCurrent (vlc_gl_t *);
static void ReleaseCurrent (vlc_gl_t *);
static void SwapBuffers (vlc_gl_t *);
static void *GetSymbol(vlc_gl_t *, const char *);

static bool CheckAPI (EGLDisplay dpy, const char *api)
{
    const char *apis = eglQueryString (dpy, EGL_CLIENT_APIS);
    size_t apilen = strlen (api);

    /* Cannot use strtok_r() on constant string... */
    do
    {
        while (*apis == ' ')
            apis++;
        if (!strncmp (apis, api, apilen)
          && (memchr (" ", apis[apilen], 2) != NULL))
            return true;

        apis = strchr (apis, ' ');
    }
    while (apis != NULL);

    return false;
}

struct gl_api
{
   const char name[10];
   EGLenum    api;
   EGLint     min_minor;
   EGLint     render_bit;
   EGLint     attr[3];
};

/**
 * Probe EGL display availability
 */
static int Open (vlc_object_t *obj, const struct gl_api *api)
{
    vlc_gl_t *gl = (vlc_gl_t *)obj;

    /* <EGL/eglplatform.h> defines the list and order of platforms */
#if defined(_WIN32) || defined(__VC32__) \
 && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
# define vlc_eglGetWindow(w) ((w)->handle.hwnd)

#elif defined(__WINSCW__) || defined(__SYMBIAN32__)  /* Symbian */
# error Symbian EGL not supported.

#elif defined(WL_EGL_PLATFORM)
# error Wayland EGL not supported.

#elif defined(__GBM__)
# error Glamor EGL not supported.

#elif defined(ANDROID)
# error Android EGL not supported

#elif defined(__unix__) /* X11 */
# define vlc_eglGetWindow(w) ((w)->handle.xid)
    /* EGL can only use the default X11 display */
    if (gl->surface->display.x11 != NULL)
        return VLC_EGENERIC;
    if (!vlc_xlib_init (obj))
        return VLC_EGENERIC;

#else
# error EGL platform not recognized.
#endif

    /* Initialize EGL display */
    /* TODO: support various display types */
    EGLDisplay dpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY)
        return VLC_EGENERIC;

    vlc_gl_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    gl->sys = sys;
    sys->display = dpy;

    EGLint major, minor;
    if (eglInitialize (dpy, &major, &minor) != EGL_TRUE)
    {
        /* No need to call eglTerminate() in this case */
        free (sys);
        return VLC_EGENERIC;
    }

    if (major != 1 || minor < api->min_minor || !CheckAPI (dpy, api->name))
        goto error;

    msg_Dbg (obj, "EGL version %s by %s", eglQueryString (dpy, EGL_VERSION),
             eglQueryString (dpy, EGL_VENDOR));
    {
        const char *ext = eglQueryString (dpy, EGL_EXTENSIONS);
        if (*ext)
            msg_Dbg (obj, " extensions: %s", ext);
    }

    const EGLint conf_attr[] = {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE, 5,
        EGL_RENDERABLE_TYPE, api->render_bit,
        EGL_NONE
    };
    EGLConfig cfgv[1];
    EGLint cfgc;

    if (eglChooseConfig (dpy, conf_attr, cfgv, 1, &cfgc) != EGL_TRUE
     || cfgc == 0)
    {
        msg_Err (obj, "cannot choose EGL configuration");
        goto error;
    }

    /* Create a drawing surface */
    EGLNativeWindowType win = vlc_eglGetWindow(gl->surface);
    EGLSurface surface = eglCreateWindowSurface (dpy, cfgv[0], win, NULL);
    if (surface == EGL_NO_SURFACE)
    {
        msg_Err (obj, "cannot create EGL window surface");
        goto error;
    }
    sys->surface = surface;

    if (eglBindAPI (api->api) != EGL_TRUE)
    {
        msg_Err (obj, "cannot bind EGL API");
        goto error;
    }

    EGLContext ctx = eglCreateContext (dpy, cfgv[0], EGL_NO_CONTEXT,
                                       api->attr);
    if (ctx == EGL_NO_CONTEXT)
    {
        msg_Err (obj, "cannot create EGL context");
        goto error;
    }
    sys->context = ctx;

    /* Initialize OpenGL callbacks */
    gl->sys = sys;
    gl->makeCurrent = MakeCurrent;
    gl->releaseCurrent = ReleaseCurrent;
    gl->swap = SwapBuffers;
    gl->getProcAddress = GetSymbol;
    gl->lock = NULL;
    gl->unlock = NULL;
    return VLC_SUCCESS;

error:
    Close (obj);
    return VLC_EGENERIC;
}

static int OpenGLES2 (vlc_object_t *obj)
{
    static const struct gl_api api = {
        "OpenGL_ES", EGL_OPENGL_ES_API, 3, EGL_OPENGL_ES2_BIT,
        { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE },
    };
    return Open (obj, &api);
}

static int OpenGLES (vlc_object_t *obj)
{
    static const struct gl_api api = {
        "OpenGL_ES", EGL_OPENGL_ES_API, 0, EGL_OPENGL_ES_BIT,
        { EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE },
    };
    return Open (obj, &api);
}

static int OpenGL (vlc_object_t *obj)
{
    static const struct gl_api api = {
        "OpenGL", EGL_OPENGL_API, 4, EGL_OPENGL_BIT,
        { EGL_NONE },
    };
    return Open (obj, &api);
}

static void Close (vlc_object_t *obj)
{
    vlc_gl_t *gl = (vlc_gl_t *)obj;
    vlc_gl_sys_t *sys = gl->sys;

    eglTerminate (sys->display);
    free (sys);
}

static int MakeCurrent (vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;

    if (eglMakeCurrent (sys->display, sys->surface, sys->surface,
                        sys->context) != EGL_TRUE)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void ReleaseCurrent (vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;

    eglMakeCurrent (sys->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                    EGL_NO_CONTEXT);
}

static void SwapBuffers (vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;

    eglSwapBuffers (sys->display, sys->surface);
}

static void *GetSymbol(vlc_gl_t *gl, const char *procname)
{
    (void) gl;
    return (void *)eglGetProcAddress (procname);
}
