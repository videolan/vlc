/*****************************************************************************
 * gtk_callbacks.h : Callbacks for the gtk plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_callbacks.h,v 1.16 2002/01/09 02:01:14 sam Exp $
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
 *  main window callbacks: specific prototypes are in headers listed before
 *****************************************************************************/

gboolean GtkExit                ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkWindowToggle        ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkFullscreen          ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkSliderRelease       ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkSliderPress         ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkWindowDelete        ( GtkWidget * widget, GdkEvent *, gpointer );
gboolean GtkJumpShow            ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkAboutShow           ( GtkWidget *, GdkEventButton *, gpointer );
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
void     GtkExitActivate        ( GtkMenuItem *, gpointer );
void     GtkWindowToggleActivate( GtkMenuItem *, gpointer );
void     GtkFullscreenActivate  ( GtkMenuItem *, gpointer );
void     GtkAboutActivate       ( GtkMenuItem *, gpointer );
void     GtkJumpActivate        ( GtkMenuItem *, gpointer );

void     GtkNetworkJoin         ( GtkEditable *, gpointer );
void     GtkChannelGo           ( GtkButton *, gpointer );

void     GtkNetworkOpenChannel  ( GtkToggleButton *, gpointer );

void
GtkEjectDiscActivate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
GtkDiscEject                           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);
