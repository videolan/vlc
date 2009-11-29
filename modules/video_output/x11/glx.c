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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>                                                 /* ENOMEM */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_vout.h>

#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
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
static int  InitGLX12    ( vout_thread_t * );
static int  InitGLX13    ( vout_thread_t * );
static void SwitchContext( vout_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DISPLAY_TEXT N_("X11 display")
#define DISPLAY_LONGTEXT N_( \
    "X11 hardware display to use. By default VLC will " \
    "use the value of the DISPLAY environment variable.")

vlc_module_begin ()
    set_shortname( "OpenGL(GLX)" )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_description( N_("OpenGL(GLX) provider") )
    set_capability( "opengl provider", 50 )
    set_callbacks( CreateOpenGL, DestroyOpenGL )

    add_string( "glx-display", NULL, NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, true )
    add_obsolete_integer( "glx-adaptor" ) /* Deprecated since 1.0.4 */
#ifdef HAVE_SYS_SHM_H
    add_obsolete_bool( "glx-shm" ) /* Deprecated since 1.0.4 */
#endif
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
extern int  Activate   ( vlc_object_t * );
extern void Deactivate ( vlc_object_t * );

/*****************************************************************************
 * CreateOpenGL: initialize an OpenGL provider
 *****************************************************************************/
static int CreateOpenGL( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    if( Activate( p_this ) != VLC_SUCCESS )
    {
        return VLC_EGENERIC;
    }

    /* Set the function pointer */
    p_vout->pf_init = InitOpenGL;
    p_vout->pf_swap = SwapBuffers;

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

    Deactivate( p_this );
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
    int i_nb, ret = VLC_EGENERIC;
    GLXFBConfig *p_fbconfs = NULL, fbconf = NULL;
    XWindowAttributes att;
    static const int p_attr[] = {
        GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5, GLX_BLUE_SIZE, 5,
        GLX_DOUBLEBUFFER, True, GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        None,
    };

    /* Get the FB configuration */
    p_fbconfs = glXChooseFBConfig( p_sys->p_display, p_sys->i_screen, p_attr, &i_nb );
    if( p_fbconfs == NULL )
    {
        msg_Err( p_vout, "Cannot get FB configurations");
        return VLC_EGENERIC;
    }

    /* We should really create the window _after_ the frame buffer
     * configuration was chosen, instead of selecting the frame buffer from
     * the window. That requires reworking xcommon.c though.
     * -- Courmisch */
    XGetWindowAttributes( p_sys->p_display, p_sys->window.video_window, &att );
    for( int i = 0; i < i_nb && !fbconf; i++ )
    {
        XVisualInfo *p_vi;

        /* Get the X11 visual */
        p_vi = glXGetVisualFromFBConfig( p_sys->p_display, p_fbconfs[i] );
        if( !p_vi )
            continue; /* OoM? */

        if( p_vi->visualid == att.visual->visualid )
            fbconf = p_fbconfs[i];
        XFree( p_vi );
    }
    if( !fbconf )
    {
        msg_Err( p_vout, "Cannot find matching frame buffer" );
        goto out;
    }

    /* Create the GLX window */
    p_sys->gwnd = glXCreateWindow( p_sys->p_display, fbconf,
                                   p_sys->window.video_window, NULL );
    if( p_sys->gwnd == None )
    {
        msg_Err( p_vout, "Cannot create GLX window" );
        goto out;
    }

    /* Create an OpenGL context */
    p_sys->gwctx = glXCreateNewContext( p_sys->p_display, fbconf,
                                        GLX_RGBA_TYPE, NULL, True );
    if( !p_sys->gwctx )
        msg_Err( p_vout, "Cannot create OpenGL context");
    else
        ret = VLC_SUCCESS;

out:
    XFree( p_fbconfs );
    return ret;
}

/*****************************************************************************
 * SwapBuffers: swap front/back buffers
 *****************************************************************************/
static void SwapBuffers( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    unsigned int i_width, i_height, i_x, i_y;

    vout_PlacePicture( p_vout, p_vout->p_sys->window.i_width,
                       p_vout->p_sys->window.i_height,
                       &i_x, &i_y, &i_width, &i_height );

    glViewport( 0, 0, (GLint)i_width, (GLint)i_height );

    if( p_sys->b_glx13 )
    {
        glXSwapBuffers( p_sys->p_display, p_sys->gwnd );
    }
    else
    {
        glXSwapBuffers( p_sys->p_display, p_sys->window.video_window );
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
        glXMakeCurrent( p_sys->p_display, p_sys->window.video_window,
                        p_sys->gwctx );
    }
}
