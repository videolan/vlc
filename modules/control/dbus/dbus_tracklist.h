/*****************************************************************************
 * dbus_tracklist.h : dbus control module (mpris v1.0) - /TrackList object
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

#ifndef _VLC_DBUS_TRACKLIST_H
#define _VLC_DBUS_TRACKLIST_H

#include <vlc_common.h>
#include <vlc_interface.h>
#include "dbus_common.h"

#define DBUS_MPRIS_TRACKLIST_INTERFACE    "org.mpris.MediaPlayer2.TrackList"
#define DBUS_MPRIS_TRACKLIST_PATH         "/org/mpris/MediaPlayer2/TrackList"

#define DBUS_MPRIS_NOTRACK   "/org/mpris/MediaPlayer2/TrackList/NoTrack"
#define DBUS_MPRIS_APPEND    "/org/mpris/MediaPlayer2/TrackList/Append"

/* Handle incoming dbus messages */
DBusHandlerResult handle_tracklist ( DBusConnection *p_conn,
                                     DBusMessage *p_from,
                                     void *p_this );

int TrackListPropertiesChangedEmit( intf_thread_t *, vlc_dictionary_t * );

#endif //dbus_tracklist.h
