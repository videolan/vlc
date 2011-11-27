/*****************************************************************************
 * vlc_xlib.h: initialization of Xlib
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_XLIB_H
# define VLC_XLIB_H 1

# include <stdio.h>
# include <stdlib.h>
# include <X11/Xlib.h>
# include <X11/Xlibint.h>

static inline bool vlc_xlib_init (vlc_object_t *obj)
{
    if (!var_InheritBool (obj, "xlib"))
        return false;

    bool ok = false;

    /* XInitThreads() can be called multiple times,
     * but it is not reentrant, so we need this global lock. */
    vlc_global_lock (VLC_XLIB_MUTEX);

    if (_Xglobal_lock == NULL && unlikely(_XErrorFunction != NULL))
        /* (_Xglobal_lock == NULL) => Xlib threads not initialized */
        /* (_XErrorFunction != NULL) => Xlib already in use */
        fprintf (stderr, "%s:%u:%s: Xlib not initialized for threads.\n"
                 "This process is probably using LibVLC incorrectly.\n"
                 "Pass \"--no-xlib\" to libvlc_new() to fix this.\n",
                 __FILE__, __LINE__, __func__);
    else if (XInitThreads ())
        ok = true;

    vlc_global_unlock (VLC_XLIB_MUTEX);

    if (!ok)
        msg_Err (obj, "Xlib not initialized for threads");
    return ok;
}

#endif
