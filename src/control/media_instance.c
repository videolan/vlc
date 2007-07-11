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
 * Release the associated input thread
 *
 * Object lock is NOT held.
 */
static void release_input_thread( libvlc_media_instance_t *p_mi ) 
{
    input_thread_t *p_input_thread;
    vlc_bool_t should_destroy;

    if( !p_mi || p_mi->i_input_id == -1 )
        return;

    p_input_thread = (input_thread_t*)vlc_object_get(
                                             p_mi->p_libvlc_instance->p_libvlc_int,
                                             p_mi->i_input_id );

    p_mi->i_input_id = -1;

    if( !p_input_thread )
        return;

    /* release for previous vlc_object_get */
    vlc_object_release( p_input_thread );

    should_destroy = p_input_thread->i_refcount == 1;

    /* release for initial p_input_thread yield (see _new()) */
    vlc_object_release( p_input_thread );

    /* No one is tracking this input_thread appart us. Destroy it */
    if( should_destroy )
        input_DestroyThread( p_input_thread );
}

/*
 * Retrieve the input thread. Be sure to release the object
 * once you are done with it. (libvlc Internal)
 *
 * Object lock is held.
 */
input_thread_t *libvlc_get_input_thread( libvlc_media_instance_t *p_mi,
                                         libvlc_exception_t *p_e ) 
{
    input_thread_t *p_input_thread;

    vlc_mutex_lock( &p_mi->object_lock );

    if( !p_mi || p_mi->i_input_id == -1 )
    {
        vlc_mutex_unlock( &p_mi->object_lock );
        RAISENULL( "Input is NULL" );
    }

    p_input_thread = (input_thread_t*)vlc_object_get(
                                             p_mi->p_libvlc_instance->p_libvlc_int,
                                             p_mi->i_input_id );
    if( !p_input_thread )
    {
        vlc_mutex_unlock( &p_mi->object_lock );
        RAISENULL( "Input does not exist" );
    }

    vlc_mutex_unlock( &p_mi->object_lock );
    return p_input_thread;
}


/**************************************************************************
 * Create a Media Instance object
 **************************************************************************/
libvlc_media_instance_t *
libvlc_media_instance_new( libvlc_instance_t * p_libvlc_instance,
                           libvlc_exception_t *p_e )
{
    libvlc_media_instance_t * p_mi;

    if( !p_libvlc_instance )
    {
        libvlc_exception_raise( p_e, "invalid libvlc instance" );
        return NULL;
    }

    p_mi = malloc( sizeof(libvlc_media_instance_t) );
    p_mi->p_md = NULL;
    p_mi->p_libvlc_instance = p_libvlc_instance;
    p_mi->i_input_id = -1;
    /* refcount strategy:
     * - All items created by _new start with a refcount set to 1
     * - Accessor _release decrease the refcount by 1, if after that
     *   operation the refcount is 0, the object is destroyed.
     * - Accessor _retain increase the refcount by 1 (XXX: to implement) */
    p_mi->i_refcount = 1;
    /* object_lock strategy:
     * - No lock held in constructor
     * - Lock when accessing all variable this lock is held
     * - Lock when attempting to destroy the object the lock is also held */
    vlc_mutex_init( p_mi->p_libvlc_instance->p_libvlc_int,
                    &p_mi->object_lock );

    return p_mi;
}

/**************************************************************************
 * Create a Media Instance object with a media descriptor
 **************************************************************************/
libvlc_media_instance_t *
libvlc_media_instance_new_from_media_descriptor(
                                    libvlc_media_descriptor_t * p_md,
                                    libvlc_exception_t *p_e )
{
    libvlc_media_instance_t * p_mi;

    if( !p_md )
    {
        libvlc_exception_raise( p_e, "invalid media descriptor" );
        return NULL;
    }

    p_mi = malloc( sizeof(libvlc_media_instance_t) );
    p_mi->p_md = libvlc_media_descriptor_duplicate( p_md );
    p_mi->p_libvlc_instance = p_mi->p_md->p_libvlc_instance;
    p_mi->i_input_id = -1;
    /* same strategy as before */
    p_mi->i_refcount = 1;
    /* same strategy as before */
    vlc_mutex_init( p_mi->p_libvlc_instance->p_libvlc_int,
                    &p_mi->object_lock );

    return p_mi;
}

/**************************************************************************
 * Create a new media instance object from an input_thread (Libvlc Internal)
 **************************************************************************/
