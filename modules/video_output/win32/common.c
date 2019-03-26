/*****************************************************************************
 * common.c: Windows video output common code
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Martell Malone <martellmalone@gmail.com>
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
 * Preamble: This file contains the functions related to the init of the vout
 *           structure, the common display code, the screensaver, but not the
 *           events and the Window Creation (events.c)
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout_display.h>

#include <windows.h>
#include <assert.h>

#define COBJMACROS
#include <shobjidl.h>

#include "common.h"
#include "../video_chroma/copy.h"

static bool GetExternalDimensions(void *opaque, UINT *width, UINT *height)
{
    const vout_display_t *vd = opaque;
    *width  = vd->source.i_visible_width;
    *height = vd->source.i_visible_height;
    return true;
}

void InitArea(vout_display_t *vd, display_win32_area_t *area, const vout_display_cfg_t *vdcfg)
{
    area->place_changed = false;
    area->pf_GetDisplayDimensions = GetExternalDimensions;
    area->opaque_dimensions = vd;
    area->vdcfg = *vdcfg;

    var_Create(vd, "disable-screensaver", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
}

#if !VLC_WINSTORE_APP
static void CommonChangeThumbnailClip(vlc_object_t *, vout_display_sys_win32_t *, bool show);

static bool GetWindowDimensions(void *opaque, UINT *width, UINT *height)
{
    const vout_display_sys_win32_t *sys = opaque;
    assert(sys != NULL);
    RECT out;
    if (!GetClientRect(sys->hwnd, &out))
        return false;
    *width  = RECTWidth(out);
    *height = RECTHeight(out);
    return true;
}

/* */
int CommonInit(vout_display_t *vd, display_win32_area_t *area, vout_display_sys_win32_t *sys)
{
    if (unlikely(area->vdcfg.window == NULL))
        return VLC_EGENERIC;

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    sys->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
#endif
    sys->hwnd      = NULL;
    sys->hvideownd = NULL;
    sys->hparent   = NULL;
    sys->hfswnd    = NULL;
    sys->is_first_placement = true;
    sys->is_on_top        = false;

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    sys->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
#endif
    area->pf_GetDisplayDimensions = GetWindowDimensions;
    area->opaque_dimensions = sys;

    var_Create(vd, "video-deco", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);

    /* */
    sys->event = EventThreadCreate(vd, area->vdcfg.window);
    if (!sys->event)
        return VLC_EGENERIC;

    vout_display_place_t original_place;
    vout_display_PlacePicture(&original_place, &vd->source, &area->vdcfg);
    EventThreadUpdatePlace(sys->event, &original_place);

    event_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
#ifdef MODULE_NAME_IS_direct3d9
    cfg.use_desktop = sys->use_desktop;
#endif
    cfg.x      = var_InheritInteger(vd, "video-x");
    cfg.y      = var_InheritInteger(vd, "video-y");
    cfg.width  = area->vdcfg.display.width;
    cfg.height = area->vdcfg.display.height;

    event_hwnd_t hwnd;
    if (EventThreadStart(sys->event, &hwnd, &cfg))
        return VLC_EGENERIC;

    sys->hparent       = hwnd.hparent;
    sys->hwnd          = hwnd.hwnd;
    sys->hvideownd     = hwnd.hvideownd;
    sys->hfswnd        = hwnd.hfswnd;

    return VLC_SUCCESS;
}
#endif /* !VLC_WINSTORE_APP */

/*****************************************************************************
* UpdateRects: update clipping rectangles
*****************************************************************************
* This function is called when the window position or size are changed, and
* its job is to update the source and destination RECTs used to display the
* picture.
*****************************************************************************/
void UpdateRects(vout_display_t *vd, display_win32_area_t *area, vout_display_sys_win32_t *sys)
{
    const video_format_t *source = &vd->source;

    UINT  display_width, display_height;

    /* Retrieve the window size */
    if (!area->pf_GetDisplayDimensions(area->opaque_dimensions, &display_width, &display_height))
    {
        msg_Err(vd, "could not get the window dimensions");
        return;
    }

    /* Update the window position and size */
    vout_display_cfg_t place_cfg = area->vdcfg;
    place_cfg.display.width = display_width;
    place_cfg.display.height = display_height;

#if (defined(MODULE_NAME_IS_glwin32))
    /* Reverse vertical alignment as the GL tex are Y inverted */
    if (place_cfg.align.vertical == VLC_VIDEO_ALIGN_TOP)
        place_cfg.align.vertical = VLC_VIDEO_ALIGN_BOTTOM;
    else if (place_cfg.align.vertical == VLC_VIDEO_ALIGN_BOTTOM)
        place_cfg.align.vertical = VLC_VIDEO_ALIGN_TOP;
#endif

    vout_display_place_t before_place = area->place;
    vout_display_PlacePicture(&area->place, source, &place_cfg);

    /* Signal the change in size/position */
    if (!vout_display_PlaceEquals(&before_place, &area->place))
    {
        area->place_changed |= true;

#ifndef NDEBUG
        msg_Dbg(vd, "DirectXUpdateRects source"
            " offset: %i,%i visible: %ix%i decoded: %ix%i",
            source->i_x_offset, source->i_y_offset,
            source->i_visible_width, source->i_visible_height,
            source->i_width, source->i_height);
        msg_Dbg(vd, "DirectXUpdateRects image_dst"
            " coords: %i,%i,%i,%i",
            area->place.x, area->place.y,
            area->place.x + area->place.width, area->place.y + area->place.height);
#endif

#if !VLC_WINSTORE_APP
        if (sys->event != NULL)
        {
            EventThreadUpdatePlace(sys->event, &area->place);

            if (sys->hvideownd)
            {
                UINT swpFlags = SWP_NOCOPYBITS | SWP_NOZORDER | SWP_ASYNCWINDOWPOS;
                if (sys->is_first_placement)
                {
                    swpFlags |= SWP_SHOWWINDOW;
                    sys->is_first_placement = false;
                }
                SetWindowPos(sys->hvideownd, 0,
                    area->place.x, area->place.y, area->place.width, area->place.height,
                    swpFlags);
            }

            CommonChangeThumbnailClip(VLC_OBJECT(vd), sys, true);
        }
#endif
    }
}

