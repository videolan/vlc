/*****************************************************************************
 * dbus.h : D-Bus control interface
 *****************************************************************************
 * Copyright (C) 2006 Rafaël Carré
 * $Id$
 *
 * Authors:    Rafaël Carré <funman at videolanorg>
 *             Mirsal ENNAIME <mirsal dot ennaime at gmail dot com>
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

/* MPRIS VERSION */
#define VLC_MPRIS_VERSION_MAJOR     1
#define VLC_MPRIS_VERSION_MINOR     0

/* DBUS IDENTIFIERS */

/* name registered on the session bus */
#define VLC_MPRIS_DBUS_SERVICE      "org.mpris.vlc"
#define MPRIS_DBUS_INTERFACE        "org.freedesktop.MediaPlayer"
#define MPRIS_DBUS_ROOT_PATH        "/"
#define MPRIS_DBUS_PLAYER_PATH      "/Player"
#define MPRIS_DBUS_TRACKLIST_PATH   "/TrackList"

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
    DBusMessage *p_msg = dbus_message_new_signal( MPRIS_DBUS_PLAYER_PATH, \
        MPRIS_DBUS_INTERFACE, signal ); \
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
#define ADD_INT32( i ) DBUS_ADD( DBUS_TYPE_INT32, i )
#define ADD_BYTE( b ) DBUS_ADD( DBUS_TYPE_BYTE, b )

/* XML data to answer org.freedesktop.DBus.Introspectable.Introspect requests */

const char* psz_introspection_xml_data_root =
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

const char* psz_introspection_xml_data_player =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node>"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.freedesktop.MediaPlayer\">\n"
"    <method name=\"GetStatus\">\n"
"      <arg type=\"(iiii)\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"Prev\">\n"
"    </method>\n"
"    <method name=\"Next\">\n"
"    </method>\n"
"    <method name=\"Stop\">\n"
"    </method>\n"
"    <method name=\"Play\">\n"
"    </method>\n"
"    <method name=\"Pause\">\n"
"    </method>\n"
"    <method name=\"Repeat\">\n"
"      <arg type=\"b\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"VolumeSet\">\n"
"      <arg type=\"i\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"VolumeGet\">\n"
"      <arg type=\"i\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"PositionSet\">\n"
"      <arg type=\"i\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"PositionGet\">\n"
"      <arg type=\"i\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"GetMetadata\">\n"
"      <arg type=\"a{sv}\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"GetCaps\">\n"
"      <arg type=\"i\" direction=\"out\" />\n"
"    </method>\n"
"    <signal name=\"TrackChange\">\n"
"      <arg type=\"a{sv}\"/>\n"
"    </signal>\n"
"    <signal name=\"StatusChange\">\n"
"      <arg type=\"(iiii)\"/>\n"
"    </signal>\n"
"    <signal name=\"CapsChange\">\n"
"      <arg type=\"i\"/>\n"
"    </signal>\n"
"  </interface>\n"
"</node>\n"
;

const char* psz_introspection_xml_data_tracklist =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node>"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.freedesktop.MediaPlayer\">\n"
"    <method name=\"AddTrack\">\n"
"      <arg type=\"s\" direction=\"in\" />\n"
"      <arg type=\"b\" direction=\"in\" />\n"
"      <arg type=\"i\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"DelTrack\">\n"
"      <arg type=\"i\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"GetMetadata\">\n"
"      <arg type=\"i\" direction=\"in\" />\n"
"      <arg type=\"a{sv}\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"GetCurrentTrack\">\n"
"      <arg type=\"i\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"GetLength\">\n"
"      <arg type=\"i\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"SetLoop\">\n"
"      <arg type=\"b\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"SetRandom\">\n"
"      <arg type=\"b\" direction=\"in\" />\n"
"    </method>\n"
"    <signal name=\"TrackListChange\">\n"
"      <arg type=\"i\" />\n"
"    </signal>\n"
"  </interface>\n"
"</node>\n"
;


/* Handle  messages reception */
DBUS_METHOD( handle_root );
DBUS_METHOD( handle_player );
DBUS_METHOD( handle_tracklist );

static const DBusObjectPathVTable vlc_dbus_root_vtable = {
        NULL, handle_root, /* handler function */
        NULL, NULL, NULL, NULL
};

static const DBusObjectPathVTable vlc_dbus_player_vtable = {
        NULL, handle_player, /* handler function */
        NULL, NULL, NULL, NULL
};

static const DBusObjectPathVTable vlc_dbus_tracklist_vtable = {
        NULL, handle_tracklist, /* handler function */
        NULL, NULL, NULL, NULL
};

