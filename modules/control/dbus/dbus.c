/*****************************************************************************
 * dbus.c : D-Bus control interface
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2012 Mirsal Ennaime
 * Copyright © 2009-2012 The VideoLAN team
 * Copyright © 2013      Alex Merry
 * $Id$
 *
 * Authors:    Rafaël Carré <funman at videolanorg>
 *             Mirsal Ennaime <mirsal at mirsal fr>
 *             Alex Merry <dev at randomguy3 me uk>
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

/*
 * D-Bus Specification:
 *      http://dbus.freedesktop.org/doc/dbus-specification.html
 * D-Bus low-level C API (libdbus)
 *      http://dbus.freedesktop.org/doc/dbus/api/html/index.html
 *  extract:
 *   "If you use this low-level API directly, you're signing up for some pain."
 *
 * MPRIS Specification version 1.0
 *      http://wiki.xmms2.xmms.se/index.php/MPRIS
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <dbus/dbus.h>
#include "dbus_common.h"
#include "dbus_root.h"
#include "dbus_player.h"
#include "dbus_tracklist.h"
#include "dbus_introspect.h"

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_meta.h>
#include <vlc_mtime.h>
#include <vlc_fs.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <poll.h>
#include <errno.h>
#include <unistd.h>

#define DBUS_MPRIS_BUS_NAME "org.mpris.MediaPlayer2.vlc"
#define DBUS_INSTANCE_ID_PREFIX "instance"

#define SEEK_THRESHOLD 1000 /* µsec */
#define EVENTS_DELAY INT64_C(100000) /* 100 ms */

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static DBusHandlerResult
MPRISEntryPoint ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this );

static const DBusObjectPathVTable dbus_mpris_vtable = {
        NULL, MPRISEntryPoint, /* handler function */
        NULL, NULL, NULL, NULL
};

typedef struct
{
    int signal;
    union {
        tracklist_append_event_t *items_appended;
        tracklist_remove_event_t *items_removed;
    };
} callback_info_t;

enum
{
    PIPE_OUT = 0,
    PIPE_IN  = 1
};

static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void *Run    ( void * );

static int TrackChange( intf_thread_t * );
static int AllCallback( vlc_object_t*, const char*, vlc_value_t, vlc_value_t, void* );
static int InputCallback( vlc_object_t*, const char*, vlc_value_t, vlc_value_t, void* );

static dbus_bool_t add_timeout(DBusTimeout *, void *);
static void remove_timeout(DBusTimeout *, void *);
static void toggle_timeout(DBusTimeout *, void *);

static dbus_bool_t add_watch   ( DBusWatch *p_watch, void *p_data );
static void remove_watch    ( DBusWatch *p_watch, void *p_data );
static void watch_toggled   ( DBusWatch *p_watch, void *p_data );

static void wakeup_main_loop( void *p_data );

static void ProcessEvents  ( intf_thread_t    *p_intf,
                             callback_info_t **p_events,
                             int               i_events );

static void ProcessWatches ( intf_thread_t    *p_intf,
                             DBusWatch       **p_watches,
                             int               i_watches,
                             struct pollfd    *p_fds,
                             int               i_fds );

