/*****************************************************************************
 * vout_events.c: Windows DirectX video output events handler
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vout_events.c,v 1.4 2001/07/30 00:53:04 sam Exp $
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
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "netutils.h"

#include "video.h"
#include "video_output.h"

#include <windows.h>
#include <windowsx.h>

#if defined( _MSC_VER )
#   include <ddraw.h>
#else
#   include <directx.h>
#endif

#include "intf_msg.h"
#include "interface.h"
#include "main.h"

#include "modules.h"
#include "modules_export.h"

#include "vout_directx.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  DirectXCreateWindow( vout_thread_t *p_vout );
static void DirectXCloseWindow ( vout_thread_t *p_vout );
static long FAR PASCAL DirectXEventProc ( HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam );

/*****************************************************************************
 * Global variables.
 * I really hate them, but here I don't have any choice. And anyway, this
 * shouldn't really affect reentrancy.
 * This variable is used to know if we have to update the overlay position
 * and size. This is to fix the bug we've got when the Windows option, to show
 * the content of a window when you drag it, is enabled.
 *****************************************************************************/
int b_directx_update_overlay = 0;

/*****************************************************************************
 * DirectXEventThread: Create video window & handle its messages
 *****************************************************************************
 * This function creates a video window and then enters an infinite loop
 * that handles the messages sent to that window.
 * The main goal of this thread is to isolate the Win32 PeekMessage function
 * because this one can block for a long time.
 *****************************************************************************/
