/*****************************************************************************
 * control.c : Handle control of the playlist & running through it
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include "vlc_playlist.h"
#include "playlist_internal.h"
#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int PlaylistVAControl( playlist_t * p_playlist, int i_query, va_list args );

static void PreparseEnqueueItemSub( playlist_t *, playlist_item_t * );

/*****************************************************************************
 * Playlist control
 *****************************************************************************/

playlist_t *__pl_Yield( vlc_object_t *p_this )
{
    playlist_t *pl = p_this->p_libvlc->p_playlist;
    assert( pl != NULL );
    vlc_object_yield( pl );
    return pl;
}

void __pl_Release( vlc_object_t *p_this )
{
    playlist_t *pl = p_this->p_libvlc->p_playlist;
    assert( pl != NULL );
    vlc_object_release( pl );
}

int playlist_Control( playlist_t * p_playlist, int i_query,
                      vlc_bool_t b_locked, ... )
{
    va_list args;
    int i_result;
    va_start( args, b_locked );
    if( !b_locked ) PL_LOCK;
    i_result = PlaylistVAControl( p_playlist, i_query, args );
    va_end( args );
    if( !b_locked ) PL_UNLOCK;

    return i_result;
}

static int PlaylistVAControl( playlist_t * p_playlist, int i_query, va_list args )
{
    playlist_item_t *p_item, *p_node;
    vlc_value_t val;

    if( playlist_IsEmpty( p_playlist ) )
        return VLC_EGENERIC;

    switch( i_query )
    {
    case PLAYLIST_STOP:
        p_playlist->request.i_status = PLAYLIST_STOPPED;
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.p_item = NULL;
        break;

    // Node can be null, it will keep the same. Use with care ...
    // Item null = take the first child of node
    case PLAYLIST_VIEWPLAY:
        p_node = (playlist_item_t *)va_arg( args, playlist_item_t * );
        p_item = (playlist_item_t *)va_arg( args, playlist_item_t * );
        if ( p_node == NULL )
        {
            p_node = p_playlist->status.p_node;
            assert( p_node );
        }
        p_playlist->request.i_status = PLAYLIST_RUNNING;
        p_playlist->request.i_skip = 0;
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.p_node = p_node;
        p_playlist->request.p_item = p_item;
        if( p_item && var_GetBool( p_playlist, "random" ) )
            p_playlist->b_reset_currently_playing = VLC_TRUE;
        break;

    case PLAYLIST_PLAY:
        if( p_playlist->p_input )
        {
            val.i_int = PLAYING_S;
            var_Set( p_playlist->p_input, "state", val );
            break;
        }
        else
        {
            p_playlist->request.i_status = PLAYLIST_RUNNING;
            p_playlist->request.b_request = VLC_TRUE;
            p_playlist->request.p_node = p_playlist->status.p_node;
            p_playlist->request.p_item = p_playlist->status.p_item;
            p_playlist->request.i_skip = 0;
        }
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
        /* if already running, keep running */
        if( p_playlist->status.i_status != PLAYLIST_STOPPED )
            p_playlist->request.i_status = p_playlist->status.i_status;
        p_playlist->request.b_request = VLC_TRUE;
        break;

    default:
        msg_Err( p_playlist, "unknown playlist query" );
        return VLC_EBADVAR;
        break;
    }
    vlc_cond_signal( &p_playlist->object_wait );

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
    vlc_cond_signal( &p_playlist->p_preparse->object_wait );
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

int playlist_AskForArtEnqueue( playlist_t *p_playlist,
                               input_item_t *p_item )
{
    int i;
    preparse_item_t p;
    p.p_item = p_item;
    p.b_fetch_art = VLC_TRUE;

    vlc_object_lock( p_playlist->p_fetcher );
    for( i = 0; i < p_playlist->p_fetcher->i_waiting &&
         p_playlist->p_fetcher->p_waiting->b_fetch_art == VLC_TRUE;
         i++ );
    vlc_gc_incref( p_item );
    INSERT_ELEM( p_playlist->p_fetcher->p_waiting,
                 p_playlist->p_fetcher->i_waiting,
                 i, p );
    vlc_object_signal_unlocked( p_playlist->p_fetcher );
    vlc_object_unlock( p_playlist->p_fetcher );
    return VLC_SUCCESS;
}

static void PreparseEnqueueItemSub( playlist_t *p_playlist,
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
            PreparseEnqueueItemSub( p_playlist, p_item->pp_children[i] );
        }
    }
}

/*****************************************************************************
 * Playback logic
 *****************************************************************************/

/**
 * Synchronise the current index of the playlist
 * to match the index of the current item.
 *
 * \param p_playlist the playlist structure
 * \param p_cur the current playlist item
 * \return nothing
 */
