/*****************************************************************************
* maemo_callbacks.h : Callbacks header file for the maemo plugin.
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

gboolean delete_event_cb( GtkWidget *widget,
                          GdkEvent *event,
                          gpointer user_data );

void play_cb( GtkButton *button, gpointer user_data );
void stop_cb( GtkButton *button, gpointer user_data );
void prev_cb( GtkButton *button, gpointer user_data );
void next_cb( GtkButton *button, gpointer user_data );
void seekbar_changed_cb( GtkRange *range, GtkScrollType scroll,
                         gdouble value, gpointer data );

void pl_row_activated_cb( GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *,
                          gpointer );

void open_cb( GtkMenuItem *menuitem, gpointer user_data );
void open_address_cb( GtkMenuItem *menuitem, gpointer user_data );
void open_webcam_cb( GtkMenuItem *menuitem, gpointer user_data );

void snapshot_cb( GtkMenuItem *menuitem, gpointer user_data );
void dropframe_cb( GtkMenuItem *menuitem, gpointer user_data );
