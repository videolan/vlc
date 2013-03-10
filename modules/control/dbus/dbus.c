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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_mtime.h>
#include <vlc_fs.h>

#include <assert.h>
#include <string.h>

#include <poll.h>
#include <errno.h>
#include <unistd.h>

#define DBUS_MPRIS_BUS_NAME "org.mpris.MediaPlayer2.vlc"
#define DBUS_INSTANCE_ID_PREFIX "instance"

#define SEEK_THRESHOLD 1000 /* µsec */

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
    int i_node;
    int i_item;
} callback_info_t;

typedef struct
{
    mtime_t      i_remaining;
    DBusTimeout *p_timeout;
} timeout_info_t;

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

static dbus_bool_t add_timeout ( DBusTimeout *p_timeout, void *p_data );
static dbus_bool_t add_watch   ( DBusWatch *p_watch, void *p_data );

static void remove_timeout  ( DBusTimeout *p_timeout, void *p_data );
static void remove_watch    ( DBusWatch *p_watch, void *p_data );

static void timeout_toggled ( DBusTimeout *p_timeout, void *p_data );
static void watch_toggled   ( DBusWatch *p_watch, void *p_data );

static void wakeup_main_loop( void *p_data );

static int UpdateTimeouts( intf_thread_t *p_intf, mtime_t i_lastrun );

static void ProcessEvents  ( intf_thread_t    *p_intf,
                             callback_info_t **p_events,
                             int               i_events );

static void ProcessWatches ( intf_thread_t    *p_intf,
                             DBusWatch       **p_watches,
                             int               i_watches,
                             struct pollfd    *p_fds,
                             int               i_fds );

static void ProcessTimeouts( intf_thread_t    *p_intf,
                             DBusTimeout     **p_timeouts,
                             int               i_timeouts );

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
        free( p_sys );
        return VLC_EGENERIC;
    }

    dbus_connection_set_exit_on_disconnect( p_conn, FALSE );

    /* register an instance-specific well known name of the form
     * org.mpris.MediaPlayer2.vlc.instanceXXXX where XXXX is the
     * current process's pid */
    size_t i_length = sizeof( DBUS_MPRIS_BUS_NAME ) +
        sizeof( DBUS_INSTANCE_ID_PREFIX ) + 10;

    char unique_service[i_length];

    snprintf( unique_service, sizeof (unique_service),
            DBUS_MPRIS_BUS_NAME"."DBUS_INSTANCE_ID_PREFIX"%"PRIu32,
            (uint32_t)getpid() );

    dbus_bus_request_name( p_conn, unique_service, 0, &error );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( p_this, "Error requesting service name %s: %s",
                 unique_service, error.message );
        dbus_error_free( &error );
        free( p_sys );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_intf, "listening on dbus as: %s", unique_service );

    /* Try to register org.mpris.MediaPlayer2.vlc as well in case we are
     * the only VLC instance currently connected to the bus */
    dbus_bus_request_name( p_conn, DBUS_MPRIS_BUS_NAME, 0, NULL );

    /* Register the entry point object path */
    dbus_connection_register_object_path( p_conn, DBUS_MPRIS_OBJECT_PATH,
            &dbus_mpris_vtable, p_this );

    dbus_connection_flush( p_conn );

    p_intf->p_sys = p_sys;
    p_sys->p_conn = p_conn;
    p_sys->p_events = vlc_array_new();
    p_sys->p_timeouts = vlc_array_new();
    p_sys->p_watches = vlc_array_new();
    vlc_mutex_init( &p_sys->lock );

    p_playlist = pl_Get( p_intf );
    p_sys->p_playlist = p_playlist;

    var_AddCallback( p_playlist, "activity", AllCallback, p_intf );
    var_AddCallback( p_playlist, "intf-change", AllCallback, p_intf );
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
                                                timeout_toggled,
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
    /* The dbus connection is private,
     * so we are responsible for closing it
     * XXX: Does this make sense when OOM ? */
    dbus_connection_close( p_sys->p_conn );
    dbus_connection_unref( p_conn );

    vlc_array_destroy( p_sys->p_events );
    vlc_array_destroy( p_sys->p_timeouts );
    vlc_array_destroy( p_sys->p_watches );

    vlc_mutex_destroy( &p_sys->lock );

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

    var_DelCallback( p_playlist, "activity", AllCallback, p_intf );
    var_DelCallback( p_playlist, "intf-change", AllCallback, p_intf );
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
    for( int i = 0; i < vlc_array_count( p_sys->p_events ); i++ )
    {
        callback_info_t* info = vlc_array_item_at_index( p_sys->p_events, i );
        free( info );
    }
    vlc_mutex_destroy( &p_sys->lock );
    vlc_array_destroy( p_sys->p_events );
    vlc_array_destroy( p_sys->p_timeouts );
    vlc_array_destroy( p_sys->p_watches );
    free( p_sys );
}

