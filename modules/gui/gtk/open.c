/*****************************************************************************
 * gtk_open.c : functions to handle file/disc/network open widgets.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: open.c,v 1.2 2002/08/24 11:57:07 sam Exp $
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

#include "netutils.h"

static void GtkOpenShow( intf_thread_t *, int );

static void GtkFileOpenChanged    ( GtkWidget *, gpointer );
static void GtkDiscOpenChanged    ( GtkWidget *, gpointer );
static void GtkNetworkOpenChanged ( GtkWidget *, gpointer );
static void GtkSatOpenChanged     ( GtkWidget *, gpointer );

/*****************************************************************************
 * File requester callbacks
 *****************************************************************************
 * The following callbacks are related to the file requester.
 *****************************************************************************/
void GtkFileShow( GtkButton * button, gpointer user_data )
{
    GtkWidget * p_file = create_intf_file();

    gtk_object_set_data( GTK_OBJECT(p_file), "p_intf", GtkGetIntf( button ) );

    gtk_widget_show( p_file );
    gdk_window_raise( p_file->window );
}

void GtkFileOk( GtkButton * button, gpointer user_data )
{
    GtkWidget * p_file = gtk_widget_get_toplevel( GTK_WIDGET (button) );

    char *psz_filename;
    intf_thread_t * p_intf = GtkGetIntf( button );

    /* add the new file to the dialog box */
    psz_filename =
            gtk_file_selection_get_filename( GTK_FILE_SELECTION( p_file ) );
    gtk_entry_set_text( GTK_ENTRY( lookup_widget( p_intf->p_sys->p_open,
                                                  "entry_file" ) ),
                        psz_filename );
    gtk_widget_destroy( p_file );
}

void GtkFileCancel( GtkButton * button, gpointer user_data )
{
    gtk_widget_destroy( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

/*****************************************************************************
 * Open file callbacks
 *****************************************************************************
 * The following callbacks are related to the file tab.
 *****************************************************************************/
gboolean GtkFileOpenShow( GtkWidget       *widget,
                          gpointer         user_data )
{
    GtkOpenShow( GtkGetIntf( widget ), 0 );

    return TRUE;
}

static void GtkFileOpenChanged( GtkWidget * button, gpointer user_data )
{
    GString *       p_target;

    p_target = g_string_new( "file://" );
    g_string_append( p_target,
                     gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                     GTK_WIDGET(button), "entry_file" ) ) ) );
    gtk_entry_set_text( GTK_ENTRY( lookup_widget(
                                   GTK_WIDGET(button), "entry_open" ) ),
                        p_target->str );
    g_string_free( p_target, TRUE );
}

/*****************************************************************************
 * Open disc callbacks
 *****************************************************************************
 * The following callbacks are related to the disc manager.
 *****************************************************************************/
gboolean GtkDiscOpenShow( GtkWidget       *widget,
                          gpointer         user_data)
{
    GtkOpenShow( GtkGetIntf( widget ), 1 );

    return TRUE;
}

void GtkDiscOpenDvd( GtkToggleButton * togglebutton, gpointer user_data )
{
    intf_thread_t * p_intf = GtkGetIntf( togglebutton );
    char *psz_device;

    if( togglebutton->active
         && (psz_device = config_GetPsz( p_intf, "dvd" )) )
    {
        gtk_entry_set_text(
            GTK_ENTRY( lookup_widget( GTK_WIDGET(togglebutton),
                                      "disc_name" ) ), psz_device );
        free( psz_device );
    }
}

void GtkDiscOpenVcd( GtkToggleButton * togglebutton, gpointer user_data )
{
    intf_thread_t * p_intf = GtkGetIntf( togglebutton );
    char *psz_device;

    if( togglebutton->active
         && (psz_device = config_GetPsz( p_intf, "vcd" )) )
    {
        gtk_entry_set_text(
            GTK_ENTRY( lookup_widget( GTK_WIDGET(togglebutton),
                                      "disc_name" ) ), psz_device );
        free( psz_device );
    }
}

