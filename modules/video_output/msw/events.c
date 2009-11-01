/*****************************************************************************
 * events.c: Windows DirectX video output events handler
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

#ifdef UNDER_CE
#include <aygshell.h>
    //WINSHELLAPI BOOL WINAPI SHFullScreen(HWND hwndRequester, DWORD dwState);
#endif

/*#if defined(UNDER_CE) && !defined(__PLUGIN__) --FIXME*/
/*#   define SHFS_SHOWSIPBUTTON 0x0004
#   define SHFS_HIDESIPBUTTON 0x0008
#   define MENU_HEIGHT 26
    BOOL SHFullScreen(HWND hwndRequester, DWORD dwState);
#endif*/

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#define WM_VLC_HIDE_MOUSE   (WM_APP + 0)
#define WM_VLC_SHOW_MOUSE   (WM_APP + 1)
#define WM_VLC_CHANGE_TEXT  (WM_APP + 2)

struct event_thread_t
{
    vout_thread_t *p_vout;

    /* */
    vlc_thread_t thread;
    vlc_mutex_t  lock;
    vlc_cond_t   wait;
    bool         b_ready;
    bool         b_done;
    bool         b_error;

    /* */
    bool use_desktop;
    bool use_overlay;

    /* Mouse */
    volatile bool    b_cursor_hidden;
    volatile mtime_t i_lastmoved;
    mtime_t          i_mouse_hide_timeout;

    /* Title */
    char *psz_title;

    int               i_window_style;
    vout_window_cfg_t wnd_cfg;

    /* */
    unsigned i_changes;

    /* */
    vout_window_t *parent_window;
    HWND hparent;
    HWND hwnd;
    HWND hvideownd;
    HWND hfswnd;
};

static int  DirectXCreateWindow( event_thread_t * );
static void DirectXCloseWindow ( event_thread_t * );
static long FAR PASCAL DirectXEventProc( HWND, UINT, WPARAM, LPARAM );

static void DirectXPopupMenu( event_thread_t *p_event, bool b_open )
{
    var_SetBool( p_event->p_vout->p_libvlc, "intf-popupmenu", b_open );
}

static int DirectXConvertKey( int i_key );

/*****************************************************************************
 * EventThread: Create video window & handle its messages
 *****************************************************************************
 * This function creates a video window and then enters an infinite loop
 * that handles the messages sent to that window.
 * The main goal of this thread is to isolate the Win32 PeekMessage function
 * because this one can block for a long time.
 *****************************************************************************/
static void *EventThread( void *p_this )
{
    event_thread_t *p_event = (event_thread_t *)p_this;
    vout_thread_t *p_vout = p_event->p_vout;
    MSG msg;
    POINT old_mouse_pos = {0,0}, mouse_pos;
    vlc_value_t val;
    unsigned int i_width, i_height, i_x, i_y;
    HMODULE hkernel32;
    int canc = vlc_savecancel ();

    vlc_mutex_lock( &p_event->lock );
    /* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message queue */
    if( DirectXCreateWindow( p_event ) )
        p_event->b_error = true;

    p_event->b_ready = true;
    vlc_cond_signal( &p_event->wait );

    const bool b_error = p_event->b_error;
    vlc_mutex_unlock( &p_event->lock );

    if( b_error )
    {
        vlc_restorecancel( canc );
        return NULL;
    }

#ifndef UNDER_CE
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
            msg_Dbg( p_vout, "no support for SetThreadExecutionState()" );
    }
