/*****************************************************************************
 * playlist.c : Playlist management functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: playlist.c,v 1.52 2003/09/15 00:01:49 fenrir Exp $
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

#include "stream_control.h"
#include "input_ext-intf.h"

#include "vlc_playlist.h"

#define PLAYLIST_FILE_HEADER_0_5  "# vlc playlist file version 0.5"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunThread ( playlist_t * );
static void SkipItem  ( playlist_t *, int );
static void PlayItem  ( playlist_t * );

static void Poubellize ( playlist_t *, input_thread_t * );

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

    p_playlist->p_input = NULL;
    p_playlist->i_status = PLAYLIST_STOPPED;
    p_playlist->i_index = -1;
    p_playlist->i_size = 0;
    p_playlist->pp_items = NULL;

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

    vlc_object_destroy( p_playlist );
}

/**
 * Add an MRL to the playlist. This is a simplified version of
 * playlist_AddExt inculded for convenince. It equals calling playlist_AddExt
 * with psz_name == psz_target and i_duration == -1
 */

int playlist_Add( playlist_t *p_playlist, const char *psz_target,
                  const char **ppsz_options, int i_options,
                  int i_mode, int i_pos )
{
    return playlist_AddExt( p_playlist, psz_target, psz_target, -1,
                            ppsz_options, i_options, i_mode, i_pos );
}

/**
 * Add a MRL into the playlist.
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_duration a hint about the duration of this item, in miliseconds, or
 *        -1 if unknown.
 * \param ppsz_options array of options
 * \param i_options number of items in ppsz_options
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \return always returns 0
*/
int playlist_AddExt( playlist_t *p_playlist, const char * psz_uri,
                     const char * psz_name, mtime_t i_duration,
                     const char **ppsz_options, int i_options, int i_mode,
                     int i_pos )
{
    playlist_item_t * p_item;

    p_item = malloc( sizeof( playlist_item_t ) );
    if( p_item == NULL )
    {
        msg_Err( p_playlist, "out of memory" );
    }

    p_item->psz_name = strdup( psz_name );
    p_item->psz_uri  = strdup( psz_uri );
    p_item->i_duration = i_duration;
    p_item->i_type = 0;
    p_item->i_status = 0;
    p_item->b_autodeletion = VLC_FALSE;

    p_item->ppsz_options = NULL;
    p_item->i_options = i_options;

    if( i_options )
    {
        int i;

        p_item->ppsz_options = (char **)malloc( i_options * sizeof(char *) );
        for( i = 0; i < i_options; i++ )
            p_item->ppsz_options[i] = strdup( ppsz_options[i] );

    }

    return playlist_AddItem( p_playlist, p_item, i_mode, i_pos );
}

/**
 * Add a playlist item into a playlist
 *
 * \param p_playlist the playlist to insert into
 * \param p_item the playlist item to insert
 * \param i_mode the mode used when adding
 * \param i_pos the possition in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \return always returns 0
*/
int playlist_AddItem( playlist_t *p_playlist, playlist_item_t * p_item,
                int i_mode, int i_pos)
{
    vlc_value_t     val;

    vlc_mutex_lock( &p_playlist->object_lock );

    /*
     * CHECK_INSERT : checks if the item is already enqued before
     * enqueing it
     */
    if ( i_mode & PLAYLIST_CHECK_INSERT )
    {
         int j;

         if ( p_playlist->pp_items )
         {
             for ( j = 0; j < p_playlist->i_size; j++ )
             {
                 if ( !strcmp( p_playlist->pp_items[j]->psz_uri, p_item->psz_uri ) )
                 {
                      if( p_item->psz_name )
                      {
                          free( p_item->psz_name );
                      }
                      if( p_item->psz_uri )
                      {
                          free( p_item->psz_uri );
                      }
                      free( p_item );
                      vlc_mutex_unlock( &p_playlist->object_lock );
                      return 0;
                 }
             }
         }
         i_mode &= ~PLAYLIST_CHECK_INSERT;
         i_mode |= PLAYLIST_APPEND;
    }


    msg_Dbg( p_playlist, "adding playlist item « %s » ( %s )", p_item->psz_name, p_item->psz_uri);

    /* Create the new playlist item */


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
        /* Additional boundary checks */
        if( i_mode & PLAYLIST_APPEND )
        {
            i_pos++;
        }

        if( i_pos < 0 )
        {
            i_pos = 0;
        }
        else if( i_pos > p_playlist->i_size )
        {
            i_pos = p_playlist->i_size;
        }

        INSERT_ELEM( p_playlist->pp_items,
                     p_playlist->i_size,
                     i_pos,
                     p_item );

        if( p_playlist->i_index >= i_pos )
        {
            p_playlist->i_index++;
        }
    }
    else
    {
        /* i_mode == PLAYLIST_REPLACE and 0 <= i_pos < p_playlist->i_size */
        if( p_playlist->pp_items[i_pos]->psz_name )
        {
            free( p_playlist->pp_items[i_pos]->psz_name );
        }
        if( p_playlist->pp_items[i_pos]->psz_uri )
        {
            free( p_playlist->pp_items[i_pos]->psz_uri );
        }
        /* XXX: what if the item is still in use? */
        free( p_playlist->pp_items[i_pos] );
        p_playlist->pp_items[i_pos] = p_item;
    }

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

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return 0;
}

