/*****************************************************************************
 * input.c: Libvlc new API input management functions
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

void libvlc_input_free( libvlc_input_t *p_input )
{
    if( p_input )
        free( p_input );
}

/**************************************************************************
 * Getters for stream information
 **************************************************************************/
vlc_int64_t libvlc_input_get_length( libvlc_input_t *p_input,
                             libvlc_exception_t *p_exception )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    if( !p_input )
    {
        libvlc_exception_raise( p_exception, "Input is NULL" );
        return -1;
    }

    p_input_thread = (input_thread_t*)vlc_object_get(
                                 p_input->p_instance->p_vlc,
                                 p_input->i_input_id );
    if( !p_input_thread )
    {
        libvlc_exception_raise( p_exception, "Input does not exist" );
        return -1;
    }
    var_Get( p_input_thread, "length", &val );
    vlc_object_release( p_input_thread );

    return val.i_time / 1000;
}

vlc_int64_t libvlc_input_get_time( libvlc_input_t *p_input,
                           libvlc_exception_t *p_exception )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    if( !p_input )
    {
        libvlc_exception_raise( p_exception, "Input is NULL" );
        return -1;
    }

    p_input_thread = (input_thread_t*)vlc_object_get(
                                 p_input->p_instance->p_vlc,
                                 p_input->i_input_id );
    if( !p_input_thread )
    {
        libvlc_exception_raise( p_exception, "Input does not exist" );
        return -1;
    }
    var_Get( p_input_thread , "time", &val );
    vlc_object_release( p_input_thread );

    return val.i_time / 1000;
}

float libvlc_input_get_position( libvlc_input_t *p_input,
                                 libvlc_exception_t *p_exception )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    if( !p_input )
    {
        libvlc_exception_raise( p_exception, "Input is NULL" );
        return -1;
    }

    p_input_thread = (input_thread_t*)vlc_object_get(
                            p_input->p_instance->p_vlc,
                            p_input->i_input_id );
    if( !p_input_thread )
    {
        libvlc_exception_raise( p_exception, "Input does not exist" );
        return -1.0;
    }
    var_Get( p_input_thread, "position", &val );
    vlc_object_release( p_input_thread );

    return val.f_float;
}
