/*****************************************************************************
 * common.c:
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


/*****************************************************************************
 * Preamble: This file contains the functions related to the creation of
 *             a window and the handling of its messages (events).
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>                                                 /* ENOMEM */
#include <ctype.h>                                              /* tolower() */

#ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x0500
#endif

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>
#include <vlc_vout_window.h>

#include <windows.h>
#include <tchar.h>
#include <windowsx.h>
#include <shellapi.h>

#ifdef MODULE_NAME_IS_directx
#include <ddraw.h>
#endif
#ifdef MODULE_NAME_IS_direct3d
#include <d3d9.h>
#endif
#ifdef MODULE_NAME_IS_glwin32
#include <GL/gl.h>
#endif

#include <vlc_keys.h>
#include "vout.h"

#ifndef UNDER_CE
#include <vlc_windows_interfaces.h>
#endif

#ifdef UNDER_CE
#include <aygshell.h>
    //WINSHELLAPI BOOL WINAPI SHFullScreen(HWND hwndRequester, DWORD dwState);
#endif

static int vaControlParentWindow( vout_thread_t *, int, va_list );

/* */
int CommonInit( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    p_sys->hwnd      = NULL;
    p_sys->hvideownd = NULL;
    p_sys->hparent   = NULL;
    p_sys->hfswnd    = NULL;
    p_sys->i_changes = 0;
    SetRectEmpty( &p_sys->rect_display );
    SetRectEmpty( &p_sys->rect_parent );
    vlc_mutex_init( &p_sys->lock );

    var_Create( p_vout, "video-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    /* Set main window's size */
    vout_window_cfg_t wnd_cfg;

    memset( &wnd_cfg, 0, sizeof(wnd_cfg) );
    wnd_cfg.type   = VOUT_WINDOW_TYPE_HWND;
    wnd_cfg.x      = 0;
    wnd_cfg.y      = 0;
    wnd_cfg.width  = p_vout->i_window_width;
    wnd_cfg.height = p_vout->i_window_height;

    p_sys->p_event = EventThreadCreate( p_vout, &wnd_cfg );
    if( !p_sys->p_event )
        return VLC_EGENERIC;

    event_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
#ifdef MODULE_NAME_IS_direct3d
    cfg.use_desktop = p_vout->p_sys->b_desktop;
#endif
#ifdef MODULE_NAME_IS_directx
    cfg.use_overlay = p_vout->p_sys->b_using_overlay;
#endif
    event_hwnd_t hwnd;
    if( EventThreadStart( p_sys->p_event, &hwnd, &cfg ) )
        return VLC_EGENERIC;

    p_sys->parent_window = hwnd.parent_window;
    p_sys->hparent       = hwnd.hparent;
    p_sys->hwnd          = hwnd.hwnd;
    p_sys->hvideownd     = hwnd.hvideownd;
    p_sys->hfswnd        = hwnd.hfswnd;

    /* Variable to indicate if the window should be on top of others */
    /* Trigger a callback right now */
    var_TriggerCallback( p_vout, "video-on-top" );

    /* Why not with glwin32 */
#if !defined(UNDER_CE) && !defined(MODULE_NAME_IS_glwin32)
    var_Create( p_vout, "disable-screensaver", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    DisableScreensaver ( p_vout );
#endif

    return VLC_SUCCESS;
}

/* */
void CommonClean( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    ExitFullscreen( p_vout );
    if( p_sys->p_event )
    {
        EventThreadStop( p_sys->p_event );
        EventThreadDestroy( p_sys->p_event );
    }

    vlc_mutex_destroy( &p_sys->lock );

#if !defined(UNDER_CE) && !defined(MODULE_NAME_IS_glwin32)
    RestoreScreensaver( p_vout );
#endif
}

void CommonManage( vout_thread_t *p_vout )
{
    /* If we do not control our window, we check for geometry changes
     * ourselves because the parent might not send us its events. */
    vlc_mutex_lock( &p_vout->p_sys->lock );
    if( p_vout->p_sys->hparent && !p_vout->b_fullscreen )
    {
        RECT rect_parent;
        POINT point;

        vlc_mutex_unlock( &p_vout->p_sys->lock );

        GetClientRect( p_vout->p_sys->hparent, &rect_parent );
        point.x = point.y = 0;
        ClientToScreen( p_vout->p_sys->hparent, &point );
        OffsetRect( &rect_parent, point.x, point.y );

        if( !EqualRect( &rect_parent, &p_vout->p_sys->rect_parent ) )
        {
            p_vout->p_sys->rect_parent = rect_parent;

            /* FIXME I find such #ifdef quite weirds. Are they really needed ? */

#if defined(MODULE_NAME_IS_direct3d)
            SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top,
                          SWP_NOZORDER );
            UpdateRects( p_vout, true );
#else
            /* This one is to force the update even if only
             * the position has changed */
            SetWindowPos( p_vout->p_sys->hwnd, 0, 1, 1,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, 0 );

            SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, 0 );

#if defined(MODULE_NAME_IS_wingdi) || defined(MODULE_NAME_IS_wingapi)
            unsigned int i_x, i_y, i_width, i_height;
            vout_PlacePicture( p_vout, rect_parent.right - rect_parent.left,
                               rect_parent.bottom - rect_parent.top,
                               &i_x, &i_y, &i_width, &i_height );

            SetWindowPos( p_vout->p_sys->hvideownd, HWND_TOP,
                          i_x, i_y, i_width, i_height, 0 );
#endif
#endif
        }
    }
    else
    {
        vlc_mutex_unlock( &p_vout->p_sys->lock );
    }

    /* */
    p_vout->p_sys->i_changes |= EventThreadRetreiveChanges( p_vout->p_sys->p_event );

    /* autoscale toggle */
    if( p_vout->i_changes & VOUT_SCALE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SCALE_CHANGE;

        p_vout->b_autoscale = var_GetBool( p_vout, "autoscale" );
        p_vout->i_zoom = (int) ZOOM_FP_FACTOR;

        UpdateRects( p_vout, true );
    }

    /* scaling factor */
    if( p_vout->i_changes & VOUT_ZOOM_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_ZOOM_CHANGE;

        p_vout->b_autoscale = false;
        p_vout->i_zoom =
            (int)( ZOOM_FP_FACTOR * var_GetFloat( p_vout, "scale" ) );
        UpdateRects( p_vout, true );
    }

    /* Check for cropping / aspect changes */
    if( p_vout->i_changes & VOUT_CROP_CHANGE ||
        p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;

        p_vout->fmt_out.i_x_offset = p_vout->fmt_in.i_x_offset;
        p_vout->fmt_out.i_y_offset = p_vout->fmt_in.i_y_offset;
        p_vout->fmt_out.i_visible_width = p_vout->fmt_in.i_visible_width;
        p_vout->fmt_out.i_visible_height = p_vout->fmt_in.i_visible_height;
        p_vout->fmt_out.i_aspect = p_vout->fmt_in.i_aspect;
        p_vout->fmt_out.i_sar_num = p_vout->fmt_in.i_sar_num;
        p_vout->fmt_out.i_sar_den = p_vout->fmt_in.i_sar_den;
        p_vout->output.i_aspect = p_vout->fmt_in.i_aspect;
        UpdateRects( p_vout, true );
    }

    /* We used to call the Win32 PeekMessage function here to read the window
     * messages. But since window can stay blocked into this function for a
     * long time (for example when you move your window on the screen), I
     * decided to isolate PeekMessage in another thread. */

    /*
     * Fullscreen change
     */
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE
        || p_vout->p_sys->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        Win32ToggleFullscreen( p_vout );

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
        p_vout->p_sys->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /*
     * Pointer change
     */
    EventThreadMouseAutoHide( p_vout->p_sys->p_event );

    /*
     * "Always on top" status change
     */
    if( p_vout->p_sys->b_on_top_change )
    {
        HMENU hMenu = GetSystemMenu( p_vout->p_sys->hwnd, FALSE );
        bool b = var_GetBool( p_vout, "video-on-top" );

        /* Set the window on top if necessary */
        if( b && !( GetWindowLong( p_vout->p_sys->hwnd, GWL_EXSTYLE )
                           & WS_EX_TOPMOST ) )
        {
            CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                           MF_BYCOMMAND | MFS_CHECKED );
            SetWindowPos( p_vout->p_sys->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOMOVE );
        }
        else
        /* The window shouldn't be on top */
        if( !b && ( GetWindowLong( p_vout->p_sys->hwnd, GWL_EXSTYLE )
                           & WS_EX_TOPMOST ) )
        {
            CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                           MF_BYCOMMAND | MFS_UNCHECKED );
            SetWindowPos( p_vout->p_sys->hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOMOVE );
        }

        p_vout->p_sys->b_on_top_change = false;
    }
}

