/*****************************************************************************
 * event.h: Windows video output header file
 *****************************************************************************
 * Copyright (C) 2001-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Damien Fouilleul <damienf@videolan.org>
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

/**
 * HWNDs manager.
 */
typedef struct event_thread_t event_thread_t;

event_thread_t *EventThreadCreate( vout_thread_t * );
void            EventThreadDestroy( event_thread_t * );
int             EventThreadStart( event_thread_t * );
void            EventThreadStop( event_thread_t * );

void            EventThreadMouseAutoHide( event_thread_t * );
void            EventThreadUpdateTitle( event_thread_t *, const char *psz_fallback );
unsigned        EventThreadRetreiveChanges( event_thread_t * );
int             EventThreadGetWindowStyle( event_thread_t * );
