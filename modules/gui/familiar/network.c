/*****************************************************************************
 * network.c : Network interface of the gtk-familiar plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: network.c,v 1.1 2003/03/13 15:50:17 marcari Exp $
 *
 * Authors: Marc Ariberti <marcari@videolan.org>
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
#include <vlc/vout.h>

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "familiar.h"

static void update_network_multicast(GtkWidget * widget);

static void update_network_multicast(GtkWidget * widget)
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );
    GtkToggleButton * p_network_multicast = 
        GTK_GET( TOGGLE_BUTTON, "network_multicast" );
    GtkEditable * p_network_multicast_address = 
        GTK_GET( EDITABLE, "network_multicast_address" );
    GtkEditable * p_network_multicast_port = 
        GTK_GET( EDITABLE, "network_multicast_port" );
        
    if (gtk_toggle_button_get_active(p_network_multicast))
    {
        gchar * str = g_strconcat( "udp://@",
            gtk_editable_get_chars(p_network_multicast_address, 0, -1), ":",
            gtk_editable_get_chars(p_network_multicast_port, 0, -1), NULL );
        gtk_entry_set_text(p_intf->p_sys->p_mrlentry, str);
        g_free( str );
    }
}


void
on_network_multicast_toggled           (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
    update_network_multicast(GTK_WIDGET(togglebutton));
}


void
on_network_multicast_port_changed      (GtkEditable     *editable,
                                        gpointer         user_data)
{
    update_network_multicast(GTK_WIDGET(editable));
}


void
on_network_multicast_address_changed   (GtkEditable     *editable,
                                        gpointer         user_data)
{
    update_network_multicast(GTK_WIDGET(editable));
}


void
on_network_http_toggled                (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( togglebutton );

    if (gtk_toggle_button_get_active(togglebutton))
    {
        gtk_entry_set_text(p_intf->p_sys->p_mrlentry, "http://");
    }
}


void
on_network_ftp_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( togglebutton );
    
    if (gtk_toggle_button_get_active(togglebutton))
    {
        gtk_entry_set_text(p_intf->p_sys->p_mrlentry, "ftp://");
    }
}


void
on_network_mms_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( togglebutton );
    
    if (gtk_toggle_button_get_active(togglebutton))
    {
        gtk_entry_set_text(p_intf->p_sys->p_mrlentry, "mms://");
    }
}

