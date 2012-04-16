/*****************************************************************************
 * dbus.h : D-Bus control interface
 *****************************************************************************
 * Copyright © 2006-2008 Rafaël Carré
 * Copyright © 2007-2012 Mirsal Ennaime
 * Copyright © 2009-2012 The VideoLAN team
 * $Id$
 *
 * Authors:    Rafaël Carré <funman at videolanorg>
 *             Mirsal Ennaime <mirsal at mirsal fr>
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

#ifndef _VLC_DBUS_H
#define _VLC_DBUS_H

#define DBUS_MPRIS_BUS_NAME "org.mpris.MediaPlayer2.vlc"
#define DBUS_INSTANCE_ID_PREFIX "instance"

static DBusHandlerResult
MPRISEntryPoint ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this );

static const DBusObjectPathVTable dbus_mpris_vtable = {
        NULL, MPRISEntryPoint, /* handler function */
        NULL, NULL, NULL, NULL
};

#define ABS(x) ( ( x ) > 0 ? ( x ) : ( -1 * ( x ) ) )
#define SEEK_THRESHOLD 1000 /* µsec */

#endif //dbus.h
