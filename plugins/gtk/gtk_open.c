/*****************************************************************************
 * gtk_open.c : functions to handle file/disc/network open widgets.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_open.c,v 1.23 2002/04/23 14:16:20 sam Exp $
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

#include <videolan/vlc.h>

#ifdef MODULE_NAME_IS_gnome
#   include <gnome.h>
#else
#   include <gtk/gtk.h>
#endif

#include <string.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_playlist.h"
#include "gtk_common.h"

#include "netutils.h"

/*****************************************************************************
 * Fileopen callbacks
 *****************************************************************************
 * The following callbacks are related to the file requester.
 *****************************************************************************/
gboolean GtkFileOpenShow( GtkWidget       *widget,
                          GdkEventButton  *event,
                          gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    /* If we have never used the file selector, open it */
    if( !GTK_IS_WIDGET( p_intf->p_sys->p_fileopen ) )
    {
        char *psz_path;

        p_intf->p_sys->p_fileopen = create_intf_fileopen();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_fileopen ),
                             "p_intf", p_intf );

        if( (psz_path = config_GetPszVariable( "search-path" )) )
            gtk_file_selection_set_filename( GTK_FILE_SELECTION(
                p_intf->p_sys->p_fileopen ), psz_path );
        if( psz_path ) free( psz_path );
    }

    gtk_widget_show( p_intf->p_sys->p_fileopen );
    gdk_window_raise( p_intf->p_sys->p_fileopen->window );

    return TRUE;
}


void GtkFileOpenCancel( GtkButton * button, gpointer user_data )
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

void GtkFileOpenOk( GtkButton * button, gpointer user_data )
{
    intf_thread_t * p_intf = GetIntf( GTK_WIDGET(button), "intf_fileopen" );
    GtkCList *      p_playlist_clist;
    GtkWidget *     p_filesel;
    gchar *         psz_filename;
    int             i_end = p_main->p_playlist->i_size;

    /* hide the file selector */
    p_filesel = gtk_widget_get_toplevel( GTK_WIDGET(button) );
    gtk_widget_hide( p_filesel );

    /* add the new file to the interface playlist */
    psz_filename =
        gtk_file_selection_get_filename( GTK_FILE_SELECTION( p_filesel ) );
    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, (char*)psz_filename );

    /* catch the GTK CList */
    p_playlist_clist = GTK_CLIST( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_playlist ), "playlist_clist" ) );
    /* update the plugin display */
    GtkRebuildCList( p_playlist_clist, p_main->p_playlist );

    /* end current item, select added item  */
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
}

/*****************************************************************************
 * Open disc callbacks
 *****************************************************************************
 * The following callbacks are related to the disc manager.
 *****************************************************************************/
gboolean GtkDiscOpenShow( GtkWidget       *widget,
                          GdkEventButton  *event,
                          gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_disc ) )
    {
        p_intf->p_sys->p_disc = create_intf_disc();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_disc ),
                             "p_intf", p_intf );
    }

    gtk_widget_show( p_intf->p_sys->p_disc );
    gdk_window_raise( p_intf->p_sys->p_disc->window );

    return TRUE;
}


void GtkDiscOpenDvd( GtkToggleButton * togglebutton, gpointer user_data )
{
    if( togglebutton->active )
    {
        char *psz_dvd_device;

        if( (psz_dvd_device = config_GetPszVariable( "dvd" )) )
            gtk_entry_set_text(
                GTK_ENTRY( lookup_widget( GTK_WIDGET(togglebutton),
                                          "disc_name" ) ), psz_dvd_device );
        if( psz_dvd_device ) free( psz_dvd_device );
    }
}

void GtkDiscOpenVcd( GtkToggleButton * togglebutton, gpointer user_data )
{
    if( togglebutton->active )
    {
        char *psz_vcd_device;

        if( (psz_vcd_device = config_GetPszVariable( "vcd" )) )
            gtk_entry_set_text(
                GTK_ENTRY( lookup_widget( GTK_WIDGET(togglebutton),
                                          "disc_name" ) ), psz_vcd_device );
        if( psz_vcd_device ) free( psz_vcd_device );
    }
}

