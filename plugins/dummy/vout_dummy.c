/*****************************************************************************
 * vout_dummy.c: Dummy video output display method for testing purposes
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: vout_dummy.c,v 1.9 2001/11/28 15:08:05 massiot Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#define MODULE_NAME dummy
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "video.h"
#include "video_output.h"

#include "modules.h"
#include "modules_export.h"

#define DUMMY_WIDTH 16
#define DUMMY_HEIGHT 16
#define DUMMY_BITS_PER_PLANE 16
#define DUMMY_BYTES_PER_PIXEL 2

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
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * intf_Probe: return a score
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "dummy" ) )
    {
        return( 999 );
    }

    return( 1 );
}

/*****************************************************************************
 * vout_Create: allocates dummy video thread output method
 *****************************************************************************
 * This function allocates and initializes a dummy vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    p_vout->i_width            = DUMMY_WIDTH;
    p_vout->i_height           = DUMMY_HEIGHT;
    p_vout->i_screen_depth     = DUMMY_BITS_PER_PLANE;
    p_vout->i_bytes_per_pixel  = DUMMY_BYTES_PER_PIXEL;
    p_vout->i_bytes_per_line   = DUMMY_WIDTH * DUMMY_BYTES_PER_PIXEL;

    p_vout->p_sys->i_page_size = DUMMY_WIDTH * DUMMY_HEIGHT
                                  * DUMMY_BYTES_PER_PIXEL;

    /* Map two framebuffers a the very beginning of the fb */
    p_vout->p_sys->p_video = malloc( 2 * p_vout->p_sys->i_page_size );
    if( p_vout->p_sys->p_video == NULL )
    {
        intf_ErrMsg( "vout error: can't map video memory (%s)",
                     strerror(errno) );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Set and initialize buffers */
    p_vout->pf_setbuffers( p_vout, p_vout->p_sys->p_video,
                     p_vout->p_sys->p_video + p_vout->p_sys->i_page_size );

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize dummy video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate dummy video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    ;
}

/*****************************************************************************
 * vout_Destroy: destroy dummy video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    free( p_vout->p_sys->p_video );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle dummy events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to dummy image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
    ;
}

