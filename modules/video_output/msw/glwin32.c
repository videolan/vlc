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
    p_vout->pf_control = Control;

    if( CommonInit( p_vout ) )
        goto error;

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
    EventThreadUpdateTitle( p_vout->p_sys->p_event, VOUT_TITLE " (OpenGL output)" );

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

    CommonClean( p_vout );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by the video output thread.
 * It returns a non null value if an error occurred.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    const int i_width  = p_sys->rect_dest.right - p_sys->rect_dest.left;
    const int i_height = p_sys->rect_dest.bottom - p_sys->rect_dest.top;
    glViewport( 0, 0, i_width, i_height );

    CommonManage( p_vout );
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
