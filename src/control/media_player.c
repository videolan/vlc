/*****************************************************************************
 * media_player.c: Libvlc API Media Instance management functions
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
#include <vlc_vout.h>
#include "libvlc.h"
#include <assert.h>

static int
input_seekable_changed( vlc_object_t * p_this, char const * psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval,
                        void * p_userdata );
static int
input_pausable_changed( vlc_object_t * p_this, char const * psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval,
                        void * p_userdata );
static int
input_event_changed( vlc_object_t * p_this, char const * psz_cmd,
                     vlc_value_t oldval, vlc_value_t newval,
                     void * p_userdata );

static int SnapshotTakenCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data );

static const libvlc_state_t vlc_to_libvlc_state_array[] =
{
    [INIT_S]        = libvlc_NothingSpecial,
    [OPENING_S]     = libvlc_Opening,
    [PLAYING_S]     = libvlc_Playing,
    [PAUSE_S]       = libvlc_Paused,
    [END_S]         = libvlc_Ended,
    [ERROR_S]       = libvlc_Error,
};

static inline libvlc_state_t vlc_to_libvlc_state( int vlc_state )
{
    if( vlc_state < 0 || vlc_state > 6 )
        return libvlc_Ended;

    return vlc_to_libvlc_state_array[vlc_state];
}

/*
 * Release the associated input thread
 *
 * Object lock is NOT held.
 */
static void release_input_thread( libvlc_media_player_t *p_mi )
{
    input_thread_t * p_input_thread;

    if( !p_mi || !p_mi->p_input_thread )
        return;

    p_input_thread = p_mi->p_input_thread;

    /* No one is tracking this input_thread appart us. Destroy it */
    if( p_mi->b_own_its_input_thread )
    {
        var_DelCallback( p_input_thread, "can-seek", input_seekable_changed, p_mi );
        var_DelCallback( p_input_thread, "can-pause", input_pausable_changed, p_mi );
        var_DelCallback( p_input_thread, "intf-event", input_event_changed, p_mi );

        /* We owned this one */
        input_StopThread( p_input_thread );
        vlc_thread_join( p_input_thread );

        var_Destroy( p_input_thread, "drawable-hwnd" );
        var_Destroy( p_input_thread, "drawable-xid" );
    }

    vlc_object_release( p_input_thread );

    p_mi->p_input_thread = NULL;
}

/*
 * Retrieve the input thread. Be sure to release the object
 * once you are done with it. (libvlc Internal)
 *
 * Object lock is held.
 */
input_thread_t *libvlc_get_input_thread( libvlc_media_player_t *p_mi,
                                         libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;

    if( !p_mi ) RAISENULL( "Media Instance is NULL" );

    vlc_mutex_lock( &p_mi->object_lock );

    if( !p_mi->p_input_thread )
    {
        vlc_mutex_unlock( &p_mi->object_lock );
        RAISENULL( "Input is NULL" );
    }

    p_input_thread = p_mi->p_input_thread;
    vlc_object_hold( p_input_thread );

    vlc_mutex_unlock( &p_mi->object_lock );

    return p_input_thread;
}

static int
input_seekable_changed( vlc_object_t * p_this, char const * psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval,
                        void * p_userdata )
{
    VLC_UNUSED(oldval);
    VLC_UNUSED(p_this);
    VLC_UNUSED(psz_cmd);
    libvlc_media_player_t * p_mi = p_userdata;
    libvlc_event_t event;

    libvlc_media_set_state( p_mi->p_md, libvlc_NothingSpecial, NULL);
    event.type = libvlc_MediaPlayerSeekableChanged;
    event.u.media_player_seekable_changed.new_seekable = newval.b_bool;

    libvlc_event_send( p_mi->p_event_manager, &event );
    return VLC_SUCCESS;
}

static int
input_pausable_changed( vlc_object_t * p_this, char const * psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval,
                        void * p_userdata )
{
    VLC_UNUSED(oldval);
    VLC_UNUSED(p_this);
    VLC_UNUSED(psz_cmd);
    libvlc_media_player_t * p_mi = p_userdata;
    libvlc_event_t event;

    libvlc_media_set_state( p_mi->p_md, libvlc_NothingSpecial, NULL);
    event.type = libvlc_MediaPlayerPausableChanged;
    event.u.media_player_pausable_changed.new_pausable = newval.b_bool;

    libvlc_event_send( p_mi->p_event_manager, &event );
    return VLC_SUCCESS;
}

