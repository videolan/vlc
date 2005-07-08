/*****************************************************************************
 * gtk_callbacks.h : Callbacks for the gtk plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN (Centrale RÃ©seaux) and its contributors
 * $Id$
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

#include "control.h"
#include "menu.h"
#include "open.h"
#include "modules.h"
#include "playlist.h"
#include "preferences.h"

/* General glade callbacks */

/*****************************************************************************
 * main window callbacks: specific prototypes are in headers listed before
 *****************************************************************************/

#ifdef MODULE_NAME_IS_gtk
gboolean GtkExit                ( GtkWidget *, gpointer );
#else
gboolean GnomeExit              ( GtkWidget *, gpointer );
#endif
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

void
GtkClose                               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkVolumeUp                            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkVolumeDown                          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkVolumeMute                          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkMenubarDeinterlace                  (GtkMenuItem *menuitem,
                                        gpointer         user_data);

void
GtkPopupDeinterlace                    (GtkRadioMenuItem *radiomenuitem,
                                        gpointer         user_data);


void
GtkOpenSubtitleShow                    (GtkButton       *button,
                                        gpointer         user_data);

void
GtkSoutSettings                        (GtkButton       *button,
                                        gpointer         user_data);

void
GtkSoutSettingsCancel                  (GtkButton       *button,
                                        gpointer         user_data);

void
GtkSoutSettingsChanged                 (GtkWidget *button,
                                        gpointer         user_data);

void
GtkSoutSettingsOk                      (GtkButton       *button,
                                        gpointer         user_data);

void
GtkSoutSettingsAccessFile              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
GtkSoutSettingsAccessUdp               (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
GtkOpenSoutShow                        (GtkButton       *button,
                                        gpointer         user_data);

