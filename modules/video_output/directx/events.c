/*****************************************************************************
 * events.c: Windows DirectX video output events handler
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN (Centrale Réseaux) and its contributors
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


/*****************************************************************************
 * Preamble: This file contains the functions related to the creation of
 *             a window and the handling of its messages (events).
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <ctype.h>                                              /* tolower() */
#include <string.h>                                            /* strerror() */

#ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x0400
#endif

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/input.h>
#include <vlc/vout.h>

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <ddraw.h>

#include "vlc_keys.h"
#include "vout.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  DirectXCreateWindow( vout_thread_t *p_vout );
static void DirectXCloseWindow ( vout_thread_t *p_vout );
static long FAR PASCAL DirectXEventProc( HWND, UINT, WPARAM, LPARAM );

static int Control( vout_thread_t *p_vout, int i_query, va_list args );

static void DirectXPopupMenu( event_thread_t *p_event, vlc_bool_t b_open )
{
    playlist_t *p_playlist =
        vlc_object_find( p_event, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist != NULL )
    {
        vlc_value_t val;
        val.b_bool = b_open;
        var_Set( p_playlist, "intf-popupmenu", val );
        vlc_object_release( p_playlist );
    }
}

static int DirectXConvertKey( int i_key );

/*****************************************************************************
 * DirectXEventThread: Create video window & handle its messages
 *****************************************************************************
 * This function creates a video window and then enters an infinite loop
 * that handles the messages sent to that window.
 * The main goal of this thread is to isolate the Win32 PeekMessage function
 * because this one can block for a long time.
 *****************************************************************************/