libvlc_media_instance_t * libvlc_media_instance_new_from_input_thread(
                                   struct libvlc_instance_t *p_libvlc_instance,
                                   input_thread_t *p_input,
                                   libvlc_exception_t *p_e )
{
    libvlc_media_instance_t * p_mi;

    if( !p_input )
    {
        libvlc_exception_raise( p_e, "invalid input thread" );
        return NULL;
    }

    p_mi = malloc( sizeof(libvlc_media_instance_t) );
    p_mi->p_md = libvlc_media_descriptor_new_from_input_item(
                    p_libvlc_instance,
                    p_input->p->input.p_item, p_e );

    if( !p_mi->p_md )
    {
        free( p_mi );
        return NULL;
    }

    p_mi->p_libvlc_instance = p_libvlc_instance;
    p_mi->i_input_id = p_input->i_object_id;

    /* will be released in media_instance_release() */
    vlc_object_yield( p_input );

    return p_mi;
}

/**************************************************************************
 * Destroy a Media Instance object (libvlc internal)
 *
 * Warning: No lock held here, but hey, this is internal.
 **************************************************************************/
void libvlc_media_instance_destroy( libvlc_media_instance_t *p_mi )
{
    input_thread_t *p_input_thread;
    libvlc_exception_t p_e;

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
    if( !p_mi )
        return;

    vlc_mutex_lock( &p_mi->object_lock );
    
    p_mi->i_refcount--;

    /* We hold the mutex, as a waiter to make sure pending operations
     * are finished. We can't hold it longer as the get_input_thread
     * function holds a lock.  */

    vlc_mutex_unlock( &p_mi->object_lock );
    
    if( p_mi->i_refcount > 0 )
        return;

    release_input_thread( p_mi );

    libvlc_media_descriptor_destroy( p_mi->p_md );

    free( p_mi );
}

/**************************************************************************
 * Set the Media descriptor associated with the instance
 **************************************************************************/
void libvlc_media_instance_set_media_descriptor(
                            libvlc_media_instance_t *p_mi,
                            libvlc_media_descriptor_t *p_md,
                            libvlc_exception_t *p_e )
{
    (void)p_e;

    if( !p_mi )
        return;

    vlc_mutex_lock( &p_mi->object_lock );
    
    release_input_thread( p_mi );

    libvlc_media_descriptor_destroy( p_mi->p_md );

    if( !p_md )
    {
        p_mi->p_md = NULL;
        vlc_mutex_unlock( &p_mi->object_lock );
        return; /* It is ok to pass a NULL md */
    }

    p_mi->p_md = libvlc_media_descriptor_duplicate( p_md );
    
    /* The policy here is to ignore that we were created using a different
     * libvlc_instance, because we don't really care */
    p_mi->p_libvlc_instance = p_md->p_libvlc_instance;

    vlc_mutex_unlock( &p_mi->object_lock );
}

/**************************************************************************
 * Set the Media descriptor associated with the instance
 **************************************************************************/
libvlc_media_descriptor_t *
libvlc_media_instance_get_media_descriptor(
                            libvlc_media_instance_t *p_mi,
                            libvlc_exception_t *p_e )
{
    (void)p_e;

    if( !p_mi->p_md )
        return NULL;

    return libvlc_media_descriptor_duplicate( p_mi->p_md );
}

/**************************************************************************
 * Play
 **************************************************************************/
void libvlc_media_instance_play( libvlc_media_instance_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t * p_input_thread;

    if( (p_input_thread = libvlc_get_input_thread( p_mi, p_e )) ) 
    {
        /* A thread alread exists, send it a play message */        
        vlc_value_t val;
        val.i_int = PLAYING_S;

        input_Control( p_input_thread, INPUT_CONTROL_SET_STATE, PLAYING_S );
        vlc_object_release( p_input_thread );
        return;
    }

    vlc_mutex_lock( &p_mi->object_lock );
    
    if( !p_mi->p_md )
    {
        libvlc_exception_raise( p_e, "no associated media descriptor" );
        vlc_mutex_unlock( &p_mi->object_lock );
        return;
    }

    p_input_thread = input_CreateThread( p_mi->p_libvlc_instance->p_libvlc_int,
                                         p_mi->p_md->p_input_item );
    p_mi->i_input_id = p_input_thread->i_object_id;

    /* will be released in media_instance_release() */
    vlc_object_yield( p_input_thread );

    vlc_mutex_unlock( &p_mi->object_lock );
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

    if( !p_input_thread )
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

