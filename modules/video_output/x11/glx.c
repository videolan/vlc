/*****************************************************************************
 * glx.c: GLX OpenGL provider
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#ifdef HAVE_SYS_SHM_H
#   include <sys/shm.h>                                /* shmget(), shmctl() */
#endif

#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef HAVE_SYS_SHM_H
#   include <X11/extensions/XShm.h>
#endif
#ifdef DPMSINFO_IN_DPMS_H
#   include <X11/extensions/dpms.h>
#endif

#include <GL/glx.h>

#include "xcommon.h"

/* RV16 */
//#define VLCGL_RGB_FORMAT GL_RGB
//#define VLCGL_RGB_TYPE GL_UNSIGNED_SHORT_5_6_5

/* RV24 */
//#define VLCGL_RGB_FORMAT GL_RGB
//#define VLCGL_RGB_TYPE GL_UNSIGNED_BYTE

/* RV32 */
#define VLCGL_RGB_FORMAT GL_RGBA
#define VLCGL_RGB_TYPE GL_UNSIGNED_BYTE


/*****************************************************************************
 * OpenGL provider interface
 *****************************************************************************/
static int  CreateOpenGL ( vlc_object_t * );
static void DestroyOpenGL( vlc_object_t * );
static int  InitOpenGL   ( vout_thread_t * );
static void SwapBuffers  ( vout_thread_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CheckGLX     ( vlc_object_t *, vlc_bool_t * );
static int  InitGLX12    ( vout_thread_t * );
static int  InitGLX13    ( vout_thread_t * );
static void SwitchContext( vout_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ADAPTOR_TEXT N_("XVideo adaptor number")
#define ADAPTOR_LONGTEXT N_( \
    "If your graphics card provides several adaptors, you have " \
    "to choose which one will be used (you shouldn't have to change this).")

#define ALT_FS_TEXT N_("Alternate fullscreen method")
#define ALT_FS_LONGTEXT N_( \
    "There are two ways to make a fullscreen window, unfortunately each one " \
    "has its drawbacks.\n" \
    "1) Let the window manager handle your fullscreen window (default), but " \
    "things like taskbars will likely show on top of the video.\n" \
    "2) Completely bypass the window manager, but then nothing will be able " \
    "to show on top of the video.")

#define DISPLAY_TEXT N_("X11 display")
#define DISPLAY_LONGTEXT N_( \
    "X11 hardware display to use. By default VLC will " \
    "use the value of the DISPLAY environment variable.")

#define SCREEN_TEXT N_("Screen for fullscreen mode.")
#define SCREEN_LONGTEXT N_( \
    "Screen to use in fullscreen mode. For instance " \
    "set it to 0 for first screen, 1 for the second.")

vlc_module_begin();
    set_shortname( "OpenGL" );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
    set_description( _("OpenGL video output") );
    set_capability( "opengl provider", 50 );
    set_callbacks( CreateOpenGL, DestroyOpenGL );

    add_string( "glx-display", NULL, NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, VLC_TRUE );
    add_integer( "glx-adaptor", -1, NULL, ADAPTOR_TEXT, ADAPTOR_LONGTEXT, VLC_TRUE );
    add_bool( "glx-altfullscreen", 0, NULL, ALT_FS_TEXT, ALT_FS_LONGTEXT, VLC_TRUE );
#ifdef HAVE_XINERAMA
    add_integer ( "glx-xineramascreen", 0, NULL, SCREEN_TEXT, SCREEN_LONGTEXT, VLC_TRUE );
#endif
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
extern int  E_(Activate)   ( vlc_object_t * );
extern void E_(Deactivate) ( vlc_object_t * );

/*****************************************************************************
 * CreateOpenGL: initialize an OpenGL provider
 *****************************************************************************/
