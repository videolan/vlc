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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( vlc_object_t * );
static void Destroy     ( vlc_object_t * );

static int  Init        ( vout_thread_t * );
static void End         ( vout_thread_t * );
static int  Manage      ( vout_thread_t * );
static void Render      ( vout_thread_t *p_vout, picture_t *p_pic );
static void DisplayVideo( vout_thread_t *, picture_t * );

static int  OpenDisplay ( vout_thread_t * );
static void CloseDisplay( vout_thread_t * );
static int  InitGLX12   ( vout_thread_t *p_vout );
static int  InitGLX13   ( vout_thread_t *p_vout );
static void CreateWindow( vout_thread_t *p_vout, XVisualInfo *p_vi );
static void SwitchContext( vout_thread_t *p_vout );

static inline int GetAlignedSize( int i_size );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("X11 OpenGL (GLX) video output") );
    set_capability( "video output", 20 );
    add_shortcut( "glx" );
    set_callbacks( Create, Destroy );
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

    int         i_width;
    int         i_height;
    int         b_fullscreen;

    Display     *p_display;
    int         b_glx13;
    GLXContext  gwctx;
    Window      wnd;
    GLXWindow   gwnd;
    Atom        wm_delete;

    GLuint      texture;

    int         i_effect; //XXX
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
 * Create: allocates GLX video thread output method
 *****************************************************************************
 * This function allocates and initializes a GLX vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
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

   /* p_vout->p_sys->i_width = p_vout->i_window_width;
    p_vout->p_sys->i_height = p_vout->i_window_height; */
    p_vout->p_sys->i_width = 700;
    p_vout->p_sys->i_height = 700;
    p_vout->p_sys->b_fullscreen = 0;

    /* A texture must have a size aligned on a power of 2 */
    p_vout->p_sys->i_tex_width  = GetAlignedSize( p_vout->render.i_width );
    p_vout->p_sys->i_tex_height = GetAlignedSize( p_vout->render.i_height );

    msg_Dbg( p_vout, "Texture size: %dx%d", p_vout->p_sys->i_tex_width,
             p_vout->p_sys->i_tex_height );

    /* Open and initialize device */
    if( OpenDisplay( p_vout ) )
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
    int i_index;
    picture_t *p_pic;

    /* No YUV textures :( */
    p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2');
    p_vout->output.i_rmask = 0x000000ff;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x00ff0000;

    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    I_OUTPUTPICTURES = 0;

    p_pic = NULL;

    /* Find an empty picture slot */
    for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
    {
        if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
        {
            p_pic = p_vout->p_picture + i_index;
            break;
        }
    }

    if( p_pic == NULL )
    {
        return -1;
    }

   /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->i_planes = 1;


    p_pic->p->p_pixels = p_vout->p_sys->p_buffer
        + 2 * p_vout->p_sys->i_tex_width *
          (p_vout->p_sys->i_tex_height - p_vout->output.i_height)
        + 2 * (p_vout->p_sys->i_tex_width - p_vout->output.i_width);
    p_pic->p->i_lines = p_vout->output.i_height;
    p_pic->p->i_pitch = p_vout->p_sys->i_tex_width * 4;
    p_pic->p->i_pixel_pitch = 4;
    p_pic->p->i_visible_pitch = p_vout->output.i_width * 4;

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ 0 ] = p_pic;

    I_OUTPUTPICTURES = 1;

    SwitchContext( p_vout );

    /* Set the texture parameters */
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

    if( p_vout->p_sys->i_effect )
    {
        glEnable( GL_CULL_FACE);
        /*glDisable( GL_DEPTH_TEST );
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
 * Terminate an output method created by Create
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    CloseDisplay( p_vout );

    /* Free the texture buffer*/
    if( p_vout->p_sys->p_buffer )
    {
        free( p_vout->p_sys->p_buffer );
    }

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Manage: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    Display *p_display;

    p_display = p_vout->p_sys->p_display;

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
                    ((Atom)evt.xclient.data.l[0] == p_vout->p_sys->wm_delete) )
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
    glClear(GL_COLOR_BUFFER_BIT);
    /*glTexImage2D (GL_TEXTURE_2D, 0, 3, p_vout->output.i_width,
                  p_vout->output.i_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                  p_vout->p_sys->p_buffer);*/
    glTexImage2D (GL_TEXTURE_2D, 0, 3, p_vout->p_sys->i_tex_width,
                  p_vout->p_sys->i_tex_height , 0, GL_RGBA, GL_UNSIGNED_BYTE,
                  p_vout->p_sys->p_buffer);

    if( !p_vout->p_sys->i_effect )
    {
        glEnable( GL_TEXTURE_2D);
        glBegin( GL_POLYGON);
        glTexCoord2f(0.0,0.0); glVertex2f(-1.0,1.0);
        glTexCoord2f(1.0,0.0); glVertex2f(1.0,1.0);
        glTexCoord2f(1.0,1.0); glVertex2f(1.0,-1.0);
        glTexCoord2f(0.0,1.0); glVertex2f(-1.0,-1.0);
        glEnd();
    }
    else
    {
        glRotatef( 1.0, 0.3, 0.5, 0.7 );

        glEnable( GL_TEXTURE_2D);
        glBegin( GL_QUADS );

        float f_width = p_vout->output.i_width;
        float f_height = p_vout->output.i_height;

        /* Correct the aspect ratio */
 /*       float f_aspect = (float)p_vout->output.i_aspect / VOUT_ASPECT_FACTOR;
        if( f_aspect > 1.0 )
        {
            f_height *= f_aspect;
        }
        else
        {
            f_width *= f_aspect;
        }*/

        float f_offset_x = (1.0 - f_width / p_vout->p_sys->i_tex_width) / 2.0;
        float f_offset_y = (1.0 - f_height / p_vout->p_sys->i_tex_height) / 2.0;

        /* Front */
        glTexCoord2f( f_offset_x, f_offset_y );
        glVertex3f( - 1.0, 1.0, 1.0 );
        glTexCoord2f( f_offset_x, 1.0 - f_offset_y );
        glVertex3f( - 1.0, - 1.0, 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, 1.0 - f_offset_y );
        glVertex3f( 1.0, - 1.0, 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, f_offset_y );
        glVertex3f( 1.0, 1.0, 1.0 );

        /* Left */
        glTexCoord2f( f_offset_x, f_offset_y );
        glVertex3f( - 1.0, 1.0, - 1.0 );
        glTexCoord2f( f_offset_x, 1.0 - f_offset_y );
        glVertex3f( - 1.0, - 1.0, - 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, 1.0 - f_offset_y );
        glVertex3f( - 1.0, - 1.0, 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, f_offset_y );
        glVertex3f( - 1.0, 1.0, 1.0 );

        /* Back */
        glTexCoord2f( f_offset_x, f_offset_y );
        glVertex3f( 1.0, 1.0, - 1.0 );
        glTexCoord2f( f_offset_x, 1.0 - f_offset_y );
        glVertex3f( 1.0, - 1.0, - 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, 1.0 - f_offset_y );
        glVertex3f( - 1.0, - 1.0, - 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, f_offset_y );
        glVertex3f( - 1.0, 1.0, - 1.0 );

        /* Right */
        glTexCoord2f( f_offset_x, f_offset_y );
        glVertex3f( 1.0, 1.0, 1.0 );
        glTexCoord2f( f_offset_x, 1.0 - f_offset_y );
        glVertex3f( 1.0, - 1.0, 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, 1.0 - f_offset_y );
        glVertex3f( 1.0, - 1.0, - 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, f_offset_y );
        glVertex3f( 1.0, 1.0, - 1.0 );

        /* Top */
        glTexCoord2f( f_offset_x, f_offset_y );
        glVertex3f( - 1.0, 1.0, - 1.0 );
        glTexCoord2f( f_offset_x, 1.0 - f_offset_y );
        glVertex3f( - 1.0, 1.0, 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, 1.0 - f_offset_y );
        glVertex3f( 1.0, 1.0, 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, f_offset_y );
        glVertex3f( 1.0, 1.0, - 1.0 );

        /* Bottom */
        glTexCoord2f( f_offset_x, f_offset_y );
        glVertex3f( - 1.0, - 1.0, 1.0 );
        glTexCoord2f( f_offset_x, 1.0 - f_offset_y );
        glVertex3f( - 1.0, - 1.0, - 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, 1.0 - f_offset_y );
        glVertex3f( 1.0, - 1.0, - 1.0 );
        glTexCoord2f( 1.0 - f_offset_x, f_offset_y );
        glVertex3f( 1.0, - 1.0, 1.0 );
        glEnd();
    }
    glDisable( GL_TEXTURE_2D);
    
  
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    if( p_vout->p_sys->b_glx13 )
    {
        glXSwapBuffers( p_vout->p_sys->p_display, p_vout->p_sys->gwnd );
    }
    else
    {
        glXSwapBuffers( p_vout->p_sys->p_display, p_vout->p_sys->wnd );
    }

}