/*****************************************************************************
 * UpdateRects: update clipping rectangles
 *****************************************************************************
 * This function is called when the window position or size are changed, and
 * its job is to update the source and destination RECTs used to display the
 * picture.
 *****************************************************************************/
void UpdateRects( vout_thread_t *p_vout, bool b_force )
{
#define rect_src p_vout->p_sys->rect_src
#define rect_src_clipped p_vout->p_sys->rect_src_clipped
#define rect_dest p_vout->p_sys->rect_dest
#define rect_dest_clipped p_vout->p_sys->rect_dest_clipped

    unsigned int i_width, i_height, i_x, i_y;

    RECT  rect;
    POINT point;

    /* Retrieve the window size */
    GetClientRect( p_vout->p_sys->hwnd, &rect );

    /* Retrieve the window position */
    point.x = point.y = 0;
    ClientToScreen( p_vout->p_sys->hwnd, &point );

    /* If nothing changed, we can return */
    bool b_changed;
    EventThreadUpdateWindowPosition( p_vout->p_sys->p_event, &b_changed,
                                     point.x, point.y,
                                     rect.right, rect.bottom );
    if( !b_force && !b_changed )
        return;

    /* Update the window position and size */
    vout_PlacePicture( p_vout, rect.right, rect.bottom,
                       &i_x, &i_y, &i_width, &i_height );

    if( p_vout->p_sys->hvideownd )
        SetWindowPos( p_vout->p_sys->hvideownd, 0,
                      i_x, i_y, i_width, i_height,
                      SWP_NOCOPYBITS|SWP_NOZORDER|SWP_ASYNCWINDOWPOS );

    /* Destination image position and dimensions */
    rect_dest.left = point.x + i_x;
    rect_dest.right = rect_dest.left + i_width;
    rect_dest.top = point.y + i_y;
    rect_dest.bottom = rect_dest.top + i_height;

#ifdef MODULE_NAME_IS_directx
    /* Apply overlay hardware constraints */
    if( p_vout->p_sys->b_using_overlay )
    {
        if( p_vout->p_sys->i_align_dest_boundary )
            rect_dest.left = ( rect_dest.left +
                p_vout->p_sys->i_align_dest_boundary / 2 ) &
                ~p_vout->p_sys->i_align_dest_boundary;

        if( p_vout->p_sys->i_align_dest_size )
            rect_dest.right = (( rect_dest.right - rect_dest.left +
                p_vout->p_sys->i_align_dest_size / 2 ) &
                ~p_vout->p_sys->i_align_dest_size) + rect_dest.left;
    }

    /* UpdateOverlay directdraw function doesn't automatically clip to the
     * display size so we need to do it otherwise it will fail */

    /* Clip the destination window */
    if( !IntersectRect( &rect_dest_clipped, &rect_dest,
                        &p_vout->p_sys->rect_display ) )
    {
        SetRectEmpty( &rect_src_clipped );
        return;
    }

#ifndef NDEBUG
    msg_Dbg( p_vout, "DirectXUpdateRects image_dst_clipped coords:"
                     " %li,%li,%li,%li",
                     rect_dest_clipped.left, rect_dest_clipped.top,
                     rect_dest_clipped.right, rect_dest_clipped.bottom );
#endif

#else /* MODULE_NAME_IS_directx */

    /* AFAIK, there are no clipping constraints in Direct3D, OpenGL and GDI */
    rect_dest_clipped = rect_dest;

#endif

    /* the 2 following lines are to fix a bug when clicking on the desktop */
    if( (rect_dest_clipped.right - rect_dest_clipped.left)==0 ||
        (rect_dest_clipped.bottom - rect_dest_clipped.top)==0 )
    {
        SetRectEmpty( &rect_src_clipped );
        return;
    }

    /* src image dimensions */
    rect_src.left = 0;
    rect_src.top = 0;
    rect_src.right = p_vout->render.i_width;
    rect_src.bottom = p_vout->render.i_height;

    /* Clip the source image */
    rect_src_clipped.left = p_vout->fmt_out.i_x_offset +
      (rect_dest_clipped.left - rect_dest.left) *
      p_vout->fmt_out.i_visible_width / (rect_dest.right - rect_dest.left);
    rect_src_clipped.right = p_vout->fmt_out.i_x_offset +
      p_vout->fmt_out.i_visible_width -
      (rect_dest.right - rect_dest_clipped.right) *
      p_vout->fmt_out.i_visible_width / (rect_dest.right - rect_dest.left);
    rect_src_clipped.top = p_vout->fmt_out.i_y_offset +
      (rect_dest_clipped.top - rect_dest.top) *
      p_vout->fmt_out.i_visible_height / (rect_dest.bottom - rect_dest.top);
    rect_src_clipped.bottom = p_vout->fmt_out.i_y_offset +
      p_vout->fmt_out.i_visible_height -
      (rect_dest.bottom - rect_dest_clipped.bottom) *
      p_vout->fmt_out.i_visible_height / (rect_dest.bottom - rect_dest.top);

#ifdef MODULE_NAME_IS_directx
    /* Apply overlay hardware constraints */
    if( p_vout->p_sys->b_using_overlay )
    {
        if( p_vout->p_sys->i_align_src_boundary )
            rect_src_clipped.left = ( rect_src_clipped.left +
                p_vout->p_sys->i_align_src_boundary / 2 ) &
                ~p_vout->p_sys->i_align_src_boundary;

        if( p_vout->p_sys->i_align_src_size )
            rect_src_clipped.right = (( rect_src_clipped.right -
                rect_src_clipped.left +
                p_vout->p_sys->i_align_src_size / 2 ) &
                ~p_vout->p_sys->i_align_src_size) + rect_src_clipped.left;
    }
#endif

#ifndef NDEBUG
    msg_Dbg( p_vout, "DirectXUpdateRects image_src_clipped"
                     " coords: %li,%li,%li,%li",
                     rect_src_clipped.left, rect_src_clipped.top,
                     rect_src_clipped.right, rect_src_clipped.bottom );
#endif

#ifdef MODULE_NAME_IS_directx
    /* The destination coordinates need to be relative to the current
     * directdraw primary surface (display) */
    rect_dest_clipped.left -= p_vout->p_sys->rect_display.left;
    rect_dest_clipped.right -= p_vout->p_sys->rect_display.left;
    rect_dest_clipped.top -= p_vout->p_sys->rect_display.top;
    rect_dest_clipped.bottom -= p_vout->p_sys->rect_display.top;

    if( p_vout->p_sys->b_using_overlay )
        DirectDrawUpdateOverlay( p_vout );
#endif

#ifndef UNDER_CE
    /* Windows 7 taskbar thumbnail code */
    LPTASKBARLIST3 p_taskbl;
    OSVERSIONINFO winVer;
    winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if( GetVersionEx(&winVer) && winVer.dwMajorVersion > 5 )
    {
        CoInitialize( 0 );

        if( S_OK == CoCreateInstance( &clsid_ITaskbarList,
                    NULL, CLSCTX_INPROC_SERVER,
                    &IID_ITaskbarList3,
                    &p_taskbl) )
        {
            RECT rect_video, rect_parent, rect_relative;
            HWND hroot = GetAncestor(p_vout->p_sys->hwnd,GA_ROOT);

            p_taskbl->vt->HrInit(p_taskbl);
            GetWindowRect(p_vout->p_sys->hvideownd, &rect_video);
            GetWindowRect(hroot, &rect_parent);
            rect_relative.left = rect_video.left - rect_parent.left - 8;
            rect_relative.right = rect_video.right - rect_video.left + rect_relative.left;
            rect_relative.top = rect_video.top - rect_parent.top - 10;
            rect_relative.bottom = rect_video.bottom - rect_video.top + rect_relative.top - 25;

            if (S_OK != p_taskbl->vt->SetThumbnailClip(p_taskbl, hroot, &rect_relative))
                msg_Err( p_vout, "SetThumbNailClip failed");

            p_taskbl->vt->Release(p_taskbl);
        }
        CoUninitialize();
    }
#endif
    /* Signal the change in size/position */
    p_vout->p_sys->i_changes |= DX_POSITION_CHANGE;

#undef rect_src
#undef rect_src_clipped
#undef rect_dest
#undef rect_dest_clipped
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    RECT rect_window;

    switch( i_query )
    {
    case VOUT_SET_SIZE:
        if( p_vout->p_sys->parent_window )
            return vaControlParentWindow( p_vout, i_query, args );

        /* Update dimensions */
        rect_window.top = rect_window.left = 0;
        rect_window.right  = va_arg( args, unsigned int );
        rect_window.bottom = va_arg( args, unsigned int );
        if( !rect_window.right ) rect_window.right = p_vout->i_window_width;
        if( !rect_window.bottom ) rect_window.bottom = p_vout->i_window_height;
        AdjustWindowRect( &rect_window, EventThreadGetWindowStyle( p_vout->p_sys->p_event ), 0 );

        SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                      rect_window.right - rect_window.left,
                      rect_window.bottom - rect_window.top, SWP_NOMOVE );

        return VLC_SUCCESS;

    case VOUT_SET_STAY_ON_TOP:
        if( p_vout->p_sys->hparent && !var_GetBool( p_vout, "fullscreen" ) )
            return vaControlParentWindow( p_vout, i_query, args );

        p_vout->p_sys->b_on_top_change = true;
        return VLC_SUCCESS;

    default:
        return VLC_EGENERIC;
    }
}