static void GtkDiscOpenChanged( GtkWidget * button, gpointer user_data )
{
    GString * p_target = g_string_new( "" );

    if( GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET(button), 
                                          "disc_dvd" ) )->active )
    {
        g_string_append( p_target, "dvd://" );
    }
    else if( GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET(button),
                                               "disc_vcd" ) )->active )
    {
        g_string_append( p_target, "vcd://" );
    }       

    g_string_append( p_target,
                     gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                     GTK_WIDGET(button), "disc_name" ) ) ) );
    g_string_sprintfa( p_target, "@%i,%i",
                       gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "disc_title" ) ) ),
                       gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "disc_chapter" ) ) ) );

    gtk_entry_set_text( GTK_ENTRY( lookup_widget(
                                   GTK_WIDGET(button), "entry_open" ) ),
                        p_target->str );
    g_string_free( p_target, TRUE );
}

/*****************************************************************************
 * Network stream callbacks
 *****************************************************************************
 * The following callbacks are related to the network stream manager.
 *****************************************************************************/
gboolean GtkNetworkOpenShow( GtkWidget       *widget,
                             gpointer         user_data )
{
    GtkOpenShow( GtkGetIntf( widget ), 2 );

    return TRUE;
}

static void GtkNetworkOpenChanged( GtkWidget *button, gpointer user_data )
{
    intf_thread_t * p_intf = GtkGetIntf( button );
    GString *       p_target = g_string_new( "" );

    unsigned int    i_port;
    vlc_bool_t      b_channel;

    /* Manage channel server */
    b_channel = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
            lookup_widget( GTK_WIDGET(button), "network_channel" ) ) );
    config_PutInt( p_intf, "network-channel", b_channel );

#define SELECTED( s ) GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET(button), \
                       (s) ) )->active
    /* Check which option was chosen */
    if( SELECTED( "network_udp" ) )
    {
        g_string_append( p_target, "udp://" );
        i_port = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(
                               lookup_widget( GTK_WIDGET(button),
                                              "network_udp_port" ) ) );
        if( i_port != 1234 )
        {
            g_string_sprintfa( p_target, "@:%i", i_port );
        }
    }
    else if( SELECTED( "network_multicast" ) )
    {
        g_string_sprintfa( p_target, "udp://@%s",
                           gtk_entry_get_text( GTK_ENTRY(
                            lookup_widget( GTK_WIDGET(button),
                                           "network_multicast_address" ) ) ) );
        i_port = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(
                               lookup_widget( GTK_WIDGET(button),
                                              "network_multicast_port" ) ) );
        if( i_port != 1234 )
        {
            g_string_sprintfa( p_target, ":%i", i_port );
        }
    }
    else if( SELECTED( "network_channel" ) )
    {
        char *          psz_channel;
        unsigned int    i_channel_port;

        if( p_intf->p_vlc->p_channel == NULL )
        {
            network_ChannelCreate( p_intf );
        }

        psz_channel = gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                        GTK_WIDGET(button), "network_channel_address" ) ) );
        i_channel_port = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(
            lookup_widget( GTK_WIDGET(button), "network_channel_port" ) ) );

        config_PutPsz( p_intf, "channel-server", psz_channel );
        if( i_channel_port < 65536 )
        {
            config_PutInt( p_intf, "channel-port", i_channel_port );
        }

        /* FIXME: we should use a playlist server instead */
        g_string_append( p_target, "udp://" );
    }
    else if( SELECTED( "network_http" ) )
    {
        g_string_sprintfa( p_target, "http://%s",
                           gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                               GTK_WIDGET(button), "network_http_url" ) ) ) );
    }

    gtk_entry_set_text( GTK_ENTRY( lookup_widget(
                                   GTK_WIDGET(button), "entry_open" ) ),
                        p_target->str );
    g_string_free( p_target, TRUE );
}