void DirectXEventThread( vout_thread_t *p_vout )
{
    MSG             msg;
    boolean_t       b_dispatch_msg = TRUE;

    /* Initialisation */

    /* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message qeue */
    if( DirectXCreateWindow( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't create window" );
        p_vout->p_sys->i_event_thread_status = THREAD_FATAL;
        /* signal the creation of the window */
        vlc_mutex_lock( &p_vout->p_sys->event_thread_lock );
        vlc_cond_signal( &p_vout->p_sys->event_thread_wait );
        vlc_mutex_unlock( &p_vout->p_sys->event_thread_lock );
        return;
    }

    /* signal the creation of the window */
    p_vout->p_sys->i_event_thread_status = THREAD_READY;
    vlc_mutex_lock( &p_vout->p_sys->event_thread_lock );
    vlc_cond_signal( &p_vout->p_sys->event_thread_wait );
    vlc_mutex_unlock( &p_vout->p_sys->event_thread_lock );

    /* Main loop */
    while( !p_vout->b_die && !p_vout->p_sys->b_event_thread_die )
    {

        /* GetMessage will sleep if there's no message in the queue */
        if( GetMessage( &msg, NULL, 0, 0 ) >= 0 )
        {
            switch( msg.message )
            {
                
            case WM_CLOSE:
                intf_WarnMsg( 3, "vout: vout_Manage WM_CLOSE" );
                break;
                
            case WM_QUIT:
                intf_WarnMsg( 3, "vout: vout_Manage WM_QUIT" );
                p_vout->p_sys->b_event_thread_die = 1;
                p_main->p_intf->b_die = 1;
                break;
                
            case WM_MOVE:
                intf_WarnMsg( 3, "vout: vout_Manage WM_MOVE" );
                if( !p_vout->b_need_render )
                {
                    p_vout->p_sys->i_changes |= VOUT_SIZE_CHANGE;
                }
                /* don't create a never ending loop */
                b_dispatch_msg = FALSE;
                break;
          
            case WM_APP:
                intf_WarnMsg( 3, "vout: vout_Manage WM_APP" );
                if( !p_vout->b_need_render )
                {
                    p_vout->p_sys->i_changes |= VOUT_SIZE_CHANGE;
                }
                /* size change has been handled (to fix a bug)*/
                b_directx_update_overlay = 0;
                /* don't create a never ending loop */
                b_dispatch_msg = FALSE;
                break;
              
#if 0
            case WM_PAINT:
                intf_WarnMsg( 4, "vout: vout_Manage WM_PAINT" );
                break;
              
            case WM_ERASEBKGND:
                intf_WarnMsg( 4, "vout: vout_Manage WM_ERASEBKGND" );
                break;
#endif
              
            case WM_MOUSEMOVE:
                intf_WarnMsg( 4, "vout: vout_Manage WM_MOUSEMOVE" );
                if( p_vout->p_sys->b_cursor )
                {
                    if( p_vout->p_sys->b_cursor_autohidden )
                    {
                        p_vout->p_sys->b_cursor_autohidden = 0;
                        p_vout->p_sys->i_lastmoved = mdate();
                        ShowCursor( TRUE );
                    }
                    else
                    {
                        p_vout->p_sys->i_lastmoved = mdate();
                    }
                }               
                break;
                
            case WM_RBUTTONUP:
                intf_WarnMsg( 4, "vout: vout_Manage WM_RBUTTONUP" );
                p_main->p_intf->b_menu_change = 1;
                break;

            case WM_KEYDOWN:
                /* the key events are first processed here. The next
                 * message processed by this main message loop will be the
                 * char translation of the key event */
                intf_WarnMsg( 3, "vout: vout_Manage WM_KEYDOWN" );
                switch( msg.wParam )
                {
                case VK_ESCAPE:
                case VK_F12:
                    PostQuitMessage( 0 );
                    break;
                }
                TranslateMessage(&msg);
                b_dispatch_msg = FALSE;
                break;
              
            case WM_CHAR:
                intf_WarnMsg( 3, "vout: vout_Manage WM_CHAR" );
                switch( msg.wParam )
                {
                case 'q':
                case 'Q':
                    PostQuitMessage( 0 );
                    break;
                  
                case 'f':                            /* switch to fullscreen */
                case 'F':
                    p_vout->p_sys->i_changes |= VOUT_FULLSCREEN_CHANGE;
                    break;
                  
                case 'y':                              /* switch to hard YUV */
                case 'Y':
                    p_vout->p_sys->i_changes |= VOUT_YUV_CHANGE;
                    break;
                  
                case 'c':                                /* toggle grayscale */
                case 'C':
                    p_vout->b_grayscale = ! p_vout->b_grayscale;
                    p_vout->p_sys->i_changes |= VOUT_GRAYSCALE_CHANGE;
                    break;
                  
                case 'i':                                     /* toggle info */
                case 'I':
                    p_vout->b_info = ! p_vout->b_info;
                    p_vout->p_sys->i_changes |= VOUT_INFO_CHANGE;
                    break;
                  
                case 's':                                  /* toggle scaling */
                case 'S':
                    p_vout->b_scale = ! p_vout->b_scale;
                    p_vout->p_sys->i_changes |= VOUT_SCALE_CHANGE;
                    break;
                  
                case ' ':                                /* toggle interface */
                    p_vout->b_interface = ! p_vout->b_interface;
                    p_vout->p_sys->i_changes |= VOUT_INTF_CHANGE;
                    break;
                  
                case '0': network_ChannelJoin( 0 ); break;
                case '1': network_ChannelJoin( 1 ); break;
                case '2': network_ChannelJoin( 2 ); break;
                case '3': network_ChannelJoin( 3 ); break;
                case '4': network_ChannelJoin( 4 ); break;
                case '5': network_ChannelJoin( 5 ); break;
                case '6': network_ChannelJoin( 6 ); break;
                case '7': network_ChannelJoin( 7 ); break;
                case '8': network_ChannelJoin( 8 ); break;
                case '9': network_ChannelJoin( 9 ); break;
                  
                default:
                    if( intf_ProcessKey( p_main->p_intf,
                                         (char )msg.wParam ) )
                    {
                        intf_DbgMsg( "unhandled key '%c' (%i)",
                                     (char)msg.wParam, msg.wParam );
                    }
                    break;
                }
              
#if 0          
            default:
                intf_WarnMsg( 4, "vout: vout_Manage WM Default %i",
                              msg.message );
                break;
#endif

            } /* End Switch */

            /* don't create a never ending loop */
            if( b_dispatch_msg )
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            b_dispatch_msg = TRUE;

        } /* if( GetMessage() ) */
        else
        {
            intf_ErrMsg("vout error: GetMessage failed in DirectXEventThread");
            p_vout->p_sys->b_event_thread_die = 1;
        } /* End if( GetMessage() ) */


    } /* End Main loop */

    /* Destroy the window */
    DirectXCloseWindow( p_vout );

    /* Set thread Status */
    p_vout->p_sys->i_event_thread_status = THREAD_OVER;

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
    WNDCLASSEX wc;                                /* window class components */
    RECT       rect_window;
    COLORREF   colorkey; 
    HDC        hdc;

    intf_WarnMsg( 3, "vout: DirectXCreateWindow" );

    /* get this module's instance */
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
    hdc = GetDC( GetDesktopWindow() );
    for( colorkey = 5; colorkey < 0xFF /*all shades of red*/; colorkey++ )
    {
        if( colorkey == GetNearestColor( hdc, colorkey ) )
          break;
    }
    intf_WarnMsg(3,"vout: DirectXCreateWindow background color:%i", colorkey);
    ReleaseDC( p_vout->p_sys->hwnd, hdc );

    /* create the actual brush */  
    p_vout->p_sys->hbrush = CreateSolidBrush(colorkey);
    p_vout->p_sys->i_colorkey = (int)colorkey;

    /* fill in the window class structure */
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;                               /* no special styles */
    wc.lpfnWndProc   = (WNDPROC)DirectXEventProc;           /* event handler */
    wc.cbClsExtra    = 0;                             /* no extra class data */
    wc.cbWndExtra    = 0;                            /* no extra window data */
    wc.hInstance     = hInstance;                                /* instance */
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION); /* load the vlc icon */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW); /* load a default cursor */
    wc.hbrBackground = p_vout->p_sys->hbrush;            /* background color */
    wc.lpszMenuName  = NULL;                                      /* no menu */
    wc.lpszClassName = "VLC DirectX";                 /* use a special class */
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION); /* load the vlc icon */

    /* register the window class */
    if (!RegisterClassEx(&wc))
    {
        intf_WarnMsg( 3, "vout: DirectXCreateWindow register window FAILED" );
        return (1);
    }

    /* when you create a window you give the dimensions you wish it to have.
     * Unfortunatly these dimensions will include the borders and title bar.
     * We use the following function to find out the size of the window
     * corresponding to the useable surface we want */
    rect_window.top    = 10;
    rect_window.left   = 10;
    rect_window.right  = rect_window.left + p_vout->p_sys->i_window_width;
    rect_window.bottom = rect_window.top + p_vout->p_sys->i_window_height;
    AdjustWindowRect( &rect_window, WS_OVERLAPPEDWINDOW|WS_SIZEBOX, 0 );

    /* create the window */
    p_vout->p_sys->hwnd = CreateWindow("VLC DirectX",/* name of window class */
                    "VLC DirectX",                  /* window title bar text */
                    WS_OVERLAPPEDWINDOW
                    | WS_SIZEBOX | WS_VISIBLE,               /* window style */
                    10,                              /* default X coordinate */
                    10,                              /* default Y coordinate */
                    rect_window.right - rect_window.left,    /* window width */
                    rect_window.bottom - rect_window.top,   /* window height */
                    NULL,                                /* no parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    NULL);                        /* no additional arguments */

    if (p_vout->p_sys->hwnd == NULL) {
        intf_WarnMsg( 3, "vout: DirectXCreateWindow create window FAILED" );
        return (1);
    }

    /* now display the window */
    ShowWindow(p_vout->p_sys->hwnd, SW_SHOW);

    return ( 0 );
}