/* Internal wrapper over GetWindowPlacement */
static WINDOWPLACEMENT getWindowState(HWND hwnd)
{
    WINDOWPLACEMENT window_placement;
    window_placement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement( hwnd, &window_placement );
    return window_placement;
}

/* Internal wrapper to call vout_ControlWindow for hparent */
static int vaControlParentWindow( vout_thread_t *p_vout, int i_query,
                                   va_list args )
{
    switch( i_query )
    {
    case VOUT_SET_SIZE:
    {
        const unsigned i_width  = va_arg(args, unsigned);
        const unsigned i_height = va_arg(args, unsigned);
        return vout_window_SetSize( p_vout->p_sys->parent_window, i_width, i_height );
    }
    case VOUT_SET_STAY_ON_TOP:
    {
        const bool is_on_top = va_arg(args, int);
        return vout_window_SetOnTop( p_vout->p_sys->parent_window, is_on_top );
    }
    default:
        return VLC_EGENERIC;
    }
}

#if 0
static int ControlParentWindow( vout_thread_t *p_vout, int i_query, ... )
{
    va_list args;
    int ret;

    va_start( args, i_query );
    ret = vaControlParentWindow( p_vout, i_query, args );
    va_end( args );
    return ret;
}
#endif

void ExitFullscreen( vout_thread_t *p_vout )
{
    if( p_vout->b_fullscreen )
    {
        msg_Dbg( p_vout, "Quitting fullscreen" );
        Win32ToggleFullscreen( p_vout );
        /* Force fullscreen in the core for the next video */
        var_SetBool( p_vout, "fullscreen", true );
    }
}

