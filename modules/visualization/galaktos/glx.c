/*****************************************************************************
 * glx.c:
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

#include "glx.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

/* Local prototypes */
static int CreateWindow( galaktos_thread_t *p_thread, XVisualInfo *p_vi,
                         int i_width, int i_height );


typedef struct
{
    Display     *p_display;
    GLXContext  gwctx;
    Window      wnd;
    GLXWindow   gwnd;
    GLXPbuffer  gpbuf;
    GLXContext  gpctx;
    Atom        wm_delete;
}
glx_data_t;
#define OS_DATA ((glx_data_t*)(p_thread->p_os_data))


int galaktos_glx_init( galaktos_thread_t *p_thread, int i_width, int i_height )
{
    Display *p_display;
    int i_opcode, i_evt, i_err;
    int i_maj, i_min;
    int i_nbelem;
    GLXFBConfig *p_fbconfs, fbconf;
    XVisualInfo *p_vi;
    GLXContext gwctx;
    int i;
    GLXPbuffer gpbuf;
    int p_attr[] = { GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
                     GLX_BLUE_SIZE, 5, GLX_DOUBLEBUFFER, True,
                     GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, 0 };

    /* Initialize OS data */
    p_thread->p_os_data = malloc( sizeof( glx_data_t ) );

    /* Open the display */
    OS_DATA->p_display = p_display = XOpenDisplay( NULL );
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
        msg_Err( p_thread, "GLX 1.3 is needed" );
        return -1;
    }

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
    if( CreateWindow( p_thread, p_vi, i_width, i_height ) == -1 )
    {
        XFree( p_fbconfs );
        XFree( p_vi );
        return -1;
    }
    XFree( p_vi );

    /* Create the GLX window */
    OS_DATA->gwnd = glXCreateWindow( p_display, fbconf, OS_DATA->wnd, NULL );
    if( OS_DATA->gwnd == None )
    {
        msg_Err( p_thread, "Cannot create GLX window" );
        return -1;
    }

    /* Create an OpenGL context */
    OS_DATA->gwctx = gwctx = glXCreateNewContext( p_display, fbconf,
            GLX_RGBA_TYPE, NULL, True );
    if( !gwctx )
    {
        msg_Err( p_thread, "Cannot create OpenGL context");
        XFree( p_fbconfs );
        return -1;
    }
    XFree( p_fbconfs );

    /* Get a FB config for the pbuffer */
    p_attr[1] = 8;                  // RED_SIZE
    p_attr[3] = 8;                  // GREEN_SIZE
    p_attr[5] = 8;                  // BLUE_SIZE
    p_attr[7] = False;              // DOUBLEBUFFER
    p_attr[9] = GLX_PBUFFER_BIT;    // DRAWABLE_TYPE
    p_fbconfs = glXChooseFBConfig( p_display, 0, p_attr, &i_nbelem );
    if( (i_nbelem <= 0) || !p_fbconfs )
    {
        msg_Err( p_thread, "Cannot get FB configurations for pbuffer");
        if( p_fbconfs )
        {
            XFree( p_fbconfs );
        }
        return -1;
    }
    fbconf = p_fbconfs[0];

    /* Create a pbuffer */
    i = 0;
    p_attr[i++] = GLX_PBUFFER_WIDTH;
    p_attr[i++] = 512;
    p_attr[i++] = GLX_PBUFFER_HEIGHT;
    p_attr[i++] = 512;
    p_attr[i++] = GLX_PRESERVED_CONTENTS;
    p_attr[i++] = True;
    p_attr[i++] = 0;
    OS_DATA->gpbuf = gpbuf = glXCreatePbuffer( p_display, fbconf, p_attr );
    if( !gpbuf )
    {
        msg_Err( p_thread, "Failed to create GLX pbuffer" );
        XFree( p_fbconfs );
        return -1;
    }

    /* Create the pbuffer context */
    OS_DATA->gpctx = glXCreateNewContext( p_display, fbconf, GLX_RGBA_TYPE,
                                          gwctx, True );
    if( !OS_DATA->gpctx )
    {
        msg_Err( p_thread, "Failed to create pbuffer context" );
        XFree( p_fbconfs );
        return -1;
    }

    XFree( p_fbconfs );

    XMapWindow( p_display, OS_DATA->wnd );
    XFlush( p_display );
    glXMakeContextCurrent( p_display, OS_DATA->gwnd, OS_DATA->gwnd, gwctx );

    return 0;
}


int galaktos_glx_handle_events( galaktos_thread_t *p_thread )
{
    Display *p_display;

    p_display = OS_DATA->p_display;

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
                    ((Atom)evt.xclient.data.l[0] == OS_DATA->wm_delete) )
                {
                    return 1;
                }
                break;
            }
        }
    }
    return 0;
}


void galaktos_glx_swap( galaktos_thread_t *p_thread )
{
    glXSwapBuffers( OS_DATA->p_display, OS_DATA->gwnd );
}


void galaktos_glx_done( galaktos_thread_t *p_thread )
{
    Display *p_display;

    p_display = OS_DATA->p_display;
    glXDestroyContext( p_display, OS_DATA->gpctx );
    glXDestroyPbuffer( p_display, OS_DATA->gpbuf );
    glXDestroyContext( p_display, OS_DATA->gwctx );
    glXDestroyWindow( p_display, OS_DATA->gwnd );
    XDestroyWindow( p_display, OS_DATA->wnd );
    XCloseDisplay( p_display );
}


int CreateWindow( galaktos_thread_t *p_thread, XVisualInfo *p_vi,
                  int i_width, int i_height )
{
    Display *p_display;
    XSetWindowAttributes xattr;
    Window wnd;
    XSizeHints* p_size_hints;

    p_display = OS_DATA->p_display;
    /* Create the window */
    xattr.background_pixel = BlackPixel( p_display, DefaultScreen(p_display) );
    xattr.border_pixel = 0;
    OS_DATA->wnd = wnd = XCreateWindow( p_display, DefaultRootWindow(p_display),
            0, 0, i_width, i_height, 0, p_vi->depth, InputOutput, p_vi->visual,
            CWBackPixel | CWBorderPixel, &xattr);

    /* Allow the window to be deleted by the window manager */
    OS_DATA->wm_delete = XInternAtom( p_display, "WM_DELETE_WINDOW", False );
    XSetWMProtocols( p_display, wnd, &OS_DATA->wm_delete, 1 );

    /* Prevent the window from being resized */
    p_size_hints = XAllocSizeHints();
    p_size_hints->flags = PMinSize | PMaxSize;
    p_size_hints->min_width = i_width;
    p_size_hints->min_height = i_height;
    p_size_hints->max_width = i_width;
    p_size_hints->max_height = i_height;
    XSetWMNormalHints( p_display, wnd, p_size_hints );
    XFree( p_size_hints );

    XSelectInput( p_display, wnd, KeyPressMask );

    return 0;
}
