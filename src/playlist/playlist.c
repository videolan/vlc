/*****************************************************************************
 * playlist.c : Playlist management functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: playlist.c,v 1.5 2002/06/04 00:11:12 sam Exp $
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
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <errno.h>                                                 /* ENOMEM */

#include <vlc/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "playlist.h"

#define PLAYLIST_STOPPED 0
#define PLAYLIST_RUNNING 1

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunThread ( playlist_t * );

/*****************************************************************************
 * playlist_Create: create playlist
 *****************************************************************************
 * Create a playlist structure.
 *****************************************************************************/
playlist_t * __playlist_Create ( vlc_object_t *p_parent )
{
    playlist_t *p_playlist;

    /* Allocate structure */
    p_playlist = vlc_object_create( p_parent, VLC_OBJECT_PLAYLIST );
    if( !p_playlist )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    p_playlist->p_input = NULL;
    p_playlist->i_status = PLAYLIST_RUNNING;
    p_playlist->i_index = -1;
    p_playlist->i_size = 0;
    p_playlist->pp_items = NULL;
    vlc_mutex_init( p_playlist, &p_playlist->change_lock );

    vlc_object_attach( p_playlist, p_parent );

    if( vlc_thread_create( p_playlist, "playlist", RunThread, 0 ) )
    {
        msg_Err( p_playlist, "cannot spawn playlist thread" );
        vlc_object_detach_all( p_playlist );
        vlc_mutex_destroy( &p_playlist->change_lock );
        vlc_object_destroy( p_playlist );
        return NULL;
    }

    return p_playlist;
}

/*****************************************************************************
 * playlist_Destroy: destroy the playlist
 *****************************************************************************
 * Delete all items in the playlist and free the playlist structure.
 *****************************************************************************/
void playlist_Destroy( playlist_t * p_playlist )
{
    p_playlist->b_die = 1;

    vlc_thread_join( p_playlist );

    vlc_mutex_destroy( &p_playlist->change_lock );
    vlc_object_destroy( p_playlist );
}

/*****************************************************************************
 * playlist_Add: add an item to the playlist
 *****************************************************************************
 * Add an item to the playlist at position i_pos. If i_pos is PLAYLIST_END,
 * add it at the end regardless of the playlist current size.
 *****************************************************************************/
int playlist_Add( playlist_t *p_playlist, int i_pos, const char * psz_item )
{
    msg_Warn( p_playlist, "adding playlist item « %s »", psz_item );

    vlc_mutex_lock( &p_playlist->change_lock );

    p_playlist->i_size++;
    p_playlist->pp_items = realloc( p_playlist->pp_items,
                                    p_playlist->i_size * sizeof(void*) );
    if( p_playlist->pp_items == NULL )
    {
        msg_Err( p_playlist, "out of memory" );
        vlc_mutex_unlock( &p_playlist->change_lock );
        vlc_object_release( p_playlist );
        return -1;
    }

    i_pos = p_playlist->i_size - 1; /* FIXME */
    p_playlist->pp_items[i_pos] = malloc( sizeof( playlist_item_t ) );
    p_playlist->pp_items[i_pos]->psz_name = strdup( psz_item );
    p_playlist->pp_items[i_pos]->i_type = 0;
    p_playlist->pp_items[i_pos]->i_status = 0;

    p_playlist->i_status = PLAYLIST_RUNNING;

    vlc_mutex_unlock( &p_playlist->change_lock );

    return 0;
}

/*****************************************************************************
 * playlist_Delete: delete an item from the playlist
 *****************************************************************************
 * Delete the item in the playlist with position i_pos.
 *****************************************************************************/
int playlist_Delete( playlist_t * p_playlist, int i_pos )
{
    vlc_mutex_lock( &p_playlist->change_lock );

    vlc_mutex_unlock( &p_playlist->change_lock );

    return 0;
}

/*****************************************************************************
 * playlist_Command: do a playlist action
 *****************************************************************************
 * 
 *****************************************************************************/
