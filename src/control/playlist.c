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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <libvlc_internal.h>
#include <vlc/libvlc.h>

#include <vlc/intf.h>

void libvlc_playlist_play( libvlc_instance_t *p_instance,
                           int i_options, char **ppsz_options,
                           libvlc_exception_t *p_exception )
{
    ///\todo Handle additionnal options

    if( p_instance->p_playlist->i_size == 0 )
    {
        libvlc_exception_raise( p_exception, "Empty playlist" );
        return;
    }
    playlist_Play( p_instance->p_playlist );
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
        return;
    }
    p_input = (libvlc_input_t *)malloc( sizeof( libvlc_input_t ) );

    p_input->i_input_id = p_instance->p_playlist->p_input->i_object_id;
    p_input->p_instance = p_instance;
    vlc_mutex_unlock( &p_instance->p_playlist->object_lock );

    return p_input;
}
