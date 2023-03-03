/*****************************************************************************
 * wgl.c: WGL extension for OpenGL
 *****************************************************************************
 * Copyright © 2009-2016 VLC authors and VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

#include "../opengl/vout_helper.h"
#include <GL/glew.h>
#include <GL/wglew.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int Open(vlc_gl_t *, unsigned width, unsigned height,
                const struct vlc_gl_cfg *gl_cfg);
static void Close(vlc_gl_t *);

#define HW_GPU_AFFINITY_TEXT N_("GPU affinity")

vlc_module_begin()
    set_shortname("WGL")
    set_description(N_("WGL extension for OpenGL"))
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_integer("gpu-affinity", -1, HW_GPU_AFFINITY_TEXT, NULL)

    set_callback_opengl(Open, 50)
    add_shortcut("wgl")
vlc_module_end()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

typedef struct vout_display_sys_t
{
    HWND                  hvideownd;
    HDC                   hGLDC;
    HGLRC                 hGLRC;
    HMODULE               hOpengl;
    vlc_gl_t              *gl;
    HDC                   affinityHDC; // DC for the selected GPU
} vout_display_sys_t;

static void          Swap(vlc_gl_t *);
static void          *OurGetProcAddress(vlc_gl_t *, const char *);
static int           MakeCurrent(vlc_gl_t *gl);
static void          ReleaseCurrent(vlc_gl_t *gl);

#define VLC_PFD_INITIALIZER { \
    .nSize = sizeof(PIXELFORMATDESCRIPTOR), \
    .nVersion = 1, \
    .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, \
    .iPixelType = PFD_TYPE_RGBA, \
    .cColorBits = 24, \
    .cDepthBits = 16, \
    .iLayerType = PFD_MAIN_PLANE, \
}

/* Create an GPU Affinity DC */
static void CreateGPUAffinityDC(vlc_gl_t *gl, UINT nVidiaAffinity) {
    vout_display_sys_t *sys = gl->sys;

    PIXELFORMATDESCRIPTOR pfd = VLC_PFD_INITIALIZER;

    /* create a temporary GL context */
    HDC winDC = GetDC(sys->hvideownd);
    SetPixelFormat(winDC, ChoosePixelFormat(winDC, &pfd), &pfd);
    HGLRC hGLRC = wglCreateContext(winDC);
    wglMakeCurrent(winDC, hGLRC);

    /* Initialize the necessary function pointers */
    PFNWGLENUMGPUSNVPROC fncEnumGpusNV = (PFNWGLENUMGPUSNVPROC)wglGetProcAddress("wglEnumGpusNV");
    PFNWGLCREATEAFFINITYDCNVPROC fncCreateAffinityDCNV = (PFNWGLCREATEAFFINITYDCNVPROC)wglGetProcAddress("wglCreateAffinityDCNV");

    /* delete the temporary GL context */
    wglMakeCurrent(NULL, NULL);
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
    sys->affinityHDC = fncCreateAffinityDCNV(GpuMask);
    if (sys->affinityHDC == NULL) return;
    SetPixelFormat(sys->affinityHDC,
        ChoosePixelFormat(sys->affinityHDC, &pfd), &pfd);

    msg_Dbg(gl, "GPU affinity set to adapter: %d", nVidiaAffinity);
}

/* Destroy an GPU Affinity DC */
static void DestroyGPUAffinityDC(vlc_gl_t *gl) {
    vout_display_sys_t *sys = gl->sys;

    if (sys->affinityHDC == NULL) return;

    PIXELFORMATDESCRIPTOR pfd = VLC_PFD_INITIALIZER;

    /* create a temporary GL context */
    HDC winDC = GetDC(sys->hvideownd);
    SetPixelFormat(winDC, ChoosePixelFormat(winDC, &pfd), &pfd);
    HGLRC hGLRC = wglCreateContext(winDC);
    wglMakeCurrent(winDC, hGLRC);

    /* Initialize the necessary function pointers */
    PFNWGLDELETEDCNVPROC fncDeleteDCNV = (PFNWGLDELETEDCNVPROC)wglGetProcAddress("wglDeleteDCNV");

    /* delete the temporary GL context */
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hGLRC);

    /* see if we have the extensions */
    if (!fncDeleteDCNV) return;

    /* delete the affinity DC */
    fncDeleteDCNV(sys->affinityHDC);
}

