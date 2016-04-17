/*****************************************************************************
 * glwin32.c: Windows OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include <windows.h>

#define GLEW_STATIC
#include "../opengl.h"
#include <GL/wglew.h>

#include "common.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define HW_GPU_AFFINITY_TEXT N_("GPU affinity")

vlc_module_begin()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_shortname("OpenGL")
    set_description(N_("OpenGL video output"))

    add_integer("gpu-affinity", -1, HW_GPU_AFFINITY_TEXT, HW_GPU_AFFINITY_TEXT, true)

    set_capability("vout display", 220)
    add_shortcut("glwin32", "opengl")
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Prepare(vout_display_t *, picture_t *, subpicture_t *);
static void           Display(vout_display_t *, picture_t *, subpicture_t *);
static void           Manage (vout_display_t *);

static void           Swap   (vlc_gl_t *);
static void          *OurGetProcAddress(vlc_gl_t *, const char *);

/* Create an GPU Affinity DC */
static void CreateGPUAffinityDC(vout_display_t *vd, UINT nVidiaAffinity) {
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    /* create a temporary GL context */
    HDC winDC = GetDC(vd->sys->hvideownd);
    SetPixelFormat(winDC, ChoosePixelFormat(winDC, &pfd), &pfd);
    HGLRC hGLRC = wglCreateContext(winDC);
    wglMakeCurrent(winDC, hGLRC);

    /* Initialize the neccessary function pointers */
    PFNWGLENUMGPUSNVPROC fncEnumGpusNV = (PFNWGLENUMGPUSNVPROC)wglGetProcAddress("wglEnumGpusNV");
    PFNWGLCREATEAFFINITYDCNVPROC fncCreateAffinityDCNV = (PFNWGLCREATEAFFINITYDCNVPROC)wglGetProcAddress("wglCreateAffinityDCNV");

    /* delete the temporary GL context */
    wglDeleteContext(hGLRC);

    /* see if we have the extensions */
    if (!fncEnumGpusNV || !fncCreateAffinityDCNV) return;

    /* find the graphics card */
    HGPUNV GpuMask[2];
    GpuMask[0] = NULL;
    GpuMask[1] = NULL;
    HGPUNV hGPU;
    if (!fncEnumGpusNV(nVidiaAffinity, &hGPU)) return;

    /* make the affinity DC */
    GpuMask[0] = hGPU;
    vd->sys->affinityHDC = fncCreateAffinityDCNV(GpuMask);
    if (vd->sys->affinityHDC == NULL) return;
    SetPixelFormat(vd->sys->affinityHDC,
        ChoosePixelFormat(vd->sys->affinityHDC, &pfd), &pfd);

    msg_Dbg( vd, "GPU affinity set to adapter: %d",
                     nVidiaAffinity );
}

/* Destroy an GPU Affinity DC */
static void DestroyGPUAffinityDC(vout_display_t *vd) {
    if (vd->sys->affinityHDC == NULL) return;

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    /* create a temporary GL context */
    HDC winDC = GetDC(vd->sys->hvideownd);
    SetPixelFormat(winDC, ChoosePixelFormat(winDC, &pfd), &pfd);
    HGLRC hGLRC = wglCreateContext(winDC);
    wglMakeCurrent(winDC, hGLRC);

    /* Initialize the neccessary function pointers */
    PFNWGLDELETEDCNVPROC fncDeleteDCNV = (PFNWGLDELETEDCNVPROC)wglGetProcAddress("wglDeleteDCNV");

    /* delete the temporary GL context */
    wglDeleteContext(hGLRC);

    /* see if we have the extensions */
    if (!fncDeleteDCNV) return;

    /* delete the affinity DC */
    fncDeleteDCNV(vd->sys->affinityHDC);
}

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

    /* process selected GPU affinity */
    int nVidiaAffinity = var_InheritInteger(vd, "gpu-affinity");
    if (nVidiaAffinity >= 0) CreateGPUAffinityDC(vd, nVidiaAffinity);

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

    /*
     * Create and enable the render context
     * For GPU affinity, attach the window DC
     * to the GPU affinity DC
     */
    sys->hGLRC = wglCreateContext((sys->affinityHDC != NULL) ? sys->affinityHDC : sys->hGLDC);
    wglMakeCurrent(sys->hGLDC, sys->hGLRC);

    const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
#ifdef WGL_EXT_swap_control
    if (HasExtension(extensions, "WGL_EXT_swap_control")) {
        PFNWGLSWAPINTERVALEXTPROC SwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
        if (SwapIntervalEXT)
            SwapIntervalEXT(1);
    }
#endif

    /* */
    sys->gl.swap = Swap;
    sys->gl.getProcAddress = OurGetProcAddress;
    sys->gl.sys = vd;

    video_format_t fmt = vd->fmt;
    const vlc_fourcc_t *subpicture_chromas;
    sys->vgl = vout_display_opengl_New(&fmt, &subpicture_chromas, &sys->gl);
    if (!sys->vgl)
        goto error;

    vout_display_info_t info = vd->info;
    info.has_double_click = true;
    info.has_hide_mouse = false;
    info.has_event_thread = true;
    info.subpicture_chromas = subpicture_chromas;

   /* Setup vout_display now that everything is fine */
    vd->fmt  = fmt;
    vd->info = info;

    vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = CommonControl;
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

    if (sys->vgl)
        vout_display_opengl_Delete(sys->vgl);

    if (sys->hGLDC && sys->hGLRC)
        wglMakeCurrent(NULL, NULL);
    if (sys->hGLRC)
        wglDeleteContext(sys->hGLRC);
    if (sys->hGLDC)
        ReleaseDC(sys->hvideownd, sys->hGLDC);
    DestroyGPUAffinityDC(vd);

    CommonClean(vd);

    free(sys);
}

/* */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
        sys->pool = vout_display_opengl_GetPool(sys->vgl, count);
    return sys->pool;
}

static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Prepare(sys->vgl, picture, subpicture);
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Display(sys->vgl, &vd->source);

    picture_Release(picture);
    if (subpicture)
        subpicture_Delete(subpicture);

    CommonDisplay(vd);
}

static void Manage (vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    CommonManage(vd);

    const int width  = sys->rect_dest.right  - sys->rect_dest.left;
    const int height = sys->rect_dest.bottom - sys->rect_dest.top;
    glViewport(0, 0, width, height);
}

static void Swap(vlc_gl_t *gl)
{
    vout_display_t *vd = gl->sys;

    SwapBuffers(vd->sys->hGLDC);
}

static void *OurGetProcAddress(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);
    return wglGetProcAddress(name);
}

