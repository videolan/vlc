/*****************************************************************************
 * interface.c: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as command line.
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: interface.c,v 1.80 2001/07/18 14:21:00 massiot Exp $
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
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                                   /* FILE */
#include <string.h>                                            /* strerror() */
#include <sys/types.h>                                              /* off_t */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "modules.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "audio_output.h"

#include "intf_msg.h"
#include "interface.h"
#include "intf_playlist.h"
#include "intf_channels.h"
#include "keystrokes.h"

#include "video.h"
#include "video_output.h"

#include "main.h"

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
    p_intf->p_module = module_Need( MODULE_CAPABILITY_INTF, NULL );

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
    p_intf->p_keys        = NULL;
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

    /* Load channels - the pointer will be set to NULL on failure. The
     * return value is ignored since the program can work without
     * channels */
    intf_LoadChannels( p_intf, main_GetPszVariable( INTF_CHANNELS_VAR,
                                                    INTF_CHANNELS_DEFAULT ));

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
    p_intf_key  p_cur;
    p_intf_key  p_next;

    /* Unload channels */
    intf_UnloadChannels( p_intf );

    /* Destroy interfaces */
    p_intf->pf_close( p_intf );

    /* Close input thread, if any (blocking) */
    if( p_intf->p_input )
    {   
        input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Destroy keymap */
    p_cur = p_intf->p_keys;
    while( p_cur != NULL)
    {
        p_next = p_cur->next;
        free(p_cur);
        p_cur = p_next;
    }
         
    /* Unlock module */
    module_Unneed( p_intf->p_module );

    vlc_mutex_destroy( &p_intf->change_lock );

    /* Free structure */
    free( p_intf );
}

/*****************************************************************************
 * intf_AssignKey: assign standartkeys                                       *
 *****************************************************************************
 * This function fills in the associative array that links the key pressed   *
 * and the key we use internally. Support one extra parameter.               *
 ****************************************************************************/
void intf_AssignKey( intf_thread_t *p_intf, int r_key, int f_key, int param)
{
    p_intf_key  p_cur =  p_intf->p_keys;
    if( p_cur == NULL )
    {
        p_cur = (p_intf_key )(malloc ( sizeof( intf_key ) ) );
        p_cur->received_key = r_key;
        p_cur->forwarded.key = f_key;
        p_cur->forwarded.param = param; 
        p_cur->next = NULL;
        p_intf->p_keys = p_cur;
    } 
    else 
    {
        while( p_cur->next != NULL && p_cur ->received_key != r_key)
        {
            p_cur = p_cur->next;
        }
        if( p_cur->next == NULL )
        {   
            p_cur->next  = ( p_intf_key )( malloc( sizeof( intf_key ) ) );
            p_cur = p_cur->next;
            p_cur->next = NULL;
            p_cur->forwarded.param = param; 
            p_cur->received_key = r_key;
        }
        p_cur->forwarded.key = f_key;
    }        
}

/* Basic getKey function... */
keyparm intf_GetKey( intf_thread_t *p_intf, int r_key)
{   
    keyparm reply;
    
    p_intf_key current = p_intf->p_keys;
    while(current != NULL && current->received_key != r_key)
    {    
        current = current->next;
    }
    if(current == NULL)
    {   /* didn't find any key in the array */ 
        reply.key = INTF_KEY_UNKNOWN;
        reply.param = 0;
    }
    else
    {
        reply.key = current->forwarded.key;
        reply.param = current->forwarded.param;
    }
    return reply;
}

/*****************************************************************************
* intf_AssignNormalKeys: used for normal interfaces.
*****************************************************************************
* This function assign the basic key to the normal keys.
*****************************************************************************/

void intf_AssignNormalKeys( intf_thread_t *p_intf)
{
    p_intf->p_intf_get_key = intf_GetKey;

    intf_AssignKey( p_intf , 'Q', INTF_KEY_QUIT, 0);
    intf_AssignKey( p_intf , 'q', INTF_KEY_QUIT, 0);
    intf_AssignKey( p_intf ,  27, INTF_KEY_QUIT, 0);
    intf_AssignKey( p_intf ,   3, INTF_KEY_QUIT, 0);
    intf_AssignKey( p_intf , '0', INTF_KEY_SET_CHANNEL, 0);
    intf_AssignKey( p_intf , '1', INTF_KEY_SET_CHANNEL, 1);
    intf_AssignKey( p_intf , '2', INTF_KEY_SET_CHANNEL, 2);
    intf_AssignKey( p_intf , '3', INTF_KEY_SET_CHANNEL, 3);
    intf_AssignKey( p_intf , '4', INTF_KEY_SET_CHANNEL, 4);
    intf_AssignKey( p_intf , '5', INTF_KEY_SET_CHANNEL, 5);
    intf_AssignKey( p_intf , '6', INTF_KEY_SET_CHANNEL, 6);
    intf_AssignKey( p_intf , '7', INTF_KEY_SET_CHANNEL, 7);
    intf_AssignKey( p_intf , '8', INTF_KEY_SET_CHANNEL, 8);
    intf_AssignKey( p_intf , '9', INTF_KEY_SET_CHANNEL, 9);
    intf_AssignKey( p_intf , '0', INTF_KEY_SET_CHANNEL, 0);
    intf_AssignKey( p_intf , '+', INTF_KEY_INC_VOLUME, 0);
    intf_AssignKey( p_intf , '-', INTF_KEY_DEC_VOLUME, 0);
    intf_AssignKey( p_intf , 'm', INTF_KEY_TOGGLE_VOLUME, 0);
    intf_AssignKey( p_intf , 'M', INTF_KEY_TOGGLE_VOLUME, 0);
    intf_AssignKey( p_intf , 'g', INTF_KEY_DEC_GAMMA, 0);
    intf_AssignKey( p_intf , 'G', INTF_KEY_INC_GAMMA, 0);
    intf_AssignKey( p_intf , 'c', INTF_KEY_TOGGLE_GRAYSCALE, 0);
    intf_AssignKey( p_intf , ' ', INTF_KEY_TOGGLE_INTERFACE, 0);
    intf_AssignKey( p_intf , 'i', INTF_KEY_TOGGLE_INFO, 0);
    intf_AssignKey( p_intf , 's', INTF_KEY_TOGGLE_SCALING, 0);
    intf_AssignKey( p_intf , 'd', INTF_KEY_DUMP_STREAM, 0);
}   

