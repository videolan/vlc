/*****************************************************************************
 * media_player.c: Libvlc API Media Instance management functions
 *****************************************************************************
 * Copyright (C) 2005-2015 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_events.h>

#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_actions.h>
#include <vlc_http.h>

#include "libvlc_internal.h"
#include "media_internal.h" // libvlc_media_set_state()
#include "media_player_internal.h"
#include "renderer_discoverer_internal.h"

#define ES_INIT (-2) /* -1 is reserved for ES deselect */

static int
input_seekable_changed( vlc_object_t * p_this, char const * psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval,
                        void * p_userdata );
static int
input_pausable_changed( vlc_object_t * p_this, char const * psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval,
                        void * p_userdata );
static int
input_scrambled_changed( vlc_object_t * p_this, char const * psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval,
                        void * p_userdata );
static int
input_event_changed( vlc_object_t * p_this, char const * psz_cmd,
                     vlc_value_t oldval, vlc_value_t newval,
                     void * p_userdata );

static int
input_es_changed( vlc_object_t * p_this, char const * psz_cmd,
                  int action, vlc_value_t *p_val,
                  void *p_userdata);

static int
corks_changed(vlc_object_t *obj, const char *name, vlc_value_t old,
              vlc_value_t cur, void *opaque);

static int
mute_changed(vlc_object_t *obj, const char *name, vlc_value_t old,
             vlc_value_t cur, void *opaque);

static int
volume_changed(vlc_object_t *obj, const char *name, vlc_value_t old,
               vlc_value_t cur, void *opaque);

static void
add_es_callbacks( input_thread_t *p_input_thread, libvlc_media_player_t *p_mi );

static void
del_es_callbacks( input_thread_t *p_input_thread, libvlc_media_player_t *p_mi );

static int
snapshot_was_taken( vlc_object_t *p_this, char const *psz_cmd,
                    vlc_value_t oldval, vlc_value_t newval, void *p_data );

static void libvlc_media_player_destroy( libvlc_media_player_t *p_mi );

/*
 * Shortcuts
 */

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

static void input_item_preparsed_changed( const vlc_event_t *p_event,
                                          void * user_data )
{
    libvlc_media_t *p_md = user_data;
    if( p_event->u.input_item_preparsed_changed.new_status & ITEM_PREPARSED )
    {
        /* Send the event */
        libvlc_event_t event;
        event.type = libvlc_MediaParsedChanged;
        event.u.media_parsed_changed.new_status = libvlc_media_parsed_status_done;
        libvlc_event_send( &p_md->event_manager, &event );
    }
}

static void media_attach_preparsed_event( libvlc_media_t *p_md )
{
    vlc_event_attach( &p_md->p_input_item->event_manager,
                      vlc_InputItemPreparsedChanged,
                      input_item_preparsed_changed, p_md );
}

static void media_detach_preparsed_event( libvlc_media_t *p_md )
{
    vlc_event_detach( &p_md->p_input_item->event_manager,
                      vlc_InputItemPreparsedChanged,
                      input_item_preparsed_changed,
                      p_md );
}

/*
 * Release the associated input thread.
 *
 * Object lock is NOT held.
 * Input lock is held or instance is being destroyed.
 */
static void release_input_thread( libvlc_media_player_t *p_mi )
{
    assert( p_mi );

    input_thread_t *p_input_thread = p_mi->input.p_thread;
    if( !p_input_thread )
        return;
    p_mi->input.p_thread = NULL;

    media_detach_preparsed_event( p_mi->p_md );

    var_DelCallback( p_input_thread, "can-seek",
                     input_seekable_changed, p_mi );
    var_DelCallback( p_input_thread, "can-pause",
                    input_pausable_changed, p_mi );
    var_DelCallback( p_input_thread, "program-scrambled",
                    input_scrambled_changed, p_mi );
    var_DelCallback( p_input_thread, "intf-event",
                     input_event_changed, p_mi );
    del_es_callbacks( p_input_thread, p_mi );

    /* We owned this one */
    input_Stop( p_input_thread );
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

    libvlc_event_send( &p_mi->event_manager, &event );
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

    libvlc_event_send( &p_mi->event_manager, &event );
    return VLC_SUCCESS;
}

static int
input_scrambled_changed( vlc_object_t * p_this, char const * psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval,
                        void * p_userdata )
{
    VLC_UNUSED(oldval);
    VLC_UNUSED(p_this);
    VLC_UNUSED(psz_cmd);
    libvlc_media_player_t * p_mi = p_userdata;
    libvlc_event_t event;

    event.type = libvlc_MediaPlayerScrambledChanged;
    event.u.media_player_scrambled_changed.new_scrambled = newval.b_bool;

    libvlc_event_send( &p_mi->event_manager, &event );
    return VLC_SUCCESS;
}

