/*****************************************************************************
 * dbus-root.h : dbus control module (mpris v1.0) - root object
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

#ifndef _VLC_DBUS_ROOT_H
#define _VLC_DBUS_ROOT_H

#include "dbus_common.h"

#define VLC_IDENTITY _("VLC media player")

/* DBUS IDENTIFIERS */
#define DBUS_MPRIS_ROOT_INTERFACE    "org.mpris.MediaPlayer2"

/* Handle incoming dbus messages */
DBusHandlerResult handle_root ( DBusConnection *p_conn,
                                DBusMessage *p_from,
                                void *p_this );

static const char* ppsz_supported_uri_schemes[] = {
    "file", "http", "https", "rtsp", "realrtsp", "pnm", "ftp", "mtp", "smb",
    "mms", "mmsu", "mmst", "mmsh", "unsv", "itpc", "icyx", "rtmp", "rtp",
    "dccp", "dvd", "vcd", "vcdx"
};

static const char* ppsz_supported_mime_types[] = {
    "audio/mpeg", "audio/x-mpeg",
    "video/mpeg", "video/x-mpeg",
    "video/mpeg-system", "video/x-mpeg-system",
    "video/mp4",
    "audio/mp4",
    "video/x-msvideo",
    "video/quicktime",
    "application/ogg", "application/x-ogg",
    "video/x-ms-asf",  "video/x-ms-asf-plugin",
    "application/x-mplayer2",
    "video/x-ms-wmv",
    "video/x-google-vlc-plugin",
    "audio/wav", "audio/x-wav",
    "audio/3gpp",
    "video/3gpp",
    "audio/3gpp2",
    "video/3gpp2",
    "video/divx",
    "video/flv", "video/x-flv",
    "video/x-matroska",
    "audio/x-matroska",
    "application/xspf+xml"
};

void UpdateRootCaps( intf_thread_t *p_intf );

#endif //dbus-root.h