static int
input_event_changed( vlc_object_t * p_this, char const * psz_cmd,
                     vlc_value_t oldval, vlc_value_t newval,
                     void * p_userdata )
{
    VLC_UNUSED(oldval);
    input_thread_t * p_input = (input_thread_t *)p_this;
    libvlc_media_player_t * p_mi = p_userdata;
    libvlc_event_t event;

    assert( !strcmp( psz_cmd, "intf-event" ) );

    if( newval.i_int == INPUT_EVENT_STATE )
    {
        libvlc_state_t libvlc_state;

        switch ( var_GetInteger( p_input, "state" ) )
        {
            case INIT_S:
                libvlc_state = libvlc_NothingSpecial;
                event.type = libvlc_MediaPlayerNothingSpecial;
                break;
            case OPENING_S:
                libvlc_state = libvlc_Opening;
                event.type = libvlc_MediaPlayerOpening;
                break;
            case PLAYING_S:
                libvlc_state = libvlc_Playing;
                event.type = libvlc_MediaPlayerPlaying;
                break;
            case PAUSE_S:
                libvlc_state = libvlc_Paused;
                event.type = libvlc_MediaPlayerPaused;
                break;
            case END_S:
                libvlc_state = libvlc_Ended;
                event.type = libvlc_MediaPlayerEndReached;
                break;
            case ERROR_S:
                libvlc_state = libvlc_Error;
                event.type = libvlc_MediaPlayerEncounteredError;
                break;

            default:
                return VLC_SUCCESS;
        }

        libvlc_media_set_state( p_mi->p_md, libvlc_state, NULL);
        libvlc_event_send( p_mi->p_event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_TIMES )
    {
        if( var_GetInteger( p_input, "state" ) != PLAYING_S )
            return VLC_SUCCESS; /* Don't send the position while stopped */

        /* */
        event.type = libvlc_MediaPlayerPositionChanged;
        event.u.media_player_position_changed.new_position = var_GetFloat( p_input, "position" );;
        libvlc_event_send( p_mi->p_event_manager, &event );

        /* */
        event.type = libvlc_MediaPlayerTimeChanged;
        event.u.media_player_time_changed.new_time = var_GetTime( p_input, "time" );
        libvlc_event_send( p_mi->p_event_manager, &event );
    }

    return VLC_SUCCESS;

}


/**************************************************************************
 * Create a Media Instance object
 **************************************************************************/
libvlc_media_player_t *
libvlc_media_player_new( libvlc_instance_t * p_libvlc_instance,
                           libvlc_exception_t * p_e )
{
    libvlc_media_player_t * p_mi;

    if( !p_libvlc_instance )
    {
        libvlc_exception_raise( p_e, "invalid libvlc instance" );
        return NULL;
    }

    p_mi = malloc( sizeof(libvlc_media_player_t) );
    if( !p_mi )
    {
        libvlc_exception_raise( p_e, "Not enough memory" );
        return NULL;
    }
    p_mi->p_md = NULL;
    p_mi->drawable.xid = 0;
    p_mi->drawable.hwnd = NULL;
    p_mi->p_libvlc_instance = p_libvlc_instance;
    p_mi->p_input_thread = NULL;
    /* refcount strategy:
     * - All items created by _new start with a refcount set to 1
     * - Accessor _release decrease the refcount by 1, if after that
     *   operation the refcount is 0, the object is destroyed.
     * - Accessor _retain increase the refcount by 1 (XXX: to implement) */
    p_mi->i_refcount = 1;
    p_mi->b_own_its_input_thread = true;
    /* object_lock strategy:
     * - No lock held in constructor
     * - Lock when accessing all variable this lock is held
     * - Lock when attempting to destroy the object the lock is also held */
    vlc_mutex_init( &p_mi->object_lock );
    p_mi->p_event_manager = libvlc_event_manager_new( p_mi,
            p_libvlc_instance, p_e );
    if( libvlc_exception_raised( p_e ) )
    {
        free( p_mi );
        return NULL;
    }

    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerNothingSpecial, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerOpening, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerBuffering, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerPlaying, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerPaused, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerStopped, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerForward, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerBackward, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerEndReached, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerEncounteredError, p_e );

    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerPositionChanged, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerTimeChanged, p_e );
     libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerTitleChanged, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerSeekableChanged, p_e );
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
            libvlc_MediaPlayerPausableChanged, p_e );

    /* Snapshot initialization */
    libvlc_event_manager_register_event_type( p_mi->p_event_manager,
           libvlc_MediaPlayerSnapshotTaken, p_e );
    /* Attach a var callback to the global object to provide the glue between
        vout_thread that generates the event and media_player that re-emits it
        with its own event manager
    */
    var_Create( p_libvlc_instance->p_libvlc_int, "vout-snapshottaken", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_libvlc_instance->p_libvlc_int, "vout-snapshottaken", SnapshotTakenCallback, p_mi );

    return p_mi;
}