/*****************************************************************************
 * DirectXCloseWindow: close the window created by DirectXCreateWindow
 *****************************************************************************
 * This function returns all resources allocated by DirectXCreateWindow.
 *****************************************************************************/
static void DirectXCloseWindow( vout_thread_t *p_vout )
{
    HINSTANCE hInstance;

    intf_WarnMsg( 3, "vout: DirectXCloseWindow" );
    if( p_vout->p_sys->hwnd != NULL )
    {
        DestroyWindow( p_vout->p_sys->hwnd);
        p_vout->p_sys->hwnd = NULL;
    }

    hInstance = GetModuleHandle(NULL);
    UnregisterClass( "VLC DirectX",                            /* class name */
                     hInstance );          /* handle to application instance */

    /* free window background brush */
    if( p_vout->p_sys->hwnd != NULL )
    {
        DeleteObject( p_vout->p_sys->hbrush );
        p_vout->p_sys->hbrush = NULL;
    }
}

/*****************************************************************************
 * DirectXEventProc: This is the window event processing function.
 *****************************************************************************
 * On Windows, when you create a window you have to attach an event processing
 * function to it. The aim of this function is to manage "Queued Messages" and
 * "Nonqueued Messages".
 * Queued Messages are those picked up and retransmitted by vout_Manage
 * (using the GetMessage function).
 * Nonqueued Messages are those that Windows will send directly to this
 * function (like WM_DESTROY, WM_WINDOWPOSCHANGED...)
 *****************************************************************************/