static int
input_event_changed( vlc_object_t * p_this, char const * psz_cmd,
                     vlc_value_t oldval, vlc_value_t newval,
                     void * p_userdata )
{
    VLC_UNUSED(oldval); VLC_UNUSED(psz_cmd);
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
        libvlc_event_send( &p_mi->event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_DEAD )
    {
        libvlc_state_t libvlc_state = libvlc_Ended;
        event.type = libvlc_MediaPlayerStopped;

        set_state( p_mi, libvlc_state, false );
        libvlc_event_send( &p_mi->event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_POSITION )
    {
        if( var_GetInteger( p_input, "state" ) != PLAYING_S )
            return VLC_SUCCESS; /* Don't send the position while stopped */

        /* */
        event.type = libvlc_MediaPlayerPositionChanged;
        event.u.media_player_position_changed.new_position =
                                          var_GetFloat( p_input, "position" );
        libvlc_event_send( &p_mi->event_manager, &event );

        /* */
        event.type = libvlc_MediaPlayerTimeChanged;
        event.u.media_player_time_changed.new_time =
           from_mtime(var_GetInteger( p_input, "time" ));
        libvlc_event_send( &p_mi->event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_LENGTH )
    {
        event.type = libvlc_MediaPlayerLengthChanged;
        event.u.media_player_length_changed.new_length =
           from_mtime(var_GetInteger( p_input, "length" ));
        libvlc_event_send( &p_mi->event_manager, &event );
    }
    else if( newval.i_int == INPUT_EVENT_CACHE )
    {
        event.type = libvlc_MediaPlayerBuffering;
        event.u.media_player_buffering.new_cache = (100 *
            var_GetFloat( p_input, "cache" ));
        libvlc_event_send( &p_mi->event_manager, &event );
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
        libvlc_event_send( &p_mi->event_manager, &event );
    }
    else if ( newval.i_int == INPUT_EVENT_TITLE )
    {
        event.type = libvlc_MediaPlayerTitleChanged;
        event.u.media_player_title_changed.new_title = var_GetInteger( p_input, "title" );
        libvlc_event_send( &p_mi->event_manager, &event );
    }
    else if ( newval.i_int == INPUT_EVENT_CHAPTER )
    {
        event.type = libvlc_MediaPlayerChapterChanged;
        event.u.media_player_chapter_changed.new_chapter = var_GetInteger( p_input, "chapter" );
        libvlc_event_send( &p_mi->event_manager, &event );
    }
    else if ( newval.i_int == INPUT_EVENT_ES )
    {
        /* Send ESSelected events from this callback ("intf-event") and not
         * from "audio-es", "video-es", "spu-es" callbacks. Indeed, these
         * callbacks are not triggered when the input_thread changes an ES
         * while this callback is. */
        struct {
            const char *psz_name;
            const libvlc_track_type_t type;
            int new_es;
        } es_list[] = {
            { "audio-es", libvlc_track_audio, ES_INIT },
            { "video-es", libvlc_track_video, ES_INIT },
            { "spu-es", libvlc_track_text, ES_INIT },
        };
        /* Check if an ES selection changed */
        lock( p_mi );
        for( size_t i = 0; i < ARRAY_SIZE( es_list ); ++i )
        {
            int current_es = var_GetInteger( p_input, es_list[i].psz_name );
            if( current_es != p_mi->selected_es[i] )
                es_list[i].new_es = p_mi->selected_es[i] = current_es;
        }
        unlock( p_mi );

        /* Send the ESSelected event for each ES that were newly selected */
        for( size_t i = 0; i < ARRAY_SIZE( es_list ); ++i )
        {
            if( es_list[i].new_es != ES_INIT )
            {
                event.type = libvlc_MediaPlayerESSelected;
                event.u.media_player_es_changed.i_type = es_list[i].type;
                event.u.media_player_es_changed.i_id = es_list[i].new_es;
                libvlc_event_send( &p_mi->event_manager, &event );
            }
        }
    }

    return VLC_SUCCESS;
}

static int track_type_from_name(const char *psz_name)
{
   if( !strcmp( psz_name, "video-es" ) )
       return libvlc_track_video;
    else if( !strcmp( psz_name, "audio-es" ) )
        return libvlc_track_audio;
    else if( !strcmp( psz_name, "spu-es" ) )
        return libvlc_track_text;
    else
        return libvlc_track_unknown;
}

static int input_es_changed( vlc_object_t *p_this,
                             char const *psz_cmd,
                             int action,
                             vlc_value_t *p_val,
                             void *p_userdata )
{
    VLC_UNUSED(p_this);
    libvlc_media_player_t *mp = p_userdata;
    libvlc_event_t event;

    /* Ignore the "Disable" element */
    if (p_val && p_val->i_int < 0)
        return VLC_EGENERIC;

    switch (action)
    {
    case VLC_VAR_ADDCHOICE:
        event.type = libvlc_MediaPlayerESAdded;
        break;
    case VLC_VAR_DELCHOICE:
    case VLC_VAR_CLEARCHOICES:
        event.type = libvlc_MediaPlayerESDeleted;
        break;
    default:
        return VLC_EGENERIC;
    }

    event.u.media_player_es_changed.i_type = track_type_from_name(psz_cmd);

    int i_id;
    if (action != VLC_VAR_CLEARCHOICES)
    {
        if (!p_val)
            return VLC_EGENERIC;
        i_id = p_val->i_int;
    }
    else
    {
        /* -1 means all ES tracks of this type were deleted. */
        i_id = -1;
    }
    event.u.media_player_es_changed.i_id = i_id;

    libvlc_event_send(&mp->event_manager, &event);

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
    libvlc_event_send(&mp->event_manager, &event);

    return VLC_SUCCESS;
}

static int corks_changed(vlc_object_t *obj, const char *name, vlc_value_t old,
                         vlc_value_t cur, void *opaque)
{
    libvlc_media_player_t *mp = (libvlc_media_player_t *)obj;

    if (!old.i_int != !cur.i_int)
    {
        libvlc_event_t event;

        event.type = cur.i_int ? libvlc_MediaPlayerCorked
                               : libvlc_MediaPlayerUncorked;
        libvlc_event_send(&mp->event_manager, &event);
    }
    VLC_UNUSED(name); VLC_UNUSED(opaque);
    return VLC_SUCCESS;
}

static int audio_device_changed(vlc_object_t *obj, const char *name,
                                vlc_value_t old, vlc_value_t cur, void *opaque)
{
    libvlc_media_player_t *mp = (libvlc_media_player_t *)obj;
    libvlc_event_t event;

    event.type = libvlc_MediaPlayerAudioDevice;
    event.u.media_player_audio_device.device = cur.psz_string;
    libvlc_event_send(&mp->event_manager, &event);
    VLC_UNUSED(name); VLC_UNUSED(old); VLC_UNUSED(opaque);
    return VLC_SUCCESS;
}

static int mute_changed(vlc_object_t *obj, const char *name, vlc_value_t old,
                        vlc_value_t cur, void *opaque)
{
    libvlc_media_player_t *mp = (libvlc_media_player_t *)obj;

    if (old.b_bool != cur.b_bool)
    {
        libvlc_event_t event;

        event.type = cur.b_bool ? libvlc_MediaPlayerMuted
                                : libvlc_MediaPlayerUnmuted;
        libvlc_event_send(&mp->event_manager, &event);
    }
    VLC_UNUSED(name); VLC_UNUSED(opaque);
    return VLC_SUCCESS;
}

static int volume_changed(vlc_object_t *obj, const char *name, vlc_value_t old,
                          vlc_value_t cur, void *opaque)
{
    libvlc_media_player_t *mp = (libvlc_media_player_t *)obj;
    libvlc_event_t event;

    event.type = libvlc_MediaPlayerAudioVolume;
    event.u.media_player_audio_volume.volume = cur.f_float;
    libvlc_event_send(&mp->event_manager, &event);
    VLC_UNUSED(name); VLC_UNUSED(old); VLC_UNUSED(opaque);
    return VLC_SUCCESS;
}

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
    var_Create (mp, "sout", VLC_VAR_STRING);
    var_Create (mp, "demux-filter", VLC_VAR_STRING);

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
    var_Create (mp, "avcodec-hw", VLC_VAR_STRING);
    var_Create (mp, "drawable-xid", VLC_VAR_INTEGER);
#if defined (_WIN32) || defined (__OS2__)
    var_Create (mp, "drawable-hwnd", VLC_VAR_INTEGER);
#endif
#ifdef __APPLE__
    var_Create (mp, "drawable-nsobject", VLC_VAR_ADDRESS);
#endif
#ifdef __ANDROID__
    var_Create (mp, "drawable-androidwindow", VLC_VAR_ADDRESS);
#endif
#ifdef HAVE_EVAS
    var_Create (mp, "drawable-evasobject", VLC_VAR_ADDRESS);
#endif

    var_Create (mp, "keyboard-events", VLC_VAR_BOOL);
    var_SetBool (mp, "keyboard-events", true);
    var_Create (mp, "mouse-events", VLC_VAR_BOOL);
    var_SetBool (mp, "mouse-events", true);

    var_Create (mp, "fullscreen", VLC_VAR_BOOL);
    var_Create (mp, "autoscale", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    var_Create (mp, "zoom", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    var_Create (mp, "aspect-ratio", VLC_VAR_STRING);
    var_Create (mp, "crop", VLC_VAR_STRING);
    var_Create (mp, "deinterlace", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create (mp, "deinterlace-mode", VLC_VAR_STRING | VLC_VAR_DOINHERIT);

    var_Create (mp, "vbi-page", VLC_VAR_INTEGER);
    var_SetInteger (mp, "vbi-page", 100);

    var_Create (mp, "video-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create (mp, "sub-source", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create (mp, "sub-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);

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
    var_Create (mp, "hue", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    var_Create (mp, "saturation", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    var_Create (mp, "gamma", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);

     /* Audio */
    var_Create (mp, "aout", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create (mp, "audio-device", VLC_VAR_STRING);
    var_Create (mp, "mute", VLC_VAR_BOOL);
    var_Create (mp, "volume", VLC_VAR_FLOAT);
    var_Create (mp, "corks", VLC_VAR_INTEGER);
    var_Create (mp, "audio-filter", VLC_VAR_STRING);
    var_Create (mp, "role", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
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

    /* Video Title */
    var_Create (mp, "video-title-show", VLC_VAR_BOOL);
    var_Create (mp, "video-title-position", VLC_VAR_INTEGER);
    var_Create (mp, "video-title-timeout", VLC_VAR_INTEGER);

    /* Equalizer */
    var_Create (mp, "equalizer-preamp", VLC_VAR_FLOAT);
    var_Create (mp, "equalizer-vlcfreqs", VLC_VAR_BOOL);
    var_Create (mp, "equalizer-bands", VLC_VAR_STRING);

    /* Initialize the shared HTTP cookie jar */
    vlc_value_t cookies;
    cookies.p_address = vlc_http_cookies_new();
    if ( likely(cookies.p_address) )
    {
        var_Create(mp, "http-cookies", VLC_VAR_ADDRESS);
        var_SetChecked(mp, "http-cookies", VLC_VAR_ADDRESS, cookies);
    }

    mp->p_md = NULL;
    mp->state = libvlc_NothingSpecial;
    mp->p_libvlc_instance = instance;
    mp->input.p_thread = NULL;
    mp->input.p_renderer = NULL;
    mp->input.p_resource = input_resource_New(VLC_OBJECT(mp));
    if (unlikely(mp->input.p_resource == NULL))
    {
        vlc_object_release(mp);
        return NULL;
    }
    audio_output_t *aout = input_resource_GetAout(mp->input.p_resource);
    if( aout != NULL )
        input_resource_PutAout(mp->input.p_resource, aout);

    vlc_viewpoint_init(&mp->viewpoint);

    var_Create (mp, "viewpoint", VLC_VAR_ADDRESS);
    var_SetAddress( mp, "viewpoint", &mp->viewpoint );
    vlc_mutex_init (&mp->input.lock);
    mp->i_refcount = 1;
    libvlc_event_manager_init(&mp->event_manager, mp);
    vlc_mutex_init(&mp->object_lock);

    var_AddCallback(mp, "corks", corks_changed, NULL);
    var_AddCallback(mp, "audio-device", audio_device_changed, NULL);
    var_AddCallback(mp, "mute", mute_changed, NULL);
    var_AddCallback(mp, "volume", volume_changed, NULL);

    /* Snapshot initialization */
    /* Attach a var callback to the global object to provide the glue between
     * vout_thread that generates the event and media_player that re-emits it
     * with its own event manager
     *
     * FIXME: It's unclear why we want to put this in public API, and why we
     * want to expose it in such a limiting and ugly way.
     */
    var_AddCallback(mp->obj.libvlc, "snapshot-file", snapshot_was_taken, mp);

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
    var_DelCallback( p_mi->obj.libvlc,
                     "snapshot-file", snapshot_was_taken, p_mi );

    /* Detach callback from the media player / input manager object */
    var_DelCallback( p_mi, "volume", volume_changed, NULL );
    var_DelCallback( p_mi, "mute", mute_changed, NULL );
    var_DelCallback( p_mi, "audio-device", audio_device_changed, NULL );
    var_DelCallback( p_mi, "corks", corks_changed, NULL );

    /* No need for lock_input() because no other threads knows us anymore */
    if( p_mi->input.p_thread )
        release_input_thread(p_mi);
    input_resource_Terminate( p_mi->input.p_resource );
    input_resource_Release( p_mi->input.p_resource );
    if( p_mi->input.p_renderer )
        vlc_renderer_item_release( p_mi->input.p_renderer );

    vlc_mutex_destroy( &p_mi->input.lock );

    libvlc_event_manager_destroy(&p_mi->event_manager);
    libvlc_media_release( p_mi->p_md );
    vlc_mutex_destroy( &p_mi->object_lock );

    vlc_http_cookie_jar_t *cookies = var_GetAddress( p_mi, "http-cookies" );
    if ( cookies )
    {
        var_Destroy( p_mi, "http-cookies" );
        vlc_http_cookies_destroy( cookies );
    }

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

    release_input_thread( p_mi );

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
    libvlc_event_send( &p_mi->event_manager, &event );

}

/**************************************************************************
 * Get the Media descriptor associated with the instance.
 **************************************************************************/
libvlc_media_t *
libvlc_media_player_get_media( libvlc_media_player_t *p_mi )
{
    libvlc_media_t *p_m;

    lock( p_mi );
    p_m = p_mi->p_md;
    if( p_m )
        libvlc_media_retain( p_m );
    unlock( p_mi );

    return p_m;
}

/**************************************************************************
 * Get the event Manager.
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_player_event_manager( libvlc_media_player_t *p_mi )
{
    return &p_mi->event_manager;
}

static void add_es_callbacks( input_thread_t *p_input_thread, libvlc_media_player_t *p_mi )
{
    var_AddListCallback( p_input_thread, "video-es", input_es_changed, p_mi );
    var_AddListCallback( p_input_thread, "audio-es", input_es_changed, p_mi );
    var_AddListCallback( p_input_thread, "spu-es", input_es_changed, p_mi );
}

static void del_es_callbacks( input_thread_t *p_input_thread, libvlc_media_player_t *p_mi )
{
    var_DelListCallback( p_input_thread, "video-es", input_es_changed, p_mi );
    var_DelListCallback( p_input_thread, "audio-es", input_es_changed, p_mi );
    var_DelListCallback( p_input_thread, "spu-es", input_es_changed, p_mi );
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

    for( size_t i = 0; i < ARRAY_SIZE( p_mi->selected_es ); ++i )
        p_mi->selected_es[i] = ES_INIT;

    media_attach_preparsed_event( p_mi->p_md );

    p_input_thread = input_Create( p_mi, p_mi->p_md->p_input_item, NULL,
                                   p_mi->input.p_resource,
                                   p_mi->input.p_renderer );
    unlock(p_mi);
    if( !p_input_thread )
    {
        unlock_input(p_mi);
        media_detach_preparsed_event( p_mi->p_md );
        libvlc_printerr( "Not enough memory" );
        return -1;
    }

    var_AddCallback( p_input_thread, "can-seek", input_seekable_changed, p_mi );
    var_AddCallback( p_input_thread, "can-pause", input_pausable_changed, p_mi );
    var_AddCallback( p_input_thread, "program-scrambled", input_scrambled_changed, p_mi );
    var_AddCallback( p_input_thread, "intf-event", input_event_changed, p_mi );
    add_es_callbacks( p_input_thread, p_mi );

    if( input_Start( p_input_thread ) )
    {
        unlock_input(p_mi);
        del_es_callbacks( p_input_thread, p_mi );
        var_DelCallback( p_input_thread, "intf-event", input_event_changed, p_mi );
        var_DelCallback( p_input_thread, "can-pause", input_pausable_changed, p_mi );
        var_DelCallback( p_input_thread, "program-scrambled", input_scrambled_changed, p_mi );
        var_DelCallback( p_input_thread, "can-seek", input_seekable_changed, p_mi );
        input_Close( p_input_thread );
        media_detach_preparsed_event( p_mi->p_md );
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

    if( paused )
    {
        if( libvlc_media_player_can_pause( p_mi ) )
            input_Control( p_input_thread, INPUT_SET_STATE, PAUSE_S );
        else
            input_Stop( p_input_thread );
    }
    else
    {
        input_Control( p_input_thread, INPUT_SET_STATE, PLAYING_S );
    }

    vlc_object_release( p_input_thread );
}

/**************************************************************************
 * Toggle pause.
 **************************************************************************/
void libvlc_media_player_pause( libvlc_media_player_t *p_mi )
{
    libvlc_media_player_set_pause( p_mi, libvlc_media_player_is_playing( p_mi ) );
}

/**************************************************************************
 * Tells whether the media player is currently playing.
 **************************************************************************/
int libvlc_media_player_is_playing( libvlc_media_player_t *p_mi )
{
    libvlc_state_t state = libvlc_media_player_get_state( p_mi );
    return libvlc_Playing == state;
}

/**************************************************************************
 * Stop playing.
 **************************************************************************/
void libvlc_media_player_stop( libvlc_media_player_t *p_mi )
{
    lock_input(p_mi);
    release_input_thread( p_mi ); /* This will stop the input thread */

    /* Force to go to stopped state, in case we were in Ended, or Error
     * state. */
    if( p_mi->state != libvlc_Stopped )
    {
        set_state( p_mi, libvlc_Stopped, false );

        /* Construct and send the event */
        libvlc_event_t event;
        event.type = libvlc_MediaPlayerStopped;
        libvlc_event_send( &p_mi->event_manager, &event );
    }

    input_resource_Terminate( p_mi->input.p_resource );
    unlock_input(p_mi);
}

int libvlc_media_player_set_renderer( libvlc_media_player_t *p_mi,
                                      libvlc_renderer_item_t *p_litem )
{
    vlc_renderer_item_t *p_item =
        p_litem ? libvlc_renderer_item_to_vlc( p_litem ) : NULL;

    lock_input( p_mi );
    input_thread_t *p_input_thread = p_mi->input.p_thread;
    if( p_input_thread )
        input_Control( p_input_thread, INPUT_SET_RENDERER, p_item );

    if( p_mi->input.p_renderer )
        vlc_renderer_item_release( p_mi->input.p_renderer );
    p_mi->input.p_renderer = p_item ? vlc_renderer_item_hold( p_item ) : NULL;
    unlock_input( p_mi );

    return 0;
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
    var_SetString( mp, "avcodec-hw", "none" );
    var_SetString( mp, "vout", "vmem" );
    var_SetString( mp, "window", "none" );
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
    var_SetString (p_mi, "avcodec-hw", "");
    var_SetString (p_mi, "vout", "");
    var_SetString (p_mi, "window", "");
    var_SetAddress (p_mi, "drawable-nsobject", drawable);
#else
    (void)drawable;
    libvlc_printerr ("can't set nsobject: APPLE build required");
    assert(false);
    var_SetString (p_mi, "vout", "none");
    var_SetString (p_mi, "window", "none");
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
    (void) p_mi;
    return NULL;
#endif
}

/**************************************************************************
 * set_agl
 **************************************************************************/
void libvlc_media_player_set_agl( libvlc_media_player_t *p_mi,
                                  uint32_t drawable )
{
    (void)drawable;
    libvlc_printerr ("can't set agl: use libvlc_media_player_set_nsobject instead");
    assert(false);
    var_SetString (p_mi, "vout", "none");
    var_SetString (p_mi, "window", "none");
}

/**************************************************************************
 * get_agl
 **************************************************************************/
uint32_t libvlc_media_player_get_agl( libvlc_media_player_t *p_mi )
{
    (void) p_mi;
    return 0;
}

/**************************************************************************
 * set_xwindow
 **************************************************************************/
void libvlc_media_player_set_xwindow( libvlc_media_player_t *p_mi,
                                      uint32_t drawable )
{
    assert (p_mi != NULL);

    var_SetString (p_mi, "avcodec-hw", "");
    var_SetString (p_mi, "vout", "");
    var_SetString (p_mi, "window", drawable ? "embed-xid,any" : "");
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
#if defined (_WIN32) || defined (__OS2__)
    var_SetString (p_mi, "avcodec-hw", "");
    var_SetString (p_mi, "vout", "");
    var_SetString (p_mi, "window",
                   (drawable != NULL) ? "embed-hwnd,any" : "");
    var_SetInteger (p_mi, "drawable-hwnd", (uintptr_t)drawable);
#else
    (void) drawable;
    libvlc_printerr ("can't set nsobject: WIN32 build required");
    assert(false);
    var_SetString (p_mi, "vout", "none");
    var_SetString (p_mi, "window", "none");
#endif
}

/**************************************************************************
 * get_hwnd
 **************************************************************************/
void *libvlc_media_player_get_hwnd( libvlc_media_player_t *p_mi )
{
    assert (p_mi != NULL);
#if defined (_WIN32) || defined (__OS2__)
    return (void *)(uintptr_t)var_GetInteger (p_mi, "drawable-hwnd");
#else
    (void) p_mi;
    return NULL;
#endif
}

/**************************************************************************
 * set_android_context
 **************************************************************************/
void libvlc_media_player_set_android_context( libvlc_media_player_t *p_mi,
                                              void *p_awindow_handler )
{
    assert (p_mi != NULL);
#ifdef __ANDROID__
    var_SetAddress (p_mi, "drawable-androidwindow", p_awindow_handler);
#else
    (void) p_awindow_handler;
    libvlc_printerr ("can't set android context: ANDROID build required");
    assert(false);
    var_SetString (p_mi, "vout", "none");
    var_SetString (p_mi, "window", "none");
#endif
}

/**************************************************************************
 * set_evas_object
 **************************************************************************/
int libvlc_media_player_set_evas_object( libvlc_media_player_t *p_mi,
                                         void *p_evas_object )
{
    assert (p_mi != NULL);
#ifdef HAVE_EVAS
    var_SetString (p_mi, "vout", "evas");
    var_SetString (p_mi, "window", "none");
    var_SetAddress (p_mi, "drawable-evasobject", p_evas_object);
    return 0;
#else
    (void) p_mi; (void) p_evas_object;
    return -1;
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

    input_resource_ResetAout(mp->input.p_resource);
}

void libvlc_audio_set_volume_callback( libvlc_media_player_t *mp,
                                       libvlc_audio_set_volume_cb cb )
{
    var_SetAddress( mp, "amem-set-volume", cb );

    input_resource_ResetAout(mp->input.p_resource);
}

void libvlc_audio_set_format_callbacks( libvlc_media_player_t *mp,
                                        libvlc_audio_setup_cb setup,
                                        libvlc_audio_cleanup_cb cleanup )
{
    var_SetAddress( mp, "amem-setup", setup );
    var_SetAddress( mp, "amem-cleanup", cleanup );

    input_resource_ResetAout(mp->input.p_resource);
}

void libvlc_audio_set_format( libvlc_media_player_t *mp, const char *format,
                              unsigned rate, unsigned channels )
{
    var_SetString( mp, "amem-format", format );
    var_SetInteger( mp, "amem-rate", rate );
    var_SetInteger( mp, "amem-channels", channels );

    input_resource_ResetAout(mp->input.p_resource);
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

    i_time = from_mtime(var_GetInteger( p_input_thread, "length" ));
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

    i_time = from_mtime(var_GetInteger( p_input_thread , "time" ));
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

    var_SetInteger( p_input_thread, "time", to_mtime(i_time) );
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

    int i_ret = var_Change( p_input_thread, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );
    vlc_object_release( p_input_thread );

    return i_ret == VLC_SUCCESS ? val.i_int : -1;
}

int libvlc_media_player_get_chapter_count_for_title(
                                 libvlc_media_player_t *p_mi,
                                 int i_title )
{
    vlc_value_t val;

    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi );
    if( !p_input_thread )
        return -1;

    char psz_name[sizeof ("title ") + 3 * sizeof (int)];
    sprintf( psz_name, "title %2u", i_title );

    int i_ret = var_Change( p_input_thread, psz_name, VLC_VAR_CHOICESCOUNT, &val, NULL );
    vlc_object_release( p_input_thread );

    return i_ret == VLC_SUCCESS ? val.i_int : -1;
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
    libvlc_event_send( &p_mi->event_manager, &event );
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

    int i_ret = var_Change( p_input_thread, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
    vlc_object_release( p_input_thread );

    return i_ret == VLC_SUCCESS ? val.i_int : -1;
}

int libvlc_media_player_get_full_title_descriptions( libvlc_media_player_t *p_mi,
                                                     libvlc_title_description_t *** pp_titles )
{
    assert( p_mi );

    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );

    if( !p_input_thread )
        return -1;

    input_title_t **p_input_title;
    int count;

    /* fetch data */
    int ret = input_Control( p_input_thread, INPUT_GET_FULL_TITLE_INFO,
                             &p_input_title, &count );
    vlc_object_release( p_input_thread );
    if( ret != VLC_SUCCESS )
        return -1;

    libvlc_title_description_t **titles = vlc_alloc( count, sizeof (*titles) );
    if( count > 0 && unlikely(titles == NULL) )
        return -1;

    /* fill array */
    for( int i = 0; i < count; i++)
    {
        libvlc_title_description_t *title = malloc( sizeof (*title) );
        if( unlikely(title == NULL) )
        {
            libvlc_title_descriptions_release( titles, i );
            return -1;
        }
        titles[i] = title;

        /* we want to return milliseconds to match the rest of the API */
        title->i_duration = p_input_title[i]->i_length / 1000;
        title->i_flags = p_input_title[i]->i_flags;
        if( p_input_title[i]->psz_name )
            title->psz_name = strdup( p_input_title[i]->psz_name );
        else
            title->psz_name = NULL;
        vlc_input_title_Delete( p_input_title[i] );
    }
    free( p_input_title );

    *pp_titles = titles;
    return count;
}

void libvlc_title_descriptions_release( libvlc_title_description_t **p_titles,
                                        unsigned i_count )
{
    for (unsigned i = 0; i < i_count; i++ )
    {
        if ( !p_titles[i] )
            continue;

        free( p_titles[i]->psz_name );
        free( p_titles[i] );
    }
    free( p_titles );
}

int libvlc_media_player_get_full_chapter_descriptions( libvlc_media_player_t *p_mi,
                                                      int i_chapters_of_title,
                                                      libvlc_chapter_description_t *** pp_chapters )
{
    assert( p_mi );

    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );

    if( !p_input_thread )
        return -1;

    seekpoint_t **p_seekpoint = NULL;

    /* fetch data */
    int ci_chapter_count = i_chapters_of_title;

    int ret = input_Control(p_input_thread, INPUT_GET_SEEKPOINTS, &p_seekpoint, &ci_chapter_count);
    if( ret != VLC_SUCCESS)
    {
        vlc_object_release( p_input_thread );
        return -1;
    }

    if (ci_chapter_count == 0 || p_seekpoint == NULL)
    {
        vlc_object_release( p_input_thread );
        return 0;
    }

    input_title_t *p_title;
    ret = input_Control( p_input_thread, INPUT_GET_TITLE_INFO, &p_title,
                         &i_chapters_of_title );
    vlc_object_release( p_input_thread );
    if( ret != VLC_SUCCESS )
    {
        goto error;
    }
    int64_t i_title_duration = p_title->i_length / 1000;
    vlc_input_title_Delete( p_title );

    *pp_chapters = calloc( ci_chapter_count, sizeof(**pp_chapters) );
    if( !*pp_chapters )
    {
        goto error;
    }

    /* fill array */
    for( int i = 0; i < ci_chapter_count; ++i )
    {
        libvlc_chapter_description_t *p_chapter = malloc( sizeof(*p_chapter) );
        if( unlikely(p_chapter == NULL) )
        {
            goto error;
        }
        (*pp_chapters)[i] = p_chapter;

        p_chapter->i_time_offset = p_seekpoint[i]->i_time_offset / 1000;

        if( i < ci_chapter_count - 1 )
        {
            p_chapter->i_duration = p_seekpoint[i + 1]->i_time_offset / 1000 -
                                    p_chapter->i_time_offset;
        }
        else
        {
            if ( i_title_duration )
                p_chapter->i_duration = i_title_duration - p_chapter->i_time_offset;
            else
                p_chapter->i_duration = 0;
        }

        if( p_seekpoint[i]->psz_name )
        {
            p_chapter->psz_name = strdup( p_seekpoint[i]->psz_name );
        }
        else
        {
            p_chapter->psz_name = NULL;
        }
        vlc_seekpoint_Delete( p_seekpoint[i] );
        p_seekpoint[i] = NULL;
    }

    free( p_seekpoint );
    return ci_chapter_count;

error:
    if( *pp_chapters )
        libvlc_chapter_descriptions_release( *pp_chapters, ci_chapter_count );
    for ( int i = 0; i < ci_chapter_count; ++i )
        vlc_seekpoint_Delete( p_seekpoint[i] );
    free( p_seekpoint );
    return -1;
}

void libvlc_chapter_descriptions_release( libvlc_chapter_description_t **p_chapters,
                                          unsigned i_count )
{
    for (unsigned i = 0; i < i_count; i++ )
    {
        if ( !p_chapters[i] )
            continue;

        free( p_chapters[i]->psz_name );
        free( p_chapters[i] );
    }
    free( p_chapters );
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
    libvlc_media_t *media = libvlc_media_player_get_media( p_mi );
    if( media == NULL )
        return 0.f;

    input_item_t *item = media->p_input_item;
    float fps = 0.f;

    vlc_mutex_lock( &item->lock );
    for( int i = 0; i < item->i_es; i++ )
    {
        const es_format_t *fmt = item->es[i];

        if( fmt->i_cat == VIDEO_ES && fmt->video.i_frame_rate_base > 0 )
            fps = (float)fmt->video.i_frame_rate
                  / (float)fmt->video.i_frame_rate_base;
    }
    vlc_mutex_unlock( &item->lock );
    libvlc_media_release( media );

    return fps;
}

int libvlc_media_player_will_play( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread =
                            libvlc_get_input_thread ( p_mi );
    if ( !p_input_thread )
        return false;

    int state = var_GetInteger( p_input_thread, "state" );
    vlc_object_release( p_input_thread );

    return state != END_S && state != ERROR_S;
}

int libvlc_media_player_set_rate( libvlc_media_player_t *p_mi, float rate )
{
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
    static const int map[] =
    {
        INPUT_NAV_ACTIVATE, INPUT_NAV_UP, INPUT_NAV_DOWN,
        INPUT_NAV_LEFT, INPUT_NAV_RIGHT, INPUT_NAV_POPUP,
    };

    if( navigate >= sizeof(map) / sizeof(map[0]) )
      return;

    input_thread_t *p_input = libvlc_get_input_thread ( p_mi );
    if ( p_input == NULL )
      return;

    input_Control( p_input, map[navigate], NULL );
    vlc_object_release( p_input );
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
    int i_ret = var_Change( p_input, psz_variable, VLC_VAR_GETCHOICES, &val_list, &text_list );
    if( i_ret != VLC_SUCCESS )
        return NULL;

    /* no tracks */
    if( val_list.p_list->i_count <= 0 )
        goto end;

    p_track_description = malloc( sizeof *p_track_description );
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
            p_actual = malloc( sizeof *p_actual );
            if ( !p_actual )
            {
                libvlc_track_description_list_release( p_track_description );
                libvlc_printerr( "Not enough memory" );

                p_track_description = NULL;
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

int libvlc_media_player_program_scrambled( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread;
    bool b_program_scrambled;

    p_input_thread = libvlc_get_input_thread ( p_mi );
    if ( !p_input_thread )
        return false;
    b_program_scrambled = var_GetBool( p_input_thread, "program-scrambled" );
    vlc_object_release( p_input_thread );

    return b_program_scrambled;
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

/**
 * Private lookup table to get subpicture alignment flag values corresponding
 * to a libvlc_position_t enumerated value.
 */
static const unsigned char position_subpicture_alignment[] = {
    [libvlc_position_center]       = 0,
    [libvlc_position_left]         = SUBPICTURE_ALIGN_LEFT,
    [libvlc_position_right]        = SUBPICTURE_ALIGN_RIGHT,
    [libvlc_position_top]          = SUBPICTURE_ALIGN_TOP,
    [libvlc_position_top_left]     = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT,
    [libvlc_position_top_right]    = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_RIGHT,
    [libvlc_position_bottom]       = SUBPICTURE_ALIGN_BOTTOM,
    [libvlc_position_bottom_left]  = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_LEFT,
    [libvlc_position_bottom_right] = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_RIGHT
};

void libvlc_media_player_set_video_title_display( libvlc_media_player_t *p_mi, libvlc_position_t position, unsigned timeout )
{
    assert( position >= libvlc_position_disable && position <= libvlc_position_bottom_right );

    if ( position != libvlc_position_disable )
    {
        var_SetBool( p_mi, "video-title-show", true );
        var_SetInteger( p_mi, "video-title-position", position_subpicture_alignment[position] );
        var_SetInteger( p_mi, "video-title-timeout", timeout );
    }
    else
    {
        var_SetBool( p_mi, "video-title-show", false );
    }
}

int libvlc_media_player_add_slave( libvlc_media_player_t *p_mi,
                                   libvlc_media_slave_type_t i_type,
                                   const char *psz_uri, bool b_select )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi );

    if( p_input_thread == NULL )
    {
        libvlc_media_t *p_media = libvlc_media_player_get_media( p_mi );
        if( p_media == NULL )
            return -1;

        int i_ret = libvlc_media_slaves_add( p_media, i_type, 4, psz_uri );
        libvlc_media_release( p_media );
        return i_ret;
    }
    else
    {
        int i_ret = input_AddSlave( p_input_thread, (enum slave_type) i_type,
                                    psz_uri, b_select, false, false );
        vlc_object_release( p_input_thread );

        return i_ret == VLC_SUCCESS ? 0 : -1;
    }
}

/**
 * Maximum size of a formatted equalizer amplification band frequency value.
 *
 * The allowed value range is supposed to be constrained from -20.0 to 20.0.
 *
 * The format string " %.07f" with a minimum value of "-20" gives a maximum
 * string length of e.g. " -19.1234567", i.e. 12 bytes (not including the null
 * terminator).
 */
#define EQZ_BAND_VALUE_SIZE 12

int libvlc_media_player_set_equalizer( libvlc_media_player_t *p_mi, libvlc_equalizer_t *p_equalizer )
{
    char bands[EQZ_BANDS_MAX * EQZ_BAND_VALUE_SIZE + 1];

    if( p_equalizer != NULL )
    {
        for( unsigned i = 0, c = 0; i < EQZ_BANDS_MAX; i++ )
        {
            c += snprintf( bands + c, sizeof(bands) - c, " %.07f",
                          p_equalizer->f_amp[i] );
            if( unlikely(c >= sizeof(bands)) )
                return -1;
        }

        var_SetFloat( p_mi, "equalizer-preamp", p_equalizer->f_preamp );
        var_SetString( p_mi, "equalizer-bands", bands );
    }
    var_SetString( p_mi, "audio-filter", p_equalizer ? "equalizer" : "" );

    audio_output_t *p_aout = input_resource_HoldAout( p_mi->input.p_resource );
    if( p_aout != NULL )
    {
        if( p_equalizer != NULL )
        {
            var_SetFloat( p_aout, "equalizer-preamp", p_equalizer->f_preamp );
            var_SetString( p_aout, "equalizer-bands", bands );
        }

        var_SetString( p_aout, "audio-filter", p_equalizer ? "equalizer" : "" );
        vlc_object_release( p_aout );
    }

    return 0;
}

static const char roles[][16] =
{
    [libvlc_role_Music] =         "music",
    [libvlc_role_Video] =         "video",
    [libvlc_role_Communication] = "communication",
    [libvlc_role_Game] =          "game",
    [libvlc_role_Notification] =  "notification",
    [libvlc_role_Animation] =     "animation",
    [libvlc_role_Production] =    "production",
    [libvlc_role_Accessibility] = "accessibility",
    [libvlc_role_Test] =          "test",
};

int libvlc_media_player_set_role(libvlc_media_player_t *mp, unsigned role)
{
    if (role >= ARRAY_SIZE(roles)
     || var_SetString(mp, "role", roles[role]) != VLC_SUCCESS)
        return -1;
    return 0;
}

int libvlc_media_player_get_role(libvlc_media_player_t *mp)
{
    int ret = -1;
    char *str = var_GetString(mp, "role");
    if (str == NULL)
        return 0;

    for (size_t i = 0; i < ARRAY_SIZE(roles); i++)
        if (!strcmp(roles[i], str))
        {
            ret = i;
            break;
        }

    free(str);
    return ret;
}
