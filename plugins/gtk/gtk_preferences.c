/*****************************************************************************
 * gtk_control.c : functions to handle stream control buttons.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_preferences.c,v 1.2 2001/05/15 14:49:48 stef Exp $
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

#define MODULE_NAME gtk
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#include <gtk/gtk.h>

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

/****************************************************************************
 * GtkPreferencesShow: display interface window after initialization
 * if necessary
 ****************************************************************************/

/* macros to create preference box */
#define ASSIGN_PSZ_ENTRY( var, default, name )                               \
    gtk_entry_set_text( GTK_ENTRY( gtk_object_get_data( GTK_OBJECT(          \
        p_intf->p_sys->p_preferences ), name ) ),                            \
                        main_GetPszVariable( var, default ) )

#define ASSIGN_INT_VALUE( var, default, name )                                        \
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( gtk_object_get_data(         \
        GTK_OBJECT( p_intf->p_sys->p_preferences ), name ) ),                \
                               main_GetIntVariable( var, default ) )

#define ASSIGN_INT_TOGGLE( var, default, name )                                       \
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( gtk_object_get_data(    \
        GTK_OBJECT( p_intf->p_sys->p_preferences ), name ) ),                \
                               main_GetIntVariable( var, default ) )

gboolean GtkPreferencesShow( GtkWidget       *widget,
                             GdkEventButton  *event,
                             gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    /* If we have never used the file selector, open it */
    if( !GTK_IS_WIDGET( p_intf->p_sys->p_preferences ) )
    {
        p_intf->p_sys->p_preferences = create_intf_preferences();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_preferences ),
                             "p_intf", p_intf );

        /* Default path */
        ASSIGN_PSZ_ENTRY( INTF_PATH_VAR, INTF_PATH_DEFAULT,
                          "preferences_file_path_entry" );
    
        /* Default DVD */
        ASSIGN_PSZ_ENTRY( INPUT_DVD_DEVICE_VAR,INPUT_DVD_DEVICE_DEFAULT,
                          "preferences_disc_dvd_entry" );
    
        /* Default VCD */
        ASSIGN_PSZ_ENTRY( INPUT_VCD_DEVICE_VAR, INPUT_VCD_DEVICE_DEFAULT,
                          "preferences_disc_vcd_entry" );
    
        /* Default server */
        ASSIGN_PSZ_ENTRY( INPUT_SERVER_VAR, INPUT_SERVER_DEFAULT,
                          "preferences_network_server_entry" );
    
        /* Default port */
        ASSIGN_INT_VALUE( INPUT_PORT_VAR, INPUT_PORT_DEFAULT,
                          "preferences_network_port_spinbutton" );
    
        /* Broadcast address */
        ASSIGN_PSZ_ENTRY( INPUT_BCAST_ADRR_VAR, INPUT_BCAST_ADDR_DEFAULT,
                          "preferences_network_broadcast_entry" );
    
        /* Broadcast stream by default ? */
        ASSIGN_INT_TOGGLE( INPUT_BROADCAST_VAR, INPUT_BROADCAST_DEFAULT,
                           "preferences_network_broadcast_checkbutton" );
    
        /* XXX Protocol */
    
        /* Default interface */
        ASSIGN_PSZ_ENTRY( INTF_METHOD_VAR, INTF_METHOD_DEFAULT,
                          "preferences_interface_entry" );
    
        /* Default video output */
        ASSIGN_PSZ_ENTRY( VOUT_METHOD_VAR, VOUT_METHOD_DEFAULT,
                          "preferences_video_output_entry" );
    
        /* Default output width */
        ASSIGN_INT_VALUE( VOUT_WIDTH_VAR, VOUT_WIDTH_DEFAULT,
                          "preferences_video_width_spinbutton" );
    
        /* Default output height */
        ASSIGN_INT_VALUE( VOUT_HEIGHT_VAR, VOUT_HEIGHT_DEFAULT,
                          "preferences_video_height_spinbutton" );
    
        /* XXX Default screen depth */
    
        /* XXX Default fullscreen depth */
    
        /* XXX Default gamma */
        
        /* Fullscreen on play */
        ASSIGN_INT_TOGGLE( VOUT_FULLSCREEN_VAR, VOUT_FULLSCREEN_DEFAULT,
                           "preferences_video_fullscreen_checkbutton" );
    
        /* Grayscale display */
        ASSIGN_INT_TOGGLE( VOUT_GRAYSCALE_VAR, VOUT_GRAYSCALE_DEFAULT,
                           "preferences_video_grayscale_checkbutton" );
    
        /* Default audio output */
        ASSIGN_PSZ_ENTRY( AOUT_METHOD_VAR, AOUT_METHOD_DEFAULT,
                          "preferences_audio_output_entry" );
    
        /* Default audio device */
        ASSIGN_PSZ_ENTRY( AOUT_DSP_VAR, AOUT_DSP_DEFAULT,
                          "preferences_audio_device_entry" );
    
        /* XXX Default frequency */
    
        /* XXX Default quality */
    
        /* XXX Default number of channels */
    
        /* Use spdif output ? */
        ASSIGN_INT_TOGGLE( AOUT_SPDIF_VAR, AOUT_SPDIF_DEFAULT,
                           "preferences_audio_spdif_checkbutton" );
    
        /* Launch playlist on startup */
        ASSIGN_INT_TOGGLE( PLAYLIST_STARTUP_VAR, PLAYLIST_STARTUP_DEFAULT,
                        "preferences_playlist_startup_checkbutton" );
    
        /* Enqueue drag'n dropped item as default */
        ASSIGN_INT_TOGGLE( PLAYLIST_ENQUEUE_VAR, PLAYLIST_ENQUEUE_DEFAULT,
                           "preferences_playlist_enqueue_checkbutton" );
    
        /* Loop on playlist end */
        ASSIGN_INT_TOGGLE( PLAYLIST_LOOP_VAR, PLAYLIST_LOOP_DEFAULT,
                           "preferences_playlist_loop_checkbutton" );
    
        /* Verbosity of warning messages */
        ASSIGN_INT_VALUE( INTF_WARNING_VAR, INTF_WARNING_DEFAULT,
                          "preferences_misc_messages_spinbutton" );