void Win32ToggleFullscreen( vout_thread_t *p_vout )
{
    HWND hwnd = (p_vout->p_sys->hparent && p_vout->p_sys->hfswnd) ?
        p_vout->p_sys->hfswnd : p_vout->p_sys->hwnd;

    /* Save the current windows placement/placement to restore
       when fullscreen is over */
    WINDOWPLACEMENT window_placement = getWindowState( hwnd );

    p_vout->b_fullscreen = ! p_vout->b_fullscreen;

    /* We want to go to Fullscreen */
    if( p_vout->b_fullscreen )
    {
        msg_Dbg( p_vout, "entering fullscreen mode" );

        /* Change window style, no borders and no title bar */
        int i_style = WS_CLIPCHILDREN | WS_VISIBLE;
        SetWindowLong( hwnd, GWL_STYLE, i_style );

        if( p_vout->p_sys->hparent )
        {
#ifdef UNDER_CE
            POINT point = {0,0};
            RECT rect;
            ClientToScreen( p_vout->p_sys->hwnd, &point );
            GetClientRect( p_vout->p_sys->hwnd, &rect );
            SetWindowPos( hwnd, 0, point.x, point.y,
                          rect.right, rect.bottom,
                          SWP_NOZORDER|SWP_FRAMECHANGED );
#else
            /* Retrieve current window position so fullscreen will happen
            *on the right screen */
            HMONITOR hmon = MonitorFromWindow(p_vout->p_sys->hparent,
                                            MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            if (GetMonitorInfo(hmon, &mi))
            SetWindowPos( hwnd, 0,
                            mi.rcMonitor.left,
                            mi.rcMonitor.top,
                            mi.rcMonitor.right - mi.rcMonitor.left,
                            mi.rcMonitor.bottom - mi.rcMonitor.top,
                            SWP_NOZORDER|SWP_FRAMECHANGED );
#endif
        }
        else
        {
            /* Maximize non embedded window */
            ShowWindow( hwnd, SW_SHOWMAXIMIZED );
        }

        if( p_vout->p_sys->hparent )
        {
            /* Hide the previous window */
            RECT rect;
            GetClientRect( hwnd, &rect );
            SetParent( p_vout->p_sys->hwnd, hwnd );
            SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                          rect.right, rect.bottom,
                          SWP_NOZORDER|SWP_FRAMECHANGED );

#ifdef UNDER_CE
            HWND topLevelParent = GetParent( p_vout->p_sys->hparent );
#else
            HWND topLevelParent = GetAncestor( p_vout->p_sys->hparent, GA_ROOT );
#endif
            ShowWindow( topLevelParent, SW_HIDE );
        }

        SetForegroundWindow( hwnd );
    }
    else
    {
        msg_Dbg( p_vout, "leaving fullscreen mode" );
        /* Change window style, no borders and no title bar */
        SetWindowLong( hwnd, GWL_STYLE, EventThreadGetWindowStyle( p_vout->p_sys->p_event ) );

        if( p_vout->p_sys->hparent )
        {
            RECT rect;
            GetClientRect( p_vout->p_sys->hparent, &rect );
            SetParent( p_vout->p_sys->hwnd, p_vout->p_sys->hparent );
            SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                          rect.right, rect.bottom,
                          SWP_NOZORDER|SWP_FRAMECHANGED );

#ifdef UNDER_CE
            HWND topLevelParent = GetParent( p_vout->p_sys->hparent );
#else
            HWND topLevelParent = GetAncestor( p_vout->p_sys->hparent, GA_ROOT );
#endif
            ShowWindow( topLevelParent, SW_SHOW );
            SetForegroundWindow( p_vout->p_sys->hparent );
            ShowWindow( hwnd, SW_HIDE );
        }
        else
        {
            /* return to normal window for non embedded vout */
            SetWindowPlacement( hwnd, &window_placement );
            ShowWindow( hwnd, SW_SHOWNORMAL );
        }

        /* Make sure the mouse cursor is displayed */
        EventThreadMouseShow( p_vout->p_sys->p_event );
    }

    /* Update the object variable and trigger callback */
    var_SetBool( p_vout, "fullscreen", p_vout->b_fullscreen );
}

