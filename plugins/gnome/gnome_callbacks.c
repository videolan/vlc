/*****************************************************************************
 * gnome_callbacks.c : Callbacks for the Gnome plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include <gnome.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_plst.h"

#include "gnome_sys.h"
#include "gnome_callbacks.h"
#include "gnome_interface.h"
#include "gnome_support.h"

#include "main.h"

/*****************************************************************************
 * Inline function to retrieve the interface structure
 *****************************************************************************/
static __inline__ intf_thread_t * GetIntf( GtkWidget *item, char * psz_parent )
{
    return( gtk_object_get_data( GTK_OBJECT( lookup_widget(item, psz_parent) ),
                                 "p_intf" ) );
}

/*****************************************************************************
 * Callbacks
 ******************************************************************************/
void
on_menubar_open_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    /* If we have never used the file selector, open it */
    if( p_intf->p_sys->p_fileopen == NULL)
    {
        p_intf->p_sys->p_fileopen = create_intf_fileopen();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_fileopen ),
                             "p_intf", p_intf );
    }

    gtk_widget_show( p_intf->p_sys->p_fileopen );
    gdk_window_raise( p_intf->p_sys->p_fileopen->window );
}


void
on_menubar_exit_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    p_intf->b_die = 1;
}


void
on_menubar_playlist_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_playlist ) )
    {
        p_intf->p_sys->p_playlist = create_intf_playlist();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_playlist ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_playlist );
}


void
on_menubar_plugins_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_menubar_preferences_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_menubar_about_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_about ) )
    {
        p_intf->p_sys->p_about = create_intf_about();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_about ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_about );
}


void
on_toolbar_open_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    /* If we have never used the file selector, open it */
    if( p_intf->p_sys->p_fileopen == NULL)
    {
        p_intf->p_sys->p_fileopen = create_intf_fileopen();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_fileopen ),
                             "p_intf", p_intf );
    }

    gtk_widget_show( p_intf->p_sys->p_fileopen );
    gdk_window_raise( p_intf->p_sys->p_fileopen->window );
}


void
on_toolbar_back_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_toolbar_stop_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_toolbar_play_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        input_SetRate( p_intf->p_input, INPUT_RATE_PLAY );
    }
}


void
on_toolbar_pause_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        input_SetRate( p_intf->p_input, INPUT_RATE_PAUSE );
    }
}


void
on_toolbar_playlist_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_playlist ) )
    {
        p_intf->p_sys->p_playlist = create_intf_playlist();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_playlist ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_playlist );
}


void
on_toolbar_prev_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        /* FIXME: temporary hack */
        intf_PlstPrev( p_main->p_playlist );
        intf_PlstPrev( p_main->p_playlist );
        p_intf->p_input->b_eof = 1;
    }
}


void
on_toolbar_next_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        /* FIXME: temporary hack */
        p_intf->p_input->b_eof = 1;
    }
}


void
on_popup_play_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( p_intf->p_input != NULL )
    {
        input_SetRate( p_intf->p_input, INPUT_RATE_PLAY );
    }
}


void
on_popup_pause_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( p_intf->p_input != NULL )
    {
        input_SetRate( p_intf->p_input, INPUT_RATE_PAUSE );
    }
}


void
on_popup_exit_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    p_intf->b_die = 1;
}


void
on_intf_window_destroy                 (GtkObject       *object,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(object), "intf_window" );

    /* FIXME don't destroy the window, just hide it */
    p_intf->b_die = 1;
    p_intf->p_sys->p_window = NULL;
}


void
on_fileopen_ok_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkWidget *filesel;
    gchar *filename;

    filesel = gtk_widget_get_toplevel (GTK_WIDGET (button));
    gtk_widget_hide (filesel);
    filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel));

    intf_PlstAdd( p_main->p_playlist, PLAYLIST_END, (char*)filename );
}


void
on_fileopen_cancel_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


void
on_intf_fileopen_destroy               (GtkObject       *object,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(object), "intf_fileopen" );

    p_intf->p_sys->p_fileopen = NULL;
}


void
on_popup_open_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    /* If we have never used the file selector, open it */
    if( p_intf->p_sys->p_fileopen == NULL)
    {
        p_intf->p_sys->p_fileopen = create_intf_fileopen();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_fileopen ),
                             "p_intf", p_intf );
    }

    gtk_widget_show( p_intf->p_sys->p_fileopen );
    gdk_window_raise( p_intf->p_sys->p_fileopen->window );
}


void
on_popup_about_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_about ) )
    {
        p_intf->p_sys->p_about = create_intf_about();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_about ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_about );
}


void
on_intf_playlist_destroy               (GtkObject       *object,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(object), "intf_playlist" );

    p_intf->p_sys->p_playlist = NULL;
}


void
on_playlist_close_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


void
on_popup_slow_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( p_intf->p_input != NULL )
    {
        input_SetRate( p_intf->p_input, INPUT_RATE_SLOWER );
    }
}


void
on_popup_fast_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( p_intf->p_input != NULL )
    {
        input_SetRate( p_intf->p_input, INPUT_RATE_FASTER );
    }
}


void
on_toolbar_slow_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        input_SetRate( p_intf->p_input, INPUT_RATE_SLOWER );
    }
}


void
on_toolbar_fast_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        input_SetRate( p_intf->p_input, INPUT_RATE_FASTER );
    }
}


gboolean
on_hscale_button_release_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), "intf_window" );

    GtkAdjustment *p_adj = gtk_range_get_adjustment( GTK_RANGE(widget) );
    off_t i_seek = (p_adj->value * p_intf->p_input->stream.i_size) / 100;

    vlc_mutex_lock( &p_intf->p_sys->change_lock );

    p_intf->p_sys->b_scale_isfree = 1;
    if( p_intf->p_input != NULL )
    {
        input_Seek( p_intf->p_input, i_seek );
    }

    vlc_mutex_unlock( &p_intf->p_sys->change_lock );

    return FALSE;
}


gboolean
on_hscale_button_press_event           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), "intf_window" );

    vlc_mutex_lock( &p_intf->p_sys->change_lock );
    p_intf->p_sys->b_scale_isfree = 0;
    vlc_mutex_unlock( &p_intf->p_sys->change_lock );

    return FALSE;
}


