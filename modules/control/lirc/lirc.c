/*****************************************************************************
 * lirc.c : lirc plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: lirc.c,v 1.4 2003/02/16 10:25:57 sigmunau Exp $
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

#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>

#include <lirc/lirc_client.h>

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
struct intf_sys_t
{
    struct lirc_config *config;
    vlc_mutex_t         change_lock;

    input_thread_t *    p_input;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("infrared remote control module") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    int i_fd;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return 1;
    }

    p_intf->pf_run = Run;

    i_fd = lirc_init( "vlc", 1 );
    if( i_fd == -1 )
    {
        msg_Err( p_intf, "lirc_init failed" );
        free( p_intf->p_sys );
        return 1;
    }

    /* We want polling */
    fcntl( i_fd, F_SETFL, fcntl( i_fd, F_GETFL ) | O_NONBLOCK );

    if( lirc_readconfig( NULL, &p_intf->p_sys->config, NULL ) != 0 )
    {
        msg_Err( p_intf, "lirc_readconfig failed" );
        lirc_deinit();
        free( p_intf->p_sys );
        return 1;
    }

    p_intf->p_sys->p_input = NULL;

    return 0;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

    /* Destroy structure */
    lirc_freeconfig( p_intf->p_sys->config );
    lirc_deinit();
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    char *code, *c;
    playlist_t *p_playlist;
    input_thread_t *p_input;

    while( !p_intf->b_die )
    {
        /* Sleep a bit */
        msleep( INTF_IDLE_SLEEP );

        /* Update the input */
        if( p_intf->p_sys->p_input == NULL )
        {
            p_intf->p_sys->p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                              FIND_ANYWHERE );
        }
        else if( p_intf->p_sys->p_input->b_dead )
        {
            vlc_object_release( p_intf->p_sys->p_input );
            p_intf->p_sys->p_input = NULL;
        }
        p_input = p_intf->p_sys->p_input;

        /* We poll the lircsocket */
        if( lirc_nextcode(&code) != 0 )
        {
            break;
        }

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
                p_intf->p_vlc->b_die = VLC_TRUE;
                continue;
            }
            if( !strcmp( c, "VOL_UP" ) )
            {
                aout_VolumeUp( p_intf, 1, NULL );
            }
            if( !strcmp( c, "VOL_DOWN" ) )
            {
                aout_VolumeDown( p_intf, 1, NULL );
            }
            if( !strcmp( c, "FULLSCREEN" ) )
            {
                vout_thread_t *p_vout;
                p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                          VLC_OBJECT_VOUT, FIND_CHILD );
                if( p_vout )
                {
                    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                    vlc_object_release( p_vout );
                }
                continue;
            }
            if( !strcmp( c, "ACTIVATE" ) )
            {
                vout_thread_t *p_vout;
                p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                          VLC_OBJECT_VOUT, FIND_CHILD );
                if( p_vout )
                {
                    vlc_value_t val;
                    val.psz_string = "ENTER";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    vlc_object_release( p_vout );
                }
                continue;
            }

            if( !strcmp( c, "LEFT" ) )
            {
                vout_thread_t *p_vout;
                p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                          VLC_OBJECT_VOUT, FIND_CHILD );
                if( p_vout )
                {
                    vlc_value_t val;
                    val.psz_string = "LEFT";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    vlc_object_release( p_vout );
                }
                continue;
            }

            if( !strcmp( c, "RIGHT" ) )
            {
                vout_thread_t *p_vout;
                p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                          VLC_OBJECT_VOUT, FIND_CHILD );
                if( p_vout )
                {
                    vlc_value_t val;
                    val.psz_string = "RIGHT";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    vlc_object_release( p_vout );
                }
                continue;
            }

            if( !strcmp( c, "UP" ) )
            {
                vout_thread_t *p_vout;
                p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                          VLC_OBJECT_VOUT, FIND_CHILD );
                if( p_vout )
                {
                    vlc_value_t val;
                    val.psz_string = "UP";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    vlc_object_release( p_vout );
                }
                continue;
            }

            if( !strcmp( c, "DOWN" ) )
            {
                vout_thread_t *p_vout;
                p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                          VLC_OBJECT_VOUT, FIND_CHILD );
                if( p_vout )
                {
                    vlc_value_t val;
                    val.psz_string = "DOWN";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    vlc_object_release( p_vout );
                }
                continue;
            }

            if( !strcmp( c, "PLAY" ) )
            {
                p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                      FIND_ANYWHERE );
                if( p_playlist )
                {
                    vlc_mutex_lock( &p_playlist->object_lock );
                    if( p_playlist->i_size )
                    {
                        vlc_mutex_unlock( &p_playlist->object_lock );
                        playlist_Play( p_playlist );
                        vlc_object_release( p_playlist );
                    }
                }
            }
            if( !strcmp( c, "PLAYPAUSE" ) )
            {
                if( p_input &&
                    p_input->stream.control.i_status != PAUSE_S )
                {
                    input_SetStatus( p_input, INPUT_STATUS_PAUSE );
                }
                else
                {
                    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                  FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        vlc_mutex_lock( &p_playlist->object_lock );
                        if( p_playlist->i_size )
                        {
                            vlc_mutex_unlock( &p_playlist->object_lock );
                            playlist_Play( p_playlist );
                            vlc_object_release( p_playlist );
                        }
                    }
                }                    
            }                
            else if( p_input )
            {
                if( !strcmp( c, "PAUSE" ) )
                {
                    input_SetStatus( p_input, INPUT_STATUS_PAUSE );
                }
                else if( !strcmp( c, "NEXT" ) )
                {
                    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                          FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        playlist_Next( p_playlist );
                        vlc_object_release( p_playlist );
                    }
                }
                else if( !strcmp( c, "PREV" ) )
                {
                    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                          FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        playlist_Prev( p_playlist );
                        vlc_object_release( p_playlist );
                    }
                }
                else if( !strcmp( c, "STOP" ) )
                {
                    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                          FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        playlist_Stop( p_playlist );
                        vlc_object_release( p_playlist );
                    }
                }
                else if( !strcmp( c, "FAST" ) )
                {
                    input_SetStatus( p_input, INPUT_STATUS_FASTER );
                }
                else if( !strcmp( c, "SLOW" ) )
                {
                    input_SetStatus( p_input, INPUT_STATUS_SLOWER );
                }
            }
        }

        free( code );
    }
}

