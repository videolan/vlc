/*****************************************************************************
 * inhibit.c : prevents the computer from suspending when VLC is playing
 *****************************************************************************
 * Copyright © 2007 Rafaël Carré
 * $Id$
 *
 * Author: Rafaël Carré <funman@videolanorg>
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
 * Based on freedesktop Power Management Specification version 0.2
 * http://people.freedesktop.org/~hughsient/temp/power-management-spec-0.2.html
 */

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
#include <vlc_playlist.h>

#include <dbus/dbus.h>

#define PM_SERVICE   "org.freedesktop.PowerManagement"
#define PM_PATH      "/org/freedesktop/PowerManagement/Inhibit"
#define PM_INTERFACE "org.freedesktop.PowerManagement.Inhibit"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Deactivate   ( vlc_object_t * );

static void Run          ( intf_thread_t *p_intf );

static int Inhibit( intf_thread_t *p_intf );
static int UnInhibit( intf_thread_t *p_intf );

struct intf_sys_t
{
    DBusConnection  *p_conn;
    dbus_uint32_t   i_cookie;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Power Management Inhibitor") )
    set_capability( "interface", 0 )
    set_callbacks( Activate, Deactivate )
vlc_module_end ()

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    DBusError     error;

    p_intf->pf_run = Run;
    p_intf->p_sys = (intf_sys_t *) calloc( 1, sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
        return VLC_ENOMEM;

    p_intf->p_sys->i_cookie = 0;

    dbus_error_init( &error );
    p_intf->p_sys->p_conn = dbus_bus_get( DBUS_BUS_SESSION, &error );
    if( !p_intf->p_sys->p_conn )
    {
        msg_Err( p_this, "Failed to connect to the D-Bus session daemon: %s",
                error.message );
        dbus_error_free( &error );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    if( p_intf->p_sys->i_cookie )
        UnInhibit( p_intf );

    dbus_connection_unref( p_intf->p_sys->p_conn );
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Inhibit: Notify the power management daemon that it shouldn't suspend
 * the computer because of inactivity
 *
 * returns false if Out of memory, else true
 *****************************************************************************/
static int Inhibit( intf_thread_t *p_intf )
{
    DBusConnection *p_conn;
    DBusMessage *p_msg;
    DBusMessageIter args;
    DBusMessage *p_reply;
    dbus_uint32_t i_cookie;

    p_conn = p_intf->p_sys->p_conn;

    p_msg = dbus_message_new_method_call( PM_SERVICE, PM_PATH, PM_INTERFACE,
                                          "Inhibit" );
    if( !p_msg )
        return false;

    dbus_message_iter_init_append( p_msg, &args );

    char *psz_app = strdup( PACKAGE );
    if( !psz_app ||
        !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &psz_app ) )
    {
        free( psz_app );
        dbus_message_unref( p_msg );
        return false;
    }
    free( psz_app );

    char *psz_inhibit_reason = strdup( _("Playing some media.") );
    if( !psz_inhibit_reason )
    {
        dbus_message_unref( p_msg );
        return false;
    }
    if( !dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING,
                                         &psz_inhibit_reason ) )
    {
        free( psz_inhibit_reason );
        dbus_message_unref( p_msg );
        return false;
    }
    free( psz_inhibit_reason );

    p_reply = dbus_connection_send_with_reply_and_block( p_conn, p_msg,
        50, NULL ); /* blocks 50ms maximum */
    dbus_message_unref( p_msg );
    if( p_reply == NULL )
    {   /* g-p-m is not active, or too slow. Better luck next time? */
        return true;
    }

    /* extract the cookie from the reply */
    if( dbus_message_get_args( p_reply, NULL,
            DBUS_TYPE_UINT32, &i_cookie,
            DBUS_TYPE_INVALID ) == FALSE )
    {
        return false;
    }

    /* Save the cookie */
    p_intf->p_sys->i_cookie = i_cookie;
    return true;
}

/*****************************************************************************
 * UnInhibit: Notify the power management daemon that we aren't active anymore
 *
 * returns false if Out of memory, else true
 *****************************************************************************/
static int UnInhibit( intf_thread_t *p_intf )
{
    DBusConnection *p_conn;
    DBusMessage *p_msg;
    DBusMessageIter args;
    dbus_uint32_t i_cookie;

    p_conn = p_intf->p_sys->p_conn;

    p_msg = dbus_message_new_method_call( PM_SERVICE, PM_PATH, PM_INTERFACE,
                                          "UnInhibit" );
    if( !p_msg )
        return false;

    dbus_message_iter_init_append( p_msg, &args );

    i_cookie = p_intf->p_sys->i_cookie;
    if( !dbus_message_iter_append_basic( &args, DBUS_TYPE_UINT32, &i_cookie ) )
    {
        dbus_message_unref( p_msg );
        return false;
    }

    if( !dbus_connection_send( p_conn, p_msg, NULL ) )
        return false;
    dbus_connection_flush( p_conn );

    dbus_message_unref( p_msg );

    p_intf->p_sys->i_cookie = 0;
    return true;
}

/*****************************************************************************
 * Run: main thread
 *****************************************************************************/
static void vlc_cleanup_playlist( void *p_playlist )
{
    pl_Release( (playlist_t*)p_playlist );
}
static void Run( intf_thread_t *p_intf )
{
    int canc = vlc_savecancel();

    playlist_t *p_playlist = pl_Hold( p_intf );

    vlc_cleanup_push( vlc_cleanup_playlist, p_intf );

    for( ;; )
    {
        vlc_restorecancel( canc );

        /* FIXME wake up on playlist event instead ?
         * Check playing state every 30 seconds */
        msleep( 30 * CLOCK_FREQ );

        canc = vlc_savecancel();

        /* */
        input_thread_t *p_input = playlist_CurrentInput( p_playlist );
        if( p_input )
        {
            const int i_state = var_GetInteger( p_input, "state" );
            vlc_object_release( p_input );

            if( PLAYING_S == i_state )
            {
               if( !p_intf->p_sys->i_cookie )
               {
                   if( !Inhibit( p_intf ) )
                       break;
               }
            }
            else if( p_intf->p_sys->i_cookie )
            {
                if( !UnInhibit( p_intf ) )
                    break;
            }
        }
        else if( p_intf->p_sys->i_cookie )
        {
            if( !UnInhibit( p_intf ) )
                break;
        }
    }

    /* */
    vlc_cleanup_run();
    vlc_restorecancel( canc );
}
