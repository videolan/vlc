/*****************************************************************************
 * screensaver.c : disable screen savers when VLC is playing
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Benjamin Pracht <bigben AT videolan DOT org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_interface.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#ifdef HAVE_DBUS

#include <dbus/dbus.h>

#define GS_SERVICE   "org.gnome.ScreenSaver"
#define GS_PATH      "/org/gnome/ScreenSaver"
#define GS_INTERFACE "org.gnome.ScreenSaver"

#define FDS_SERVICE   "org.freedesktop.ScreenSaver"
#define FDS_PATH      "/ScreenSaver"
#define FDS_INTERFACE "org.freedesktop.ScreenSaver"

#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void  Deactivate   ( vlc_object_t * );

static void Timer( void * );

#ifdef HAVE_DBUS

static DBusConnection * dbus_init( intf_thread_t *p_intf );
static void poke_screensaver( intf_thread_t *p_intf,
                              DBusConnection *p_connection );
static void screensaver_send_message_void ( intf_thread_t *p_intf,
                                       DBusConnection *p_connection,
                                       const char *psz_service,
                                       const char *psz_path,
                                       const char *psz_interface,
                                       const char *psz_name );
static bool screensaver_is_running( DBusConnection *p_connection, const char *psz_service );
#endif

struct intf_sys_t
{
#ifdef HAVE_DBUS
    DBusConnection *p_connection;
#endif
    vlc_timer_t timer;
};


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("X Screensaver disabler") )
    set_capability( "interface", 0 )
    set_callbacks( Activate, Deactivate )
vlc_module_end ()

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys;

    p_sys = p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    if( vlc_timer_create( &p_sys->timer, Timer, p_intf ) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    vlc_timer_schedule( &p_sys->timer, false, 30*CLOCK_FREQ, 30*CLOCK_FREQ );

#ifdef HAVE_DBUS
    p_sys->p_connection = dbus_init( p_intf );
#endif
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_timer_destroy( &p_sys->timer );
#ifdef HAVE_DBUS
    if( p_sys->p_connection )
        dbus_connection_unref( p_sys->p_connection );
#endif

    free( p_sys );
}

/*****************************************************************************
 * Execute: Spawns a process using execv()
 *****************************************************************************/
