/*****************************************************************************
 * gtk_open.c : functions to handle file/disc/network open widgets.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_open.c,v 1.7 2001/10/10 14:25:15 sam Exp $
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
#include "defs.h"
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#define gtk 12
#define gnome 42
#if ( MODULE_NAME == gtk )
#   include <gtk/gtk.h>
#elif ( MODULE_NAME == gnome )
#   include <gnome.h>
#endif
#undef gtk
#undef gnome

#include <string.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"
#include "intf_msg.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_playlist.h"
#include "intf_gtk.h"

#include "main.h"
#include "netutils.h"

#include "modules_export.h"

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
        p_intf->p_sys->p_fileopen = create_intf_fileopen();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_fileopen ),
                             "p_intf", p_intf );

        gtk_file_selection_set_filename( GTK_FILE_SELECTION(
            p_intf->p_sys->p_fileopen ),
            main_GetPszVariable( INTF_PATH_VAR, INTF_PATH_DEFAULT ) );
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
    if( p_intf->p_input != NULL )
    {
        p_intf->p_input->b_eof = 1;
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
        gtk_entry_set_text(
          GTK_ENTRY( lookup_widget( GTK_WIDGET(togglebutton), "disc_name" ) ),
          main_GetPszVariable( INPUT_DVD_DEVICE_VAR, INPUT_DVD_DEVICE_DEFAULT )
        );
    }
}

void GtkDiscOpenVcd( GtkToggleButton * togglebutton, gpointer user_data )
{
    if( togglebutton->active )
    {
        gtk_entry_set_text(
          GTK_ENTRY( lookup_widget( GTK_WIDGET(togglebutton), "disc_name" ) ),
          main_GetPszVariable( INPUT_VCD_DEVICE_VAR, INPUT_VCD_DEVICE_DEFAULT )
        );
    }
}

void GtkDiscOpenOk( GtkButton * button, gpointer user_data )
{
    intf_thread_t * p_intf = GetIntf( GTK_WIDGET(button), "intf_disc" );
    GtkCList *      p_playlist_clist;
    char *          psz_device, *psz_source, *psz_method;
    int             i_end = p_main->p_playlist->i_size;

    gtk_widget_hide( p_intf->p_sys->p_disc );
    psz_device = gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                         GTK_WIDGET(button), "disc_name" ) ) );

    /* "dvd:foo" has size 5 + strlen(foo) */
    psz_source = malloc( 3 /* "dvd" */ + 1 /* ":" */
                           + strlen( psz_device ) + 1 /* "\0" */ );
    if( psz_source == NULL )
    {
        return;
    }

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
        free( psz_source );
        return;
    }
    
    /* Select title and chapter */
    main_PutIntVariable( INPUT_TITLE_VAR, gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "disc_title" ) ) ) );

    main_PutIntVariable( INPUT_CHAPTER_VAR, gtk_spin_button_get_value_as_int(
                              GTK_SPIN_BUTTON( lookup_widget(
                                  GTK_WIDGET(button), "disc_chapter" ) ) ) );

    /* Build source name and add it to playlist */
    sprintf( psz_source, "%s:%s", psz_method, psz_device );
    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, psz_source );
    free( psz_source );

    /* catch the GTK CList */
    p_playlist_clist = GTK_CLIST( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_playlist ), "playlist_clist" ) );

    /* update the display */
    GtkRebuildCList( p_playlist_clist, p_main->p_playlist );

    /* stop current item, select added item */
    if( p_intf->p_input != NULL )
    {
        p_intf->p_input->b_eof = 1;
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
        p_intf->p_sys->p_network = create_intf_network();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_network ),
                             "p_intf", p_intf );
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

    /* Check which protocol was activated */
    if( GTK_TOGGLE_BUTTON( lookup_widget( GTK_WIDGET(button),
                                          "network_ts" ) )->active )
    {
        psz_protocol = "ts";
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

    /* Get the port number and make sure it will not overflow 5 characters */
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
                               + strlen( psz_server ) + 1 /* ":" */
                               + 5 /* 0-65535 */
                               + strlen( psz_broadcast ) + 2 /* "::" */ 
                               + 1 /* "\0" */ );
        if( psz_source == NULL )
        {
            return;
        }

        /* Build source name and add it to playlist */
        sprintf( psz_source, "%s://%s:%i/%s", psz_protocol,
                                              psz_server,
                                              i_port,
                                              psz_broadcast );
    }
    else
    {
        /* Allocate room for "protocol://server:port" */
        psz_source = malloc( strlen( psz_protocol ) + 3 /* "://" */
                               + strlen( psz_server ) + 1 /* ":" */
                               + 5 /* 0-65535 */ + 1 /* "\0" */ );
        if( psz_source == NULL )
        {
            return;
        }
       
        /* Build source name and add it to playlist */
        sprintf( psz_source, "%s://%s:%i", psz_protocol, psz_server, i_port );
    }

    /* Manage channel server */
    b_channel = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
        lookup_widget( GTK_WIDGET(button), "network_channel_check" ) ) );
    main_PutIntVariable( INPUT_NETWORK_CHANNEL_VAR, b_channel );
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

        main_PutPszVariable( INPUT_CHANNEL_SERVER_VAR, psz_channel );
        if( i_channel_port < 65536 )
        {
            main_PutIntVariable( INPUT_CHANNEL_PORT_VAR, i_channel_port );
        }
    }

    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, psz_source );
    free( psz_source );

    /* catch the GTK CList */
    p_playlist_clist = GTK_CLIST( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_playlist ), "playlist_clist" ) );
    /* update the display */
    GtkRebuildCList( p_playlist_clist, p_main->p_playlist );

    /* select added item */
    if( p_intf->p_input != NULL )
    {
        p_intf->p_input->b_eof = 1;
    }

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
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

    p_network = gtk_widget_get_toplevel( GTK_WIDGET (togglebutton) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_channel_combo" ),
            gtk_toggle_button_get_active( togglebutton ) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_channel" ),
            gtk_toggle_button_get_active( togglebutton ) );

    gtk_widget_set_sensitive( gtk_object_get_data( GTK_OBJECT( p_network ),
            "network_channel_port" ),
            gtk_toggle_button_get_active( togglebutton ) );

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