#endif

    /* Main loop */
    /* GetMessage will sleep if there's no message in the queue */
    for( ;; )
    {
        if( !GetMessage( &msg, 0, 0, 0 ) )
        {
            vlc_mutex_lock( &p_event->lock );
            p_event->b_done = true;
            vlc_mutex_unlock( &p_event->lock );
            break;
        }

        /* Check if we are asked to exit */
        vlc_mutex_lock( &p_event->lock );
        const bool b_done = p_event->b_done;
        vlc_mutex_unlock( &p_event->lock );
        if( b_done )
            break;

        /* */
        switch( msg.message )
        {

        case WM_MOUSEMOVE:
            vlc_mutex_lock( &p_event->lock );
            vout_PlacePicture( p_event->p_vout,
                               p_event->wnd_cfg.width,
                               p_event->wnd_cfg.height,
                               &i_x, &i_y, &i_width, &i_height );
            vlc_mutex_unlock( &p_event->lock );

            if( msg.hwnd == p_event->hvideownd )
            {
                /* Child window */
                i_x = i_y = 0;
            }

            if( i_width && i_height )
            {
                val.i_int = ( GET_X_LPARAM(msg.lParam) - i_x ) *
                    p_event->p_vout->fmt_in.i_visible_width / i_width +
                    p_event->p_vout->fmt_in.i_x_offset;
                var_Set( p_event->p_vout, "mouse-x", val );
                val.i_int = ( GET_Y_LPARAM(msg.lParam) - i_y ) *
                    p_event->p_vout->fmt_in.i_visible_height / i_height +
                    p_event->p_vout->fmt_in.i_y_offset;
                var_Set( p_event->p_vout, "mouse-y", val );

                var_SetBool( p_event->p_vout, "mouse-moved", true );
            }

        case WM_NCMOUSEMOVE:
            GetCursorPos( &mouse_pos );
            if( (abs(mouse_pos.x - old_mouse_pos.x) > 2 ||
                (abs(mouse_pos.y - old_mouse_pos.y)) > 2 ) )
            {
                GetCursorPos( &old_mouse_pos );
                p_event->i_lastmoved = mdate();

                if( p_event->b_cursor_hidden )
                {
                    p_event->b_cursor_hidden = 0;
                    ShowCursor( TRUE );
                }
            }
            break;

        case WM_VLC_HIDE_MOUSE:
            if( p_event->b_cursor_hidden ) break;
            p_event->b_cursor_hidden = true;
            GetCursorPos( &old_mouse_pos );
            ShowCursor( FALSE );
            break;

        case WM_VLC_SHOW_MOUSE:
            if( !p_event->b_cursor_hidden ) break;
            p_event->b_cursor_hidden = false;
            GetCursorPos( &old_mouse_pos );
            ShowCursor( TRUE );
            break;

        case WM_LBUTTONDOWN:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int |= 1;
            var_Set( p_event->p_vout, "mouse-button-down", val );
            DirectXPopupMenu( p_event, false );
            break;

        case WM_LBUTTONUP:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int &= ~1;
            var_Set( p_event->p_vout, "mouse-button-down", val );

            var_SetBool( p_event->p_vout, "mouse-clicked", true );
            break;

        case WM_LBUTTONDBLCLK:
            vlc_mutex_lock( &p_event->lock );
            p_event->i_changes |= VOUT_FULLSCREEN_CHANGE;
            vlc_mutex_unlock( &p_event->lock );
            break;

        case WM_MBUTTONDOWN:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int |= 2;
            var_Set( p_event->p_vout, "mouse-button-down", val );
            DirectXPopupMenu( p_event, false );
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
            DirectXPopupMenu( p_event, false );
            break;

        case WM_RBUTTONUP:
            var_Get( p_event->p_vout, "mouse-button-down", &val );
            val.i_int &= ~4;
            var_Set( p_event->p_vout, "mouse-button-down", val );
            DirectXPopupMenu( p_event, true );
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

                var_Set( p_vout->p_libvlc, "key-pressed", val );
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

                var_Set( p_vout->p_libvlc, "key-pressed", val );
            }
            break;

        case WM_VLC_CHANGE_TEXT:
        {
            vlc_mutex_lock( &p_event->lock );
            wchar_t *pwz_title = NULL;
            if( p_event->psz_title )
            {
                const size_t i_length = strlen(p_event->psz_title);
                pwz_title = malloc( 2 * (i_length + 1) );
                if( pwz_title )
                {
                    mbstowcs( pwz_title, p_event->psz_title, 2 * i_length );
                    pwz_title[i_length] = 0;
                }
            }
            vlc_mutex_unlock( &p_event->lock );

            if( pwz_title )
            {
                SetWindowText( p_event->hwnd, (LPCTSTR)pwz_title );
                if( p_event->hfswnd )
                    SetWindowText( p_event->hfswnd, (LPCTSTR)pwz_title );
                free( pwz_title );
            }
            break;
        }

        default:
            /* Messages we don't handle directly are dispatched to the
             * window procedure */
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            break;

        } /* End Switch */

    } /* End Main loop */

    /* Check for WM_QUIT if we created the window */
    if( !p_event->hparent && msg.message == WM_QUIT )
    {
        msg_Warn( p_vout, "WM_QUIT... should not happen!!" );
        p_event->hwnd = NULL; /* Window already destroyed */
    }

    msg_Dbg( p_vout, "DirectXEventThread terminating" );

    DirectXCloseWindow( p_event );
    vlc_restorecancel(canc);
    return NULL;
}


