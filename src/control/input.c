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
#include <vlc_demux.h>
#include <vlc/libvlc.h>

#include <vlc/intf.h>

void libvlc_input_free( libvlc_input_t *p_input )
{
    if( p_input ) free( p_input );
}

/*
 * Retrieve the input thread. Be sure to release the object
 * once you are done with it.
 */
input_thread_t *libvlc_get_input_thread( libvlc_input_t *p_input,
                                         libvlc_exception_t *p_e ) 
{
    input_thread_t *p_input_thread;

    if( !p_input ) RAISENULL( "Input is NULL" );

    p_input_thread = (input_thread_t*)vlc_object_get(
                                                    p_input->p_instance->p_vlc,
                                                    p_input->i_input_id );
    if( !p_input_thread ) RAISENULL( "Input does not exist" );

    return p_input_thread;
}

    

/**************************************************************************
 * Getters for stream information
 **************************************************************************/
vlc_int64_t libvlc_input_get_length( libvlc_input_t *p_input,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_input, p_e);
    if( libvlc_exception_raised( p_e ) )  return -1.0;
       
    var_Get( p_input_thread, "length", &val );
    vlc_object_release( p_input_thread );

    return val.i_time / 1000;
}

vlc_int64_t libvlc_input_get_time( libvlc_input_t *p_input,
                                   libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_input, p_e );
    if( libvlc_exception_raised( p_e ) )  return -1.0;

    var_Get( p_input_thread , "time", &val );
    vlc_object_release( p_input_thread );
    return val.i_time / 1000;
}

void libvlc_input_set_time( libvlc_input_t *p_input, vlc_int64_t time,
                            libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t value;

    p_input_thread = libvlc_get_input_thread ( p_input, p_e );
    if( libvlc_exception_raised( p_e ) )  return;
    
    value.i_time = time;
    var_Set( p_input_thread, "time", value );
    vlc_object_release( p_input_thread );
}

void libvlc_input_set_position( libvlc_input_t *p_input, float position,
                                libvlc_exception_t *p_e ) 
{
    input_thread_t *p_input_thread;
    vlc_value_t val;
    val.f_float = position;
    
    p_input_thread = libvlc_get_input_thread ( p_input, p_e);
    if ( libvlc_exception_raised( p_e ) ) return;

    var_Set( p_input_thread, "position", val );
    vlc_object_release( p_input_thread );
}

float libvlc_input_get_position( libvlc_input_t *p_input,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_input, p_e);
    if ( libvlc_exception_raised( p_e ) )  return -1.0;

    var_Get( p_input_thread, "position", &val );
    vlc_object_release( p_input_thread );

    return val.f_float;
}

float libvlc_input_get_fps( libvlc_input_t *p_input,
                            libvlc_exception_t *p_e) 
{
    double f_fps;
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_input, p_e );
    if ( libvlc_exception_raised( p_e ) )  return 0.0;

    if( demux2_Control( p_input_thread->input.p_demux, DEMUX_GET_FPS, &f_fps )
        || f_fps < 0.1 ) 
    {
        vlc_object_release( p_input_thread );
        return 0.0;
    }
    else
    {
        vlc_object_release( p_input_thread );
        return( f_fps );
    }
}

vlc_bool_t libvlc_input_will_play( libvlc_input_t *p_input,
                                   libvlc_exception_t *p_e) 
{
    input_thread_t *p_input_thread =
                            libvlc_get_input_thread ( p_input, p_e);
    if ( libvlc_exception_raised( p_e ) ) return VLC_FALSE;

    if ( !p_input_thread->b_die && !p_input_thread->b_dead ) 
    {
        vlc_object_release( p_input_thread );
        return VLC_TRUE;
    }
    vlc_object_release( p_input_thread );
    return VLC_FALSE;
}