static void ResyncCurrentIndex( playlist_t *p_playlist, playlist_item_t *p_cur )
{
     PL_DEBUG( "resyncing on %s", PLI_NAME( p_cur ) );
     /* Simply resync index */
     int i;
     p_playlist->i_current_index = -1;
     for( i = 0 ; i< p_playlist->current.i_size; i++ )
     {
          if( ARRAY_VAL( p_playlist->current, i ) == p_cur )
          {
              p_playlist->i_current_index = i;
              break;
          }
     }
     PL_DEBUG( "%s is at %i", PLI_NAME( p_cur ), p_playlist->i_current_index );
}

void ResetCurrentlyPlaying( playlist_t *p_playlist, vlc_bool_t b_random,
                            playlist_item_t *p_cur )
{
    playlist_item_t *p_next = NULL;
    stats_TimerStart( p_playlist, "Items array build",
                      STATS_TIMER_PLAYLIST_BUILD );
    PL_DEBUG( "rebuilding array of current - root %s",
              PLI_NAME( p_playlist->status.p_node ) );
    ARRAY_RESET( p_playlist->current );
    p_playlist->i_current_index = -1;
    while( 1 )
    {
        /** FIXME: this is *slow* */
        p_next = playlist_GetNextLeaf( p_playlist,
                                       p_playlist->status.p_node,
                                       p_next, VLC_TRUE, VLC_FALSE );
        if( p_next )
        {
            if( p_next == p_cur )
                p_playlist->i_current_index = p_playlist->current.i_size;
            ARRAY_APPEND( p_playlist->current, p_next);
        }
        else break;
    }
    PL_DEBUG("rebuild done - %i items, index %i", p_playlist->current.i_size,
                                                  p_playlist->i_current_index);
    if( b_random )
    {
        /* Shuffle the array */
        srand( (unsigned int)mdate() );
        int j;
        for( j = p_playlist->current.i_size - 1; j > 0; j-- )
        {
            int i = rand() % (j+1); /* between 0 and j */
            playlist_item_t *p_tmp;
            /* swap the two items */
            p_tmp = ARRAY_VAL(p_playlist->current, i);
            ARRAY_VAL(p_playlist->current,i) = ARRAY_VAL(p_playlist->current,j);
            ARRAY_VAL(p_playlist->current,j) = p_tmp;
        }
    }
    p_playlist->b_reset_currently_playing = VLC_FALSE;
    stats_TimerStop( p_playlist, STATS_TIMER_PLAYLIST_BUILD );
}

/**
 * Compute the next playlist item depending on
 * the playlist course mode (forward, backward, random, view,...).
 *
 * \param p_playlist the playlist object
 * \return nothing
 */
