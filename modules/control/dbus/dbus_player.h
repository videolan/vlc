/*****************************************************************************
 * dbus-player.h : dbus control module (mpris v1.0) - /Player object
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

#ifndef _VLC_DBUS_PLAYER_H
#define _VLC_DBUS_PLAYER_H

#include <vlc_interface.h>
#include "dbus_common.h"

#define DBUS_MPRIS_PLAYER_INTERFACE    "org.freedesktop.MediaPlayer"
#define DBUS_MPRIS_PLAYER_PATH         "/Player"

/* Handle incoming dbus messages */
DBusHandlerResult handle_player ( DBusConnection *p_conn,
                                  DBusMessage *p_from,
                                  void *p_this );

static const DBusObjectPathVTable dbus_mpris_player_vtable = {
        NULL, handle_player, /* handler function */
        NULL, NULL, NULL, NULL
};

/* GetCaps() capabilities */
enum
{
     CAPS_NONE                  = 0,
     CAPS_CAN_GO_NEXT           = 1 << 0,
     CAPS_CAN_GO_PREV           = 1 << 1,
     CAPS_CAN_PAUSE             = 1 << 2,
     CAPS_CAN_PLAY              = 1 << 3,
     CAPS_CAN_SEEK              = 1 << 4,
     CAPS_CAN_PROVIDE_METADATA  = 1 << 5,
     CAPS_CAN_HAS_TRACKLIST     = 1 << 6
};

int StatusChangeEmit ( intf_thread_t * );
int CapsChangeEmit   ( intf_thread_t * );
int TrackChangeEmit  ( intf_thread_t *, input_item_t * );

#endif //dbus_player.h
