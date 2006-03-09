/*****************************************************************************
 * callbacks.h : pda plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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

#include <vlc/vlc.h>
#include <vlc/intf.h>

void ReadDirectory( intf_thread_t *p_intf, GtkListStore *p_list, char *psz_dir );
void PlaylistRebuildListStore( GtkListStore *p_list, playlist_t * p_playlist );


gboolean
onPDADeleteEvent                       (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
onRewind                               (GtkButton       *button,
                                        gpointer         user_data);

void
onPause                                (GtkButton       *button,
                                        gpointer         user_data);

void
onPlay                                 (GtkButton       *button,
                                        gpointer         user_data);

void
onStop                                 (GtkButton       *button,
                                        gpointer         user_data);

void
onForward                              (GtkButton       *button,
                                        gpointer         user_data);

void
onAbout                                (GtkButton       *button,
                                        gpointer         user_data);

gboolean
SliderRelease                          (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
SliderPress                            (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
onFileListRow                          (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data);

void
onAddFileToPlaylist                    (GtkButton       *button,
                                        gpointer         user_data);

void
onAddNetworkPlaylist                   (GtkButton       *button,
                                        gpointer         user_data);

void
onAddCameraToPlaylist                  (GtkButton       *button,
                                        gpointer         user_data);

gboolean
PlaylistEvent                          (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
onPlaylistColumnsChanged               (GtkTreeView     *treeview,
                                        gpointer         user_data);

gboolean
onPlaylistRowSelected                  (GtkTreeView     *treeview,
                                        gboolean         start_editing,
                                        gpointer         user_data);

void
onPlaylistRow                          (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data);

void
onUpdatePlaylist                       (GtkButton       *button,
                                        gpointer         user_data);

void
onDeletePlaylist                       (GtkButton       *button,
                                        gpointer         user_data);

void
onClearPlaylist                        (GtkButton       *button,
                                        gpointer         user_data);

void
onPreferenceSave                       (GtkButton       *button,
                                        gpointer         user_data);

void
onPreferenceApply                      (GtkButton       *button,
                                        gpointer         user_data);

void
onPreferenceCancel                     (GtkButton       *button,
                                        gpointer         user_data);


void
NetworkBuildMRL                        (GtkEditable     *editable,
                                        gpointer         user_data);


void
onAddTranscodeToPlaylist               (GtkButton       *button,
                                        gpointer         user_data);


void
onEntryStdAccessChanged                (GtkEditable     *editable,
                                        gpointer         user_data);

void
SliderMove                             (GtkRange        *range,
                                        GtkScrollType    scroll,
                                        gpointer         user_data);

