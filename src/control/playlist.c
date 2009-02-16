/*****************************************************************************
 * playlist.c: libvlc new API playlist handling functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include "libvlc_internal.h"

#include <vlc/libvlc.h>
#include <vlc_playlist.h>

#include <assert.h>

#include "../playlist/playlist_internal.h"

#define PL (libvlc_priv (p_instance->p_libvlc_int)->p_playlist)

static inline int playlist_was_locked( libvlc_instance_t *p_instance )
{
    int was_locked;
    vlc_mutex_lock( &p_instance->instance_lock );
    was_locked = p_instance->b_playlist_locked;
    vlc_mutex_unlock( &p_instance->instance_lock );
    return was_locked;
}

static inline void playlist_mark_locked( libvlc_instance_t *p_instance,
                                         int locked )
{
    vlc_mutex_lock( &p_instance->instance_lock );
    p_instance->b_playlist_locked = locked;
    vlc_mutex_unlock( &p_instance->instance_lock );
}

void libvlc_playlist_loop( libvlc_instance_t *p_instance, int loop,
                           libvlc_exception_t *p_e)
{
    VLC_UNUSED(p_e);

    assert( PL );
    var_SetBool( PL, "loop", loop );
}

void libvlc_playlist_play( libvlc_instance_t *p_instance, int i_id,
                           int i_options, char **ppsz_options,
                           libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e); VLC_UNUSED(i_options); VLC_UNUSED(ppsz_options);

    int did_lock = 0;
    assert( PL );
    ///\todo Handle additionnal options

    if( PL->items.i_size == 0 ) RAISEVOID( "Empty playlist" );
    if( i_id > 0 )
    {
        playlist_item_t *p_item;
        if (! playlist_was_locked( p_instance ) )
        {
            playlist_mark_locked( p_instance, 1 );
            vlc_object_lock( PL );
            did_lock = 1;
        }

        p_item = playlist_ItemGetByInputId( PL, i_id,
                                            PL->p_root_category );
        if( !p_item )
        {
            if( did_lock == 1 )
            {
                vlc_object_unlock( PL );
                playlist_mark_locked( p_instance, 0 );
            }
            RAISEVOID( "Unable to find item" );
        }

        playlist_Control( PL, PLAYLIST_VIEWPLAY, pl_Locked,
                          PL->p_root_category, p_item );
        if( did_lock == 1 )
        {
            vlc_object_unlock( PL );
            playlist_mark_locked( p_instance, 0 );
        }
    }
    else
    {
        playlist_Control( PL, PLAYLIST_PLAY,
                          playlist_was_locked( p_instance ) );
    }
}

void libvlc_playlist_pause( libvlc_instance_t *p_instance,
                            libvlc_exception_t *p_e )
{
    assert( PL );
    if( playlist_Control( PL, PLAYLIST_PAUSE,
                          playlist_was_locked( p_instance ) ) != VLC_SUCCESS )
        RAISEVOID( "Empty playlist" );
}


void libvlc_playlist_stop( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_e )
{
    assert( PL );
    if( playlist_Control( PL, PLAYLIST_STOP,
                          playlist_was_locked( p_instance ) ) != VLC_SUCCESS )
        RAISEVOID( "Empty playlist" );
}

void libvlc_playlist_clear( libvlc_instance_t *p_instance,
                            libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    assert( PL );
    playlist_Clear( PL, playlist_was_locked( p_instance ) );
}

void libvlc_playlist_next( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_e )
{
    assert( PL );
    if( playlist_Control( PL, PLAYLIST_SKIP, playlist_was_locked( p_instance ),
                          1 ) != VLC_SUCCESS )
        RAISEVOID( "Empty playlist" );
}

void libvlc_playlist_prev( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_e )
{
    if( playlist_Control( PL, PLAYLIST_SKIP, playlist_was_locked( p_instance ),
                          -1  ) != VLC_SUCCESS )
        RAISEVOID( "Empty playlist" );
}

int libvlc_playlist_add( libvlc_instance_t *p_instance, const char *psz_uri,
                         const char *psz_name, libvlc_exception_t *p_e )
{
    return libvlc_playlist_add_extended( p_instance, psz_uri, psz_name,
                                         0, NULL, p_e );
}

static int PlaylistAddExtended( libvlc_instance_t *p_instance,
                                const char *psz_uri, const char *psz_name,
                                int i_options, const char **ppsz_options,
                                unsigned i_option_flags,
                                libvlc_exception_t *p_e )
{
    assert( PL );
    if( playlist_was_locked( p_instance ) )
    {
        libvlc_exception_raise( p_e, "You must unlock playlist before "
                               "calling libvlc_playlist_add" );
        return VLC_EGENERIC;
    }
    return playlist_AddExt( PL, psz_uri, psz_name,
                            PLAYLIST_INSERT, PLAYLIST_END, -1,
                            i_options, ppsz_options, i_option_flags,
                            true, pl_Unlocked );
}
int libvlc_playlist_add_extended( libvlc_instance_t *p_instance,
                                  const char *psz_uri, const char *psz_name,
                                  int i_options, const char **ppsz_options,
                                  libvlc_exception_t *p_e )
{
    return PlaylistAddExtended( p_instance, psz_uri, psz_name,
                                i_options, ppsz_options, VLC_INPUT_OPTION_TRUSTED,
                                p_e );
}
int libvlc_playlist_add_extended_untrusted( libvlc_instance_t *p_instance,
                                            const char *psz_uri, const char *psz_name,
                                            int i_options, const char **ppsz_options,
                                            libvlc_exception_t *p_e )
{
    return PlaylistAddExtended( p_instance, psz_uri, psz_name,
                                i_options, ppsz_options, 0,
                                p_e );
}

int libvlc_playlist_delete_item( libvlc_instance_t *p_instance, int i_id,
                                 libvlc_exception_t *p_e )
{
    assert( PL );

    if( playlist_DeleteFromInput( PL, i_id,
                                  playlist_was_locked( p_instance ) ) )
    {
        libvlc_exception_raise( p_e, "deletion failed" );
        return VLC_ENOITEM;
    }
    return VLC_SUCCESS;
}

int libvlc_playlist_isplaying( libvlc_instance_t *p_instance,
                               libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    assert( PL );
    return playlist_Status( PL ) == PLAYLIST_RUNNING;
}

int libvlc_playlist_items_count( libvlc_instance_t *p_instance,
                                 libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    assert( PL );
    return playlist_CurrentSize( PL );
}

int libvlc_playlist_get_current_index ( libvlc_instance_t *p_instance,
                                        libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    assert( PL );
    playlist_item_t *p_item = playlist_CurrentPlayingItem( PL );
    if( !p_item )
        return -1;
    return p_item->i_id;
}

void libvlc_playlist_lock( libvlc_instance_t *p_instance )
{
    assert( PL );
    vlc_object_lock( PL );
    p_instance->b_playlist_locked = 1;
}

void libvlc_playlist_unlock( libvlc_instance_t *p_instance )
{
    assert( PL );
    p_instance->b_playlist_locked = 0;
    vlc_object_unlock( PL );
}

libvlc_media_player_t * libvlc_playlist_get_media_player(
                                libvlc_instance_t *p_instance,
                                libvlc_exception_t *p_e )
{
    libvlc_media_player_t *p_mi;
    assert( PL );
    input_thread_t * input = playlist_CurrentInput( PL );
    if( input )
    {
        p_mi = libvlc_media_player_new_from_input_thread(
                            p_instance, input, p_e );
        vlc_object_release( input );
    }
    else
    {
        /* no active input */
        p_mi = NULL;
        libvlc_exception_raise( p_e, "No active input" );
    }

    return p_mi;
}

