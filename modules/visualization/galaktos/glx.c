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
static int CreateWindow( galaktos_thread_t *p_thread, int i_width,
                         int i_height );


typedef struct
{
    Display     *p_display;
    GLXFBConfig fbconf;
    Window      wnd;
    GLXWindow   gwnd;
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
    static const int p_attr[] = { GLX_RED_SIZE, 5, GLX_GREEN_SIZE, 5,
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
    OS_DATA->fbconf = fbconf = p_fbconfs[0];

    if( CreateWindow( p_thread, i_width, i_height ) == -1 )
    {
        XFree( p_fbconfs );
        return -1;
    }

    msg_Err( p_thread, "NOT IMPLEMENTED YET ;)" );
    return 0;
}


int CreateWindow( galaktos_thread_t *p_thread, int i_width, int i_height )
{
    Display *p_display;
    XVisualInfo *p_vi;
    XSetWindowAttributes xattr;
    Window wnd;

    p_display = OS_DATA->p_display;
    /* Get the X11 visual */
    p_vi = glXGetVisualFromFBConfig( p_display, OS_DATA->fbconf );
    if( !p_vi )
    {
        msg_Err( p_thread, "Cannot get X11 visual" );
        return -1;
    }

    /* Create the window */
    xattr.background_pixel = BlackPixel( p_display, DefaultScreen(p_display) );
    xattr.border_pixel = 0;
    OS_DATA->wnd = wnd = XCreateWindow( p_display, DefaultRootWindow(p_display),
            0, 0, i_width, i_height, 0, p_vi->depth, InputOutput, p_vi->visual,
            CWBackPixel | CWBorderPixel, &xattr);
    XFree( p_vi );

    /* Create the GLX window */
    OS_DATA->gwnd = glXCreateWindow( p_display, OS_DATA->fbconf, wnd, NULL );
    if( OS_DATA->gwnd == None )
    {
        msg_Err( p_thread, "Cannot create GLX window" );
        return -1;
    }

    return 0;
}
