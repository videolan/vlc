/*****************************************************************************
 * control.c : Handle control of the playlist & running through it
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "vlc_playlist.h"
#include "playlist_internal.h"
#include <assert.h>

/*****************************************************************************
 * Playlist control
 *****************************************************************************/

void playlist_Lock( playlist_t *pl )
{
    vlc_mutex_lock( &pl_priv(pl)->lock );
}

void playlist_Unlock( playlist_t *pl )
{
    vlc_mutex_unlock( &pl_priv(pl)->lock );
}

void playlist_AssertLocked( playlist_t *pl )
{
    vlc_mutex_assert( &pl_priv(pl)->lock );
}

static void playlist_vaControl( playlist_t *p_playlist, int i_query,
                                bool locked, va_list args )
{
    PL_LOCK_IF( !locked );

    if( pl_priv(p_playlist)->killed )
        ;
    else
    switch( i_query )
    {
    case PLAYLIST_STOP:
        pl_priv(p_playlist)->request.b_request = true;
        pl_priv(p_playlist)->request.p_item = NULL;
        pl_priv(p_playlist)->request.p_node = NULL;
        break;

    // Node can be null, it will keep the same. Use with care ...
    // Item null = take the first child of node
    case PLAYLIST_VIEWPLAY:
    {
        playlist_item_t *p_node = va_arg( args, playlist_item_t * );
        playlist_item_t *p_item = va_arg( args, playlist_item_t * );

        assert( locked || (p_item == NULL && p_node == NULL) );

        if ( p_node == NULL )
        {
            p_node = get_current_status_node( p_playlist );
            assert( p_node );
        }
        pl_priv(p_playlist)->request.i_skip = 0;
        pl_priv(p_playlist)->request.b_request = true;
        pl_priv(p_playlist)->request.p_node = p_node;
        pl_priv(p_playlist)->request.p_item = p_item;
        if( p_item && var_GetBool( p_playlist, "random" ) )
            pl_priv(p_playlist)->b_reset_currently_playing = true;
        break;
    }

    case PLAYLIST_PLAY:
        if( pl_priv(p_playlist)->p_input == NULL )
        {
            pl_priv(p_playlist)->request.b_request = true;
            pl_priv(p_playlist)->request.p_node = get_current_status_node( p_playlist );
            pl_priv(p_playlist)->request.p_item = get_current_status_item( p_playlist );
            pl_priv(p_playlist)->request.i_skip = 0;
        }
        else
            var_SetInteger( pl_priv(p_playlist)->p_input, "state", PLAYING_S );
        break;

    case PLAYLIST_TOGGLE_PAUSE:
        if( pl_priv(p_playlist)->p_input == NULL )
        {
            pl_priv(p_playlist)->request.b_request = true;
            pl_priv(p_playlist)->request.p_node = get_current_status_node( p_playlist );
            pl_priv(p_playlist)->request.p_item = get_current_status_item( p_playlist );
            pl_priv(p_playlist)->request.i_skip = 0;
        }
        else
        if( var_GetInteger( pl_priv(p_playlist)->p_input, "state" ) == PAUSE_S )
            var_SetInteger( pl_priv(p_playlist)->p_input, "state", PLAYING_S );
        else
            var_SetInteger( pl_priv(p_playlist)->p_input, "state", PAUSE_S );
        break;

    case PLAYLIST_SKIP:
        pl_priv(p_playlist)->request.p_node = get_current_status_node( p_playlist );
        pl_priv(p_playlist)->request.p_item = get_current_status_item( p_playlist );
        pl_priv(p_playlist)->request.i_skip = (int) va_arg( args, int );
        pl_priv(p_playlist)->request.b_request = true;
        break;

    case PLAYLIST_PAUSE:
        if( pl_priv(p_playlist)->p_input == NULL )
            break;
        var_SetInteger( pl_priv(p_playlist)->p_input, "state", PAUSE_S );
        break;

    case PLAYLIST_RESUME:
        if( pl_priv(p_playlist)->p_input == NULL )
            break;
        var_SetInteger( pl_priv(p_playlist)->p_input, "state", PLAYING_S );
        break;
    }
    vlc_cond_signal( &pl_priv(p_playlist)->signal );
    PL_UNLOCK_IF( !locked );
}

void playlist_Control( playlist_t *p_playlist, int query, int locked, ... )
{
    va_list args;

    va_start( args, locked );
    playlist_vaControl( p_playlist, query, (bool)locked, args );
    va_end( args );
}