#undef ASSIGN_PSZ_ENTRY
#undef ASSIGN_INT_VALUE
#undef ASSIGN_INT_TOGGLE
    }

    gtk_widget_show( p_intf->p_sys->p_preferences );
    gdk_window_raise( p_intf->p_sys->p_preferences->window );

    return TRUE;
}

/****************************************************************************
 * GtkPreferencesApply: store the values into the environnement variables
 ****************************************************************************/

/* macros to read value frfom preference box */
#define ASSIGN_PSZ_ENTRY( var, name )                                        \
    main_PutPszVariable( var, gtk_entry_get_text(                            \
    GTK_ENTRY( gtk_object_get_data( GTK_OBJECT( p_preferences ), name ) ) ) )

#define ASSIGN_INT_VALUE( var, name )                                        \
    main_PutIntVariable( var, gtk_spin_button_get_value_as_int(              \
        GTK_SPIN_BUTTON( gtk_object_get_data( GTK_OBJECT( p_preferences ),   \
        name ) ) ) )

#define ASSIGN_INT_TOGGLE( var, name )                                       \
    main_PutIntVariable( var, gtk_toggle_button_get_active(                  \
        GTK_TOGGLE_BUTTON( gtk_object_get_data( GTK_OBJECT( p_preferences ), \
        name ) ) ) )

