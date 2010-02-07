/*****************************************************************************
 * maemo.h: private Maemo Interface Description
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <hildon/hildon-program.h>
#include <hildon/hildon-seekbar.h>
#include <hildon/hildon-banner.h>

#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_vout.h>

struct intf_sys_t
{
    vlc_thread_t thread;

    playlist_t *p_playlist;
    input_thread_t *p_input;
    vlc_sem_t ready;

    HildonWindow  *p_main_window;
    HildonSeekbar *p_seekbar;
    GtkWidget     *p_play_button;

    GtkListStore  *p_playlist_store;
    GtkWidget     *p_playlist_window;

    int i_event;
    vlc_spinlock_t event_lock;

    GtkWidget *p_video_window;
    uint32_t xid; /* X11 windows ID */
    bool b_fullscreen;

    GtkWidget *p_control_window;
};
