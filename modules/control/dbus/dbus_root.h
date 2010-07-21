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

/* MPRIS VERSION */
#define DBUS_MPRIS_VERSION_MAJOR     2
#define DBUS_MPRIS_VERSION_MINOR     0

/* DBUS IDENTIFIERS */
#define DBUS_MPRIS_ROOT_INTERFACE    "org.freedesktop.MediaPlayer"
#define DBUS_MPRIS_ROOT_PATH         "/"

/* Handle incoming dbus messages */
DBusHandlerResult handle_root ( DBusConnection *p_conn,
                                DBusMessage *p_from,
                                void *p_this );

static const DBusObjectPathVTable dbus_mpris_root_vtable = {
        NULL, handle_root, /* handler function */
        NULL, NULL, NULL, NULL
};

#endif //dbus-root.h
