/*****************************************************************************
 * intf_sdl.c: SDL interface plugin
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
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
#include <SDL/SDL.h>	        		       /* for all the SDL stuff	     */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "input.h"
#include "video.h"
#include "video_output.h"

#include "interface.h"
#include "intf_msg.h"

#include "main.h"

/*****************************************************************************
 * intf_sys_t: description and status of SDL interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    /* SDL system information */
    SDL_Surface	*		p_display;                                /* display */
	
} intf_sys_t;


/*****************************************************************************
 * intf_SDLCreate: initialize and create SDL interface
 *****************************************************************************/
int intf_SDLCreate( intf_thread_t *p_intf )
{
    /* Check that b_video is set */
    if( !p_main->b_video )
    {
        intf_ErrMsg("error: SDL interface require a video output thread\n");
        return( 1 );
    }

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM) );
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
        intf_ErrMsg("error: can't create video output thread\n" );
        free( p_intf->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * intf_SDLDestroy: destroy interface
 *****************************************************************************/
void intf_SDLDestroy( intf_thread_t *p_intf )
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
    
    SDL_FreeSurface( p_intf->p_sys->p_display );     /* destroy the "screen" */
    SDL_Quit();
    free( p_intf->p_sys );
}


/*****************************************************************************
 * intf_SDLManage: event loop
 *****************************************************************************/
void intf_SDLManage( intf_thread_t *p_intf )
{
	SDL_Event event; 										/*	SDL event	 */
    Uint8   i_key;
    
    while ( SDL_PollEvent(&event) ) {
        switch (event.type) {           
            case SDL_KEYDOWN:                         /* if a key is pressed */
                i_key = event.key.keysym.sym;                  /* forward it */
                
                if( intf_ProcessKey( p_intf, (char ) i_key ) )
                {
                    intf_DbgMsg("unhandled key '%c' (%i)\n", 
                                (char) i_key, i_key );
                }
                break;
            case SDL_QUIT:
                intf_ProcessKey( p_intf, 'Q' ); 
                break;
            default:
                break;
        }
    }
}