/* following functions are local */

/*****************************************************************************
 * OpenDisplay: open and initialize OpenGL device
 *****************************************************************************/

static int OpenDisplay( vout_thread_t *p_vout )
{
    Display *p_display;
    int i_opcode, i_evt, i_err;
    int i_maj, i_min;

    /* Open the display */
    p_vout->p_sys->p_display = p_display = XOpenDisplay( NULL );
    if( !p_display )
    {
        msg_Err( p_vout, "Cannot open display" );
        return -1;
    }

    /* Check for GLX extension */
    if( !XQueryExtension( p_display, "GLX", &i_opcode, &i_evt, &i_err ) )
    {
        msg_Err( p_vout, "GLX extension not supported" );
        return -1;
    }
    if( !glXQueryExtension( p_display, &i_err, &i_evt ) )
    {
        msg_Err( p_vout, "glXQueryExtension failed" );
        return -1;
    }

    /* Check GLX version */
    if (!glXQueryVersion( p_display, &i_maj, &i_min ) )
    {
        msg_Err( p_vout, "glXQueryVersion failed" );
        return -1;
    }
    if( i_maj <= 0 || ((i_maj == 1) && (i_min < 3)) )
    {
        p_vout->p_sys->b_glx13 = 0;
        msg_Dbg( p_vout, "Using GLX 1.2 API" );
        if( InitGLX12( p_vout ) == -1 )
        {
            return -1;
        }
    }
    else
    {
        p_vout->p_sys->b_glx13 = 1;
        msg_Dbg( p_vout, "Using GLX 1.3 API" );
        if( InitGLX13( p_vout ) == -1 )
        {
            return -1;
        }
    }

    XMapWindow( p_display, p_vout->p_sys->wnd );
    if( p_vout->p_sys->b_fullscreen )
    {
        //XXX
        XMoveWindow( p_display, p_vout->p_sys->wnd, 0, 0 );
    }
    XFlush( p_display );

    /* Allocate the texture buffer */
    p_vout->p_sys->p_buffer =
        malloc( p_vout->p_sys->i_tex_width * p_vout->p_sys->i_tex_height * 4 );
    if( !p_vout->p_sys->p_buffer )
    {
        msg_Err( p_vout, "Out of memory" );
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * CloseDisplay: close and reset OpenGL device
 *****************************************************************************
 * Returns all resources allocated by OpenDisplay and restore the original
 * state of the device.
 *****************************************************************************/
static void CloseDisplay( vout_thread_t *p_vout )
{
    Display *p_display;

    glFlush();

    p_display = p_vout->p_sys->p_display;
    glXDestroyContext( p_display, p_vout->p_sys->gwctx );
    if( p_vout->p_sys->b_glx13 )
    {
        glXDestroyWindow( p_display, p_vout->p_sys->gwnd );
    }
    XDestroyWindow( p_display, p_vout->p_sys->wnd );
    XCloseDisplay( p_display );

}


int InitGLX12( vout_thread_t *p_vout )
{
    Display *p_display;
    XVisualInfo *p_vi;
    GLXContext gwctx;
    int p_attr[] = { GLX_RGBA, GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
                     GLX_BLUE_SIZE, 5, GLX_DOUBLEBUFFER,
                     0 };

    p_display = p_vout->p_sys->p_display;

    p_vi = glXChooseVisual( p_display, DefaultScreen( p_display), p_attr );
    if(! p_vi )
    {
        msg_Err( p_vout, "Cannot get GLX 1.2 visual" );
        return -1;
    }

    /* Create the window */
    CreateWindow( p_vout, p_vi );

     /* Create an OpenGL context */
    p_vout->p_sys->gwctx = gwctx = glXCreateContext( p_display, p_vi, 0, True );
    if( !gwctx )
    {
        msg_Err( p_vout, "Cannot create OpenGL context");
        XFree( p_vi );
        return -1;
    }
    XFree( p_vi );

    return 0;
}


int InitGLX13( vout_thread_t *p_vout )
{
    Display *p_display;
    int i_nbelem;
    GLXFBConfig *p_fbconfs, fbconf;
    XVisualInfo *p_vi;
    GLXContext gwctx;
    int p_attr[] = { GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
                     GLX_BLUE_SIZE, 5, GLX_DOUBLEBUFFER, True,
                     GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, 0 };

    p_display = p_vout->p_sys->p_display;

    /* Get the FB configuration */
    p_fbconfs = glXChooseFBConfig( p_display, 0, p_attr, &i_nbelem );
    if( (i_nbelem <= 0) || !p_fbconfs )
    {
        msg_Err( p_vout, "Cannot get FB configurations");
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
        msg_Err( p_vout, "Cannot get X11 visual" );
        XFree( p_fbconfs );
        return -1;
    }

    /* Create the window */
    CreateWindow( p_vout, p_vi );

    XFree( p_vi );

    /* Create the GLX window */
    p_vout->p_sys->gwnd = glXCreateWindow( p_display, fbconf, p_vout->p_sys->wnd, NULL );
    if( p_vout->p_sys->gwnd == None )
    {
        msg_Err( p_vout, "Cannot create GLX window" );
        return -1;
    }

    /* Create an OpenGL context */
    p_vout->p_sys->gwctx = gwctx = glXCreateNewContext( p_display, fbconf,
                                                  GLX_RGBA_TYPE, NULL, True );
    if( !gwctx )
    {
        msg_Err( p_vout, "Cannot create OpenGL context");
        XFree( p_fbconfs );
        return -1;
    }
    XFree( p_fbconfs );

    return 0;
}



void CreateWindow( vout_thread_t *p_vout, XVisualInfo *p_vi )
{
    Display *p_display;
    XSetWindowAttributes xattr;
    Window wnd;
    Colormap cm;
    XSizeHints* p_size_hints;
    Atom prop;
    mwmhints_t mwmhints;

    p_display = p_vout->p_sys->p_display;

    /* Create a colormap */
    cm = XCreateColormap( p_display, RootWindow( p_display, p_vi->screen ),
                          p_vi->visual, AllocNone );

    /* Create the window */
    xattr.background_pixel = BlackPixel( p_display, DefaultScreen(p_display) );
    xattr.border_pixel = 0;
    xattr.colormap = cm;
    p_vout->p_sys->wnd = wnd = XCreateWindow( p_display, DefaultRootWindow(p_display),
            0, 0, p_vout->p_sys->i_width, p_vout->p_sys->i_height, 0, p_vi->depth,
            InputOutput, p_vi->visual,
            CWBackPixel | CWBorderPixel | CWColormap, &xattr);

    /* Allow the window to be deleted by the window manager */
    p_vout->p_sys->wm_delete = XInternAtom( p_display, "WM_DELETE_WINDOW", False );
    XSetWMProtocols( p_display, wnd, &p_vout->p_sys->wm_delete, 1 );

    if( p_vout->p_sys->b_fullscreen )
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
        p_size_hints->min_width = p_vout->p_sys->i_width;
        p_size_hints->min_height = p_vout->p_sys->i_height;
        p_size_hints->max_width = p_vout->p_sys->i_width;
        p_size_hints->max_height = p_vout->p_sys->i_height;
        XSetWMNormalHints( p_display, wnd, p_size_hints );
        XFree( p_size_hints );
    }
    XSelectInput( p_display, wnd, KeyPressMask );
}


void SwitchContext( vout_thread_t *p_vout )
{
    /* Change the current OpenGL context */
    if( p_vout->p_sys->b_glx13 )
    {
        glXMakeContextCurrent( p_vout->p_sys->p_display, p_vout->p_sys->gwnd,
                               p_vout->p_sys->gwnd, p_vout->p_sys->gwctx );
    }
    else
    {
        glXMakeCurrent( p_vout->p_sys->p_display, p_vout->p_sys->wnd,
                        p_vout->p_sys->gwctx );
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