/**
 * delete an item from a playlist.
 *
 * \param p_playlist the playlist to remove from.
 * \param i_pos the position of the item to remove
 * \return returns 0
 */
int playlist_Delete( playlist_t * p_playlist, int i_pos )
{
    vlc_value_t     val;
    vlc_mutex_lock( &p_playlist->object_lock );

    if( i_pos >= 0 && i_pos < p_playlist->i_size )
    {
        msg_Dbg( p_playlist, "deleting playlist item « %s »",
                             p_playlist->pp_items[i_pos]->psz_name );

        if( p_playlist->pp_items[i_pos]->psz_name )
        {
            free( p_playlist->pp_items[i_pos]->psz_name );
        }
        if( p_playlist->pp_items[i_pos]->psz_uri )
        {
            free( p_playlist->pp_items[i_pos]->psz_uri );
        }
        if( p_playlist->pp_items[i_pos]->i_options )
        {
            int i;

            for( i = 0; i < p_playlist->pp_items[i_pos]->i_options; i++ )
                free( p_playlist->pp_items[i_pos]->ppsz_options[i] );

            free( p_playlist->pp_items[i_pos]->ppsz_options );
        }

        /* XXX: what if the item is still in use? */
        free( p_playlist->pp_items[i_pos] );

        if( i_pos <= p_playlist->i_index )
        {
            p_playlist->i_index--;
        }

        /* Renumber the playlist */
        REMOVE_ELEM( p_playlist->pp_items,
                     p_playlist->i_size,
                     i_pos );
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return 0;
}


/**
 * Sort the playlist
 *
 */
int playlist_Sort( playlist_t * p_playlist , int i_type )
{
    int i , i_small , i_position;
    playlist_item_t *p_temp;

    vlc_mutex_lock( &p_playlist->object_lock );

    for( i_position = 0; i_position < p_playlist->i_size -1 ; i_position ++ )
    {
        i_small  = i_position;
        for( i = i_position + 1 ; i<  p_playlist->i_size ; i++)
        {
            int i_test;

            i_test = strcasecmp( p_playlist->pp_items[i]->psz_name,
                                 p_playlist->pp_items[i_small]->psz_name );

            if( i_type == SORT_NORMAL  && i_test < 0 ||
                i_type == SORT_REVERSE && i_test > 0 )
            {
                i_small = i;
            }

            p_temp = p_playlist->pp_items[i_position];
            p_playlist->pp_items[i_position] = p_playlist->pp_items[i_small];
            p_playlist->pp_items[i_small] = p_temp;
        }
    }

    for( i=0; i < p_playlist->i_size; i++ )
    {
        msg_Dbg( p_playlist, "%s", p_playlist->pp_items[i]->psz_name );
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    return 0;
}

/**
 * Move an item in a playlist
 *
 * Move the item in the playlist with position i_pos before the current item
 * at position i_newpos.
 * \param p_playlist the playlist to move items in
 * \param i_pos the position of the item to move
 * \param i_newpos the position of the item that will be behind the moved item
 *        after the move
 * \return returns 0
 */
int playlist_Move( playlist_t * p_playlist, int i_pos, int i_newpos)
{
    vlc_value_t     val;
    vlc_mutex_lock( &p_playlist->object_lock );

    /* take into account that our own row disappears. */
    if ( i_pos < i_newpos ) i_newpos--;

    if( i_pos >= 0 && i_newpos >=0 && i_pos <= p_playlist->i_size 
                     && i_newpos <= p_playlist->i_size )
    {
        playlist_item_t * temp;

        msg_Dbg( p_playlist, "moving playlist item « %s »",
                             p_playlist->pp_items[i_pos]->psz_name );

        if( i_pos == p_playlist->i_index )
        {
            p_playlist->i_index = i_newpos;
        }
        else if( i_pos > p_playlist->i_index && i_newpos <= p_playlist->i_index )
        {
            p_playlist->i_index++;
        }
        else if( i_pos < p_playlist->i_index && i_newpos >= p_playlist->i_index )
        {
            p_playlist->i_index--;
        }

        if ( i_pos < i_newpos )
        {
            temp = p_playlist->pp_items[i_pos];
            while ( i_pos < i_newpos )
            {
                p_playlist->pp_items[i_pos] = p_playlist->pp_items[i_pos+1];
                i_pos++;
            }
            p_playlist->pp_items[i_newpos] = temp;
        }
        else if ( i_pos > i_newpos )
        {
            temp = p_playlist->pp_items[i_pos];
            while ( i_pos > i_newpos )
            {
                p_playlist->pp_items[i_pos] = p_playlist->pp_items[i_pos-1];
                i_pos--;
            }
            p_playlist->pp_items[i_newpos] = temp;
        }
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return 0;
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
        if( !p_playlist->p_input )
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
        p_playlist->i_status = PLAYLIST_PAUSED;
        if( p_playlist->p_input )
        {
            val.i_int = PAUSE_S;
            var_Set( p_playlist->p_input, "state", val );
        }
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

static void ObjectGarbageCollector( playlist_t *p_playlist,
                                    int i_type,
                                    vlc_bool_t *pb_obj_destroyed,
                                    mtime_t *pi_obj_destroyed_date )
{
    vlc_object_t *p_obj;
    if( *pb_obj_destroyed || *pi_obj_destroyed_date > mdate() )
    {
        return;
    }

    if( *pi_obj_destroyed_date == 0 )
    {
        /* give a little time */
        *pi_obj_destroyed_date = mdate() + 300000LL;
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
        *pb_obj_destroyed = VLC_TRUE;
    }
}

/*****************************************************************************
 * RunThread: main playlist thread
 *****************************************************************************/
static void RunThread ( playlist_t *p_playlist )
{
    vlc_object_t *p_obj;
    vlc_bool_t b_vout_destroyed = VLC_FALSE; /*we do vout garbage collector */
    mtime_t    i_vout_destroyed_date = 0;

    vlc_bool_t b_sout_destroyed = VLC_FALSE; /*we do vout garbage collector */
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

                /* Unlink current input (_after_ input_DestroyThread for vout garbage collector)*/
                vlc_object_detach( p_input );

                /* Destroy object */
                vlc_object_destroy( p_input );

                b_vout_destroyed = VLC_FALSE;
                i_vout_destroyed_date = 0;
                b_sout_destroyed = VLC_FALSE;
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
                    vlc_mutex_lock( &p_playlist->object_lock );
                }

                /* Select the next playlist item */
                SkipItem( p_playlist, 1 );

                input_StopThread( p_playlist->p_input );
                vlc_mutex_unlock( &p_playlist->object_lock );
                continue;
            }
            else if( p_playlist->p_input->stream.control.i_status != INIT_S )
            {
                ObjectGarbageCollector( p_playlist, VLC_OBJECT_VOUT,
                                        &b_vout_destroyed, &i_vout_destroyed_date );
                ObjectGarbageCollector( p_playlist, VLC_OBJECT_SOUT,
                                        &b_sout_destroyed, &i_sout_destroyed_date );
            }
        }
        else if( p_playlist->i_status != PLAYLIST_STOPPED )
        {
            SkipItem( p_playlist, 0 );
            PlayItem( p_playlist );
        }
        else if( p_playlist->i_status == PLAYLIST_STOPPED )
        {
            ObjectGarbageCollector( p_playlist, VLC_OBJECT_SOUT,
                                    &b_sout_destroyed, &i_sout_destroyed_date );
            ObjectGarbageCollector( p_playlist, VLC_OBJECT_VOUT,
                                    &b_vout_destroyed, &i_vout_destroyed_date );
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
    vlc_bool_t b_random;
    vlc_value_t val;

    /* If the playlist is empty, there is no current item */
    if( p_playlist->i_size == 0 )
    {
        p_playlist->i_index = -1;
        return;
    }

    b_random = config_GetInt( p_playlist, "random" );

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

    p_playlist->i_index += i_arg;

    /* Boundary check */
    if( p_playlist->i_index >= p_playlist->i_size )
    {
        if( p_playlist->i_status == PLAYLIST_STOPPED
             || b_random
             || config_GetInt( p_playlist, "loop" ) )
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

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );
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
                                  p_playlist->pp_items[p_playlist->i_index] );
}

/*****************************************************************************
 * Poubellize: put an input thread in the trashcan
 *****************************************************************************
 * XXX: unused
 *****************************************************************************/
static void Poubellize ( playlist_t *p_playlist, input_thread_t *p_input )
{
    msg_Dbg( p_playlist, "poubellizing input %i\n", p_input->i_object_id );
}

/*****************************************************************************
 * playlist_LoadFile: load a playlist file.
 ****************************************************************************/
int playlist_LoadFile( playlist_t * p_playlist, const char *psz_filename )
{
    FILE *file;
    char line[1024];
    int i_current_status;
    int i;

    msg_Dbg( p_playlist, "opening playlist file %s", psz_filename );

    file = fopen( psz_filename, "rt" );
    if( !file )
    {
        msg_Err( p_playlist, "playlist file %s does not exist", psz_filename );
        return -1;
    }
    fseek( file, 0L, SEEK_SET );

    /* check the file is not empty */
    if ( ! fgets( line, 1024, file ) )
    {
        msg_Err( p_playlist, "playlist file %s is empty", psz_filename );
        fclose( file );
        return -1;
    }

    /* get rid of line feed */
    if( line[strlen(line)-1] == '\n' || line[strlen(line)-1] == '\r' )
    {
       line[strlen(line)-1] = (char)0;
       if( line[strlen(line)-1] == '\r' ) line[strlen(line)-1] = (char)0;
    }
    /* check the file format is valid */
    if ( strcmp ( line , PLAYLIST_FILE_HEADER_0_5 ) )
    {
        msg_Err( p_playlist, "playlist file %s format is unsupported"
                , psz_filename );
        fclose( file );
        return -1;
    }

    /* stop playing */
    i_current_status = p_playlist->i_status;
    if ( p_playlist->i_status != PLAYLIST_STOPPED )
    {
        playlist_Stop ( p_playlist );
    }

    /* delete current content of the playlist */
    for( i = p_playlist->i_size - 1; i >= 0; i-- )
    {
        playlist_Delete ( p_playlist , i );
    }

    /* simply add each line */
    while( fgets( line, 1024, file ) )
    {
       /* ignore comments or empty lines */
       if( (line[0] == '#') || (line[0] == '\r') || (line[0] == '\n')
               || (line[0] == (char)0) )
           continue;

       /* get rid of line feed */
       if( line[strlen(line)-1] == '\n' || line[strlen(line)-1] == '\r' )
       {
           line[strlen(line)-1] = (char)0;
           if( line[strlen(line)-1] == '\r' ) line[strlen(line)-1] = (char)0;
       }

       playlist_Add ( p_playlist , (char *)&line ,
                      0, 0, PLAYLIST_APPEND , PLAYLIST_END );
    }

    /* start playing */
    if ( i_current_status != PLAYLIST_STOPPED )
    {
        playlist_Play ( p_playlist );
    }

    fclose( file );

    return 0;
}

/*****************************************************************************
 * playlist_SaveFile: Save a playlist in a file.
 *****************************************************************************/
int playlist_SaveFile( playlist_t * p_playlist, const char * psz_filename )
{
    FILE *file;
    int i;

    vlc_mutex_lock( &p_playlist->object_lock );

    msg_Dbg( p_playlist, "saving playlist file %s", psz_filename );

    file = fopen( psz_filename, "wt" );
    if( !file )
    {
        msg_Err( p_playlist , "could not create playlist file %s"
                , psz_filename );
        return -1;
    }

    fprintf( file , PLAYLIST_FILE_HEADER_0_5 "\n" );

    for ( i = 0 ; i < p_playlist->i_size ; i++ )
    {
        fprintf( file , p_playlist->pp_items[i]->psz_uri );
        fprintf( file , "\n" );
    }

    fclose( file );

    vlc_mutex_unlock( &p_playlist->object_lock );

    return 0;
}
