/*****************************************************************************
 * vout_dummy.c: Dummy video output display method for testing purposes
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
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
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "video.h"
#include "video_output.h"

#include "intf_msg.h"

#define WIDTH 16
#define HEIGHT 16
#define BITS_PER_PLANE 16
#define BYTES_PER_PIXEL 2

/*****************************************************************************
 * vout_sys_t: dummy video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the dummy specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* Dummy video memory */
    byte_t *                    p_video;                      /* base adress */
    size_t                      i_page_size;                    /* page size */

} vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     DummyOpenDisplay   ( vout_thread_t *p_vout );
static void    DummyCloseDisplay  ( vout_thread_t *p_vout );

/*****************************************************************************
 * vout_SysCreate: allocates dummy video thread output method
 *****************************************************************************
 * This function allocates and initializes a dummy vout method.
 *****************************************************************************/
int vout_SysCreate( vout_thread_t *p_vout, char *psz_display,
                    int i_root_window, void *p_data )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM) );
        return( 1 );
    }

    /* Open and initialize device */
    if( DummyOpenDisplay( p_vout ) )
    {
        intf_ErrMsg("vout error: can't open display\n");
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_SysInit: initialize dummy video thread output method
 *****************************************************************************/
int vout_SysInit( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * vout_SysEnd: terminate dummy video thread output method
 *****************************************************************************/
void vout_SysEnd( vout_thread_t *p_vout )
{
    ;
}

/*****************************************************************************
 * vout_SysDestroy: destroy dummy video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
void vout_SysDestroy( vout_thread_t *p_vout )
{
    DummyCloseDisplay( p_vout );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_SysManage: handle dummy events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
int vout_SysManage( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * vout_SysDisplay: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to dummy image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
void vout_SysDisplay( vout_thread_t *p_vout )
{
    ;
}

/* following functions are local */

/*****************************************************************************
 * DummyOpenDisplay: open and initialize dummy device
 *****************************************************************************
 * XXX?? The framebuffer mode is only provided as a fast and efficient way to
 * display video, providing the card is configured and the mode ok. It is
 * not portable, and is not supposed to work with many cards. Use at your
 * own risk !
 *****************************************************************************/

static int DummyOpenDisplay( vout_thread_t *p_vout )
{
    p_vout->i_width =                   WIDTH;
    p_vout->i_height =                  HEIGHT;
    p_vout->i_screen_depth =            BITS_PER_PLANE;
    p_vout->i_bytes_per_pixel =         BYTES_PER_PIXEL;
    p_vout->i_bytes_per_line =          WIDTH * BYTES_PER_PIXEL;

    p_vout->p_sys->i_page_size = WIDTH * HEIGHT * BYTES_PER_PIXEL;

    /* Map two framebuffers a the very beginning of the fb */
    p_vout->p_sys->p_video = malloc( p_vout->p_sys->i_page_size * 2 );
    if( (int)p_vout->p_sys->p_video == -1 )
    {
        intf_ErrMsg("vout error: can't map video memory (%s)\n", strerror(errno) );
        return( 1 );
    }

    /* Set and initialize buffers */
    vout_SetBuffers( p_vout, p_vout->p_sys->p_video,
                     p_vout->p_sys->p_video + p_vout->p_sys->i_page_size );
    return( 0 );
}

/*****************************************************************************
 * DummyCloseDisplay: close and reset dummy device
 *****************************************************************************
 * Returns all resources allocated by DummyOpenDisplay and restore the original
 * state of the device.
 *****************************************************************************/
static void DummyCloseDisplay( vout_thread_t *p_vout )
{
    free( p_vout->p_sys->p_video );
}