#if !VLC_WINSTORE_APP
/* */
void CommonClean(vlc_object_t *obj, vout_display_sys_win32_t *sys)
{
    if (sys->event) {
        CommonChangeThumbnailClip(obj, sys, false);
        EventThreadStop(sys->event);
        EventThreadDestroy(sys->event);
    }
}

void CommonManage(vout_display_t *vd, display_win32_area_t *area, vout_display_sys_win32_t *sys)
{
    if (sys->event != NULL && EventThreadGetAndResetSizeChanged(sys->event))
        UpdateRects(vd, area, sys);
}

/* */
static void CommonChangeThumbnailClip(vlc_object_t *obj, vout_display_sys_win32_t *sys, bool show)
{
    /* Windows 7 taskbar thumbnail code */
    OSVERSIONINFO winVer;
    winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!GetVersionEx(&winVer) || winVer.dwMajorVersion <= 5)
        return;

    if( FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) )
        vlc_assert_unreachable();

    void *ptr;
    if (S_OK == CoCreateInstance(&CLSID_TaskbarList,
                                 NULL, CLSCTX_INPROC_SERVER,
                                 &IID_ITaskbarList3,
                                 &ptr)) {
        ITaskbarList3 *taskbl = ptr;
        taskbl->lpVtbl->HrInit(taskbl);

        HWND hroot = GetAncestor(sys->hwnd,GA_ROOT);
        RECT video;
        if (show) {
            GetWindowRect(sys->hparent, &video);
            POINT client = {video.left, video.top};
            if (ScreenToClient(hroot, &client))
            {
                unsigned int width = RECTWidth(video);
                unsigned int height = RECTHeight(video);
                video.left = client.x;
                video.top = client.y;
                video.right = video.left + width;
                video.bottom = video.top + height;
            }
        }
        HRESULT hr;
        hr = taskbl->lpVtbl->SetThumbnailClip(taskbl, hroot,
                                                 show ? &video : NULL);
        if ( hr != S_OK )
            msg_Err(obj, "SetThumbNailClip failed: 0x%0lx", hr);

        taskbl->lpVtbl->Release(taskbl);
    }
    CoUninitialize();
}

