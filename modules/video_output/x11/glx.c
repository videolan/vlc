/*****************************************************************************
 * glx.c: GLX video output
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
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
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

/* For the opengl provider interface */
#include "vlc_opengl.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

/* Data common to vout and opengl provider structures */
typedef struct glx_t
{
    Display     *p_display;
    int         b_glx13;
    int         i_width;
    int         i_height;
    int         b_fullscreen;
    GLXContext  gwctx;
    Window      wnd;
    GLXWindow   gwnd;
    Atom        wm_delete;

} glx_t;

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
 * Vout interface
 *****************************************************************************/
static int  CreateVout   ( vlc_object_t * );
static void DestroyVout  ( vlc_object_t * );
static int  Init         ( vout_thread_t * );
static void End          ( vout_thread_t * );
static int  Manage       ( vout_thread_t * );
static void Render       ( vout_thread_t *p_vout, picture_t *p_pic );
static void DisplayVideo ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * OpenGL providerinterface
 *****************************************************************************/
static int  CreateOpenGL ( vlc_object_t * );
static void DestroyOpenGL( vlc_object_t * );
static int  InitOpenGL   ( opengl_t *, int, int );
static void SwapBuffers  ( opengl_t * );
static int  HandleEvents ( opengl_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDisplay ( vlc_object_t *, glx_t * );
static void CloseDisplay( glx_t * );
static int  InitGLX12   ( vlc_object_t *, glx_t * );
static int  InitGLX13   ( vlc_object_t *, glx_t * );
static void CreateWindow( vlc_object_t *, glx_t *, XVisualInfo *);
static int  HandleX11Events( vlc_object_t *, glx_t * );
static void SwitchContext( glx_t * );

static inline int GetAlignedSize( int i_size );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("X11 OpenGL (GLX) video output") );
    set_capability( "video output", 20 );
    add_shortcut( "glx" );
    set_callbacks( CreateVout, DestroyVout );

    add_submodule();
    set_description( _("X11 OpenGL provider") );
    set_capability( "opengl provider", 50 );
    set_callbacks( CreateOpenGL, DestroyOpenGL );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: GLX video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the GLX specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    uint8_t     *p_buffer;
    int         i_index;
    int         i_tex_width;
    int         i_tex_height;
    GLuint      texture;
    int         i_effect; //XXX

    glx_t       glx;
};

struct opengl_sys_t
{
    glx_t       glx;
};


#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct mwmhints_t
{
    uint32_t flags;
    uint32_t functions;
    uint32_t decorations;
    int32_t  input_mode;
    uint32_t status;
} mwmhints_t;


/*****************************************************************************
 * CreateVout: allocates GLX video thread output method
 *****************************************************************************
 * This function allocates and initializes a GLX vout method.
 *****************************************************************************/
