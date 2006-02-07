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

void libvlc_playlist_play( libvlc_instance_t *p_instance, int i_id,
                           int i_options, char **ppsz_options,
                           libvlc_exception_t *p_exception )
{
    ///\todo Handle additionnal options

    if( p_instance->p_playlist->i_size == 0 )
    {
        libvlc_exception_raise( p_exception, "Empty playlist" );
        return;
    }
    if( i_id > 0 )
    {
        /* Always use the current view when using libvlc */
        playlist_view_t *p_view;
        playlist_item_t *p_item;

        if( p_instance->p_playlist->status.i_view == -1 )
        {
            playlist_Control( p_instance->p_playlist, PLAYLIST_GOTO,
                              i_id );
        }
        p_view = playlist_ViewFind( p_instance->p_playlist,
                                    p_instance->p_playlist->status.i_view );
        if( !p_view )
        {
             libvlc_exception_raise( p_exception,
                                     "Unable to find current playlist view ");
             return;
        }

        p_item = playlist_ItemGetById( p_instance->p_playlist, i_id );

        if( !p_item )
        {
            libvlc_exception_raise( p_exception, "Unable to find item " );
            return;
        }

        playlist_Control( p_instance->p_playlist, PLAYLIST_VIEWPLAY,
                          p_instance->p_playlist->status.i_view,
                          p_view->p_root, p_item );
    }
    else
    {
        playlist_Play( p_instance->p_playlist );
    }
}

void libvlc_playlist_stop( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_exception )
{
    if( playlist_Stop( p_instance->p_playlist ) != VLC_SUCCESS )
    {
        libvlc_exception_raise( p_exception, "Empty playlist" );
    }
}

void libvlc_playlist_clear( libvlc_instance_t *p_instance,
                           libvlc_exception_t *p_exception )
{
    playlist_Clear( p_instance->p_playlist );
}

int libvlc_playlist_add( libvlc_instance_t *p_instance, const char *psz_uri,
                         const char *psz_name, libvlc_exception_t *p_exception )
{
    return libvlc_playlist_add_extended( p_instance, psz_uri, psz_name,
                                         0, NULL, p_exception );
}

int libvlc_playlist_add_extended( libvlc_instance_t *p_instance,
                                  const char *psz_uri, const char *psz_name,
                                  int i_options, const char **ppsz_options,
                                  libvlc_exception_t *p_exception )
{
    return playlist_AddExt( p_instance->p_playlist, psz_uri, psz_name,
                            PLAYLIST_INSERT, PLAYLIST_END, -1, ppsz_options,
                            i_options );
}



libvlc_input_t * libvlc_playlist_get_input( libvlc_instance_t *p_instance,
                                            libvlc_exception_t *p_exception )
{
    libvlc_input_t *p_input;

    vlc_mutex_lock( &p_instance->p_playlist->object_lock );
    if( p_instance->p_playlist->p_input == NULL )
    {
        libvlc_exception_raise( p_exception, "No active input" );
        vlc_mutex_unlock( &p_instance->p_playlist->object_lock );
        return NULL;
    }
    p_input = (libvlc_input_t *)malloc( sizeof( libvlc_input_t ) );

    p_input->i_input_id = p_instance->p_playlist->p_input->i_object_id;
    p_input->p_instance = p_instance;
    vlc_mutex_unlock( &p_instance->p_playlist->object_lock );

    return p_input;
}
