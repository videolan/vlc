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
#include <vlc_interface.h>
#include <vlc_aout.h>
#include <vlc_vout.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef HAVE_SIGNAL_H
#   include <signal.h>
#endif

#ifdef HAVE_DBUS

#include <dbus/dbus.h>

#define GS_SERVICE   "org.gnome.ScreenSaver"
#define GS_PATH      "/org/gnome/ScreenSaver"
#define GS_INTERFACE "org.gnome.ScreenSaver"

#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void  Deactivate   ( vlc_object_t * );

static void Run          ( intf_thread_t *p_intf );

#ifdef HAVE_DBUS

static DBusConnection * dbus_init( intf_thread_t *p_intf );
static void poke_screensaver( intf_thread_t *p_intf,
                              DBusConnection *p_connection );
static void screensaver_send_message_void ( intf_thread_t *p_intf,
                                       DBusConnection *p_connection,
                                       const char *psz_name );
static bool screensaver_is_running( DBusConnection *p_connection );


struct intf_sys_t
{
    DBusConnection *p_connection;
};

#endif

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

    p_intf->pf_run = Run;

#ifdef HAVE_DBUS
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( !p_intf->p_sys ) return VLC_ENOMEM;
#endif

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
#ifdef HAVE_DBUS
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

    if( p_intf->p_sys->p_connection )
    {
        dbus_connection_unref( p_intf->p_sys->p_connection );
    }

    free( p_intf->p_sys );
    p_intf->p_sys = NULL;
#endif
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
static void Run( intf_thread_t *p_intf )
{
    int canc = vlc_savecancel();
#ifdef HAVE_DBUS
    p_intf->p_sys->p_connection = dbus_init( p_intf );
#endif

    for( ;; )
    {
        vlc_object_t *p_vout;

        p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );

        /* If there is a video output, disable xscreensaver */
        if( p_vout )
        {
            input_thread_t *p_input;
            p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT, FIND_PARENT );
            vlc_object_release( p_vout );
            if( p_input )
            {
                if( PLAYING_S == var_GetInteger( p_input, "state" ) )
                {
                    /* http://www.jwz.org/xscreensaver/faq.html#dvd */
                    const char *const ppsz_xsargs[] = { "/bin/sh", "-c",
                            "xscreensaver-command -deactivate &", (char*)NULL };
                    Execute( p_intf, ppsz_xsargs );

                    /* If we have dbus support, let's communicate directly
                       with gnome-screensave else, run
                       gnome-screensaver-command */
#ifdef HAVE_DBUS
                    poke_screensaver( p_intf, p_intf->p_sys->p_connection );
#else
                    const char *const ppsz_gsargs[] = { "/bin/sh", "-c",
                            "gnome-screensaver-command --poke &", (char*)NULL };
                    Execute( p_intf, ppsz_gsargs );
#endif
                    /* FIXME: add support for other screensavers */
                }
                vlc_object_release( p_input );
            }
        }

        vlc_restorecancel( canc );
        /* Check screensaver every 30 seconds */
        msleep( 30 * CLOCK_FREQ );
        canc = vlc_savecancel( );
    }
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
    if( screensaver_is_running( p_connection ) )
    {
#   ifdef SCREENSAVER_DEBUG
        msg_Dbg( p_intf, "found a running gnome-screensaver instance" );
#   endif
        /* gnome-screensaver changed it's D-Bus interface, so we need both */
        screensaver_send_message_void( p_intf, p_connection, "Poke" );
        screensaver_send_message_void( p_intf, p_connection,
                "SimulateUserActivity" );
    }
#   ifdef SCREENSAVER_DEBUG
    else
    {
        msg_Dbg( p_intf, "found no running gnome-screensaver instance" );
    }
#   endif
}

static void screensaver_send_message_void ( intf_thread_t *p_intf,
                                       DBusConnection *p_connection,
                                       const char *psz_name )
{
    DBusMessage *p_message;

    if( !p_connection || !psz_name ) return;

    p_message = dbus_message_new_method_call( GS_SERVICE, GS_PATH,
                                              GS_INTERFACE, psz_name );
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

static bool screensaver_is_running( DBusConnection *p_connection )
{
    DBusError error;
    bool b_return;

    if( !p_connection ) return false;

    dbus_error_init( &error );
    b_return = dbus_bus_name_has_owner( p_connection, GS_SERVICE, &error );
    if( dbus_error_is_set( &error ) ) dbus_error_free (&error);

    return b_return;
}

#endif

