/*****************************************************************************
 * familiar_callbacks.c : Callbacks for the Familiar Linux Gtk+ plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: familiar_callbacks.c,v 1.5 2002/08/06 19:12:07 jpsaman Exp $
 *
 * Authors: Jean-Paul Saman <jpsaman@wxs.nl>
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

#include <unistd.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "familiar_callbacks.h"
#include "familiar_interface.h"
#include "familiar_support.h"
#include "familiar.h"

//#include "netutils.h"

static void MediaURLOpenChanged( GtkEditable *editable, gpointer user_data );
static void PreferencesURLOpenChanged( GtkEditable *editable, gpointer user_data );

/*****************************************************************************
 * Useful function to retrieve p_intf
 ****************************************************************************/
void * __GtkGetIntf( GtkWidget * widget )
{
    void *p_data;

    if( GTK_IS_MENU_ITEM( widget ) )
    {
        /* Look for a GTK_MENU */
        while( widget->parent && !GTK_IS_MENU( widget ) )
        {
            widget = widget->parent;
        }

        /* Maybe this one has the data */
        p_data = gtk_object_get_data( GTK_OBJECT( widget ), "p_intf" );
        if( p_data )
        {
            return p_data;
        }

        /* Otherwise, the parent widget has it */
        widget = gtk_menu_get_attach_widget( GTK_MENU( widget ) );
    }

    /* We look for the top widget */
    widget = gtk_widget_get_toplevel( GTK_WIDGET( widget ) );

    p_data = gtk_object_get_data( GTK_OBJECT( widget ), "p_intf" );

    return p_data;
}

/*****************************************************************************
 * Helper functions for URL changes in Media and Preferences notebook pages.
 ****************************************************************************/
static void MediaURLOpenChanged( GtkEditable *editable, gpointer user_data )
{
    intf_thread_t *p_intf = GtkGetIntf( editable );
    playlist_t *p_playlist;
    gchar *       psz_url;

    psz_url = gtk_entry_get_text(GTK_ENTRY(editable));
    g_print( "%s\n",psz_url );
//    p_url = gtk_editable_get_chars(editable,0,-1);

    // Add p_url to playlist .... but how ?
    if (p_intf)
    {
        p_playlist = (playlist_t *)
                 vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if( p_playlist )
        {
           playlist_Add( p_playlist, (char*)psz_url,
                         PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
           vlc_object_release( p_playlist );
        }
    }
}

static void PreferencesURLOpenChanged( GtkEditable *editable, gpointer user_data )
{
    gchar *       p_url;
//    GtkWidget *   item;

    p_url = gtk_entry_get_text(GTK_ENTRY(editable) );
    g_print( "%s\n",p_url );

//    p_url = gtk_editable_get_chars(editable,0,-1);
//    item = gtk_list_item_new();
//    gtk_widget_show (item);
//    gtk_combo_set_item_string (GTK_COMBO (combo), GTK_ITEM (item), p_url);
//    /* Now we simply add the item to the combo's list. */
//    gtk_container_add (GTK_CONTAINER (GTK_COMBO (combo)->list), item);
}


/*
 * Main interface callbacks
 */

gboolean GtkExit( GtkWidget       *widget,
                  gpointer         user_data )
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_vlc->b_die = VLC_TRUE;
    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}

gboolean
on_familiar_destroy_event              (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    GtkExit( GTK_WIDGET( widget ), user_data );
    return TRUE;
}


void
on_toolbar_open_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );
    if (p_intf)
    {
        gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}


void
on_toolbar_preferences_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );
    if (p_intf) {
        gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}


void
on_toolbar_rewind_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf )
    {
        if( p_intf->p_sys->p_input )
        {
            input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
        }
    }
}


void
on_toolbar_pause_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf )
    {
        if( p_intf->p_sys->p_input )
        {
            input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );
        }
    }
}


void
on_toolbar_play_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        if( p_intf )
        {
           gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
           gdk_window_raise( p_intf->p_sys->p_window->window );
        }
        // Display open page
    }

    /* If the playlist is empty, open a file requester instead */
    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
        vlc_object_release( p_playlist );
        gdk_window_lower( p_intf->p_sys->p_window->window );
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        // Display open page
    }
}


void
on_toolbar_stop_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist)
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}


void
on_toolbar_forward_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf )
    {
        if( p_intf->p_sys->p_input )
        {
            input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_FASTER );
        }
    }
}


void
on_toolbar_about_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );
    if (p_intf)
    {// Toggle notebook
        if (p_intf->p_sys->p_notebook)
        {
//        if ( gtk_get_data(  GTK_WIDGET(p_intf->p_sys->p_notebook), "visible" ) )
//           gtk_widget_hide( GTK_WIDGET(p_intf->p_sys->p_notebook) );
//        else
           gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        }
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}


void
on_comboURL_entry_changed              (GtkEditable     *editable,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf = GtkGetIntf( editable );

    if (p_intf)
    {
        MediaURLOpenChanged( editable, NULL );
    }
}


void
on_comboPrefs_entry_changed            (GtkEditable     *editable,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf = GtkGetIntf( editable );

    if (p_intf)
    {
        PreferencesURLOpenChanged( editable, NULL );
    }
}

