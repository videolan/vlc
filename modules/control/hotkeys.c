/*****************************************************************************
 * hotkeys.c: Hotkey handling for vlc
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: hotkeys.c,v 1.1 2003/10/26 12:46:55 sigmunau Exp $
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

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <osd.h>

#define BUFFER_SIZE 10
/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
struct intf_sys_t
{
    vlc_mutex_t         change_lock;  /* mutex to keep the callback
                                       * and the main loop from
                                       * stepping on each others
                                       * toes */
    int                 p_keys[ BUFFER_SIZE ]; /* buffer that contains
                                                * keyevents */
    int                 i_size;        /* number of events in buffer */
    input_thread_t *    p_input;       /* pointer to input */
    vout_thread_t *     p_vout;        /* pointer to vout object */
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );
static void Feedback( intf_thread_t *, char * );
static int  GetKey  ( intf_thread_t *);
static int  KeyEvent( vlc_object_t *, char const *,
                      vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("hotkey interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return 1;
    }
    vlc_mutex_init( p_intf, &p_intf->p_sys->change_lock );
    p_intf->p_sys->i_size = 0;
    p_intf->pf_run = Run;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->p_vout = NULL;
    var_AddCallback( p_intf->p_vlc, "key-pressed", KeyEvent, p_intf );
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
    if( p_intf->p_sys->p_vout )
    {
        vlc_object_release( p_intf->p_sys->p_vout );
    }
    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    playlist_t *p_playlist;
    input_thread_t *p_input;
    vout_thread_t *p_vout = NULL;
    int i_fullscreen = config_GetInt( p_intf, "fullscreen-key" );
    int i_quit = config_GetInt( p_intf, "quit-key" );
    int i_vol_up = config_GetInt( p_intf, "vol-up-key" );
    int i_vol_down = config_GetInt( p_intf, "vol-down-key" );
    int i_play_pause = config_GetInt( p_intf, "play-pause-key" );    
    int i_play = config_GetInt( p_intf, "play-key" );    
    int i_pause = config_GetInt( p_intf, "pause-key" );    
    int i_stop = config_GetInt( p_intf, "stop-key" );
    int i_next = config_GetInt( p_intf, "next-key" );
    int i_prev = config_GetInt( p_intf, "prev-key" );
    int i_faster = config_GetInt( p_intf, "faster-key" );
    int i_slower = config_GetInt( p_intf, "slower-key" );
    int i_key = 0;
    
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

        /* Update the vout */
        if( p_vout == NULL )
        {
            p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                      FIND_ANYWHERE );
            p_intf->p_sys->p_vout = p_vout;
        }
        else if( p_vout->b_die )
        {
            vlc_object_release( p_vout );
            p_vout = NULL;
            p_intf->p_sys->p_vout = NULL;
        }

        i_key = GetKey( p_intf );
        if ( !i_key )
        {
            /* No key pressed, sleep a bit more */
            msleep( INTF_IDLE_SLEEP );
            continue;
        }
        if( i_key == i_quit )
        {
            p_intf->p_vlc->b_die = VLC_TRUE;
            Feedback( p_intf, _("Quit" ) );
            continue;
        }
        if( i_key == i_vol_up )
        {
            audio_volume_t i_newvol;
            char string[9];
            aout_VolumeUp( p_intf, 1, &i_newvol );
            sprintf( string, "Vol %%%d", i_newvol*100/AOUT_VOLUME_MAX );
            Feedback( p_intf, string );
        }
        if( i_key == i_vol_down )
        {
            audio_volume_t i_newvol;
            char string[9];
            aout_VolumeDown( p_intf, 1, &i_newvol );
            sprintf( string, "Vol %%%d", i_newvol*100/AOUT_VOLUME_MAX );
            Feedback( p_intf, string );
        }
        if( p_vout )
        {
            if( i_key == i_fullscreen )
            {
                p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                continue;
            }
        }
            
        if( i_key == i_play )
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
            continue;
        }
            
        if( i_key == i_play_pause )
        {
            if( p_input &&
                p_input->stream.control.i_status != PAUSE_S )
            {
                Feedback( p_intf, _( "Pause" ) );
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
                        Feedback( p_intf, _( "Play" ) );
                        playlist_Play( p_playlist );
                        vlc_object_release( p_playlist );
                    }
                }
            }                    
            continue;
        }
            
        else if( p_input )
        {
            if( i_key == i_pause )
            {
                Feedback( p_intf, _( "Pause" ) );
                input_SetStatus( p_input, INPUT_STATUS_PAUSE );
            }
            else if( i_key == i_next )
            {
                p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                              FIND_ANYWHERE );
                if( p_playlist )
                {
                    playlist_Next( p_playlist );
                    vlc_object_release( p_playlist );
                }
            }
            else if( i_key == i_prev )
            {
                p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                              FIND_ANYWHERE );
                if( p_playlist )
                {
                    playlist_Prev( p_playlist );
                    vlc_object_release( p_playlist );
                }
            }
            else if( i_key == i_stop )
            {
                p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                              FIND_ANYWHERE );
                if( p_playlist )
                {
                    playlist_Stop( p_playlist );
                    vlc_object_release( p_playlist );
                }
            }
            else if( i_key == i_faster )
            {
                input_SetStatus( p_input, INPUT_STATUS_FASTER );
            }
            else if( i_key == i_slower )
            {
                input_SetStatus( p_input, INPUT_STATUS_SLOWER );
            }
        }

    }
}

static void Feedback( intf_thread_t *p_intf, char *psz_string )
{
    if ( p_intf->p_sys->p_vout )
    {
	vout_ShowTextRelative( p_intf->p_sys->p_vout, psz_string, NULL, 
				 OSD_ALIGN_TOP|OSD_ALIGN_RIGHT, 30,20,400000 );
    }
}

static int  GetKey  ( intf_thread_t *p_intf)
{
    vlc_mutex_lock( &p_intf->p_sys->change_lock );
    if ( p_intf->p_sys->i_size == 0 )
    {
        vlc_mutex_unlock( &p_intf->p_sys->change_lock );
        return 0;
    }
    else
    {
        int i_return = p_intf->p_sys->p_keys[ 0 ];
        int i;
        p_intf->p_sys->i_size--;
        for ( i = 0; i < BUFFER_SIZE - 1; i++)
        {
            p_intf->p_sys->p_keys[ i ] = p_intf->p_sys->p_keys[ i + 1 ];
        }
        vlc_mutex_unlock( &p_intf->p_sys->change_lock );
        return i_return;
    }
}

/*****************************************************************************
 * KeyEvent: callback for keyboard events
 *****************************************************************************/
static int KeyEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    vlc_mutex_lock( &p_intf->p_sys->change_lock );
    if ( p_intf->p_sys->i_size == BUFFER_SIZE )
    {
        msg_Warn( p_intf, "Event buffer full, dropping keypress" );
        vlc_mutex_unlock( &p_intf->p_sys->change_lock );
        return VLC_EGENERIC;
    }
    else
    {
        p_intf->p_sys->p_keys[ p_intf->p_sys->i_size ] = newval.i_int;
        p_intf->p_sys->i_size++;
    }            
    vlc_mutex_unlock( &p_intf->p_sys->change_lock );

    return VLC_SUCCESS;
}

