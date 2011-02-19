/*****************************************************************************
 * dbus.c : D-Bus control interface
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2010 Mirsal Ennaime
 * Copyright © 2009-2010 The VideoLAN team
 * $Id$
 *
 * Authors:    Rafaël Carré <funman at videolanorg>
 *             Mirsal Ennaime <mirsal at mirsal fr>
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
#include "dbus.h"
#include "dbus_common.h"
#include "dbus_root.h"
#include "dbus_player.h"
#include "dbus_tracklist.h"

#include <vlc_common.h>
#include <vlc_fixups.h>
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

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

typedef struct
{
    int signal;
    int i_node;
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
static void Run     ( intf_thread_t * );

static int StateChange( intf_thread_t * );
static int TrackChange( intf_thread_t * );
static int AllCallback( vlc_object_t*, const char*, vlc_value_t, vlc_value_t, void* );

static void dispatch_status_cb( DBusConnection *p_conn,
                                DBusDispatchStatus i_status,
                                void *p_data);

static dbus_bool_t add_timeout ( DBusTimeout *p_timeout, void *p_data );
static dbus_bool_t add_watch   ( DBusWatch *p_watch, void *p_data );

static void remove_timeout  ( DBusTimeout *p_timeout, void *p_data );
static void remove_watch    ( DBusWatch *p_watch, void *p_data );

static void timeout_toggled ( DBusTimeout *p_timeout, void *p_data );
static void watch_toggled   ( DBusWatch *p_watch, void *p_data );

static void wakeup_main_loop( void *p_data );

static int GetPollFds( intf_thread_t *p_intf, struct pollfd *p_fds );
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
#define DBUS_UNIQUE_TEXT N_("Unique DBUS service id (org.mpris.vlc-<pid>)")
#define DBUS_UNIQUE_LONGTEXT N_( \
    "Use a unique dbus service id to identify this VLC instance on the DBUS bus. " \
    "The process identifier (PID) is added to the service name: org.mpris.vlc-<pid>" )

vlc_module_begin ()
    set_shortname( N_("dbus"))
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("D-Bus control interface") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
    add_bool( "dbus-unique-service-id", false,
              DBUS_UNIQUE_TEXT, DBUS_UNIQUE_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/

static int Open( vlc_object_t *p_this )
{ /* initialisation of the connection */
    intf_thread_t   *p_intf = (intf_thread_t*)p_this;
    intf_sys_t      *p_sys  = malloc( sizeof( intf_sys_t ) );
    playlist_t      *p_playlist;
    DBusConnection  *p_conn;
    DBusError       error;
    char            *psz_service_name = NULL;

    if( !p_sys || !dbus_threads_init_default())
        return VLC_ENOMEM;

    p_sys->b_meta_read = false;
    p_sys->i_caps = CAPS_NONE;
    p_sys->b_dead = false;
    p_sys->p_input = NULL;
    p_sys->i_playing_state = -1;

    if( vlc_pipe( p_sys->p_pipe_fds ) )
    {
        free( p_sys );
        msg_Err( p_intf, "Could not create pipe" );
        return VLC_EGENERIC;
    }

    p_sys->b_unique = var_CreateGetBool( p_intf, "dbus-unique-service-id" );
    if( p_sys->b_unique )
    {
        if( asprintf( &psz_service_name, "%s-%d",
            DBUS_MPRIS_BUS_NAME, getpid() ) < 0 )
        {
            free( p_sys );
            return VLC_ENOMEM;
        }
    }
    else
    {
        psz_service_name = strdup(DBUS_MPRIS_BUS_NAME);
    }

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
        free( psz_service_name );
        free( p_sys );
        return VLC_EGENERIC;
    }

    dbus_connection_set_exit_on_disconnect( p_conn, FALSE );

    /* register a well-known name on the bus */
    dbus_bus_request_name( p_conn, psz_service_name, 0, &error );
    if( dbus_error_is_set( &error ) )
    {
        msg_Err( p_this, "Error requesting service %s: %s",
                 psz_service_name, error.message );
        dbus_error_free( &error );
        free( psz_service_name );
        free( p_sys );
        return VLC_EGENERIC;
    }
    msg_Info( p_intf, "listening on dbus as: %s", psz_service_name );
    free( psz_service_name );

    /* we register the objects */
    dbus_connection_register_object_path( p_conn, DBUS_MPRIS_ROOT_PATH,
            &dbus_mpris_root_vtable, p_this );
    dbus_connection_register_object_path( p_conn, DBUS_MPRIS_PLAYER_PATH,
            &dbus_mpris_player_vtable, p_this );
    dbus_connection_register_object_path( p_conn, DBUS_MPRIS_TRACKLIST_PATH,
            &dbus_mpris_tracklist_vtable, p_this );

    dbus_connection_flush( p_conn );

    p_intf->pf_run = Run;
    p_intf->p_sys = p_sys;
    p_sys->p_conn = p_conn;
    p_sys->p_events = vlc_array_new();
    p_sys->p_timeouts = vlc_array_new();
    p_sys->p_watches = vlc_array_new();
    vlc_mutex_init( &p_sys->lock );

    p_playlist = pl_Get( p_intf );
    p_sys->p_playlist = p_playlist;

    var_AddCallback( p_playlist, "item-current", AllCallback, p_intf );
    var_AddCallback( p_playlist, "intf-change", AllCallback, p_intf );
    var_AddCallback( p_playlist, "playlist-item-append", AllCallback, p_intf );
    var_AddCallback( p_playlist, "playlist-item-deleted", AllCallback, p_intf );
    var_AddCallback( p_playlist, "random", AllCallback, p_intf );
    var_AddCallback( p_playlist, "repeat", AllCallback, p_intf );
    var_AddCallback( p_playlist, "loop", AllCallback, p_intf );

    dbus_connection_set_dispatch_status_function( p_conn,
                                                  dispatch_status_cb,
                                                  p_intf, NULL );

    if( !dbus_connection_set_timeout_functions( p_conn,
                                                add_timeout,
                                                remove_timeout,
                                                timeout_toggled,
                                                p_intf, NULL ) )
    {
        dbus_connection_unref( p_conn );
        free( psz_service_name );
        free( p_sys );
        return VLC_ENOMEM;
    }

    if( !dbus_connection_set_watch_functions( p_conn,
                                              add_watch,
                                              remove_watch,
                                              watch_toggled,
                                              p_intf, NULL ) )
    {
        dbus_connection_unref( p_conn );
        free( psz_service_name );
        free( p_sys );
        return VLC_ENOMEM;
    }