static long FAR PASCAL DirectXEventProc( HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam )
{
    switch( message )
    {

#if 0
    case WM_APP:
        intf_WarnMsg( 3, "vout: WinProc WM_APP" );
        break;

    case WM_ACTIVATE:
        intf_WarnMsg( 4, "vout: WinProc WM_ACTIVED" );
        break;

    case WM_CREATE:
        intf_WarnMsg( 4, "vout: WinProc WM_CREATE" );
        break;

    /* the user wants to close the window */
    case WM_CLOSE:
        intf_WarnMsg( 4, "vout: WinProc WM_CLOSE" );
        break;
#endif

    /* the window has been closed so shut down everything now */
    case WM_DESTROY:
        intf_WarnMsg( 4, "vout: WinProc WM_DESTROY" );
        PostQuitMessage( 0 );
        break;

    case WM_SYSCOMMAND:
        switch (wParam)
        {
            case SC_SCREENSAVE:                     /* catch the screensaver */
            case SC_MONITORPOWER:              /* catch the monitor turn-off */
            intf_WarnMsg( 3, "vout: WinProc WM_SYSCOMMAND" );
            return 0;                      /* this stops them from happening */
        }
        break;

#if 0
    case WM_MOVE:
        intf_WarnMsg( 4, "vout: WinProc WM_MOVE" );
        break;

    case WM_SIZE:
        intf_WarnMsg( 4, "vout: WinProc WM_SIZE" );
        break;

    case WM_MOVING:
        intf_WarnMsg( 4, "vout: WinProc WM_MOVING" );
        break;

    case WM_ENTERSIZEMOVE:
        intf_WarnMsg( 4, "vout: WinProc WM_ENTERSIZEMOVE" );
        break;

    case WM_SIZING:
        intf_WarnMsg( 4, "vout: WinProc WM_SIZING" );
        break;
#endif

    case WM_WINDOWPOSCHANGED:
        intf_WarnMsg( 3, "vout: WinProc WM_WINDOWPOSCHANGED" );
        b_directx_update_overlay = 1;
        PostMessage( hwnd, WM_APP, 0, 0);
        break;

#if 0
    case WM_WINDOWPOSCHANGING:
        intf_WarnMsg( 3, "vout: WinProc WM_WINDOWPOSCHANGING" );
        break;

    case WM_PAINT:
        intf_WarnMsg( 4, "vout: WinProc WM_PAINT" );
        break;

    case WM_ERASEBKGND:
        intf_WarnMsg( 4, "vout: WinProc WM_ERASEBKGND" );
        break;

    default:
        intf_WarnMsg( 4, "vout: WinProc WM Default %i", message );
        break;
#endif
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}