/**************************************************************************
 * Create a Media Instance object with a media descriptor
 **************************************************************************/
libvlc_media_player_t *
libvlc_media_player_new_from_media(
                                    libvlc_media_t * p_md,
                                    libvlc_exception_t *p_e )
{
    libvlc_media_player_t * p_mi;
    p_mi = libvlc_media_player_new( p_md->p_libvlc_instance, p_e );

    if( !p_mi )
        return NULL;

    libvlc_media_retain( p_md );
    p_mi->p_md = p_md;

    return p_mi;
}

/**************************************************************************
 * Create a new media instance object from an input_thread (Libvlc Internal)
 **************************************************************************/
libvlc_media_player_t * libvlc_media_player_new_from_input_thread(
                                   struct libvlc_instance_t *p_libvlc_instance,
                                   input_thread_t *p_input,
                                   libvlc_exception_t *p_e )
{
    libvlc_media_player_t * p_mi;

    if( !p_input )
    {
        libvlc_exception_raise( p_e, "invalid input thread" );
        return NULL;
    }

    p_mi = libvlc_media_player_new( p_libvlc_instance, p_e );

    if( !p_mi )
        return NULL;

    p_mi->p_md = libvlc_media_new_from_input_item(
                    p_libvlc_instance,
                    input_GetItem( p_input ), p_e );

    if( !p_mi->p_md )
    {
        libvlc_media_player_destroy( p_mi );
        return NULL;
    }

    /* will be released in media_player_release() */
    vlc_object_hold( p_input );

    p_mi->p_input_thread = p_input;
    p_mi->b_own_its_input_thread = false;

    return p_mi;
}

/**************************************************************************
 * Destroy a Media Instance object (libvlc internal)
 *
 * Warning: No lock held here, but hey, this is internal.
 **************************************************************************/
void libvlc_media_player_destroy( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    libvlc_exception_t p_e;

    libvlc_exception_init( &p_e );

    if( !p_mi )
        return;

	/* Detach Callback from the main libvlc object */
    var_DelCallback( p_mi->p_libvlc_instance->p_libvlc_int, "vout-snapshottaken", SnapshotTakenCallback, p_mi );

    p_input_thread = libvlc_get_input_thread( p_mi, &p_e );

    if( libvlc_exception_raised( &p_e ) )
    {
        libvlc_event_manager_release( p_mi->p_event_manager );
        libvlc_exception_clear( &p_e );
        free( p_mi );
        return; /* no need to worry about no input thread */
    }
    vlc_mutex_destroy( &p_mi->object_lock );

    vlc_object_release( p_input_thread );

    libvlc_media_release( p_mi->p_md );

    free( p_mi );
}

/**************************************************************************
 * Release a Media Instance object
 **************************************************************************/
void libvlc_media_player_release( libvlc_media_player_t *p_mi )
{
    if( !p_mi )
        return;

    vlc_mutex_lock( &p_mi->object_lock );

    p_mi->i_refcount--;

    if( p_mi->i_refcount > 0 )
    {
        vlc_mutex_unlock( &p_mi->object_lock );
        return;
    }
    vlc_mutex_unlock( &p_mi->object_lock );
    vlc_mutex_destroy( &p_mi->object_lock );

    release_input_thread( p_mi );

    libvlc_event_manager_release( p_mi->p_event_manager );

    libvlc_media_release( p_mi->p_md );

    free( p_mi );
}

/**************************************************************************
 * Retain a Media Instance object
 **************************************************************************/
void libvlc_media_player_retain( libvlc_media_player_t *p_mi )
{
    if( !p_mi )
        return;

    p_mi->i_refcount++;
}

/**************************************************************************
 * Set the Media descriptor associated with the instance
 **************************************************************************/
