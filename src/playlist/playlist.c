/*****************************************************************************
 * playlist.c : Playlist management functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: playlist.c,v 1.8 2002/06/07 19:54:37 sam Exp $
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
static void SkipItem  ( playlist_t *, int );
static void PlayItem  ( playlist_t * );

static void Poubellize ( playlist_t *, input_thread_t * );

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

    if( vlc_thread_create( p_playlist, "playlist", RunThread, VLC_TRUE ) )
    {
        msg_Err( p_playlist, "cannot spawn playlist thread" );
        vlc_object_destroy( p_playlist );
        return NULL;
    }

    /* The object has been initialized, now attach it */
    vlc_object_attach( p_playlist, p_parent );

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

    vlc_object_destroy( p_playlist );
}

/*****************************************************************************
 * playlist_Add: add an item to the playlist
 *****************************************************************************
 * Add an item to the playlist at position i_pos. If i_pos is PLAYLIST_END,
 * add it at the end regardless of the playlist current size.
 *****************************************************************************/
int playlist_Add( playlist_t *p_playlist, const char * psz_target,
                                          int i_mode, int i_pos )
{
    playlist_item_t *p_item;

    msg_Warn( p_playlist, "adding playlist item « %s »", psz_target );

    /* Create the new playlist item */
    p_item = malloc( sizeof( playlist_item_t ) );
    if( p_item == NULL )
    {
        msg_Err( p_playlist, "out of memory" );
    }

    p_item->psz_name = strdup( psz_target );
    p_item->i_type = 0;
    p_item->i_status = 0;

    vlc_mutex_lock( &p_playlist->object_lock );

    /* Do a few boundary checks and allocate space for the item */
    if( i_pos == PLAYLIST_END )
    {
        if( i_mode & PLAYLIST_INSERT )
        {
            i_mode &= ~PLAYLIST_INSERT;
            i_mode |= PLAYLIST_APPEND;
        }

        i_pos = p_playlist->i_size - 1;
    }

    if( !(i_mode & PLAYLIST_REPLACE)
         || i_pos < 0 || i_pos >= p_playlist->i_size )
    {
        int i_index;

        p_playlist->i_size++;
        p_playlist->pp_items = realloc( p_playlist->pp_items,
                                        p_playlist->i_size * sizeof(void*) );
        if( p_playlist->pp_items == NULL )
        {
            msg_Err( p_playlist, "out of memory" );
            free( p_item->psz_name );
            free( p_item );
            vlc_mutex_unlock( &p_playlist->object_lock );
            return -1;
        }

        /* Additional boundary checks */
        if( i_mode & PLAYLIST_APPEND )
        {
            i_pos++;
        }

        if( i_pos < 0 )
        {
            i_pos = 0;
        }
        else if( i_pos > p_playlist->i_size - 1 )
        {
            i_pos = p_playlist->i_size - 1;
        }

        /* Now we know exactly where it goes. Just renumber the playlist */
        for( i_index = p_playlist->i_size - 1; i_index > i_pos ; i_index-- )
        {
            p_playlist->pp_items[i_index] = p_playlist->pp_items[i_index - 1];
        }

        if( p_playlist->i_index >= i_pos )
        {
            p_playlist->i_index++;
        }
    }
    else
    {
        /* i_mode == PLAYLIST_REPLACE and 0 <= i_pos < p_playlist->i_size */
        free( p_playlist->pp_items[i_pos]->psz_name );
        free( p_playlist->pp_items[i_pos] );
        /* XXX: what if the item is still in use? */
    }

    p_playlist->pp_items[i_pos] = p_item;

    if( i_mode & PLAYLIST_GO )
    {
        p_playlist->i_index = i_pos;
        if( p_playlist->p_input )
        {
            input_StopThread( p_playlist->p_input );
        }
        p_playlist->i_status = PLAYLIST_RUNNING;
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    return 0;
}

/*****************************************************************************
 * playlist_Delete: delete an item from the playlist
 *****************************************************************************
 * Delete the item in the playlist with position i_pos.
 *****************************************************************************/
int playlist_Delete( playlist_t * p_playlist, int i_pos )
{
    int i_index;

    vlc_mutex_lock( &p_playlist->object_lock );

    if( i_pos >= 0 && i_pos < p_playlist->i_size )
    {
        msg_Warn( p_playlist, "deleting playlist item « %s »",
                              p_playlist->pp_items[i_pos]->psz_name );

        free( p_playlist->pp_items[i_pos]->psz_name );
        free( p_playlist->pp_items[i_pos] );
        /* XXX: what if the item is still in use? */

        if( i_pos < p_playlist->i_index )
        {
            p_playlist->i_index--;
        }

        /* Renumber the playlist */
        for( i_index = i_pos + 1; i_index < p_playlist->i_size; i_index++ )
        {
            p_playlist->pp_items[i_index - 1] = p_playlist->pp_items[i_index];
        }

        p_playlist->i_size--;
        if( p_playlist->i_size )
        {
            p_playlist->pp_items = realloc( p_playlist->pp_items,
                                        p_playlist->i_size * sizeof(void*) );
        }
        else
        {
            free( p_playlist->pp_items );
            p_playlist->pp_items = NULL;
        }
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    return 0;
}

/*****************************************************************************
 * playlist_Command: do a playlist action
 *****************************************************************************
 *
 *****************************************************************************/
void playlist_Command( playlist_t * p_playlist, int i_command, int i_arg )
{
    vlc_mutex_lock( &p_playlist->object_lock );

    switch( i_command )
    {
    case PLAYLIST_STOP:
        p_playlist->i_status = PLAYLIST_STOPPED;
        if( p_playlist->p_input )
        {
            input_StopThread( p_playlist->p_input );
        }
        break;

    case PLAYLIST_PLAY:
        p_playlist->i_status = PLAYLIST_RUNNING;
        break;

    case PLAYLIST_SKIP:
        p_playlist->i_status = PLAYLIST_STOPPED;
        SkipItem( p_playlist, i_arg );
        if( p_playlist->p_input )
        {
            input_StopThread( p_playlist->p_input );
        }
        p_playlist->i_status = PLAYLIST_RUNNING;
        break;

    case PLAYLIST_GOTO:
        if( i_arg >= 0 && i_arg < p_playlist->i_size )
        {
            p_playlist->i_index = i_arg;
            if( p_playlist->p_input )
            {
                input_StopThread( p_playlist->p_input );
            }
            p_playlist->i_status = PLAYLIST_RUNNING;
        }
        break;

    default:
        msg_Err( p_playlist, "unknown playlist command" );
        break;
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    return;
}

/* Following functions are local */

/*****************************************************************************
 * RunThread: main playlist thread
 *****************************************************************************/
static void RunThread ( playlist_t *p_playlist )
{
    /* Tell above that we're ready */
    vlc_thread_ready( p_playlist );

    while( !p_playlist->b_die )
    {
        vlc_mutex_lock( &p_playlist->object_lock );

        /* If there is an input, check that it doesn't need to die. */
        if( p_playlist->p_input )
        {
            /* This input is dead. Remove it ! */
            if( p_playlist->p_input->b_dead )
            {
                input_thread_t *p_input;

                /* Unlink current input */
                p_input = p_playlist->p_input;
                p_playlist->p_input = NULL;
                vlc_object_detach_all( p_input );

                /* Release the playlist lock, because we may get stuck
                 * in input_DestroyThread() for some time. */
                vlc_mutex_unlock( &p_playlist->object_lock );

                /* Destroy input */
                input_DestroyThread( p_input );
                vlc_object_destroy( p_input );
                continue;
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
                /* Select the next playlist item */
                SkipItem( p_playlist, 1 );

                /* Release the playlist lock, because we may get stuck
                 * in input_StopThread() for some time. */
                vlc_mutex_unlock( &p_playlist->object_lock );
                input_StopThread( p_playlist->p_input );
                continue;
            }
        }
        else if( p_playlist->i_status != PLAYLIST_STOPPED )
        {
            PlayItem( p_playlist );
        }

        vlc_mutex_unlock( &p_playlist->object_lock );

        msleep( INTF_IDLE_SLEEP );
    }

    /* If there is an input, kill it */
    while( 1 )
    {
        vlc_mutex_lock( &p_playlist->object_lock );

        if( p_playlist->p_input == NULL )
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
            break;
        }

        if( p_playlist->p_input->b_dead )
        {
            input_thread_t *p_input;

            /* Unlink current input */
            p_input = p_playlist->p_input;
            p_playlist->p_input = NULL;
            vlc_object_detach_all( p_input );
            vlc_mutex_unlock( &p_playlist->object_lock );

            /* Destroy input */
            input_DestroyThread( p_input );
            vlc_object_destroy( p_input );
            continue;
        }
        else if( p_playlist->p_input->b_die )
        {
            /* This input is dying, leave him alone */
            ;
        }
        else if( p_playlist->p_input->b_error || p_playlist->p_input->b_eof )
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
            input_StopThread( p_playlist->p_input );
            continue;
        }
        else
        {
            p_playlist->p_input->b_eof = 1;
        }

        vlc_mutex_unlock( &p_playlist->object_lock );

        msleep( INTF_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * SkipItem: go to Xth playlist item
 *****************************************************************************
 * This function calculates the position of the next playlist item, depending
 * on the playlist course mode (forward, backward, random...).
 *****************************************************************************/
static void SkipItem( playlist_t *p_playlist, int i_arg )
{
    int i_oldindex = p_playlist->i_index;

    /* If the playlist is empty, there is no current item */
    if( p_playlist->i_size == 0 )
    {
        p_playlist->i_index = -1;
        return;
    }

    /* Increment */
    p_playlist->i_index += i_arg;

    /* Boundary check */
    if( p_playlist->i_index >= p_playlist->i_size )
    {
        if( p_playlist->i_status == PLAYLIST_STOPPED
             || config_GetInt( p_playlist, "loop" ) )
        {
            p_playlist->i_index = 0;
        }
        else
        {
            /* Don't loop by default: stop at playlist end */
            p_playlist->i_index = i_oldindex;
            p_playlist->i_status = PLAYLIST_STOPPED;
        }
    }
    else if( p_playlist->i_index < 0 )
    {
        p_playlist->i_index = p_playlist->i_size - 1;
    }
}

/*****************************************************************************
 * PlayItem: play current playlist item
 *****************************************************************************
 * This function calculates the position of the next playlist item, depending
 * on the playlist course mode (forward, backward, random...).
 *****************************************************************************/
static void PlayItem( playlist_t *p_playlist )
{
    if( p_playlist->i_index == -1 )
    {
        if( p_playlist->i_size == 0 )
        {
            return;
        }

        SkipItem( p_playlist, 1 );
    }

    msg_Dbg( p_playlist, "creating new input thread" );
    p_playlist->p_input = input_CreateThread( p_playlist,
                p_playlist->pp_items[p_playlist->i_index], NULL );
}

/*****************************************************************************
 * Poubellize: put an input thread in the trashcan
 *****************************************************************************
 * XXX: unused
 *****************************************************************************/
static void Poubellize ( playlist_t *p_playlist, input_thread_t *p_input )
{
    
}

