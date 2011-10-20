/*****************************************************************************
 * media_player.c: Libvlc API Media Instance management functions
 *****************************************************************************
 * Copyright (C) 2005-2011 the VideoLAN team
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
#include <vlc_keys.h>

#include "libvlc_internal.h"
#include "media_internal.h" // libvlc_media_set_state()
#include "media_player_internal.h"

/*
 * mapping of libvlc_navigate_mode_t to vlc_action_t
 */
static const vlc_action_t libvlc_navigate_to_action[] =
{
    ACTIONID_NAV_ACTIVATE,
    ACTIONID_NAV_UP,
    ACTIONID_NAV_DOWN,
    ACTIONID_NAV_LEFT,
    ACTIONID_NAV_RIGHT
};

static const uint32_t libvlc_navigate_to_action_size =                        \
  sizeof( libvlc_navigate_to_action ) / sizeof( libvlc_navigate_to_action[0] );


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
    libvlc_event_manager_register_event_type(mp->p_event_manager, type);
}

/*
 * The input lock protects the input and input resource pointer.
 * It MUST NOT be used from callbacks.
 *
 * The object lock protects the reset, namely the media and the player state.
 * It can, and usually needs to be taken from callbacks.
 * The object lock can be acquired under the input lock... and consequently
 * the opposite order is STRICTLY PROHIBITED.
 */
static inline void lock(libvlc_media_player_t *mp)
{
    vlc_mutex_lock(&mp->object_lock);
}

static inline void unlock(libvlc_media_player_t *mp)
{
    vlc_mutex_unlock(&mp->object_lock);
}

static inline void lock_input(libvlc_media_player_t *mp)
{
    vlc_mutex_lock(&mp->input.lock);
}

static inline void unlock_input(libvlc_media_player_t *mp)
{
    vlc_mutex_unlock(&mp->input.lock);
}

/*
 * Release the associated input thread.
 *
 * Object lock is NOT held.
 * Input lock is held or instance is being destroyed.
 */
static void release_input_thread( libvlc_media_player_t *p_mi, bool b_input_abort )
{
    assert( p_mi );

    input_thread_t *p_input_thread = p_mi->input.p_thread;
    if( !p_input_thread )
        return;
    p_mi->input.p_thread = NULL;

    var_DelCallback( p_input_thread, "can-seek",
                     input_seekable_changed, p_mi );
    var_DelCallback( p_input_thread, "can-pause",
                    input_pausable_changed, p_mi );
    var_DelCallback( p_input_thread, "intf-event",
                     input_event_changed, p_mi );

    /* We owned this one */
    input_Stop( p_input_thread, b_input_abort );
    input_Close( p_input_thread );
}

/*
 * Retrieve the input thread. Be sure to release the object
 * once you are done with it. (libvlc Internal)
 */
input_thread_t *libvlc_get_input_thread( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;

    assert( p_mi );

    lock_input(p_mi);
    p_input_thread = p_mi->input.p_thread;
    if( p_input_thread )
        vlc_object_hold( p_input_thread );
    else
        libvlc_printerr( "No active input" );
    unlock_input(p_mi);

    return p_input_thread;
}

/*
 * Set the internal state of the media_player. (media player Internal)
 *
 * Function will lock the media_player.
 */
