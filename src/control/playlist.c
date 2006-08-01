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

#include <libvlc_internal.h>
#include <vlc/libvlc.h>

#include <vlc/intf.h>

#include <assert.h>

void libvlc_playlist_play( libvlc_instance_t *p_instance, int i_id,
                           int i_options, char **ppsz_options,
                           libvlc_exception_t *p_e )
{
    assert( p_instance->p_playlist );
    ///\todo Handle additionnal options

    if( p_instance->p_playlist->i_size == 0 ) RAISEVOID( "Empty playlist" );
    if( i_id > 0 )
    {
        playlist_item_t *p_item = playlist_ItemGetById( p_instance->p_playlist,
                                                        i_id );
        if( !p_item ) RAISEVOID( "Unable to find item" );

        playlist_LockControl( p_instance->p_playlist, PLAYLIST_VIEWPLAY,
                          p_instance->p_playlist->status.p_node, p_item );
    }
    else
    {
        playlist_Play( p_instance->p_playlist );
    }
}

void libvlc_playlist_pause( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_e )
{
    assert( p_instance->p_playlist );
    if( playlist_Pause( p_instance->p_playlist ) != VLC_SUCCESS )
        RAISEVOID( "Empty playlist" );
}


void libvlc_playlist_stop( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_e )
{
    assert( p_instance->p_playlist );
    if( playlist_Stop( p_instance->p_playlist ) != VLC_SUCCESS )
        RAISEVOID( "Empty playlist" );
}

void libvlc_playlist_clear( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_e )
{
    assert( p_instance->p_playlist );
    playlist_Clear( p_instance->p_playlist );
}

void libvlc_playlist_next( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_e )
{
    assert( p_instance->p_playlist );
    if( playlist_Next( p_instance->p_playlist ) != VLC_SUCCESS )
        RAISEVOID( "Empty playlist" );
}

void libvlc_playlist_prev( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_e )
{
    if( playlist_Prev( p_instance->p_playlist ) != VLC_SUCCESS )
        RAISEVOID( "Empty playlist" );
}

int libvlc_playlist_add( libvlc_instance_t *p_instance, const char *psz_uri,
                         const char *psz_name, libvlc_exception_t *p_e )
{
    return libvlc_playlist_add_extended( p_instance, psz_uri, psz_name,
                                         0, NULL, p_e );
}

int libvlc_playlist_add_extended( libvlc_instance_t *p_instance,
                                  const char *psz_uri, const char *psz_name,
                                  int i_options, const char **ppsz_options,
                                  libvlc_exception_t *p_e )
{
    assert( p_instance->p_playlist );
    return playlist_PlaylistAddExt( p_instance->p_playlist, psz_uri, psz_name,
                            PLAYLIST_INSERT, PLAYLIST_END, -1, ppsz_options,
                            i_options );
}

int libvlc_playlist_delete_item( libvlc_instance_t *p_instance, int i_id,
                                 libvlc_exception_t *p_e )
{
    assert( p_instance->p_playlist );
    return playlist_DeleteFromItemId( p_instance->p_playlist, i_id );
}


int libvlc_playlist_isplaying( libvlc_instance_t *p_instance,
                               libvlc_exception_t *p_e )
{
    assert( p_instance->p_playlist );
    return playlist_IsPlaying( p_instance->p_playlist );
}

int libvlc_playlist_items_count( libvlc_instance_t *p_instance,
                                 libvlc_exception_t *p_e )
{
    assert( p_instance->p_playlist );
    return p_instance->p_playlist->i_size;
}

libvlc_input_t * libvlc_playlist_get_input( libvlc_instance_t *p_instance,
                                            libvlc_exception_t *p_e )
{
    libvlc_input_t *p_input;
    assert( p_instance->p_playlist );

    vlc_mutex_lock( &p_instance->p_playlist->object_lock );
    if( p_instance->p_playlist->p_input == NULL )
    {
        libvlc_exception_raise( p_e, "No active input" );
        vlc_mutex_unlock( &p_instance->p_playlist->object_lock );
        return NULL;
    }
    p_input = (libvlc_input_t *)malloc( sizeof( libvlc_input_t ) );

    p_input->i_input_id = p_instance->p_playlist->p_input->i_object_id;
    p_input->p_instance = p_instance;
    vlc_mutex_unlock( &p_instance->p_playlist->object_lock );

    return p_input;
}
