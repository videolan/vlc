/*****************************************************************************
 * media_instance.c: Libvlc API Media Instance management functions
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
#include <vlc_demux.h>
#include <vlc_input.h>
#include "input/input_internal.h"

/*
 * Retrieve the input thread. Be sure to release the object
 * once you are done with it. (libvlc Internal)
 */
input_thread_t *libvlc_get_input_thread( libvlc_media_instance_t *p_mi,
                                         libvlc_exception_t *p_e ) 
{
    input_thread_t *p_input_thread;

    if( !p_mi || p_mi->i_input_id == -1 )
        RAISENULL( "Input is NULL" );

    p_input_thread = (input_thread_t*)vlc_object_get(
                                             p_mi->p_libvlc_instance->p_libvlc_int,
                                             p_mi->i_input_id );
    if( !p_input_thread )
        RAISENULL( "Input does not exist" );

    return p_input_thread;
}

/**************************************************************************
 * Create a Media Instance object
 **************************************************************************/
libvlc_media_instance_t *
libvlc_media_instance_new( libvlc_media_descriptor_t *p_md )
{
    libvlc_media_instance_t * p_mi;

    if( !p_md )
        return NULL;

    p_mi = malloc( sizeof(libvlc_media_instance_t) );
    p_mi->p_md = libvlc_media_descriptor_duplicate( p_md );
    p_mi->p_libvlc_instance = p_mi->p_md->p_libvlc_instance;
    p_mi->i_input_id = -1;

    return p_mi;
}

/**************************************************************************
 * Create a new media instance object from an input_thread (Libvlc Internal)
 **************************************************************************/
libvlc_media_instance_t * libvlc_media_instance_new_from_input_thread(
                                   struct libvlc_instance_t *p_libvlc_instance,
                                   input_thread_t *p_input )
{
    libvlc_media_instance_t * p_mi;

    p_mi = malloc( sizeof(libvlc_media_instance_t) );
    p_mi->p_md = libvlc_media_descriptor_new_from_input_item(
                    p_libvlc_instance,
                    p_input->p->input.p_item );
    p_mi->p_libvlc_instance = p_libvlc_instance;
    p_mi->i_input_id = p_input->i_object_id;

    /* will be released in media_instance_release() */
    vlc_object_retain( p_input );

    return p_mi;
}

/**************************************************************************
 * Destroy a Media Instance object
 **************************************************************************/
void libvlc_media_instance_destroy( libvlc_media_instance_t *p_mi )
{
    input_thread_t *p_input_thread;
    libvlc_exception_t p_e;

    /* XXX: locking */
    libvlc_exception_init( &p_e );

    if( !p_mi )
        return;

    p_input_thread = libvlc_get_input_thread( p_mi, &p_e );

    if( libvlc_exception_raised( &p_e ) )
        return; /* no need to worry about no input thread */
    
    input_DestroyThread( p_input_thread );

    libvlc_media_descriptor_destroy( p_mi->p_md );

    free( p_mi );
}

/**************************************************************************
 * Release a Media Instance object
 **************************************************************************/
void libvlc_media_instance_release( libvlc_media_instance_t *p_mi )
{
    input_thread_t *p_input_thread;
    libvlc_exception_t p_e;

    /* XXX: locking */
    libvlc_exception_init( &p_e );

    if( !p_mi )
        return;

    p_input_thread = libvlc_get_input_thread( p_mi, &p_e );

    if( !libvlc_exception_raised( &p_e ) )
    {
        /* release for previous libvlc_get_input_thread */
        vlc_object_release( p_input_thread );

        /* release for initial p_input_thread yield (see _new()) */
        vlc_object_release( p_input_thread );

        /* No one is tracking this input_thread appart us. Destroy it */
        if( p_input_thread->i_refcount <= 0 )
            input_DestroyThread( p_input_thread );
        /* btw, we still have an XXX locking here */
    }

    libvlc_media_descriptor_destroy( p_mi->p_md );

    free( p_mi );
}

/**************************************************************************
 * Play
 **************************************************************************/
