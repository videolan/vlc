/*****************************************************************************
 * control.c : Hanle control of the playlist & running through it
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id: /local/vlc/0.8.6-playlist-vlm/src/playlist/playlist.c 13741 2006-03-21T19:29:39.792444Z zorglub  $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/input.h>
#include "vlc_playlist.h"

#define PLAYLIST_DEBUG 1

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int PlaylistVAControl( playlist_t * p_playlist, int i_query, va_list args );

void PreparseEnqueueItemSub( playlist_t *, playlist_item_t * );

playlist_item_t *playlist_RecursiveFindLast(playlist_t *p_playlist,
                                            playlist_item_t *p_node );

/*****************************************************************************
 * Playlist control
 *****************************************************************************/

/**
 * Do a playlist action. Should be entered without playlist lock
 * \see playlist_Control
 */
int playlist_LockControl( playlist_t * p_playlist, int i_query, ... )
{
    va_list args;
    int i_result;
    va_start( args, i_query );
    vlc_mutex_lock( &p_playlist->object_lock );
    i_result = PlaylistVAControl( p_playlist, i_query, args );
    va_end( args );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return i_result;
}

/**
 * Do a playlist action.
 * If there is something in the playlist then you can do playlist actions.
 * Should be entered with playlist lock. See include/vlc_playlist.h for
 * possible queries
 *
 * \param p_playlist the playlist to do the command on
 * \param i_query the command to do
 * \param variable number of arguments
 * \return VLC_SUCCESS or an error
 */
int playlist_Control( playlist_t * p_playlist, int i_query, ... )
{
    va_list args;
    int i_result;
    va_start( args, i_query );
    i_result = PlaylistVAControl( p_playlist, i_query, args );
    va_end( args );

    return i_result;
}

