/*****************************************************************************
 * glwin32.c: Windows OpenGL provider
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

#include <errno.h>                                                 /* ENOMEM */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_vout.h>

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
static void FirstSwap( vout_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_shortname( "OpenGL" )
    set_description( N_("OpenGL video output") )
    set_capability( "opengl provider", 100 )
    add_shortcut( "glwin32" )
    set_callbacks( OpenVideo, CloseVideo )

    /* FIXME: Hack to avoid unregistering our window class */
    linked_with_a_crap_library_which_uses_atexit ()
vlc_module_end ()

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

    /* Allocate structure */
    p_vout->p_sys = calloc( 1, sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    /* Initialisations */
    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_swap = FirstSwap;

    p_vout->p_sys->hwnd = p_vout->p_sys->hvideownd = NULL;
    p_vout->p_sys->hparent = p_vout->p_sys->hfswnd = NULL;
    p_vout->p_sys->i_changes = 0;
    vlc_mutex_init( &p_vout->p_sys->lock );
    SetRectEmpty( &p_vout->p_sys->rect_display );
    SetRectEmpty( &p_vout->p_sys->rect_parent );

    var_Create( p_vout, "video-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    p_vout->p_sys->b_cursor_hidden = 0;
    p_vout->p_sys->i_lastmoved = mdate();
    p_vout->p_sys->i_mouse_hide_timeout =
        var_GetInteger(p_vout, "mouse-hide-timeout") * 1000;

    /* Set main window's size */
    p_vout->p_sys->i_window_width = p_vout->i_window_width;
    p_vout->p_sys->i_window_height = p_vout->i_window_height;

    /* Create the Vout EventThread, this thread is created by us to isolate
     * the Win32 PeekMessage function calls. We want to do this because
     * Windows can stay blocked inside this call for a long time, and when
     * this happens it thus blocks vlc's video_output thread.
     * Vout EventThread will take care of the creation of the video
     * window (because PeekMessage has to be called from the same thread which
     * created the window). */
    msg_Dbg( p_vout, "creating Vout EventThread" );
    p_vout->p_sys->p_event =
        vlc_object_create( p_vout, sizeof(event_thread_t) );
    p_vout->p_sys->p_event->p_vout = p_vout;
    p_vout->p_sys->p_event->window_ready = CreateEvent( NULL, TRUE, FALSE, NULL );
    if( vlc_thread_create( p_vout->p_sys->p_event, "Vout Events Thread",
                           EventThread, 0 ) )
    {
        msg_Err( p_vout, "cannot create Vout EventThread" );
        CloseHandle( p_vout->p_sys->p_event->window_ready );
        vlc_object_release( p_vout->p_sys->p_event );
        p_vout->p_sys->p_event = NULL;
        goto error;
    }
    WaitForSingleObject( p_vout->p_sys->p_event->window_ready, INFINITE );
    CloseHandle( p_vout->p_sys->p_event->window_ready );

    if( p_vout->p_sys->p_event->b_error )
    {
        msg_Err( p_vout, "Vout EventThread failed" );
        goto error;
    }

    vlc_object_attach( p_vout->p_sys->p_event, p_vout );

    msg_Dbg( p_vout, "Vout EventThread running" );

    /* Variable to indicate if the window should be on top of others */
    /* Trigger a callback right now */
    var_TriggerCallback( p_vout, "video-on-top" );

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

    if( p_vout->b_fullscreen )
    {
        msg_Dbg( p_vout, "Quitting fullscreen" );
        Win32ToggleFullscreen( p_vout );
        /* Force fullscreen in the core for the next video */
        var_SetBool( p_vout, "fullscreen", true );
    }

    if( p_vout->p_sys->p_event )
    {
        vlc_object_detach( p_vout->p_sys->p_event );

        /* Kill Vout EventThread */
        vlc_object_kill( p_vout->p_sys->p_event );

        /* we need to be sure Vout EventThread won't stay stuck in
         * GetMessage, so we send a fake message */
        if( p_vout->p_sys->hwnd )
        {
            PostMessage( p_vout->p_sys->hwnd, WM_NULL, 0, 0);
        }

        vlc_thread_join( p_vout->p_sys->p_event );
        vlc_object_release( p_vout->p_sys->p_event );
    }

    vlc_mutex_destroy( &p_vout->p_sys->lock );

    free( p_vout->p_sys );
    p_vout->p_sys = NULL;
}

/*****************************************************************************
 * Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by the video output thread.
 * It returns a non null value if an error occurred.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
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
    if( p_vout->b_fullscreen && !p_vout->p_sys->b_cursor_hidden &&
        (mdate() - p_vout->p_sys->i_lastmoved) >
            p_vout->p_sys->i_mouse_hide_timeout )
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

        p_vout->p_sys->b_on_top_change = false;
    }

    /* Check if the event thread is still running */
    if( !vlc_object_alive (p_vout->p_sys->p_event) )
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

/*
** this function is only used once when the first picture is received
** this function will show the video window once a picture is ready
*/

static void FirstSwap( vout_thread_t *p_vout )
{
    /* get initial picture buffer swapped to front buffer */
    GLSwapBuffers( p_vout );

    /*
    ** Video window is initially hidden, show it now since we got a
    ** picture to show.
    */
    SetWindowPos( p_vout->p_sys->hvideownd, NULL, 0, 0, 0, 0,
        SWP_ASYNCWINDOWPOS|
        SWP_FRAMECHANGED|
        SWP_SHOWWINDOW|
        SWP_NOMOVE|
        SWP_NOSIZE|
        SWP_NOZORDER );

    /* use and restores proper swap function for further pictures */
    p_vout->pf_swap = GLSwapBuffers;
}