void libvlc_media_instance_play( libvlc_media_instance_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t * p_input_thread;

    if( p_mi->i_input_id != -1) 
    {
        vlc_value_t val;
        val.i_int = PLAYING_S;

        /* A thread alread exists, send it a play message */
        p_input_thread = libvlc_get_input_thread( p_mi, p_e );

        if( libvlc_exception_raised( p_e ) )
            return;

        input_Control( p_input_thread, INPUT_CONTROL_SET_STATE, PLAYING_S );
        vlc_object_release( p_input_thread );
        return;
    }

    p_input_thread = input_CreateThread( p_mi->p_libvlc_instance->p_libvlc_int,
                                         p_mi->p_md->p_input_item );
    p_mi->i_input_id = p_input_thread->i_object_id;

    /* will be released in media_instance_release() */
    vlc_object_yield( p_input_thread );
}

/**************************************************************************
 * Pause
 **************************************************************************/
void libvlc_media_instance_pause( libvlc_media_instance_t *p_mi,
                                  libvlc_exception_t *p_e )
{
    input_thread_t * p_input_thread;
    vlc_value_t val;
    val.i_int = PAUSE_S;

    p_input_thread = libvlc_get_input_thread( p_mi, p_e );

    if( libvlc_exception_raised( p_e ) )
        return;

    input_Control( p_input_thread, INPUT_CONTROL_SET_STATE, val );
    vlc_object_release( p_input_thread );
}

/**************************************************************************
 * Getters for stream information
 **************************************************************************/
vlc_int64_t libvlc_media_instance_get_length(
                             libvlc_media_instance_t *p_mi,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return -1;

    var_Get( p_input_thread, "length", &val );
    vlc_object_release( p_input_thread );

    return (val.i_time+500LL)/1000LL;
}

vlc_int64_t libvlc_media_instance_get_time(
                                   libvlc_media_instance_t *p_mi,
                                   libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1;

    var_Get( p_input_thread , "time", &val );
    vlc_object_release( p_input_thread );
    return (val.i_time+500LL)/1000LL;
}

void libvlc_media_instance_set_time(
                                 libvlc_media_instance_t *p_mi,
                                 vlc_int64_t time,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t value;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return;

    value.i_time = time*1000LL;
    var_Set( p_input_thread, "time", value );
    vlc_object_release( p_input_thread );
}

void libvlc_media_instance_set_position(
                                libvlc_media_instance_t *p_mi,
                                float position,
                                libvlc_exception_t *p_e ) 
{
    input_thread_t *p_input_thread;
    vlc_value_t val;
    val.f_float = position;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return;

    var_Set( p_input_thread, "position", val );
    vlc_object_release( p_input_thread );
}

float libvlc_media_instance_get_position(
                                 libvlc_media_instance_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1.0;

    var_Get( p_input_thread, "position", &val );
    vlc_object_release( p_input_thread );

    return val.f_float;
}

float libvlc_media_instance_get_fps(
                                 libvlc_media_instance_t *p_mi,
                                 libvlc_exception_t *p_e) 
{
    double f_fps = 0.0;
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return 0.0;

    if( (NULL == p_input_thread->p->input.p_demux)
        || demux2_Control( p_input_thread->p->input.p_demux, DEMUX_GET_FPS, &f_fps )
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

vlc_bool_t libvlc_media_instance_will_play(
                                 libvlc_media_instance_t *p_mi,
                                 libvlc_exception_t *p_e) 
{
    input_thread_t *p_input_thread =
                            libvlc_get_input_thread ( p_mi, p_e);
    if ( !p_input_thread )
        return VLC_FALSE;

    if ( !p_input_thread->b_die && !p_input_thread->b_dead ) 
    {
        vlc_object_release( p_input_thread );
        return VLC_TRUE;
    }
    vlc_object_release( p_input_thread );
    return VLC_FALSE;
}

void libvlc_media_instance_set_rate(
                                 libvlc_media_instance_t *p_mi,
                                 float rate,
                                 libvlc_exception_t *p_e ) 
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    if( rate <= 0 )
        RAISEVOID( "Rate value is invalid" );

    val.i_int = 1000.0f/rate;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if ( !p_input_thread )
        return;

    var_Set( p_input_thread, "rate", val );
    vlc_object_release( p_input_thread );
}

float libvlc_media_instance_get_rate(
                                 libvlc_media_instance_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if ( !p_input_thread )
        return -1.0;

    var_Get( p_input_thread, "rate", &val );
    vlc_object_release( p_input_thread );

    return (float)1000.0f/val.i_int;
}

int libvlc_media_instance_get_state(
                                 libvlc_media_instance_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if ( !p_input_thread )
        return 0;

    var_Get( p_input_thread, "state", &val );
    vlc_object_release( p_input_thread );

    return val.i_int;
}

