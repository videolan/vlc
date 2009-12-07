/*****************************************************************************
 * media_player.c: Libvlc API Media Instance management functions
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_events.h>

#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_vout.h>

#include "libvlc.h"

#include "libvlc_internal.h"
#include "media_internal.h" // libvlc_media_set_state()
#include "media_player_internal.h"

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

static int
snapshot_was_taken( vlc_object_t *p_this, char const *psz_cmd,
                    vlc_value_t oldval, vlc_value_t newval, void *p_data );

static void libvlc_media_player_destroy( libvlc_media_player_t *p_mi );

/*
 * Shortcuts
 */

#define register_event(a, b) __register_event(a, libvlc_MediaPlayer ## b)
static inline void __register_event(libvlc_media_player_t *mp, libvlc_event_type_t type)
{
    libvlc_event_manager_register_event_type(mp->p_event_manager, type, NULL);
}

static inline void lock(libvlc_media_player_t *mp)
{
    vlc_mutex_lock(&mp->object_lock);
}

static inline void unlock(libvlc_media_player_t *mp)
{
    vlc_mutex_unlock(&mp->object_lock);
}

static inline void clear_if_needed(libvlc_exception_t *e)
{
    if (libvlc_exception_raised(e))
        libvlc_exception_clear(e);
}

/*
 * Release the associated input thread.
 *
 * Object lock is NOT held.
 */
static void release_input_thread( libvlc_media_player_t *p_mi, bool b_input_abort )
{
    input_thread_t * p_input_thread;

    if( !p_mi || !p_mi->p_input_thread )
        return;

    p_input_thread = p_mi->p_input_thread;

    var_DelCallback( p_input_thread, "can-seek",
                     input_seekable_changed, p_mi );
    var_DelCallback( p_input_thread, "can-pause",
                    input_pausable_changed, p_mi );
    var_DelCallback( p_input_thread, "intf-event",
                     input_event_changed, p_mi );

    /* We owned this one */
    input_Stop( p_input_thread, b_input_abort );

    vlc_thread_join( p_input_thread );

    assert( p_mi->p_input_resource == NULL );
    assert( p_input_thread->b_dead );
    /* Store the input resource for future use. */
    p_mi->p_input_resource = input_DetachResource( p_input_thread );

    var_Destroy( p_input_thread, "drawable-hwnd" );
    var_Destroy( p_input_thread, "drawable-xid" );
    var_Destroy( p_input_thread, "drawable-agl" );

    vlc_object_release( p_input_thread );

    p_mi->p_input_thread = NULL;
}

/*
 * Retrieve the input thread. Be sure to release the object
 * once you are done with it. (libvlc Internal)
 *
 * Function will lock the object.
 */
input_thread_t *libvlc_get_input_thread( libvlc_media_player_t *p_mi,
                                         libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;

    if( !p_mi ) RAISENULL( "Media Instance is NULL" );

    lock(p_mi);

    if( !p_mi->p_input_thread )
    {
        unlock(p_mi);
        RAISENULL( "Input is NULL" );
    }

    p_input_thread = p_mi->p_input_thread;
    vlc_object_hold( p_input_thread );

    unlock(p_mi);

    return p_input_thread;
}

/*
 * Set the internal state of the media_player. (media player Internal)
 *
 * Function will lock the media_player.
 */
