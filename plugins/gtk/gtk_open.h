/*****************************************************************************
 * gtk_open.h: prototypes for open functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: gtk_open.h,v 1.1 2001/05/15 01:01:44 stef Exp $
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

gboolean
GtkFileOpenShow                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void GtkFileOpenCancel( GtkButton * button, gpointer user_data);

void GtkFileOpenOk( GtkButton * button, gpointer user_data );


gboolean
GtkDiscOpenShow                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);
void GtkDiscOpenDvd( GtkToggleButton * togglebutton, gpointer user_data );
void GtkDiscOpenVcd( GtkToggleButton *togglebutton, gpointer user_data );
void GtkDiscOpenOk( GtkButton * button, gpointer user_data );
void GtkDiscOpenCancel( GtkButton * button, gpointer user_data);




gboolean
GtkNetworkOpenShow                     (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);
void GtkNetworkOpenOk( GtkButton *button, gpointer user_data );
void GtkNetworkOpenCancel( GtkButton * button, gpointer user_data);
void GtkNetworkOpenBroadcast( GtkToggleButton * togglebutton,
                              gpointer user_data );


