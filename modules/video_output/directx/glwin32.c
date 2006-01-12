/*****************************************************************************
 * glwin32.c: Windows OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
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

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#include <windows.h>
#include <ddraw.h>
#include <commctrl.h>

#include <multimon.h>
#undef GetSystemMetrics

#ifndef MONITOR_DEFAULTTONEAREST
#   define MONITOR_DEFAULTTONEAREST 2
#endif

#include <GL/gl.h>

#include "vout.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  OpenVideo  ( vlc_object_t * );
static void CloseVideo ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void GLSwapBuffers( vout_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
    set_shortname( "OpenGL" );
    set_description( _("OpenGL video output") );
    set_capability( "opengl provider", 100 );
    add_shortcut( "glwin32" );
    set_callbacks( OpenVideo, CloseVideo );

    /* FIXME: Hack to avoid unregistering our window class */
    linked_with_a_crap_library_which_uses_atexit( );
vlc_module_end();

#if 0 /* FIXME */
    /* check if we registered a window class because we need to
     * unregister it */
    WNDCLASS wndclass;
    if( GetClassInfo( GetModuleHandle(NULL), "VLC DirectX", &wndclass ) )
        UnregisterClass( "VLC DirectX", GetModuleHandle(NULL) );
#endif

/*****************************************************************************
 * OpenVideo: allocate OpenGL provider
 *****************************************************************************
 * This function creates and initializes a video window.
 *****************************************************************************/
static int OpenVideo( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    vlc_value_t val;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }
    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    /* Initialisations */
    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_swap = GLSwapBuffers;

    p_vout->p_sys->p_ddobject = NULL;
    p_vout->p_sys->p_display = NULL;
    p_vout->p_sys->p_current_surface = NULL;
    p_vout->p_sys->p_clipper = NULL;
    p_vout->p_sys->hwnd = p_vout->p_sys->hvideownd = NULL;
    p_vout->p_sys->hparent = p_vout->p_sys->hfswnd = NULL;
    p_vout->p_sys->i_changes = 0;
    p_vout->p_sys->b_wallpaper = 0;
    vlc_mutex_init( p_vout, &p_vout->p_sys->lock );
    SetRectEmpty( &p_vout->p_sys->rect_display );
    SetRectEmpty( &p_vout->p_sys->rect_parent );

    var_Create( p_vout, "video-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    p_vout->p_sys->b_cursor_hidden = 0;
    p_vout->p_sys->i_lastmoved = mdate();

    /* Set main window's size */
    p_vout->p_sys->i_window_width = p_vout->i_window_width;
    p_vout->p_sys->i_window_height = p_vout->i_window_height;

    /* Create the DirectXEventThread, this thread is created by us to isolate
     * the Win32 PeekMessage function calls. We want to do this because
     * Windows can stay blocked inside this call for a long time, and when
     * this happens it thus blocks vlc's video_output thread.
     * DirectXEventThread will take care of the creation of the video
     * window (because PeekMessage has to be called from the same thread which
     * created the window). */
    msg_Dbg( p_vout, "creating DirectXEventThread" );
    p_vout->p_sys->p_event =
        vlc_object_create( p_vout, sizeof(event_thread_t) );
    p_vout->p_sys->p_event->p_vout = p_vout;
    if( vlc_thread_create( p_vout->p_sys->p_event, "DirectX Events Thread",
                           E_(DirectXEventThread), 0, 1 ) )
    {
        msg_Err( p_vout, "cannot create DirectXEventThread" );
        vlc_object_destroy( p_vout->p_sys->p_event );
        p_vout->p_sys->p_event = NULL;
        goto error;
    }

    if( p_vout->p_sys->p_event->b_error )
    {
        msg_Err( p_vout, "DirectXEventThread failed" );
        goto error;
    }

    vlc_object_attach( p_vout->p_sys->p_event, p_vout );

    msg_Dbg( p_vout, "DirectXEventThread running" );

    /* Variable to indicate if the window should be on top of others */
    /* Trigger a callback right now */
    var_Get( p_vout, "video-on-top", &val );
    var_Set( p_vout, "video-on-top", val );

    return VLC_SUCCESS;

 error:
    CloseVideo( VLC_OBJECT(p_vout) );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Init: initialize video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    PIXELFORMATDESCRIPTOR pfd;
    int iFormat;

    /* Change the window title bar text */
    PostMessage( p_vout->p_sys->hwnd, WM_VLC_CHANGE_TEXT, 0, 0 );

    p_vout->p_sys->hGLDC = GetDC( p_vout->p_sys->hvideownd );

    /* Set the pixel format for the DC */
    memset( &pfd, 0, sizeof( pfd ) );
    pfd.nSize = sizeof( pfd );
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;
    iFormat = ChoosePixelFormat( p_vout->p_sys->hGLDC, &pfd );
    SetPixelFormat( p_vout->p_sys->hGLDC, iFormat, &pfd );

    /* Create and enable the render context */
    p_vout->p_sys->hGLRC = wglCreateContext( p_vout->p_sys->hGLDC );
    wglMakeCurrent( p_vout->p_sys->hGLDC, p_vout->p_sys->hGLRC );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by Create.
 * It is called at the end of the thread.
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    wglMakeCurrent( NULL, NULL );
    wglDeleteContext( p_vout->p_sys->hGLRC );
    ReleaseDC( p_vout->p_sys->hvideownd, p_vout->p_sys->hGLDC );
    return;
}

