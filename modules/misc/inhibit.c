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

static int Inhibit( intf_thread_t *p_intf );
static int UnInhibit( intf_thread_t *p_intf );

static int InputChange( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int StateChange( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );

struct intf_sys_t
{
    playlist_t      *p_playlist;
    vlc_object_t    *p_input;
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
    intf_sys_t *p_sys;
    DBusError     error;

    p_sys = p_intf->p_sys = (intf_sys_t *) calloc( 1, sizeof( intf_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_cookie = 0;
    p_sys->p_input = NULL;

    dbus_error_init( &error );
    p_sys->p_conn = dbus_bus_get( DBUS_BUS_SESSION, &error );
    if( !p_sys->p_conn )
    {
        msg_Err( p_this, "Failed to connect to the D-Bus session daemon: %s",
                error.message );
        dbus_error_free( &error );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->p_playlist = pl_Get( p_intf );
    var_AddCallback( p_sys->p_playlist, "item-current", InputChange, p_intf );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    var_DelCallback( p_sys->p_playlist, "item-current", InputChange, p_intf );

    if( p_sys->p_input ) /* Do delete "state" after "item-changed"! */
    {
        var_DelCallback( p_sys->p_input, "state", StateChange, p_intf );
        vlc_object_release( p_sys->p_input );
    }

    if( p_sys->i_cookie )
        UnInhibit( p_intf );
    dbus_connection_unref( p_sys->p_conn );

    free( p_sys );
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


static int StateChange( vlc_object_t *p_input, const char *var,
                        vlc_value_t prev, vlc_value_t value, void *data )
{
    intf_thread_t *p_intf = data;
    const int old = prev.i_int, cur = value.i_int;

    if( ( old == PLAYING_S ) == ( cur == PLAYING_S ) )
        return VLC_SUCCESS; /* No interesting change */

    if( ( p_intf->p_sys->i_cookie != 0 ) == ( cur == PLAYING_S ) )
        return VLC_SUCCESS; /* Already in correct state */

    if( cur == PLAYING_S )
        Inhibit( p_intf );
    else
        UnInhibit( p_intf );

    (void)p_input; (void)var; (void)prev;
    return VLC_SUCCESS;
}

static int InputChange( vlc_object_t *p_playlist, const char *var,
                        vlc_value_t prev, vlc_value_t value, void *data )
{
    intf_thread_t *p_intf = data;
    intf_sys_t *p_sys = p_intf->p_sys;

    if( p_sys->p_input )
    {
        var_DelCallback( p_sys->p_input, "state", StateChange, p_intf );
        vlc_object_release( p_sys->p_input );
    }
    p_sys->p_input = VLC_OBJECT(playlist_CurrentInput( p_sys->p_playlist ));
    if( p_sys->p_input )
        var_AddCallback( p_sys->p_input, "state", StateChange, p_intf );

    (void)var; (void)prev; (void)value; (void)p_playlist;
    return VLC_SUCCESS;
}