/* following functions are local */

/*****************************************************************************
 * DirectXCreateWindow: create a window for the video.
 *****************************************************************************
 * Before creating a direct draw surface, we need to create a window in which
 * the video will be displayed. This window will also allow us to capture the
 * events.
 *****************************************************************************/
static int DirectXCreateWindow( event_thread_t *p_event )
{
    vout_thread_t *p_vout = p_event->p_vout;
    HINSTANCE  hInstance;
    HMENU      hMenu;
    RECT       rect_window;
    WNDCLASS   wc;                            /* window class components */
    HICON      vlc_icon;
    char       vlc_path[MAX_PATH+1];
    int        i_style, i_stylex;

    msg_Dbg( p_vout, "DirectXCreateWindow" );

    /* Get this module's instance */
    hInstance = GetModuleHandle(NULL);

    #ifdef MODULE_NAME_IS_direct3d
    if( !p_event->use_desktop )
    {
    #endif
        /* If an external window was specified, we'll draw in it. */
        p_event->parent_window = vout_window_New( VLC_OBJECT(p_vout), NULL, &p_event->wnd_cfg );
        if( p_event->parent_window )
            p_event->hparent = p_event->parent_window->handle.hwnd;
        else
            p_event->hparent = NULL;
    #ifdef MODULE_NAME_IS_direct3d
    }
    else
    {
        /* Find Program Manager */
        HWND hwnd = FindWindow( _T("Progman"), NULL );
        if( hwnd ) hwnd = FindWindowEx( hwnd, NULL, _T("SHELLDLL_DefView"), NULL );
        if( hwnd ) hwnd = FindWindowEx( hwnd, NULL, _T("SysListView32"), NULL );
        if( !hwnd )
            msg_Err( p_vout, "Couldn't find desktop icon window. Desktop mode can't be established." );
        p_event->hparent = hwnd;
    }
    #endif

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
            msg_Err( p_vout, "DirectXCreateWindow RegisterClass FAILED (err=%lu)", GetLastError() );
            return VLC_EGENERIC;
        }
    }

    /* Register the video sub-window class */
    wc.lpszClassName = _T("VLC DirectX video"); wc.hIcon = 0;
    wc.hbrBackground = NULL; /* no background color */
    if( !RegisterClass(&wc) )
    {
        WNDCLASS wndclass;

        /* Check why it failed. If it's because one already exists
         * then fine, otherwise return with an error. */
        if( !GetClassInfo( hInstance, _T("VLC DirectX video"), &wndclass ) )
        {
            msg_Err( p_vout, "DirectXCreateWindow RegisterClass FAILED (err=%lu)", GetLastError() );
            return VLC_EGENERIC;
        }
    }

    /* When you create a window you give the dimensions you wish it to
     * have. Unfortunatly these dimensions will include the borders and
     * titlebar. We use the following function to find out the size of
     * the window corresponding to the useable surface we want */
    rect_window.top    = 10;
    rect_window.left   = 10;
    rect_window.right  = rect_window.left + p_event->wnd_cfg.width;
    rect_window.bottom = rect_window.top  + p_event->wnd_cfg.height;

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

    if( p_event->hparent )
    {
        i_style = WS_VISIBLE|WS_CLIPCHILDREN|WS_CHILD;
        i_stylex = 0;
    }

    p_event->i_window_style = i_style;

    /* Create the window */
    p_event->hwnd =
        CreateWindowEx( WS_EX_NOPARENTNOTIFY | i_stylex,
                    _T("VLC DirectX"),               /* name of window class */
                    _T(VOUT_TITLE) _T(" (DirectX Output)"),  /* window title */
                    i_style,                                 /* window style */
                    (p_event->wnd_cfg.x < 0) ? CW_USEDEFAULT :
                        (UINT)p_event->wnd_cfg.x,   /* default X coordinate */
                    (p_event->wnd_cfg.y < 0) ? CW_USEDEFAULT :
                        (UINT)p_event->wnd_cfg.y,   /* default Y coordinate */
                    rect_window.right - rect_window.left,    /* window width */
                    rect_window.bottom - rect_window.top,   /* window height */
                    p_event->hparent,                       /* parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    (LPVOID)p_event );           /* send p_vout to WM_CREATE */

    if( !p_event->hwnd )
    {
        msg_Warn( p_vout, "DirectXCreateWindow create window FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }

    if( p_event->hparent )
    {
        LONG i_style;

        /* We don't want the window owner to overwrite our client area */
        i_style = GetWindowLong( p_event->hparent, GWL_STYLE );

        if( !(i_style & WS_CLIPCHILDREN) )
            /* Hmmm, apparently this is a blocking call... */
            SetWindowLong( p_event->hparent, GWL_STYLE,
                           i_style | WS_CLIPCHILDREN );

        /* Create our fullscreen window */
        p_event->hfswnd =
            CreateWindowEx( WS_EX_APPWINDOW, _T("VLC DirectX"),
                            _T(VOUT_TITLE) _T(" (DirectX Output)"),
                            WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_SIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            NULL, NULL, hInstance, NULL );
    }
    else
    {
        p_event->hfswnd = NULL;
    }

    /* Append a "Always On Top" entry in the system menu */
    hMenu = GetSystemMenu( p_event->hwnd, FALSE );
    AppendMenu( hMenu, MF_SEPARATOR, 0, _T("") );
    AppendMenu( hMenu, MF_STRING | MF_UNCHECKED,
                       IDM_TOGGLE_ON_TOP, _T("Always on &Top") );

    /* Create video sub-window. This sub window will always exactly match
     * the size of the video, which allows us to use crazy overlay colorkeys
     * without having them shown outside of the video area. */
    p_event->hvideownd =
    CreateWindow( _T("VLC DirectX video"), _T(""),   /* window class */
        WS_CHILD,                   /* window style, not visible initially */
        0, 0,
        p_vout->render.i_width,          /* default width */
        p_vout->render.i_height,        /* default height */
        p_event->hwnd,                   /* parent window */
        NULL, hInstance,
        (LPVOID)p_event );    /* send p_vout to WM_CREATE */

    if( !p_event->hvideownd )
        msg_Warn( p_vout, "can't create video sub-window" );
    else
        msg_Dbg( p_vout, "created video sub-window" );

    /* Now display the window */
    ShowWindow( p_event->hwnd, SW_SHOW );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DirectXCloseWindow: close the window created by DirectXCreateWindow
 *****************************************************************************
 * This function returns all resources allocated by DirectXCreateWindow.
 *****************************************************************************/
static void DirectXCloseWindow( event_thread_t *p_event )
{
    vout_thread_t *p_vout = p_event->p_vout;
    msg_Dbg( p_vout, "DirectXCloseWindow" );

    DestroyWindow( p_event->hwnd );
    if( p_event->hfswnd ) DestroyWindow( p_event->hfswnd );

    #ifdef MODULE_NAME_IS_direct3d
    if( !p_event->use_desktop )
    #endif
        vout_window_Delete( p_event->parent_window );
    p_event->hwnd = NULL;

    /* We don't unregister the Window Class because it could lead to race
     * conditions and it will be done anyway by the system when the app will
     * exit */
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
    event_thread_t *p_event;

    if( message == WM_CREATE )
    {
        /* Store p_vout for future use */
        p_event = (event_thread_t *)((CREATESTRUCT *)lParam)->lpCreateParams;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)p_event );
        return TRUE;
    }
    else
    {
        LONG_PTR p_user_data = GetWindowLongPtr( hwnd, GWLP_USERDATA );
        p_event = (event_thread_t *)p_user_data;
        if( !p_event )
        {
            /* Hmmm mozilla does manage somehow to save the pointer to our
             * windowproc and still calls it after the vout has been closed. */
            return DefWindowProc(hwnd, message, wParam, lParam);
        }
    }
    vout_thread_t *p_vout = p_event->p_vout;

#ifndef UNDER_CE
    /* Catch the screensaver and the monitor turn-off */
    if( message == WM_SYSCOMMAND &&
        ( (wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER ) )
    {
        //if( p_vout ) msg_Dbg( p_vout, "WinProc WM_SYSCOMMAND screensaver" );
        return 0; /* this stops them from happening */
    }
#endif

    if( hwnd == p_event->hvideownd )
    {
#ifdef MODULE_NAME_IS_directx
        vlc_mutex_lock( &p_event->lock );
        const bool use_overlay = p_event->use_overlay;
        vlc_mutex_unlock( &p_event->lock );
#endif

        switch( message )
        {
#ifdef MODULE_NAME_IS_directx
        case WM_ERASEBKGND:
        /* For overlay, we need to erase background */
            return !use_overlay ? 1 : DefWindowProc(hwnd, message, wParam, lParam);
        case WM_PAINT:
        /*
        ** For overlay, DefWindowProc() will erase dirty regions
        ** with colorkey.
        ** For non-overlay, vout will paint the whole window at
        ** regular interval, therefore dirty regions can be ignored
        ** to minimize repaint.
        */
            if( !use_overlay )
            {
                ValidateRect(hwnd, NULL);
            }
            // fall through to default
#else
        /*
        ** For OpenGL and Direct3D, vout will update the whole
        ** window at regular interval, therefore dirty region
        ** can be ignored to minimize repaint.
        */
        case WM_ERASEBKGND:
            /* nothing to erase */
            return 1;
        case WM_PAINT:
            /* nothing to repaint */
            ValidateRect(hwnd, NULL);
            // fall through
#endif
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
        }
    }

    switch( message )
    {

    case WM_WINDOWPOSCHANGED:
        UpdateRects( p_vout, true );
        return 0;

    /* the user wants to close the window */
    case WM_CLOSE:
    {
        playlist_t * p_playlist = pl_Hold( p_vout );
        if( p_playlist )
        {
            playlist_Stop( p_playlist );
            pl_Release( p_vout );
        }
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
                msg_Dbg( p_vout, "WinProc WM_SYSCOMMAND: IDM_TOGGLE_ON_TOP");

                /* Change the current value */
                var_ToggleBool( p_vout, "video-on-top" );
                return 0;
            }
        }
        break;

    case WM_PAINT:
    case WM_NCPAINT:
    case WM_ERASEBKGND:
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_KILLFOCUS:
#ifdef MODULE_NAME_IS_wingapi
        GXSuspend();
#endif
#ifdef UNDER_CE
        if( hwnd == p_event->hfswnd )
        {
            HWND htbar = FindWindow( _T("HHTaskbar"), NULL );
            ShowWindow( htbar, SW_SHOW );
        }

        if( !p_event->hparent ||
            hwnd == p_event->hfswnd )
        {
            SHFullScreen( hwnd, SHFS_SHOWSIPBUTTON );
        }
#endif
        return 0;

    case WM_SETFOCUS:
#ifdef MODULE_NAME_IS_wingapi
        GXResume();
#endif
#ifdef UNDER_CE
        if( p_event->hparent &&
            hwnd != p_event->hfswnd && p_vout->b_fullscreen )
        {
            vlc_mutex_lock( &p_event->lock );
            p_event->i_changes |= VOUT_FULLSCREEN_CHANGE;
            vlc_mutex_unlock( &p_event->lock );
        }

        if( hwnd == p_event->hfswnd )
        {
            HWND htbar = FindWindow( _T("HHTaskbar"), NULL );
            ShowWindow( htbar, SW_HIDE );
        }

        if( !p_event->hparent ||
            hwnd == p_event->hfswnd )
        {
            SHFullScreen( hwnd, SHFS_HIDESIPBUTTON );
        }
#endif
        return 0;

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
    { VK_SPACE, ' ' },
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

    { VK_BROWSER_BACK, KEY_BROWSER_BACK },
    { VK_BROWSER_FORWARD, KEY_BROWSER_FORWARD },
    { VK_BROWSER_REFRESH, KEY_BROWSER_REFRESH },
    { VK_BROWSER_STOP, KEY_BROWSER_STOP },
    { VK_BROWSER_SEARCH, KEY_BROWSER_SEARCH },
    { VK_BROWSER_FAVORITES, KEY_BROWSER_FAVORITES },
    { VK_BROWSER_HOME, KEY_BROWSER_HOME },
    { VK_VOLUME_MUTE, KEY_VOLUME_MUTE },
    { VK_VOLUME_DOWN, KEY_VOLUME_DOWN },
    { VK_VOLUME_UP, KEY_VOLUME_UP },
    { VK_MEDIA_NEXT_TRACK, KEY_MEDIA_NEXT_TRACK },
    { VK_MEDIA_PREV_TRACK, KEY_MEDIA_PREV_TRACK },
    { VK_MEDIA_STOP, KEY_MEDIA_STOP },
    { VK_MEDIA_PLAY_PAUSE, KEY_MEDIA_PLAY_PAUSE },

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

void EventThreadMouseAutoHide( event_thread_t *p_event )
{
    vout_thread_t *p_vout = p_event->p_vout;

    if( p_vout->b_fullscreen &&
        !p_event->b_cursor_hidden &&
        (mdate() - p_event->i_lastmoved) > p_event->i_mouse_hide_timeout )
    {
        /* Hide the cursor only if it is inside our window */
        POINT point;
        GetCursorPos( &point );

        HWND hwnd = WindowFromPoint(point);
        if( hwnd == p_event->hwnd || hwnd == p_event->hvideownd )
        {
            PostMessage( p_event->hwnd, WM_VLC_HIDE_MOUSE, 0, 0 );
        }
        else
        {
            p_event->i_lastmoved = mdate();
        }
    }
}
void EventThreadMouseShow( event_thread_t *p_event )
{
    PostMessage( p_event->hwnd, WM_VLC_SHOW_MOUSE, 0, 0 );
}
void EventThreadUpdateTitle( event_thread_t *p_event, const char *psz_fallback )
{
    char *psz_title = var_GetNonEmptyString( p_event->p_vout, "video-title" );
    if( !psz_title )
        psz_title = strdup( psz_fallback );
    if( !psz_title )
        return;

    vlc_mutex_lock( &p_event->lock );
    free( p_event->psz_title );
    p_event->psz_title = psz_title;
    vlc_mutex_unlock( &p_event->lock );

    PostMessage( p_event->hwnd, WM_VLC_CHANGE_TEXT, 0, 0 );
}
unsigned EventThreadRetreiveChanges( event_thread_t *p_event )
{
    vlc_mutex_lock( &p_event->lock );
    unsigned i_changes = p_event->i_changes;
    p_event->i_changes = 0;
    vlc_mutex_unlock( &p_event->lock );

    return i_changes;
}
int EventThreadGetWindowStyle( event_thread_t *p_event )
{
    /* No need to lock, it is serialized by EventThreadStart */
    return p_event->i_window_style;
}

void EventThreadUpdateWindowPosition( event_thread_t *p_event, bool *pb_changed,
                                      int x, int y, int w, int h )
{
    vlc_mutex_lock( &p_event->lock );
    *pb_changed = x != p_event->wnd_cfg.x ||
                  y != p_event->wnd_cfg.y ||
                  w != p_event->wnd_cfg.width ||
                  h != p_event->wnd_cfg.height;

    p_event->wnd_cfg.x      = x;
    p_event->wnd_cfg.y      = y;
    p_event->wnd_cfg.width  = w;
    p_event->wnd_cfg.height = h;
    vlc_mutex_unlock( &p_event->lock );
}

void EventThreadUseOverlay( event_thread_t *p_event, bool b_used )
{
    vlc_mutex_lock( &p_event->lock );
    p_event->use_overlay = b_used;
    vlc_mutex_unlock( &p_event->lock );
}

event_thread_t *EventThreadCreate( vout_thread_t *p_vout, const vout_window_cfg_t *p_wnd_cfg )
{
     /* Create the Vout EventThread, this thread is created by us to isolate
     * the Win32 PeekMessage function calls. We want to do this because
     * Windows can stay blocked inside this call for a long time, and when
     * this happens it thus blocks vlc's video_output thread.
     * Vout EventThread will take care of the creation of the video
     * window (because PeekMessage has to be called from the same thread which
     * created the window). */
    msg_Dbg( p_vout, "creating Vout EventThread" );
    event_thread_t *p_event = malloc( sizeof(*p_event) );
    if( !p_event )
        return NULL;

    p_event->p_vout = p_vout;
    vlc_mutex_init( &p_event->lock );
    vlc_cond_init( &p_event->wait );

    p_event->b_cursor_hidden      = false;
    p_event->i_lastmoved          = mdate();
    p_event->i_mouse_hide_timeout =
        var_GetInteger(p_vout, "mouse-hide-timeout") * 1000;
    p_event->psz_title = NULL;
    p_event->wnd_cfg = *p_wnd_cfg;
   
    return p_event;
}

void EventThreadDestroy( event_thread_t *p_event )
{
    free( p_event->psz_title );
    vlc_cond_destroy( &p_event->wait );
    vlc_mutex_destroy( &p_event->lock );
    free( p_event );
}

int EventThreadStart( event_thread_t *p_event, event_hwnd_t *p_hwnd, const event_cfg_t *p_cfg )
{
    p_event->use_desktop = p_cfg->use_desktop;
    p_event->use_overlay = p_cfg->use_overlay;

    p_event->i_changes = 0;

    p_event->b_ready = false;
    p_event->b_done  = false;
    p_event->b_error = false;

    if( vlc_clone( &p_event->thread, EventThread, p_event,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_event->p_vout, "cannot create Vout EventThread" );
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &p_event->lock );
    while( !p_event->b_ready )
        vlc_cond_wait( &p_event->wait, &p_event->lock );
    const bool b_error = p_event->b_error;
    vlc_mutex_unlock( &p_event->lock );

    if( b_error )
    {
        vlc_join( p_event->thread, NULL );
        p_event->b_ready = false;
        return VLC_EGENERIC;
    }
    msg_Dbg( p_event->p_vout, "Vout EventThread running" );

    if( !p_event->use_desktop )
        p_hwnd->parent_window = p_event->parent_window;
    else
        p_hwnd->parent_window = NULL;
    p_hwnd->hparent       = p_event->hparent;
    p_hwnd->hwnd          = p_event->hwnd;
    p_hwnd->hvideownd     = p_event->hvideownd;
    p_hwnd->hfswnd        = p_event->hfswnd;
    return VLC_SUCCESS;
}

void EventThreadStop( event_thread_t *p_event )
{
    if( !p_event->b_ready )
        return;

    vlc_mutex_lock( &p_event->lock );
    p_event->b_done = true;
    vlc_mutex_unlock( &p_event->lock );

    /* we need to be sure Vout EventThread won't stay stuck in
     * GetMessage, so we send a fake message */
    if( p_event->hwnd )
        PostMessage( p_event->hwnd, WM_NULL, 0, 0);

    vlc_join( p_event->thread, NULL );
    p_event->b_ready = false;
}

