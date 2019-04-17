/*****************************************************************************
 * events.h: Windows video output header file
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Damien Fouilleul <damienf@videolan.org>
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

#include <vlc_vout_window.h>
#include <vlc_vout_display.h>

/**
 * HWNDs manager.
 */
typedef struct event_thread_t event_thread_t;

typedef struct {
    bool is_projected;
    unsigned width;
    unsigned height;
} event_cfg_t;

typedef struct {
    vout_window_t *parent_window;
    HWND hparent;
    HWND hvideownd;
} event_hwnd_t;

event_thread_t *EventThreadCreate( vlc_object_t *, vout_window_t *);
void            EventThreadDestroy( event_thread_t * );
int             EventThreadStart( event_thread_t *, event_hwnd_t *, const event_cfg_t * );
void            EventThreadStop( event_thread_t * );
