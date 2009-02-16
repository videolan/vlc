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

#include <vlc_common.h>
#include "vlc_playlist.h"
#include "playlist_internal.h"
#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int PlaylistVAControl( playlist_t * p_playlist, int i_query, va_list args );

/*****************************************************************************
 * Playlist control
 *****************************************************************************/

playlist_t *__pl_Hold( vlc_object_t *p_this )
{
    playlist_t *pl;

    barrier();
    pl = libvlc_priv (p_this->p_libvlc)->p_playlist;

    assert( VLC_OBJECT(pl) != p_this /* This does not make sense to hold the playlist
    using pl_Hold. use vlc_object_hold in this case */ );

    if (pl)
        vlc_object_hold (pl);
    return pl;
}

void __pl_Release( vlc_object_t *p_this )
{
    playlist_t *pl = libvlc_priv (p_this->p_libvlc)->p_playlist;
    assert( pl != NULL );

    /* The rule is that pl_Release() should act on
       the same object than pl_Hold() */
    assert( VLC_OBJECT(pl) != p_this);

    vlc_object_release( pl );
}

void playlist_Lock( playlist_t *pl )
{
    vlc_object_lock( pl );
}

void playlist_Unlock( playlist_t *pl )
{
    vlc_object_unlock( pl );
}

void playlist_AssertLocked( playlist_t *pl )
{
    vlc_object_assert_locked( pl );
}

int playlist_Control( playlist_t * p_playlist, int i_query,
                      bool b_locked, ... )
{
    va_list args;
    int i_result;
    PL_LOCK_IF( !b_locked );
    va_start( args, b_locked );
    i_result = PlaylistVAControl( p_playlist, i_query, args );
    va_end( args );
    PL_UNLOCK_IF( !b_locked );

    return i_result;
}

static int PlaylistVAControl( playlist_t * p_playlist, int i_query, va_list args )
{
    playlist_item_t *p_item, *p_node;

    PL_ASSERT_LOCKED;

    if( !vlc_object_alive( p_playlist ) )
        return VLC_EGENERIC;

    if( playlist_IsEmpty( p_playlist ) )
        return VLC_EGENERIC;

    switch( i_query )
    {
    case PLAYLIST_STOP:
        pl_priv(p_playlist)->request.i_status = PLAYLIST_STOPPED;
        pl_priv(p_playlist)->request.b_request = true;
        pl_priv(p_playlist)->request.p_item = NULL;
        break;

    // Node can be null, it will keep the same. Use with care ...
    // Item null = take the first child of node
    case PLAYLIST_VIEWPLAY:
        p_node = (playlist_item_t *)va_arg( args, playlist_item_t * );
        p_item = (playlist_item_t *)va_arg( args, playlist_item_t * );
        if ( p_node == NULL )
        {
            p_node = get_current_status_node( p_playlist );
            assert( p_node );
        }
        pl_priv(p_playlist)->request.i_status = PLAYLIST_RUNNING;
        pl_priv(p_playlist)->request.i_skip = 0;
        pl_priv(p_playlist)->request.b_request = true;
        pl_priv(p_playlist)->request.p_node = p_node;
        pl_priv(p_playlist)->request.p_item = p_item;
        if( p_item && var_GetBool( p_playlist, "random" ) )
            pl_priv(p_playlist)->b_reset_currently_playing = true;
        break;

    case PLAYLIST_PLAY:
        if( pl_priv(p_playlist)->p_input )
        {
            var_SetInteger( pl_priv(p_playlist)->p_input, "state", PLAYING_S );
            break;
        }
        else
        {
            pl_priv(p_playlist)->request.i_status = PLAYLIST_RUNNING;
            pl_priv(p_playlist)->request.b_request = true;
            pl_priv(p_playlist)->request.p_node = get_current_status_node( p_playlist );
            pl_priv(p_playlist)->request.p_item = get_current_status_item( p_playlist );
            pl_priv(p_playlist)->request.i_skip = 0;
        }
        break;

    case PLAYLIST_PAUSE:
        if( pl_priv(p_playlist)->p_input &&
            var_GetInteger( pl_priv(p_playlist)->p_input, "state" ) == PAUSE_S )
        {
            pl_priv(p_playlist)->status.i_status = PLAYLIST_RUNNING;
            if( pl_priv(p_playlist)->p_input )
            {
                var_SetInteger( pl_priv(p_playlist)->p_input, "state", PLAYING_S );
            }
        }
        else
        {
            pl_priv(p_playlist)->status.i_status = PLAYLIST_PAUSED;
            if( pl_priv(p_playlist)->p_input )
            {
                var_SetInteger( pl_priv(p_playlist)->p_input, "state", PAUSE_S );
            }
        }
        break;

    case PLAYLIST_SKIP:
        pl_priv(p_playlist)->request.p_node = get_current_status_node( p_playlist );
        pl_priv(p_playlist)->request.p_item = get_current_status_item( p_playlist );
        pl_priv(p_playlist)->request.i_skip = (int) va_arg( args, int );
        /* if already running, keep running */
        if( pl_priv(p_playlist)->status.i_status != PLAYLIST_STOPPED )
            pl_priv(p_playlist)->request.i_status = pl_priv(p_playlist)->status.i_status;
        pl_priv(p_playlist)->request.b_request = true;
        break;

    default:
        msg_Err( p_playlist, "unknown playlist query" );
        return VLC_EBADVAR;
    }
    vlc_cond_signal( &pl_priv(p_playlist)->signal );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Preparse control
 *****************************************************************************/
/** Enqueue an item for preparsing */
int playlist_PreparseEnqueue( playlist_t *p_playlist,
                              input_item_t *p_item, bool b_locked )
{
    playlist_private_t *p_sys = pl_priv(p_playlist);

    PL_LOCK_IF( !b_locked );
    if( p_sys->p_preparser )
        playlist_preparser_Push( p_sys->p_preparser, p_item );
    PL_UNLOCK_IF( !b_locked );

    return VLC_SUCCESS;
}

int playlist_AskForArtEnqueue( playlist_t *p_playlist,
                               input_item_t *p_item, bool b_locked )
{
    playlist_private_t *p_sys = pl_priv(p_playlist);

    PL_LOCK_IF( !b_locked );
    if( p_sys->p_fetcher )
        playlist_fetcher_Push( p_sys->p_fetcher, p_item );
    PL_UNLOCK_IF( !b_locked );

    return VLC_SUCCESS;
}