static dbus_bool_t add_timeout( DBusTimeout *p_timeout, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    intf_sys_t    *p_sys  = (intf_sys_t*) p_intf->p_sys;

    timeout_info_t *p_info = calloc( 1, sizeof( timeout_info_t ) );
    p_info->i_remaining = dbus_timeout_get_interval( p_timeout ) * 1000;/* µs */
    p_info->p_timeout = p_timeout;

    dbus_timeout_set_data( p_timeout, p_info, free );

    vlc_mutex_lock( &p_sys->lock );
    vlc_array_append( p_sys->p_timeouts, p_timeout );
    vlc_mutex_unlock( &p_sys->lock );

    return TRUE;
}

static void remove_timeout( DBusTimeout *p_timeout, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    intf_sys_t    *p_sys  = (intf_sys_t*) p_intf->p_sys;

    vlc_mutex_lock( &p_sys->lock );

    vlc_array_remove( p_sys->p_timeouts,
                      vlc_array_index_of_item( p_sys->p_timeouts, p_timeout ) );

    vlc_mutex_unlock( &p_sys->lock );
}

static void timeout_toggled( DBusTimeout *p_timeout, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;

    if( dbus_timeout_get_enabled( p_timeout ) )
        wakeup_main_loop( p_intf );
}

static dbus_bool_t add_watch( DBusWatch *p_watch, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    intf_sys_t    *p_sys  = (intf_sys_t*) p_intf->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    vlc_array_append( p_sys->p_watches, p_watch );
    vlc_mutex_unlock( &p_sys->lock );

    return TRUE;
}

