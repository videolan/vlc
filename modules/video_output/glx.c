/**
 * @file glx.c
 * @brief GLX OpenGL extension module
 */
/*****************************************************************************
 * Copyright © 2010-2012 Rémi Denis-Courmont
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
#include <GL/glx.h>
#include <GL/glxext.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>
#include <vlc_vout_window.h>
#include <vlc_xlib.h>

#ifndef GLX_ARB_get_proc_address
#error GLX_ARB_get_proc_address extension missing
#endif

typedef struct vlc_gl_sys_t
{
    Display *display;
    GLXWindow win;
    GLXContext ctx;
} vlc_gl_sys_t;

static int MakeCurrent (vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;

    if (!glXMakeContextCurrent (sys->display, sys->win, sys->win, sys->ctx))
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void ReleaseCurrent (vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;

    glXMakeContextCurrent (sys->display, None, None, NULL);
}

static void SwapBuffers (vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;

    glXSwapBuffers (sys->display, sys->win);
}

static void *GetSymbol(vlc_gl_t *gl, const char *procname)
{
    (void) gl;
    return glXGetProcAddressARB ((const GLubyte *)procname);
}

static bool CheckGLX (vlc_object_t *vd, Display *dpy)
{
    int major, minor;
    bool ok = false;

    if (!glXQueryVersion (dpy, &major, &minor))
        msg_Dbg (vd, "GLX extension not available");
    else
    if (major != 1)
        msg_Dbg (vd, "GLX extension version %d.%d unknown", major, minor);
    else
    if (minor < 3)
        msg_Dbg (vd, "GLX extension version %d.%d too old", major, minor);
    else
    {
        msg_Dbg (vd, "using GLX extension version %d.%d", major, minor);
        ok = true;
    }
    return ok;
}

static bool CheckGLXext (Display *dpy, unsigned snum, const char *ext)
{
    const char *exts = glXQueryExtensionsString (dpy, snum);
    const size_t extlen = strlen (ext);

    while (*exts)
    {
        exts += strspn (exts, " ");

        size_t len = strcspn (exts, " ");
        if (len == extlen && !memcmp (exts, ext, extlen))
            return true;
        exts += len;
    }
    return false;
}

static void Close(vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;
    Display *dpy = sys->display;

    glXDestroyContext(dpy, sys->ctx);
    glXDestroyWindow(dpy, sys->win);
    XCloseDisplay(dpy);
    free(sys);
}

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    vlc_object_t *obj = VLC_OBJECT(gl);

    if (gl->surface->type != VOUT_WINDOW_TYPE_XID || !vlc_xlib_init (obj))
        return VLC_EGENERIC;

    /* Initialize GLX display */
    Display *dpy = XOpenDisplay (gl->surface->display.x11);
    if (dpy == NULL)
        return VLC_EGENERIC;

    vlc_gl_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
    {
        XCloseDisplay (dpy);
        return VLC_ENOMEM;
    }
    gl->sys = sys;
    sys->display = dpy;

    if (!CheckGLX (obj, dpy))
        goto error;

    /* Determine our pixel format */
    XWindowAttributes wa;
    if (!XGetWindowAttributes (dpy, gl->surface->handle.xid, &wa))
        goto error;

    const int snum = XScreenNumberOfScreen (wa.screen);
    const VisualID visual = XVisualIDFromVisual (wa.visual);
    static const int attr[] = {
        GLX_RED_SIZE, 5,
        GLX_GREEN_SIZE, 5,
        GLX_BLUE_SIZE, 5,
        GLX_DOUBLEBUFFER, True,
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        None
    };

    int nelem;
    GLXFBConfig *confs = glXChooseFBConfig (dpy, snum, attr, &nelem);
    if (confs == NULL)
    {
        msg_Err (obj, "cannot choose GLX frame buffer configurations");
        goto error;
    }

    GLXFBConfig conf;
    bool found = false;
    for (int i = 0; i < nelem && !found; i++)
    {
        conf = confs[i];

        XVisualInfo *vi = glXGetVisualFromFBConfig (dpy, conf);
        if (vi == NULL)
            continue;

        if (vi->visualid == visual)
            found = true;
        XFree (vi);
    }
    XFree (confs);

    if (!found)
    {
        msg_Err (obj, "cannot match GLX frame buffer configuration");
        goto error;
    }

    /* Create a drawing surface */
    sys->win = glXCreateWindow (dpy, conf, gl->surface->handle.xid, NULL);
    if (sys->win == None)
    {
        msg_Err (obj, "cannot create GLX window");
        goto error;
    }

    /* Create an OpenGL context */
    sys->ctx = glXCreateNewContext (dpy, conf, GLX_RGBA_TYPE, NULL, True);
    if (sys->ctx == NULL)
    {
        glXDestroyWindow (dpy, sys->win);
        msg_Err (obj, "cannot create GLX context");
        goto error;
    }

    /* Initialize OpenGL callbacks */
    gl->sys = sys;
    gl->make_current = MakeCurrent;
    gl->release_current = ReleaseCurrent;
    gl->resize = NULL;
    gl->swap = SwapBuffers;
    gl->get_proc_address = GetSymbol;
    gl->destroy = Close;

    bool is_swap_interval_set = false;

    MakeCurrent (gl);
# ifdef GLX_SGI_swap_control
    if (!is_swap_interval_set
     && CheckGLXext (dpy, snum, "GLX_SGI_swap_control"))
    {
        PFNGLXSWAPINTERVALSGIPROC SwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)
            glXGetProcAddressARB ((const GLubyte *)"glXSwapIntervalSGI");
        assert (SwapIntervalSGI != NULL);
        is_swap_interval_set = !SwapIntervalSGI (1);
    }
# endif
# ifdef GLX_EXT_swap_control
    if (!is_swap_interval_set
     && CheckGLXext (dpy, snum, "GLX_EXT_swap_control"))
    {
        PFNGLXSWAPINTERVALEXTPROC SwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)
            glXGetProcAddress ((const GLubyte *)"glXSwapIntervalEXT");
        assert (SwapIntervalEXT != NULL);
        SwapIntervalEXT (dpy, sys->win, 1);
        is_swap_interval_set = true;
    }
# endif
    ReleaseCurrent (gl);

    /* XXX: Prevent other gl backends (like EGL) to be opened within the same
     * X11 window instance. Indeed, using EGL after GLX on the same X11 window
     * instance leads to an SEGFAULT in the libEGL_nvidia.so library. */
    const char *vendor = glXGetClientString(dpy, GLX_VENDOR);
    if (vendor && strncmp(vendor, "NVIDIA", sizeof("NVIDIA") - 1) == 0)
    {
        var_Create(gl->surface, "gl", VLC_VAR_STRING);
        var_SetString(gl->surface, "gl", "glx");
    }

    (void) width; (void) height;
    return VLC_SUCCESS;

error:
    XCloseDisplay (dpy);
    free (sys);
    return VLC_EGENERIC;
}

vlc_module_begin ()
    set_shortname (N_("GLX"))
    set_description (N_("GLX extension for OpenGL"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("opengl", 20)
    set_callback(Open)
vlc_module_end ()
