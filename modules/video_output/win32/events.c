/*****************************************************************************
 * events.c: Windows video output events handler
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
 * Preamble: This file contains the functions related to the creation of
 *             a window and the handling of its messages (events).
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "win32touch.h"

#include <vlc_common.h>
#include <vlc_vout_display.h>

#include <stdatomic.h>
#include <windows.h>
#include <windowsx.h>                                        /* GET_X_LPARAM */

#include "events.h"
#include "common.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
struct event_thread_t
{
    vlc_object_t *obj;

    /* */
    vlc_thread_t thread;
    vlc_mutex_t  lock;
    vlc_cond_t   wait;
    bool         b_ready;
    atomic_bool  b_done;
    bool         b_error;

    /* */
    bool is_projected;

    /* Gestures */
    win32_gesture_sys_t *p_gesture;

    RECT window_area;

    /* */
    vout_window_t *parent_window;
    WCHAR class_video[256];
    HWND hparent;
    HWND hvideownd;
};

/***************************
 * Local Prototypes        *
 ***************************/
/* Window Creation */
static int  Win32VoutCreateWindow( event_thread_t * );
static void Win32VoutCloseWindow ( event_thread_t * );

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
    MSG msg;
    int canc = vlc_savecancel ();


    vlc_mutex_lock( &p_event->lock );
    /* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message queue */
    if( Win32VoutCreateWindow( p_event ) )
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

    /* Main loop */
    /* GetMessage will sleep if there's no message in the queue */
    for( ;; )
    {
        if( !GetMessage( &msg, 0, 0, 0 ) || atomic_load( &p_event->b_done ) )
        {
            break;
        }

        /* Messages we don't handle directly are dispatched to the
         * window procedure */
        TranslateMessage(&msg);
        DispatchMessage(&msg);

    } /* End Main loop */

    msg_Dbg( p_event->obj, "Win32 Vout EventThread terminating" );

    Win32VoutCloseWindow( p_event );
    vlc_restorecancel(canc);
    return NULL;
}

event_thread_t *EventThreadCreate( vlc_object_t *obj, vout_window_t *parent_window)
{
    if (parent_window->type != VOUT_WINDOW_TYPE_HWND)
        return NULL;
     /* Create the Vout EventThread, this thread is created by us to isolate
     * the Win32 PeekMessage function calls. We want to do this because
     * Windows can stay blocked inside this call for a long time, and when
     * this happens it thus blocks vlc's video_output thread.
     * Vout EventThread will take care of the creation of the video
     * window (because PeekMessage has to be called from the same thread which
     * created the window). */
    msg_Dbg( obj, "creating Vout EventThread" );
    event_thread_t *p_event = malloc( sizeof(*p_event) );
    if( !p_event )
        return NULL;

    p_event->obj = obj;
    vlc_mutex_init( &p_event->lock );
    vlc_cond_init( &p_event->wait );
    atomic_init( &p_event->b_done, false );

    p_event->parent_window = parent_window;

    _snwprintf( p_event->class_video, ARRAY_SIZE(p_event->class_video),
                TEXT("VLC video output %p"), (void *)p_event );
    return p_event;
}

void EventThreadDestroy( event_thread_t *p_event )
{
    free( p_event );
}

