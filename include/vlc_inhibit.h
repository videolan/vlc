/*****************************************************************************
 * vlc_inhibit.h: VLC screen saver inhibition
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

/**
 * \file
 * This file defines the interface for screen-saver inhibition modules
 */

#ifndef VLC_INHIBIT_H
# define VLC_INHIBIT_H 1

typedef struct vlc_inhibit vlc_inhibit_t;
typedef struct vlc_inhibit_sys vlc_inhibit_sys_t;

struct vout_window_t;

enum vlc_inhibit_flags
{
    VLC_INHIBIT_NONE=0 /*< No inhibition */,
    VLC_INHIBIT_SUSPEND=0x1 /*< Processor is in use - do not suspend */,
    VLC_INHIBIT_DISPLAY=0x2 /*< Display is in use - do not blank/lock */,
#define VLC_INHIBIT_AUDIO (VLC_INHIBIT_SUSPEND)
#define VLC_INHIBIT_VIDEO (VLC_INHIBIT_SUSPEND|VLC_INHIBIT_DISPLAY)
};

struct vlc_inhibit
{
    struct vlc_object_t obj;

    vlc_inhibit_sys_t *p_sys;
    void (*inhibit) (vlc_inhibit_t *, unsigned flags);
};

static inline struct vout_window_t *vlc_inhibit_GetWindow(vlc_inhibit_t *ih)
{
    return (struct vout_window_t *)vlc_object_parent(ih);
}

static inline void vlc_inhibit_Set (vlc_inhibit_t *ih, unsigned flags)
{
    ih->inhibit (ih, flags);
}

#endif