void GtkPreferencesApply( GtkButton * button, gpointer user_data )
{
    GtkWidget *     p_preferences;

    /* get preferences window */
    p_preferences = gtk_widget_get_toplevel( GTK_WIDGET( button ) );

    /* Default path */
    ASSIGN_PSZ_ENTRY( INTF_PATH_VAR, "preferences_file_path_entry" );

    /* Default DVD */
    ASSIGN_PSZ_ENTRY( INPUT_DVD_DEVICE_VAR, "preferences_disc_dvd_entry" );

    /* Default VCD */
    ASSIGN_PSZ_ENTRY( INPUT_VCD_DEVICE_VAR, "preferences_disc_vcd_entry" );

    /* Default server */
    ASSIGN_PSZ_ENTRY( INPUT_SERVER_VAR, "preferences_network_server_entry" );

    /* Default port */
    ASSIGN_INT_VALUE( INPUT_PORT_VAR, "preferences_network_port_spinbutton" );

    /* Broadcast address */
    ASSIGN_PSZ_ENTRY( INPUT_BCAST_ADRR_VAR,
                      "preferences_network_broadcast_entry" );

    /* Broadcast stream by default ? */
    ASSIGN_INT_TOGGLE( INPUT_BROADCAST_VAR,
                       "preferences_network_broadcast_checkbutton" );

    /* XXX Protocol */

    /* Default interface */
    ASSIGN_PSZ_ENTRY( INTF_METHOD_VAR, "preferences_interface_entry" );

    /* Default video output */
    ASSIGN_PSZ_ENTRY( VOUT_METHOD_VAR, "preferences_video_output_entry" );

    /* Default output width */
    ASSIGN_INT_VALUE( VOUT_WIDTH_VAR, "preferences_video_width_spinbutton" );

    /* Default output height */
    ASSIGN_INT_VALUE( VOUT_HEIGHT_VAR, "preferences_video_height_spinbutton" );

    /* XXX Default screen depth */

    /* XXX Default fullscreen depth */

    /* XXX Default gamma */
    
    /* Fullscreen on play */
    ASSIGN_INT_TOGGLE( VOUT_FULLSCREEN_VAR,
                       "preferences_video_fullscreen_checkbutton" );

    /* Grayscale display */
    ASSIGN_INT_TOGGLE( VOUT_GRAYSCALE_VAR,
                       "preferences_video_grayscale_checkbutton" );

    /* Default audio output */
    ASSIGN_PSZ_ENTRY( AOUT_METHOD_VAR, "preferences_audio_output_entry" );

    /* Default audio device */
    ASSIGN_PSZ_ENTRY( AOUT_DSP_VAR, "preferences_audio_device_entry" );

    /* XXX Default frequency */

    /* XXX Default quality */

    /* XXX Default number of channels */

    /* Use spdif output ? */
    ASSIGN_INT_TOGGLE( AOUT_SPDIF_VAR, "preferences_audio_spdif_checkbutton" );

    /* Launch playlist on startup */
    ASSIGN_INT_TOGGLE( PLAYLIST_STARTUP_VAR,
                       "preferences_playlist_startup_checkbutton" );

    /* Enqueue drag'n dropped item as default */
    ASSIGN_INT_TOGGLE( PLAYLIST_ENQUEUE_VAR,
                       "preferences_playlist_enqueue_checkbutton" );

    /* Loop on playlist end */
    ASSIGN_INT_TOGGLE( PLAYLIST_LOOP_VAR,
                       "preferences_playlist_loop_checkbutton" );

    /* Verbosity of warning messages */
    ASSIGN_INT_VALUE( INTF_WARNING_VAR,
                      "preferences_misc_messages_spinbutton" );
}
#undef ASSIGN_PSZ_ENTRY
#undef ASSIGN_INT_VALUE
#undef ASSIGN_INT_TOGGLE


void GtkPreferencesOk( GtkButton * button, gpointer user_data )
{
    GtkPreferencesApply( button, user_data );
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


void GtkPreferencesCancel( GtkButton * button, gpointer user_data )
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

/****************************************************************************
 * Callbacks for menuitems
 ****************************************************************************/
void GtkPreferencesActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkPreferencesShow( GTK_WIDGET( menuitem ), NULL, user_data );
}
