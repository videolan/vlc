/*****************************************************************************
 * lirc.c : lirc plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: lirc.c,v 1.6 2002/02/20 05:56:18 sam Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <videolan/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"
#include "intf_msg.h"
#include "interface.h"
#include "intf_playlist.h"

#include "video.h"
#include "video_output.h"

#include <lirc/lirc_client.h>

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    struct lirc_config *config;
    vlc_mutex_t         change_lock;
} intf_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void intf_getfunctions( function_list_t * p_function_list );

static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for lirc module" )
    ADD_COMMENT( "use ~/.lircrc" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "infrared remote control module" )
    ADD_CAPABILITY( INTF, 8 )
    ADD_SHORTCUT( "lirc" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    intf_getfunctions( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void intf_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Open: initialize dummy interface
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg("no mem?");
        return 1;
    };

    if( lirc_init("vlc", 1) == -1 )
    {
        intf_ErrMsg( "intf error: lirc_init failed" );
        free( p_intf->p_sys );
        return 1;
    }

    if( lirc_readconfig( NULL, &p_intf->p_sys->config, NULL ) != 0 )
    {
        intf_ErrMsg( "intf error: lirc_readconfig failed" );
        lirc_deinit();
        free( p_intf->p_sys );
        return 1;
    }

    return 0;
}

/*****************************************************************************
 * intf_Close: destroy dummy interface
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy structure */
    lirc_freeconfig( p_intf->p_sys->config );
    lirc_deinit();
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: main loop
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    char *code;
    char *c;

    /* Manage core vlc functions through the callback */
    p_intf->pf_manage( p_intf );

    while( !p_intf->b_die && lirc_nextcode(&code) == 0 )
    {
        if( code == NULL )
        {
            continue;
        }

        while( !p_intf->b_die 
                && lirc_code2char( p_intf->p_sys->config, code, &c ) == 0
                && c != NULL )
        {
            if( !strcmp( c, "QUIT" ) )
            {
                p_intf->b_die = 1;
                continue;
            }

            if( !strcmp( c, "FULLSCREEN" ) )
            {
                vlc_mutex_lock( &p_vout_bank->lock );
                /* XXX: only fullscreen the first video output */
                if( p_vout_bank->i_count )
                {
                    p_vout_bank->pp_vout[0]->i_changes
                        |= VOUT_FULLSCREEN_CHANGE;
                }
                vlc_mutex_unlock( &p_vout_bank->lock );
                continue;
            }

            vlc_mutex_lock( &p_input_bank->lock );

            if( !strcmp( c, "PLAY" ) )
            {
                if( p_input_bank->pp_input[0] != NULL )
                {
                    input_SetStatus( p_input_bank->pp_input[0],
                                     INPUT_STATUS_PLAY );
                    p_main->p_playlist->b_stopped = 0;
                }
                else
                {
                    vlc_mutex_lock( &p_main->p_playlist->change_lock );

                    if( p_main->p_playlist->b_stopped )
                    {
                        if( p_main->p_playlist->i_size )
                        {
                            vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                            intf_PlaylistJumpto( p_main->p_playlist,
                                                 p_main->p_playlist->i_index );
                        }
                        else
                        {
                            vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                        }
                    }
                    else
                    {
                        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                    }
                }
            }
            else if( p_input_bank->pp_input[0] != NULL )
            {
                if( !strcmp( c, "PAUSE" ) )
                {
                    input_SetStatus( p_input_bank->pp_input[0],
                                     INPUT_STATUS_PAUSE );

                    vlc_mutex_lock( &p_main->p_playlist->change_lock );
                    p_main->p_playlist->b_stopped = 0;
                    vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                }
                else if( !strcmp( c, "NEXT" ) )
                {
                    p_input_bank->pp_input[0]->b_eof = 1;
                }
                else if( !strcmp( c, "LAST" ) )
                {
                    /* FIXME: temporary hack */
                    intf_PlaylistPrev( p_main->p_playlist );
                    intf_PlaylistPrev( p_main->p_playlist );
                    p_input_bank->pp_input[0]->b_eof = 1;
                }
                else if( !strcmp( c, "STOP" ) )
                {
                    /* end playing item */
                    p_input_bank->pp_input[0]->b_eof = 1;
    
                    /* update playlist */
                    vlc_mutex_lock( &p_main->p_playlist->change_lock );
    
                    p_main->p_playlist->i_index--;
                    p_main->p_playlist->b_stopped = 1;
    
                    vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                }
                else if( !strcmp( c, "FAST" ) )
                {
                    input_SetStatus( p_input_bank->pp_input[0],
                                     INPUT_STATUS_FASTER );
    
                    vlc_mutex_lock( &p_main->p_playlist->change_lock );
                    p_main->p_playlist->b_stopped = 0;
                    vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                }
                else if( !strcmp( c, "SLOW" ) )
                {
                    input_SetStatus( p_input_bank->pp_input[0],
                                     INPUT_STATUS_SLOWER );
    
                    vlc_mutex_lock( &p_main->p_playlist->change_lock );
                    p_main->p_playlist->b_stopped = 0;
                    vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                }
            }

            vlc_mutex_unlock( &p_input_bank->lock );
        }

        free( code );

        /* Manage core vlc functions through the callback */
        p_intf->pf_manage( p_intf );
    }
}

