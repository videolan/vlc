/*****************************************************************************
 * vlc_window.h: Embedded video output window
 *****************************************************************************
 * Copyright (C) 2008 Rémi Denis-Courmont
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

#ifndef LIBVLCCORE_WINDOW_H
# define LIBVLCCORE_WINDOW_H 1

/**
 * \file
 * This file defines functions and structures for output windows
 */

# include <stdarg.h>

typedef struct vout_window_t vout_window_t;
typedef struct vout_window_sys_t vout_window_sys_t;

struct vout_window_t
{
    VLC_COMMON_MEMBERS

    module_t      *module;
    vout_thread_t *vout;
    union
    {
        void      *hwnd; /* Win32 window handle */
        uint32_t   xid;  /* X11 window ID */
    } handle;
    vout_window_sys_t *p_sys;  /* window provider private data */

    unsigned       width;  /* pixels width */
    unsigned       height; /* pixels height */
    int            pos_x;  /* horizontal position hint */
    int            pos_y;  /* vertical position hint */

    int (*control) (struct vout_window_t *, int, va_list);
};

VLC_EXPORT( vout_window_t *, vout_RequestWindow, ( vout_thread_t *, const char *, int *, int *, unsigned int *, unsigned int * ) );
VLC_EXPORT( void,   vout_ReleaseWindow, ( vout_window_t * ) );
VLC_EXPORT( int, vout_ControlWindow, ( vout_window_t *, int, va_list ) );

static inline vout_window_t *
vout_RequestXWindow (vout_thread_t *vout,
                     int *x, int *y, unsigned *w, unsigned *h)
{
    return vout_RequestWindow (vout, "xwindow", x, y, w, h);
}

static inline vout_window_t *
vout_RequestHWND (vout_thread_t *vout,
                  int *x, int *y, unsigned *w, unsigned *h)
{
    return vout_RequestWindow (vout, "hwnd", x, y, w, h);
}

#endif /* !LIBVLCCORE_WINDOW_H */
