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
#include <EGL/eglext.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>
#include <vlc_vout_window.h>
#ifdef USE_PLATFORM_X11
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
    add_shortcut ("egl")

    add_submodule ()
    set_capability ("opengl es2", 50)
    set_callbacks (OpenGLES2, Close)
    add_shortcut ("egl")

    add_submodule ()
    set_capability ("opengl es", 50)
    set_callbacks (OpenGLES, Close)
    add_shortcut ("egl")

vlc_module_end ()

typedef struct vlc_gl_sys_t
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
#if defined (USE_PLATFORM_X11)
    Display *x11;
#endif
} vlc_gl_sys_t;

/* OpenGL callbacks */
static int MakeCurrent (vlc_gl_t *);
static void ReleaseCurrent (vlc_gl_t *);
static void SwapBuffers (vlc_gl_t *);
static void *GetSymbol(vlc_gl_t *, const char *);

static bool CheckToken(const char *haystack, const char *needle)
{
    size_t len = strlen(needle);

    while (haystack != NULL)
    {
        while (*haystack == ' ')
            haystack++;
        if (!strncmp(haystack, needle, len)
         && (memchr(" ", haystack[len], 2) != NULL))
            return true;

        haystack = strchr(haystack, ' ');
    }
    return false;
}

static bool CheckAPI (EGLDisplay dpy, const char *api)
{
    const char *apis = eglQueryString (dpy, EGL_CLIENT_APIS);
    return CheckToken(apis, api);
}

static bool CheckClientExt(const char *name)
{
    const char *exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    return CheckToken(exts, name);
}

struct gl_api
{
   const char name[10];
   EGLenum    api;
   EGLint     min_minor;
   EGLint     render_bit;
   EGLint     attr[3];
};

/* See http://www.khronos.org/registry/egl/api/EGL/eglplatform.h *
 * for list and order of default EGL platforms. */
#if defined (_WIN32) || defined (__VC32__) \
 && !defined (__CYGWIN__) && !defined (__SCITECH_SNAP__) /* Win32 and WinCE */
# define USE_DEFAULT_PLATFORM USE_PLATFORM_WIN32
#elif defined (__WINSCW__) || defined (__SYMBIAN32__)  /* Symbian */
# define USE_DEFAULT_PLATFORM USE_PLATFORM_SYMBIAN
#elif defined (__ANDROID__) || defined (ANDROID)
# define USE_DEFAULT_PLATFORM USE_PLATFORM_ANDROID
#elif defined (__unix__) /* X11 (tentative) */
# define USE_DEFAULT_PLATFORM USE_PLATFORM_X11
#endif

/**
 * Probe EGL display availability
 */
static int Open (vlc_object_t *obj, const struct gl_api *api)
{
    vlc_gl_t *gl = (vlc_gl_t *)obj;
    vout_window_t *wnd = gl->surface;
    union {
        void *ext_platform;
        EGLNativeWindowType native;
    } window;
#ifdef EGL_EXT_platform_base
    PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC createSurface = NULL;
#endif

    vlc_gl_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    gl->sys = sys;
    sys->display = EGL_NO_DISPLAY;
    sys->surface = EGL_NO_SURFACE;

#ifdef USE_PLATFORM_X11
    sys->x11 = NULL;

    if (wnd->type != VOUT_WINDOW_TYPE_XID || !vlc_xlib_init(obj))
        goto error;

    sys->x11 = XOpenDisplay(wnd->display.x11);
    if (sys->x11 == NULL)
        goto error;

    int snum;
    {
        XWindowAttributes wa;

        if (!XGetWindowAttributes(sys->x11, wnd->handle.xid, &wa))
            goto error;
        snum = XScreenNumberOfScreen(wa.screen);
    }
# ifdef EGL_EXT_platform_x11
    if (CheckClientExt("EGL_EXT_platform_x11"))
    {
        PFNEGLGETPLATFORMDISPLAYEXTPROC getDisplay;
        const EGLint attrs[] = {
            EGL_PLATFORM_X11_SCREEN_EXT, snum,
            EGL_NONE
        };

        getDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
            eglGetProcAddress("eglGetPlatformDisplayEXT");
        createSurface = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)
            eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
        sys->display = getDisplay(EGL_PLATFORM_X11_EXT, sys->x11, attrs);
        window.ext_platform = &wnd->handle.xid;
    }
    else
# endif
    {
# if USE_DEFAULT_PLATFORM
        if (snum == XDefaultScreen(sys->x11))
        {
            sys->display = eglGetDisplay(sys->x11);
            window.native = wnd->handle.xid;
        }
# endif
    }

#elif defined (USE_PLATFORM_WIN32)
    if (wnd->type != VOUT_WINDOW_TYPE_HWND)
        goto error;

# if USE_DEFAULT_PLATFORM
    sys->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    window.native = wnd->handle.hwnd;
# endif

#elif defined (USE_PLATFORM_ANDROID)
    if (wnd->type != VOUT_WINDOW_TYPE_ANDROID_NATIVE)
        goto error;

# if USE_DEFAULT_PLATFORM
    sys->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    window.native = wnd->handle.anativewindow;
# endif

#endif

    if (sys->display == EGL_NO_DISPLAY)
        goto error;

    /* Initialize EGL display */
    EGLint major, minor;
    if (eglInitialize(sys->display, &major, &minor) != EGL_TRUE)
        goto error;
    msg_Dbg(obj, "EGL version %s by %s",
            eglQueryString(sys->display, EGL_VERSION),
            eglQueryString(sys->display, EGL_VENDOR));

    const char *ext = eglQueryString(sys->display, EGL_EXTENSIONS);
    if (*ext)
        msg_Dbg(obj, " extensions: %s", ext);

    if (major != 1 || minor < api->min_minor
     || !CheckAPI(sys->display, api->name))
    {
        msg_Err(obj, "cannot select %s API", api->name);
        goto error;
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

    if (eglChooseConfig(sys->display, conf_attr, cfgv, 1, &cfgc) != EGL_TRUE
     || cfgc == 0)
    {
        msg_Err (obj, "cannot choose EGL configuration");
        goto error;
    }

    /* Create a drawing surface */
#ifdef EGL_EXT_platform_base
    if (createSurface != NULL)
        sys->surface = createSurface(sys->display, cfgv[0],
                                     window.ext_platform, NULL);
    else
#endif
        sys->surface = eglCreateWindowSurface(sys->display, cfgv[0],
                                              window.native, NULL);

    if (sys->surface == EGL_NO_SURFACE)
    {
        msg_Err (obj, "cannot create EGL window surface");
        goto error;
    }

    if (eglBindAPI (api->api) != EGL_TRUE)
    {
        msg_Err (obj, "cannot bind EGL API");
        goto error;
    }

    EGLContext ctx = eglCreateContext(sys->display, cfgv[0], EGL_NO_CONTEXT,
                                      api->attr);
    if (ctx == EGL_NO_CONTEXT)
    {
        msg_Err (obj, "cannot create EGL context");
        goto error;
    }
    sys->context = ctx;

    /* Initialize OpenGL callbacks */
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

    if (sys->display != EGL_NO_DISPLAY)
    {
        if (sys->surface != EGL_NO_SURFACE)
            eglDestroySurface(sys->display, sys->surface);
        eglTerminate(sys->display);
    }
#ifdef USE_PLATFORM_X11
    if (sys->x11 != NULL)
        XCloseDisplay(sys->x11);
#endif
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