void libvlc_media_player_set_media(
                            libvlc_media_player_t *p_mi,
                            libvlc_media_t *p_md,
                            libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    if( !p_mi )
        return;

    vlc_mutex_lock( &p_mi->object_lock );

    release_input_thread( p_mi );

    if( p_mi->p_md )
        libvlc_media_set_state( p_mi->p_md, libvlc_NothingSpecial, p_e );

    libvlc_media_release( p_mi->p_md );

    if( !p_md )
    {
        p_mi->p_md = NULL;
        vlc_mutex_unlock( &p_mi->object_lock );
        return; /* It is ok to pass a NULL md */
    }

    libvlc_media_retain( p_md );
    p_mi->p_md = p_md;

    /* The policy here is to ignore that we were created using a different
     * libvlc_instance, because we don't really care */
    p_mi->p_libvlc_instance = p_md->p_libvlc_instance;

    vlc_mutex_unlock( &p_mi->object_lock );
}

/**************************************************************************
 * Get the Media descriptor associated with the instance
 **************************************************************************/
libvlc_media_t *
libvlc_media_player_get_media(
                            libvlc_media_player_t *p_mi,
                            libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    if( !p_mi->p_md )
        return NULL;

    libvlc_media_retain( p_mi->p_md );
    return p_mi->p_md;
}

/**************************************************************************
 * Get the event Manager
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_player_event_manager(
                            libvlc_media_player_t *p_mi,
                            libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    return p_mi->p_event_manager;
}

/**************************************************************************
 * Trigger a snapshot Taken Event
 *************************************************************************/
static int SnapshotTakenCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(p_this) ;

    libvlc_media_player_t* p_mi = (libvlc_media_player_t*) p_data ;
    libvlc_event_t event ;
    event.type = libvlc_MediaPlayerSnapshotTaken ;
    event.u.media_player_snapshot_taken.psz_filename = newval.psz_string ;
    /* Snapshot psz data is a vlc_variable owned by libvlc object .
         Its memmory management is taken care by the obj*/
    msg_Dbg( p_this, "about to emit libvlc_snapshot_taken.make psz_str=0x%p (%s)",
          event.u.media_player_snapshot_taken.psz_filename ,event.u.media_player_snapshot_taken.psz_filename );
    libvlc_event_send( p_mi->p_event_manager, &event );

    return VLC_SUCCESS;
}

/**************************************************************************
 * Play
 **************************************************************************/
void libvlc_media_player_play( libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t * p_input_thread;

    if( (p_input_thread = libvlc_get_input_thread( p_mi, p_e )) )
    {
        /* A thread already exists, send it a play message */
        input_Control( p_input_thread, INPUT_SET_STATE, PLAYING_S );
        vlc_object_release( p_input_thread );
        return;
    }

    /* Ignore previous exception */
    libvlc_exception_clear( p_e );

    vlc_mutex_lock( &p_mi->object_lock );

    if( !p_mi->p_md )
    {
        libvlc_exception_raise( p_e, "no associated media descriptor" );
        vlc_mutex_unlock( &p_mi->object_lock );
        return;
    }

    p_mi->p_input_thread = input_CreateThread( p_mi->p_libvlc_instance->p_libvlc_int,
                      p_mi->p_md->p_input_item );


    if( !p_mi->p_input_thread )
    {
        vlc_mutex_unlock( &p_mi->object_lock );
        return;
    }

    p_input_thread = p_mi->p_input_thread;

    var_Create( p_input_thread, "drawable-xid", VLC_VAR_INTEGER );
    if( p_mi->drawable.xid )
        var_SetInteger( p_input_thread, "drawable-xid", p_mi->drawable.xid );

    var_Create( p_input_thread, "drawable-hwnd", VLC_VAR_ADDRESS );
    if( p_mi->drawable.hwnd != NULL )
    {
        vlc_value_t val = { .p_address = p_mi->drawable.hwnd };
        var_Set( p_input_thread, "drawable-hwnd", val );
    }

    var_AddCallback( p_input_thread, "can-seek", input_seekable_changed, p_mi );
    var_AddCallback( p_input_thread, "can-pause", input_pausable_changed, p_mi );
    var_AddCallback( p_input_thread, "intf-event", input_event_changed, p_mi );

    vlc_mutex_unlock( &p_mi->object_lock );
}

/**************************************************************************
 * Pause
 **************************************************************************/
