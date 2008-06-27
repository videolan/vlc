/*****************************************************************************
* maemo_input.h : Input handling header file for the maemo plugin.
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

#include <gtk/gtk.h>

#include <vlc_common.h>
#include <vlc_interface.h>

#define EVENT_ITEM_STATE_CHANGE (1<<0)
#define EVENT_PLAYLIST_CURRENT  (1<<1)
#define EVENT_ACTIVITY          (1<<2)
#define EVENT_ITEM_CHANGED      (1<<3)
#define EVENT_INTF_CHANGED      (1<<4)

gboolean process_events( gpointer data );

void set_input( intf_thread_t *p_intf, input_thread_t *p_input );
void delete_input( intf_thread_t *p_intf );

int playlist_current_cb( vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *param );
int activity_cb( vlc_object_t *p_this, const char *psz_var,
                 vlc_value_t oldval, vlc_value_t newval, void *param );
void item_changed_pl( intf_thread_t *p_intf );

int item_changed_cb( vlc_object_t *p_this, const char *psz_var,
                     vlc_value_t oldval, vlc_value_t newval, void *param );
void item_changed( intf_thread_t *p_intf );

int interface_changed_cb( vlc_object_t *p_this, const char *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *param );
void update_position( intf_thread_t *p_intf );