void playlist_Command( playlist_t * p_playlist, int i_command, int i_arg )
{   
    vlc_mutex_lock( &p_playlist->change_lock );

    switch( i_command )
    {
    case PLAYLIST_STOP:
        msg_Dbg( p_playlist, "stopping" );
        p_playlist->i_status = PLAYLIST_STOPPED;
        break;
    case PLAYLIST_PLAY:
        msg_Dbg( p_playlist, "running" );
        p_playlist->i_status = PLAYLIST_RUNNING;
        break;
    case PLAYLIST_SKIP:
        msg_Dbg( p_playlist, "next" );
        if( p_playlist->i_size )
        {
            p_playlist->i_index = 0;
            p_playlist->i_status = PLAYLIST_RUNNING;
        }
        break;
    default:
        break;
    }

    vlc_mutex_unlock( &p_playlist->change_lock );

    return;
}

/* Following functions are local */

/*****************************************************************************
 * RunThread: main playlist thread
 *****************************************************************************/
static void RunThread ( playlist_t *p_playlist )
{
    while( !p_playlist->b_die )
    {
        /* If there is an input, check that it doesn't need to die. */
        if( p_playlist->p_input )
        {
            /* This input is dead. Remove it ! */
            if( p_playlist->p_input->b_dead )
            {
                input_thread_t *p_input;

                /* Unlink current input */
                vlc_mutex_lock( &p_playlist->change_lock );
                p_input = p_playlist->p_input;
                p_playlist->p_input = NULL;
                vlc_object_detach_all( p_input );
                vlc_mutex_unlock( &p_playlist->change_lock );

                /* Destroy input */
                vlc_object_release( p_input );
                input_DestroyThread( p_input );
                vlc_object_destroy( p_input );
            }
            /* This input is dying, let him do */
            else if( p_playlist->p_input->b_die )
            {
                ;
            }
            /* This input has finished, ask him to die ! */
            else if( p_playlist->p_input->b_error
                      || p_playlist->p_input->b_eof )
            {
                input_StopThread( p_playlist->p_input );
            }
        }
        else if( p_playlist->i_status != PLAYLIST_STOPPED )
        {
            /* Select the next playlist item */
            playlist_Next( p_playlist );

            /* don't loop by default: stop at playlist end */
            if( p_playlist->i_index == -1 )
            {
                p_playlist->i_status = PLAYLIST_STOPPED;
            }
            else
            {
                input_thread_t *p_input;

                //p_playlist->i_mode = PLAYLIST_FORWARD +
                //    config_GetInt( p_playlist, "loop-playlist" );
                msg_Dbg( p_playlist, "creating new input thread" );
                p_input = input_CreateThread( p_playlist,
                            p_playlist->pp_items[p_playlist->i_index], NULL );
                if( p_input != NULL )
                {
                    /* Link current input */
                    vlc_mutex_lock( &p_playlist->change_lock );
                    p_playlist->p_input = p_input;
                    vlc_mutex_unlock( &p_playlist->change_lock );
                }
            }
        }

        msleep( INTF_IDLE_SLEEP );
    }

    /* If there is an input, kill it */
    while( p_playlist->p_input )
    {
        if( p_playlist->p_input->b_dead )
        {
            input_thread_t *p_input;

            /* Unlink current input */
            vlc_mutex_lock( &p_playlist->change_lock );
            p_input = p_playlist->p_input;
            p_playlist->p_input = NULL;
            vlc_object_detach_all( p_input );
            vlc_mutex_unlock( &p_playlist->change_lock );

            /* Destroy input */
            vlc_object_release( p_input );
            input_DestroyThread( p_input );
            vlc_object_destroy( p_input );
        }
        /* This input is dying, let him do */
        else if( p_playlist->p_input->b_die )
        {
            ;
        }
        else if( p_playlist->p_input->b_error || p_playlist->p_input->b_eof )
        {
            input_StopThread( p_playlist->p_input );
        }
        else
        {
            p_playlist->p_input->b_eof = 1;
        }

        msleep( INTF_IDLE_SLEEP );
    }
}