void libvlc_media_player_pause( libvlc_media_player_t *p_mi,
                                  libvlc_exception_t *p_e )
{
    input_thread_t * p_input_thread = libvlc_get_input_thread( p_mi, p_e );

    if( !p_input_thread )
        return;

    libvlc_state_t state = libvlc_media_player_get_state( p_mi, p_e );

    if( state == libvlc_Playing )
    {
        if( libvlc_media_player_can_pause( p_mi, p_e ) )
            input_Control( p_input_thread, INPUT_SET_STATE, PAUSE_S );
        else
            libvlc_media_player_stop( p_mi, p_e );
    }
    else
        input_Control( p_input_thread, INPUT_SET_STATE, PLAYING_S );

    vlc_object_release( p_input_thread );
}

/**************************************************************************
 * is_playing
 **************************************************************************/
int libvlc_media_player_is_playing( libvlc_media_player_t *p_mi,
                                     libvlc_exception_t *p_e )
{
    input_thread_t * p_input_thread = libvlc_get_input_thread( p_mi, p_e );

    if( !p_input_thread )
        return 0;

    libvlc_state_t state = libvlc_media_player_get_state( p_mi, p_e );

    vlc_object_release( p_input_thread );
    
    if( state == libvlc_Playing )
    {
        return 1;
    }
    return 0;
}


/**************************************************************************
 * Stop
 **************************************************************************/
void libvlc_media_player_stop( libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    libvlc_state_t state = libvlc_media_player_get_state( p_mi, p_e );

    if( state == libvlc_Playing || state == libvlc_Paused )
    {
        /* Send a stop notification event only of we are in playing or paused states */
        libvlc_media_set_state( p_mi->p_md, libvlc_Ended, p_e );

        /* Construct and send the event */
        libvlc_event_t event;
        event.type = libvlc_MediaPlayerStopped;
        libvlc_event_send( p_mi->p_event_manager, &event );
    }

    if( p_mi->b_own_its_input_thread )
    {
        vlc_mutex_lock( &p_mi->object_lock );
        release_input_thread( p_mi ); /* This will stop the input thread */
        vlc_mutex_unlock( &p_mi->object_lock );
    }
    else
    {
        input_thread_t * p_input_thread = libvlc_get_input_thread( p_mi, p_e );

        if( !p_input_thread )
            return;

        input_StopThread( p_input_thread );
        vlc_object_release( p_input_thread );
    }
}

void libvlc_media_player_set_xwindow( libvlc_media_player_t *p_mi,
                                      uint32_t drawable,
                                      libvlc_exception_t *p_e )
{
    (void) p_e;
    p_mi->drawable.xid = drawable;
}

uint32_t libvlc_media_player_get_xwindow( libvlc_media_player_t *p_mi )
{
    return p_mi->drawable.xid;
}

void libvlc_media_player_set_hwnd( libvlc_media_player_t *p_mi,
                                   void *drawable,
                                   libvlc_exception_t *p_e )
{
    (void) p_e;
    p_mi->drawable.hwnd = drawable;
}

void *libvlc_media_player_get_hwnd( libvlc_media_player_t *p_mi )
{
    return p_mi->drawable.hwnd;
}

/**************************************************************************
 * Set Drawable
 **************************************************************************/
void libvlc_media_player_set_drawable( libvlc_media_player_t *p_mi,
                                       libvlc_drawable_t drawable,
                                       libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vout_thread_t *p_vout = NULL;

    p_mi->drawable.xid = drawable;

    /* Allow on the fly drawable changing. This is tricky has this may
     * not be supported by every vout. We though can't disable it
     * because of some creepy drawable type that are not flexible enough
     * (Win32 HWND for instance) */
    p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    if( !p_input_thread ) {
        /* No input, nothing more to do, we are fine */
        libvlc_exception_clear( p_e );
        return;
    }

    p_vout = vlc_object_find( p_input_thread, VLC_OBJECT_VOUT, FIND_CHILD );
    if( p_vout )
    {
        vout_Control( p_vout , VOUT_REPARENT, drawable);
        vlc_object_release( p_vout );
    }
    vlc_object_release( p_input_thread );
}

/**************************************************************************
 * Get Drawable
 **************************************************************************/
libvlc_drawable_t
libvlc_media_player_get_drawable ( libvlc_media_player_t *p_mi, libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);
    return p_mi->drawable.xid;
}

