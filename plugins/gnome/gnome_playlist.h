/*****************************************************************************
 * gnome_playlist.h : Playlist functions for the Gnome plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gnome_playlist.h,v 1.1 2001/05/06 18:41:52 stef Exp $
 *
 * Authors: Pierre Baillet <oct@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

void    GnomeDropDataReceived ( intf_thread_t *, GtkSelectionData *,
                                guint, int );
void    GnomeRebuildCList     ( GtkCList *, playlist_t * );
int     GnomeHasValidExtension( gchar * );
int     GnomeAppendList       ( playlist_t *, int, GList * );
void    GnomePlayListManage   ( intf_thread_t * );
gint    GnomeCompareItems     ( gconstpointer, gconstpointer );
GList * GnomeReadFiles        ( gchar * );
void    GnomeDeleteGListItem  ( gpointer, gpointer );