static int CreateVout( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    //XXX set to 0 to disable the cube effect
    p_vout->p_sys->i_effect = 1;

    p_vout->p_sys->glx.i_width = p_vout->i_window_width;
    p_vout->p_sys->glx.i_height = p_vout->i_window_height;
    p_vout->p_sys->glx.b_fullscreen = 0;

    /* A texture must have a size aligned on a power of 2 */
    p_vout->p_sys->i_tex_width  = GetAlignedSize( p_vout->render.i_width );
    p_vout->p_sys->i_tex_height = GetAlignedSize( p_vout->render.i_height );

    msg_Dbg( p_vout, "Texture size: %dx%d", p_vout->p_sys->i_tex_width,
             p_vout->p_sys->i_tex_height );

    /* Open and initialize device */
    if( OpenDisplay( p_this, &p_vout->p_sys->glx ) )
    {
        msg_Err( p_vout, "cannot open display" );
        free( p_vout->p_sys );
        return( 1 );
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Render;
    p_vout->pf_display = DisplayVideo;

    return( 0 );
}

/*****************************************************************************
 * Init: initialize GLX video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_pixel_pitch;

    /* No YUV textures :( */

#if VLCGL_RGB_FORMAT == GL_RGB
#   if VLCGL_RGB_TYPE == GL_UNSIGNED_BYTE
    p_vout->output.i_chroma = VLC_FOURCC('R','V','2','4');
    p_vout->output.i_rmask = 0x000000ff;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x00ff0000;
    i_pixel_pitch = 3;
#   else
    p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6');
    p_vout->output.i_rmask = 0xf800;
    p_vout->output.i_gmask = 0x07e0;
    p_vout->output.i_bmask = 0x001f;
    i_pixel_pitch = 2;
#   endif
#else
    p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2');
    p_vout->output.i_rmask = 0x000000ff;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x00ff0000;
    i_pixel_pitch = 4;
#endif

    /* Since OpenGL can do rescaling for us, stick to the default
     * coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_vout->p_picture[0].i_planes = 1;
    p_vout->p_sys->p_buffer =
        malloc( p_vout->p_sys->i_tex_width * p_vout->p_sys->i_tex_height *
                i_pixel_pitch );
    if( !p_vout->p_sys->p_buffer )
    {
        msg_Err( p_vout, "Out of memory" );
        return -1;
    }

    p_vout->p_picture[0].p->p_pixels = p_vout->p_sys->p_buffer;
    p_vout->p_picture[0].p->i_lines = p_vout->output.i_height;
    p_vout->p_picture[0].p->i_pixel_pitch = i_pixel_pitch;
    p_vout->p_picture[0].p->i_pitch = p_vout->p_sys->i_tex_width *
        p_vout->p_picture[0].p->i_pixel_pitch;
    p_vout->p_picture[0].p->i_visible_pitch = p_vout->output.i_width *
        p_vout->p_picture[0].p->i_pixel_pitch;

    p_vout->p_picture[0].i_status = DESTROYED_PICTURE;
    p_vout->p_picture[0].i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ 0 ] = &p_vout->p_picture[0];
    I_OUTPUTPICTURES = 1;

    SwitchContext( &p_vout->p_sys->glx );

    /* Set the texture parameters */
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

    if( p_vout->p_sys->i_effect )
    {
        glEnable( GL_CULL_FACE);
        /* glDisable( GL_DEPTH_TEST );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE );*/

        /* Set the perpective */
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        glFrustum( -1.0, 1.0, -1.0, 1.0, 3.0, 20.0 );
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        glTranslatef( 0.0, 0.0, - 5.0 );
    }

    return 0;
}

/*****************************************************************************
 * End: terminate GLX video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    ;
}

/*****************************************************************************
 * Destroy: destroy GLX video thread output method
 *****************************************************************************
 * Terminate an output method created by CreateVout
 *****************************************************************************/
static void DestroyVout( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    CloseDisplay( &p_vout->p_sys->glx );

    /* Free the texture buffer*/
    if( p_vout->p_sys->p_buffer )
    {
        free( p_vout->p_sys->p_buffer );
    }

    free( p_vout->p_sys );
}

/*****************************************************************************
 * CreateOpenGL: initialize an OpenGL provider
 *****************************************************************************/
