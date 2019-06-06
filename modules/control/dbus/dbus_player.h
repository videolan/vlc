/*****************************************************************************
 * dbus_player.h : dbus control module (mpris v2.2) - Player object
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2010 Mirsal Ennaime
 * Copyright © 2009-2010 The VideoLAN team
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

#ifndef VLC_DBUS_DBUS_PLAYER_H_
#define VLC_DBUS_DBUS_PLAYER_H_

#include <vlc_interface.h>
#include "dbus_common.h"

#define DBUS_MPRIS_PLAYER_INTERFACE    "org.mpris.MediaPlayer2.Player"

#define LOOP_STATUS_NONE  "None"
#define LOOP_STATUS_TRACK "Track"
#define LOOP_STATUS_PLAYLIST "Playlist"

#define PLAYBACK_STATUS_STOPPED "Stopped"
#define PLAYBACK_STATUS_PLAYING "Playing"
#define PLAYBACK_STATUS_PAUSED  "Paused"

/* Handle incoming dbus messages */
DBusHandlerResult handle_player ( DBusConnection *p_conn,
                                  DBusMessage *p_from,
                                  void *p_this );

/* Player capabilities */
enum
{
     PLAYER_CAPS_NONE            = 0,
     PLAYER_CAN_GO_NEXT          = 1 << 0,
     PLAYER_CAN_GO_PREVIOUS      = 1 << 1,
     PLAYER_CAN_PAUSE            = 1 << 2,
     PLAYER_CAN_PLAY             = 1 << 3,
     PLAYER_CAN_SEEK             = 1 << 4,
     PLAYER_CAN_PROVIDE_METADATA = 1 << 5,
     PLAYER_CAN_PROVIDE_POSITION = 1 << 6,
     PLAYER_CAN_REPEAT           = 1 << 7,
     PLAYER_CAN_LOOP             = 1 << 8,
     PLAYER_CAN_SHUFFLE          = 1 << 9,
     PLAYER_CAN_CONTROL_RATE     = 1 << 10,
     PLAYER_CAN_PLAY_BACKWARDS   = 1 << 11
};

int PlayerStatusChangedEmit     ( intf_thread_t * );
int PlayerCapsChangedEmit ( intf_thread_t * );
int PlayerMetadataChangedEmit( intf_thread_t*, input_item_t* );
int TrackChangedEmit      ( intf_thread_t *, input_item_t * );
int SeekedEmit( intf_thread_t * );

int PlayerPropertiesChangedEmit( intf_thread_t *, vlc_dictionary_t * );

void UpdatePlayerCaps( intf_thread_t * );

#endif /* include-guard */
