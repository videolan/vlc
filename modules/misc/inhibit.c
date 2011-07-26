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

enum {
    FREEDESKTOP = 0, /* as used by KDE and gnome <= 2.26 */
    GNOME       = 1, /* as used by gnome > 2.26 */
};

static const char *dbus_service[] = {
    [FREEDESKTOP]   = "org.freedesktop.PowerManagement",
    [GNOME]         = "org.gnome.SessionManager",
};

static const char *dbus_path[] = {
    [FREEDESKTOP]   = "/org/freedesktop/PowerManagement",
    [GNOME]         = "/org/gnome/SessionManager",
};

static const char *dbus_interface[] = {
    [FREEDESKTOP]   = "org.freedesktop.PowerManagement.Inhibit",
    [GNOME]         = "org.gnome.SessionManager",
};


/*****************************************************************************
 * Local prototypes
 !*****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Deactivate   ( vlc_object_t * );

static void UnInhibit( intf_thread_t *p_intf, int type );

static int InputChange( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int StateChange( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );

struct intf_sys_t
{
    playlist_t      *p_playlist;
    vlc_object_t    *p_input;
    DBusConnection  *p_conn;
    dbus_uint32_t   i_cookie[2];
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

    p_sys->i_cookie[FREEDESKTOP] = 0;
    p_sys->i_cookie[GNOME] = 0;
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

    if( p_sys->i_cookie[FREEDESKTOP] )
        UnInhibit( p_intf, FREEDESKTOP );
    if( p_sys->i_cookie[GNOME] )
        UnInhibit( p_intf, GNOME );
    dbus_connection_unref( p_sys->p_conn );

    free( p_sys );
}

/*****************************************************************************
 * Inhibit: Notify the power management daemon that it shouldn't suspend
 * the computer because of inactivity
 *****************************************************************************/
static void Inhibit( intf_thread_t *p_intf, int type )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    DBusMessage *msg = dbus_message_new_method_call(
        dbus_service[type], dbus_path[type], dbus_interface[type], "Inhibit" );
    if( unlikely(msg == NULL) )
        return;

    const char *app = PACKAGE;
    const char *reason = _("Playing some media.");

    p_sys->i_cookie[type] = 0;

    dbus_bool_t ret;
    dbus_uint32_t xid = 0; // FIXME?
    dbus_uint32_t flags = 8 /* Inhibit suspending the session or computer */
                        | 4;/* Inhibit the session being marked as idle */
    switch( type ) {
    case FREEDESKTOP:
        ret = dbus_message_append_args( msg, DBUS_TYPE_STRING, &app,
                                        DBUS_TYPE_STRING, &reason,
                                        DBUS_TYPE_INVALID );
        break;
    case GNOME:
    default:
        ret = dbus_message_append_args( msg, DBUS_TYPE_STRING, &app,
                                        DBUS_TYPE_UINT32, &xid,
                                        DBUS_TYPE_STRING, &reason,
                                        DBUS_TYPE_UINT32, &flags,
                                        DBUS_TYPE_INVALID );
        break;
    }

    if( !ret )
    {
        dbus_message_unref( msg );
        return;
    }

    /* blocks 50ms maximum */
    DBusMessage *reply;

    reply = dbus_connection_send_with_reply_and_block( p_sys->p_conn, msg,
                                                       50, NULL );
    dbus_message_unref( msg );
    if( reply == NULL )
        /* g-p-m is not active, or too slow. Better luck next time? */
        return;

    /* extract the cookie from the reply */
    dbus_uint32_t i_cookie;

    if( dbus_message_get_args( reply, NULL,
                               DBUS_TYPE_UINT32, &i_cookie,
                               DBUS_TYPE_INVALID ) )
        p_sys->i_cookie[type] = i_cookie;

    dbus_message_unref( reply );
}

/*****************************************************************************
 * UnInhibit: Notify the power management daemon that we aren't active anymore
 *****************************************************************************/
static void UnInhibit( intf_thread_t *p_intf, int type )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    DBusMessage *msg = dbus_message_new_method_call( dbus_service[type],
            dbus_path[type], dbus_interface[type], "UnInhibit" );
    if( unlikely(msg == NULL) )
        return;

    dbus_uint32_t i_cookie = p_sys->i_cookie[type];
    if( dbus_message_append_args( msg, DBUS_TYPE_UINT32, &i_cookie,
                                       DBUS_TYPE_INVALID )
     && dbus_connection_send( p_sys->p_conn, msg, NULL ) )
    {
        dbus_connection_flush( p_sys->p_conn );
        p_sys->i_cookie[type] = 0;
    }
    dbus_message_unref( msg );
}


static int StateChange( vlc_object_t *p_input, const char *var,
                        vlc_value_t prev, vlc_value_t value, void *data )
{
    intf_thread_t *p_intf = data;
    intf_sys_t *p_sys = p_intf->p_sys;
    const int old = prev.i_int, cur = value.i_int;

    if( ( old == PLAYING_S ) == ( cur == PLAYING_S ) )
        return VLC_SUCCESS; /* No interesting change */

    if( cur == PLAYING_S ) {
        if (p_sys->i_cookie[FREEDESKTOP] == 0)
            Inhibit( p_intf, FREEDESKTOP );
        if (p_sys->i_cookie[GNOME] == 0)
            Inhibit( p_intf, GNOME );
    }
    else {
        if (p_sys->i_cookie[FREEDESKTOP] != 0)
            UnInhibit( p_intf, FREEDESKTOP );
        if (p_sys->i_cookie[GNOME] != 0)
            UnInhibit( p_intf, GNOME );
    }

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
    {
        Inhibit( p_intf, FREEDESKTOP );
        Inhibit( p_intf, GNOME );

        var_AddCallback( p_sys->p_input, "state", StateChange, p_intf );
    }

    (void)var; (void)prev; (void)value; (void)p_playlist;
    return VLC_SUCCESS;
}