/**************************************************************************
 * Getters for stream information
 **************************************************************************/
libvlc_time_t libvlc_media_player_get_length(
                             libvlc_media_player_t *p_mi,
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

libvlc_time_t libvlc_media_player_get_time(
                                   libvlc_media_player_t *p_mi,
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

void libvlc_media_player_set_time(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_time_t time,
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

void libvlc_media_player_set_position(
                                libvlc_media_player_t *p_mi,
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

float libvlc_media_player_get_position(
                                 libvlc_media_player_t *p_mi,
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

void libvlc_media_player_set_chapter(
                                 libvlc_media_player_t *p_mi,
                                 int chapter,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;
    val.i_int = chapter;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return;

    var_Set( p_input_thread, "chapter", val );
    vlc_object_release( p_input_thread );
}

int libvlc_media_player_get_chapter(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1;

    var_Get( p_input_thread, "chapter", &val );
    vlc_object_release( p_input_thread );

    return val.i_int;
}

int libvlc_media_player_get_chapter_count(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );
    vlc_object_release( p_input_thread );

    return val.i_int;
}

int libvlc_media_player_get_chapter_count_for_title(
                                 libvlc_media_player_t *p_mi,
                                 int i_title,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1;

    char *psz_name;
    if( asprintf( &psz_name,  "title %2i", i_title ) == -1 )
    {
        vlc_object_release( p_input_thread );
        return -1;
    }
    var_Change( p_input_thread, psz_name, VLC_VAR_CHOICESCOUNT, &val, NULL );
    vlc_object_release( p_input_thread );
    free( psz_name );

    return val.i_int;
}

void libvlc_media_player_set_title(
                                 libvlc_media_player_t *p_mi,
                                 int i_title,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;
    val.i_int = i_title;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return;

    var_Set( p_input_thread, "title", val );
    vlc_object_release( p_input_thread );

    //send event
    libvlc_event_t event;
    event.type = libvlc_MediaPlayerTitleChanged;
    event.u.media_player_title_changed.new_title = i_title;
    libvlc_event_send( p_mi->p_event_manager, &event );
}

int libvlc_media_player_get_title(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1;

    var_Get( p_input_thread, "title", &val );
    vlc_object_release( p_input_thread );

    return val.i_int;
}

int libvlc_media_player_get_title_count(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
    vlc_object_release( p_input_thread );

    return val.i_int;
}

void libvlc_media_player_next_chapter(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return;

    int i_type = var_Type( p_input_thread, "next-chapter" );
    vlc_value_t val;
    val.b_bool = true;
    var_Set( p_input_thread, (i_type & VLC_VAR_TYPE) != 0 ?
                            "next-chapter":"next-title", val );

    vlc_object_release( p_input_thread );
}

void libvlc_media_player_previous_chapter(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return;

    int i_type = var_Type( p_input_thread, "next-chapter" );
    vlc_value_t val;
    val.b_bool = true;
    var_Set( p_input_thread, (i_type & VLC_VAR_TYPE) != 0 ?
                            "prev-chapter":"prev-title", val );

    vlc_object_release( p_input_thread );
}

float libvlc_media_player_get_fps(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e)
{
    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    double f_fps = 0.0;

    if( p_input_thread )
    {
        if( input_Control( p_input_thread, INPUT_GET_VIDEO_FPS, &f_fps ) )
            f_fps = 0.0;
        vlc_object_release( p_input_thread );
    }
    return f_fps;
}

int libvlc_media_player_will_play( libvlc_media_player_t *p_mi,
                                     libvlc_exception_t *p_e)
{
    input_thread_t *p_input_thread =
                            libvlc_get_input_thread ( p_mi, p_e);
    if ( !p_input_thread )
        return false;

    if ( !p_input_thread->b_die && !p_input_thread->b_dead )
    {
        vlc_object_release( p_input_thread );
        return true;
    }
    vlc_object_release( p_input_thread );
    return false;
}

void libvlc_media_player_set_rate(
                                 libvlc_media_player_t *p_mi,
                                 float rate,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;
    bool b_can_rewind;

    if( rate != 0 )
        RAISEVOID( "Rate value is invalid" );

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return;

    b_can_rewind = var_GetBool( p_input_thread, "can-rewind" );
    if( (rate < 0) && !b_can_rewind )
    {
        vlc_object_release( p_input_thread );
        libvlc_exception_raise( p_e, "Rate value is invalid" );
        return;
    }

    val.i_int = 1000.0f/rate;
    var_Set( p_input_thread, "rate", val );
    vlc_object_release( p_input_thread );
}

float libvlc_media_player_get_rate(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;
    bool b_can_rewind;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return 0.0;  /* rate < 0 indicates rewind */

    var_Get( p_input_thread, "rate", &val );
    b_can_rewind = var_GetBool( p_input_thread, "can-rewind" );
    if( (val.i_int < 0) && !b_can_rewind )
    {
        libvlc_exception_raise( p_e, "invalid rate" );
        return 0.0;
    }
    vlc_object_release( p_input_thread );

    return (float)1000.0f/val.i_int;
}

libvlc_state_t libvlc_media_player_get_state(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if ( !p_input_thread )
    {
        /* We do return the right value, no need to throw an exception */
        if( libvlc_exception_raised( p_e ) )
            libvlc_exception_clear( p_e );
        return libvlc_Ended;
    }

    var_Get( p_input_thread, "state", &val );
    vlc_object_release( p_input_thread );

    return vlc_to_libvlc_state(val.i_int);
}

int libvlc_media_player_is_seekable( libvlc_media_player_t *p_mi,
                                       libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if ( !p_input_thread )
    {
        /* We do return the right value, no need to throw an exception */
        if( libvlc_exception_raised( p_e ) )
            libvlc_exception_clear( p_e );
        return false;
    }
    var_Get( p_input_thread, "can-seek", &val );
    vlc_object_release( p_input_thread );

    return val.b_bool;
}

/* internal function, used by audio, video */
libvlc_track_description_t *
        libvlc_get_track_description( libvlc_media_player_t *p_mi,
                                      const char *psz_variable,
                                      libvlc_exception_t *p_e )
{
    input_thread_t *p_input = libvlc_get_input_thread( p_mi, p_e );

    if( !p_input )
        return NULL;

    vlc_value_t val_list, text_list;
    var_Change( p_input, psz_variable, VLC_VAR_GETLIST, &val_list, &text_list);

    if( val_list.p_list->i_count <= 0 ) /* no tracks */
        return NULL;

    libvlc_track_description_t *p_track_description, *p_actual, *p_previous;
    p_track_description = ( libvlc_track_description_t * )
        malloc( sizeof( libvlc_track_description_t ) );
    if ( !p_track_description )
    {
        var_Change( p_input, psz_variable, VLC_VAR_FREELIST, &val_list, &text_list);
        vlc_object_release( p_input );
        libvlc_exception_raise( p_e, "no enough memory" );
        return NULL;
    }
    p_actual = p_track_description;
    p_previous = NULL;
    for( int i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( !p_actual )
        {
            p_actual = ( libvlc_track_description_t * )
                malloc( sizeof( libvlc_track_description_t ) );
            if ( !p_actual )
            {
                libvlc_track_description_release( p_track_description );
                var_Change( p_input, psz_variable, VLC_VAR_FREELIST, &val_list, &text_list);
                vlc_object_release( p_input );
                libvlc_exception_raise( p_e, "no enough memory" );
                return NULL;
            }
        }
        p_actual->i_id = val_list.p_list->p_values[i].i_int;
        p_actual->psz_name = strdup( text_list.p_list->p_values[i].psz_string );
        p_actual->p_next = NULL;
        if( p_previous )
            p_previous->p_next = p_actual;
        p_previous = p_actual;
        p_actual =  NULL;
    }
    var_Change( p_input, psz_variable, VLC_VAR_FREELIST, &val_list, &text_list);
    vlc_object_release( p_input );

    return p_track_description;
}

void libvlc_track_description_release( libvlc_track_description_t *p_track_description )
{
    libvlc_track_description_t *p_actual, *p_before;
    p_actual = p_track_description;

    while ( p_actual )
    {
        free( p_actual->psz_name );
        p_before = p_actual;
        p_actual = p_before->p_next;
        free( p_before );
    }
}

int libvlc_media_player_can_pause( libvlc_media_player_t *p_mi,
                                     libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if ( !p_input_thread )
    {
        /* We do return the right value, no need to throw an exception */
        if( libvlc_exception_raised( p_e ) )
            libvlc_exception_clear( p_e );
        return false;
    }
    var_Get( p_input_thread, "can-pause", &val );
    vlc_object_release( p_input_thread );

    return val.b_bool;
}
