/*****************************************************************************
 * callbacks.h : pda plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: callbacks.h,v 1.1 2003/07/23 22:02:56 jpsaman Exp $
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

#include "network.h"
#include "playlist.h"

gboolean PDAExit           ( GtkWidget *, gpointer );

void ReadDirectory(GtkCList *clist, char *psz_dir);
void MediaURLOpenChanged( GtkWidget *widget, gchar *psz_url );

void
on_toolbar_open_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_preferences_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_rewind_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_pause_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_play_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_stop_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_forward_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_about_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_comboURL_entry_changed              (GtkEditable     *editable,
                                        gpointer         user_data);


void
on_clistmedia_click_column             (GtkCList        *clist,
                                        gint             column,
                                        gpointer         user_data);

void
on_clistmedia_select_row               (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_cbautoplay_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);


gboolean
on_pda_delete_event               (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
PDASliderRelease                  (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
PDASliderPress                    (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);




void
PDAMrlGo                          (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_playlist_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
PDAPreferencesApply               (GtkButton       *button,
                                        gpointer         user_data);
