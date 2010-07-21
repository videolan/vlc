/*****************************************************************************
 * dbus-root.c : dbus control module (mpris v1.0) - root object
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2010 Mirsal Ennaime
 * Copyright © 2009-2010 The VideoLAN team
 * $Id$
 *
 * Authors:    Mirsal Ennaime <mirsal at mirsal fr>
 *             Rafaël Carré <funman at videolanorg>
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

#include <vlc_common.h>
#include <vlc_interface.h>

#include "dbus_root.h"
#include "dbus_common.h"

/* XML data to answer org.freedesktop.DBus.Introspectable.Introspect requests */
static const char* psz_root_introspection_xml =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node>\n"
"  <node name=\"Player\"/>\n"
"  <node name=\"TrackList\"/>\n"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.freedesktop.MediaPlayer\">\n"
"    <method name=\"Identity\">\n"
"      <arg type=\"s\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"MprisVersion\">\n"
"      <arg type=\"(qq)\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"Quit\">\n"
"    </method>\n"
"  </interface>\n"
"</node>\n"
;

DBUS_METHOD( Identity )
{
    VLC_UNUSED(p_this);
    REPLY_INIT;
    OUT_ARGUMENTS;
    char *psz_identity;

    if( asprintf( &psz_identity, "%s %s", PACKAGE, VERSION ) != -1 )
    {
        ADD_STRING( &psz_identity );
        free( psz_identity );
    }
    else
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

DBUS_METHOD( MprisVersion )
{ /*implemented version of the mpris spec */
    REPLY_INIT;
    OUT_ARGUMENTS;
    VLC_UNUSED( p_this );
    dbus_uint16_t i_major = DBUS_MPRIS_VERSION_MAJOR;
    dbus_uint16_t i_minor = DBUS_MPRIS_VERSION_MINOR;
    DBusMessageIter version;

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_STRUCT, NULL,
            &version ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_append_basic( &version, DBUS_TYPE_UINT16,
            &i_major ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_append_basic( &version, DBUS_TYPE_UINT16,
            &i_minor ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( !dbus_message_iter_close_container( &args, &version ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    REPLY_SEND;
}

DBUS_METHOD( Quit )
{ /* exits vlc */
    REPLY_INIT;
    libvlc_Quit(INTF->p_libvlc);
    REPLY_SEND;
}

DBUS_METHOD( handle_introspect_root )
{ /* handles introspection of root object */
    VLC_UNUSED(p_this);
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_root_introspection_xml );
    REPLY_SEND;
}

#define METHOD_FUNC( interface, method, function ) \
    else if( dbus_message_is_method_call( p_from, interface, method ) )\
        return function( p_conn, p_from, p_this )

DBusHandlerResult
handle_root ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this )
{
    if( dbus_message_is_method_call( p_from,
                DBUS_INTERFACE_INTROSPECTABLE, "Introspect" ) )
        return handle_introspect_root( p_conn, p_from, p_this );

    /* here D-Bus method names are associated to an handler */

    METHOD_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "Identity",      Identity );
    METHOD_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "MprisVersion",  MprisVersion );
    METHOD_FUNC( DBUS_MPRIS_ROOT_INTERFACE, "Quit",          Quit );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

#undef METHOD_FUNC