static void set_state( libvlc_media_player_t *p_mi, libvlc_state_t state )
{    
    lock(p_mi);
    p_mi->state = state;
    libvlc_media_t *media = p_mi->p_md;
    if (media)
        libvlc_media_retain(media);
    unlock(p_mi);


    if (media) {
        // Also set the state of the corresponding media
        // This is strictly for convenience.
        libvlc_media_set_state(media, state);
        
        libvlc_media_release(media);        
    }
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
        
        set_state( p_mi, libvlc_state );
        libvlc_event_send( p_mi->p_event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_ABORT )
    {
        libvlc_state_t libvlc_state = libvlc_Stopped;
        event.type = libvlc_MediaPlayerStopped;

        set_state( p_mi, libvlc_state );
        libvlc_event_send( p_mi->p_event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_POSITION )
    {
        if( var_GetInteger( p_input, "state" ) != PLAYING_S )
            return VLC_SUCCESS; /* Don't send the position while stopped */

        /* */
        event.type = libvlc_MediaPlayerPositionChanged;
        event.u.media_player_position_changed.new_position =
                                          var_GetFloat( p_input, "position" );
        libvlc_event_send( p_mi->p_event_manager, &event );

        /* */
        event.type = libvlc_MediaPlayerTimeChanged;
        event.u.media_player_time_changed.new_time =
                                               var_GetTime( p_input, "time" );
        libvlc_event_send( p_mi->p_event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_LENGTH )
    {
        event.type = libvlc_MediaPlayerLengthChanged;
        event.u.media_player_length_changed.new_length =
                                               var_GetTime( p_input, "length" );
        libvlc_event_send( p_mi->p_event_manager, &event );
    }

    return VLC_SUCCESS;

}

/**************************************************************************
 * Snapshot Taken Event.
 *
 * FIXME: This snapshot API interface makes no sense in media_player.
 *************************************************************************/
static int snapshot_was_taken(vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_this);

    libvlc_media_player_t *mp = p_data;
    libvlc_event_t event;
    event.type = libvlc_MediaPlayerSnapshotTaken;
    event.u.media_player_snapshot_taken.psz_filename = newval.psz_string;
    libvlc_event_send(mp->p_event_manager, &event);

    return VLC_SUCCESS;
}


static void libvlc_media_player_destroy( libvlc_media_player_t * );


/**************************************************************************
 * Create a Media Instance object.
 *
 * Refcount strategy:
 * - All items created by _new start with a refcount set to 1.
 * - Accessor _release decrease the refcount by 1, if after that
 *   operation the refcount is 0, the object is destroyed.
 * - Accessor _retain increase the refcount by 1 (XXX: to implement)
 *
 * Object locking strategy:
 * - No lock held while in constructor.
 * - When accessing any member variable this lock is held. (XXX who locks?)
 * - When attempting to destroy the object the lock is also held.
 **************************************************************************/
libvlc_media_player_t *
libvlc_media_player_new( libvlc_instance_t *instance, libvlc_exception_t *e )
{
    libvlc_media_player_t * mp;

    assert(instance);

    mp = malloc(sizeof(libvlc_media_player_t));
    if (!mp)
    {
        libvlc_exception_raise(e);
        libvlc_printerr("Not enough memory");
        return NULL;
    }
    mp->p_md = NULL;
    mp->drawable.agl = 0;
    mp->drawable.xid = 0;
    mp->drawable.hwnd = NULL;
    mp->drawable.nsobject = NULL;
    mp->keyboard_events = mp->mouse_events = 1;
    mp->state = libvlc_NothingSpecial;
    mp->p_libvlc_instance = instance;
    mp->p_input_thread = NULL;
    mp->p_input_resource = NULL;
    mp->i_refcount = 1;
    vlc_mutex_init(&mp->object_lock);
    mp->p_event_manager = libvlc_event_manager_new(mp, instance, e);
    if (libvlc_exception_raised(e))
    {
        vlc_mutex_destroy(&mp->object_lock);
        free(mp);
        return NULL;
    }

    register_event(mp, NothingSpecial);
    register_event(mp, Opening);
    register_event(mp, Buffering);
    register_event(mp, Playing);
    register_event(mp, Paused);
    register_event(mp, Stopped);
    register_event(mp, Forward);
    register_event(mp, Backward);
    register_event(mp, EndReached);
    register_event(mp, EncounteredError);

    register_event(mp, PositionChanged);
    register_event(mp, TimeChanged);
    register_event(mp, LengthChanged);
    register_event(mp, TitleChanged);
    register_event(mp, PausableChanged);

    /* Snapshot initialization */
    register_event(mp, SnapshotTaken);

    /* Attach a var callback to the global object to provide the glue between
     * vout_thread that generates the event and media_player that re-emits it
     * with its own event manager
     *
     * FIXME: It's unclear why we want to put this in public API, and why we
     * want to expose it in such a limiting and ugly way.
     */
    var_AddCallback(instance->p_libvlc_int, "snapshot-file", snapshot_was_taken, mp);

    return mp;
}

/**************************************************************************
 * Create a Media Instance object with a media descriptor.
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
 * Destroy a Media Instance object (libvlc internal)
 *
 * Warning: No lock held here, but hey, this is internal. Caller must lock.
 **************************************************************************/
static void libvlc_media_player_destroy( libvlc_media_player_t *p_mi )
{
    assert( p_mi );

    /* Detach Callback from the main libvlc object */
    var_DelCallback( p_mi->p_libvlc_instance->p_libvlc_int,
                     "snapshot-file", snapshot_was_taken, p_mi );

    /* If the input thread hasn't been already deleted it means
     * that the owners didn't stop the thread before releasing it. */
    assert(!p_mi->p_input_thread);

    /* Fallback for those who don't use NDEBUG */
    if (p_mi->p_input_thread)
        release_input_thread(p_mi, true);

    if( p_mi->p_input_resource )
    {
        input_resource_Delete( p_mi->p_input_resource );
        p_mi->p_input_resource = NULL;    
    }

    libvlc_event_manager_release( p_mi->p_event_manager );
    libvlc_media_release( p_mi->p_md );
    vlc_mutex_destroy( &p_mi->object_lock );
    free( p_mi );
}

/**************************************************************************
 * Release a Media Instance object.
 *
 * Function does the locking.
 **************************************************************************/
void libvlc_media_player_release( libvlc_media_player_t *p_mi )
{
    bool destroy;

    assert( p_mi );
    lock(p_mi);
    destroy = !--p_mi->i_refcount;
    unlock(p_mi);

    if( destroy )
        libvlc_media_player_destroy( p_mi );
}

/**************************************************************************
 * Retain a Media Instance object.
 *
 * Caller must hold the lock.
 **************************************************************************/
void libvlc_media_player_retain( libvlc_media_player_t *p_mi )
{
    assert( p_mi );

    lock(p_mi);
    p_mi->i_refcount++;
    unlock(p_mi);
}

/**************************************************************************
 * Set the Media descriptor associated with the instance.
 *
 * Enter without lock -- function will lock the object.
 **************************************************************************/
void libvlc_media_player_set_media(
                            libvlc_media_player_t *p_mi,
                            libvlc_media_t *p_md,
                            libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);

    lock(p_mi);

    /* FIXME I am not sure if it is a user request or on die(eof/error)
     * request here */
    release_input_thread( p_mi,
                          p_mi->p_input_thread &&
                          !p_mi->p_input_thread->b_eof &&
                          !p_mi->p_input_thread->b_error );
    unlock(p_mi);

    set_state( p_mi, libvlc_NothingSpecial );

    lock(p_mi);

    libvlc_media_release( p_mi->p_md );

    if( !p_md )
    {
        p_mi->p_md = NULL;
        unlock(p_mi);
        return; /* It is ok to pass a NULL md */
    }

    libvlc_media_retain( p_md );
    p_mi->p_md = p_md;

    /* The policy here is to ignore that we were created using a different
     * libvlc_instance, because we don't really care */
    p_mi->p_libvlc_instance = p_md->p_libvlc_instance;

    unlock(p_mi);
}

