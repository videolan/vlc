/*****************************************************************************
 * glwin32.c: Windows OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include <windows.h>
#include <ddraw.h>
#include <commctrl.h>

#include <multimon.h>
#undef GetSystemMetrics

#ifndef MONITOR_DEFAULTTONEAREST
#   define MONITOR_DEFAULTTONEAREST 2
#endif

#include "../opengl.h"
#include "common.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_shortname("OpenGL")
    set_description(N_("OpenGL video output"))
    set_capability("vout display", 20)
    add_shortcut("glwin32")
    add_shortcut("opengl")
    set_callbacks(Open, Close)

    /* FIXME: Hack to avoid unregistering our window class */
    linked_with_a_crap_library_which_uses_atexit ()
vlc_module_end()

#if 0 /* FIXME */
    /* check if we registered a window class because we need to
     * unregister it */
    WNDCLASS wndclass;
    if(GetClassInfo(GetModuleHandle(NULL), "VLC DirectX", &wndclass))
        UnregisterClass("VLC DirectX", GetModuleHandle(NULL));
#endif


/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static picture_t *Get    (vout_display_t *);
static void       Prepare(vout_display_t *, picture_t *);
static void       Display(vout_display_t *, picture_t *);
static int        Control(vout_display_t *, int, va_list);
static void       Manage (vout_display_t *);

static void       Swap   (vout_opengl_t *);

/**
 * It creates an OpenGL vout display.
 */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    /* */
    if (CommonInit(vd))
        goto error;

    EventThreadUpdateTitle(sys->event, VOUT_TITLE " (OpenGL output)");

    /* */
    sys->hGLDC = GetDC(sys->hvideownd);

    /* Set the pixel format for the DC */
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;
    SetPixelFormat(sys->hGLDC,
                   ChoosePixelFormat(sys->hGLDC, &pfd), &pfd);

    /* Create and enable the render context */
    sys->hGLRC = wglCreateContext(sys->hGLDC);
    wglMakeCurrent(sys->hGLDC, sys->hGLRC);

    /* */
    sys->gl.lock = NULL;
    sys->gl.unlock = NULL;
    sys->gl.swap = Swap;
    sys->gl.sys = vd;

    video_format_t fmt = vd->fmt;
    if (vout_display_opengl_Init(&sys->vgl, &fmt, &sys->gl))
        goto error;

    vout_display_info_t info = vd->info;
    info.has_double_click = true;
    info.has_hide_mouse = true;
    info.has_pictures_invalid = true;

   /* Setup vout_display now that everything is fine */
    vd->fmt  = fmt;
    vd->info = info;

    vd->get     = Get;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    return VLC_SUCCESS;

error:
    Close(object);
    return VLC_EGENERIC;
}

/**
 * It destroys an OpenGL vout display.
 */
static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    if (sys->vgl.gl)
        vout_display_opengl_Clean(&sys->vgl);

    if (sys->hGLDC && sys->hGLRC)
        wglMakeCurrent(NULL, NULL);
    if (sys->hGLRC)
        wglDeleteContext(sys->hGLRC);
    if (sys->hGLDC)
        ReleaseDC(sys->hvideownd, sys->hGLDC);

    CommonClean(vd);

    free(sys);
}

/* */
static picture_t *Get(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool) {
        sys->pool = vout_display_opengl_GetPool(&sys->vgl);
        if (!sys->pool)
            return NULL;
    }
    return picture_pool_Get(sys->pool);
}

static void Prepare(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Prepare(&sys->vgl, picture);
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Display(&sys->vgl, &vd->source);

    picture_Release(picture);

    CommonDisplay(vd);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    switch (query) {
    case VOUT_DISPLAY_GET_OPENGL: {
        vout_opengl_t **gl = va_arg(args, vout_opengl_t **);
        *gl = &vd->sys->gl;
        return VLC_SUCCESS;
    }
    default:
        return CommonControl(vd, query, args);
    }
}

static void Manage (vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    CommonManage(vd);

    const int width  = sys->rect_dest.right  - sys->rect_dest.left;
    const int height = sys->rect_dest.bottom - sys->rect_dest.top;
    glViewport(0, 0, width, height);
}

static void Swap(vout_opengl_t *gl)
{
    vout_display_t *vd = gl->sys;

    SwapBuffers(vd->sys->hGLDC);
}

