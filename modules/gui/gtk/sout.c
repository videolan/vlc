/*****************************************************************************
 * sout.c :
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#ifdef MODULE_NAME_IS_gnome
#   include <gnome.h>
#else
#   include <gtk/gtk.h>
#endif

#include <string.h>

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"

#include "playlist.h"
#include "common.h"

void GtkSoutSettings ( GtkButton       *button,
                       gpointer         user_data );

void GtkSoutSettingsChanged  ( GtkWidget *button, gpointer user_data);






void GtkSoutSettingsAccessFile              (GtkToggleButton *togglebutton,
                                                gpointer         user_data)
{
    GtkWidget *     p_sout;

    p_sout = gtk_widget_get_toplevel( GTK_WIDGET (togglebutton) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_file_path_label" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_file_path" ),
                    gtk_toggle_button_get_active( togglebutton ) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_mux_ts" ), TRUE );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_mux_ps" ), TRUE );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_mux_avi" ), TRUE );

    GtkSoutSettingsChanged( GTK_WIDGET( togglebutton ), user_data );
}


void GtkSoutSettingsAccessUdp               (GtkToggleButton *togglebutton,
                                                gpointer         user_data)
{
    GtkWidget *     p_sout;

    p_sout = gtk_widget_get_toplevel( GTK_WIDGET (togglebutton) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_udp_address_label" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_udp_address_combo" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_udp_port_label" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_udp_port" ),
                    gtk_toggle_button_get_active( togglebutton ) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_mux_ts" ), TRUE );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_mux_ps" ), FALSE );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_sout ),
                    "sout_mux_avi" ), FALSE );

    gtk_toggle_button_set_active(  gtk_object_get_data( GTK_OBJECT( p_sout ),
                                   "sout_mux_ts" ), TRUE );

    GtkSoutSettingsChanged( GTK_WIDGET( togglebutton ), user_data );
}


void GtkSoutSettingsChanged  ( GtkWidget *button,
                               gpointer         user_data)
{
#define SELECTED( s ) GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET(button), \
                                   (s) ) )->active
    //intf_thread_t * p_intf = GtkGetIntf( button );
    GString *       p_target;

    p_target = g_string_new( "" );

    /* first set access */
    if( SELECTED( "sout_access_file" ) )
    {
        g_string_append( p_target, "file/" );
    }
    else if( SELECTED( "sout_access_udp" ) )
    {
        g_string_append( p_target, "udp/" );
    }
    else if( SELECTED( "sout_access_rtp" ) )
    {
        g_string_append( p_target, "rtp/" );
    }

    /* then set muxer */
    if( SELECTED( "sout_mux_ts" ) )
    {
        g_string_append( p_target, "ts://" );
    }
    else if( SELECTED( "sout_mux_ps" ) )
    {
        g_string_append( p_target, "ps://" );
    }
    else if( SELECTED( "sout_mux_avi" ) )
    {
        g_string_append( p_target, "avi://" );
    }

    /* last part of the url */
    if( SELECTED( "sout_access_file" ) )
    {
        g_string_append( p_target,
                         gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                             GTK_WIDGET(button), "sout_file_path" ) ) ) );
    }
    else if( SELECTED( "sout_access_udp" ) || SELECTED( "sout_access_rtp" ) )
    {
        g_string_append( p_target,
                         gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                             GTK_WIDGET(button), "sout_udp_address" ) ) ) );
        g_string_append( p_target, ":" );
        g_string_sprintfa( p_target, "%i",
                         gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( lookup_widget(
                                                           GTK_WIDGET(button), "sout_udp_port" ) ) ) );
    }

    gtk_entry_set_text( GTK_ENTRY( lookup_widget(
                                   GTK_WIDGET(button), "sout_entry_target" ) ),
                        p_target->str );
    g_string_free( p_target, TRUE );
}


/****************************************************************************/
void GtkSoutSettingsOk     ( GtkButton       *button,
                             gpointer         user_data)
{
    /* Hide the dialog box */
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );

    /* set sout */
#if 0
    psz_target = gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                       GTK_WIDGET(button), "sout_entry_target" ) ) );

    config_PutPsz( p_intf, "sout", psz_target );
#endif
}

void GtkSoutSettingsCancel ( GtkButton      *button,
                             gpointer        user_data)
{
    /* Hide the dialog box */
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

void GtkSoutSettings ( GtkButton       *button,
                       gpointer         user_data )
{
    intf_thread_t * p_intf = GtkGetIntf( button );

    gtk_widget_show( p_intf->p_sys->p_sout );
    gdk_window_raise( p_intf->p_sys->p_sout->window );
}