/**************************************************************************
 * Get the Media descriptor associated with the instance.
 **************************************************************************/
libvlc_media_t *
libvlc_media_player_get_media(
                            libvlc_media_player_t *p_mi,
                            libvlc_exception_t *p_e )
{
    libvlc_media_t *p_m;
    VLC_UNUSED(p_e);

    lock(p_mi);
    p_m = p_mi->p_md;
    if( p_m )
        libvlc_media_retain( p_mi->p_md );
    unlock(p_mi);
    return p_mi->p_md;
}

/**************************************************************************
 * Get the event Manager.
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
 * Tell media player to start playing.
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

    lock(p_mi);

    if( !p_mi->p_md )
    {
        unlock(p_mi);
        libvlc_exception_raise( p_e );
        libvlc_printerr( "No associated media descriptor" );
        return;
    }

    p_mi->p_input_thread = input_Create( p_mi->p_libvlc_instance->p_libvlc_int,
                                         p_mi->p_md->p_input_item, NULL, p_mi->p_input_resource );

    if( !p_mi->p_input_thread )
    {
        unlock(p_mi);
        return;
    }

    p_mi->p_input_resource = NULL;
    p_input_thread = p_mi->p_input_thread;

    var_Create( p_input_thread, "drawable-agl", VLC_VAR_INTEGER );
    if( p_mi->drawable.agl )
        var_SetInteger( p_input_thread, "drawable-agl", p_mi->drawable.agl );

    var_Create( p_input_thread, "drawable-xid", VLC_VAR_INTEGER );
    if( p_mi->drawable.xid )
        var_SetInteger( p_input_thread, "drawable-xid", p_mi->drawable.xid );

    var_Create( p_input_thread, "drawable-hwnd", VLC_VAR_ADDRESS );
    if( p_mi->drawable.hwnd != NULL )
        var_SetAddress( p_input_thread, "drawable-hwnd", p_mi->drawable.hwnd );

    var_Create( p_input_thread, "drawable-nsobject", VLC_VAR_ADDRESS );
    if( p_mi->drawable.nsobject != NULL )
        var_SetAddress( p_input_thread, "drawable-nsobject", p_mi->drawable.nsobject );

    var_Create( p_input_thread, "keyboard-events", VLC_VAR_BOOL );
    var_SetBool( p_input_thread, "keyboard-events", p_mi->keyboard_events );
    var_Create( p_input_thread, "mouse-events", VLC_VAR_BOOL );
    var_SetBool( p_input_thread, "mouse-events", p_mi->mouse_events );

    var_AddCallback( p_input_thread, "can-seek", input_seekable_changed, p_mi );
    var_AddCallback( p_input_thread, "can-pause", input_pausable_changed, p_mi );
    var_AddCallback( p_input_thread, "intf-event", input_event_changed, p_mi );

    if( input_Start( p_input_thread ) )
    {
        vlc_object_release( p_input_thread );
        p_mi->p_input_thread = NULL;
    }

    unlock(p_mi);
}

/**************************************************************************
 * Pause.
 **************************************************************************/