static void DispatchDBusMessages( intf_thread_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( N_("DBus"))
    set_category( CAT_INTERFACE )
    set_description( N_("D-Bus control interface") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/

static int Open( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf = (intf_thread_t*)p_this;

    /* initialisation of the connection */
    if( !dbus_threads_init_default() )
        return VLC_EGENERIC;

    intf_sys_t *p_sys  = calloc( 1, sizeof( intf_sys_t ) );
    if( unlikely(!p_sys) )
        return VLC_ENOMEM;

    playlist_t      *p_playlist;
    DBusConnection  *p_conn;
    p_sys->i_player_caps   = PLAYER_CAPS_NONE;
    p_sys->i_playing_state = PLAYBACK_STATE_INVALID;

    if( vlc_pipe( p_sys->p_pipe_fds ) )
    {
        free( p_sys );
        msg_Err( p_intf, "Could not create pipe" );
        return VLC_EGENERIC;
    }

    DBusError error;
    dbus_error_init( &error );

    /* connect privately to the session bus
     * the connection will not be shared with other vlc modules which use dbus,
     * thus avoiding a whole class of concurrency issues */
    p_conn = dbus_bus_get_private( DBUS_BUS_SESSION, &error );
    if( !p_conn )
    {
        msg_Err( p_this, "Failed to connect to the D-Bus session daemon: %s",
                error.message );
        dbus_error_free( &error );
        vlc_close( p_sys->p_pipe_fds[1] );
        vlc_close( p_sys->p_pipe_fds[0] );
        free( p_sys );
        return VLC_EGENERIC;
    }

    dbus_connection_set_exit_on_disconnect( p_conn, FALSE );

    /* Register the entry point object path */
    dbus_connection_register_object_path( p_conn, DBUS_MPRIS_OBJECT_PATH,
            &dbus_mpris_vtable, p_this );

    /* Try to register org.mpris.MediaPlayer2.vlc */
    const unsigned bus_flags = DBUS_NAME_FLAG_DO_NOT_QUEUE;
    var_Create(p_intf->obj.libvlc, "dbus-mpris-name", VLC_VAR_STRING);
    if( dbus_bus_request_name( p_conn, DBUS_MPRIS_BUS_NAME, bus_flags, NULL )
                                     != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
    {
        /* Register an instance-specific well known name of the form
         * org.mpris.MediaPlayer2.vlc.instanceXXXX where XXXX is the
         * current Process ID */
        char unique_service[sizeof( DBUS_MPRIS_BUS_NAME ) +
                            sizeof( DBUS_INSTANCE_ID_PREFIX ) + 10];

        snprintf( unique_service, sizeof (unique_service),
                  DBUS_MPRIS_BUS_NAME"."DBUS_INSTANCE_ID_PREFIX"%"PRIu32,
                  (uint32_t)getpid() );

        if( dbus_bus_request_name( p_conn, unique_service, bus_flags, NULL )
                                     == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
        {
            msg_Dbg( p_intf, "listening on dbus as: %s", unique_service );
            var_SetString(p_intf->obj.libvlc, "dbus-mpris-name",
                          unique_service);
        }
    }
    else
    {
        msg_Dbg( p_intf, "listening on dbus as: %s", DBUS_MPRIS_BUS_NAME );
        var_SetString(p_intf->obj.libvlc, "dbus-mpris-name",
                      DBUS_MPRIS_BUS_NAME);
    }
    dbus_connection_flush( p_conn );

    p_intf->p_sys = p_sys;
    p_sys->p_conn = p_conn;
    vlc_array_init( &p_sys->events );
    vlc_array_init( &p_sys->timeouts );
    vlc_array_init( &p_sys->watches );
    vlc_mutex_init( &p_sys->lock );

    p_playlist = pl_Get( p_intf );
    p_sys->p_playlist = p_playlist;

    var_AddCallback( p_playlist, "input-current", AllCallback, p_intf );
    var_AddCallback( p_playlist, "volume", AllCallback, p_intf );
    var_AddCallback( p_playlist, "mute", AllCallback, p_intf );
    var_AddCallback( p_playlist, "playlist-item-append", AllCallback, p_intf );
    var_AddCallback( p_playlist, "playlist-item-deleted", AllCallback, p_intf );
    var_AddCallback( p_playlist, "random", AllCallback, p_intf );
    var_AddCallback( p_playlist, "repeat", AllCallback, p_intf );
    var_AddCallback( p_playlist, "loop", AllCallback, p_intf );
    var_AddCallback( p_playlist, "fullscreen", AllCallback, p_intf );

    if( !dbus_connection_set_timeout_functions( p_conn,
                                                add_timeout,
                                                remove_timeout,
                                                toggle_timeout,
                                                p_intf, NULL ) )
        goto error;

    if( !dbus_connection_set_watch_functions( p_conn,
                                              add_watch,
                                              remove_watch,
                                              watch_toggled,
                                              p_intf, NULL ) )
        goto error;

    if( vlc_clone( &p_sys->thread, Run, p_intf, VLC_THREAD_PRIORITY_LOW ) )
        goto error;

    return VLC_SUCCESS;

error:
    var_Destroy(p_intf->obj.libvlc, "dbus-mpris-name");
    /* The dbus connection is private,
     * so we are responsible for closing it
     * XXX: Does this make sense when OOM ? */
    dbus_connection_close( p_sys->p_conn );
    dbus_connection_unref( p_conn );

    vlc_mutex_destroy( &p_sys->lock );

    vlc_close( p_sys->p_pipe_fds[1] );
    vlc_close( p_sys->p_pipe_fds[0] );
    free( p_sys );
    return VLC_ENOMEM;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/

static void Close   ( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf     = (intf_thread_t*) p_this;
    intf_sys_t      *p_sys      = p_intf->p_sys;
    playlist_t      *p_playlist = p_sys->p_playlist;

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    var_DelCallback( p_playlist, "input-current", AllCallback, p_intf );
    var_DelCallback( p_playlist, "volume", AllCallback, p_intf );
    var_DelCallback( p_playlist, "mute", AllCallback, p_intf );
    var_DelCallback( p_playlist, "playlist-item-append", AllCallback, p_intf );
    var_DelCallback( p_playlist, "playlist-item-deleted", AllCallback, p_intf );
    var_DelCallback( p_playlist, "random", AllCallback, p_intf );
    var_DelCallback( p_playlist, "repeat", AllCallback, p_intf );
    var_DelCallback( p_playlist, "loop", AllCallback, p_intf );
    var_DelCallback( p_playlist, "fullscreen", AllCallback, p_intf );

    if( p_sys->p_input )
    {
        var_DelCallback( p_sys->p_input, "intf-event", InputCallback, p_intf );
        var_DelCallback( p_sys->p_input, "can-pause", AllCallback, p_intf );
        var_DelCallback( p_sys->p_input, "can-seek", AllCallback, p_intf );
        vlc_object_release( p_sys->p_input );
    }

    /* The dbus connection is private, so we are responsible
     * for closing it */
    dbus_connection_close( p_sys->p_conn );
    dbus_connection_unref( p_sys->p_conn );

    // Free the events array
    for( size_t i = 0; i < vlc_array_count( &p_sys->events ); i++ )
    {
        callback_info_t* info = vlc_array_item_at_index( &p_sys->events, i );
        free( info );
    }
    vlc_mutex_destroy( &p_sys->lock );
    vlc_array_clear( &p_sys->events );
    vlc_array_clear( &p_sys->timeouts );
    vlc_array_clear( &p_sys->watches );
    vlc_close( p_sys->p_pipe_fds[1] );
    vlc_close( p_sys->p_pipe_fds[0] );
    free( p_sys );
}

static dbus_bool_t add_timeout(DBusTimeout *to, void *data)
{
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;

    vlc_tick_t *expiry = malloc(sizeof (*expiry));
    if (unlikely(expiry == NULL))
        return FALSE;

    dbus_timeout_set_data(to, expiry, free);

    vlc_mutex_lock(&sys->lock);
    vlc_array_append_or_abort(&sys->timeouts, to);
    vlc_mutex_unlock(&sys->lock);

    return TRUE;
}

static void remove_timeout(DBusTimeout *to, void *data)
{
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;
    size_t idx;

    vlc_mutex_lock(&sys->lock);
    idx = vlc_array_index_of_item(&sys->timeouts, to);
    vlc_array_remove(&sys->timeouts, idx);
    vlc_mutex_unlock(&sys->lock);
}

static void toggle_timeout(DBusTimeout *to, void *data)
{
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;
    vlc_tick_t *expiry = dbus_timeout_get_data(to);

    vlc_mutex_lock(&sys->lock);
    if (dbus_timeout_get_enabled(to))
        *expiry = mdate() + UINT64_C(1000) * dbus_timeout_get_interval(to);
    vlc_mutex_unlock(&sys->lock);

    wakeup_main_loop(intf);
}

/**
 * Computes the time until the next timeout expiration.
 * @note Interface lock must be held.
 * @return The time in milliseconds until the next expiration,
 *         or -1 if there are no pending timeouts.
 */
static int next_timeout(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;
    vlc_tick_t next_timeout = LAST_MDATE;
    unsigned count = vlc_array_count(&sys->timeouts);

    for (unsigned i = 0; i < count; i++)
    {
        DBusTimeout *to = vlc_array_item_at_index(&sys->timeouts, i);

        if (!dbus_timeout_get_enabled(to))
            continue;

        vlc_tick_t *expiry = dbus_timeout_get_data(to);

        if (next_timeout > *expiry)
            next_timeout = *expiry;
    }

    if (next_timeout >= LAST_MDATE)
        return -1;

    next_timeout /= 1000;

    if (next_timeout > INT_MAX)
        return INT_MAX;

    return (int)next_timeout;
}

/**
 * Process pending D-Bus timeouts.
 *
 * @note Interface lock must be held.
 */
static void process_timeouts(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;

    for (size_t i = 0; i < vlc_array_count(&sys->timeouts); i++)
    {
        DBusTimeout *to = vlc_array_item_at_index(&sys->timeouts, i);

        if (!dbus_timeout_get_enabled(to))
            continue;

        vlc_tick_t *expiry = dbus_timeout_get_data(to);
        if (*expiry > mdate())
            continue;

        expiry += UINT64_C(1000) * dbus_timeout_get_interval(to);
        vlc_mutex_unlock(&sys->lock);

        dbus_timeout_handle(to);

        vlc_mutex_lock(&sys->lock);
        i = -1; /* lost track of state, restart from beginning */
    }
}


static dbus_bool_t add_watch( DBusWatch *p_watch, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    intf_sys_t    *p_sys  = (intf_sys_t*) p_intf->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    vlc_array_append_or_abort( &p_sys->watches, p_watch );
    vlc_mutex_unlock( &p_sys->lock );

    return TRUE;
}

static void remove_watch( DBusWatch *p_watch, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    intf_sys_t    *p_sys  = (intf_sys_t*) p_intf->p_sys;
    size_t idx;

    vlc_mutex_lock( &p_sys->lock );
    idx = vlc_array_index_of_item( &p_sys->watches, p_watch );
    vlc_array_remove( &p_sys->watches, idx );
    vlc_mutex_unlock( &p_sys->lock );
}

static void watch_toggled( DBusWatch *p_watch, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;

    if( dbus_watch_get_enabled( p_watch ) )
        wakeup_main_loop( p_intf );
}

/**
 * GetPollFds() fills an array of pollfd data structures with :
 *  - the set of enabled dbus watches
 *  - the unix pipe which we use to manually wake up the main loop
 *
 * This function must be called with p_sys->lock locked
 *
 * @return The number of file descriptors
 *
 * @param intf_thread_t *p_intf this interface thread's state
 * @param struct pollfd *p_fds a pointer to a pollfd array large enough to
 * contain all the returned data (number of enabled dbus watches + 1)
 */
static int GetPollFds( intf_thread_t *p_intf, struct pollfd *p_fds )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    size_t i_watches = vlc_array_count( &p_sys->watches );
    int i_fds = 1;

    p_fds[0].fd = p_sys->p_pipe_fds[PIPE_OUT];
    p_fds[0].events = POLLIN | POLLPRI;

    for( size_t i = 0; i < i_watches; i++ )
    {
        DBusWatch *p_watch = NULL;
        p_watch = vlc_array_item_at_index( &p_sys->watches, i );
        if( !dbus_watch_get_enabled( p_watch ) )
            continue;

        p_fds[i_fds].fd = dbus_watch_get_unix_fd( p_watch );
        int i_flags = dbus_watch_get_flags( p_watch );

        if( i_flags & DBUS_WATCH_READABLE )
            p_fds[i_fds].events |= POLLIN | POLLPRI;

        if( i_flags & DBUS_WATCH_WRITABLE )
            p_fds[i_fds].events |= POLLOUT;

        i_fds++;
    }

    return i_fds;
}


/**
 * ProcessPlaylistChanged() reacts to tracks being either inserted or removed from the playlist
 *
 * This function must be called by ProcessEvents only
 *
 * @param intf_thread_t *p_intf This interface thread state
 * @param callback_info_t *p_events the list of events to process
 */
static void ProcessPlaylistChanged( intf_thread_t *p_intf,
                                    vlc_dictionary_t *player_properties,
                                    vlc_dictionary_t *tracklist_properties )
{
    playlist_t *playlist = p_intf->p_sys->p_playlist;
    playlist_Lock(playlist);
    bool b_can_play = !playlist_IsEmpty(playlist);
    playlist_Unlock(playlist);

    if( b_can_play != p_intf->p_sys->b_can_play )
    {
        p_intf->p_sys->b_can_play = b_can_play;
        vlc_dictionary_insert( player_properties, "CanPlay", NULL );
    }

    if( !vlc_dictionary_has_key( tracklist_properties, "Tracks" ) )
        vlc_dictionary_insert( tracklist_properties, "Tracks", NULL );
}

/**
 * ProcessEvents() reacts to a list of events originating from other VLC threads
 *
 * This function must be called with p_sys->lock unlocked
 *
 * @param intf_thread_t *p_intf This interface thread state
 * @param callback_info_t *p_events the list of events to process
 */
static void ProcessEvents( intf_thread_t *p_intf,
                           callback_info_t **p_events, int i_events )
{
    vlc_dictionary_t player_properties, tracklist_properties, root_properties;
    vlc_dictionary_init( &player_properties,    0 );
    vlc_dictionary_init( &tracklist_properties, 0 );
    vlc_dictionary_init( &root_properties,      0 );

    // In case multiple *_ITEM_APPEND or *_ITEM_DELETED events appear on the
    // list, the elements in their respective map values will be linked in
    // order.
    // We keep the tail of the list in order to append the elements to the end
    // of each list.
    tracklist_append_event_t *last_append = NULL;
    tracklist_remove_event_t *last_remove = NULL;

    for( int i = 0; i < i_events; i++ )
    {
        switch( p_events[i]->signal )
        {
        case SIGNAL_ITEM_CURRENT:
        {
            TrackChange( p_intf );

            // Update status when new item starts playing
            // Just relying on the variables change callbacks is not enough
            // as by the time the callbacks are attached, we might already
            // have missed a change, so we need to query and set the initial
            // values reliably here.
            vlc_dictionary_insert( &player_properties, "PlaybackStatus", NULL );
            input_thread_t *p_input = pl_CurrentInput( p_intf );
            if( p_input )
            {
                if ( var_GetBool( p_input, "can-pause" ) )
                    vlc_dictionary_insert( &player_properties, "CanPause", NULL );
                if ( var_GetBool( p_input, "can-seek" ) )
                    vlc_dictionary_insert( &player_properties, "CanSeek", NULL );

                // If VLC is started from a file (double-clicking or specifying on CLI)
                // there is a chance that we miss the initial SIGNAL_PLAYLIST_ITEM_APPEND
                // event, resulting in CanPlay never being signalled.
                // However it is not enough to check once this module is loaded, as
                // when items from the non-main playlist (like Lua discoveries) are
                // played, CanPlay would incorrectly be false too, even though we
                // have a current item that can be resumed.
                if ( !p_intf->p_sys->b_can_play )
                {
                    p_intf->p_sys->b_can_play = 1;
                    vlc_dictionary_insert( &player_properties, "CanPlay", NULL );
                }

                vlc_object_release( p_input );
            }

            // rate depends on current item
            if( !vlc_dictionary_has_key( &player_properties, "Rate" ) )
                vlc_dictionary_insert( &player_properties, "Rate", NULL );

            vlc_dictionary_insert( &player_properties, "Metadata", NULL );
            break;
        }
        case SIGNAL_PLAYLIST_ITEM_APPEND:
            if( !last_append ) {
                assert (!vlc_dictionary_has_key( &tracklist_properties, "TrackAdded" ) );
                vlc_dictionary_insert( &tracklist_properties, "TrackAdded", p_events[i]->items_appended );

                last_append = p_events[i]->items_appended;
            } else {
                last_append->change_ev.next = &p_events[i]->items_appended->change_ev;
                last_append = p_events[i]->items_appended;
            }
            ProcessPlaylistChanged( p_intf, &player_properties, &tracklist_properties );
            break;
        case SIGNAL_PLAYLIST_ITEM_DELETED:
            if( !last_remove ) {
                assert (!vlc_dictionary_has_key( &tracklist_properties, "TrackRemoved" ) );
                vlc_dictionary_insert( &tracklist_properties, "TrackRemoved", p_events[i]->items_removed );

                last_remove = p_events[i]->items_removed;
            } else {
                last_remove->change_ev.next = &p_events[i]->items_removed->change_ev;
                last_remove = p_events[i]->items_removed;
            }
            ProcessPlaylistChanged( p_intf, &player_properties, &tracklist_properties );
            break;
        case SIGNAL_VOLUME_MUTED:
        case SIGNAL_VOLUME_CHANGE:
            vlc_dictionary_insert( &player_properties, "Volume", NULL );
            break;
        case SIGNAL_RANDOM:
            vlc_dictionary_insert( &player_properties, "Shuffle", NULL );
            break;
        case SIGNAL_FULLSCREEN:
            vlc_dictionary_insert( &root_properties, "Fullscreen", NULL );
            break;
        case SIGNAL_REPEAT:
        case SIGNAL_LOOP:
            vlc_dictionary_insert( &player_properties, "LoopStatus", NULL );
            break;
        case SIGNAL_STATE:
            vlc_dictionary_insert( &player_properties, "PlaybackStatus", NULL );
            break;
        case SIGNAL_RATE:
            vlc_dictionary_insert( &player_properties, "Rate", NULL );
            break;
        case SIGNAL_INPUT_METADATA:
        {
            input_thread_t *p_input = pl_CurrentInput( p_intf );
            input_item_t   *p_item;
            if( p_input )
            {
                p_item = input_GetItem( p_input );
                vlc_object_release( p_input );

                if( p_item )
                    vlc_dictionary_insert( &player_properties,
                                           "Metadata", NULL );
            }
            break;
        }
        case SIGNAL_CAN_SEEK:
            vlc_dictionary_insert( &player_properties, "CanSeek", NULL );
            break;
        case SIGNAL_CAN_PAUSE:
            vlc_dictionary_insert( &player_properties, "CanPause", NULL );
            break;
        case SIGNAL_SEEK:
            SeekedEmit( p_intf );
            break;
        default:
            vlc_assert_unreachable();
        }
        free( p_events[i] );
    }

    if( !vlc_dictionary_is_empty( &player_properties ) )
        PlayerPropertiesChangedEmit( p_intf, &player_properties );

    if( !vlc_dictionary_is_empty( &tracklist_properties ) )
        TrackListPropertiesChangedEmit( p_intf, &tracklist_properties );

    if( !vlc_dictionary_is_empty( &root_properties ) )
        RootPropertiesChangedEmit( p_intf, &root_properties );

    vlc_dictionary_clear( &player_properties,    NULL, NULL );
    vlc_dictionary_clear( &tracklist_properties, NULL, NULL );
    vlc_dictionary_clear( &root_properties,      NULL, NULL );
}

/**
 * ProcessWatches() handles a list of dbus watches after poll() has returned
 *
 * This function must be called with p_sys->lock unlocked
 *
 * @param intf_thread_t *p_intf This interface thread state
 * @param DBusWatch **p_watches The list of dbus watches to process
 * @param int i_watches The size of the p_watches array
 * @param struct pollfd *p_fds The result of a poll() call
 * @param int i_fds The number of file descriptors processed by poll()
 */
static void ProcessWatches( intf_thread_t *p_intf,
                            DBusWatch **p_watches, int i_watches,
                            struct pollfd *p_fds,  int i_fds )
{
    VLC_UNUSED(p_intf);

    /* Process watches */
    for( int i = 0; i < i_watches; i++ )
    {
        DBusWatch *p_watch = p_watches[i];
        if( !dbus_watch_get_enabled( p_watch ) )
            continue;

        for( int j = 0; j < i_fds; j++ )
        {
            if( p_fds[j].fd != dbus_watch_get_unix_fd( p_watch ) )
                continue;

            int i_flags   = 0;
            int i_revents = p_fds[j].revents;

            if( i_revents & POLLIN )
                i_flags |= DBUS_WATCH_READABLE;

            if( i_revents & POLLOUT )
                i_flags |= DBUS_WATCH_WRITABLE;

            if( i_revents & POLLERR )
                i_flags |= DBUS_WATCH_ERROR;

            if( i_revents & POLLHUP )
                i_flags |= DBUS_WATCH_HANGUP;

            if( i_flags )
                dbus_watch_handle( p_watch, i_flags );
        }
    }
}

/**
 * DispatchDBusMessages() dispatches incoming dbus messages
 * (indirectly invoking the callbacks), then it sends outgoing
 * messages which needs to be sent on the bus (method replies and signals)
 *
 * This function must be called with p_sys->lock unlocked
 *
 * @param intf_thread_t *p_intf This interface thread state
 */
static void DispatchDBusMessages( intf_thread_t *p_intf )
{
    DBusDispatchStatus status;
    intf_sys_t *p_sys = p_intf->p_sys;

    /* Dispatch incoming messages */
    status = dbus_connection_get_dispatch_status( p_sys->p_conn );
    while( status != DBUS_DISPATCH_COMPLETE )
    {
        dbus_connection_dispatch( p_sys->p_conn );
        status = dbus_connection_get_dispatch_status( p_sys->p_conn );
    }

    /* Send outgoing data */
    if( dbus_connection_has_messages_to_send( p_sys->p_conn ) )
        dbus_connection_flush( p_sys->p_conn );
}

/**
 * MPRISEntryPoint() routes incoming messages to their respective interface
 * implementation.
 *
 * This function is called during dbus_connection_dispatch()
 */
static DBusHandlerResult
MPRISEntryPoint ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this )
{
    const char *psz_target_interface;
    const char *psz_interface = dbus_message_get_interface( p_from );
    const char *psz_method    = dbus_message_get_member( p_from );

    DBusError error;

    if( psz_interface && strcmp( psz_interface, DBUS_INTERFACE_PROPERTIES ) )
        psz_target_interface = psz_interface;

    else
    {
        dbus_error_init( &error );
        dbus_message_get_args( p_from, &error,
                               DBUS_TYPE_STRING, &psz_target_interface,
                               DBUS_TYPE_INVALID );

        if( dbus_error_is_set( &error ) )
        {
            msg_Err( (vlc_object_t*) p_this, "D-Bus error on %s.%s: %s",
                                             psz_interface, psz_method,
                                             error.message );
            dbus_error_free( &error );
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
    }

    if( !strcmp( psz_target_interface, DBUS_INTERFACE_INTROSPECTABLE ) )
        return handle_introspect( p_conn, p_from, p_this );

    if( !strcmp( psz_target_interface, DBUS_MPRIS_ROOT_INTERFACE ) )
        return handle_root( p_conn, p_from, p_this );

    if( !strcmp( psz_target_interface, DBUS_MPRIS_PLAYER_INTERFACE ) )
        return handle_player( p_conn, p_from, p_this );

    if( !strcmp( psz_target_interface, DBUS_MPRIS_TRACKLIST_INTERFACE ) )
        return handle_tracklist( p_conn, p_from, p_this );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/

static void *Run( void *data )
{
    intf_thread_t *p_intf = data;
    intf_sys_t    *p_sys = p_intf->p_sys;

    int canc = vlc_savecancel();

    vlc_tick_t events_last_date = VLC_TICK_INVALID;
    int events_poll_timeout = -1;
    for( ;; )
    {
        vlc_mutex_lock( &p_sys->lock );

        size_t i_watches = vlc_array_count( &p_sys->watches );
        struct pollfd fds[i_watches];
        memset(fds, 0, sizeof fds);

        int i_fds = GetPollFds( p_intf, fds );
        int timeout = next_timeout(p_intf);

        vlc_mutex_unlock( &p_sys->lock );

        /* thread cancellation is allowed while the main loop sleeps */
        vlc_restorecancel( canc );
        if( timeout == -1 )
            timeout = events_poll_timeout;

        while (poll(fds, i_fds, timeout) == -1)
        {
            if (errno != EINTR)
                goto error;
        }

        canc = vlc_savecancel();

        /* Was the main loop woken up manually ? */
        if (fds[0].revents & POLLIN)
        {
            char buf;
            (void)read( fds[0].fd, &buf, 1 );
        }

        /* We need to lock the mutex while building lists of events,
         * timeouts and watches to process but we can't keep the lock while
         * processing them, or else we risk a deadlock:
         *
         * The signal functions could lock mutex X while p_events is locked;
         * While some other function in vlc (playlist) might lock mutex X
         * and then set a variable which would call AllCallback(), which itself
         * needs to lock p_events to add a new event.
         */
        vlc_mutex_lock( &p_intf->p_sys->lock );

        process_timeouts(p_intf);

        /* Get the list of watches to process */
        i_watches = vlc_array_count( &p_sys->watches );
        DBusWatch *p_watches[i_watches ? i_watches : 1];
        for( size_t i = 0; i < i_watches; i++ )
        {
            p_watches[i] = vlc_array_item_at_index( &p_sys->watches, i );
        }

        /* Get the list of events to process */
        size_t i_events = vlc_array_count( &p_sys->events );
        callback_info_t** pp_info = NULL;

        if( i_events > 0 )
        {
            vlc_tick_t now = mdate();
            if( now - events_last_date > EVENTS_DELAY )
            {
                /* Send events every EVENTS_DELAY */
                events_last_date = now;
                events_poll_timeout = -1;

                pp_info = vlc_alloc( i_events, sizeof(*pp_info) );
                if( pp_info )
                {
                    for( size_t i = 0; i < i_events; i++ )
                        pp_info[i] = vlc_array_item_at_index( &p_sys->events, i );
                    vlc_array_clear( &p_sys->events );
                }
            }
            else if( events_poll_timeout == -1 )
            {
                /* Request poll to wake up in order to send these events after
                 * some delay */
                events_poll_timeout = ( EVENTS_DELAY - ( now - events_last_date ) ) / 1000;
            }
        }
        else /* No events: clear timeout */
            events_poll_timeout = -1;

        /* now we can release the lock and process what's pending */
        vlc_mutex_unlock( &p_intf->p_sys->lock );

        if( pp_info )
        {
            ProcessEvents( p_intf, pp_info, i_events );
            free( pp_info );
        }
        ProcessWatches( p_intf, p_watches, i_watches, fds, i_fds );

        DispatchDBusMessages( p_intf );
    }
error:
    vlc_restorecancel(canc);
    return NULL;
}

static void   wakeup_main_loop( void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;

    if( !write( p_intf->p_sys->p_pipe_fds[PIPE_IN], "\0", 1 ) )
        msg_Err( p_intf, "Could not wake up the main loop: %s",
                 vlc_strerror_c(errno) );
}

static bool add_event_locked( intf_thread_t *p_intf, callback_info_t *p_info )
{
    if( !p_info->signal )
    {
        free( p_info );
        return false;
    }

    for( size_t i = 0; i < vlc_array_count( &p_intf->p_sys->events ); ++ i )
    {
        callback_info_t *oldinfo =
            vlc_array_item_at_index( &p_intf->p_sys->events, i );
        if( p_info->signal == oldinfo->signal )
        {
            free( p_info );
            return false;
        }
    }

    vlc_array_append( &p_intf->p_sys->events, p_info );
    return true;
}

/* Flls a callback_info_t data structure in response
 * to an "intf-event" input event.
 *
 * @warning This function executes in the input thread.
 *
 * @return VLC_SUCCESS on success, VLC_E* on error.
 */
static int InputCallback( vlc_object_t *p_this, const char *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    intf_thread_t *p_intf = data;
    intf_sys_t *p_sys = p_intf->p_sys;

    dbus_int32_t i_state = PLAYBACK_STATE_INVALID;

    callback_info_t *p_info = calloc( 1, sizeof( callback_info_t ) );
    if( unlikely(p_info == NULL) )
        return VLC_ENOMEM;

    switch( newval.i_int )
    {
        case INPUT_EVENT_DEAD:
            i_state = PLAYBACK_STATE_STOPPED;
            break;
        case INPUT_EVENT_STATE:
            switch( var_GetInteger( p_input, "state" ) )
            {
                case OPENING_S:
                case PLAYING_S:
                    i_state = PLAYBACK_STATE_PLAYING;
                    break;
                case PAUSE_S:
                    i_state = PLAYBACK_STATE_PAUSED;
                    break;
                default:
                    i_state = PLAYBACK_STATE_STOPPED;
            }
            break;
        case INPUT_EVENT_ITEM_META:
            p_info->signal = SIGNAL_INPUT_METADATA;
            break;
        case INPUT_EVENT_RATE:
            p_info->signal = SIGNAL_RATE;
            break;
        case INPUT_EVENT_POSITION:
        {
            vlc_tick_t i_now = mdate(), i_pos, i_projected_pos, i_interval;
            float f_current_rate;

            /* Detect seeks
             * XXX: This is way more convoluted than it should be... */
            i_pos = var_GetInteger( p_input, "time" );

            if( !p_intf->p_sys->i_last_input_pos_event ||
                !( var_GetInteger( p_input, "state" ) == PLAYING_S ) )
            {
                p_intf->p_sys->i_last_input_pos_event = i_now;
                p_intf->p_sys->i_last_input_pos = i_pos;
                break;
            }

            f_current_rate = var_GetFloat( p_input, "rate" );
            i_interval = ( i_now - p_intf->p_sys->i_last_input_pos_event );

            i_projected_pos = p_intf->p_sys->i_last_input_pos +
                ( i_interval * f_current_rate );

            p_intf->p_sys->i_last_input_pos_event = i_now;
            p_intf->p_sys->i_last_input_pos = i_pos;

            if( llabs( i_pos - i_projected_pos ) < SEEK_THRESHOLD )
                break;

            p_info->signal = SIGNAL_SEEK;
            break;
        }
        default:
            free( p_info );
            return VLC_SUCCESS; /* don't care */
    }

    vlc_mutex_lock( &p_sys->lock );
    if( i_state != PLAYBACK_STATE_INVALID &&
        i_state != p_sys->i_playing_state )
    {
        p_sys->i_playing_state = i_state;
        p_info->signal = SIGNAL_STATE;
    }
    bool added = add_event_locked( p_intf, p_info );
    vlc_mutex_unlock( &p_intf->p_sys->lock );

    if( added )
        wakeup_main_loop( p_intf );

    (void)psz_var;
    (void)oldval;
    return VLC_SUCCESS;
}

// Get all the callbacks
static int AllCallback( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = p_data;
    callback_info_t info = { .signal = SIGNAL_NONE };

    // Wich event is it ?
    if( !strcmp( "input-current", psz_var ) )
        info.signal = SIGNAL_ITEM_CURRENT;
    else if( !strcmp( "volume", psz_var ) )
    {
        if( oldval.f_float != newval.f_float )
            info.signal = SIGNAL_VOLUME_CHANGE;
    }
    else if( !strcmp( "mute", psz_var ) )
    {
        if( oldval.b_bool != newval.b_bool )
            info.signal = SIGNAL_VOLUME_MUTED;
    }
    else if( !strcmp( "playlist-item-append", psz_var ) )
    {
        playlist_item_t *items[] = {newval.p_address};
        info = (callback_info_t){
            .signal = SIGNAL_PLAYLIST_ITEM_APPEND,
            .items_appended = tracklist_append_event_create(items[0]->i_id, items, 1)
        };
    }
    else if( !strcmp( "playlist-item-deleted", psz_var ) )
    {
        playlist_item_t *item = newval.p_address;
        info = (callback_info_t){
            .signal = SIGNAL_PLAYLIST_ITEM_DELETED,
            .items_removed = tracklist_remove_event_create(item->i_id, 1)
        };
    }
    else if( !strcmp( "random", psz_var ) )
        info.signal = SIGNAL_RANDOM;
    else if( !strcmp( "fullscreen", psz_var ) )
        info.signal = SIGNAL_FULLSCREEN;
    else if( !strcmp( "repeat", psz_var ) )
        info.signal = SIGNAL_REPEAT;
    else if( !strcmp( "loop", psz_var ) )
        info.signal = SIGNAL_LOOP;
    else if( !strcmp( "can-seek", psz_var ) )
        info.signal = SIGNAL_CAN_SEEK;
    else if( !strcmp( "can-pause", psz_var ) )
        info.signal = SIGNAL_CAN_PAUSE;
    else
        vlc_assert_unreachable();

    if( info.signal == SIGNAL_NONE )
        return VLC_SUCCESS;

    callback_info_t *p_info = malloc( sizeof( *p_info ) );
    if( unlikely(p_info == NULL) )
        return VLC_ENOMEM;

    // Append the event
    *p_info = info;
    vlc_mutex_lock( &p_intf->p_sys->lock );
    bool added = add_event_locked( p_intf, p_info );
    vlc_mutex_unlock( &p_intf->p_sys->lock );

    if( added )
        wakeup_main_loop( p_intf );

    (void) p_this;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * TrackChange: callback on playlist "input-current"
 *****************************************************************************/
static int TrackChange( intf_thread_t *p_intf )
{
    intf_sys_t          *p_sys      = p_intf->p_sys;
    input_thread_t      *p_input    = NULL;
    input_item_t        *p_item     = NULL;

    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    if( p_sys->p_input )
    {
        var_DelCallback( p_sys->p_input, "intf-event", InputCallback, p_intf );
        var_DelCallback( p_sys->p_input, "can-pause", AllCallback, p_intf );
        var_DelCallback( p_sys->p_input, "can-seek", AllCallback, p_intf );
        vlc_object_release( p_sys->p_input );
        p_sys->p_input = NULL;
    }

    p_sys->b_meta_read = false;

    p_input = pl_CurrentInput( p_intf );
    if( !p_input )
    {
        return VLC_SUCCESS;
    }

    p_item = input_GetItem( p_input );
    if( !p_item )
    {
        vlc_object_release( p_input );
        return VLC_EGENERIC;
    }

    if( input_item_IsPreparsed( p_item ) )
        p_sys->b_meta_read = true;

    p_sys->p_input = p_input;
    var_AddCallback( p_input, "intf-event", InputCallback, p_intf );
    var_AddCallback( p_input, "can-pause", AllCallback, p_intf );
    var_AddCallback( p_input, "can-seek", AllCallback, p_intf );

    return VLC_SUCCESS;
}

/**
 * DemarshalSetPropertyValue() extracts the new property value from a
 * org.freedesktop.DBus.Properties.Set method call message.
 *
 * @return int VLC_SUCCESS on success
 * @param DBusMessage *p_msg a org.freedesktop.DBus.Properties.Set method call
 * @param void *p_arg placeholder for the demarshalled value
 */
int DemarshalSetPropertyValue( DBusMessage *p_msg, void *p_arg )
{
    int  i_type;
    bool b_valid_input = FALSE;
    DBusMessageIter in_args, variant;
    dbus_message_iter_init( p_msg, &in_args );

    do
    {
        i_type = dbus_message_iter_get_arg_type( &in_args );
        if( DBUS_TYPE_VARIANT == i_type )
        {
            dbus_message_iter_recurse( &in_args, &variant );
            dbus_message_iter_get_basic( &variant, p_arg );
            b_valid_input = TRUE;
        }
    } while( dbus_message_iter_next( &in_args ) );

    return b_valid_input ? VLC_SUCCESS : VLC_EGENERIC;
}

/*****************************************************************************
 * GetInputMeta: Fill a DBusMessage with the given input item metadata
 *****************************************************************************/

#define ADD_META( entry, type, data ) \
    if( data ) { \
        dbus_message_iter_open_container( &dict, DBUS_TYPE_DICT_ENTRY, \
                NULL, &dict_entry ); \
        dbus_message_iter_append_basic( &dict_entry, DBUS_TYPE_STRING, \
                &ppsz_meta_items[entry] ); \
        dbus_message_iter_open_container( &dict_entry, DBUS_TYPE_VARIANT, \
                type##_AS_STRING, &variant ); \
        dbus_message_iter_append_basic( &variant, \
                type, \
                & data ); \
        dbus_message_iter_close_container( &dict_entry, &variant ); \
        dbus_message_iter_close_container( &dict, &dict_entry ); }

#define ADD_VLC_META_STRING( entry, item ) \
    { \
        char * psz = input_item_Get##item( p_input );\
        ADD_META( entry, DBUS_TYPE_STRING, \
                  psz ); \
        free( psz ); \
    }

#define ADD_META_SINGLETON_STRING_LIST( entry, item ) \
    { \
        char * psz = input_item_Get##item( p_input );\
        if( psz ) { \
            dbus_message_iter_open_container( &dict, DBUS_TYPE_DICT_ENTRY, \
                    NULL, &dict_entry ); \
            dbus_message_iter_append_basic( &dict_entry, DBUS_TYPE_STRING, \
                    &ppsz_meta_items[entry] ); \
            dbus_message_iter_open_container( &dict_entry, DBUS_TYPE_VARIANT, \
                    "as", &variant ); \
            dbus_message_iter_open_container( &variant, DBUS_TYPE_ARRAY, "s", \
                                              &list ); \
            dbus_message_iter_append_basic( &list, \
                    DBUS_TYPE_STRING, \
                    &psz ); \
            dbus_message_iter_close_container( &variant, &list ); \
            dbus_message_iter_close_container( &dict_entry, &variant ); \
            dbus_message_iter_close_container( &dict, &dict_entry ); \
        } \
        free( psz ); \
    }

int GetInputMeta( playlist_item_t *item, DBusMessageIter *args )
{
    input_item_t *p_input = item->p_input;
    DBusMessageIter dict, dict_entry, variant, list;
    /** The duration of the track can be expressed in second, milli-seconds and
        µ-seconds */
    dbus_int64_t i_mtime = input_item_GetDuration( p_input );
    dbus_uint32_t i_time = i_mtime / 1000000;
    dbus_int64_t i_length = i_mtime / 1000;
    char *psz_trackid;

    if( -1 == asprintf( &psz_trackid, MPRIS_TRACKID_FORMAT, item->i_id ) )
        return VLC_ENOMEM;

    const char* ppsz_meta_items[] =
    {
        "mpris:trackid", "xesam:url", "xesam:title", "xesam:artist",
        "xesam:album", "xesam:tracknumber", "vlc:time", "mpris:length",
        "xesam:genre", "xesam:userRating", "xesam:contentCreated",
        "mpris:artUrl", "mb:trackId", "vlc:audio-bitrate",
        "vlc:audio-samplerate", "vlc:video-bitrate", "vlc:audio-codec",
        "vlc:copyright", "xesam:comment", "vlc:encodedby", "language",
        "vlc:length", "vlc:nowplaying", "vlc:publisher", "vlc:setting",
        "status", "vlc:url", "vlc:video-codec"
    };

    dbus_message_iter_open_container( args, DBUS_TYPE_ARRAY, "{sv}", &dict );

    ADD_META( 0, DBUS_TYPE_OBJECT_PATH, psz_trackid );
    ADD_VLC_META_STRING( 1,  URI );
    ADD_VLC_META_STRING( 2,  Title );
    ADD_META_SINGLETON_STRING_LIST( 3,  Artist );
    ADD_VLC_META_STRING( 4,  Album );
    ADD_VLC_META_STRING( 5,  TrackNum );
    ADD_META( 6, DBUS_TYPE_UINT32, i_time );
    ADD_META( 7, DBUS_TYPE_INT64,  i_mtime );
    ADD_META_SINGLETON_STRING_LIST( 8,  Genre );
    //ADD_META( 9, DBUS_TYPE_DOUBLE, rating );
    ADD_VLC_META_STRING( 10, Date ); // this is supposed to be in ISO 8601 extended format
    ADD_VLC_META_STRING( 11, ArtURL );
    ADD_VLC_META_STRING( 12, TrackID );

    ADD_VLC_META_STRING( 17, Copyright );
    ADD_META_SINGLETON_STRING_LIST( 18, Description );
    ADD_VLC_META_STRING( 19, EncodedBy );
    ADD_VLC_META_STRING( 20, Language );
    ADD_META( 21, DBUS_TYPE_INT64, i_length );
    ADD_VLC_META_STRING( 22, NowPlaying );
    ADD_VLC_META_STRING( 23, Publisher );
    ADD_VLC_META_STRING( 24, Setting );
    ADD_VLC_META_STRING( 25, URL );

    free( psz_trackid );

    vlc_mutex_lock( &p_input->lock );
    if( p_input->p_meta )
    {
        int i_status = vlc_meta_GetStatus( p_input->p_meta );
        ADD_META( 23, DBUS_TYPE_INT32, i_status );
    }
    vlc_mutex_unlock( &p_input->lock );

    dbus_message_iter_close_container( args, &dict );
    return VLC_SUCCESS;
}

int AddProperty( intf_thread_t *p_intf,
                 DBusMessageIter *p_container,
                 const char* psz_property_name,
                 const char* psz_signature,
                 int (*pf_marshaller) (intf_thread_t*, DBusMessageIter*) )
{
    DBusMessageIter entry, v;

    if( !dbus_message_iter_open_container( p_container,
                                           DBUS_TYPE_DICT_ENTRY, NULL,
                                           &entry ) )
        return VLC_ENOMEM;

    if( !dbus_message_iter_append_basic( &entry,
                                         DBUS_TYPE_STRING,
                                         &psz_property_name ) )
        return VLC_ENOMEM;

    if( !dbus_message_iter_open_container( &entry,
                                           DBUS_TYPE_VARIANT, psz_signature,
                                           &v ) )
        return VLC_ENOMEM;

    if( VLC_SUCCESS != pf_marshaller( p_intf, &v ) )
        return VLC_ENOMEM;

    if( !dbus_message_iter_close_container( &entry, &v) )
        return VLC_ENOMEM;

    if( !dbus_message_iter_close_container( p_container, &entry ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

#undef ADD_META
#undef ADD_VLC_META_STRING


