/*****************************************************************************
 * dbus.h : D-Bus control interface
 *****************************************************************************
 * Copyright (C) 2006 Rafaël Carré
 * $Id$
 *
 * Author:    Rafaël Carré <funman at videolanorg>
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

/* DBUS IDENTIFIERS */

/* this is also defined in src/libvlc-common.c for one-instance mode */

/* name registered on the session bus */
#define VLC_DBUS_SERVICE        "org.videolan.vlc"
#define VLC_DBUS_INTERFACE      "org.videolan.vlc"
#define VLC_DBUS_OBJECT_PATH    "/org/videolan/vlc"

/* MACROS */

#define DBUS_METHOD( method_function ) \
    static DBusHandlerResult method_function \
            ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this ) 

#define DBUS_SIGNAL( signal_function ) \
    static DBusHandlerResult signal_function \
            ( DBusConnection *p_conn, void *p_data )

#define REPLY_INIT \
    DBusMessage* p_msg = dbus_message_new_method_return( p_from ); \
    if( !p_msg ) return DBUS_HANDLER_RESULT_NEED_MEMORY; \

#define REPLY_SEND \
    if( !dbus_connection_send( p_conn, p_msg, NULL ) ) \
        return DBUS_HANDLER_RESULT_NEED_MEMORY; \
    dbus_connection_flush( p_conn ); \
    dbus_message_unref( p_msg ); \
    return DBUS_HANDLER_RESULT_HANDLED

#define SIGNAL_INIT( signal ) \
    DBusMessage *p_msg = dbus_message_new_signal( VLC_DBUS_OBJECT_PATH, \
        VLC_DBUS_INTERFACE, signal ); \
    if( !p_msg ) return DBUS_HANDLER_RESULT_NEED_MEMORY; \

#define SIGNAL_SEND \
    if( !dbus_connection_send( p_conn, p_msg, NULL ) ) \
    return DBUS_HANDLER_RESULT_NEED_MEMORY; \
    dbus_message_unref( p_msg ); \
    dbus_connection_flush( p_conn ); \
    return DBUS_HANDLER_RESULT_HANDLED

#define OUT_ARGUMENTS \
    DBusMessageIter args; \
    dbus_message_iter_init_append( p_msg, &args )

#define DBUS_ADD( dbus_type, value ) \
    if( !dbus_message_iter_append_basic( &args, dbus_type, value ) ) \
        return DBUS_HANDLER_RESULT_NEED_MEMORY

#define ADD_STRING( s ) DBUS_ADD( DBUS_TYPE_STRING, s )
#define ADD_BOOL( b ) DBUS_ADD( DBUS_TYPE_BOOLEAN, b )
#define ADD_UINT32( i ) DBUS_ADD( DBUS_TYPE_UINT32, i )
#define ADD_UINT16( i ) DBUS_ADD( DBUS_TYPE_UINT16, i )
#define ADD_BYTE( b ) DBUS_ADD( DBUS_TYPE_BYTE, b )

/* XML data to answer org.freedesktop.DBus.Introspectable.Introspect requests */

const char* psz_introspection_xml_data =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node>"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.videolan.vlc\">\n"
"    <method name=\"GetPlayStatus\">\n"
"      <arg type=\"s\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"GetPlayingItem\">\n"
"      <arg type=\"s\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"TogglePause\">\n"
"      <arg type=\"b\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"AddMRL\">\n"
"      <arg type=\"s\" direction=\"in\" />\n"
"      <arg type=\"b\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"Nothing\">\n"
"    </method>\n"
"    <method name=\"Quit\">\n"
"    </method>\n"
"    <method name=\"Prev\">\n"
"    </method>\n"
"    <method name=\"Next\">\n"
"    </method>\n"
"    <method name=\"Stop\">\n"
"    </method>\n"
"    <method name=\"VolumeSet\">\n"
"      <arg type=\"q\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"VolumeGet\">\n"
"      <arg type=\"q\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"PositionSet\">\n"
"      <arg type=\"q\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"PositionGet\">\n"
"      <arg type=\"q\" direction=\"out\" />\n"
"    </method>\n"
"  </interface>\n"
"</node>\n"
;

/* Handling of messages received onn VLC_DBUS_OBJECT_PATH */
DBUS_METHOD( handle_messages ); /* handler function */

/* vtable passed to dbus_connection_register_object_path() */
static DBusObjectPathVTable vlc_dbus_vtable = {
        NULL, /* Called when vtable is unregistered or its connection is freed*/
        handle_messages, /* handler function */
        NULL,
        NULL,
        NULL,
        NULL
};