/*     dbus_connection_set_wakeup_main_function( p_conn,
                                              wakeup_main_loop,
                                              p_intf, NULL); */

    UpdateCaps( p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/

static void Close   ( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf     = (intf_thread_t*) p_this;
    intf_sys_t      *p_sys      = p_intf->p_sys;
    playlist_t      *p_playlist = p_sys->p_playlist;

    var_DelCallback( p_playlist, "item-current", AllCallback, p_intf );
    var_DelCallback( p_playlist, "intf-change", AllCallback, p_intf );
    var_DelCallback( p_playlist, "playlist-item-append", AllCallback, p_intf );
    var_DelCallback( p_playlist, "playlist-item-deleted", AllCallback, p_intf );
    var_DelCallback( p_playlist, "random", AllCallback, p_intf );
    var_DelCallback( p_playlist, "repeat", AllCallback, p_intf );
    var_DelCallback( p_playlist, "loop", AllCallback, p_intf );

    if( p_sys->p_input )
    {
        var_DelCallback( p_sys->p_input, "intf-event", AllCallback, p_intf );
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

static void dispatch_status_cb( DBusConnection *p_conn,
                                DBusDispatchStatus i_status,
                                void *p_data)
{
    (void) p_conn;
    intf_thread_t *p_intf = (intf_thread_t*) p_data;

    static const char *p_statuses[] = { "DATA_REMAINS",
                                        "COMPLETE",
                                        "NEED_MEMORY" };

    msg_Dbg( p_intf,
             "DBus dispatch status changed to %s.",
             p_statuses[i_status]);
}

static dbus_bool_t add_timeout( DBusTimeout *p_timeout, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    intf_sys_t    *p_sys  = (intf_sys_t*) p_intf->p_sys;

    timeout_info_t *p_info = calloc( sizeof( timeout_info_t ), 1 );
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

    msg_Dbg( p_intf, "Toggling dbus timeout" );

    if( dbus_timeout_get_enabled( p_timeout ) )
    {
        msg_Dbg( p_intf, "Timeout is enabled, main loop needs to wake up" );
        wakeup_main_loop( p_intf );
    }
}

static dbus_bool_t add_watch( DBusWatch *p_watch, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    intf_sys_t    *p_sys  = (intf_sys_t*) p_intf->p_sys;
    int            i_fd   = dbus_watch_get_unix_fd( p_watch );

    msg_Dbg( p_intf, "Adding dbus watch on fd %d", i_fd );

    if( dbus_watch_get_flags( p_watch ) & DBUS_WATCH_READABLE )
        msg_Dbg( p_intf, "Watching fd %d for readability", i_fd );

    if( dbus_watch_get_flags( p_watch ) & DBUS_WATCH_WRITABLE )
        msg_Dbg( p_intf, "Watching fd %d for writeability", i_fd );

    vlc_mutex_lock( &p_sys->lock );
    vlc_array_append( p_sys->p_watches, p_watch );
    vlc_mutex_unlock( &p_sys->lock );

    return TRUE;
}

static void remove_watch( DBusWatch *p_watch, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;
    intf_sys_t    *p_sys  = (intf_sys_t*) p_intf->p_sys;

    msg_Dbg( p_intf, "Removing dbus watch on fd %d",
              dbus_watch_get_unix_fd( p_watch ) );

    vlc_mutex_lock( &p_sys->lock );

    vlc_array_remove( p_sys->p_watches,
                      vlc_array_index_of_item( p_sys->p_watches, p_watch ) );

    vlc_mutex_unlock( &p_sys->lock );
}

static void watch_toggled( DBusWatch *p_watch, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;

    msg_Dbg( p_intf, "Toggling dbus watch on fd %d",
             dbus_watch_get_unix_fd( p_watch ) );

    if( dbus_watch_get_enabled( p_watch ) )
    {
        msg_Dbg( p_intf,
                  "Watch on fd %d has been enabled, "
                  "the main loops needs to wake up",
                  dbus_watch_get_unix_fd( p_watch ) );

        wakeup_main_loop( p_intf );
    }
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
    for( int i = 0; i < i_events; i++ )
    {
        switch( p_events[i]->signal )
        {
        case SIGNAL_ITEM_CURRENT:
            TrackChange( p_intf );
            break;
        case SIGNAL_INTF_CHANGE:
        case SIGNAL_PLAYLIST_ITEM_APPEND:
        case SIGNAL_PLAYLIST_ITEM_DELETED:
            TrackListChangeEmit( p_intf,
                                 p_events[i]->signal,
                                 p_events[i]->i_node );
            break;
        case SIGNAL_RANDOM:
        case SIGNAL_REPEAT:
        case SIGNAL_LOOP:
            StatusChangeEmit( p_intf );
            break;
        case SIGNAL_STATE:
            StateChange( p_intf );
            break;
        case SIGNAL_INPUT_METADATA:
            break;
        default:
            assert(0);
        }
        free( p_events[i] );
    }
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
            int i_fd      = p_fds[j].fd;

            if( i_revents & POLLIN )
            {
                msg_Dbg( p_intf, "fd %d is ready for reading", i_fd );
                i_flags |= DBUS_WATCH_READABLE;
            }

            if( i_revents & POLLOUT )
            {
                msg_Dbg( p_intf, "fd %d is ready for writing", i_fd );
                i_flags |= DBUS_WATCH_WRITABLE;
            }

            if( i_revents & POLLERR )
            {
                msg_Dbg( p_intf, "error when polling fd %d", i_fd );
                i_flags |= DBUS_WATCH_ERROR;
            }

            if( i_revents & POLLHUP )
            {
                msg_Dbg( p_intf, "Hangup signal on fd %d", i_fd );
                i_flags |= DBUS_WATCH_HANGUP;
            }

            if( i_flags )
            {
                msg_Dbg( p_intf, "Handling dbus watch on fd %d", i_fd );
                dbus_watch_handle( p_watch, i_flags );
            }
            else
                msg_Dbg( p_intf, "Nothing happened on fd %d", i_fd );
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
        msg_Dbg( p_intf, "Dispatching incoming dbus message" );
        dbus_connection_dispatch( p_sys->p_conn );
        status = dbus_connection_get_dispatch_status( p_sys->p_conn );
    }

    /* Send outgoing data */
    if( dbus_connection_has_messages_to_send( p_sys->p_conn ) )
    {
        msg_Dbg( p_intf, "Sending outgoing data" );
        dbus_connection_flush( p_sys->p_conn );
    }
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/

static void Run          ( intf_thread_t *p_intf )
{
    intf_sys_t    *p_sys = p_intf->p_sys;
    mtime_t        i_last_run = mdate();

    for( ;; )
    {
        int canc = vlc_savecancel();
        vlc_mutex_lock( &p_sys->lock );

        int i_watches = vlc_array_count( p_sys->p_watches );
        struct pollfd *p_fds = calloc( sizeof( struct pollfd ), i_watches );

        int i_fds = GetPollFds( p_intf, p_fds );

        mtime_t i_now = mdate(), i_loop_interval = i_now - i_last_run;

        msg_Dbg( p_intf,
                 "%lld µs elapsed since last wakeup",
                 (long long) i_loop_interval );

        int i_next_timeout = UpdateTimeouts( p_intf, i_loop_interval );
        i_last_run = i_now;

        vlc_mutex_unlock( &p_sys->lock );

        if( -1 != i_next_timeout )
            msg_Dbg( p_intf, "next timeout is in %d ms", i_next_timeout );
        msg_Dbg( p_intf, "Sleeping until something happens" );

        /* thread cancellation is allowed while the main loop sleeps */
        vlc_restorecancel( canc );

        int i_pollres = poll( p_fds, i_fds, i_next_timeout );
        int i_errsv   = errno;

        canc = vlc_savecancel();

        msg_Dbg( p_intf, "the main loop has been woken up" );

        if( -1 == i_pollres )
        { /* XXX: What should we do when poll() fails ? */
            char buf[64];
            msg_Err( p_intf, "poll() failed: %s", strerror_r( i_errsv, buf, 64 ) );
            free( p_fds ); p_fds = NULL;
            vlc_restorecancel( canc );
            continue;
        }

        /* Was the main loop woken up manually ? */
        if( 0 < i_pollres && ( p_fds[0].revents & POLLIN ) )
        {
            char buf;
            msg_Dbg( p_intf, "Removing a byte from the self-pipe" );
            (void)read( p_fds[0].fd, &buf, 1 );
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
        ProcessWatches( p_intf, p_watches, i_watches, p_fds, i_fds );

        free( p_fds ); p_fds = NULL;

        ProcessTimeouts( p_intf, p_timeouts, i_timeouts );
        DispatchDBusMessages( p_intf );

        vlc_restorecancel( canc );
    }
}

static void   wakeup_main_loop( void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_data;

    msg_Dbg( p_intf, "Sending wakeup signal to the main loop" );

    if( !write( p_intf->p_sys->p_pipe_fds[PIPE_IN], "\0", 1 ) )
    {
        msg_Err( p_intf,
            "Could not wake up the main loop: %s", strerror( errno ) );
    }
}

/*****************************************************************************
 * UpdateCaps: update p_sys->i_caps
 * This function have to be called with the playlist unlocked
 ****************************************************************************/
int UpdateCaps( intf_thread_t* p_intf )
{
    intf_sys_t* p_sys = p_intf->p_sys;
    dbus_int32_t i_caps = CAPS_CAN_HAS_TRACKLIST;
    playlist_t* p_playlist = p_sys->p_playlist;

    PL_LOCK;
    if( p_playlist->current.i_size > 0 )
        i_caps |= CAPS_CAN_PLAY | CAPS_CAN_GO_PREV | CAPS_CAN_GO_NEXT;
    PL_UNLOCK;

    input_thread_t* p_input = playlist_CurrentInput( p_playlist );
    if( p_input )
    {
        /* XXX: if UpdateCaps() is called too early, these are
         * unconditionnaly true */
        if( var_GetBool( p_input, "can-pause" ) )
            i_caps |= CAPS_CAN_PAUSE;
        if( var_GetBool( p_input, "can-seek" ) )
            i_caps |= CAPS_CAN_SEEK;
        vlc_object_release( p_input );
    }

    if( p_sys->b_meta_read )
        i_caps |= CAPS_CAN_PROVIDE_METADATA;

    if( i_caps != p_intf->p_sys->i_caps )
    {
        p_sys->i_caps = i_caps;
        CapsChangeEmit( p_intf );
    }

    return VLC_SUCCESS;
}

/* InputIntfEventCallback() fills a callback_info_t data structure in response
 * to an "intf-event" input event.
 *
 * Caution: This function executes in the input thread
 *
 * This function must be called with p_sys->lock locked
 *
 * @return int VLC_SUCCESS on success, VLC_E* on error
 * @param intf_thread_t *p_intf the interface thread
 * @param input_thread_t *p_input This input thread
 * @param const int i_event input event type
 * @param callback_info_t *p_info Location of the callback info to fill
 */
static int InputIntfEventCallback( intf_thread_t   *p_intf,
                                   input_thread_t  *p_input,
                                   const int        i_event,
                                   callback_info_t *p_info )
{
    dbus_int32_t i_state = PLAYBACK_STATE_INVALID;
    assert(!p_info->signal);

    switch( i_event )
    {
        case INPUT_EVENT_DEAD:
        case INPUT_EVENT_ABORT:
            i_state = PLAYBACK_STATE_STOPPED;
            break;
        case INPUT_EVENT_STATE:
            i_state = ( var_GetInteger( p_input, "state" ) == PAUSE_S ) ?
                PLAYBACK_STATE_PAUSED : PLAYBACK_STATE_PLAYING;
            break;
        case INPUT_EVENT_ITEM_META:
            p_info->signal = SIGNAL_INPUT_METADATA;
            return VLC_SUCCESS;
        default:
            return VLC_EGENERIC;
    }

    if( i_state != p_intf->p_sys->i_playing_state )
    {
        p_intf->p_sys->i_playing_state = i_state;
        p_info->signal = SIGNAL_STATE;
    }

    return p_info->signal ? VLC_SUCCESS : VLC_EGENERIC;
}

// Get all the callbacks
static int AllCallback( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)p_this;
    (void)oldval;

    intf_thread_t *p_intf = (intf_thread_t*)p_data;
    callback_info_t *info = calloc( 1, sizeof( callback_info_t ) );

    if( !info )
        return VLC_ENOMEM;

    vlc_mutex_lock( &p_intf->p_sys->lock );

    // Wich event is it ?
    if( !strcmp( "item-current", psz_var ) )
        info->signal = SIGNAL_ITEM_CURRENT;

    else if( !strcmp( "intf-change", psz_var ) )
        info->signal = SIGNAL_INTF_CHANGE;

    else if( !strcmp( "playlist-item-append", psz_var ) )
    {
        info->signal = SIGNAL_PLAYLIST_ITEM_APPEND;
        info->i_node = ((playlist_add_t*)newval.p_address)->i_node;
    }

    else if( !strcmp( "playlist-item-deleted", psz_var ) )
        info->signal = SIGNAL_PLAYLIST_ITEM_DELETED;

    else if( !strcmp( "random", psz_var ) )
        info->signal = SIGNAL_RANDOM;

    else if( !strcmp( "repeat", psz_var ) )
        info->signal = SIGNAL_REPEAT;

    else if( !strcmp( "loop", psz_var ) )
        info->signal = SIGNAL_LOOP;

    else if( !strcmp( "intf-event", psz_var ) )
    {
        int i_res;
        i_res = InputIntfEventCallback( p_intf, p_this, newval.i_int, info );

        if( VLC_SUCCESS != i_res )
        {
            vlc_mutex_unlock( &p_intf->p_sys->lock );
            free( info );

            return i_res;
        }
    }

    else
        assert(0);

    // Append the event
    vlc_array_append( p_intf->p_sys->p_events, info );
    vlc_mutex_unlock( &p_intf->p_sys->lock );

    msg_Dbg( p_intf,
             "Got a VLC event on %s. The main loop needs to wake up "
             "in order to process it", psz_var );

    wakeup_main_loop( p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * StateChange: callback on input "state"
 *****************************************************************************/
static int StateChange( intf_thread_t *p_intf )
{
    intf_sys_t          *p_sys      = p_intf->p_sys;
    playlist_t          *p_playlist = p_sys->p_playlist;
    input_thread_t      *p_input;
    input_item_t        *p_item;

    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    UpdateCaps( p_intf );

    if( !p_sys->b_meta_read && p_sys->i_playing_state == 0)
    {
        p_input = playlist_CurrentInput( p_playlist );
        if( p_input )
        {
            p_item = input_GetItem( p_input );
            if( p_item )
            {
                p_sys->b_meta_read = true;
                TrackChangeEmit( p_intf, p_item );
            }
            vlc_object_release( p_input );
        }
    }

    StatusChangeEmit( p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * TrackChange: callback on playlist "item-current"
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
        var_DelCallback( p_sys->p_input, "intf-event", AllCallback, p_intf );
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
    {
        p_sys->b_meta_read = true;
        TrackChangeEmit( p_intf, p_item );
    }

    p_sys->p_input = p_input;
    var_AddCallback( p_input, "intf-event", AllCallback, p_intf );

    return VLC_SUCCESS;
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

int GetInputMeta( input_item_t* p_input,
                        DBusMessageIter *args )
{
    DBusMessageIter dict, dict_entry, variant;
    /** The duration of the track can be expressed in second, milli-seconds and
        µ-seconds */
    dbus_int64_t i_mtime = input_item_GetDuration( p_input );
    dbus_uint32_t i_time = i_mtime / 1000000;
    dbus_int64_t i_length = i_mtime / 1000;

    const char* ppsz_meta_items[] =
    {
    /* Official MPRIS metas */
    "location", "title", "artist", "album", "tracknumber", "time", "mtime",
    "genre", "rating", "date", "arturl",
    "audio-bitrate", "audio-samplerate", "video-bitrate",
    /* VLC specifics metas */
    "audio-codec", "copyright", "description", "encodedby", "language", "length",
    "nowplaying", "publisher", "setting", "status", "trackid", "url",
    "video-codec"
    };

    dbus_message_iter_open_container( args, DBUS_TYPE_ARRAY, "{sv}", &dict );

    ADD_VLC_META_STRING( 0,  URI );
    ADD_VLC_META_STRING( 1,  Title );
    ADD_VLC_META_STRING( 2,  Artist );
    ADD_VLC_META_STRING( 3,  Album );
    ADD_VLC_META_STRING( 4,  TrackNum );
    ADD_META( 5, DBUS_TYPE_UINT32, i_time );
    ADD_META( 6, DBUS_TYPE_UINT32, i_mtime );
    ADD_VLC_META_STRING( 7,  Genre );
    ADD_VLC_META_STRING( 8,  Rating );
    ADD_VLC_META_STRING( 9,  Date );
    ADD_VLC_META_STRING( 10, ArtURL );

    ADD_VLC_META_STRING( 15, Copyright );
    ADD_VLC_META_STRING( 16, Description );
    ADD_VLC_META_STRING( 17, EncodedBy );
    ADD_VLC_META_STRING( 18, Language );
    ADD_META( 19, DBUS_TYPE_INT64, i_length );
    ADD_VLC_META_STRING( 20, NowPlaying );
    ADD_VLC_META_STRING( 21, Publisher );
    ADD_VLC_META_STRING( 22, Setting );
    ADD_VLC_META_STRING( 24, TrackID );
    ADD_VLC_META_STRING( 25, URL );

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

#undef ADD_META
#undef ADD_VLC_META_STRING