playlist_item_t * playlist_NextItem( playlist_t *p_playlist )
{
    playlist_item_t *p_new = NULL;
    int i_skip = 0, i;

    vlc_bool_t b_loop = var_GetBool( p_playlist, "loop" );
    vlc_bool_t b_random = var_GetBool( p_playlist, "random" );
    vlc_bool_t b_repeat = var_GetBool( p_playlist, "repeat" );
    vlc_bool_t b_playstop = var_GetBool( p_playlist, "play-and-stop" );

    /* Handle quickly a few special cases */
    /* No items to play */
    if( p_playlist->items.i_size == 0 )
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
        msg_Dbg( p_playlist,"stopping (play and stop)" );
        return NULL;
    }

    if( !p_playlist->request.b_request && p_playlist->status.p_item )
    {
        playlist_item_t *p_parent = p_playlist->status.p_item;
        while( p_parent )
        {
            if( p_parent->i_flags & PLAYLIST_SKIP_FLAG )
            {
                msg_Dbg( p_playlist, "blocking item, stopping") ;
                return NULL;
            }
            p_parent = p_parent->p_parent;
        }
    }

    /* Start the real work */
    if( p_playlist->request.b_request )
    {
        p_new = p_playlist->request.p_item;
        i_skip = p_playlist->request.i_skip;
        PL_DEBUG( "processing request item %s node %s skip %i",
                        PLI_NAME( p_playlist->request.p_item ),
                        PLI_NAME( p_playlist->request.p_node ), i_skip );

        if( p_playlist->request.p_node &&
            p_playlist->request.p_node != p_playlist->status.p_node )
        {
            p_playlist->status.p_node = p_playlist->request.p_node;
            p_playlist->b_reset_currently_playing = VLC_TRUE;
        }

        /* If we are asked for a node, don't take it */
        if( i_skip == 0 && ( p_new == NULL || p_new->i_children != -1 ) )
            i_skip++;

        if( p_playlist->b_reset_currently_playing )
            /* A bit too bad to reset twice ... */
            ResetCurrentlyPlaying( p_playlist, b_random, p_new );
        else if( p_new )
            ResyncCurrentIndex( p_playlist, p_new );
        else
            p_playlist->i_current_index = -1;

        if( p_playlist->current.i_size && (i_skip > 0) )
        {
            if( p_playlist->i_current_index < -1 )
                p_playlist->i_current_index = -1;
            for( i = i_skip; i > 0 ; i-- )
            {
                p_playlist->i_current_index++;
                if( p_playlist->i_current_index >= p_playlist->current.i_size )
                {
                    PL_DEBUG( "looping - restarting at beginning of node" );
                    p_playlist->i_current_index = 0;
                }
            }
            p_new = ARRAY_VAL( p_playlist->current,
                               p_playlist->i_current_index );
        }
        else if( p_playlist->current.i_size && (i_skip < 0) )
        {
            for( i = i_skip; i < 0 ; i++ )
            {
                p_playlist->i_current_index--;
                if( p_playlist->i_current_index <= -1 )
                {
                    PL_DEBUG( "looping - restarting at end of node" );
                    p_playlist->i_current_index = p_playlist->current.i_size-1;
                }
            }
            p_new = ARRAY_VAL( p_playlist->current,
                               p_playlist->i_current_index );
        }
        /* Clear the request */
        p_playlist->request.b_request = VLC_FALSE;
    }
    /* "Automatic" item change ( next ) */
    else
    {
        PL_DEBUG( "changing item without a request (current %i/%i)",
                  p_playlist->i_current_index, p_playlist->current.i_size );
        /* Cant go to next from current item */
        if( p_playlist->status.p_item &&
            p_playlist->status.p_item->i_flags & PLAYLIST_SKIP_FLAG )
            return NULL;

        if( p_playlist->b_reset_currently_playing )
            ResetCurrentlyPlaying( p_playlist, b_random,
                                   p_playlist->status.p_item );

        p_playlist->i_current_index++;
        if( p_playlist->i_current_index == p_playlist->current.i_size )
        {
            if( !b_loop || p_playlist->current.i_size == 0 ) return NULL;
            p_playlist->i_current_index = 0;
        }
        PL_DEBUG( "using item %i", p_playlist->i_current_index );
        if ( p_playlist->current.i_size == 0 ) return NULL;

        p_new = ARRAY_VAL( p_playlist->current, p_playlist->i_current_index );
        /* The new item can't be autoselected  */
        if( p_new != NULL && p_new->i_flags & PLAYLIST_SKIP_FLAG )
            return NULL;
    }
    return p_new;
}

/**
 * Start the input for an item
 *
 * \param p_playlist the playlist objetc
 * \param p_item the item to play
 * \return nothing
 */
int playlist_PlayItem( playlist_t *p_playlist, playlist_item_t *p_item )
{
    vlc_value_t val;
    input_item_t *p_input = p_item->p_input;
    int i_activity = var_GetInteger( p_playlist, "activity" ) ;

    msg_Dbg( p_playlist, "creating new input thread" );

    p_input->i_nb_played++;
    p_playlist->status.p_item = p_item;

    p_playlist->status.i_status = PLAYLIST_RUNNING;

    var_SetInteger( p_playlist, "activity", i_activity +
                    DEFAULT_INPUT_ACTIVITY );
    p_playlist->p_input = input_CreateThread( p_playlist, p_input );

    char *psz_uri = input_item_GetURI( p_item->p_input );
    if( psz_uri && ( !strncmp( psz_uri, "directory:", 10 ) ||
                     !strncmp( psz_uri, "vlc:", 4 ) ) )
    {
        free( psz_uri );
        return VLC_SUCCESS;
    }
    free( psz_uri );

    if( p_playlist->p_fetcher &&
            p_playlist->p_fetcher->i_art_policy == ALBUM_ART_WHEN_PLAYED )
    {
        vlc_bool_t b_has_art;

        char *psz_arturl, *psz_name;
        psz_arturl = input_item_GetArtURL( p_input );
        psz_name = input_item_GetName( p_input );

        /* p_input->p_meta should not be null after a successfull CreateThread */
        b_has_art = !EMPTY_STR( psz_arturl );

        if( !b_has_art || strncmp( psz_arturl, "attachment://", 13 ) )
        {
            PL_DEBUG( "requesting art for %s", psz_name );
            playlist_AskForArtEnqueue( p_playlist, p_input );
        }
        free( psz_arturl );
        free( psz_name );
    }

    val.i_int = p_input->i_id;
    PL_UNLOCK;
    var_Set( p_playlist, "playlist-current", val );
    PL_LOCK;

    return VLC_SUCCESS;
}