int EventThreadStart( event_thread_t *p_event, event_hwnd_t *p_hwnd, const event_cfg_t *p_cfg )
{
    p_event->is_projected = p_cfg->is_projected;
    p_event->window_area.left   = 0;
    p_event->window_area.top    = 0;
    p_event->window_area.right  = p_cfg->width;
    p_event->window_area.bottom = p_cfg->height;

    p_event->b_ready = false;
    atomic_store( &p_event->b_done, false);
    p_event->b_error = false;

    if( vlc_clone( &p_event->thread, EventThread, p_event,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_event->obj, "cannot create Vout EventThread" );
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
    msg_Dbg( p_event->obj, "Vout EventThread running" );

    /* */
    p_hwnd->parent_window = p_event->parent_window;
    p_hwnd->hparent       = p_event->hparent;
    p_hwnd->hvideownd     = p_event->hvideownd;
    return VLC_SUCCESS;
}

void EventThreadStop( event_thread_t *p_event )
{
    if( !p_event->b_ready )
        return;

    atomic_store( &p_event->b_done, true );

    /* we need to be sure Vout EventThread won't stay stuck in
     * GetMessage, so we send a fake message */
    if( p_event->hvideownd )
        PostMessage( p_event->hvideownd, WM_NULL, 0, 0);

    vlc_join( p_event->thread, NULL );
    p_event->b_ready = false;
}


/***********************************
 * Local functions implementations *
 ***********************************/
static long FAR PASCAL VideoEventProc( HWND hwnd, UINT message,
                                       WPARAM wParam, LPARAM lParam )
{
    if( message == WM_CREATE )
    {
        /* Store p_event for future use */
        CREATESTRUCT *c = (CREATESTRUCT *)lParam;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)c->lpCreateParams );
        return 0;
    }

    LONG_PTR p_user_data = GetWindowLongPtr( hwnd, GWLP_USERDATA );
    if( p_user_data == 0 ) /* messages before WM_CREATE */
        return DefWindowProc(hwnd, message, wParam, lParam);
    event_thread_t *p_event = (event_thread_t *)p_user_data;

    switch( message )
    {
    /* the user wants to close the window */
    case WM_CLOSE:
        vout_window_ReportClose(p_event->parent_window);
        return 0;

    /* the window has been closed so shut down everything now */
    case WM_DESTROY:
        msg_Dbg( p_event->obj, "WinProc WM_DESTROY" );
        /* just destroy the window */
        PostQuitMessage( 0 );
        return 0;

    case WM_GESTURE:
        return DecodeGesture( p_event->obj, p_event->p_gesture, hwnd, message, wParam, lParam );

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
    default:
        /* Let windows handle the message */
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

/*****************************************************************************
 * Win32VoutCreateWindow: create a window for the video.
 *****************************************************************************
 * Before creating a direct draw surface, we need to create a window in which
 * the video will be displayed. This window will also allow us to capture the
 * events.
 *****************************************************************************/
static int Win32VoutCreateWindow( event_thread_t *p_event )
{
    HINSTANCE  hInstance;
    WNDCLASS   wc;                            /* window class components */
    int        i_style;

    msg_Dbg( p_event->obj, "Win32VoutCreateWindow" );

    /* Get this module's instance */
    hInstance = GetModuleHandle(NULL);

    /* If an external window was specified, we'll draw in it. */
    assert( p_event->parent_window->type == VOUT_WINDOW_TYPE_HWND );
    p_event->hparent = p_event->parent_window->handle.hwnd;

    /* Fill in the window class structure */
    wc.style         = CS_OWNDC|CS_DBLCLKS;          /* style: dbl click */
    wc.lpfnWndProc   = (WNDPROC)VideoEventProc;         /* event handler */
    wc.cbClsExtra    = 0;                         /* no extra class data */
    wc.cbWndExtra    = 0;                        /* no extra window data */
    wc.hInstance     = hInstance;                            /* instance */
    wc.hIcon         = 0;
    wc.hCursor       = 0;
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);  /* background color */
    wc.lpszMenuName  = NULL;                                  /* no menu */
    wc.lpszClassName = p_event->class_video;      /* use a special class */

    /* Register the window class */
    if( !RegisterClass(&wc) )
    {
        msg_Err( p_event->obj, "Win32VoutCreateWindow RegisterClass FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }

    i_style = WS_VISIBLE|WS_CLIPCHILDREN|WS_CHILD|WS_DISABLED;

    /* Create the window */
    p_event->hvideownd =
        CreateWindowEx( WS_EX_NOPARENTNOTIFY | WS_EX_NOACTIVATE,
                    p_event->class_video,            /* name of window class */
                    TEXT(VOUT_TITLE) TEXT(" (VLC Video Output)"),/* window title */
                    i_style,                                 /* window style */
                    CW_USEDEFAULT,                   /* default X coordinate */
                    CW_USEDEFAULT,                   /* default Y coordinate */
                    RECTWidth(p_event->window_area),         /* window width */
                    RECTHeight(p_event->window_area),       /* window height */
                    p_event->hparent,                       /* parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    (LPVOID)p_event );           /* send vd to WM_CREATE */

    if( !p_event->hvideownd )
    {
        msg_Warn( p_event->obj, "Win32VoutCreateWindow create window FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_event->obj, "created video window" );

    /* We don't want the window owner to overwrite our client area */
    LONG  parent_style = GetWindowLong( p_event->hparent, GWL_STYLE );
    if( !(parent_style & WS_CLIPCHILDREN) )
        /* Hmmm, apparently this is a blocking call... */
        SetWindowLong( p_event->hparent, GWL_STYLE,
                       parent_style | WS_CLIPCHILDREN );

    InitGestures( p_event->hvideownd, &p_event->p_gesture, p_event->is_projected );

    /* Now display the window */
    ShowWindow( p_event->hvideownd, SW_SHOWNOACTIVATE );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Win32VoutCloseWindow: close the window created by Win32VoutCreateWindow
 *****************************************************************************
 * This function returns all resources allocated by Win32VoutCreateWindow.
 *****************************************************************************/
static void Win32VoutCloseWindow( event_thread_t *p_event )
{
    msg_Dbg( p_event->obj, "Win32VoutCloseWindow" );

    DestroyWindow( p_event->hvideownd );

    HINSTANCE hInstance = GetModuleHandle(NULL);
    UnregisterClass( p_event->class_video, hInstance );

    CloseGestures( p_event->p_gesture);
}