void GtkDiscOpenOk( GtkButton * button, gpointer user_data )
{
    intf_thread_t * p_intf = GetIntf( GTK_WIDGET(button), "intf_disc" );
    GtkCList *      p_playlist_clist;
    char *          psz_device, *psz_source, *psz_method;
    int             i_end = p_main->p_playlist->i_size;
    int             i_title, i_chapter;

    gtk_widget_hide( p_intf->p_sys->p_disc );
    psz_device = gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                         GTK_WIDGET(button), "disc_name" ) ) );

    /* Check which method was activated */
    if( GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET(button),
                                          "disc_dvd" ) )->active )
    {
        psz_method = "dvd";
    }
    else if( GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET(button),
                                               "disc_vcd" ) )->active )
    {
        psz_method = "vcd";
    }
    else
    {
        intf_ErrMsg( "intf error: unknown disc type toggle button position" );
        return;
    }
    
    /* Select title and chapter */
    i_title = gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "disc_title" ) ) );

    i_chapter = gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "disc_chapter" ) ) );
    
    /* "dvd:foo" has size 5 + strlen(foo) */
    psz_source = malloc( 3 /* "dvd" */ + 1 /* ":" */
                           + strlen( psz_device ) + 2 /* @, */
                           + 4 /* i_title & i_chapter < 100 */ + 1 /* "\0" */ );
    if( psz_source == NULL )
    {
        return;
    }

    /* Build source name and add it to playlist */
    sprintf( psz_source, "%s:%s@%d,%d",
             psz_method, psz_device, i_title, i_chapter );
    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, psz_source );
    free( psz_source );

    /* catch the GTK CList */
    p_playlist_clist = GTK_CLIST( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_playlist ), "playlist_clist" ) );

    /* update the display */
    GtkRebuildCList( p_playlist_clist, p_main->p_playlist );

    /* stop current item, select added item */
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
}


void GtkDiscOpenCancel( GtkButton * button, gpointer user_data )
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


/*****************************************************************************
 * Network stream callbacks
 *****************************************************************************
 * The following callbacks are related to the network stream manager.
 *****************************************************************************/
gboolean GtkNetworkOpenShow( GtkWidget       *widget,
                             GdkEventButton  *event,
                             gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_network ) )
    {
        char *psz_channel_server;

        p_intf->p_sys->p_network = create_intf_network();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_network ),
                             "p_intf", p_intf );

        gtk_spin_button_set_value( GTK_SPIN_BUTTON( gtk_object_get_data(
            GTK_OBJECT( p_intf->p_sys->p_network ), "network_port" ) ),
            config_GetIntVariable( "server-port" ) );

        psz_channel_server = config_GetPszVariable( "channel-server" );
        if( psz_channel_server )
            gtk_entry_set_text( GTK_ENTRY( gtk_object_get_data(
                GTK_OBJECT( p_intf->p_sys->p_network ), "network_channel" ) ),
                psz_channel_server );
        if( psz_channel_server ) free( psz_channel_server );

        gtk_spin_button_set_value( GTK_SPIN_BUTTON( gtk_object_get_data(
            GTK_OBJECT( p_intf->p_sys->p_network ), "network_channel_port" ) ),
            config_GetIntVariable( "channel-port" ) );

        gtk_toggle_button_set_active( gtk_object_get_data( GTK_OBJECT(
            p_intf->p_sys->p_network ), "network_channel_check" ),
            config_GetIntVariable( "network-channel" ) );
    }

    gtk_widget_show( p_intf->p_sys->p_network );
    gdk_window_raise( p_intf->p_sys->p_network->window );

    return TRUE;
}


