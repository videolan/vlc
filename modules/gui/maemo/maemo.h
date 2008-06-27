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

#include <hildon/hildon-program.h>
#include <hildon/hildon-seekbar.h>
#include <hildon/hildon-file-chooser-dialog.h>
#include <hildon/hildon-banner.h>

#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_vout.h>

struct intf_sys_t
{
    playlist_t *p_playlist;
    input_thread_t *p_input;

    HildonWindow  *p_main_window;
    HildonSeekbar *p_seekbar;
    GtkWidget     *p_tabs;
    GtkWidget     *p_play_button;

    GtkWidget *p_playlist_store;

    int i_event;
    vlc_spinlock_t event_lock;

    GtkWidget *p_video_window;
    vout_thread_t *p_vout;
    vlc_cond_t p_video_cond;
    vlc_mutex_t p_video_mutex;
};
