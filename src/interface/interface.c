/*****************************************************************************
 * interface.c: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as command line.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: interface.c,v 1.85 2001/12/30 07:09:56 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                                   /* FILE */
#include <string.h>                                            /* strerror() */
#include <sys/types.h>                                              /* off_t */

#include <videolan/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "audio_output.h"

#include "interface.h"
#include "intf_playlist.h"

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void intf_Manage( intf_thread_t *p_intf );

/*****************************************************************************
 * intf_Create: prepare interface before main loop
 *****************************************************************************
 * This function opens output devices and creates specific interfaces. It sends
 * its own error messages.
 *****************************************************************************/
intf_thread_t* intf_Create( void )
{
    intf_thread_t * p_intf;

    /* Allocate structure */
    p_intf = malloc( sizeof( intf_thread_t ) );
    if( !p_intf )
    {
        intf_ErrMsg( "intf error: cannot create interface thread (%s)",
                     strerror( ENOMEM ) );
        return( NULL );
    }

    /* Choose the best module */
    p_intf->p_module = module_Need( MODULE_CAPABILITY_INTF,
                           main_GetPszVariable( INTF_METHOD_VAR, NULL ),
                           NULL );

    if( p_intf->p_module == NULL )
    {
        intf_ErrMsg( "intf error: no suitable intf module" );
        free( p_intf );
        return( NULL );
    }

#define f p_intf->p_module->p_functions->intf.functions.intf
    p_intf->pf_open       = f.pf_open;
    p_intf->pf_close      = f.pf_close;
    p_intf->pf_run        = f.pf_run;
#undef f

    /* Initialize callbacks */
    p_intf->pf_manage     = intf_Manage;

    /* Initialize structure */
    p_intf->b_die         = 0;

    p_intf->p_input       = NULL;
    p_intf->b_menu        = 0;
    p_intf->b_menu_change = 0;

    if( p_intf->pf_open( p_intf ) )
    {
        intf_ErrMsg("intf error: cannot create interface");
        module_Unneed( p_intf->p_module );
        free( p_intf );
        return( NULL );
    }

    /* Initialize mutexes */
    vlc_mutex_init( &p_intf->change_lock );

    intf_WarnMsg( 1, "intf: interface initialized");
    return( p_intf );
}

/*****************************************************************************
 * intf_Manage: manage interface
 *****************************************************************************
 * This function has to be called regularly by the interface plugin. It
 * checks for playlist end, module expiration, message flushing, and a few
 * other useful things.
 *****************************************************************************/
static void intf_Manage( intf_thread_t *p_intf )
{
    /* Flush waiting messages */
    intf_FlushMsg();

    /* Manage module bank */
    module_ManageBank( );

    if( ( p_intf->p_input != NULL ) &&
            ( p_intf->p_input->b_error || p_intf->p_input->b_eof ) )
    {
        input_DestroyThread( p_intf->p_input, NULL );
        p_intf->p_input = NULL;
        intf_DbgMsg("Input thread destroyed");
    }

    /* If no stream is being played, try to find one */
    if( p_intf->p_input == NULL && !p_intf->b_die )
    {
//        vlc_mutex_lock( &p_main->p_playlist->change_lock );

        if( !p_main->p_playlist->b_stopped )
        {
            /* Select the next playlist item */
            intf_PlaylistNext( p_main->p_playlist );

            /* don't loop by default: stop at playlist end */
            if( p_main->p_playlist->i_index == -1 )
            {
                p_main->p_playlist->b_stopped = 1;
            }
            else
            {
                p_main->p_playlist->b_stopped = 0;
                p_main->p_playlist->i_mode = PLAYLIST_FORWARD + 
                    main_GetIntVariable( PLAYLIST_LOOP_VAR,
                                         PLAYLIST_LOOP_DEFAULT );
                p_intf->p_input =
                    input_CreateThread( &p_main->p_playlist->current, NULL );
            }
        }
        else
        {
            /* playing has been stopped: we no longer need outputs */
            if( p_aout_bank->i_count )
            {
                /* FIXME kludge that does not work with several outputs */
                aout_DestroyThread( p_aout_bank->pp_aout[0], NULL );
                p_aout_bank->i_count--;
            }
            if( p_vout_bank->i_count )
            {
                vout_DestroyThread( p_vout_bank->pp_vout[0], NULL );
                p_vout_bank->i_count--;
            }
        }

//        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }
}

/*****************************************************************************
 * intf_Destroy: clean interface after main loop
 *****************************************************************************
 * This function destroys specific interfaces and close output devices.
 *****************************************************************************/
void intf_Destroy( intf_thread_t *p_intf )
{
    /* Destroy interfaces */
    p_intf->pf_close( p_intf );

    /* Close input thread, if any (blocking) */
    if( p_intf->p_input )
    {   
        input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Unlock module */
    module_Unneed( p_intf->p_module );

    vlc_mutex_destroy( &p_intf->change_lock );

    /* Free structure */
    free( p_intf );
}