#ifndef UNDER_CE
void DisableScreensaver( vout_thread_t *p_vout )
{
    /* disable screensaver by temporarily changing system settings */
    p_vout->p_sys->i_spi_lowpowertimeout = 0;
    p_vout->p_sys->i_spi_powerofftimeout = 0;
    p_vout->p_sys->i_spi_screensavetimeout = 0;
    if( var_GetBool( p_vout, "disable-screensaver" ) )
    {
        msg_Dbg(p_vout, "disabling screen saver");
        SystemParametersInfo(SPI_GETLOWPOWERTIMEOUT,
            0, &(p_vout->p_sys->i_spi_lowpowertimeout), 0);
        if( 0 != p_vout->p_sys->i_spi_lowpowertimeout ) {
            SystemParametersInfo(SPI_SETLOWPOWERTIMEOUT, 0, NULL, 0);
        }
        SystemParametersInfo(SPI_GETPOWEROFFTIMEOUT, 0,
            &(p_vout->p_sys->i_spi_powerofftimeout), 0);
        if( 0 != p_vout->p_sys->i_spi_powerofftimeout ) {
            SystemParametersInfo(SPI_SETPOWEROFFTIMEOUT, 0, NULL, 0);
        }
        SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0,
            &(p_vout->p_sys->i_spi_screensavetimeout), 0);
        if( 0 != p_vout->p_sys->i_spi_screensavetimeout ) {
            SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, 0, NULL, 0);
        }
    }
}

void RestoreScreensaver( vout_thread_t *p_vout )
{
    /* restore screensaver system settings */
    if( 0 != p_vout->p_sys->i_spi_lowpowertimeout ) {
        SystemParametersInfo(SPI_SETLOWPOWERTIMEOUT,
            p_vout->p_sys->i_spi_lowpowertimeout, NULL, 0);
    }
    if( 0 != p_vout->p_sys->i_spi_powerofftimeout ) {
        SystemParametersInfo(SPI_SETPOWEROFFTIMEOUT,
            p_vout->p_sys->i_spi_powerofftimeout, NULL, 0);
    }
    if( 0 != p_vout->p_sys->i_spi_screensavetimeout ) {
        SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT,
            p_vout->p_sys->i_spi_screensavetimeout, NULL, 0);
    }
}
#endif