/*****************************************************************************
 * intf_ProcessKey: process standard keys
 *****************************************************************************
 * This function will process standard keys and return non 0 if the key was
 * unknown.
 *****************************************************************************/
int intf_ProcessKey( intf_thread_t *p_intf, int g_key )
{
    int i_index;
    keyparm k_reply;
    
    k_reply = intf_GetKey( p_intf, g_key); 
    switch( k_reply.key )
    {
    case INTF_KEY_QUIT:                                        /* quit order */
        p_intf->b_die = 1;
        break;

    case INTF_KEY_SET_CHANNEL:
        /* Change channel - return code is ignored since SelectChannel displays
         * its own error messages */
/*        intf_SelectChannel( p_intf, k_reply.param ); */
/*        network_ChannelJoin() */
/* FIXME : keyboard event is for the time being half handled by the interface
 * half handled directly by the plugins. We should decide what to do. */        
        break;

    case INTF_KEY_INC_VOLUME:                                    /* volume + */
        vlc_mutex_lock( &p_aout_bank->lock );
        for( i_index = 0 ; i_index < p_aout_bank->i_count ; i_index++ )
        {
            if( p_aout_bank->pp_aout[i_index]->i_volume
                                                   < VOLUME_MAX - VOLUME_STEP )
            {
                p_aout_bank->pp_aout[i_index]->i_volume += VOLUME_STEP;
            }
            else
            {
                p_aout_bank->pp_aout[i_index]->i_volume = VOLUME_MAX;
            }
        }
        vlc_mutex_unlock( &p_aout_bank->lock );
        break;

    case INTF_KEY_DEC_VOLUME:                                    /* volume - */
        vlc_mutex_lock( &p_aout_bank->lock );
        for( i_index = 0 ; i_index < p_aout_bank->i_count ; i_index++ )
        {
            if( p_aout_bank->pp_aout[i_index]->i_volume > VOLUME_STEP )
            {
                p_aout_bank->pp_aout[i_index]->i_volume -= VOLUME_STEP;
            }
            else
            {
                p_aout_bank->pp_aout[i_index]->i_volume = 0;
            }
        }
        vlc_mutex_unlock( &p_aout_bank->lock );
        break;

    case INTF_KEY_TOGGLE_VOLUME:                              /* toggle mute */
        /* Start/stop feeding audio data. */
        if( p_intf->p_input != NULL )
        {
            input_ToggleMute( p_intf->p_input );
        }

        /* Start/stop playing sound. */
        vlc_mutex_lock( &p_aout_bank->lock );
        for( i_index = 0 ; i_index < p_aout_bank->i_count ; i_index++ )
        {
            if( p_aout_bank->pp_aout[i_index]->i_savedvolume )
            {
                p_aout_bank->pp_aout[i_index]->i_volume =
                                p_aout_bank->pp_aout[i_index]->i_savedvolume;
                p_aout_bank->pp_aout[i_index]->i_savedvolume = 0;
            }
            else
            {
                p_aout_bank->pp_aout[i_index]->i_savedvolume =
                                p_aout_bank->pp_aout[i_index]->i_volume;
                p_aout_bank->pp_aout[i_index]->i_volume = 0;
            }
        }
        vlc_mutex_unlock( &p_aout_bank->lock );
        break;

/* XXX: fix this later */
#if 0
    case INTF_KEY_DEC_GAMMA:                                      /* gamma - */
        if( (p_main->p_vout != NULL) && (p_main->p_vout->f_gamma > -INTF_GAMMA_LIMIT) )
        {
            /* FIXME: we should lock if called from the interface */
            p_main->p_vout->f_gamma   -= INTF_GAMMA_STEP;
            p_main->p_vout->i_changes |= VOUT_GAMMA_CHANGE;
        }
        break;
    case INTF_KEY_INC_GAMMA:                                      /* gamma + */
        if( (p_main->p_vout != NULL) && (p_main->p_vout->f_gamma < INTF_GAMMA_LIMIT) )
        {
            /* FIXME: we should lock if called from the interface */
            p_main->p_vout->f_gamma   += INTF_GAMMA_STEP;
            p_main->p_vout->i_changes |= VOUT_GAMMA_CHANGE;
        }
        break;
#endif

   case INTF_KEY_DUMP_STREAM:
        if( p_intf->p_input != NULL )
        {
            vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
            input_DumpStream( p_intf->p_input );
            vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );
        }
        break;

    default:                                                  /* unknown key */
        return( 1 );
    }

    return( 0 );
}

