/*****************************************************************************
 * events.c: Windows DirectX video output events handler
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: events.c,v 1.8 2002/11/25 03:12:42 ipkiss Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#include "netutils.h"

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <ddraw.h>

#include "vout.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  DirectXCreateWindow( vout_thread_t *p_vout );
static void DirectXCloseWindow ( vout_thread_t *p_vout );
static long FAR PASCAL DirectXEventProc ( HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam );

/*****************************************************************************
 * DirectXEventThread: Create video window & handle its messages
 *****************************************************************************
 * This function creates a video window and then enters an infinite loop
 * that handles the messages sent to that window.
 * The main goal of this thread is to isolate the Win32 PeekMessage function
 * because this one can block for a long time.
 *****************************************************************************/
void DirectXEventThread( event_thread_t *p_event )
{
    MSG msg;
    POINT old_mouse_pos;
    vlc_value_t val;
    int i_width, i_height, i_x, i_y;

    /* Initialisation */

    /* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message queue */
    if( DirectXCreateWindow( p_event->p_vout ) )
    {
        msg_Err( p_event, "out of memory" );
        p_event->b_dead = VLC_TRUE;
    }

    /* signal the creation of the window */
    vlc_thread_ready( p_event );

    /* Main loop */
    /* GetMessage will sleep if there's no message in the queue */
    while( !p_event->b_die
           && GetMessage( &msg, p_event->p_vout->p_sys->hwnd, 0, 0 ) )
    {
        /* Check if we are asked to exit */
        if( p_event->b_die )
            break;

        switch( msg.message )
        {

        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE:
            vout_PlacePicture( p_event->p_vout,
                               p_event->p_vout->p_sys->i_window_width,
                               p_event->p_vout->p_sys->i_window_height,
                               &i_x, &i_y, &i_width, &i_height );

            val.i_int = ( GET_X_LPARAM(msg.lParam) - i_x )
                         * p_event->p_vout->render.i_width / i_width;
            var_Set( p_event->p_vout, "mouse-x", val );
            val.i_int = ( GET_Y_LPARAM(msg.lParam) - i_y )
                         * p_event->p_vout->render.i_height / i_height;
            var_Set( p_event->p_vout, "mouse-y", val );

            val.b_bool = VLC_TRUE;
            var_Set( p_event->p_vout, "mouse-moved", val );

            if( (abs(GET_X_LPARAM(msg.lParam) - old_mouse_pos.x) > 2 ||
                (abs(GET_Y_LPARAM(msg.lParam) - old_mouse_pos.y)) > 2 ) )
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
            GetCursorPos( &old_mouse_pos );
            ShowCursor( FALSE );
            break;

        case WM_RBUTTONUP:
            {
                intf_thread_t *p_intf;
                p_intf = vlc_object_find( p_event, VLC_OBJECT_INTF,
                                                   FIND_ANYWHERE );
                if( p_intf )
                {
                    p_intf->b_menu_change = 1;
                    vlc_object_release( p_intf );
                }
            }
            break;

        case WM_LBUTTONUP:
            val.b_bool = VLC_TRUE;
            var_Set( p_event->p_vout, "mouse-clicked", val );
            break;

        case WM_LBUTTONDOWN:
            break;

        case WM_LBUTTONDBLCLK:
            p_event->p_vout->p_sys->i_changes |= VOUT_FULLSCREEN_CHANGE;
            break;

        case WM_KEYDOWN:
            /* the key events are first processed here. The next
             * message processed by this main message loop will be the
             * char translation of the key event */
            msg_Dbg( p_event, "WM_KEYDOWN" );
            switch( msg.wParam )
            {
            case VK_ESCAPE:
                /* exit application */
                p_event->p_vlc->b_die = VLC_TRUE;
                break;

            case VK_F1: network_ChannelJoin( p_event, 1 ); break;
            case VK_F2: network_ChannelJoin( p_event, 2 ); break;
            case VK_F3: network_ChannelJoin( p_event, 3 ); break;
            case VK_F4: network_ChannelJoin( p_event, 4 ); break;
            case VK_F5: network_ChannelJoin( p_event, 5 ); break;
            case VK_F6: network_ChannelJoin( p_event, 6 ); break;
            case VK_F7: network_ChannelJoin( p_event, 7 ); break;
            case VK_F8: network_ChannelJoin( p_event, 8 ); break;
            case VK_F9: network_ChannelJoin( p_event, 9 ); break;
            case VK_F10: network_ChannelJoin( p_event, 10 ); break;
            case VK_F11: network_ChannelJoin( p_event, 11 ); break;
            case VK_F12: network_ChannelJoin( p_event, 12 ); break;
            }
            TranslateMessage(&msg);
            break;

        case WM_CHAR:
            switch( msg.wParam )
            {
            case 'q':
            case 'Q':
                /* exit application */
                p_event->p_vlc->b_die = VLC_TRUE;
                break;

            case 'f':                            /* switch to fullscreen */
            case 'F':
                p_event->p_vout->p_sys->i_changes |= VOUT_FULLSCREEN_CHANGE;
                break;

            case 'c':                                /* toggle grayscale */
            case 'C':
                p_event->p_vout->b_grayscale = ! p_event->p_vout->b_grayscale;
                p_event->p_vout->p_sys->i_changes |= VOUT_GRAYSCALE_CHANGE;
                break;

            case 'i':                                     /* toggle info */
            case 'I':
                p_event->p_vout->b_info = ! p_event->p_vout->b_info;
                p_event->p_vout->p_sys->i_changes |= VOUT_INFO_CHANGE;
                break;

            case 's':                                  /* toggle scaling */
            case 'S':
                p_event->p_vout->b_scale = ! p_event->p_vout->b_scale;
                p_event->p_vout->p_sys->i_changes |= VOUT_SCALE_CHANGE;
                break;

            case ' ':                                /* toggle interface */
                p_event->p_vout->b_interface = ! p_event->p_vout->b_interface;
                p_event->p_vout->p_sys->i_changes |= VOUT_INTF_CHANGE;
                break;

            default:
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

    if( msg.message == WM_QUIT )
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
    COLORREF   colorkey;
    HDC        hdc;
    HMENU      hMenu;
    RECT       rect_window;

    msg_Dbg( p_vout, "DirectXCreateWindow" );

    /* Get this module's instance */
    hInstance = GetModuleHandle(NULL);

    /* Create a BRUSH that will be used by Windows to paint the window
     * background.
     * This window background is important for us as it will be used by the
     * graphics card to display the overlay.
     * This is why we carefully choose the color for this background, the goal
     * being to choose a color which isn't complete black but nearly. We
     * obviously don't want to use black as a colorkey for the overlay because
     * black is one of the most used color and thus would give us undesirable
     * effects */
    /* the first step is to find the colorkey we want to use. The difficulty
     * comes from the potential dithering (depends on the display depth)
     * because we need to know the real RGB value of the chosen colorkey */
    hdc = GetDC( NULL );
    for( colorkey = 0x0a; colorkey < 0xff /* all shades of red */; colorkey++ )
    {
        if( colorkey == GetNearestColor( hdc, colorkey ) )
        {
            break;
        }
    }
    msg_Dbg( p_vout, "background color: %i", colorkey );

    /* Create the actual brush */
    p_vout->p_sys->hbrush = CreateSolidBrush(colorkey);
    p_vout->p_sys->i_rgb_colorkey = (int)colorkey;

    /* Get the current size of the display and its colour depth */
    p_vout->p_sys->rect_display.right = GetDeviceCaps( hdc, HORZRES );
    p_vout->p_sys->rect_display.bottom = GetDeviceCaps( hdc, VERTRES );
    p_vout->p_sys->i_display_depth = GetDeviceCaps( hdc, BITSPIXEL );
    msg_Dbg( p_vout, "screen dimensions %ix%i colour depth %i",
                      p_vout->p_sys->rect_display.right,
                      p_vout->p_sys->rect_display.bottom,
                      p_vout->p_sys->i_display_depth );

    ReleaseDC( NULL, hdc );

    /* If an external window was specified, we'll draw in it. */
    p_vout->p_sys->hparent = p_vout->p_sys->hwnd =
                (void*)(ptrdiff_t)config_GetInt( p_vout, "directx-window" );

    if( p_vout->p_sys->hparent )
    {
        msg_Dbg( p_vout, "using external window %p\n", p_vout->p_sys->hwnd );

        /* Set stuff in the window that we can not put directly in
         * a class (see below). */
        SetClassLong( p_vout->p_sys->hwnd,
                      GCL_STYLE, CS_DBLCLKS );
        SetClassLong( p_vout->p_sys->hwnd,
                      GCL_HBRBACKGROUND, (LONG)p_vout->p_sys->hbrush );
        SetClassLong( p_vout->p_sys->hwnd,
                      GCL_HCURSOR, (LONG)LoadCursor(NULL, IDC_ARROW) );
        /* Store a p_vout pointer into the window local storage (for later
         * use in DirectXEventProc). */
        SetWindowLong( p_vout->p_sys->hwnd, GWL_USERDATA, (LONG)p_vout );

        p_vout->p_sys->pf_wndproc =
               (WNDPROC)SetWindowLong( p_vout->p_sys->hwnd,
                                       GWL_WNDPROC, (LONG)DirectXEventProc );

        /* Blam! Erase everything that might have been there. */
        RedrawWindow( p_vout->p_sys->hwnd, NULL, NULL,
                      RDW_INVALIDATE | RDW_ERASE );
    }
    else
    {
        WNDCLASSEX wc;                            /* window class components */
        HICON      vlc_icon = NULL;
        char       vlc_path[MAX_PATH+1];

        /* Get the Icon from the main app */
        vlc_icon = NULL;
        if( GetModuleFileName( NULL, vlc_path, MAX_PATH ) )
        {
            vlc_icon = ExtractIcon( hInstance, vlc_path, 0 );
        }

        /* Fill in the window class structure */
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.style         = CS_DBLCLKS;                   /* style: dbl click */
        wc.lpfnWndProc   = (WNDPROC)DirectXEventProc;       /* event handler */
        wc.cbClsExtra    = 0;                         /* no extra class data */
        wc.cbWndExtra    = 0;                        /* no extra window data */
        wc.hInstance     = hInstance;                            /* instance */
        wc.hIcon         = vlc_icon;                /* load the vlc big icon */
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);    /* default cursor */
        wc.hbrBackground = p_vout->p_sys->hbrush;        /* background color */
        wc.lpszMenuName  = NULL;                                  /* no menu */
        wc.lpszClassName = "VLC DirectX";             /* use a special class */
        wc.hIconSm       = vlc_icon;              /* load the vlc small icon */

        /* Register the window class */
        if( !RegisterClassEx(&wc) )
        {
            WNDCLASS wndclass;

            /* Free window background brush */
            if( p_vout->p_sys->hbrush )
            {
                DeleteObject( p_vout->p_sys->hbrush );
                p_vout->p_sys->hbrush = NULL;
            }

            if( vlc_icon )
            {
                DestroyIcon( vlc_icon );
            }

            /* Check why it failed. If it's because one already exists
             * then fine, otherwise return with an error. */
            if( !GetClassInfo( hInstance, "VLC DirectX", &wndclass ) )
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
        AdjustWindowRect( &rect_window, WS_OVERLAPPEDWINDOW|WS_SIZEBOX, 0 );

        /* Create the window */
        p_vout->p_sys->hwnd =
            CreateWindow( "VLC DirectX",             /* name of window class */
                    VOUT_TITLE " (DirectX Output)", /* window title bar text */
                    WS_OVERLAPPEDWINDOW | WS_SIZEBOX,        /* window style */
                    CW_USEDEFAULT,                   /* default X coordinate */
                    0,                               /* default Y coordinate */
                    rect_window.right - rect_window.left,    /* window width */
                    rect_window.bottom - rect_window.top,   /* window height */
                    NULL,                                /* no parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    (LPVOID)p_vout );            /* send p_vout to WM_CREATE */

        if( !p_vout->p_sys->hwnd )
        {
            msg_Warn( p_vout, "DirectXCreateWindow create window FAILED" );
            return VLC_EGENERIC;
        }
    }

    /* Append a "Always On Top" entry in the system menu */
    hMenu = GetSystemMenu( p_vout->p_sys->hwnd, FALSE );
    AppendMenu( hMenu, MF_SEPARATOR, 0, "" );
    AppendMenu( hMenu, MF_STRING | MF_UNCHECKED,
                       IDM_TOGGLE_ON_TOP, "Always on &Top" );

    /* Now display the window */
    ShowWindow( p_vout->p_sys->hwnd, SW_SHOW );

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

    if( p_vout->p_sys->hwnd && !p_vout->p_sys->hparent )
    {
        DestroyWindow( p_vout->p_sys->hwnd );
    }

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
void DirectXUpdateRects( vout_thread_t *p_vout, vlc_bool_t b_force )
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

    /* Destination image position and dimensions */
    rect_dest.left = point.x + i_x;
    rect_dest.right = rect_dest.left + i_width;
    rect_dest.top = point.y + i_y;
    rect_dest.bottom = rect_dest.top + i_height;


    /* UpdateOverlay directdraw function doesn't automatically clip to the
     * display size so we need to do it otherwise it will fail */

    /* Clip the destination window */
    IntersectRect( &rect_dest_clipped,
                   &rect_dest,
                   &p_vout->p_sys->rect_display );

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

#if 0
    msg_Dbg( p_vout, "DirectXUpdateRects image_src_clipped"
                     " coords: %i,%i,%i,%i",
                     rect_src_clipped.left, rect_src_clipped.top,
                     rect_src_clipped.right, rect_src_clipped.bottom );
#endif

    /* Signal the size change */
    if( !p_vout->p_sys->p_event->b_die )
    {
        if( p_vout->p_sys->b_using_overlay )
        {
            DirectXUpdateOverlay( p_vout );
        }
        else
        {
            p_vout->p_sys->i_changes |= VOUT_SIZE_CHANGE;
        }
    }

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
        SetWindowLong( hwnd, GWL_USERDATA, (LONG)p_vout );
    }
    else
    {
        p_vout = (vout_thread_t *)GetWindowLong( hwnd, GWL_USERDATA );
    }

    switch( message )
    {

    case WM_WINDOWPOSCHANGED:
        DirectXUpdateRects( p_vout, VLC_TRUE );
        return 0;

    /* the user wants to close the window */
    case WM_CLOSE:
        msg_Dbg( p_vout, "WinProc WM_CLOSE" );
        /* exit application */
        p_vout->p_vlc->b_die = VLC_TRUE;
        return 0;

    /* the window has been closed so shut down everything now */
    case WM_DESTROY:
        msg_Dbg( p_vout, "WinProc WM_DESTROY" );
        /* just destroy the window */
        PostQuitMessage( 0 );
        return 0;

    case WM_SYSCOMMAND:
        switch (wParam)
        {
            case SC_SCREENSAVE:                     /* catch the screensaver */
            case SC_MONITORPOWER:              /* catch the monitor turn-off */
                msg_Dbg( p_vout, "WinProc WM_SYSCOMMAND" );
                return 0;                  /* this stops them from happening */
            case IDM_TOGGLE_ON_TOP:            /* toggle the "on top" status */
            {
                HMENU hMenu = GetSystemMenu( hwnd , FALSE );

                msg_Dbg( p_vout, "WinProc WM_SYSCOMMAND: IDM_TOGGLE_ON_TOP");

                // Check if the window is already on top
                if( GetWindowLong( hwnd, GWL_EXSTYLE ) & WS_EX_TOPMOST )
                {
                    CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                                   MF_BYCOMMAND | MFS_UNCHECKED );
                    SetWindowPos( hwnd, HWND_NOTOPMOST,
                                  0, 0, 0, 0,
                                  SWP_NOSIZE | SWP_NOMOVE );
                }
                else
                {
                    CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                                   MF_BYCOMMAND | MFS_CHECKED );
                    SetWindowPos( hwnd, HWND_TOPMOST,
                                  0, 0, 0, 0,
                                  SWP_NOSIZE | SWP_NOMOVE );
                }
                return 0;
                break;
            }
        }
        break;

    case WM_ERASEBKGND:
        if( !p_vout->p_sys->b_using_overlay )
        {
            /* We want to eliminate unnecessary background redraws which create
             * an annoying flickering */
            int i_width, i_height, i_x, i_y;
            RECT rect_temp;
            GetClipBox( (HDC)wParam, &rect_temp );
#if 0
            msg_Dbg( p_vout, "WinProc WM_ERASEBKGND %i,%i,%i,%i",
                          rect_temp.left, rect_temp.top,
                          rect_temp.right, rect_temp.bottom );
#endif
            vout_PlacePicture( p_vout, p_vout->p_sys->i_window_width,
                               p_vout->p_sys->i_window_height,
                               &i_x, &i_y, &i_width, &i_height );
            ExcludeClipRect( (HDC)wParam, i_x, i_y,
                             i_x + i_width, i_y + i_height );
        }
        break;

    default:
        //msg_Dbg( p_vout, "WinProc WM Default %i", message );
        break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