static void set_state( libvlc_media_player_t *p_mi, libvlc_state_t state,
    bool b_locked )
{
    if(!b_locked)
        lock(p_mi);
    p_mi->state = state;

    libvlc_media_t *media = p_mi->p_md;
    if (media)
        libvlc_media_retain(media);

    if(!b_locked)
        unlock(p_mi);

    if (media)
    {
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

        set_state( p_mi, libvlc_state, false );
        libvlc_event_send( p_mi->p_event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_ABORT )
    {
        libvlc_state_t libvlc_state = libvlc_Stopped;
        event.type = libvlc_MediaPlayerStopped;

        set_state( p_mi, libvlc_state, false );
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
           from_mtime(var_GetTime( p_input, "time" ));
        libvlc_event_send( p_mi->p_event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_LENGTH )
    {
        event.type = libvlc_MediaPlayerLengthChanged;
        event.u.media_player_length_changed.new_length =
           from_mtime(var_GetTime( p_input, "length" ));
        libvlc_event_send( p_mi->p_event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_CACHE )
    {
        event.type = libvlc_MediaPlayerBuffering;
        event.u.media_player_buffering.new_cache = (int)(100 *
            var_GetFloat( p_input, "cache" ));
        libvlc_event_send( p_mi->p_event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_VOUT )
    {
        vout_thread_t **pp_vout;
        size_t i_vout;
        if( input_Control( p_input, INPUT_GET_VOUTS, &pp_vout, &i_vout ) )
        {
            i_vout  = 0;
        }
        else
        {
            for( size_t i = 0; i < i_vout; i++ )
                vlc_object_release( pp_vout[i] );
            free( pp_vout );
        }

        event.type = libvlc_MediaPlayerVout;
        event.u.media_player_vout.new_count = i_vout;
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

static input_thread_t *find_input (vlc_object_t *obj)
{
    libvlc_media_player_t *mp = (libvlc_media_player_t *)obj;

    return libvlc_get_input_thread (mp);
}

/* */
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
libvlc_media_player_new( libvlc_instance_t *instance )
{
    libvlc_media_player_t * mp;

    assert(instance);

    mp = vlc_object_create (instance->p_libvlc_int, sizeof(*mp));
    if (unlikely(mp == NULL))
    {
        libvlc_printerr("Not enough memory");
        return NULL;
    }

    /* Input */
    var_Create (mp, "rate", VLC_VAR_FLOAT|VLC_VAR_DOINHERIT);

    /* Video */
    var_Create (mp, "vout", VLC_VAR_STRING|VLC_VAR_DOINHERIT);
    var_Create (mp, "window", VLC_VAR_STRING);
    var_Create (mp, "vmem-lock", VLC_VAR_ADDRESS);
    var_Create (mp, "vmem-unlock", VLC_VAR_ADDRESS);
    var_Create (mp, "vmem-display", VLC_VAR_ADDRESS);
    var_Create (mp, "vmem-data", VLC_VAR_ADDRESS);
    var_Create (mp, "vmem-setup", VLC_VAR_ADDRESS);
    var_Create (mp, "vmem-cleanup", VLC_VAR_ADDRESS);
    var_Create (mp, "vmem-chroma", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create (mp, "vmem-width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "vmem-height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "vmem-pitch", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "drawable-xid", VLC_VAR_INTEGER);
#if defined (WIN32) || defined (__OS2__)
    var_Create (mp, "drawable-hwnd", VLC_VAR_INTEGER);
#endif
#ifdef __APPLE__
    var_Create (mp, "drawable-agl", VLC_VAR_INTEGER);
    var_Create (mp, "drawable-nsobject", VLC_VAR_ADDRESS);
#endif

    var_Create (mp, "keyboard-events", VLC_VAR_BOOL);
    var_SetBool (mp, "keyboard-events", true);
    var_Create (mp, "mouse-events", VLC_VAR_BOOL);
    var_SetBool (mp, "mouse-events", true);

    var_Create (mp, "fullscreen", VLC_VAR_BOOL);
    var_Create (mp, "autoscale", VLC_VAR_BOOL);
    var_SetBool (mp, "autoscale", true);
    var_Create (mp, "scale", VLC_VAR_FLOAT);
    var_SetFloat (mp, "scale", 1.);
    var_Create (mp, "aspect-ratio", VLC_VAR_STRING);
    var_Create (mp, "crop", VLC_VAR_STRING);
    var_Create (mp, "deinterlace", VLC_VAR_INTEGER);
    var_Create (mp, "deinterlace-mode", VLC_VAR_STRING);

    var_Create (mp, "vbi-page", VLC_VAR_INTEGER);

    var_Create (mp, "marq-marquee", VLC_VAR_STRING);
    var_Create (mp, "marq-color", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "marq-opacity", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "marq-position", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "marq-refresh", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "marq-size", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "marq-timeout", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "marq-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "marq-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);

    var_Create (mp, "logo-file", VLC_VAR_STRING);
    var_Create (mp, "logo-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "logo-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "logo-delay", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "logo-repeat", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "logo-opacity", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "logo-position", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);

    var_Create (mp, "contrast", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    var_Create (mp, "brightness", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    var_Create (mp, "hue", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "saturation", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    var_Create (mp, "gamma", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);

     /* Audio */
    var_Create (mp, "aout", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create (mp, "mute", VLC_VAR_BOOL);
    var_Create (mp, "volume", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "find-input-callback", VLC_VAR_ADDRESS);
    var_SetAddress (mp, "find-input-callback", find_input);
    var_Create (mp, "amem-data", VLC_VAR_ADDRESS);
    var_Create (mp, "amem-setup", VLC_VAR_ADDRESS);
    var_Create (mp, "amem-cleanup", VLC_VAR_ADDRESS);
    var_Create (mp, "amem-play", VLC_VAR_ADDRESS);
    var_Create (mp, "amem-pause", VLC_VAR_ADDRESS);
    var_Create (mp, "amem-resume", VLC_VAR_ADDRESS);
    var_Create (mp, "amem-flush", VLC_VAR_ADDRESS);
    var_Create (mp, "amem-drain", VLC_VAR_ADDRESS);
    var_Create (mp, "amem-set-volume", VLC_VAR_ADDRESS);
    var_Create (mp, "amem-format", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create (mp, "amem-rate", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "amem-channels", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);

    mp->p_md = NULL;
    mp->state = libvlc_NothingSpecial;
    mp->p_libvlc_instance = instance;
    mp->input.p_thread = NULL;
    mp->input.p_resource = NULL;
    vlc_mutex_init (&mp->input.lock);
    mp->i_refcount = 1;
    mp->p_event_manager = libvlc_event_manager_new(mp, instance);
    if (unlikely(mp->p_event_manager == NULL))
    {
        vlc_object_release(mp);
        return NULL;
    }
    vlc_mutex_init(&mp->object_lock);

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
    register_event(mp, SeekableChanged);

    register_event(mp, PositionChanged);
    register_event(mp, TimeChanged);
    register_event(mp, LengthChanged);
    register_event(mp, TitleChanged);
    register_event(mp, PausableChanged);

    register_event(mp, Vout);

    /* Snapshot initialization */
    register_event(mp, SnapshotTaken);

    register_event(mp, MediaChanged);

    /* Attach a var callback to the global object to provide the glue between
     * vout_thread that generates the event and media_player that re-emits it
     * with its own event manager
     *
     * FIXME: It's unclear why we want to put this in public API, and why we
     * want to expose it in such a limiting and ugly way.
     */
    var_AddCallback(mp->p_libvlc, "snapshot-file", snapshot_was_taken, mp);

    libvlc_retain(instance);
    return mp;
}

/**************************************************************************
 * Create a Media Instance object with a media descriptor.
 **************************************************************************/
libvlc_media_player_t *
libvlc_media_player_new_from_media( libvlc_media_t * p_md )
{
    libvlc_media_player_t * p_mi;

    p_mi = libvlc_media_player_new( p_md->p_libvlc_instance );
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
    var_DelCallback( p_mi->p_libvlc,
                     "snapshot-file", snapshot_was_taken, p_mi );

    /* No need for lock_input() because no other threads knows us anymore */
    if( p_mi->input.p_thread )
        release_input_thread(p_mi, true);
    if( p_mi->input.p_resource )
    {
        input_resource_Terminate( p_mi->input.p_resource );
        input_resource_Release( p_mi->input.p_resource );
        p_mi->input.p_resource = NULL;
    }
    vlc_mutex_destroy( &p_mi->input.lock );

    libvlc_event_manager_release( p_mi->p_event_manager );
    libvlc_media_release( p_mi->p_md );
    vlc_mutex_destroy( &p_mi->object_lock );

    libvlc_instance_t *instance = p_mi->p_libvlc_instance;
    vlc_object_release( p_mi );
    libvlc_release(instance);
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
                            libvlc_media_t *p_md )
{
    lock_input(p_mi);

    /* FIXME I am not sure if it is a user request or on die(eof/error)
     * request here */
    release_input_thread( p_mi,
                          p_mi->input.p_thread &&
                          !p_mi->input.p_thread->b_eof &&
                          !p_mi->input.p_thread->b_error );

    lock( p_mi );
    set_state( p_mi, libvlc_NothingSpecial, true );
    unlock_input( p_mi );

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

    /* Send an event for the newly available media */
    libvlc_event_t event;
    event.type = libvlc_MediaPlayerMediaChanged;
    event.u.media_player_media_changed.new_media = p_md;
    libvlc_event_send( p_mi->p_event_manager, &event );

}

/**************************************************************************
 * Get the Media descriptor associated with the instance.
 **************************************************************************/
libvlc_media_t *
libvlc_media_player_get_media( libvlc_media_player_t *p_mi )
{
    libvlc_media_t *p_m;

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
libvlc_media_player_event_manager( libvlc_media_player_t *p_mi )
{
    return p_mi->p_event_manager;
}

/**************************************************************************
 * Tell media player to start playing.
 **************************************************************************/
int libvlc_media_player_play( libvlc_media_player_t *p_mi )
{
    lock_input( p_mi );

    input_thread_t *p_input_thread = p_mi->input.p_thread;
    if( p_input_thread )
    {
        /* A thread already exists, send it a play message */
        input_Control( p_input_thread, INPUT_SET_STATE, PLAYING_S );
        unlock_input( p_mi );
        return 0;
    }

    /* Ignore previous exception */
    lock(p_mi);

    if( !p_mi->p_md )
    {
        unlock(p_mi);
        unlock_input( p_mi );
        libvlc_printerr( "No associated media descriptor" );
        return -1;
    }

    if( !p_mi->input.p_resource )
        p_mi->input.p_resource = input_resource_New( VLC_OBJECT( p_mi ) );
    p_input_thread = input_Create( p_mi, p_mi->p_md->p_input_item, NULL,
                                   p_mi->input.p_resource );
    unlock(p_mi);
    if( !p_input_thread )
    {
        unlock_input(p_mi);
        libvlc_printerr( "Not enough memory" );
        return -1;
    }

    var_AddCallback( p_input_thread, "can-seek", input_seekable_changed, p_mi );
    var_AddCallback( p_input_thread, "can-pause", input_pausable_changed, p_mi );
    var_AddCallback( p_input_thread, "intf-event", input_event_changed, p_mi );

    if( input_Start( p_input_thread ) )
    {
        unlock_input(p_mi);
        var_DelCallback( p_input_thread, "intf-event", input_event_changed, p_mi );
        var_DelCallback( p_input_thread, "can-pause", input_pausable_changed, p_mi );
        var_DelCallback( p_input_thread, "can-seek", input_seekable_changed, p_mi );
        vlc_object_release( p_input_thread );
        libvlc_printerr( "Input initialization failure" );
        return -1;
    }
    p_mi->input.p_thread = p_input_thread;
    unlock_input(p_mi);
    return 0;
}

void libvlc_media_player_set_pause( libvlc_media_player_t *p_mi, int paused )
{
    input_thread_t * p_input_thread = libvlc_get_input_thread( p_mi );
    if( !p_input_thread )
        return;

    libvlc_state_t state = libvlc_media_player_get_state( p_mi );
    if( state == libvlc_Playing || state == libvlc_Buffering )
    {
        if( paused )
        {
            if( libvlc_media_player_can_pause( p_mi ) )
                input_Control( p_input_thread, INPUT_SET_STATE, PAUSE_S );
            else
                libvlc_media_player_stop( p_mi );
        }
    }
    else
    {
        if( !paused )
            input_Control( p_input_thread, INPUT_SET_STATE, PLAYING_S );
    }

    vlc_object_release( p_input_thread );
}

/**************************************************************************
 * Toggle pause.
 **************************************************************************/
void libvlc_media_player_pause( libvlc_media_player_t *p_mi )
{
    libvlc_state_t state = libvlc_media_player_get_state( p_mi );
    bool playing = (state == libvlc_Playing || state == libvlc_Buffering);

    libvlc_media_player_set_pause( p_mi, playing );
}

/**************************************************************************
 * Tells whether the media player is currently playing.
 *
 * Enter with lock held.
 **************************************************************************/
int libvlc_media_player_is_playing( libvlc_media_player_t *p_mi )
{
    libvlc_state_t state = libvlc_media_player_get_state( p_mi );
    return (libvlc_Playing == state) || (libvlc_Buffering == state);
}

/**************************************************************************
 * Stop playing.
 **************************************************************************/
void libvlc_media_player_stop( libvlc_media_player_t *p_mi )
{
    libvlc_state_t state = libvlc_media_player_get_state( p_mi );

    lock_input(p_mi);
    release_input_thread( p_mi, true ); /* This will stop the input thread */

    /* Force to go to stopped state, in case we were in Ended, or Error
     * state. */
    if( state != libvlc_Stopped )
    {
        set_state( p_mi, libvlc_Stopped, false );

        /* Construct and send the event */
        libvlc_event_t event;
        event.type = libvlc_MediaPlayerStopped;
        libvlc_event_send( p_mi->p_event_manager, &event );
    }

    if( p_mi->input.p_resource != NULL )
        input_resource_Terminate( p_mi->input.p_resource );
    unlock_input(p_mi);
}


void libvlc_video_set_callbacks( libvlc_media_player_t *mp,
    void *(*lock_cb) (void *, void **),
    void (*unlock_cb) (void *, void *, void *const *),
    void (*display_cb) (void *, void *),
    void *opaque )
{
    var_SetAddress( mp, "vmem-lock", lock_cb );
    var_SetAddress( mp, "vmem-unlock", unlock_cb );
    var_SetAddress( mp, "vmem-display", display_cb );
    var_SetAddress( mp, "vmem-data", opaque );
    var_SetString( mp, "vout", "vmem" );
}

void libvlc_video_set_format_callbacks( libvlc_media_player_t *mp,
                                        libvlc_video_format_cb setup,
                                        libvlc_video_cleanup_cb cleanup )
{
    var_SetAddress( mp, "vmem-setup", setup );
    var_SetAddress( mp, "vmem-cleanup", cleanup );
}

void libvlc_video_set_format( libvlc_media_player_t *mp, const char *chroma,
                              unsigned width, unsigned height, unsigned pitch )
{
    var_SetString( mp, "vmem-chroma", chroma );
    var_SetInteger( mp, "vmem-width", width );
    var_SetInteger( mp, "vmem-height", height );
    var_SetInteger( mp, "vmem-pitch", pitch );
}

/**************************************************************************
 * set_nsobject
 **************************************************************************/
void libvlc_media_player_set_nsobject( libvlc_media_player_t *p_mi,
                                        void * drawable )
{
    assert (p_mi != NULL);
#ifdef __APPLE__
    var_SetAddress (p_mi, "drawable-nsobject", drawable);
#else
    (void) p_mi; (void)drawable;
#endif
}

/**************************************************************************
 * get_nsobject
 **************************************************************************/
void * libvlc_media_player_get_nsobject( libvlc_media_player_t *p_mi )
{
    assert (p_mi != NULL);
#ifdef __APPLE__
    return var_GetAddress (p_mi, "drawable-nsobject");
#else
    return NULL;
#endif
}

/**************************************************************************
 * set_agl
 **************************************************************************/
void libvlc_media_player_set_agl( libvlc_media_player_t *p_mi,
                                  uint32_t drawable )
{
#ifdef __APPLE__
    var_SetInteger (p_mi, "drawable-agl", drawable);
#else
    (void) p_mi; (void)drawable;
#endif
}

/**************************************************************************
 * get_agl
 **************************************************************************/
uint32_t libvlc_media_player_get_agl( libvlc_media_player_t *p_mi )
{
    assert (p_mi != NULL);
#ifdef __APPLE__
    return var_GetInteger (p_mi, "drawable-agl");
#else
    return 0;
#endif
}

/**************************************************************************
 * set_xwindow
 **************************************************************************/
void libvlc_media_player_set_xwindow( libvlc_media_player_t *p_mi,
                                      uint32_t drawable )
{
    assert (p_mi != NULL);

    var_SetString (p_mi, "vout", drawable ? "xid" : "any");
    var_SetString (p_mi, "window", drawable ? "embed-xid,any" : "any");
    var_SetInteger (p_mi, "drawable-xid", drawable);
}

/**************************************************************************
 * get_xwindow
 **************************************************************************/
uint32_t libvlc_media_player_get_xwindow( libvlc_media_player_t *p_mi )
{
    return var_GetInteger (p_mi, "drawable-xid");
}

/**************************************************************************
 * set_hwnd
 **************************************************************************/
void libvlc_media_player_set_hwnd( libvlc_media_player_t *p_mi,
                                   void *drawable )
{
    assert (p_mi != NULL);
#if defined (WIN32) || defined (__OS2__)
    var_SetString (p_mi, "window",
                   (drawable != NULL) ? "embed-hwnd,any" : "");
    var_SetInteger (p_mi, "drawable-hwnd", (uintptr_t)drawable);
#else
    (void) p_mi; (void) drawable;
#endif
}

/**************************************************************************
 * get_hwnd
 **************************************************************************/
void *libvlc_media_player_get_hwnd( libvlc_media_player_t *p_mi )
{
    assert (p_mi != NULL);
#if defined (WIN32) || defined (__OS2__)
    return (void *)(uintptr_t)var_GetInteger (p_mi, "drawable-hwnd");
#else
    return NULL;
#endif
}

void libvlc_audio_set_callbacks( libvlc_media_player_t *mp,
                                 libvlc_audio_play_cb play_cb,
                                 libvlc_audio_pause_cb pause_cb,
                                 libvlc_audio_resume_cb resume_cb,
                                 libvlc_audio_flush_cb flush_cb,
                                 libvlc_audio_drain_cb drain_cb,
                                 void *opaque )
{
    var_SetAddress( mp, "amem-play", play_cb );
    var_SetAddress( mp, "amem-pause", pause_cb );
    var_SetAddress( mp, "amem-resume", resume_cb );
    var_SetAddress( mp, "amem-flush", flush_cb );
    var_SetAddress( mp, "amem-drain", drain_cb );
    var_SetAddress( mp, "amem-data", opaque );
    var_SetString( mp, "aout", "amem,none" );
}

void libvlc_audio_set_volume_callback( libvlc_media_player_t *mp,
                                       libvlc_audio_set_volume_cb cb )
{
    var_SetAddress( mp, "amem-set-volume", cb );
}

void libvlc_audio_set_format_callbacks( libvlc_media_player_t *mp,
                                        libvlc_audio_setup_cb setup,
                                        libvlc_audio_cleanup_cb cleanup )
{
    var_SetAddress( mp, "amem-setup", setup );
    var_SetAddress( mp, "amem-cleanup", cleanup );
}

void libvlc_audio_set_format( libvlc_media_player_t *mp, const char *format,
                              unsigned rate, unsigned channels )
{
    var_SetString( mp, "amem-format", format );
    var_SetInteger( mp, "amem-rate", rate );
    var_SetInteger( mp, "amem-channels", channels );
}


/**************************************************************************
 * Getters for stream information
 **************************************************************************/
libvlc_time_t libvlc_media_player_get_length(
                             libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    libvlc_time_t i_time;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return -1;

    i_time = from_mtime(var_GetTime( p_input_thread, "length" ));
    vlc_object_release( p_input_thread );

    return i_time;
}

libvlc_time_t libvlc_media_player_get_time( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    libvlc_time_t i_time;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return -1;

    i_time = from_mtime(var_GetTime( p_input_thread , "time" ));
    vlc_object_release( p_input_thread );
    return i_time;
}

void libvlc_media_player_set_time( libvlc_media_player_t *p_mi,
                                   libvlc_time_t i_time )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return;

    var_SetTime( p_input_thread, "time", to_mtime(i_time) );
    vlc_object_release( p_input_thread );
}

void libvlc_media_player_set_position( libvlc_media_player_t *p_mi,
                                       float position )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return;

    var_SetFloat( p_input_thread, "position", position );
    vlc_object_release( p_input_thread );
}

float libvlc_media_player_get_position( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    float f_position;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return -1.0;

    f_position = var_GetFloat( p_input_thread, "position" );
    vlc_object_release( p_input_thread );

    return f_position;
}

void libvlc_media_player_set_chapter( libvlc_media_player_t *p_mi,
                                      int chapter )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return;

    var_SetInteger( p_input_thread, "chapter", chapter );
    vlc_object_release( p_input_thread );
}

int libvlc_media_player_get_chapter( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    int i_chapter;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return -1;

    i_chapter = var_GetInteger( p_input_thread, "chapter" );
    vlc_object_release( p_input_thread );

    return i_chapter;
}

int libvlc_media_player_get_chapter_count( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );
    vlc_object_release( p_input_thread );

    return val.i_int;
}

int libvlc_media_player_get_chapter_count_for_title(
                                 libvlc_media_player_t *p_mi,
                                 int i_title )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi );
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

void libvlc_media_player_set_title( libvlc_media_player_t *p_mi,
                                    int i_title )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi );
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

int libvlc_media_player_get_title( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    int i_title;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return -1;

    i_title = var_GetInteger( p_input_thread, "title" );
    vlc_object_release( p_input_thread );

    return i_title;
}

int libvlc_media_player_get_title_count( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    vlc_value_t val;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
    vlc_object_release( p_input_thread );

    return val.i_int;
}

void libvlc_media_player_next_chapter( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return;

    int i_type = var_Type( p_input_thread, "next-chapter" );
    var_TriggerCallback( p_input_thread, (i_type & VLC_VAR_TYPE) != 0 ?
                            "next-chapter":"next-title" );

    vlc_object_release( p_input_thread );
}

void libvlc_media_player_previous_chapter( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return;

    int i_type = var_Type( p_input_thread, "next-chapter" );
    var_TriggerCallback( p_input_thread, (i_type & VLC_VAR_TYPE) != 0 ?
                            "prev-chapter":"prev-title" );

    vlc_object_release( p_input_thread );
}

float libvlc_media_player_get_fps( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi );
    double f_fps = 0.0;

    if( p_input_thread )
    {
        if( input_Control( p_input_thread, INPUT_GET_VIDEO_FPS, &f_fps ) )
            f_fps = 0.0;
        vlc_object_release( p_input_thread );
    }
    return f_fps;
}

int libvlc_media_player_will_play( libvlc_media_player_t *p_mi )
{
    bool b_will_play;
    input_thread_t *p_input_thread =
                            libvlc_get_input_thread ( p_mi );
    if ( !p_input_thread )
        return false;

    b_will_play = !p_input_thread->b_die && !p_input_thread->b_dead;
    vlc_object_release( p_input_thread );

    return b_will_play;
}

int libvlc_media_player_set_rate( libvlc_media_player_t *p_mi, float rate )
{
    if (rate < 0.)
    {
        libvlc_printerr ("Playing backward not supported");
        return -1;
    }

    var_SetFloat (p_mi, "rate", rate);

    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return 0;
    var_SetFloat( p_input_thread, "rate", rate );
    vlc_object_release( p_input_thread );
    return 0;
}

float libvlc_media_player_get_rate( libvlc_media_player_t *p_mi )
{
    return var_GetFloat (p_mi, "rate");
}

libvlc_state_t libvlc_media_player_get_state( libvlc_media_player_t *p_mi )
{
    lock(p_mi);
    libvlc_state_t state = p_mi->state;
    unlock(p_mi);
    return state;
}

int libvlc_media_player_is_seekable( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    bool b_seekable;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if ( !p_input_thread )
        return false;
    b_seekable = var_GetBool( p_input_thread, "can-seek" );
    vlc_object_release( p_input_thread );

    return b_seekable;
}

void libvlc_media_player_navigate( libvlc_media_player_t* p_mi,
                                   unsigned navigate )
{
    input_thread_t *p_input_thread;

    if ( navigate > libvlc_navigate_to_action_size)
      return;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if ( !p_input_thread )
      return;

    var_SetInteger( p_mi->p_libvlc_instance->p_libvlc_int,
                    "key-action", libvlc_navigate_to_action[navigate] );

    vlc_object_release( p_input_thread );
}

/* internal function, used by audio, video */
libvlc_track_description_t *
        libvlc_get_track_description( libvlc_media_player_t *p_mi,
                                      const char *psz_variable )
{
    input_thread_t *p_input = libvlc_get_input_thread( p_mi );
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
                libvlc_track_description_list_release( p_track_description );
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

// Deprecated alias for libvlc_track_description_list_release
void libvlc_track_description_release( libvlc_track_description_t *p_td )
{
    libvlc_track_description_list_release( p_td );
}

void libvlc_track_description_list_release( libvlc_track_description_t *p_td )
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

int libvlc_media_player_can_pause( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    bool b_can_pause;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if ( !p_input_thread )
        return false;
    b_can_pause = var_GetBool( p_input_thread, "can-pause" );
    vlc_object_release( p_input_thread );

    return b_can_pause;
}

void libvlc_media_player_next_frame( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi );
    if( p_input_thread != NULL )
    {
        var_TriggerCallback( p_input_thread, "frame-next" );
        vlc_object_release( p_input_thread );
    }
}
