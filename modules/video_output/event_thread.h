/*****************************************************************************
 * event_thread.h: event thread for legacy video output display plugins
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

typedef struct vout_display_event_thread vout_display_event_thread_t;

void VoutDisplayEventKey(vout_display_event_thread_t *vdet, int key);
struct vout_display_event_thread *
VoutDisplayEventCreateThread(vout_display_t *) VLC_USED;
void VoutDisplayEventKillThread(vout_display_event_thread_t *);