void GtkNetworkOpenUDP( GtkToggleButton *togglebutton,
                                        gpointer user_data )
{
    GtkWidget *     p_open;

    p_open = gtk_widget_get_toplevel( GTK_WIDGET (togglebutton) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_udp_port_label" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_udp_port" ),
                    gtk_toggle_button_get_active( togglebutton ) );

    GtkNetworkOpenChanged( GTK_WIDGET( togglebutton ), user_data );
}

void GtkNetworkOpenMulticast( GtkToggleButton *togglebutton,
                                              gpointer user_data )
{
    GtkWidget *     p_open;

    p_open = gtk_widget_get_toplevel( GTK_WIDGET (togglebutton) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_multicast_address_label" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_multicast_address_combo" ),
                    gtk_toggle_button_get_active( togglebutton ) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_multicast_port_label" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_multicast_port" ),
                    gtk_toggle_button_get_active( togglebutton ) );

    GtkNetworkOpenChanged( GTK_WIDGET( togglebutton ), user_data );
}


void GtkNetworkOpenChannel( GtkToggleButton *togglebutton,
                                       gpointer user_data )
{
    GtkWidget *     p_open;

    p_open = gtk_widget_get_toplevel( GTK_WIDGET (togglebutton) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_channel_address_label" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_channel_address_combo" ),
                    gtk_toggle_button_get_active( togglebutton ) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_channel_port_label" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_channel_port" ),
                    gtk_toggle_button_get_active( togglebutton ) );

    GtkNetworkOpenChanged( GTK_WIDGET( togglebutton ), user_data );
}

void GtkNetworkOpenHTTP( GtkToggleButton *togglebutton,
                                         gpointer user_data )
{   
    GtkWidget *     p_open;

    p_open = gtk_widget_get_toplevel( GTK_WIDGET (togglebutton) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_http_url_label" ),
                    gtk_toggle_button_get_active( togglebutton ) );
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_open ),
                    "network_http_url" ),
                    gtk_toggle_button_get_active( togglebutton ) );

    GtkNetworkOpenChanged( GTK_WIDGET( togglebutton ), user_data );
}

/*****************************************************************************
 * Open satellite callbacks
 *****************************************************************************
 * The following callbacks are related to the satellite card manager.
 *****************************************************************************/
gboolean GtkSatOpenShow( GtkWidget       *widget,
                         gpointer         user_data)
{
    GtkOpenShow( GtkGetIntf( widget ), 3 );

    return TRUE;
}

static void GtkSatOpenChanged( GtkWidget * button, gpointer user_data )
{
    GString *       p_target = g_string_new( "" );

    g_string_sprintfa( p_target, "%s://%d,%d,%ld,%d", "satellite",
                       gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "sat_freq" ) ) ),
                       !GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET( button ),
                                               "sat_pol_vert" ) )->active,
                       strtol( gtk_entry_get_text( GTK_ENTRY( GTK_COMBO( 
                               lookup_widget( GTK_WIDGET( button ), "sat_fec" )
                               )->entry ) ), NULL, 10 ),
                       gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "sat_srate" ) ) ) );

    gtk_entry_set_text( GTK_ENTRY( lookup_widget(
                                   GTK_WIDGET(button), "entry_open" ) ),
                        p_target->str );
    g_string_free( p_target, TRUE );
}

void
GtkSatOpenToggle                       (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
    if( togglebutton->active )
    {
        GtkSatOpenChanged( GTK_WIDGET( togglebutton ), user_data );
    }
}

/******************************
  ******************************/

