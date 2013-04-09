/*****************************************************************************
 * direct2d.c : Direct2D video output plugin for vlc (Win7/Vista SP2 PF Update)
 *****************************************************************************
 * Copyright (C) 2010 VideoLAN and AUTHORS
 * $Id$
 *
 * Author: David Kaplan <david@2of1.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_vout_display.h>

#include <windows.h>
#include <commctrl.h>
#include <initguid.h>
#include <d2d1.h>

#include "common.h"


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define D2D_HELP N_("Video output for Windows 7/Windows Vista with Platform update")

vlc_module_begin ()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_help(D2D_HELP)
    set_shortname("Direct2D")
    set_description(N_("Direct2D video output"))
    set_capability("vout display", 180)
    add_shortcut("direct2d")
    set_callbacks(Open, Close)
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Prepare(vout_display_t *, picture_t *, subpicture_t *);
static void           Display(vout_display_t *, picture_t *, subpicture_t *);
static int            Control(vout_display_t *, int, va_list);
static void           Manage (vout_display_t *);

static int      D2D_CreateRenderTarget(vout_display_t *vd);
static void     D2D_ResizeRenderTarget(vout_display_t *vd);
static void     D2D_DestroyRenderTarget(vout_display_t *vd);

/**
 * Initialises Direct2D vout module
 */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->d2_render_target = NULL;

    sys->d2_dll = LoadLibrary(TEXT("D2D1.DLL"));
    if (!sys->d2_dll) {
        if (object->b_force)
            msg_Err(vd, "Cannot load D2D1.DLL, aborting");
        goto error;
    }

    D2D1_FACTORY_OPTIONS fo = {
        D2D1_DEBUG_LEVEL_NONE
    };

    HRESULT (WINAPI *D2D1CreateFactory)(D2D1_FACTORY_TYPE, REFIID,
                                        const D2D1_FACTORY_OPTIONS *,
                                        void **);

    D2D1CreateFactory = (void *)GetProcAddress(sys->d2_dll,
                                               "D2D1CreateFactory");
    if (!D2D1CreateFactory) {
        msg_Err(vd,
                "Cannot locate reference to a D2D1CreateFactory ABI in D2D1.DLL");
        goto error;
    }

#ifndef NDEBUG
    msg_Dbg(vd, "D2D1.DLL loaded");
#endif

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   (REFIID)&IID_ID2D1Factory,
                                   &fo,
                                   (void **)&sys->d2_factory);
    if (hr != S_OK) {
        msg_Err(vd, "Cannot create Direct2D factory (hr = 0x%x)!",
                (unsigned)hr);
        goto error;
    }

    if (CommonInit(vd))
        goto error;

    if (D2D_CreateRenderTarget(vd) != VLC_SUCCESS)
        goto error;

    vout_display_info_t info = vd->info;
    info.is_slow              = false;
    info.has_double_click     = true;
    info.has_hide_mouse       = false;
    info.has_pictures_invalid = false;
    vd->info = info;

    vd->fmt.i_chroma = VLC_CODEC_RGB32; /* masks change this to BGR32 for ID2D1Bitmap */
    vd->fmt.i_rmask  = 0x0000ff00;
    vd->fmt.i_gmask  = 0x00ff0000;
    vd->fmt.i_bmask  = 0xff000000;

    vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->manage  = Manage;
    vd->control = Control;

    EventThreadUpdateTitle(sys->event, VOUT_TITLE " (Direct2D output)");

#ifndef NDEBUG
    msg_Dbg(vd, "Ready");
#endif

    return VLC_SUCCESS;

error:
    Close(VLC_OBJECT(vd));
    return VLC_EGENERIC;
}

/**
 * Close Direct2D vout
 */
static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;

    D2D_DestroyRenderTarget(vd);

    if (vd->sys->pool)
        picture_pool_Delete(vd->sys->pool);

    CommonClean(vd);

    free(vd->sys);
}

/**
 * Handles pool allocations for bitmaps
 */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool) {
        sys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
#ifndef NDEBUG
        msg_Dbg(vd, "New picture pool created");
#endif
    }

    return sys->pool;
}

/**
 * Performs set up of ID2D1Bitmap memory ready for blitting
 */
static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->d2_render_target && sys->d2_bitmap) {

        HRESULT hr = ID2D1Bitmap_CopyFromMemory(sys->d2_bitmap,
                                                NULL /*&r_src*/,
                                                picture->p[0].p_pixels,
                                                picture->p[0].i_pitch);
        if (hr != S_OK)
            msg_Err(vd, "Failed to copy bitmap memory (hr = 0x%x)!",
                    (unsigned)hr);

#ifndef NDEBUG
        /*msg_Dbg(vd, "Bitmap dbg: target = %p, pitch = %d, bitmap = %p",
                sys->d2_render_target, pitch, sys->d2_bitmap);*/
#endif
    }
    VLC_UNUSED(subpicture);
}

/**
 * Blits a scaled picture_t to the render target
 */