void GtkNetworkOpenOk( GtkButton *button, gpointer user_data )
{
    intf_thread_t * p_intf = GetIntf( GTK_WIDGET(button), "intf_network" );
    GtkCList *      p_playlist_clist;
    char *          psz_source, *psz_server, *psz_protocol;
    unsigned int    i_port;
    boolean_t       b_broadcast;
    boolean_t       b_channel;
    int             i_end = p_main->p_playlist->i_size;

    gtk_widget_hide( p_intf->p_sys->p_network );
    psz_server = gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                 GTK_WIDGET(button), "network_server" ) ) );

    /* select added item */
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    /* Check which protocol was activated */
    if( GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET(button),
                                          "network_ts" ) )->active )
    {
        psz_protocol = "udpstream";
    }
    else if( GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET(button),
                                               "network_rtp" ) )->active )
    {
        psz_protocol = "rtp";
    }
    else
    {
        intf_ErrMsg( "intf error: unknown protocol toggle button position" );
        return;
    }

    /* Manage channel server */
    b_channel = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
        lookup_widget( GTK_WIDGET(button), "network_channel_check" ) ) );
    config_PutIntVariable( "network-channel", b_channel );
    if( b_channel )
    {
        char *          psz_channel;
        unsigned int    i_channel_port;

        if( p_main->p_channel == NULL )
        {
            network_ChannelCreate();
        }

        psz_channel = gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                             GTK_WIDGET(button), "network_channel" ) ) );
        i_channel_port = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(
            lookup_widget( GTK_WIDGET(button), "network_channel_port" ) ) );

        config_PutPszVariable( "channel-server", psz_channel );
        if( i_channel_port < 65536 )
        {
            config_PutIntVariable( "channel-port", i_channel_port );
        }

        p_intf->p_sys->b_playing = 1;

    }
    else
    {
        /* Get the port number and make sure it will not
         * overflow 5 characters */
        i_port = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(
                     lookup_widget( GTK_WIDGET(button), "network_port" ) ) );
        if( i_port > 65535 )
        {
            intf_ErrMsg( "intf error: invalid port %i", i_port );
        }

        /* do we have a broadcast address */
        b_broadcast = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
            lookup_widget( GTK_WIDGET(button), "network_broadcast_check" ) ) );
        if( b_broadcast )
        {
            char *  psz_broadcast;
            psz_broadcast = gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                            GTK_WIDGET(button), "network_broadcast" ) ) );
            /* Allocate room for "protocol://server:port" */
            psz_source = malloc( strlen( psz_protocol ) + 3 /* "://" */
                                   + strlen( psz_server ) + 2 /* "@:" */
                                   + 5 /* 0-65535 */
                                   + strlen( psz_broadcast ) + 2 /* "::" */ 
                                   + 1 /* "\0" */ );
            if( psz_source == NULL )
            {
                return;
            }

            /* Build source name and add it to playlist */
            sprintf( psz_source, "%s://%s@:%i/%s", psz_protocol,
                                                  psz_server,
                                                  i_port,
                                                  psz_broadcast );
        }
        else
        {
            /* Allocate room for "protocol://server:port" */
            psz_source = malloc( strlen( psz_protocol ) + 3 /* "://" */
                                   + strlen( psz_server ) + 2 /* "@:" */
                                   + 5 /* 0-65535 */ + 1 /* "\0" */ );
            if( psz_source == NULL )
            {
                return;
            }
           
            /* Build source name and add it to playlist */
            sprintf( psz_source, "%s://%s@:%i",
                     psz_protocol, psz_server, i_port );
        }

        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, psz_source );
        free( psz_source );
        
        /* catch the GTK CList */
        p_playlist_clist = GTK_CLIST( gtk_object_get_data(
            GTK_OBJECT( p_intf->p_sys->p_playlist ), "playlist_clist" ) );
        /* update the display */
        GtkRebuildCList( p_playlist_clist, p_main->p_playlist );

        intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
    }
}

void GtkNetworkOpenCancel( GtkButton * button, gpointer user_data)
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


void GtkNetworkOpenBroadcast( GtkToggleButton * togglebutton,
                              gpointer user_data )
{
    GtkWidget *     p_network;

    p_network = gtk_widget_get_toplevel( GTK_WIDGET (togglebutton) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_broadcast_combo" ),
            gtk_toggle_button_get_active( togglebutton ) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_broadcast" ),
            gtk_toggle_button_get_active( togglebutton ) );
}


