/*****************************************************************************
 * familiar_callbacks.h : familiar plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: familiar_callbacks.h,v 1.7.2.5 2002/10/07 21:37:11 jpsaman Exp $
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

gboolean GtkExit                ( GtkWidget *, gpointer );

void ReadDirectory(GtkCList *clist, char *psz_dir);

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
on_familiar_delete_event               (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_buttonSave_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_buttonApply_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_buttonCancel_clicked                (GtkButton       *button,
                                        gpointer         user_data);
