/*****************************************************************************
 * gtk_display.c: Gtk+ tools for main interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: gtk_display.c,v 1.19 2002/03/11 07:23:09 gbazin Exp $
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <videolan/vlc.h>

#ifdef MODULE_NAME_IS_gnome
#   include <gnome.h>
#else
#   include <gtk/gtk.h>
#endif

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"

#include "video.h"
#include "video_output.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_menu.h"
#include "gtk_display.h"
#include "gtk_common.h"

/*****************************************************************************
 * GtkDisplayDate: display stream date
 *****************************************************************************
 * This function displays the current date related to the position in
 * the stream. It is called whenever the slider changes its value.
 * The lock has to be taken before you call the function.
 *****************************************************************************/
void GtkDisplayDate( GtkAdjustment *p_adj )
{
    intf_thread_t *p_intf;
   
    p_intf = gtk_object_get_data( GTK_OBJECT( p_adj ), "p_intf" );

    if( p_input_bank->pp_input[0] != NULL )
    {
#define p_area p_input_bank->pp_input[0]->stream.p_selected_area
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];

        gtk_frame_set_label( GTK_FRAME( p_intf->p_sys->p_slider_frame ),
                            input_OffsetToTime( p_input_bank->pp_input[0], psz_time,
                                   ( p_area->i_size * p_adj->value ) / 100 ) );
#undef p_area
     }
}


/*****************************************************************************
 * GtkModeManage: actualise the aspect of the interface whenever the input
 *                changes.
 *****************************************************************************
 * The lock has to be taken before you call the function.
 *****************************************************************************/
gint GtkModeManage( intf_thread_t * p_intf )
{
    GtkWidget *     p_dvd_box;
    GtkWidget *     p_file_box;
    GtkWidget *     p_network_box;
    GtkWidget *     p_slider;
    GtkWidget *     p_label;
    GtkWidget *     p_channel;
    boolean_t       b_control;

#define GETWIDGET( ptr, name ) GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( \
                           p_intf->p_sys->ptr ) , ( name ) ) )
    /* hide all boxes except default file box */
    p_file_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "file_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_file_box ) );

    p_network_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "network_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_network_box ) );

    p_dvd_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "dvd_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_dvd_box ) );

    /* hide slider */
    p_slider = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                           p_intf->p_sys->p_window ), "slider_frame" ) );
    gtk_widget_hide( GTK_WIDGET( p_slider ) );

    /* controls unavailable */
    b_control = 0;

    /* show the box related to current input mode */
    if( p_input_bank->pp_input[0] != NULL )
    {
        switch( p_input_bank->pp_input[0]->stream.i_method & 0xf0 )
        {
            case INPUT_METHOD_FILE:
//intf_WarnMsg( 2, "intf info: file method" );
                gtk_widget_show( GTK_WIDGET( p_file_box ) );
                p_label = gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_window ),
                            "label_status" );
                gtk_label_set_text( GTK_LABEL( p_label ),
                                    p_input_bank->pp_input[0]->psz_source );
                break;
            case INPUT_METHOD_DISC:
//intf_WarnMsg( 2, "intf info: disc method" );
                gtk_widget_show( GTK_WIDGET( p_dvd_box ) );
                break;
            case INPUT_METHOD_NETWORK:
//intf_WarnMsg( 2, "intf info: network method" );
                gtk_widget_show( GTK_WIDGET( p_network_box ) );
                p_label = gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_window ),
                            "network_address_label" );
                gtk_label_set_text( GTK_LABEL( p_label ),
                                    p_input_bank->pp_input[0]->psz_source );
                p_channel = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                           p_intf->p_sys->p_window ), "network_channel_box" ) );
                if( config_GetIntVariable( "network_channel" ) )
                {
                    gtk_widget_show( GTK_WIDGET( p_channel ) );
                }
                else
                {
                    gtk_widget_hide( GTK_WIDGET( p_channel ) );
                }

                break;
            default:
                intf_WarnMsg( 3, "intf: can't determine input method" );
                gtk_widget_show( GTK_WIDGET( p_file_box ) );
                p_label = gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_window ),
                            "label_status" );
                gtk_label_set_text( GTK_LABEL( p_label ),
                                    p_input_bank->pp_input[0]->psz_source );
                break;
        }
    
        /* initialize and show slider for seekable streams */
        if( p_input_bank->pp_input[0]->stream.b_seekable )
        {
            p_intf->p_sys->p_adj->value = p_intf->p_sys->f_adj_oldvalue = 0;
            gtk_signal_emit_by_name( GTK_OBJECT( p_intf->p_sys->p_adj ),
                                     "value_changed" );
            gtk_widget_show( GTK_WIDGET( p_slider ) );
        }
    
        /* control buttons for free pace streams */
        b_control = p_input_bank->pp_input[0]->stream.b_pace_control;

        /* get ready for menu regeneration */
        p_intf->p_sys->b_program_update = 1;
        p_intf->p_sys->b_title_update = 1;
        p_intf->p_sys->b_chapter_update = 1;
        p_intf->p_sys->b_audio_update = 1;
        p_intf->p_sys->b_spu_update = 1;
        p_intf->p_sys->i_part = 0;
    
        p_input_bank->pp_input[0]->stream.b_changed = 0;
        intf_WarnMsg( 3, "intf: stream has changed, refreshing interface" );
    }
    else
    {
        if( config_GetIntVariable( "network_channel" ) )
        {
            gtk_widget_show( GTK_WIDGET( p_network_box ) );

            p_channel = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                       p_intf->p_sys->p_window ), "network_channel_box" ) );
            gtk_widget_show( GTK_WIDGET( p_channel ) );
        }
        else
        {
//intf_WarnMsg( 2, "intf info: default to file method" );
            /* default mode */
            p_label = gtk_object_get_data(
                    GTK_OBJECT( p_intf->p_sys->p_window ), "label_status" );
            gtk_label_set_text( GTK_LABEL( p_label ), "" );
            gtk_widget_show( GTK_WIDGET( p_file_box ) );

            /* unsensitize menus */
            gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_program"),
                    FALSE );
            gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_title"), FALSE );
            gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_chapter"),
                                      FALSE );
            gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_audio"), FALSE );
            gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_subpictures"),
                                      FALSE );
            gtk_widget_set_sensitive( GETWIDGET(p_popup,"popup_navigation"),
                                      FALSE );
            gtk_widget_set_sensitive( GETWIDGET(p_popup,"popup_audio"), FALSE );
            gtk_widget_set_sensitive( GETWIDGET(p_popup,"popup_subpictures"),
                                      FALSE );
        }
    }

    /* set control items */
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_back"), FALSE );
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_eject"), !b_control);
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_pause"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_slow"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_fast"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_popup, "popup_back"), FALSE );
    gtk_widget_set_sensitive( GETWIDGET(p_popup, "popup_pause"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_popup, "popup_slow"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_popup, "popup_fast"), b_control );

#undef GETWIDGET
    return TRUE;
}
