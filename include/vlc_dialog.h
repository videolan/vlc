/*****************************************************************************
 * vlc_dialog.h: user interaction dialogs
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

#ifndef VLC_DIALOG_H_
#define VLC_DIALOG_H_

/**
 * \file vlc_dialog.h
 * User interaction dialog APIs
 */

/**
 * A fatal error dialog.
 * No response expected from the user.
 */
typedef struct dialog_fatal_t
{
    const char *title;
    const char *message;
} dialog_fatal_t;

VLC_EXPORT( void, dialog_Fatal, (vlc_object_t *, const char *, const char *, ...) ) LIBVLC_FORMAT(3, 4);
#define dialog_Fatal(o, t, ...) \
        dialog_Fatal(VLC_OBJECT(o), t, __VA_ARGS__)

VLC_EXPORT( int, dialog_Register, (vlc_object_t *) );
VLC_EXPORT( int, dialog_Unregister, (vlc_object_t *) );
#define dialog_Register(o) dialog_Register(VLC_OBJECT(o))
#define dialog_Unregister(o) dialog_Unregister(VLC_OBJECT(o))

#endif