void E_(DirectXEventThread)( event_thread_t *p_event )
{
    MSG msg;
    POINT old_mouse_pos = {0,0}, mouse_pos;
    vlc_value_t val;
    int i_width, i_height, i_x, i_y;
    HMODULE hkernel32;

    /* Initialisation */
    p_event->p_vout->pf_control = Control;

    /* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message queue */
    if( DirectXCreateWindow( p_event->p_vout ) )
    {
        msg_Err( p_event, "out of memory" );
        p_event->b_dead = VLC_TRUE;
    }

    /* Signal the creation of the window */
    vlc_thread_ready( p_event );

    /* Set power management stuff */
    if( (hkernel32 = GetModuleHandle( _T("KERNEL32") ) ) )
    {
        ULONG (WINAPI* OurSetThreadExecutionState)( ULONG );

        OurSetThreadExecutionState = (ULONG (WINAPI*)( ULONG ))
            GetProcAddress( hkernel32, _T("SetThreadExecutionState") );

        if( OurSetThreadExecutionState )
            /* Prevent monitor from powering off */
            OurSetThreadExecutionState( ES_DISPLAY_REQUIRED | ES_CONTINUOUS );
        else
            msg_Dbg( p_event, "no support for SetThreadExecutionState()" );
    }

    /* Main loop */
    /* GetMessage will sleep if there's no message in the queue */
    while( !p_event->b_die && GetMessage( &msg, 0, 0, 0 ) )
    {
        /* Check if we are asked to exit */
        if( p_event->b_die )
            break;

        switch( msg.message )
        {

        case WM_MOUSEMOVE:
            vout_PlacePicture( p_event->p_vout,
                               p_event->p_vout->p_sys->i_window_width,
                               p_event->p_vout->p_sys->i_window_height,
                               &i_x, &i_y, &i_width, &i_height );

            if( msg.hwnd == p_event->p_vout->p_sys->hvideownd )
            {
                /* Child window */
                i_x = i_y = 0;
            }

            if( i_width && i_height )
            {
                val.i_int = ( GET_X_LPARAM(msg.lParam) - i_x )
                             * p_event->p_vout->render.i_width / i_width;
                var_Set( p_event->p_vout, "mouse-x", val );
                val.i_int = ( GET_Y_LPARAM(msg.lParam) - i_y )
                             * p_event->p_vout->render.i_height / i_height;
                var_Set( p_event->p_vout, "mouse-y", val );

                val.b_bool = VLC_TRUE;
                var_Set( p_event->p_vout, "mouse-moved", val );
            }

        case WM_NCMOUSEMOVE:
            GetCursorPos( &mouse_pos );
            if( (abs(mouse_pos.x - old_mouse_pos.x) > 2 ||
                (abs(mouse_pos.y - old_mouse_pos.y)) > 2 ) )
            {
                GetCursorPos( &old_mouse_pos );
                p_event->p_vout->p_sys->i_lastmoved = mdate();

                if( p_event->p_vout->p_sys->b_cursor_hidden )
                {
                    p_event->p_vout->p_sys->b_cursor_hidden = 0;
                    ShowCursor( TRUE );
                }
            }
            break;

        case WM_VLC_HIDE_MOUSE:
            if( p_event->p_vout->p_sys->b_cursor_hidden ) break;
            p_event->p_vout->p_sys->b_cursor_hidden = VLC_TRUE;
            GetCursorPos( &old_mouse_pos );
            ShowCursor( FALSE );
            break;

        case WM_VLC_SHOW_MOUSE:
            if( !p_event->p_vout->p_sys->b_cursor_hidden ) break;
            p_event->p_vout->p_sys->b_cursor_hidden = VLC_FALSE;
            GetCursorPos( &old_mouse_pos );
            ShowCursor( TRUE );
            break;

        case WM_LBUTTONDOWN:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int |= 1;
            var_Set( p_event->p_vout, "mouse-button-down", val );
            DirectXPopupMenu( p_event, VLC_FALSE );
            break;

        case WM_LBUTTONUP:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int &= ~1;
            var_Set( p_event->p_vout, "mouse-button-down", val );

            val.b_bool = VLC_TRUE;
            var_Set( p_event->p_vout, "mouse-clicked", val );
            break;

        case WM_LBUTTONDBLCLK:
            p_event->p_vout->p_sys->i_changes |= VOUT_FULLSCREEN_CHANGE;
            break;

        case WM_MBUTTONDOWN:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int |= 2;
            var_Set( p_event->p_vout, "mouse-button-down", val );
            DirectXPopupMenu( p_event, VLC_FALSE );
            break;

        case WM_MBUTTONUP:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int &= ~2;
            var_Set( p_event->p_vout, "mouse-button-down", val );
            break;

        case WM_RBUTTONDOWN:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int |= 4;
            var_Set( p_event->p_vout, "mouse-button-down", val );
            DirectXPopupMenu( p_event, VLC_FALSE );
            break;

        case WM_RBUTTONUP:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int &= ~4;
            var_Set( p_event->p_vout, "mouse-button-down", val );
            DirectXPopupMenu( p_event, VLC_TRUE );
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            /* The key events are first processed here and not translated
             * into WM_CHAR events because we need to know the status of the
             * modifier keys. */
            val.i_int = DirectXConvertKey( msg.wParam );
            if( !val.i_int )
            {
                /* This appears to be a "normal" (ascii) key */
                val.i_int = tolower( MapVirtualKey( msg.wParam, 2 ) );
            }

            if( val.i_int )
            {
                if( GetKeyState(VK_CONTROL) & 0x8000 )
                {
                    val.i_int |= KEY_MODIFIER_CTRL;
                }
                if( GetKeyState(VK_SHIFT) & 0x8000 )
                {
                    val.i_int |= KEY_MODIFIER_SHIFT;
                }
                if( GetKeyState(VK_MENU) & 0x8000 )
                {
                    val.i_int |= KEY_MODIFIER_ALT;
                }

                var_Set( p_event->p_vlc, "key-pressed", val );
            }
            break;

        case WM_MOUSEWHEEL:
            if( GET_WHEEL_DELTA_WPARAM( msg.wParam ) > 0 )
            {
                val.i_int = KEY_MOUSEWHEELUP;
            }
            else
            {
                val.i_int = KEY_MOUSEWHEELDOWN;
            }
            if( val.i_int )
            {
                if( GetKeyState(VK_CONTROL) & 0x8000 )
                {
                    val.i_int |= KEY_MODIFIER_CTRL;
                }
                if( GetKeyState(VK_SHIFT) & 0x8000 )
                {
                    val.i_int |= KEY_MODIFIER_SHIFT;
                }
                if( GetKeyState(VK_MENU) & 0x8000 )
                {
                    val.i_int |= KEY_MODIFIER_ALT;
                }

                var_Set( p_event->p_vlc, "key-pressed", val );
            }
            break;

        case WM_VLC_CHANGE_TEXT:
            var_Get( p_event->p_vout, "video-title", &val );

            if( !val.psz_string || !*val.psz_string ) /* Default video title */
            {
#ifdef MODULE_NAME_IS_glwin32
                SetWindowText( p_event->p_vout->p_sys->hwnd,
                    _T(VOUT_TITLE) _T(" (OpenGL output)") );
#else
                if( p_event->p_vout->p_sys->b_using_overlay )
                    SetWindowText( p_event->p_vout->p_sys->hwnd, _T(VOUT_TITLE)
                        _T(" (hardware YUV overlay DirectX output)") );
                else if( p_event->p_vout->p_sys->b_hw_yuv )
                    SetWindowText( p_event->p_vout->p_sys->hwnd, _T(VOUT_TITLE)
                        _T(" (hardware YUV DirectX output)") );
                else
                    SetWindowText( p_event->p_vout->p_sys->hwnd, _T(VOUT_TITLE)
                        _T(" (software RGB DirectX output)") );
#endif
            }
            else
            {
                SetWindowText( p_event->p_vout->p_sys->hwnd, val.psz_string );
            }
            break;

        default:
            /* Messages we don't handle directly are dispatched to the
             * window procedure */
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            break;

        } /* End Switch */

    } /* End Main loop */

    /* Check for WM_QUIT if we created the window */
    if( !p_event->p_vout->p_sys->hparent && msg.message == WM_QUIT )
    {
        msg_Warn( p_event, "WM_QUIT... should not happen!!" );
        p_event->p_vout->p_sys->hwnd = NULL; /* Window already destroyed */
    }

    msg_Dbg( p_event, "DirectXEventThread terminating" );

    /* clear the changes formerly signaled */
    p_event->p_vout->p_sys->i_changes = 0;

    DirectXCloseWindow( p_event->p_vout );
}