/*****************************************************************************
 * CloseVideo: destroy Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void CloseVideo( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    msg_Dbg( p_vout, "CloseVideo" );

    if( p_vout->p_sys->p_event )
    {
        vlc_object_detach( p_vout->p_sys->p_event );

        /* Kill DirectXEventThread */
        p_vout->p_sys->p_event->b_die = VLC_TRUE;

        /* we need to be sure DirectXEventThread won't stay stuck in
         * GetMessage, so we send a fake message */
        if( p_vout->p_sys->hwnd )
        {
            PostMessage( p_vout->p_sys->hwnd, WM_NULL, 0, 0);
        }

        vlc_thread_join( p_vout->p_sys->p_event );
        vlc_object_destroy( p_vout->p_sys->p_event );
    }

    vlc_mutex_destroy( &p_vout->p_sys->lock );

    if( p_vout->p_sys )
    {
        free( p_vout->p_sys );
        p_vout->p_sys = NULL;
    }
}

/*****************************************************************************
 * Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by the video output thread.
 * It returns a non null value if an error occurred.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    WINDOWPLACEMENT window_placement;

    int i_width = p_vout->p_sys->rect_dest.right -
        p_vout->p_sys->rect_dest.left;
    int i_height = p_vout->p_sys->rect_dest.bottom -
        p_vout->p_sys->rect_dest.top;
    glViewport( 0, 0, i_width, i_height );

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

            /* This one is to force the update even if only
             * the position has changed */
            SetWindowPos( p_vout->p_sys->hwnd, 0, 1, 1,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, 0 );

            SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                          rect_parent.right - rect_parent.left,
                          rect_parent.bottom - rect_parent.top, 0 );
        }
    }
    else
    {
        vlc_mutex_unlock( &p_vout->p_sys->lock );
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
        E_(DirectXUpdateRects)( p_vout, VLC_TRUE );
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
        vlc_value_t val;
        HWND hwnd = (p_vout->p_sys->hparent && p_vout->p_sys->hfswnd) ?
            p_vout->p_sys->hfswnd : p_vout->p_sys->hwnd;

        p_vout->b_fullscreen = ! p_vout->b_fullscreen;

        /* We need to switch between Maximized and Normal sized window */
        window_placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement( hwnd, &window_placement );
        if( p_vout->b_fullscreen )
        {
            /* Change window style, no borders and no title bar */
            int i_style = WS_CLIPCHILDREN | WS_VISIBLE;
            SetWindowLong( hwnd, GWL_STYLE, i_style );

            if( p_vout->p_sys->hparent )
            {
                /* Retrieve current window position so fullscreen will happen
                 * on the right screen */
                POINT point = {0,0};
                RECT rect;
                ClientToScreen( p_vout->p_sys->hwnd, &point );
                GetClientRect( p_vout->p_sys->hwnd, &rect );
                SetWindowPos( hwnd, 0, point.x, point.y,
                              rect.right, rect.bottom,
                              SWP_NOZORDER|SWP_FRAMECHANGED );
                GetWindowPlacement( hwnd, &window_placement );
            }

            /* Maximize window */
            window_placement.showCmd = SW_SHOWMAXIMIZED;
            SetWindowPlacement( hwnd, &window_placement );
            SetWindowPos( hwnd, 0, 0, 0, 0, 0,
                          SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);

            if( p_vout->p_sys->hparent )
            {
                RECT rect;
                GetClientRect( hwnd, &rect );
                SetParent( p_vout->p_sys->hwnd, hwnd );
                SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                              rect.right, rect.bottom,
                              SWP_NOZORDER|SWP_FRAMECHANGED );
            }

            SetForegroundWindow( hwnd );
        }
        else
        {
            /* Change window style, no borders and no title bar */
            SetWindowLong( hwnd, GWL_STYLE, p_vout->p_sys->i_window_style );

            /* Normal window */
            window_placement.showCmd = SW_SHOWNORMAL;
            SetWindowPlacement( hwnd, &window_placement );
            SetWindowPos( hwnd, 0, 0, 0, 0, 0,
                          SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);

            if( p_vout->p_sys->hparent )
            {
                RECT rect;
                GetClientRect( p_vout->p_sys->hparent, &rect );
                SetParent( p_vout->p_sys->hwnd, p_vout->p_sys->hparent );
                SetWindowPos( p_vout->p_sys->hwnd, 0, 0, 0,
                              rect.right, rect.bottom,
                              SWP_NOZORDER|SWP_FRAMECHANGED );

                ShowWindow( hwnd, SW_HIDE );
                SetForegroundWindow( p_vout->p_sys->hparent );
            }

            /* Make sure the mouse cursor is displayed */
            PostMessage( p_vout->p_sys->hwnd, WM_VLC_SHOW_MOUSE, 0, 0 );
        }

        /* Update the object variable and trigger callback */
        val.b_bool = p_vout->b_fullscreen;
        var_Set( p_vout, "fullscreen", val );

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
        p_vout->p_sys->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /*
     * Pointer change
     */
    if( p_vout->b_fullscreen && !p_vout->p_sys->b_cursor_hidden &&
        (mdate() - p_vout->p_sys->i_lastmoved) > 5000000 )
    {
        POINT point;
        HWND hwnd;

        /* Hide the cursor only if it is inside our window */
        GetCursorPos( &point );
        hwnd = WindowFromPoint(point);
        if( hwnd == p_vout->p_sys->hwnd || hwnd == p_vout->p_sys->hvideownd )
        {
            PostMessage( p_vout->p_sys->hwnd, WM_VLC_HIDE_MOUSE, 0, 0 );
        }
        else
        {
            p_vout->p_sys->i_lastmoved = mdate();
        }
    }

    /*
     * "Always on top" status change
     */
    if( p_vout->p_sys->b_on_top_change )
    {
        vlc_value_t val;
        HMENU hMenu = GetSystemMenu( p_vout->p_sys->hwnd, FALSE );

        var_Get( p_vout, "video-on-top", &val );

        /* Set the window on top if necessary */
        if( val.b_bool && !( GetWindowLong( p_vout->p_sys->hwnd, GWL_EXSTYLE )
                           & WS_EX_TOPMOST ) )
        {
            CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                           MF_BYCOMMAND | MFS_CHECKED );
            SetWindowPos( p_vout->p_sys->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOMOVE );
        }
        else
        /* The window shouldn't be on top */
        if( !val.b_bool && ( GetWindowLong( p_vout->p_sys->hwnd, GWL_EXSTYLE )
                           & WS_EX_TOPMOST ) )
        {
            CheckMenuItem( hMenu, IDM_TOGGLE_ON_TOP,
                           MF_BYCOMMAND | MFS_UNCHECKED );
            SetWindowPos( p_vout->p_sys->hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOMOVE );
        }

        p_vout->p_sys->b_on_top_change = VLC_FALSE;
    }

    /* Check if the event thread is still running */
    if( p_vout->p_sys->p_event->b_die )
    {
        return VLC_EGENERIC; /* exit */
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GLSwapBuffers: swap front/back buffers
 *****************************************************************************/
static void GLSwapBuffers( vout_thread_t *p_vout )
{
    SwapBuffers( p_vout->p_sys->hGLDC );
}

int E_(DirectXUpdateOverlay)( vout_thread_t *p_vout )
{
    return 1;
}
