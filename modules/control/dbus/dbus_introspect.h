/*****************************************************************************
 * dbus_introspect.h : dbus control module (mpris v2.1) - introspection data
 *****************************************************************************
 * Copyright © 2006-2011 Rafaël Carré
 * Copyright © 2007-2011 Mirsal Ennaime
 * Copyright © 2009-2011 The VideoLAN team
 * Copyright © 2013      Alex Merry
 * $Id$
 *
 * Authors:    Mirsal Ennaime <mirsal at mirsal fr>
 *             Rafaël Carré <funman at videolanorg>
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

#ifndef _VLC_DBUS_INTROSPECT_H
#define _VLC_DBUS_INTROSPECT_H

#include "dbus_common.h"

/* XML data to answer org.freedesktop.DBus.Introspectable.Introspect requests */
static const char* psz_introspection_xml =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node>\n"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.freedesktop.DBus.Properties\">\n"
"    <method name=\"Get\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"out\" type=\"v\"/>\n"
"    </method>\n"
"    <method name=\"Set\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"v\"/>\n"
"    </method>\n"
"    <method name=\"GetAll\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"out\" type=\"a{sv}\"/>\n"
"    </method>\n"
"    <signal name=\"PropertiesChanged\">\n"
"      <arg type=\"s\"/>\n"
"      <arg type=\"a{sv}\"/>\n"
"      <arg type=\"as\"/>\n"
"    </signal>\n"
"  </interface>\n"
"  <interface name=\"org.mpris.MediaPlayer2\">\n"
"    <property name=\"Identity\" type=\"s\" access=\"read\" />\n"
"    <property name=\"DesktopEntry\" type=\"s\" access=\"read\" />\n"
"    <property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\" />\n"
"    <property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\" />\n"
"    <property name=\"HasTrackList\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanQuit\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanSetFullscreen\" type=\"b\" access=\"read\" />\n"
"    <property name=\"Fullscreen\" type=\"b\" access=\"readwrite\" />\n"
"    <property name=\"CanRaise\" type=\"b\" access=\"read\" />\n"
"    <method name=\"Quit\" />\n"
"    <method name=\"Raise\" />\n"
"  </interface>\n"
"  <interface name=\"org.mpris.MediaPlayer2.Player\">\n"
"    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\" />\n"
"    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\" />\n"
"    <property name=\"LoopStatus\" type=\"s\" access=\"readwrite\" />\n"
"    <property name=\"Volume\" type=\"d\" access=\"readwrite\" />\n"
"    <property name=\"Shuffle\" type=\"d\" access=\"readwrite\" />\n"
"    <property name=\"Position\" type=\"i\" access=\"read\" />\n"
"    <property name=\"Rate\" type=\"d\" access=\"readwrite\" />\n"
"    <property name=\"MinimumRate\" type=\"d\" access=\"readwrite\" />\n"
"    <property name=\"MaximumRate\" type=\"d\" access=\"readwrite\" />\n"
"    <property name=\"CanControl\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanPlay\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanPause\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanSeek\" type=\"b\" access=\"read\" />\n"
"    <method name=\"Previous\" />\n"
"    <method name=\"Next\" />\n"
"    <method name=\"Stop\" />\n"
"    <method name=\"Play\" />\n"
"    <method name=\"Pause\" />\n"
"    <method name=\"PlayPause\" />\n"
"    <method name=\"Seek\">\n"
"      <arg type=\"x\" direction=\"in\" />\n"
"    </method>"
"    <method name=\"OpenUri\">\n"
"      <arg type=\"s\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"SetPosition\">\n"
"      <arg type=\"o\" direction=\"in\" />\n"
"      <arg type=\"x\" direction=\"in\" />\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.mpris.MediaPlayer2.TrackList\">\n"
"    <property name=\"Tracks\" type=\"ao\" access=\"read\" />\n"
"    <property name=\"CanEditTracks\" type=\"b\" access=\"read\" />\n"
"    <method name=\"GetTracksMetadata\">\n"
"      <arg type=\"ao\" direction=\"in\" />\n"
"      <arg type=\"aa{sv}\" direction=\"out\" />\n"
"    </method>\n"
"    <method name=\"AddTrack\">\n"
"      <arg type=\"s\" direction=\"in\" />\n"
"      <arg type=\"o\" direction=\"in\" />\n"
"      <arg type=\"b\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"RemoveTrack\">\n"
"      <arg type=\"o\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"GoTo\">\n"
"      <arg type=\"o\" direction=\"in\" />\n"
"    </method>\n"
"    <signal name=\"TrackListReplaced\">\n"
"      <arg type=\"ao\" />\n"
"      <arg type=\"o\" />\n"
"    </signal>\n"
"    <signal name=\"TrackAdded\">\n"
"      <arg type=\"a{sv}\" />\n"
"      <arg type=\"o\" />\n"
"    </signal>\n"
"    <signal name=\"TrackRemoved\">\n"
"      <arg type=\"o\" />\n"
"    </signal>\n"
"    <signal name=\"TrackMetadataChanged\">\n"
"      <arg type=\"o\" />\n"
"      <arg type=\"a{sv}\" />\n"
"    </signal>\n"
"  </interface>\n"
"</node>\n"
;

/* Handle incoming dbus messages */
DBUS_METHOD( handle_introspect )
{
    VLC_UNUSED(p_this);
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_introspection_xml );
    REPLY_SEND;
}

#endif //dbus-introspect.h
