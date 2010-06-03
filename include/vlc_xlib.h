/*****************************************************************************
 * vlc_xlib.h: initialization of Xlib
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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

#ifndef VLC_XLIB_H
# define VLC_XLIB_H 1

# include <X11/Xlib.h>

static inline bool vlc_xlib_init (vlc_object_t *obj)
{
    bool ok = false;

    if (var_InheritBool (obj, "xlib"))
    {
        /* XInitThreads() can be called multiple times,
         * but it is not reentrant. */
        vlc_global_lock (VLC_XLIB_MUTEX);
        ok = XInitThreads () != 0;
        vlc_global_unlock (VLC_XLIB_MUTEX);
    }
    return ok;
}

#endif