static void remove_watch( DBusWatch *p_watch, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    intf_sys_t    *p_sys  = (intf_sys_t*) p_intf->p_sys;

    vlc_mutex_lock( &p_sys->lock );

    vlc_array_remove( p_sys->p_watches,
                      vlc_array_index_of_item( p_sys->p_watches, p_watch ) );

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
    int i_fds = 1, i_watches = vlc_array_count( p_sys->p_watches );

    p_fds[0].fd = p_sys->p_pipe_fds[PIPE_OUT];
    p_fds[0].events = POLLIN | POLLPRI;

    for( int i = 0; i < i_watches; i++ )
    {
        DBusWatch *p_watch = NULL;
        p_watch = vlc_array_item_at_index( p_sys->p_watches, i );
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
 * UpdateTimeouts() updates the remaining time for each timeout and
 * returns how much time is left until the next timeout.
 *
 * This function must be called with p_sys->lock locked
 *
 * @return int The time remaining until the next timeout, in milliseconds
 * or -1 if there are no timeouts
 *
 * @param intf_thread_t *p_intf This interface thread's state
 * @param mtime_t i_loop_interval The time which has elapsed since the last
 * call to this function
 */
static int UpdateTimeouts( intf_thread_t *p_intf, mtime_t i_loop_interval )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    mtime_t i_next_timeout = LAST_MDATE;
    unsigned int i_timeouts = vlc_array_count( p_sys->p_timeouts );

    if( 0 == i_timeouts )
        return -1;

    for( unsigned int i = 0; i < i_timeouts; i++ )
    {
        timeout_info_t *p_info = NULL;
        DBusTimeout    *p_timeout = NULL;
        mtime_t         i_interval = 0;

        p_timeout = vlc_array_item_at_index( p_sys->p_timeouts, i );
        i_interval = dbus_timeout_get_interval( p_timeout ) * 1000; /* µs */
        p_info = (timeout_info_t*) dbus_timeout_get_data( p_timeout );

        p_info->i_remaining -= __MAX( 0, i_loop_interval ) % i_interval;

        if( !dbus_timeout_get_enabled( p_timeout ) )
            continue;

        /* The correct poll timeout value is the shortest one
         * in the dbus timeouts list */
        i_next_timeout = __MIN( i_next_timeout,
                                __MAX( 0, p_info->i_remaining ) );
    }

    /* next timeout in milliseconds */
    return i_next_timeout / 1000;
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
    playlist_t *p_playlist = p_intf->p_sys->p_playlist;
    bool        b_can_play = p_intf->p_sys->b_can_play;

    vlc_dictionary_t player_properties, tracklist_properties, root_properties;
    vlc_dictionary_init( &player_properties,    0 );
    vlc_dictionary_init( &tracklist_properties, 0 );
    vlc_dictionary_init( &root_properties,      0 );

    for( int i = 0; i < i_events; i++ )
    {
        switch( p_events[i]->signal )
        {
        case SIGNAL_ITEM_CURRENT:
            TrackChange( p_intf );

            // rate depends on current item
            if( !vlc_dictionary_has_key( &tracklist_properties, "Rate" ) )
                vlc_dictionary_insert( &player_properties, "Rate", NULL );

            vlc_dictionary_insert( &player_properties, "Metadata", NULL );
            break;
        case SIGNAL_INTF_CHANGE:
        case SIGNAL_PLAYLIST_ITEM_APPEND:
        case SIGNAL_PLAYLIST_ITEM_DELETED:
            PL_LOCK;
            b_can_play = playlist_CurrentSize( p_playlist ) > 0;
            PL_UNLOCK;

            if( b_can_play != p_intf->p_sys->b_can_play )
            {
                p_intf->p_sys->b_can_play = b_can_play;
                vlc_dictionary_insert( &player_properties, "CanPlay", NULL );
            }

            if( !vlc_dictionary_has_key( &tracklist_properties, "Tracks" ) )
                vlc_dictionary_insert( &tracklist_properties, "Tracks", NULL );
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
            input_thread_t *p_input = playlist_CurrentInput( p_playlist );
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
        {
            input_thread_t *p_input;
            input_item_t *p_item;
            p_input = playlist_CurrentInput( p_intf->p_sys->p_playlist );
            if( p_input )
            {
                p_item = input_GetItem( p_input );
                vlc_object_release( p_input );

                if( p_item && ( p_item->i_id == p_events[i]->i_item ) )
                    SeekedEmit( p_intf );
            }
            break;
        }
        default:
            assert(0);
        }
        free( p_events[i] );
    }

    if( vlc_dictionary_keys_count( &player_properties ) )
        PlayerPropertiesChangedEmit( p_intf, &player_properties );

    if( vlc_dictionary_keys_count( &tracklist_properties ) )
        TrackListPropertiesChangedEmit( p_intf, &tracklist_properties );

    if( vlc_dictionary_keys_count( &root_properties ) )
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
 * ProcessTimeouts() handles DBus timeouts
 *
 * This function must be called with p_sys->lock locked
 *
 * @param intf_thread_t *p_intf This interface thread state
 * @param DBusTimeout **p_timeouts List of timeouts to process
 * @param int i_timeouts Size of p_timeouts
 */
static void ProcessTimeouts( intf_thread_t *p_intf,
                             DBusTimeout  **p_timeouts, int i_timeouts )
{
    VLC_UNUSED( p_intf );

    for( int i = 0; i < i_timeouts; i++ )
    {
        timeout_info_t *p_info = NULL;

        p_info = (timeout_info_t*) dbus_timeout_get_data( p_timeouts[i] );

        if( !dbus_timeout_get_enabled( p_info->p_timeout ) )
            continue;

        if( p_info->i_remaining > 0 )
            continue;

        dbus_timeout_handle( p_info->p_timeout );
        p_info->i_remaining = dbus_timeout_get_interval( p_info->p_timeout );
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
    mtime_t        i_last_run = mdate();

    for( ;; )
    {
        int canc = vlc_savecancel();
        vlc_mutex_lock( &p_sys->lock );

        int i_watches = vlc_array_count( p_sys->p_watches );
        struct pollfd fds[i_watches];
        memset(fds, 0, sizeof fds);

        int i_fds = GetPollFds( p_intf, fds );

        mtime_t i_now = mdate(), i_loop_interval = i_now - i_last_run;

        int i_next_timeout = UpdateTimeouts( p_intf, i_loop_interval );
        i_last_run = i_now;

        vlc_mutex_unlock( &p_sys->lock );

        /* thread cancellation is allowed while the main loop sleeps */
        vlc_restorecancel( canc );

        int i_pollres = poll( fds, i_fds, i_next_timeout );

        canc = vlc_savecancel();

        if( -1 == i_pollres )
        { /* XXX: What should we do when poll() fails ? */
            msg_Err( p_intf, "poll() failed: %m" );
            vlc_restorecancel( canc );
            continue;
        }

        /* Was the main loop woken up manually ? */
        if( 0 < i_pollres && ( fds[0].revents & POLLIN ) )
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

        /* Get the list of timeouts to process */
        unsigned int i_timeouts = vlc_array_count( p_sys->p_timeouts );
        DBusTimeout *p_timeouts[i_timeouts];
        for( unsigned int i = 0; i < i_timeouts; i++ )
        {
            p_timeouts[i] = vlc_array_item_at_index( p_sys->p_timeouts, i );
        }

        /* Get the list of watches to process */
        i_watches = vlc_array_count( p_sys->p_watches );
        DBusWatch *p_watches[i_watches];
        for( int i = 0; i < i_watches; i++ )
        {
            p_watches[i] = vlc_array_item_at_index( p_sys->p_watches, i );
        }

        /* Get the list of events to process */
        int i_events = vlc_array_count( p_intf->p_sys->p_events );
        callback_info_t* p_info[i_events];
        for( int i = i_events - 1; i >= 0; i-- )
        {
            p_info[i] = vlc_array_item_at_index( p_intf->p_sys->p_events, i );
            vlc_array_remove( p_intf->p_sys->p_events, i );
        }

        /* now we can release the lock and process what's pending */
        vlc_mutex_unlock( &p_intf->p_sys->lock );

        ProcessEvents( p_intf, p_info, i_events );
        ProcessWatches( p_intf, p_watches, i_watches, fds, i_fds );

        ProcessTimeouts( p_intf, p_timeouts, i_timeouts );
        DispatchDBusMessages( p_intf );

        vlc_restorecancel( canc );
    }
    assert(0);
}

static void   wakeup_main_loop( void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;

    if( !write( p_intf->p_sys->p_pipe_fds[PIPE_IN], "\0", 1 ) )
        msg_Err( p_intf, "Could not wake up the main loop: %m" );
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
        case INPUT_EVENT_ABORT:
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
            mtime_t i_now = mdate(), i_pos, i_projected_pos, i_interval;
            float f_current_rate;

            /* Detect seeks
             * XXX: This is way more convoluted than it should be... */
            i_pos = var_GetTime( p_input, "time" );

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
            p_info->i_item = input_GetItem( p_input )->i_id;
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
    if( p_info->signal )
        vlc_array_append( p_intf->p_sys->p_events, p_info );
    else
        free( p_info );
    vlc_mutex_unlock( &p_intf->p_sys->lock );

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
    if( !strcmp( "activity", psz_var ) )
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
    else if( !strcmp( "intf-change", psz_var ) )
        info.signal = SIGNAL_INTF_CHANGE;
    else if( !strcmp( "playlist-item-append", psz_var ) )
    {
        info.signal = SIGNAL_PLAYLIST_ITEM_APPEND;
        info.i_node = ((playlist_add_t*)newval.p_address)->i_node;
    }
    else if( !strcmp( "playlist-item-deleted", psz_var ) )
        info.signal = SIGNAL_PLAYLIST_ITEM_DELETED;
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
        assert(0);

    if( info.signal == SIGNAL_NONE )
        return VLC_SUCCESS;

    callback_info_t *p_info = malloc( sizeof( *p_info ) );
    if( unlikely(p_info == NULL) )
        return VLC_ENOMEM;

    // Append the event
    *p_info = info;
    vlc_mutex_lock( &p_intf->p_sys->lock );
    vlc_array_append( p_intf->p_sys->p_events, p_info );
    vlc_mutex_unlock( &p_intf->p_sys->lock );

    wakeup_main_loop( p_intf );
    (void) p_this;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * TrackChange: callback on playlist "activity"
 *****************************************************************************/
static int TrackChange( intf_thread_t *p_intf )
{
    intf_sys_t          *p_sys      = p_intf->p_sys;
    playlist_t          *p_playlist = p_sys->p_playlist;
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

    p_input = playlist_CurrentInput( p_playlist );
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

int GetInputMeta( input_item_t* p_input,
                  DBusMessageIter *args )
{
    DBusMessageIter dict, dict_entry, variant, list;
    /** The duration of the track can be expressed in second, milli-seconds and
        µ-seconds */
    dbus_int64_t i_mtime = input_item_GetDuration( p_input );
    dbus_uint32_t i_time = i_mtime / 1000000;
    dbus_int64_t i_length = i_mtime / 1000;
    char *psz_trackid;

    if( -1 == asprintf( &psz_trackid, MPRIS_TRACKID_FORMAT, p_input->i_id ) )
        return VLC_ENOMEM;

    const char* ppsz_meta_items[] =
    {
    "mpris:trackid", "xesam:url", "xesam:title", "xesam:artist", "xesam:album",
    "xesam:tracknumber", "vlc:time", "mpris:length", "xesam:genre",
    "xesam:userRating", "xesam:contentCreated", "mpris:artUrl", "mb:trackId",
    "vlc:audio-bitrate", "vlc:audio-samplerate", "vlc:video-bitrate",
    "vlc:audio-codec", "vlc:copyright", "xesam:comment", "vlc:encodedby",
    "language", "vlc:length", "vlc:nowplaying", "vlc:publisher", "vlc:setting",
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