static void Execute( intf_thread_t *p_this, const char *const *ppsz_args )
{
    pid_t pid = fork();
    switch( pid )
    {
        case 0:     /* we're the child */
        {
            sigset_t set;
            sigemptyset (&set);
            pthread_sigmask (SIG_SETMASK, &set, NULL);

            /* We don't want output */
            if( ( freopen( "/dev/null", "w", stdout ) != NULL )
             && ( freopen( "/dev/null", "w", stderr ) != NULL ) )
                execv( ppsz_args[0] , (char *const *)ppsz_args );
            /* If the file we want to execute doesn't exist we exit() */
            exit( EXIT_FAILURE );
        }
        case -1:    /* we're the error */
            msg_Dbg( p_this, "Couldn't fork() while launching %s",
                     ppsz_args[0] );
            break;
        default:    /* we're the parent */
            /* Wait for the child to exit.
             * We will not deadlock because we ran "/bin/sh &" */
            while( waitpid( pid, NULL, 0 ) != pid);
            break;
    }
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************
 * This part of the module is in a separate thread so that we do not have
 * too much system() overhead.
 *****************************************************************************/
static void Timer( void *data )
{
    intf_thread_t *p_intf = data;
    playlist_t *p_playlist = pl_Hold( p_intf );
    input_thread_t *p_input = playlist_CurrentInput( p_playlist );
    pl_Release( p_intf );
    if( !p_input )
        return;

    vlc_object_t *p_vout;
    if( var_GetInteger( p_input, "state" ) == PLAYING_S )
        p_vout = (vlc_object_t *)input_GetVout( p_input );
    else
        p_vout = NULL;
    vlc_object_release( p_input );
    if( !p_vout )
        return;
    vlc_object_release( p_vout );

    /* If there is a playing video output, disable xscreensaver */
    /* http://www.jwz.org/xscreensaver/faq.html#dvd */
    const char *const ppsz_xsargs[] = { "/bin/sh", "-c",
        "xscreensaver-command -deactivate &", (char*)NULL };
    Execute( p_intf, ppsz_xsargs );

    /* If we have dbus support, let's communicate directly with
     * gnome-screensave else, run gnome-screensaver-command */
#ifdef HAVE_DBUS
    poke_screensaver( p_intf, p_intf->p_sys->p_connection );
#else
    const char *const ppsz_gsargs[] = { "/bin/sh", "-c",
        "gnome-screensaver-command --poke &", (char*)NULL };
    Execute( p_intf, ppsz_gsargs );
#endif
    /* FIXME: add support for other screensavers */
}

#ifdef HAVE_DBUS

static DBusConnection * dbus_init( intf_thread_t *p_intf )
{
    DBusError dbus_error;

    dbus_error_init (&dbus_error);
    DBusConnection * p_connection = dbus_bus_get( DBUS_BUS_SESSION, &dbus_error );

    if ( !p_connection )
    {
        msg_Warn( p_intf, "failed to connect to the D-BUS daemon: %s",
                          dbus_error.message);
        dbus_error_free( &dbus_error );
        return NULL;
    }

    return p_connection;
}

static void poke_screensaver( intf_thread_t *p_intf,
                              DBusConnection *p_connection )
{
    if( screensaver_is_running( p_connection, GS_SERVICE ) )
    {
#   ifdef SCREENSAVER_DEBUG
        msg_Dbg( p_intf, "found a running gnome-screensaver instance" );
#   endif
        /* gnome-screensaver changed it's D-Bus interface, so we need both */
        screensaver_send_message_void( p_intf, p_connection, GS_SERVICE, GS_PATH,
                                       GS_INTERFACE, "Poke" );
        screensaver_send_message_void( p_intf, p_connection, GS_SERVICE, GS_PATH,
                                       GS_INTERFACE, "SimulateUserActivity" );
    }
    else if( screensaver_is_running( p_connection, FDS_SERVICE ) )
    {
#   ifdef SCREENSAVER_DEBUG
        msg_Dbg( p_intf, "found a running freedesktop-screensaver instance" );
#   endif
        screensaver_send_message_void( p_intf, p_connection, FDS_SERVICE, FDS_PATH,
                                       FDS_INTERFACE, "SimulateUserActivity" );
    }
#   ifdef SCREENSAVER_DEBUG
    else
    {
        msg_Dbg( p_intf, "found no running (gnome|freedesktop)-screensaver instance" );
    }
#   endif

}

static void screensaver_send_message_void ( intf_thread_t *p_intf,
                                       DBusConnection *p_connection,
                                       const char *psz_service,
                                       const char *psz_path,
                                       const char *psz_interface,
                                       const char *psz_name )
{
    DBusMessage *p_message;

    if( !p_connection || !psz_name ) return;

    p_message = dbus_message_new_method_call( psz_service, psz_path,
                                              psz_interface, psz_name );
    if( p_message == NULL )
    {
        msg_Err( p_intf, "DBUS initialization failed: message initialization" );
        return;
    }

    if( !dbus_connection_send( p_connection, p_message, NULL ) )
    {
        msg_Err( p_intf, "DBUS communication failed" );
    }

    dbus_connection_flush( p_connection );

    dbus_message_unref( p_message );
}

static bool screensaver_is_running( DBusConnection *p_connection, const char *psz_service )
{
    DBusError error;
    bool b_return;

    if( !p_connection ) return false;

    dbus_error_init( &error );
    b_return = dbus_bus_name_has_owner( p_connection, psz_service, &error );
    if( dbus_error_is_set( &error ) ) dbus_error_free (&error);

    return b_return;
}

#endif