void libvlc_media_player_pause( libvlc_media_player_t *p_mi,
                                  libvlc_exception_t *p_e )
{
    input_thread_t * p_input_thread = libvlc_get_input_thread( p_mi, p_e );
    if( !p_input_thread )
        return;

    libvlc_state_t state = libvlc_media_player_get_state( p_mi, p_e );
    if( state == libvlc_Playing || state == libvlc_Buffering )
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
 * Tells whether the media player is currently playing.
 *
 * Enter with lock held.
 **************************************************************************/
int libvlc_media_player_is_playing( libvlc_media_player_t *p_mi,
                                     libvlc_exception_t *p_e )
{
    libvlc_state_t state = libvlc_media_player_get_state( p_mi, p_e );
    return (libvlc_Playing == state) || (libvlc_Buffering == state);
}

/**************************************************************************
 * Stop playing.
 **************************************************************************/
void libvlc_media_player_stop( libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    libvlc_state_t state = libvlc_media_player_get_state( p_mi, p_e );

    lock(p_mi);
    release_input_thread( p_mi, true ); /* This will stop the input thread */
    unlock(p_mi);

    /* Force to go to stopped state, in case we were in Ended, or Error
     * state. */
    if( state != libvlc_Stopped )
    {
        set_state( p_mi, libvlc_Stopped );

        /* Construct and send the event */
        libvlc_event_t event;
        event.type = libvlc_MediaPlayerStopped;
        libvlc_event_send( p_mi->p_event_manager, &event );
    }
}

/**************************************************************************
 * set_nsobject
 **************************************************************************/
void libvlc_media_player_set_nsobject( libvlc_media_player_t *p_mi,
                                        void * drawable,
                                        libvlc_exception_t *p_e )
{
    (void) p_e;
    p_mi->drawable.nsobject = drawable;
}

/**************************************************************************
 * get_nsobject
 **************************************************************************/
void * libvlc_media_player_get_nsobject( libvlc_media_player_t *p_mi )
{
    return p_mi->drawable.nsobject;
}

/**************************************************************************
 * set_agl
 **************************************************************************/
void libvlc_media_player_set_agl( libvlc_media_player_t *p_mi,
                                      uint32_t drawable,
                                      libvlc_exception_t *p_e )
{
    (void) p_e;
    p_mi->drawable.agl = drawable;
}

/**************************************************************************
 * get_agl
 **************************************************************************/
uint32_t libvlc_media_player_get_agl( libvlc_media_player_t *p_mi )
{
    return p_mi->drawable.agl;
}

/**************************************************************************
 * set_xwindow
 **************************************************************************/
void libvlc_media_player_set_xwindow( libvlc_media_player_t *p_mi,
                                      uint32_t drawable,
                                      libvlc_exception_t *p_e )
{
    (void) p_e;
    p_mi->drawable.xid = drawable;
}

/**************************************************************************
 * get_xwindow
 **************************************************************************/
uint32_t libvlc_media_player_get_xwindow( libvlc_media_player_t *p_mi )
{
    return p_mi->drawable.xid;
}

/**************************************************************************
 * set_hwnd
 **************************************************************************/
void libvlc_media_player_set_hwnd( libvlc_media_player_t *p_mi,
                                   void *drawable,
                                   libvlc_exception_t *p_e )
{
    (void) p_e;
    p_mi->drawable.hwnd = drawable;
}

/**************************************************************************
 * get_hwnd
 **************************************************************************/
void *libvlc_media_player_get_hwnd( libvlc_media_player_t *p_mi )
{
    return p_mi->drawable.hwnd;
}

/**************************************************************************
 * Getters for stream information
 **************************************************************************/
libvlc_time_t libvlc_media_player_get_length(
                             libvlc_media_player_t *p_mi,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    libvlc_time_t i_time;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return -1;

    i_time = var_GetTime( p_input_thread, "length" );
    vlc_object_release( p_input_thread );

    return (i_time+500LL)/1000LL;
}

libvlc_time_t libvlc_media_player_get_time(
                                   libvlc_media_player_t *p_mi,
                                   libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    libvlc_time_t i_time;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1;

    i_time = var_GetTime( p_input_thread , "time" );
    vlc_object_release( p_input_thread );
    return (i_time+500LL)/1000LL;
}

void libvlc_media_player_set_time(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_time_t i_time,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return;

    var_SetTime( p_input_thread, "time", i_time*1000LL );
    vlc_object_release( p_input_thread );
}

void libvlc_media_player_set_position(
                                libvlc_media_player_t *p_mi,
                                float position,
                                libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return;

    var_SetFloat( p_input_thread, "position", position );
    vlc_object_release( p_input_thread );
}

float libvlc_media_player_get_position(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    float f_position;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1.0;

    f_position = var_GetFloat( p_input_thread, "position" );
    vlc_object_release( p_input_thread );

    return f_position;
}

void libvlc_media_player_set_chapter(
                                 libvlc_media_player_t *p_mi,
                                 int chapter,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return;

    var_SetInteger( p_input_thread, "chapter", chapter );
    vlc_object_release( p_input_thread );
}

int libvlc_media_player_get_chapter(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    int i_chapter;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1;

    i_chapter = var_GetInteger( p_input_thread, "chapter" );
    vlc_object_release( p_input_thread );

    return i_chapter;
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

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e);
    if( !p_input_thread )
        return;

    var_SetInteger( p_input_thread, "title", i_title );
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
    int i_title;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return -1;

    i_title = var_GetInteger( p_input_thread, "title" );
    vlc_object_release( p_input_thread );

    return i_title;
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
    var_SetBool( p_input_thread, (i_type & VLC_VAR_TYPE) != 0 ?
                            "next-chapter":"next-title", true );

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
    var_SetBool( p_input_thread, (i_type & VLC_VAR_TYPE) != 0 ?
                            "prev-chapter":"prev-title", true );

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
    bool b_will_play;
    input_thread_t *p_input_thread =
                            libvlc_get_input_thread ( p_mi, p_e);
    if ( !p_input_thread )
        return false;

    b_will_play = !p_input_thread->b_die && !p_input_thread->b_dead;
    vlc_object_release( p_input_thread );

    return b_will_play;
}