/* following functions are local */

/*****************************************************************************
 * DirectXCreateWindow: create a window for the video.
 *****************************************************************************
 * Before creating a direct draw surface, we need to create a window in which
 * the video will be displayed. This window will also allow us to capture the
 * events.
 *****************************************************************************/
static int DirectXCreateWindow( vout_thread_t *p_vout )
{
    HINSTANCE  hInstance;
    HMENU      hMenu;
    RECT       rect_window;
    WNDCLASS   wc;                            /* window class components */
    HICON      vlc_icon = NULL;
    char       vlc_path[MAX_PATH+1];
    int        i_style, i_stylex;

    msg_Dbg( p_vout, "DirectXCreateWindow" );

    /* Get this module's instance */
    hInstance = GetModuleHandle(NULL);

    /* If an external window was specified, we'll draw in it. */
    p_vout->p_sys->hparent =
        vout_RequestWindow( p_vout, &p_vout->p_sys->i_window_x,
                            &p_vout->p_sys->i_window_y,
                            &p_vout->p_sys->i_window_width,
                            &p_vout->p_sys->i_window_height );

    /* We create the window ourself, there is no previous window proc. */
    p_vout->p_sys->pf_wndproc = NULL;

    /* Get the Icon from the main app */
    vlc_icon = NULL;
#ifndef UNDER_CE
    if( GetModuleFileName( NULL, vlc_path, MAX_PATH ) )
    {
        vlc_icon = ExtractIcon( hInstance, vlc_path, 0 );
    }
#endif

    /* Fill in the window class structure */
    wc.style         = CS_OWNDC|CS_DBLCLKS;          /* style: dbl click */
    wc.lpfnWndProc   = (WNDPROC)DirectXEventProc;       /* event handler */
    wc.cbClsExtra    = 0;                         /* no extra class data */
    wc.cbWndExtra    = 0;                        /* no extra window data */
    wc.hInstance     = hInstance;                            /* instance */
    wc.hIcon         = vlc_icon;                /* load the vlc big icon */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);    /* default cursor */
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);  /* background color */
    wc.lpszMenuName  = NULL;                                  /* no menu */
    wc.lpszClassName = _T("VLC DirectX");         /* use a special class */

    /* Register the window class */
    if( !RegisterClass(&wc) )
    {
        WNDCLASS wndclass;

        if( vlc_icon ) DestroyIcon( vlc_icon );

        /* Check why it failed. If it's because one already exists
         * then fine, otherwise return with an error. */
        if( !GetClassInfo( hInstance, _T("VLC DirectX"), &wndclass ) )
        {
            msg_Err( p_vout, "DirectXCreateWindow RegisterClass FAILED" );
            return VLC_EGENERIC;
        }
    }

    /* Register the video sub-window class */
    wc.lpszClassName = _T("VLC DirectX video"); wc.hIcon = 0;
    if( !RegisterClass(&wc) )
    {
        WNDCLASS wndclass;

        /* Check why it failed. If it's because one already exists
         * then fine, otherwise return with an error. */
        if( !GetClassInfo( hInstance, _T("VLC DirectX video"), &wndclass ) )
        {
            msg_Err( p_vout, "DirectXCreateWindow RegisterClass FAILED" );
            return VLC_EGENERIC;
        }
    }

    /* When you create a window you give the dimensions you wish it to
     * have. Unfortunatly these dimensions will include the borders and
     * titlebar. We use the following function to find out the size of
     * the window corresponding to the useable surface we want */
    rect_window.top    = 10;
    rect_window.left   = 10;
    rect_window.right  = rect_window.left + p_vout->p_sys->i_window_width;
    rect_window.bottom = rect_window.top + p_vout->p_sys->i_window_height;

    if( var_GetBool( p_vout, "video-deco" ) )
    {
        /* Open with window decoration */
        AdjustWindowRect( &rect_window, WS_OVERLAPPEDWINDOW|WS_SIZEBOX, 0 );
        i_style = WS_OVERLAPPEDWINDOW|WS_SIZEBOX|WS_VISIBLE|WS_CLIPCHILDREN;
        i_stylex = 0;
    }
    else
    {
        /* No window decoration */
        AdjustWindowRect( &rect_window, WS_POPUP, 0 );
        i_style = WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN;
        i_stylex = 0; // WS_EX_TOOLWINDOW; Is TOOLWINDOW really needed ?
                      // It messes up the fullscreen window.
    }

    if( p_vout->p_sys->hparent )
    {
        i_style = WS_VISIBLE|WS_CLIPCHILDREN|WS_CHILD;
        i_stylex = 0;
    }

    p_vout->p_sys->i_window_style = i_style;

    /* Create the window */
    p_vout->p_sys->hwnd =
        CreateWindowEx( WS_EX_NOPARENTNOTIFY | i_stylex,
                    _T("VLC DirectX"),               /* name of window class */
                    _T(VOUT_TITLE) _T(" (DirectX Output)"),  /* window title */
                    i_style,                                 /* window style */
                    (p_vout->p_sys->i_window_x < 0) ? CW_USEDEFAULT :
                        p_vout->p_sys->i_window_x,   /* default X coordinate */
                    (p_vout->p_sys->i_window_y < 0) ? CW_USEDEFAULT :
                        p_vout->p_sys->i_window_y,   /* default Y coordinate */
                    rect_window.right - rect_window.left,    /* window width */
                    rect_window.bottom - rect_window.top,   /* window height */
                    p_vout->p_sys->hparent,                 /* parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    (LPVOID)p_vout );            /* send p_vout to WM_CREATE */

    if( !p_vout->p_sys->hwnd )
    {
        msg_Warn( p_vout, "DirectXCreateWindow create window FAILED" );
        return VLC_EGENERIC;
    }

    if( p_vout->p_sys->hparent )
    {
        LONG i_style;

        /* We don't want the window owner to overwrite our client area */
        i_style = GetWindowLong( p_vout->p_sys->hparent, GWL_STYLE );

        if( !(i_style & WS_CLIPCHILDREN) )
            /* Hmmm, apparently this is a blocking call... */
            SetWindowLong( p_vout->p_sys->hparent, GWL_STYLE,
                           i_style | WS_CLIPCHILDREN );

        /* Create our fullscreen window */
        p_vout->p_sys->hfswnd =
            CreateWindowEx( WS_EX_APPWINDOW, _T("VLC DirectX"),
                            _T(VOUT_TITLE) _T(" (DirectX Output)"),
                            WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_SIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            NULL, NULL, hInstance, NULL );
    }

    /* Now display the window */
    ShowWindow( p_vout->p_sys->hwnd, SW_SHOW );

    /* Create video sub-window. This sub window will always exactly match
     * the size of the video, which allows us to use crazy overlay colorkeys
     * without having them shown outside of the video area. */
    SendMessage( p_vout->p_sys->hwnd, WM_VLC_CREATE_VIDEO_WIN, 0, 0 );

    /* Append a "Always On Top" entry in the system menu */
    hMenu = GetSystemMenu( p_vout->p_sys->hwnd, FALSE );
    AppendMenu( hMenu, MF_SEPARATOR, 0, _T("") );
    AppendMenu( hMenu, MF_STRING | MF_UNCHECKED,
                       IDM_TOGGLE_ON_TOP, _T("Always on &Top") );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DirectXCloseWindow: close the window created by DirectXCreateWindow
 *****************************************************************************
 * This function returns all resources allocated by DirectXCreateWindow.
 *****************************************************************************/
static void DirectXCloseWindow( vout_thread_t *p_vout )
{
    msg_Dbg( p_vout, "DirectXCloseWindow" );

    DestroyWindow( p_vout->p_sys->hwnd );
    if( p_vout->p_sys->hfswnd ) DestroyWindow( p_vout->p_sys->hfswnd );

    if( p_vout->p_sys->hparent )
        vout_ReleaseWindow( p_vout, (void *)p_vout->p_sys->hparent );

    p_vout->p_sys->hwnd = NULL;

    /* We don't unregister the Window Class because it could lead to race
     * conditions and it will be done anyway by the system when the app will
     * exit */
}

/*****************************************************************************
 * DirectXUpdateRects: update clipping rectangles
 *****************************************************************************
 * This function is called when the window position or size are changed, and
 * its job is to update the source and destination RECTs used to display the
 * picture.
 *****************************************************************************/
void E_(DirectXUpdateRects)( vout_thread_t *p_vout, vlc_bool_t b_force )
{
#define rect_src p_vout->p_sys->rect_src
#define rect_src_clipped p_vout->p_sys->rect_src_clipped
#define rect_dest p_vout->p_sys->rect_dest
#define rect_dest_clipped p_vout->p_sys->rect_dest_clipped

    int i_width, i_height, i_x, i_y;

    RECT  rect;
    POINT point;

    /* Retrieve the window size */
    GetClientRect( p_vout->p_sys->hwnd, &rect );

    /* Retrieve the window position */
    point.x = point.y = 0;
    ClientToScreen( p_vout->p_sys->hwnd, &point );

    /* If nothing changed, we can return */
    if( !b_force
         && p_vout->p_sys->i_window_width == rect.right
         && p_vout->p_sys->i_window_height == rect.bottom
         && p_vout->p_sys->i_window_x == point.x
         && p_vout->p_sys->i_window_y == point.y )
    {
        return;
    }

    /* Update the window position and size */
    p_vout->p_sys->i_window_x = point.x;
    p_vout->p_sys->i_window_y = point.y;
    p_vout->p_sys->i_window_width = rect.right;
    p_vout->p_sys->i_window_height = rect.bottom;

    vout_PlacePicture( p_vout, rect.right, rect.bottom,
                       &i_x, &i_y, &i_width, &i_height );

    if( p_vout->p_sys->hvideownd )
        SetWindowPos( p_vout->p_sys->hvideownd, HWND_TOP,
                      i_x, i_y, i_width, i_height, 0 );

    /* Destination image position and dimensions */
    rect_dest.left = point.x + i_x;
    rect_dest.right = rect_dest.left + i_width;
    rect_dest.top = point.y + i_y;
    rect_dest.bottom = rect_dest.top + i_height;

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

#if 0
    msg_Dbg( p_vout, "DirectXUpdateRects image_dst_clipped coords:"
                     " %i,%i,%i,%i",
                     rect_dest_clipped.left, rect_dest_clipped.top,
                     rect_dest_clipped.right, rect_dest_clipped.bottom );
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
    rect_src_clipped.left = (rect_dest_clipped.left - rect_dest.left) *
      p_vout->render.i_width / (rect_dest.right - rect_dest.left);
    rect_src_clipped.right = p_vout->render.i_width -
      (rect_dest.right - rect_dest_clipped.right) * p_vout->render.i_width /
      (rect_dest.right - rect_dest.left);
    rect_src_clipped.top = (rect_dest_clipped.top - rect_dest.top) *
      p_vout->render.i_height / (rect_dest.bottom - rect_dest.top);
    rect_src_clipped.bottom = p_vout->render.i_height -
      (rect_dest.bottom - rect_dest_clipped.bottom) * p_vout->render.i_height /
      (rect_dest.bottom - rect_dest.top);

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

#if 0
    msg_Dbg( p_vout, "DirectXUpdateRects image_src_clipped"
                     " coords: %i,%i,%i,%i",
                     rect_src_clipped.left, rect_src_clipped.top,
                     rect_src_clipped.right, rect_src_clipped.bottom );
#endif

    /* The destination coordinates need to be relative to the current
     * directdraw primary surface (display) */
    rect_dest_clipped.left -= p_vout->p_sys->rect_display.left;
    rect_dest_clipped.right -= p_vout->p_sys->rect_display.left;
    rect_dest_clipped.top -= p_vout->p_sys->rect_display.top;
    rect_dest_clipped.bottom -= p_vout->p_sys->rect_display.top;

    if( p_vout->p_sys->b_using_overlay )
        E_(DirectXUpdateOverlay)( p_vout );

    /* Signal the change in size/position */
    p_vout->p_sys->i_changes |= DX_POSITION_CHANGE;

#undef rect_src
#undef rect_src_clipped
#undef rect_dest
#undef rect_dest_clipped
}

/*****************************************************************************
 * DirectXEventProc: This is the window event processing function.
 *****************************************************************************
 * On Windows, when you create a window you have to attach an event processing
 * function to it. The aim of this function is to manage "Queued Messages" and
 * "Nonqueued Messages".
 * Queued Messages are those picked up and retransmitted by vout_Manage
 * (using the GetMessage and DispatchMessage functions).
 * Nonqueued Messages are those that Windows will send directly to this
 * procedure (like WM_DESTROY, WM_WINDOWPOSCHANGED...)
 *****************************************************************************/
static long FAR PASCAL DirectXEventProc( HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam )
{
    vout_thread_t *p_vout;

    if( message == WM_CREATE )
    {
        /* Store p_vout for future use */
        p_vout = (vout_thread_t *)((CREATESTRUCT *)lParam)->lpCreateParams;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)p_vout );
    }
    else
    {
        p_vout = (vout_thread_t *)GetWindowLongPtr( hwnd, GWLP_USERDATA );
    }

    /* Catch the screensaver and the monitor turn-off */
    if( message == WM_SYSCOMMAND &&
        ( wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER ) )
    {
        //if( p_vout ) msg_Dbg( p_vout, "WinProc WM_SYSCOMMAND screensaver" );
        return 0; /* this stops them from happening */
    }

    if( !p_vout )
    {
        /* Hmmm mozilla does manage somehow to save the pointer to our
         * windowproc and still calls it after the vout has been closed. */
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    if( hwnd == p_vout->p_sys->hvideownd )
        return DefWindowProc(hwnd, message, wParam, lParam);

    switch( message )
    {

    case WM_WINDOWPOSCHANGED:
        E_(DirectXUpdateRects)( p_vout, VLC_TRUE );
        return 0;

    /* the user wants to close the window */
    case WM_CLOSE:
    {
        playlist_t * p_playlist =
            (playlist_t *)vlc_object_find( p_vout, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
        if( p_playlist == NULL )
        {
            return 0;
        }

        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
        return 0;
    }

    /* the window has been closed so shut down everything now */
    case WM_DESTROY:
        msg_Dbg( p_vout, "WinProc WM_DESTROY" );
        /* just destroy the window */
        PostQuitMessage( 0 );
        return 0;

    case WM_SYSCOMMAND:
        switch (wParam)
        {
            case IDM_TOGGLE_ON_TOP:            /* toggle the "on top" status */
            {
                vlc_value_t val;
                msg_Dbg( p_vout, "WinProc WM_SYSCOMMAND: IDM_TOGGLE_ON_TOP");

                /* Change the current value */
                var_Get( p_vout, "video-on-top", &val );
                val.b_bool = !val.b_bool;
                var_Set( p_vout, "video-on-top", val );
                return 0;
            }
        }
        break;

    case WM_VLC_CREATE_VIDEO_WIN:
        /* Create video sub-window */
        p_vout->p_sys->hvideownd =
            CreateWindow( _T("VLC DirectX video"), _T(""),   /* window class */
                    WS_CHILD | WS_VISIBLE,                   /* window style */
                    CW_USEDEFAULT, CW_USEDEFAULT,     /* default coordinates */
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    hwnd,                                   /* parent window */
                    NULL, GetModuleHandle(NULL),
                    (LPVOID)p_vout );            /* send p_vout to WM_CREATE */

        if( !p_vout->p_sys->hvideownd )
            msg_Warn( p_vout, "Can't create video sub-window" );
        else
            msg_Dbg( p_vout, "Created video sub-window" );
        break;

    case WM_PAINT:
    case WM_NCPAINT:
    case WM_ERASEBKGND:
        /* We do not want to relay these messages to the parent window
         * because we rely on the background color for the overlay. */
        return DefWindowProc(hwnd, message, wParam, lParam);
        break;

    default:
        //msg_Dbg( p_vout, "WinProc WM Default %i", message );
        break;
    }

    /* Let windows handle the message */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

static struct
{
    int i_dxkey;
    int i_vlckey;

} dxkeys_to_vlckeys[] =
{
    { VK_F1, KEY_F1 }, { VK_F2, KEY_F2 }, { VK_F3, KEY_F3 }, { VK_F4, KEY_F4 },
    { VK_F5, KEY_F5 }, { VK_F6, KEY_F6 }, { VK_F7, KEY_F7 }, { VK_F8, KEY_F8 },
    { VK_F9, KEY_F9 }, { VK_F10, KEY_F10 }, { VK_F11, KEY_F11 },
    { VK_F12, KEY_F12 },

    { VK_RETURN, KEY_ENTER },
    { VK_SPACE, KEY_SPACE },
    { VK_ESCAPE, KEY_ESC },

    { VK_LEFT, KEY_LEFT },
    { VK_RIGHT, KEY_RIGHT },
    { VK_UP, KEY_UP },
    { VK_DOWN, KEY_DOWN },

    { VK_HOME, KEY_HOME },
    { VK_END, KEY_END },
    { VK_PRIOR, KEY_PAGEUP },
    { VK_NEXT, KEY_PAGEDOWN },

    { VK_INSERT, KEY_INSERT },
    { VK_DELETE, KEY_DELETE },

    { VK_CONTROL, 0 },
    { VK_SHIFT, 0 },
    { VK_MENU, 0 },

    { 0, 0 }
};

static int DirectXConvertKey( int i_key )
{
    int i;

    for( i = 0; dxkeys_to_vlckeys[i].i_dxkey != 0; i++ )
    {
        if( dxkeys_to_vlckeys[i].i_dxkey == i_key )
        {
            return dxkeys_to_vlckeys[i].i_vlckey;
        }
    }

    return 0;
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    double f_arg;
    RECT rect_window;
    POINT point;

    switch( i_query )
    {
    case VOUT_SET_ZOOM:
        if( p_vout->p_sys->hparent )
            return vout_ControlWindow( p_vout,
                    (void *)p_vout->p_sys->hparent, i_query, args );

        f_arg = va_arg( args, double );

        /* Update dimensions */
        rect_window.top = rect_window.left = 0;
        rect_window.right  = p_vout->i_window_width * f_arg;
        rect_window.bottom = p_vout->i_window_height * f_arg;
        AdjustWindowRect( &rect_window, p_vout->p_sys->i_window_style, 0 );

        SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                      rect_window.right - rect_window.left,
                      rect_window.bottom - rect_window.top, SWP_NOMOVE );

        return VLC_SUCCESS;

    case VOUT_CLOSE:
        ShowWindow( p_vout->p_sys->hwnd, SW_HIDE );
    case VOUT_REPARENT:
        /* Change window style, borders and title bar */
        vlc_mutex_lock( &p_vout->p_sys->lock );
        p_vout->p_sys->hparent = 0;
        vlc_mutex_unlock( &p_vout->p_sys->lock );

        /* Retrieve the window position */
        point.x = point.y = 0;
        ClientToScreen( p_vout->p_sys->hwnd, &point );

        SetParent( p_vout->p_sys->hwnd, 0 );
        p_vout->p_sys->i_window_style =
            WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
        SetWindowLong( p_vout->p_sys->hwnd, GWL_STYLE,
                       p_vout->p_sys->i_window_style |
                       (i_query == VOUT_CLOSE ? 0 : WS_VISIBLE) );
        SetWindowLong( p_vout->p_sys->hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW );
        SetWindowPos( p_vout->p_sys->hwnd, 0, point.x, point.y, 0, 0,
                      SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED );

        return vout_vaControlDefault( p_vout, i_query, args );

    case VOUT_SET_STAY_ON_TOP:
        if( p_vout->p_sys->hparent )
            return vout_ControlWindow( p_vout,
                    (void *)p_vout->p_sys->hparent, i_query, args );

        p_vout->p_sys->b_on_top_change = VLC_TRUE;
        return VLC_SUCCESS;

    default:
        return vout_vaControlDefault( p_vout, i_query, args );
    }
}
