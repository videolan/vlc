/*****************************************************************************
 * gtk_callbacks.h : Callbacks for the gtk plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_callbacks.h,v 1.21 2002/07/11 19:28:13 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <gtk/gtk.h>

#include "config.h"

#include "gtk_control.h"
#include "gtk_menu.h"
#include "gtk_open.h"
#include "gtk_modules.h"
#include "gtk_playlist.h"
#include "gtk_preferences.h"

/* General glade callbacks */

/*****************************************************************************
 * main window callbacks: specific prototypes are in headers listed before
 *****************************************************************************/

gboolean GtkExit                ( GtkWidget *, gpointer );
gboolean GtkWindowToggle        ( GtkWidget *, gpointer );
gboolean GtkFullscreen          ( GtkWidget *, gpointer );
gboolean GtkSliderRelease       ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkSliderPress         ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkWindowDelete        ( GtkWidget * widget, GdkEvent *, gpointer );
gboolean GtkJumpShow            ( GtkWidget *, gpointer );
gboolean GtkAboutShow           ( GtkWidget *, gpointer );
gboolean GtkMessagesShow        ( GtkWidget *, gpointer );
void     GtkTitlePrev           ( GtkButton * button, gpointer );
void     GtkTitleNext           ( GtkButton * button, gpointer );
void     GtkChapterPrev         ( GtkButton *, gpointer );
void     GtkChapterNext         ( GtkButton * button, gpointer );
void     GtkAboutOk             ( GtkButton *, gpointer );
void     GtkWindowDrag          ( GtkWidget *, GdkDragContext *,
                                  gint, gint, GtkSelectionData *,
                                  guint , guint, gpointer );
void     GtkJumpOk              ( GtkButton * button, gpointer );
void     GtkJumpCancel          ( GtkButton * button, gpointer user_data );

void     GtkNetworkJoin         ( GtkEditable *, gpointer );
void     GtkChannelGo           ( GtkButton *, gpointer );

void     GtkNetworkOpenChannel  ( GtkToggleButton *, gpointer );

gboolean
GtkDiscEject                           (GtkWidget       *widget,
                                        gpointer         user_data);

void
GtkMessagesOk                          (GtkButton       *button,
                                        gpointer         user_data);

gboolean
GtkMessagesDelete                      (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
GtkSatOpenShow                         (GtkWidget       *widget,
                                        gpointer         user_data);

void
GtkSatOpenOk                           (GtkButton       *button,
                                        gpointer         user_data);

void
GtkSatOpenCancel                       (GtkButton       *button,
                                        gpointer         user_data);

void
GtkNetworkOpenUDP                      (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
GtkNetworkOpenMulticast                (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
GtkNetworkOpenCS                       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
GtkNetworkOpenHTTP                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
GtkNetworkOpenChannel                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
GtkOpenOk                              (GtkButton       *button,
                                        gpointer         user_data);

void
GtkOpenCancel                          (GtkButton       *button,
                                        gpointer         user_data);

void
GtkOpenChanged                         (GtkWidget       *button,
                                        gpointer         user_data);

void
GtkOpenNotebookChanged                 (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        gint             page_num,
                                        gpointer         user_data);

void
GtkSatOpenToggle                       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
GtkFileShow                            (GtkButton       *button,
                                        gpointer         user_data);

void
GtkFileOk                              (GtkButton       *button,
                                        gpointer         user_data);

void
GtkFileCancel                          (GtkButton       *button,
                                        gpointer         user_data);