void GtkNetworkOpenChannel( GtkToggleButton * togglebutton,
                            gpointer user_data )
{
    GtkWidget *     p_network;
    boolean_t       b_channel;
    boolean_t       b_broadcast;

    p_network = gtk_widget_get_toplevel( GTK_WIDGET (togglebutton) );
    b_channel = gtk_toggle_button_get_active( togglebutton );
    b_broadcast = gtk_toggle_button_get_active( gtk_object_get_data(
                  GTK_OBJECT( p_network ), "network_broadcast_check" ) );
        
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_channel_combo" ), b_channel ) ;

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_channel" ), b_channel );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_channel_port" ), b_channel );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_channel_port_label" ), b_channel );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_server_combo" ), ! b_channel );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_server_label" ), ! b_channel );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_server" ), ! b_channel );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_port_label" ), ! b_channel );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_port" ), ! b_channel );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_broadcast_check" ), ! b_channel );
    
    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_broadcast_combo" ), b_broadcast && ! b_channel );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_broadcast" ), b_broadcast && ! b_channel );
}

/*****************************************************************************
 * Open satellite callbacks
 *****************************************************************************
 * The following callbacks are related to the satellite card manager.
 *****************************************************************************/
gboolean GtkSatOpenShow( GtkWidget       *widget,
                          GdkEventButton  *event,
                          gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_sat ) )
    {
        p_intf->p_sys->p_sat = create_intf_sat();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_sat ),
                             "p_intf", p_intf );
    }

    gtk_widget_show( p_intf->p_sys->p_sat );
    gdk_window_raise( p_intf->p_sys->p_sat->window );

    return TRUE;
}

void GtkSatOpenOk( GtkButton * button, gpointer user_data )
{
    intf_thread_t * p_intf = GetIntf( GTK_WIDGET(button), "intf_sat" );
    GtkCList *      p_playlist_clist;
    char *          psz_source;
    int             i_end = p_main->p_playlist->i_size;
    int             i_freq, i_srate;
    int             i_fec;
    boolean_t       b_pol;

    gtk_widget_hide( p_intf->p_sys->p_sat );

    /* Check which polarization was activated */
    if( GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET( button ),
                                        "sat_pol_vert" ) )->active )
    {
        b_pol = 0;
    }
    else
    {
        b_pol = 1;
    }

    i_fec = strtol( gtk_entry_get_text( GTK_ENTRY( GTK_COMBO( 
                lookup_widget( GTK_WIDGET( button ), "sat_fec" )
                )->entry ) ), NULL, 10 );
        
    /* Select frequency and symbol rate */
    i_freq = gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "sat_freq" ) ) );

    i_srate = gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "sat_srate" ) ) );
    
    psz_source = malloc( 22 );
    if( psz_source == NULL )
    {
        return;
    }

    /* Build source name and add it to playlist */
    sprintf( psz_source, "%s:%d,%d,%d,%d",
             "satellite", i_freq, b_pol, i_fec, i_srate );
    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, psz_source );
    free( psz_source );

    /* catch the GTK CList */
    p_playlist_clist = GTK_CLIST( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_playlist ), "playlist_clist" ) );

    /* update the display */
    GtkRebuildCList( p_playlist_clist, p_main->p_playlist );

    /* stop current item, select added item */
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
}


void GtkSatOpenCancel( GtkButton * button, gpointer user_data )
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

/****************************************************************************
 * Callbacks for menuitem
 ****************************************************************************/
void GtkFileOpenActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkFileOpenShow( GTK_WIDGET( menuitem ), NULL, user_data );
}


void GtkDiscOpenActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkDiscOpenShow( GTK_WIDGET( menuitem ), NULL, user_data );
}


void GtkNetworkOpenActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkNetworkOpenShow( GTK_WIDGET( menuitem ), NULL, user_data );
}