static int Open(vlc_gl_t *gl, unsigned width, unsigned height,
                const struct vlc_gl_cfg *gl_cfg)
{
    vout_display_sys_t *sys;

    if (gl_cfg->need_alpha)
    {
        msg_Err(gl, "Cannot support alpha yet");
        return VLC_ENOTSUP;
    }

    /* Allocate structure */
    gl->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    /* Process selected GPU affinity */
    int nVidiaAffinity = var_InheritInteger(gl, "gpu-affinity");
    if (nVidiaAffinity >= 0) CreateGPUAffinityDC(gl, nVidiaAffinity);

    vlc_window_t *wnd = gl->surface;
    if (wnd->type != VLC_WINDOW_TYPE_HWND || wnd->handle.hwnd == 0)
        goto error;

    sys->hvideownd = wnd->handle.hwnd;
    sys->hGLDC = GetDC(sys->hvideownd);
    sys->hOpengl = LoadLibraryA("opengl32.dll");
    if (sys->hGLDC == NULL)
    {
        msg_Err(gl, "Could not get the device context");
        goto error;
    }

    /* Set the pixel format for the DC */
    PIXELFORMATDESCRIPTOR pfd = VLC_PFD_INITIALIZER;
    SetPixelFormat(sys->hGLDC, ChoosePixelFormat(sys->hGLDC, &pfd), &pfd);

    /* Create the context. */
    sys->hGLRC = wglCreateContext((sys->affinityHDC != NULL) ? sys->affinityHDC : sys->hGLDC);
    if (sys->hGLRC == NULL)
    {
        msg_Err(gl, "Could not create the OpenGL rendering context");
        goto error;
    }

    wglMakeCurrent(sys->hGLDC, sys->hGLRC);
#if 0 /* TODO pick higher display depth if possible and the source requires it */
    int attribsDesired[] = {
        WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_RED_BITS_ARB, 10,
        WGL_GREEN_BITS_ARB, 10,
        WGL_BLUE_BITS_ARB, 10,
        WGL_ALPHA_BITS_ARB, 2,
        WGL_DOUBLE_BUFFER_ARB, 1,
        0,0
    };

    UINT nMatchingFormats;
    int index = 0;
    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB__ = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress( "wglChoosePixelFormatARB" );
    if (wglChoosePixelFormatARB__!= NULL)
        wglChoosePixelFormatARB__(sys->hGLDC, attribsDesired, NULL, 1, &index,  &nMatchingFormats);
#endif
#ifdef WGL_EXT_swap_control
    /* Create an GPU Affinity DC */
    const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (vlc_gl_StrHasToken(extensions, "WGL_EXT_swap_control")) {
        PFNWGLSWAPINTERVALEXTPROC SwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
        if (SwapIntervalEXT)
            SwapIntervalEXT(1);
    }
#endif

    wglMakeCurrent(sys->hGLDC, NULL);

    static const struct vlc_gl_operations gl_ops =
    {
        .make_current = MakeCurrent,
        .release_current = ReleaseCurrent,
        .resize = NULL,
        .swap = Swap,
        .get_proc_address = OurGetProcAddress,
        .close = Close,
    };
    gl->ops = &gl_ops;

    (void) width; (void) height;
    return VLC_SUCCESS;

error:
    Close(gl);
    return VLC_EGENERIC;
}

static void Close(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;

    if (sys->hGLRC)
        wglDeleteContext(sys->hGLRC);
    if (sys->hGLDC)
        ReleaseDC(sys->hvideownd, sys->hGLDC);
    if (sys->hOpengl)
        FreeLibrary(sys->hOpengl);

    DestroyGPUAffinityDC(gl);

    free(sys);
}

static void Swap(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;
    SwapBuffers(sys->hGLDC);
}

static void *OurGetProcAddress(vlc_gl_t *gl, const char *name)
{
    vout_display_sys_t *sys = gl->sys;

    /* See https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions */
    void *f= (void *)wglGetProcAddress(name);
    if(f == 0 || (f == (void*)0x1) || (f == (void*)0x2) ||
      (f == (void*)0x3) || (f == (void*)-1) )
    {
        f = (void *)GetProcAddress(sys->hOpengl, name);
    }
    return f;
}

static int MakeCurrent(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = gl->sys;

    /* After painting with a common DC, the ReleaseDC function must be called
     * to release the DC. Class and private DCs do not have to be released.
     * ReleaseDC must be called from the same thread that called GetDC. The
     * number of DCs is limited only by available memory. */

    HDC winDC = GetDC(sys->hvideownd);
    assert(winDC != NULL);

    bool success = wglMakeCurrent(winDC, sys->hGLRC);
    ReleaseDC(sys->hvideownd, winDC);

    if (likely(success))
        return VLC_SUCCESS;

    /* vlc_gl_MakeCurrent should never fail. */

    DWORD dw = GetLastError();
    msg_Err(gl, "Cannot make wgl current, error %lx", dw);

    return VLC_EGENERIC;
}

static void ReleaseCurrent(vlc_gl_t *gl)
{
    VLC_UNUSED(gl);
    wglMakeCurrent(NULL, NULL);
}
