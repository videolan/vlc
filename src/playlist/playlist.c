/*****************************************************************************
 * playlist.c : Playlist management functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: playlist.c,v 1.70 2003/12/13 17:16:11 gbazin Exp $
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

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>
#include <vlc/input.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "vlc_playlist.h"

#define PLAYLIST_FILE_HEADER_0_5  "# vlc playlist file version 0.5"
#define PLAYLIST_FILE_HEADER_0_6  "# vlc playlist file version 0.6"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunThread ( playlist_t * );
static void SkipItem  ( playlist_t *, int );
static void PlayItem  ( playlist_t * );

#if 0
static void Poubellize ( playlist_t *, input_thread_t * );
#endif

/**
 * Create playlist
 *
 * Create a playlist structure.
 * \param p_parent the vlc object that is to be the parent of this playlist
 * \return a pointer to the created playlist, or NULL on error
 */
playlist_t * __playlist_Create ( vlc_object_t *p_parent )
{
    playlist_t *p_playlist;
    vlc_value_t     val;

    /* Allocate structure */
    p_playlist = vlc_object_create( p_parent, VLC_OBJECT_PLAYLIST );
    if( !p_playlist )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    var_Create( p_playlist, "intf-change", VLC_VAR_BOOL );
    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    var_Create( p_playlist, "intf-popupmenu", VLC_VAR_BOOL );

    var_Create( p_playlist, "intf-show", VLC_VAR_BOOL );
    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-show", val );

    var_Create( p_playlist, "random", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "repeat", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "loop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    p_playlist->p_input = NULL;
    p_playlist->i_status = PLAYLIST_STOPPED;
    p_playlist->i_index = -1;
    p_playlist->i_size = 0;
    p_playlist->pp_items = NULL;

    p_playlist->i_groups = 0;
    p_playlist->pp_groups = NULL;
    p_playlist->i_max_id = 0;

    playlist_CreateGroup( p_playlist, "Normal" );

    if( vlc_thread_create( p_playlist, "playlist", RunThread,
                           VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        msg_Err( p_playlist, "cannot spawn playlist thread" );
        vlc_object_destroy( p_playlist );
        return NULL;
    }

    /* The object has been initialized, now attach it */
    vlc_object_attach( p_playlist, p_parent );

    return p_playlist;
}

/**
 * Destroy the playlist.
 *
 * Delete all items in the playlist and free the playlist structure.
 * \param p_playlist the playlist structure to destroy
 */
void playlist_Destroy( playlist_t * p_playlist )
{
    p_playlist->b_die = 1;

    vlc_thread_join( p_playlist );

    var_Destroy( p_playlist, "intf-change" );

    while( p_playlist->i_groups > 0 )
    {
        playlist_DeleteGroup( p_playlist, p_playlist->pp_groups[0]->i_id );
    }

    while( p_playlist->i_size > 0 )
    {
        playlist_Delete( p_playlist, 0 );
    }

    vlc_object_destroy( p_playlist );
}


/**
 * Do a playlist action
 *
 * \param p_playlist the playlist to do the command on
 * \param i_command the command to do
 * \param i_arg the argument to the command. See playlist_command_t for details
 */
 void playlist_Command( playlist_t * p_playlist, playlist_command_t i_command,
                       int i_arg )
{
    vlc_value_t val;

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
        if( !p_playlist->p_input && p_playlist->i_enabled != 0 )
        {
            PlayItem( p_playlist );
        }
        if( p_playlist->p_input )
        {
            val.i_int = PLAYING_S;
            var_Set( p_playlist->p_input, "state", val );
        }
        break;

    case PLAYLIST_PAUSE:
        val.i_int = 0;
        if( p_playlist->p_input )
            var_Get( p_playlist->p_input, "state", &val );

        if( val.i_int == PAUSE_S )
        {
            p_playlist->i_status = PLAYLIST_RUNNING;
            if( p_playlist->p_input )
            {
                val.i_int = PLAYING_S;
                var_Set( p_playlist->p_input, "state", val );
            }
        }
        else
        {
            p_playlist->i_status = PLAYLIST_PAUSED;
            if( p_playlist->p_input )
            {
                val.i_int = PAUSE_S;
                var_Set( p_playlist->p_input, "state", val );
            }
        }
        break;

    case PLAYLIST_SKIP:
        p_playlist->i_status = PLAYLIST_STOPPED;
        if( p_playlist->i_enabled == 0)
        {
            break;
        }
        SkipItem( p_playlist, i_arg );
        if( p_playlist->p_input )
        {
            input_StopThread( p_playlist->p_input );
        }
        p_playlist->i_status = PLAYLIST_RUNNING;
        break;

    case PLAYLIST_GOTO:
        if( i_arg >= 0 && i_arg < p_playlist->i_size &&
            p_playlist->i_enabled != 0 )
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

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return;
}
/* Following functions are local */

static void ObjectGarbageCollector( playlist_t *p_playlist,
                                    int i_type,
                                    mtime_t *pi_obj_destroyed_date )
{
    vlc_object_t *p_obj;
    if( *pi_obj_destroyed_date > mdate() )
    {
        return;
    }

    if( *pi_obj_destroyed_date == 0 )
    {
        /* give a little time */
        *pi_obj_destroyed_date = mdate() + I64C(300000);
    }
    else
    {
        while( ( p_obj = vlc_object_find( p_playlist,
                                           i_type,
                                           FIND_CHILD ) ) )
        {
            if( p_obj->p_parent != (vlc_object_t*)p_playlist )
            {
                /* only first chiled (ie unused) */
                vlc_object_release( p_obj );
                break;
            }
            if( i_type == VLC_OBJECT_VOUT )
            {
                msg_Dbg( p_playlist, "vout garbage collector destroying 1 vout" );
                vlc_object_detach( p_obj );
                vlc_object_release( p_obj );
                vout_Destroy( (vout_thread_t *)p_obj );
            }
            else if( i_type == VLC_OBJECT_SOUT )
            {
                vlc_object_release( p_obj );
                sout_DeleteInstance( (sout_instance_t*)p_obj );
            }
        }
        *pi_obj_destroyed_date = 0;
    }
}

/*****************************************************************************
 * RunThread: main playlist thread
 *****************************************************************************/
static void RunThread ( playlist_t *p_playlist )
{
    vlc_object_t *p_obj;
    vlc_value_t val;

    mtime_t    i_vout_destroyed_date = 0;
    mtime_t    i_sout_destroyed_date = 0;

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

                p_input = p_playlist->p_input;
                p_playlist->p_input = NULL;

                /* Release the playlist lock, because we may get stuck
                 * in input_DestroyThread() for some time. */
                vlc_mutex_unlock( &p_playlist->object_lock );

                /* Destroy input */
                input_DestroyThread( p_input );

                /* Unlink current input
                 * (_after_ input_DestroyThread for vout garbage collector) */
                vlc_object_detach( p_input );

                /* Destroy object */
                vlc_object_destroy( p_input );

                i_vout_destroyed_date = 0;
                i_sout_destroyed_date = 0;
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
                /* Check for autodeletion */
                if( p_playlist->pp_items[p_playlist->i_index]->b_autodeletion )
                {
                    vlc_mutex_unlock( &p_playlist->object_lock );
                    playlist_Delete( p_playlist, p_playlist->i_index );
                }
                else
                {
                    /* Select the next playlist item */
                    SkipItem( p_playlist, 1 );
                    input_StopThread( p_playlist->p_input );
                    vlc_mutex_unlock( &p_playlist->object_lock );
                }

                val.b_bool = VLC_TRUE;
                var_Set( p_playlist, "intf-change", val );
                continue;
            }
            else if( p_playlist->p_input->stream.control.i_status != INIT_S )
            {
                vlc_mutex_unlock( &p_playlist->object_lock );
                ObjectGarbageCollector( p_playlist, VLC_OBJECT_VOUT,
                                        &i_vout_destroyed_date );
                ObjectGarbageCollector( p_playlist, VLC_OBJECT_SOUT,
                                        &i_sout_destroyed_date );
                vlc_mutex_lock( &p_playlist->object_lock );
            }
        }
        else if( p_playlist->i_status != PLAYLIST_STOPPED )
        {
            SkipItem( p_playlist, 0 );
            PlayItem( p_playlist );
        }
        else if( p_playlist->i_status == PLAYLIST_STOPPED )
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
            ObjectGarbageCollector( p_playlist, VLC_OBJECT_SOUT,
                                    &i_sout_destroyed_date );
            ObjectGarbageCollector( p_playlist, VLC_OBJECT_VOUT,
                                    &i_vout_destroyed_date );
            vlc_mutex_lock( &p_playlist->object_lock );
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
            vlc_mutex_unlock( &p_playlist->object_lock );

            /* Destroy input */
            input_DestroyThread( p_input );
            /* Unlink current input (_after_ input_DestroyThread for vout
             * garbage collector)*/
            vlc_object_detach( p_input );

            /* Destroy object */
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
            input_StopThread( p_playlist->p_input );
            vlc_mutex_unlock( &p_playlist->object_lock );
            continue;
        }
        else
        {
            p_playlist->p_input->b_eof = 1;
        }

        vlc_mutex_unlock( &p_playlist->object_lock );

        msleep( INTF_IDLE_SLEEP );
    }

    /* close all remaining sout */
    while( ( p_obj = vlc_object_find( p_playlist,
                                      VLC_OBJECT_SOUT, FIND_CHILD ) ) )
    {
        vlc_object_release( p_obj );
        sout_DeleteInstance( (sout_instance_t*)p_obj );
    }

    /* close all remaining vout */
    while( ( p_obj = vlc_object_find( p_playlist,
                                      VLC_OBJECT_VOUT, FIND_CHILD ) ) )
    {
        vlc_object_detach( p_obj );
        vlc_object_release( p_obj );
        vout_Destroy( (vout_thread_t *)p_obj );
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
    vlc_bool_t b_random, b_repeat, b_loop;
    vlc_value_t val;

    /* If the playlist is empty, there is no current item */
    if( p_playlist->i_size == 0 )
    {
        p_playlist->i_index = -1;
        return;
    }

    var_Get( p_playlist, "random", &val );
    b_random = val.b_bool;
    var_Get( p_playlist, "repeat", &val );
    b_repeat = val.b_bool;
    var_Get( p_playlist, "loop", &val );
    b_loop = val.b_bool;

    /* Increment */
    if( b_random )
    {
        srand( (unsigned int)mdate() );

        /* Simple random stuff - we cheat a bit to minimize the chances to
         * get the same index again. */
        i_arg = (int)((float)p_playlist->i_size * rand() / (RAND_MAX+1.0));
        if( i_arg == 0 )
        {
            i_arg = (int)((float)p_playlist->i_size * rand() / (RAND_MAX+1.0));
        }
    }
    if( b_repeat )
    {
        i_arg = 0;
    }
    p_playlist->i_index += i_arg;

    /* Boundary check */
    if( p_playlist->i_index >= p_playlist->i_size )
    {
        if( p_playlist->i_status == PLAYLIST_STOPPED
             || b_random
             || b_loop )
        {
            p_playlist->i_index -= p_playlist->i_size
                         * ( p_playlist->i_index / p_playlist->i_size );
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

    /* Check that the item is enabled */
   if( p_playlist->pp_items[p_playlist->i_index]->b_enabled == VLC_FALSE &&
       p_playlist->i_enabled != 0)
   {
        SkipItem( p_playlist , 1 );
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
        if( p_playlist->i_size == 0 || p_playlist->i_enabled == 0)
        {
            return;
        }

        SkipItem( p_playlist, 1 );
    }

    if( p_playlist->i_enabled == 0)
    {
        return;
    }

    msg_Dbg( p_playlist, "creating new input thread" );
    p_playlist->p_input = input_CreateThread( p_playlist,
                                  p_playlist->pp_items[p_playlist->i_index] );
}
