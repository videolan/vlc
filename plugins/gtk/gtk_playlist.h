/*****************************************************************************
 * gtk_playlist.h : Playlist functions for the Gtk plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_playlist.h,v 1.3 2001/05/15 14:49:48 stef Exp $
 *
 * Authors: Pierre Baillet <oct@zoy.org>
 *          Stéphane Borel <stef@via.ecp.fr>
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

void GtkPlaylistDeleteAll     ( GtkMenuItem *, gpointer );
void GtkPlaylistDeleteSelected( GtkMenuItem *, gpointer );
void GtkPlaylistCrop          ( GtkMenuItem *, gpointer );
void GtkPlaylistInvert        ( GtkMenuItem *, gpointer );
void GtkPlaylistSelect        ( GtkMenuItem *, gpointer );
void GtkPlaylistOk            ( GtkButton *, gpointer );
void GtkPlaylistCancel        ( GtkButton *, gpointer );
void GtkPlaylistAddUrl        ( GtkMenuItem *, gpointer );

gint     GtkCompareItems      ( gconstpointer, gconstpointer );
int      GtkHasValidExtension ( gchar * );
GList *  GtkReadFiles         ( gchar * );

gboolean GtkPlaylistShow      ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkPlaylistPrev      ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkPlaylistNext      ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkPlaylistDragMotion( GtkWidget *, GdkDragContext *,
                                gint, gint, uint, gpointer );
gboolean GtkPlaylistEvent     ( GtkWidget *, GdkEvent *, gpointer );
void     GtkPlaylistDragData  ( GtkWidget *, GdkDragContext *,
                                gint, gint, GtkSelectionData *,
                                guint, guint, gpointer  );
void     GtkDeleteGListItem   ( gpointer, gpointer );

void     GtkPlaylistActivate  ( GtkMenuItem *, gpointer );
void     GtkNextActivate      ( GtkMenuItem *, gpointer );
void     GtkPrevActivate      ( GtkMenuItem *, gpointer );