static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    D2D1_RECT_F r_dest = {
        sys->rect_dest.left,
        sys->rect_dest.top,
        sys->rect_dest.right,
        sys->rect_dest.bottom
    };

    if (sys->d2_render_target && sys->d2_bitmap) {
        ID2D1HwndRenderTarget_BeginDraw(sys->d2_render_target);

        ID2D1HwndRenderTarget_DrawBitmap(sys->d2_render_target,
                                         sys->d2_bitmap,
                                         &r_dest,
                                         1.0f,
                                         D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                         NULL);

        HRESULT hr = ID2D1HwndRenderTarget_EndDraw(sys->d2_render_target,
                                                   NULL,
                                                   NULL);
        if (hr == D2DERR_RECREATE_TARGET) {
            D2D_DestroyRenderTarget(vd);
            D2D_CreateRenderTarget(vd);
        }
    }

    picture_Release(picture);
    VLC_UNUSED(subpicture);

    CommonDisplay(vd);
}

 /**
  * Control event handler
  */
static int Control(vout_display_t *vd, int query, va_list args)
{
    return CommonControl(vd, query, args);
}

/**
 * Handles surface management
 * ID2D1RenderTargets cannot be resized and must be recreated
 */
static void Manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    CommonManage(vd);

    if (sys->changes & DX_POSITION_CHANGE) {
        D2D_ResizeRenderTarget(vd);
        sys->changes &= ~DX_POSITION_CHANGE;
    }
}

/**
 * Creates a ID2D1HwndRenderTarget and associated ID2D1Bitmap
 */
static int D2D_CreateRenderTarget(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    sys->d2_render_target = NULL;

    D2D1_PIXEL_FORMAT pf = {
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D2D1_ALPHA_MODE_IGNORE
    };

    D2D1_RENDER_TARGET_PROPERTIES rtp = {
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        pf,
        0,
        0,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT
    };

    D2D1_SIZE_U size = {
        sys->rect_dest.right - sys->rect_dest.left,
        sys->rect_dest.bottom - sys->rect_dest.top
    };

    D2D1_HWND_RENDER_TARGET_PROPERTIES hrtp = {
        sys->hvideownd,
        size,
        D2D1_PRESENT_OPTIONS_IMMEDIATELY /* this might need fiddling */
    };

    HRESULT hr  = ID2D1Factory_CreateHwndRenderTarget(sys->d2_factory,
                                                      &rtp,
                                                      &hrtp,
                                                      &sys->d2_render_target);
    if (hr != S_OK) {
        msg_Err(vd, "Cannot create render target (hvideownd = 0x%x, width = %d, height = %d, pf.format = %d, hr = 0x%x)!",
                (unsigned)hrtp.hwnd, hrtp.pixelSize.width,
                hrtp.pixelSize.height, pf.format, (unsigned)hr);

        sys->d2_render_target = NULL;

        return VLC_EGENERIC;
    }

    FLOAT dpi_x, dpi_y;

    ID2D1Factory_GetDesktopDpi(sys->d2_factory,
                               &dpi_x,
                               &dpi_y);

    D2D1_BITMAP_PROPERTIES bp = {
        pf,
        dpi_x,
        dpi_y
    };

    D2D1_SIZE_U bitmap_size = {
        vd->fmt.i_width,
        vd->fmt.i_height
    };

    hr = ID2D1HwndRenderTarget_CreateBitmap(sys->d2_render_target,
                                            bitmap_size,
                                            NULL,
                                            0,
                                            &bp,
                                            &sys->d2_bitmap);
    if (hr != S_OK) {
        msg_Err(vd, "Failed to create bitmap (hr = 0x%x)!", (unsigned)hr);

        sys->d2_bitmap = NULL;
        D2D_DestroyRenderTarget(vd);

        return VLC_EGENERIC;
    }

#ifndef NDEBUG
    msg_Dbg(vd, "Render trgt dbg: dpi = %f, render_target = %p, bitmap = %p",
            dpi_x, sys->d2_render_target, sys->d2_bitmap);
#endif

    return VLC_SUCCESS;
}

/**
 * Resizes a ID2D1HWndRenderTarget
 */
static void D2D_ResizeRenderTarget(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    D2D1_SIZE_U size = {
        sys->rect_dest.right - sys->rect_dest.left,
        sys->rect_dest.bottom - sys->rect_dest.top
    };

    HRESULT hr  = ID2D1HwndRenderTarget_Resize(sys->d2_render_target, &size);
    if (hr != S_OK)
        msg_Err(vd, "Cannot resize render target (width = %d, height = %d, hr = 0x%x)!",
                size.width, size.height, (unsigned)hr);
}

/**
 * Cleans up ID2D1HwndRenderTarget and ID2D1Bitmap
 */
static void D2D_DestroyRenderTarget(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->d2_render_target) {
        ID2D1HwndRenderTarget_Release(sys->d2_render_target);
        sys->d2_render_target = NULL;
    }

    if (sys->d2_bitmap) {
        ID2D1Bitmap_Release(sys->d2_bitmap);
        sys->d2_bitmap = NULL;
    }

#ifndef NDEBUG
    msg_Dbg(vd, "Destroyed");
#endif
}
