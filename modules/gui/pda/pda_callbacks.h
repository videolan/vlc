/*****************************************************************************
 * callbacks.h : pda plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: pda_callbacks.h,v 1.3 2003/11/07 13:01:51 jpsaman Exp $
 *
 * Authors: Jean-Paul Saman <jpsaman@wxs.nl>
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

#include <gtk/gtk.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

void ReadDirectory( GtkListStore *p_list, char *psz_dir );
void MediaURLOpenChanged( GtkWidget *widget, gchar *psz_url );
void PlaylistRebuildListStore( GtkListStore *p_list, playlist_t * p_playlist );

gboolean
onPDADeleteEvent                       (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
onFileOpen                             (GtkButton       *button,
                                        gpointer         user_data);

void
onPlaylist                             (GtkButton       *button,
                                        gpointer         user_data);

void
onPreferences                          (GtkButton       *button,
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
onFileListColumns                      (GtkTreeView     *treeview,
                                        gpointer         user_data);

gboolean
onFileListRowSelected                  (GtkTreeView     *treeview,
                                        gboolean         start_editing,
                                        gpointer         user_data);

void
onAddFileToPlaylist                    (GtkButton       *button,
                                        gpointer         user_data);

void
onEntryMRLChanged                      (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryMRLEditingDone                  (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onNetworkPortChanged                   (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryNetworkPortEditingDone          (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onNetworkAddressChanged                (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryNetworkAddressEditingDone       (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onNetworkTypeChanged                   (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryNetworkTypeEditingDone          (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onProtocolTypeChanged                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryProtocolTypeEditingDone         (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onMRLTypeChanged                       (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryMRLTypeEditingDone              (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onStreamTypeChanged                    (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryStreamTypeEditingDone           (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onAddNetworkPlaylist                   (GtkButton       *button,
                                        gpointer         user_data);

void
onV4LAudioChanged                      (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryV4LAudioEditingDone             (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onV4LVideoChanged                      (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryV4LVideoEditingDone             (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onAddCameraToPlaylist                  (GtkButton       *button,
                                        gpointer         user_data);

void
onVideoDeviceChanged                   (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryVideoDeviceEditingDone          (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onVideoCodecChanged                    (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryVideoCodecEditingDone           (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onVideoBitrateChanged                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
onVideoBitrateEditingDone              (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onAudioDeviceChanged                   (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryAudioDeviceEditingDone          (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onAudioCodecChanged                    (GtkEditable     *editable,
                                        gpointer         user_data);

void
onEntryAudioCodecEditingDone           (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onAudioBitrateChanged                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
onAudioBitrateEditingDone              (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
onAddServerToPlaylist                  (GtkButton       *button,
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