static int CreateOpenGL( vlc_object_t *p_this )
{
    opengl_t *p_opengl = (opengl_t*)p_this;

    /* Allocate the structure */
    p_opengl->p_sys = malloc(sizeof(opengl_sys_t));

    /* Set the function pointers */
    p_opengl->pf_init = InitOpenGL;
    p_opengl->pf_swap = SwapBuffers;
    p_opengl->pf_handle_events = HandleEvents;

    p_opengl->p_sys->glx.wnd = None;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroyOpenGL: destroys an OpenGL provider
 *****************************************************************************/
static void DestroyOpenGL( vlc_object_t *p_this )
{
    opengl_t *p_opengl = (opengl_t*)p_this;

    /* Free the structure */
    free(p_opengl->p_sys);
}

/*****************************************************************************
 * InitOpenGL: creates the OpenGL window
 *****************************************************************************/
static int InitOpenGL( opengl_t *p_opengl, int i_width, int i_height )
{
    p_opengl->p_sys->glx.i_width = i_width;
    p_opengl->p_sys->glx.i_height = i_height;
    p_opengl->p_sys->glx.b_fullscreen = 0;

    if( OpenDisplay( (vlc_object_t*)p_opengl, &p_opengl->p_sys->glx ) )
    {
        msg_Err( p_opengl, "Cannot create OpenGL window" );
        return VLC_EGENERIC;
    }

    /* Set the OpenGL context _for the current thread_ */
    SwitchContext( &p_opengl->p_sys->glx );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SwapBuffers: swap front/back buffers
 *****************************************************************************/
static void SwapBuffers( opengl_t *p_opengl )
{
    if( p_opengl->p_sys->glx.b_glx13 )
    {
        glXSwapBuffers( p_opengl->p_sys->glx.p_display,
                        p_opengl->p_sys->glx.gwnd );
    }
    else
    {
        glXSwapBuffers( p_opengl->p_sys->glx.p_display,
                        p_opengl->p_sys->glx.wnd );
    }
}

/*****************************************************************************
 * HandleEvents: handle window events
 *****************************************************************************/
static int HandleEvents( opengl_t *p_opengl )
{
    int i_ret =
        HandleX11Events( (vlc_object_t*)p_opengl, &p_opengl->p_sys->glx );
    if( i_ret )
    {
        /* close the window */
        CloseDisplay( &p_opengl->p_sys->glx );
    }
    return i_ret;
}

/*****************************************************************************
 * Manage: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    if( HandleX11Events( (vlc_object_t*)p_vout, &p_vout->p_sys->glx ) )
    {
        /* the user wants to close the window */
        playlist_t * p_playlist =
            (playlist_t *)vlc_object_find( p_vout, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
        if( p_playlist != NULL )
        {
            playlist_Stop( p_playlist );
            vlc_object_release( p_playlist );
        }
    }
    return 0;
}

static int HandleX11Events( vlc_object_t *p_thread, glx_t *p_glx )
{
    Display *p_display;

    p_display = p_glx->p_display;

    /* loop on X11 events */
    while( XPending( p_display ) > 0 )
    {
        XEvent evt;
        XNextEvent( p_display, &evt );
        switch( evt.type )
        {
            case ClientMessage:
            {
                /* Delete notification */
                if( (evt.xclient.format == 32) &&
                    ((Atom)evt.xclient.data.l[0] ==
                     p_glx->wm_delete) )
                {
                    return -1;
                }
                break;
            }
        }
    }
    return 0;
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    float f_width = (float)p_vout->output.i_width / p_sys->i_tex_width;
    float f_height = (float)p_vout->output.i_height / p_sys->i_tex_height;

    glClear(GL_COLOR_BUFFER_BIT);

    glTexImage2D (GL_TEXTURE_2D, 0, 3, p_vout->p_sys->i_tex_width,
                  p_vout->p_sys->i_tex_height , 0,
                  VLCGL_RGB_FORMAT, VLCGL_RGB_TYPE,
                  p_vout->p_sys->p_buffer);

    if( !p_vout->p_sys->i_effect )
    {
        glEnable( GL_TEXTURE_2D );
        glBegin( GL_POLYGON );
        glTexCoord2f( 0.0, 0.0 ); glVertex2f( -1.0, 1.0 );
        glTexCoord2f( f_width, 0.0 ); glVertex2f( 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex2f( 1.0, -1.0 );
        glTexCoord2f( 0.0, f_height ); glVertex2f( -1.0, -1.0 );
        glEnd();
    }
    else
    {
        glRotatef( 1.0, 0.3, 0.5, 0.7 );

        glEnable( GL_TEXTURE_2D );
        glBegin( GL_QUADS );

        /* Front */
        glTexCoord2f( 0, 0 ); glVertex3f( - 1.0, 1.0, 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( - 1.0, - 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( 1.0, - 1.0, 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( 1.0, 1.0, 1.0 );

        /* Left */
        glTexCoord2f( 0, 0 ); glVertex3f( - 1.0, 1.0, - 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( - 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( - 1.0, - 1.0, 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( - 1.0, 1.0, 1.0 );

        /* Back */
        glTexCoord2f( 0, 0 ); glVertex3f( 1.0, 1.0, - 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( - 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( - 1.0, 1.0, - 1.0 );

        /* Right */
        glTexCoord2f( 0, 0 ); glVertex3f( 1.0, 1.0, 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( 1.0, - 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( 1.0, 1.0, - 1.0 );

        /* Top */
        glTexCoord2f( 0, 0 ); glVertex3f( - 1.0, 1.0, - 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( - 1.0, 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( 1.0, 1.0, 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( 1.0, 1.0, - 1.0 );

        /* Bottom */
        glTexCoord2f( 0, 0 ); glVertex3f( - 1.0, - 1.0, 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( - 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( 1.0, - 1.0, 1.0 );
        glEnd();
    }

    glDisable( GL_TEXTURE_2D);
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    if( p_vout->p_sys->glx.b_glx13 )
    {
        glXSwapBuffers( p_vout->p_sys->glx.p_display,
                        p_vout->p_sys->glx.gwnd );
    }
    else
    {
        glXSwapBuffers( p_vout->p_sys->glx.p_display,
                        p_vout->p_sys->glx.wnd );
    }
}

/*****************************************************************************
 * OpenDisplay: open and initialize OpenGL device
 *****************************************************************************/
static int OpenDisplay( vlc_object_t *p_thread, glx_t *p_glx )
{
    Display *p_display;
    int i_opcode, i_evt, i_err;
    int i_maj, i_min;

    /* Open the display */
    p_glx->p_display = p_display = XOpenDisplay( NULL );
    if( !p_display )
    {
        msg_Err( p_thread, "Cannot open display" );
        return -1;
    }

    /* Check for GLX extension */
    if( !XQueryExtension( p_display, "GLX", &i_opcode, &i_evt, &i_err ) )
    {
        msg_Err( p_thread, "GLX extension not supported" );
        return -1;
    }
    if( !glXQueryExtension( p_display, &i_err, &i_evt ) )
    {
        msg_Err( p_thread, "glXQueryExtension failed" );
        return -1;
    }

    /* Check GLX version */
    if (!glXQueryVersion( p_display, &i_maj, &i_min ) )
    {
        msg_Err( p_thread, "glXQueryVersion failed" );
        return -1;
    }
    if( i_maj <= 0 || ((i_maj == 1) && (i_min < 3)) )
    {
        p_glx->b_glx13 = 0;
        msg_Dbg( p_thread, "Using GLX 1.2 API" );
        if( InitGLX12( p_thread, p_glx ) == -1 )
        {
            return -1;
        }
    }
    else
    {
        p_glx->b_glx13 = 1;
        msg_Dbg( p_thread, "Using GLX 1.3 API" );
        if( InitGLX13( p_thread, p_glx ) == -1 )
        {
            return -1;
        }
    }

    XMapWindow( p_display, p_glx->wnd );
    if( p_glx->b_fullscreen )
    {
        //XXX
        XMoveWindow( p_display, p_glx->wnd, 0, 0 );
    }
    XFlush( p_display );
    return 0;
}

/*****************************************************************************
 * CloseDisplay: close and reset OpenGL device
 *****************************************************************************
 * Returns all resources allocated by OpenDisplay and restore the original
 * state of the device.
 *****************************************************************************/
static void CloseDisplay( glx_t *p_glx )
{
    Display *p_display;

    if (p_glx->wnd == None )
    {
        // Already closed or not opened...
        return;
    }

    glFlush();
    p_display = p_glx->p_display;
    glXDestroyContext( p_display, p_glx->gwctx );
    if( p_glx->b_glx13 )
    {
        glXDestroyWindow( p_display, p_glx->gwnd );
    }
    XDestroyWindow( p_display, p_glx->wnd );
    p_glx->wnd = None;
}

int InitGLX12( vlc_object_t *p_thread, glx_t *p_glx )
{
    Display *p_display;
    XVisualInfo *p_vi;
    GLXContext gwctx;
    int p_attr[] = { GLX_RGBA, GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
                     GLX_BLUE_SIZE, 5, GLX_DOUBLEBUFFER,
                     0 };

    p_display = p_glx->p_display;

    p_vi = glXChooseVisual( p_display, DefaultScreen( p_display), p_attr );
    if(! p_vi )
    {
        msg_Err( p_thread, "Cannot get GLX 1.2 visual" );
        return -1;
    }

    /* Create the window */
    CreateWindow( p_thread, p_glx, p_vi );

     /* Create an OpenGL context */
    p_glx->gwctx = gwctx = glXCreateContext( p_display, p_vi, 0, True );
    if( !gwctx )
    {
        msg_Err( p_thread, "Cannot create OpenGL context");
        XFree( p_vi );
        return -1;
    }
    XFree( p_vi );

    return 0;
}

int InitGLX13( vlc_object_t *p_thread, glx_t *p_glx )
{
    Display *p_display;
    int i_nbelem;
    GLXFBConfig *p_fbconfs, fbconf;
    XVisualInfo *p_vi;
    GLXContext gwctx;
    int p_attr[] = { GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
                     GLX_BLUE_SIZE, 5, GLX_DOUBLEBUFFER, True,
                     GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, 0 };

    p_display = p_glx->p_display;

    /* Get the FB configuration */
    p_fbconfs = glXChooseFBConfig( p_display, 0, p_attr, &i_nbelem );
    if( (i_nbelem <= 0) || !p_fbconfs )
    {
        msg_Err( p_thread, "Cannot get FB configurations");
        if( p_fbconfs )
        {
            XFree( p_fbconfs );
        }
        return -1;
    }
    fbconf = p_fbconfs[0];

    /* Get the X11 visual */
    p_vi = glXGetVisualFromFBConfig( p_display, fbconf );
    if( !p_vi )
    {
        msg_Err( p_thread, "Cannot get X11 visual" );
        XFree( p_fbconfs );
        return -1;
    }

    /* Create the window */
    CreateWindow( p_thread, p_glx, p_vi );

    XFree( p_vi );

    /* Create the GLX window */
    p_glx->gwnd = glXCreateWindow( p_display, fbconf, p_glx->wnd, NULL );
    if( p_glx->gwnd == None )
    {
        msg_Err( p_thread, "Cannot create GLX window" );
        return -1;
    }

    /* Create an OpenGL context */
    p_glx->gwctx = gwctx = glXCreateNewContext( p_display, fbconf,
                                                  GLX_RGBA_TYPE, NULL, True );
    if( !gwctx )
    {
        msg_Err( p_thread, "Cannot create OpenGL context");
        XFree( p_fbconfs );
        return -1;
    }
    XFree( p_fbconfs );

    return 0;
}

void CreateWindow( vlc_object_t *p_thread, glx_t *p_glx, XVisualInfo *p_vi )
{
    Display *p_display;
    XSetWindowAttributes xattr;
    Window wnd;
    Colormap cm;
    XSizeHints* p_size_hints;
    Atom prop;
    mwmhints_t mwmhints;

    p_display = p_glx->p_display;

    /* Create a colormap */
    cm = XCreateColormap( p_display, RootWindow( p_display, p_vi->screen ),
                          p_vi->visual, AllocNone );

    /* Create the window */
    xattr.background_pixel = BlackPixel( p_display, DefaultScreen(p_display) );
    xattr.border_pixel = 0;
    xattr.colormap = cm;
    p_glx->wnd = wnd = XCreateWindow( p_display, DefaultRootWindow(p_display),
            0, 0, p_glx->i_width, p_glx->i_height, 0, p_vi->depth,
            InputOutput, p_vi->visual,
            CWBackPixel | CWBorderPixel | CWColormap, &xattr);

    /* Allow the window to be deleted by the window manager */
    p_glx->wm_delete = XInternAtom( p_display, "WM_DELETE_WINDOW", False );
    XSetWMProtocols( p_display, wnd, &p_glx->wm_delete, 1 );

    if( p_glx->b_fullscreen )
    {
        mwmhints.flags = MWM_HINTS_DECORATIONS;
        mwmhints.decorations = False;

        prop = XInternAtom( p_display, "_MOTIF_WM_HINTS", False );
        XChangeProperty( p_display, wnd, prop, prop, 32, PropModeReplace,
                         (unsigned char *)&mwmhints, PROP_MWM_HINTS_ELEMENTS );
    }
    else
    {
        /* Prevent the window from being resized */
        p_size_hints = XAllocSizeHints();
        p_size_hints->flags = PMinSize | PMaxSize;
        p_size_hints->min_width = p_glx->i_width;
        p_size_hints->min_height = p_glx->i_height;
        p_size_hints->max_width = p_glx->i_width;
        p_size_hints->max_height = p_glx->i_height;
        XSetWMNormalHints( p_display, wnd, p_size_hints );
        XFree( p_size_hints );
    }
    XSelectInput( p_display, wnd, KeyPressMask );
}

void SwitchContext( glx_t *p_glx )
{
    /* Change the current OpenGL context */
    if( p_glx->b_glx13 )
    {
        glXMakeContextCurrent( p_glx->p_display, p_glx->gwnd,
                               p_glx->gwnd, p_glx->gwctx );
    }
    else
    {
        glXMakeCurrent( p_glx->p_display, p_glx->wnd,
                        p_glx->gwctx );
    }
}

int GetAlignedSize( int i_size )
{
    /* Return the nearest power of 2 */
    int i_result = 1;
    while( i_result < i_size )
    {
        i_result *= 2;
    }
    return i_result;
}