static void GtkOpenShow( intf_thread_t *p_intf, int i_page )
{
    char *psz_var;
    GtkWidget *p_notebook;

    /* If we have already created this window, do nothing */
    if( GTK_IS_WIDGET( p_intf->p_sys->p_open ) )
    {
        goto setpage;
    }

    p_intf->p_sys->p_open = create_intf_open();
    gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_open ),
                         "p_intf", p_intf );

    /* FileOpen stuff */
    psz_var = config_GetPsz( p_intf, "search-path" );
    if( psz_var )
    {
        gtk_file_selection_set_filename( GTK_FILE_SELECTION(
            p_intf->p_sys->p_open ), psz_var );
        free( psz_var );
    }

    /* Disc stuff */
    psz_var = config_GetPsz( p_intf, "dvd" );
    if( psz_var )
    {
        gtk_entry_set_text( GTK_ENTRY( gtk_object_get_data(
            GTK_OBJECT( p_intf->p_sys->p_open ), "disc_name" ) ),
            psz_var );
        free( psz_var );
    }

    /* Network stuff */
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_open ), "network_udp_port" ) ),
        config_GetInt( p_intf, "server-port" ) );

    psz_var = config_GetPsz( p_intf, "channel-server" );
    if( psz_var )
    {
        gtk_entry_set_text( GTK_ENTRY( gtk_object_get_data(
            GTK_OBJECT( p_intf->p_sys->p_open ), "network_channel_address" ) ),
            psz_var );
        free( psz_var );
    }

    gtk_spin_button_set_value( GTK_SPIN_BUTTON( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_open ), "network_channel_port" ) ),
        config_GetInt( p_intf, "channel-port" ) );

    gtk_toggle_button_set_active( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_open ), "network_channel" ),
        config_GetInt( p_intf, "network-channel" ) );

    /* Satellite stuff */
    psz_var = config_GetPsz( p_intf, "frequency" );
    if( psz_var )
    {
        gtk_entry_set_text( GTK_ENTRY( gtk_object_get_data(
            GTK_OBJECT( p_intf->p_sys->p_open ), "sat_freq" ) ),
            psz_var );
        free( psz_var );
    }

    psz_var = config_GetPsz( p_intf, "symbol-rate" );
    if( psz_var )
    {
        gtk_entry_set_text( GTK_ENTRY( gtk_object_get_data(
            GTK_OBJECT( p_intf->p_sys->p_open ), "sat_srate" ) ),
            psz_var );
        free( psz_var );
    }

    /* Set the right page */
setpage:
    p_notebook = lookup_widget( GTK_WIDGET( p_intf->p_sys->p_open ),
                                "open_notebook" );
    gtk_notebook_set_page( GTK_NOTEBOOK( p_notebook ), i_page );

    gtk_widget_show( p_intf->p_sys->p_open );
    gdk_window_raise( p_intf->p_sys->p_open->window );
}

void GtkOpenOk( GtkButton * button, gpointer user_data )
{
    /* Check what was pressed */
    intf_thread_t * p_intf = GtkGetIntf( button );
    playlist_t *    p_playlist;
    GtkCList *      p_playlist_clist;
    gchar *         psz_target;

    /* Hide the dialog box */
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );

    /* Update the playlist */
    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    psz_target = gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                       GTK_WIDGET(button), "entry_open" ) ) );
    playlist_Add( p_playlist, (char*)psz_target,
                  PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );

    /* catch the GTK CList */
    p_playlist_clist = GTK_CLIST( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_playwin ), "playlist_clist" ) );
    /* update the plugin display */
    GtkRebuildCList( p_playlist_clist, p_playlist );

    vlc_object_release( p_playlist );
}

void GtkOpenCancel( GtkButton * button, gpointer user_data )
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

void GtkOpenChanged( GtkWidget * button, gpointer user_data )
{
    intf_thread_t * p_intf = GtkGetIntf( button );
    GtkWidget *p_notebook;
    int i_page;

    p_notebook = lookup_widget( GTK_WIDGET( p_intf->p_sys->p_open ),
                                "open_notebook" );
    i_page = gtk_notebook_get_current_page( GTK_NOTEBOOK( p_notebook ) );

    switch( i_page )
    {
        case 0:
            GtkFileOpenChanged( button, NULL );
            break;
        case 1:
            GtkDiscOpenChanged( button, NULL );
            break;
        case 2:
            GtkNetworkOpenChanged( button, NULL );
            break;
        case 3:
            GtkSatOpenChanged( button, NULL );
            break;
    }
}

