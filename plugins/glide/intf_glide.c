/*****************************************************************************
 * intf_glide.c: 3dfx interface plugin
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

#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */
#include <linutil.h>                            /* Glide kbhit() and getch() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "video.h"
#include "video_output.h"

#include "intf_msg.h"
#include "interface.h"

#include "main.h"

/*****************************************************************************
 * intf_sys_t: description and status of 3dfx interface
 *****************************************************************************/
typedef struct intf_sys_s
{

} intf_sys_t;

/*****************************************************************************
 * intf_GlideCreate: initialize 3dfx interface
 *****************************************************************************/
int intf_GlideCreate( intf_thread_t *p_intf )
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    /* Spawn video output thread */
    if( p_main->b_video )
    {
        p_intf->p_vout = vout_CreateThread( NULL, 0, 0, 0, NULL, 0, NULL );
        if( p_intf->p_vout == NULL )                                /* error */
        {
            intf_ErrMsg("intf error: can't create output thread" );
            return( 1 );
        }
    }
    
    /* bind keys */
    intf_AssignNormalKeys( p_intf );

    return( 0 );
}

/*****************************************************************************
 * intf_GlideDestroy: destroy 3dfx interface
 *****************************************************************************/
void intf_GlideDestroy( intf_thread_t *p_intf )
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
 * intf_GlideManage: event loop
 *****************************************************************************/
void intf_GlideManage( intf_thread_t *p_intf )
{
    unsigned int buf;

    /* very Linux specific - see tlib.c in Glide for other versions */
    while( kbhit() )
    {
        if( intf_ProcessKey(p_intf, (int)buf = getch()) )
        {
            intf_ErrMsg( "unhandled key '%c' (%i)", (char) buf, buf );
        }
    }
}