static int CreateOpenGL( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vlc_bool_t b_glx13;

    if( CheckGLX( p_this, &b_glx13 ) != VLC_SUCCESS )
    {
        msg_Err( p_vout, "no GLX support" );
        return VLC_EGENERIC;
    }

    if( E_(Activate)( p_this ) != VLC_SUCCESS )
    {
        return VLC_EGENERIC;
    }

    /* Set the function pointer */
    p_vout->pf_init = InitOpenGL;
    p_vout->pf_swap = SwapBuffers;
    p_vout->p_sys->b_glx13 = b_glx13;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroyOpenGL: destroys an OpenGL provider
 *****************************************************************************/
static void DestroyOpenGL( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;

    glXDestroyContext( p_sys->p_display, p_sys->gwctx );
    if( p_sys->b_glx13 )
    {
        glXDestroyWindow( p_sys->p_display, p_sys->gwnd );
    }

    E_(Deactivate)( p_this );
}

/*****************************************************************************
 * OpenDisplay: open and initialize OpenGL device
 *****************************************************************************/
static int CheckGLX( vlc_object_t *p_this, vlc_bool_t *b_glx13 )
{
    Display *p_display = NULL;
    int i_opcode, i_evt, i_err = 0;
    int i_maj, i_min = 0;

    /* Open the display */
    p_display = XOpenDisplay( NULL );
    if( p_display == NULL )
    {
        msg_Err( p_this, "cannot open display" );
        return VLC_EGENERIC;
    }

    /* Check for GLX extension */
    if( !XQueryExtension( p_display, "GLX", &i_opcode, &i_evt, &i_err ) )
    {
        msg_Err( p_this, "GLX extension not supported" );
        XCloseDisplay( p_display );
        return VLC_EGENERIC;
    }
    if( !glXQueryExtension( p_display, &i_err, &i_evt ) )
    {
        msg_Err( p_this, "glXQueryExtension failed" );
        XCloseDisplay( p_display );
        return VLC_EGENERIC;
    }

    /* Check GLX version */
    if (!glXQueryVersion( p_display, &i_maj, &i_min ) )
    {
        msg_Err( p_this, "glXQueryVersion failed" );
        XCloseDisplay( p_display );
        return VLC_EGENERIC;
    }
    if( i_maj <= 0 || ((i_maj == 1) && (i_min < 3)) )
    {
        *b_glx13 = VLC_FALSE;
        msg_Dbg( p_this, "using GLX 1.2 API" );
    }
    else
    {
        *b_glx13 = VLC_TRUE;
        msg_Dbg( p_this, "using GLX 1.3 API" );
    }

    XCloseDisplay( p_display );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * InitOpenGL: initializes OpenGL provider
 *****************************************************************************/
static int InitOpenGL( vout_thread_t *p_vout )
{
    /* Initialize GLX */
    if( !p_vout->p_sys->b_glx13 )
    {
        if( InitGLX12( p_vout ) != VLC_SUCCESS )
        {
            return VLC_EGENERIC;
        }
    }
    else
    {
        if( InitGLX13( p_vout ) != VLC_SUCCESS )
        {
            return VLC_EGENERIC;
        }
    }

    /* Set the OpenGL context _for the current thread_ */
    SwitchContext( p_vout );

    return VLC_SUCCESS;
}

int InitGLX12( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    XVisualInfo *p_vi;
    int p_attr[] = { GLX_RGBA, GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
                     GLX_BLUE_SIZE, 5, GLX_DOUBLEBUFFER, 0 };

    p_vi = glXChooseVisual( p_sys->p_display,
                            DefaultScreen( p_sys->p_display), p_attr );
    if(! p_vi )
    {
        msg_Err( p_vout, "Cannot get GLX 1.2 visual" );
        return VLC_EGENERIC;
    }

    /* Create an OpenGL context */
    p_sys->gwctx = glXCreateContext( p_sys->p_display, p_vi, 0, True );
    XFree( p_vi );
    if( !p_sys->gwctx )
    {
        msg_Err( p_vout, "Cannot create OpenGL context");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

int InitGLX13( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_nbelem;
    GLXFBConfig *p_fbconfs, fbconf;
    XVisualInfo *p_vi;
    int p_attr[] = { GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
                     GLX_BLUE_SIZE, 5, GLX_DOUBLEBUFFER, True,
                     GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, 0 };

    /* Get the FB configuration */
    p_fbconfs = glXChooseFBConfig( p_sys->p_display, 0, p_attr, &i_nbelem );
    if( (i_nbelem <= 0) || !p_fbconfs )
    {
        msg_Err( p_vout, "Cannot get FB configurations");
        if( p_fbconfs ) XFree( p_fbconfs );
        return VLC_EGENERIC;
    }
    fbconf = p_fbconfs[0];

    /* Get the X11 visual */
    p_vi = glXGetVisualFromFBConfig( p_sys->p_display, fbconf );
    if( !p_vi )
    {
        msg_Err( p_vout, "Cannot get X11 visual" );
        XFree( p_fbconfs );
        return VLC_EGENERIC;
    }
    XFree( p_vi );

    /* Create the GLX window */
    p_sys->gwnd = glXCreateWindow( p_sys->p_display, fbconf,
                                   p_sys->p_win->video_window, NULL );
    if( p_sys->gwnd == None )
    {
        msg_Err( p_vout, "Cannot create GLX window" );
        return VLC_EGENERIC;
    }

    /* Create an OpenGL context */
    p_sys->gwctx = glXCreateNewContext( p_sys->p_display, fbconf,
                                        GLX_RGBA_TYPE, NULL, True );
    XFree( p_fbconfs );
    if( !p_sys->gwctx )
    {
        msg_Err( p_vout, "Cannot create OpenGL context");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SwapBuffers: swap front/back buffers
 *****************************************************************************/
static void SwapBuffers( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_width, i_height, i_x, i_y;

    vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                       p_vout->p_sys->p_win->i_height,
                       &i_x, &i_y, &i_width, &i_height );

    glViewport( 0, 0, (GLint)i_width, (GLint)i_height );

    if( p_sys->b_glx13 )
    {
        glXSwapBuffers( p_sys->p_display, p_sys->gwnd );
    }
    else
    {
        glXSwapBuffers( p_sys->p_display, p_sys->p_win->video_window );
    }
}

void SwitchContext( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    /* Change the current OpenGL context */
    if( p_sys->b_glx13 )
    {
        glXMakeContextCurrent( p_sys->p_display, p_sys->gwnd,
                               p_sys->gwnd, p_sys->gwctx );
    }
    else
    {
        glXMakeCurrent( p_sys->p_display, p_sys->p_win->video_window,
                        p_sys->gwctx );
    }
}