static int CommonControlSetFullscreen(vlc_object_t *obj, vout_display_sys_win32_t *sys, bool is_fullscreen)
{
#ifdef MODULE_NAME_IS_direct3d9
    if (sys->use_desktop && is_fullscreen)
        return VLC_EGENERIC;
#endif

    /* */
    HWND hwnd = sys->hparent && sys->hfswnd ? sys->hfswnd : sys->hwnd;

    /* Save the current windows placement/placement to restore
       when fullscreen is over */
    WINDOWPLACEMENT window_placement;
    window_placement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hwnd, &window_placement);

    if (is_fullscreen) {
        msg_Dbg(obj, "entering fullscreen mode");

        /* Change window style, no borders and no title bar */
        SetWindowLong(hwnd, GWL_STYLE, WS_CLIPCHILDREN | WS_VISIBLE);

        if (sys->hparent) {
            /* Retrieve current window position so fullscreen will happen
            *on the right screen */
            HMONITOR hmon = MonitorFromWindow(sys->hparent,
                                              MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            mi.cbSize = sizeof(MONITORINFO);
            if (GetMonitorInfo(hmon, &mi))
                SetWindowPos(hwnd, 0,
                             mi.rcMonitor.left,
                             mi.rcMonitor.top,
                             RECTWidth(mi.rcMonitor),
                             RECTHeight(mi.rcMonitor),
                             SWP_NOZORDER|SWP_FRAMECHANGED);
        } else {
            /* Maximize non embedded window */
            ShowWindow(hwnd, SW_SHOWMAXIMIZED);
        }

        if (sys->hparent) {
            /* Hide the previous window */
            RECT rect;
            GetClientRect(hwnd, &rect);
            SetParent(sys->hwnd, hwnd);
            SetWindowPos(sys->hwnd, 0, 0, 0,
                         rect.right, rect.bottom,
                         SWP_NOZORDER|SWP_FRAMECHANGED);

            HWND topLevelParent = GetAncestor(sys->hparent, GA_ROOT);
            ShowWindow(topLevelParent, SW_HIDE);
        }
        SetForegroundWindow(hwnd);
    } else {
        msg_Dbg(obj, "leaving fullscreen mode");

        /* Change window style, no borders and no title bar */
        SetWindowLong(hwnd, GWL_STYLE, EventThreadGetWindowStyle(sys->event));

        if (sys->hparent) {
            RECT rect;
            GetClientRect(sys->hparent, &rect);
            SetParent(sys->hwnd, sys->hparent);
            SetWindowPos(sys->hwnd, 0, 0, 0,
                         rect.right, rect.bottom,
                         SWP_NOZORDER|SWP_FRAMECHANGED);

            HWND topLevelParent = GetAncestor(sys->hparent, GA_ROOT);
            ShowWindow(topLevelParent, SW_SHOW);
            SetForegroundWindow(sys->hparent);
            ShowWindow(hwnd, SW_HIDE);
        } else {
            /* return to normal window for non embedded vout */
            SetWindowPlacement(hwnd, &window_placement);
            ShowWindow(hwnd, SW_SHOWNORMAL);
        }
    }
    return VLC_SUCCESS;
}
#endif /* !VLC_WINSTORE_APP */

int CommonControl(vout_display_t *vd, display_win32_area_t *area, vout_display_sys_win32_t *sys, int query, va_list args)
{
    switch (query) {
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED: /* const vout_display_cfg_t *p_cfg */
    case VOUT_DISPLAY_CHANGE_ZOOM:           /* const vout_display_cfg_t *p_cfg */
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP: {
        area->vdcfg = *va_arg(args, const vout_display_cfg_t *);
        UpdateRects(vd, area, sys);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:   /* const vout_display_cfg_t *p_cfg */
    {   /* Update dimensions */
        area->vdcfg = *va_arg(args, const vout_display_cfg_t *);
#if !VLC_WINSTORE_APP
        if (!area->vdcfg.is_fullscreen && sys->event != NULL) {
            RECT rect_window = {
                .top    = 0,
                .left   = 0,
                .right  = area->vdcfg.display.width,
                .bottom = area->vdcfg.display.height,
            };
            AdjustWindowRect(&rect_window, EventThreadGetWindowStyle(sys->event), 0);
            SetWindowPos(sys->hwnd, 0, 0, 0,
                         RECTWidth(rect_window),
                         RECTHeight(rect_window), SWP_NOMOVE);
        }
#endif /* !VLC_WINSTORE_APP */
        UpdateRects(vd, area, sys);
        return VLC_SUCCESS;
    }
#if !VLC_WINSTORE_APP
    case VOUT_DISPLAY_CHANGE_WINDOW_STATE: {       /* unsigned state */
        const unsigned state = va_arg(args, unsigned);
        const bool is_on_top = (state & VOUT_WINDOW_STATE_ABOVE) != 0;
        if (sys->event != NULL)
        {
#ifdef MODULE_NAME_IS_direct3d9
            if (sys->use_desktop && is_on_top)
                return VLC_EGENERIC;
#endif
            HMENU hMenu = GetSystemMenu(sys->hwnd, FALSE);

            if (is_on_top && !(GetWindowLong(sys->hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
                CheckMenuItem(hMenu, IDM_TOGGLE_ON_TOP, MF_BYCOMMAND | MFS_CHECKED);
                SetWindowPos(sys->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
            } else if (!is_on_top && (GetWindowLong(sys->hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
                CheckMenuItem(hMenu, IDM_TOGGLE_ON_TOP, MF_BYCOMMAND | MFS_UNCHECKED);
                SetWindowPos(sys->hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
            }
            sys->is_on_top = is_on_top;
        }
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_FULLSCREEN: {
        bool fs = va_arg(args, int);
        if (sys->event != NULL)
        {
            if (CommonControlSetFullscreen(VLC_OBJECT(vd), sys, fs))
                return VLC_EGENERIC;
            UpdateRects(vd, area, sys);
        }
        return VLC_SUCCESS;
    }
#endif /* !VLC_WINSTORE_APP */

    case VOUT_DISPLAY_RESET_PICTURES:
        vlc_assert_unreachable();

    default:
        return VLC_EGENERIC;
    }
}