void libvlc_media_player_set_rate(
                                 libvlc_media_player_t *p_mi,
                                 float rate,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    bool b_can_rewind;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return;

    b_can_rewind = var_GetBool( p_input_thread, "can-rewind" );
    if( (rate < 0.0) && !b_can_rewind )
    {
        vlc_object_release( p_input_thread );
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Invalid playback rate" );
        return;
    }

    var_SetFloat( p_input_thread, "rate", rate );
    vlc_object_release( p_input_thread );
}

float libvlc_media_player_get_rate(
                                 libvlc_media_player_t *p_mi,
                                 libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    float f_rate;
    bool b_can_rewind;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( !p_input_thread )
        return 0.0;  /* rate < 0 indicates rewind */

    f_rate = var_GetFloat( p_input_thread, "rate" );
    b_can_rewind = var_GetBool( p_input_thread, "can-rewind" );
    /* FIXME: why are negative values forbidden ?? (rewinding) */
    if( f_rate < 0 && !b_can_rewind )
    {
        vlc_object_release( p_input_thread );
        return 0.0;
    }
    vlc_object_release( p_input_thread );

    return f_rate;
}

libvlc_state_t libvlc_media_player_get_state( libvlc_media_player_t *p_mi, libvlc_exception_t *p_e )
{
    VLC_UNUSED(p_e);
    lock(p_mi);
    libvlc_state_t state = p_mi->state;
    unlock(p_mi);
    return state;
}