int PlaylistVAControl( playlist_t * p_playlist, int i_query, va_list args )
{
    int i_view;
    playlist_item_t *p_item, *p_node;
    vlc_value_t val;

    if( p_playlist->i_size <= 0 )
    {
        return VLC_EGENERIC;
    }

    switch( i_query )
    {
    case PLAYLIST_STOP:
        p_playlist->request.i_status = PLAYLIST_STOPPED;
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.p_item = NULL;
        break;

    case PLAYLIST_ITEMPLAY:
        p_item = (playlist_item_t *)va_arg( args, playlist_item_t * );
        if ( p_item == NULL || p_item->p_input->psz_uri == NULL )
            return VLC_EGENERIC;
        p_playlist->request.i_status = PLAYLIST_RUNNING;
        p_playlist->request.i_skip = 0;
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.p_item = p_item;
        p_playlist->request.p_node = p_playlist->status.p_node;
        break;

    case PLAYLIST_VIEWPLAY:
        i_view = (int) va_arg( args, playlist_item_t *);
        p_node = (playlist_item_t *)va_arg( args, playlist_item_t * );
        p_item = (playlist_item_t *)va_arg( args, playlist_item_t * );
        if ( p_node == NULL )
        {
            p_playlist->status.i_status = PLAYLIST_STOPPED;
            p_playlist->request.b_request = VLC_TRUE;
            msg_Err( p_playlist, "null node" );
            return VLC_SUCCESS;
        }
        p_playlist->request.i_status = PLAYLIST_RUNNING;
        p_playlist->request.i_skip = 0;
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.p_node = p_node;
        p_playlist->request.p_item = p_item;
        break;

    case PLAYLIST_PLAY:
        p_playlist->request.i_status = PLAYLIST_RUNNING;
        p_playlist->request.b_request = VLC_TRUE;

        if( p_playlist->p_input )
        {
            val.i_int = PLAYING_S;
            var_Set( p_playlist->p_input, "state", val );
            break;
        }
        p_playlist->request.p_node = p_playlist->status.p_node;
        p_playlist->request.p_item = p_playlist->status.p_item;
        p_playlist->request.i_skip = 0;
        break;

    case PLAYLIST_AUTOPLAY:
        p_playlist->status.i_status = PLAYLIST_RUNNING;
        p_playlist->status.p_node = p_playlist->p_local_category;
        p_playlist->request.b_request = VLC_FALSE;
        break;

    case PLAYLIST_PAUSE:
        val.i_int = 0;
        if( p_playlist->p_input )
            var_Get( p_playlist->p_input, "state", &val );

        if( val.i_int == PAUSE_S )
        {
            p_playlist->status.i_status = PLAYLIST_RUNNING;
            if( p_playlist->p_input )
            {
                val.i_int = PLAYING_S;
                var_Set( p_playlist->p_input, "state", val );
            }
        }
        else
        {
            p_playlist->status.i_status = PLAYLIST_PAUSED;
            if( p_playlist->p_input )
            {
                val.i_int = PAUSE_S;
                var_Set( p_playlist->p_input, "state", val );
            }
        }
        break;

    case PLAYLIST_SKIP:
        p_playlist->request.p_node = p_playlist->status.p_node;
        p_playlist->request.p_item = p_playlist->status.p_item;
        p_playlist->request.i_skip = (int) va_arg( args, int );
        p_playlist->request.b_request = VLC_TRUE;
        break;

    default:
        msg_Err( p_playlist, "unknown playlist query" );
        return VLC_EBADVAR;
        break;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Preparse control
 *****************************************************************************/
/** Enqueue an item for preparsing */
int playlist_PreparseEnqueue( playlist_t *p_playlist,
                              input_item_t *p_item )
{
    vlc_mutex_lock( &p_playlist->p_preparse->object_lock );
    vlc_gc_incref( p_item );
    INSERT_ELEM( p_playlist->p_preparse->pp_waiting,
                 p_playlist->p_preparse->i_waiting,
                 p_playlist->p_preparse->i_waiting,
                 p_item );
    vlc_mutex_unlock( &p_playlist->p_preparse->object_lock );
    return VLC_SUCCESS;
}

/** Enqueue a playlist item or a node for peparsing.
 *  This function should be entered without playlist and preparser locks */
int playlist_PreparseEnqueueItem( playlist_t *p_playlist,
                                  playlist_item_t *p_item )
{
    vlc_mutex_lock( &p_playlist->object_lock );
    vlc_mutex_lock( &p_playlist->p_preparse->object_lock );
    PreparseEnqueueItemSub( p_playlist, p_item );
    vlc_mutex_unlock( &p_playlist->p_preparse->object_lock );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return VLC_SUCCESS;
}

void PreparseEnqueueItemSub( playlist_t *p_playlist,
                             playlist_item_t *p_item )
{
    int i;
    if( p_item->i_children == -1 )
    {
        vlc_gc_incref( p_item );
        INSERT_ELEM( p_playlist->p_preparse->pp_waiting,
                     p_playlist->p_preparse->i_waiting,
                     p_playlist->p_preparse->i_waiting,
                     p_item->p_input );
    }
    else
    {
        for( i = 0; i < p_item->i_children; i++)
        {
            PreparseEnqueueItemSub( p_playlist,
                                             p_item->pp_children[i] );
        }
    }
}

/*****************************************************************************
 * Playback logic
 *****************************************************************************/

/** This function calculates the next playlist item, depending
 *  on the playlist course mode (forward, backward, random, view,...). */
playlist_item_t * playlist_NextItem( playlist_t *p_playlist )
{
    playlist_item_t *p_new = NULL;
    int i_skip,i;

    vlc_bool_t b_loop = var_GetBool( p_playlist, "loop" );
    vlc_bool_t b_random = var_GetBool( p_playlist, "random" );
    vlc_bool_t b_repeat = var_GetBool( p_playlist, "repeat" );
    vlc_bool_t b_playstop = var_GetBool( p_playlist, "play-and-stop" );

    /* Handle quickly a few special cases */

    /* No items to play */
    if( p_playlist->i_size == 0 )
    {
        msg_Info( p_playlist, "playlist is empty" );
        return NULL;
    }

    /* Repeat and play/stop */
    if( !p_playlist->request.b_request && b_repeat == VLC_TRUE &&
         p_playlist->status.p_item )
    {
        msg_Dbg( p_playlist,"repeating item" );
        return p_playlist->status.p_item;
    }
    if( !p_playlist->request.b_request && b_playstop == VLC_TRUE )
    {
        msg_Dbg( p_playlist,"stopping (play and stop)");
        return NULL;
    }

    if( !p_playlist->request.b_request && p_playlist->status.p_item &&
         p_playlist->status.p_item->i_flags & PLAYLIST_SKIP_FLAG )
    {
        msg_Dbg( p_playlist, "blocking item, stopping") ;
        return NULL;
    }

    /* Random case. This is an exception: if request, but request is skip +- 1
     * we don't go to next item but select a new random one. */
    if( b_random )
        msg_Err( p_playlist, "random unsupported" );
#if 0
            &&
        ( !p_playlist->request.b_request ||
        ( p_playlist->request.b_request && ( p_playlist->request.p_item == NULL ||
          p_playlist->request.i_skip == 1 || p_playlist->request.i_skip == -1 ) ) ) )
    {
        /* how many items to choose from ? */
        i_count = 0;
        for ( i = 0; i < p_playlist->i_size; i++ )
        {
            if ( p_playlist->pp_items[i]->p_input->i_nb_played == 0 )
                i_count++;
        }
        /* Nothing left? */
        if ( i_count == 0 )
        {
            /* Don't loop? Exit! */
            if( !b_loop )
                return NULL;
            /* Otherwise reset the counter */
            for ( i = 0; i < p_playlist->i_size; i++ )
            {
                p_playlist->pp_items[i]->p_input->i_nb_played = 0;
            }
            i_count = p_playlist->i_size;
        }
        srand( (unsigned int)mdate() );
        i = rand() % i_count + 1 ;
        /* loop thru the list and count down the unplayed items to the selected one */
        for ( i_new = 0; i_new < p_playlist->i_size && i > 0; i_new++ )
        {
            if ( p_playlist->pp_items[i_new]->p_input->i_nb_played == 0 )
                i--;
        }
        i_new--;

        p_playlist->request.i_skip = 0;
        p_playlist->request.b_request = VLC_FALSE;
        return p_playlist->pp_items[i_new];
    }
#endif

    /* Start the real work */
    if( p_playlist->request.b_request )
    {
#ifdef PLAYLIST_DEBUG
        msg_Dbg( p_playlist,"processing request" );
#endif
        p_new = p_playlist->request.p_item;
        i_skip = p_playlist->request.i_skip;

        p_playlist->status.p_node = p_playlist->request.p_node;

        /* If we are asked for a node, take its first item */
        if( i_skip == 0 &&
              ( p_new == NULL || p_new->i_children != -1 ) )
        {
            i_skip++;
        }

        if( i_skip > 0 )
        {
            for( i = i_skip; i > 0 ; i-- )
            {
                p_new = playlist_GetNextEnabledLeaf( p_playlist,
                                                     p_playlist->request.p_node,
                                                     p_new );
                if( p_new == NULL )
                {
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "looping - restarting at beginning "
                                         "of node" );
#endif
                    p_new = playlist_GetNextLeaf( p_playlist,
                                                  p_playlist->request.p_node,
                                                  NULL );
                    if( p_new == NULL ) break;
                }
            }
        }
        else if( i_skip < 0 )
        {
            for( i = i_skip; i < 0 ; i++ )
            {
                p_new = playlist_GetPrevLeaf( p_playlist,
                                              p_playlist->request.p_node,
                                              p_new );
                if( p_new == NULL )
                {
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "looping - restarting at end "
                                         "of node" );
#endif
                    /** \bug This is needed because GetPrevLeaf does not loop
                      * by itself */
                    p_new = playlist_GetLastLeaf( p_playlist,
                                                 p_playlist->request.p_node );
                }
                if( p_new == NULL ) break;
            }
        }
        /* Clear the request */
        p_playlist->request.b_request = VLC_FALSE;
    }
    /* "Automatic" item change ( next ) */
    else
    {
#ifdef PLAYLIST_DEBUG
        msg_Dbg( p_playlist,"changing item without a request" );
#endif
        /* Cant go to next from current item */
        if( p_playlist->status.p_item &&
            p_playlist->status.p_item->i_flags & PLAYLIST_SKIP_FLAG )
            return NULL;

        p_new = playlist_GetNextLeaf( p_playlist,
                                      p_playlist->status.p_node,
                                      p_playlist->status.p_item );
        if( p_new == NULL && b_loop )
        {
#ifdef PLAYLIST_DEBUG
            msg_Dbg( p_playlist, "looping" );
#endif
            p_new = playlist_GetNextLeaf( p_playlist,
                                          p_playlist->status.p_node,
                                          NULL );
        }
        /* The new item can't be autoselected  */
        if( p_new != NULL && p_new->i_flags & PLAYLIST_SKIP_FLAG )
            return NULL;
    }
    if( p_new == NULL )
    {
        msg_Dbg( p_playlist, "did not find something to play" );
    }
    return p_new;
}

/** Start the input for an item */
int playlist_PlayItem( playlist_t *p_playlist, playlist_item_t *p_item )
{
    vlc_value_t val;
    int i_activity = var_GetInteger( p_playlist, "activity") ;

    msg_Dbg( p_playlist, "creating new input thread" );

    p_item->p_input->i_nb_played++;
    p_playlist->status.p_item = p_item;

    p_playlist->status.i_status = PLAYLIST_RUNNING;

    var_SetInteger( p_playlist, "activity", i_activity +
                    DEFAULT_INPUT_ACTIVITY );
    p_playlist->p_input = input_CreateThread( p_playlist, p_item->p_input );

    val.i_int = p_item->p_input->i_id;
    /* unlock the playlist to set the var...mmm */
    vlc_mutex_unlock( &p_playlist->object_lock);
    var_Set( p_playlist, "playlist-current", val);
    vlc_mutex_lock( &p_playlist->object_lock);

    return VLC_SUCCESS;
}
