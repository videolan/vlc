/*****************************************************************************
 * intf_ggi.c: GGI interface plugin
 * Since GII doesnt seem to work well for keyboard events, the GGI display is
 * used, and therefore the GII interface can't be spawned without a video output
 * thread. It also needs a kludge to get the visual from the video output GGI
 * driver.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_ggi.c,v 1.7 2001/01/05 18:46:43 massiot Exp $
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ggi/ggi.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "video.h"
#include "video_output.h"

#include "interface.h"
#include "intf_msg.h"

#include "main.h"

/*****************************************************************************
 * intf_sys_t: description and status of GGI interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    /* GGI system information */
    ggi_visual_t                 p_display;                       /* display */

} intf_sys_t;

/*****************************************************************************
 * External prototypes
 *****************************************************************************/

/* vout_SysGetVisual: get back visual from video output thread - in video_ggi.c
 * This function is used to get back the display pointer once the video output
 * thread has been spawned. */
ggi_visual_t    vout_SysGetVisual( vout_thread_t *p_vout );

/*****************************************************************************
 * intf_GGICreate: initialize and create GII interface
 *****************************************************************************/
int intf_GGICreate( intf_thread_t *p_intf )
{
    /* Check that b_video is set */
    if( !p_main->b_video )
    {
        intf_ErrMsg("error: GGI interface require a video output thread");
        return( 1 );
    }

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Spawn video output thread */
    p_intf->p_vout = vout_CreateThread( main_GetPszVariable( VOUT_DISPLAY_VAR,
                                                             NULL), 0,
                                        main_GetIntVariable( VOUT_WIDTH_VAR,
                                                         VOUT_WIDTH_DEFAULT ),
                                        main_GetIntVariable( VOUT_HEIGHT_VAR,
                                                        VOUT_HEIGHT_DEFAULT ),
                                        NULL, 0,
                                        (void *)&p_intf->p_sys->p_display );

    if( p_intf->p_vout == NULL )                                  /* error */
    {
        intf_ErrMsg("error: can't create video output thread" );
        free( p_intf->p_sys );
        return( 1 );
    }
    
    /* Assign basic keys */
    intf_AssignNormalKeys( p_intf );

    
    return( 0 );
}

/*****************************************************************************
 * intf_GGIDestroy: destroy interface
 *****************************************************************************/
void intf_GGIDestroy( intf_thread_t *p_intf )
{
    /* Close input thread, if any (blocking) */
    if( p_intf->p_input )
    {
        input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Close video output thread, if any (blocking) */
    if( p_intf->p_vout )
    {
        vout_DestroyThread( p_intf->p_vout, NULL );
    }

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * intf_GGIManage: event loop
 *****************************************************************************/
void intf_GGIManage( intf_thread_t *p_intf )
{
    int         i_key;                                        /* unicode key */

    /* For all events in queue */
    while( ggiKbhit( p_intf->p_sys->p_display ) )
    {
        i_key = ggiGetc( p_intf->p_sys->p_display );
        if( intf_ProcessKey( p_intf, i_key ) )
        {
            intf_DbgMsg("unhandled key '%c' (%i)", (char) i_key, i_key );
        }
    }
}