int libvlc_media_player_is_seekable( libvlc_media_player_t *p_mi,
                                       libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;
    bool b_seekable;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if ( !p_input_thread )
    {
        /* We do return the right value, no need to throw an exception */
        clear_if_needed(p_e);
        return false;
    }
    b_seekable = var_GetBool( p_input_thread, "can-seek" );
    vlc_object_release( p_input_thread );

    return b_seekable;
}

/* internal function, used by audio, video */
libvlc_track_description_t *
        libvlc_get_track_description( libvlc_media_player_t *p_mi,
                                      const char *psz_variable,
                                      libvlc_exception_t *p_e )
{
    input_thread_t *p_input = libvlc_get_input_thread( p_mi, p_e );
    libvlc_track_description_t *p_track_description = NULL,
                               *p_actual, *p_previous;

    if( !p_input )
        return NULL;

    vlc_value_t val_list, text_list;
    var_Change( p_input, psz_variable, VLC_VAR_GETLIST, &val_list, &text_list);

    /* no tracks */
    if( val_list.p_list->i_count <= 0 )
        goto end;

    p_track_description = ( libvlc_track_description_t * )
        malloc( sizeof( libvlc_track_description_t ) );
    if ( !p_track_description )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Not enough memory" );
        goto end;
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
                libvlc_exception_raise( p_e );
                libvlc_printerr( "Not enough memory" );
                goto end;
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

end:
    var_FreeList( &val_list, &text_list );
    vlc_object_release( p_input );

    return p_track_description;
}

void libvlc_track_description_release( libvlc_track_description_t *p_td )
{
    libvlc_track_description_t *p_actual, *p_before;
    p_actual = p_td;

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
    bool b_can_pause;

    p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if ( !p_input_thread )
    {
        /* We do return the right value, no need to throw an exception */
        clear_if_needed(p_e);
        return false;
    }
    b_can_pause = var_GetBool( p_input_thread, "can-pause" );
    vlc_object_release( p_input_thread );

    return b_can_pause;
}

void libvlc_media_player_next_frame( libvlc_media_player_t *p_mi, libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi, p_e );
    if( p_input_thread != NULL )
    {
        var_TriggerCallback( p_input_thread, "frame-next" );
        vlc_object_release( p_input_thread );
    }
    else
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "No active input" );
    }
}
