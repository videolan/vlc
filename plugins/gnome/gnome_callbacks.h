/*****************************************************************************
 * gnome_callbacks.h : Callbacks for the Gnome plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gnome_callbacks.h,v 1.12 2001/03/21 13:42:34 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "config.h"
#include <gnome.h>

/*****************************************************************************
 * Callback prototypes
 *****************************************************************************/
void
on_menubar_open_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_exit_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_playlist_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_plugins_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_preferences_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_about_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbar_open_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_back_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_stop_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_play_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_pause_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_playlist_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_prev_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_next_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_popup_play_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_pause_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_exit_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_intf_window_destroy                 (GtkObject       *object,
                                        gpointer         user_data);

void
on_fileopen_ok_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_fileopen_cancel_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_intf_fileopen_destroy               (GtkObject       *object,
                                        gpointer         user_data);

void
on_popup_open_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_about_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_intf_playlist_destroy               (GtkObject       *object,
                                        gpointer         user_data);

void
on_playlist_close_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_popup_slow_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_fast_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbar_slow_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_fast_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_intf_modules_destroy                (GtkObject       *object,
                                        gpointer         user_data);

void
on_modules_ok_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_modules_apply_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_modules_cancel_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_intf_playlist_destroy               (GtkObject       *object,
                                        gpointer         user_data);

void
on_playlist_ok_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_menubar_modules_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_intf_window_drag_data_received      (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data);

void
on_menubar_audio_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_disc_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbar_disc_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_disc_ok_clicked                     (GtkButton       *button,
                                        gpointer         user_data);

void
on_disc_cancel_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_disc_dvd_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_disc_vcd_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_popup_disc_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_audio_toggle                (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data);

void
on_menubar_subtitle_toggle             (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data);

void
on_popup_audio_toggle                  (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data);

void
on_popup_subtitle_toggle               (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data);

void
on_popup_navigation_toggle             (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data);

void
on_menubar_title_toggle                (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data);

void
on_menubar_network_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbar_network_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_network_ok_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_network_cancel_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_popup_network_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_slider_button_press_event           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_slider_button_release_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_menubar_chapter_toggle              (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data);
