/*****************************************************************************
 * gtk_callbacks.c : Callbacks for the Gtk+ plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
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
#include "intf_plst.h"
#include "intf_msg.h"

#include "gtk_sys.h"
#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"

#include "main.h"

/****************************************************************************
 * External function
 */
void on_generic_drop_data_received( intf_thread_t * p_intf,
                        GtkSelectionData *data, guint info, int position);



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
    gdk_window_raise( p_intf->p_sys->p_about->window );
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
        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
    }
}


void
on_toolbar_pause_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_PAUSE );
    }
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
        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
    }
}


void
on_popup_pause_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_PAUSE );
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
    gdk_window_raise( p_intf->p_sys->p_about->window );
}
void
on_popup_slow_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_SLOWER );
    }
}


void
on_popup_fast_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_FASTER );
    }
}


void
on_toolbar_slow_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_SLOWER );
    }
}


void
on_toolbar_fast_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_FASTER );
    }
}


gboolean
on_hscale_button_release_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), "intf_window" );

    GtkAdjustment *p_adj = gtk_range_get_adjustment( GTK_RANGE(widget) );
    off_t i_seek;

    vlc_mutex_lock( &p_intf->p_sys->change_lock );

    if( p_intf->p_input != NULL )
    {
        i_seek = (p_adj->value *
                  p_intf->p_input->stream.p_selected_area->i_size) / 100;
        input_Seek( p_intf->p_input, i_seek );
    }
    p_intf->p_sys->b_scale_isfree = 1;

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



void
on_intf_modules_destroy                (GtkObject       *object,
                                        gpointer         user_data)
{

}


void
on_modules_ok_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_modules" );

    gtk_widget_hide( p_intf->p_sys->p_modules );

}


void
on_modules_apply_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_modules_cancel_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_modules" );

    gtk_widget_hide( p_intf->p_sys->p_modules );
}

void
on_menubar_modules_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_modules ) )
    {
//        p_intf->p_sys->p_modules = create_intf_modules();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_modules ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_modules );
    gdk_window_raise( p_intf->p_sys->p_modules->window );
}


void
on_intf_window_drag_data_received      (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf =  GetIntf( GTK_WIDGET(widget), "intf_window" );
    on_generic_drop_data_received( p_intf, data, info, 0);
     if( p_intf->p_input != NULL )
     {
        /* FIXME: temporary hack */
        p_intf->p_input->b_eof = 1;
     }
     
    intf_PlstJumpto( p_main->p_playlist, -1 );

}


void
on_about_ok_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_about" );

    gtk_widget_hide( p_intf->p_sys->p_about );
}


void
on_menubar_disc_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_disc ) )
    {
        p_intf->p_sys->p_disc = create_intf_disc();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_disc ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_disc );
    gdk_window_raise( p_intf->p_sys->p_disc->window );
}


void
on_toolbar_disc_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_disc ) )
    {
        p_intf->p_sys->p_disc = create_intf_disc();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_disc ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_disc );
    gdk_window_raise( p_intf->p_sys->p_disc->window );
}


void
on_disc_ok_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_disc" );
    char *psz_device, *psz_source, *psz_method;

    psz_device = gtk_entry_get_text( GTK_ENTRY( lookup_widget(
                                         GTK_WIDGET(button), "disc_name" ) ) );

    /* "dvd:foo" has size 5 + strlen(foo) */
    psz_source = malloc( 5 + strlen( psz_device ) );
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
        intf_ErrMsg( "intf error: unknown toggle button configuration" );
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
    intf_PlstAdd( p_main->p_playlist, PLAYLIST_END, psz_source );

    gtk_widget_hide( p_intf->p_sys->p_disc );
}


void
on_disc_cancel_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_disc" );

    gtk_widget_hide( p_intf->p_sys->p_disc );
}


void
on_disc_dvd_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
    if( togglebutton->active )
    {
        gtk_entry_set_text( GTK_ENTRY( lookup_widget(
            GTK_WIDGET(togglebutton), "disc_name" ) ), "/dev/dvd" );
    }
}


void
on_disc_vcd_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
    if( togglebutton->active )
    {
        gtk_entry_set_text( GTK_ENTRY( lookup_widget(
            GTK_WIDGET(togglebutton), "disc_name" ) ), "/dev/cdrom" );
    }
}


void
on_popup_disc_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_disc ) )
    {
        p_intf->p_sys->p_disc = create_intf_disc();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_disc ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_disc );
    gdk_window_raise( p_intf->p_sys->p_disc->window );
}

void
on_popup_audio_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );
    es_descriptor_t *       p_es;

    p_es = (es_descriptor_t*)user_data;

    input_ChangeES( p_intf->p_input, p_es, 1 );
}


void
on_popup_subpictures_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );
    es_descriptor_t *       p_es;

    p_es = (es_descriptor_t*)user_data;

    input_ChangeES( p_intf->p_input, p_es, 2 );
}


void
on_menubar_audio_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );
    es_descriptor_t *       p_es;

    p_es = (es_descriptor_t*)user_data;

    input_ChangeES( p_intf->p_input, p_es, 1 );
}


void
on_menubar_subpictures_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );
    es_descriptor_t *       p_es;

    p_es = (es_descriptor_t*)user_data;

    input_ChangeES( p_intf->p_input, p_es, 2 );
}


void
on_popup_navigation_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf    = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );
    input_area_t *  p_area;
    gint            i_title;
    gint            i_chapter;

    i_title   = (gint)(user_data) / 100 ;
    i_chapter = (gint)(user_data) - ( 100 * i_title );
    p_area    = p_intf->p_input->stream.pp_areas[i_title];
    p_area->i_part = i_chapter;

    p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );
    input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
}


void
on_menubar_title_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)user_data );
    p_intf->p_sys->b_menus_update = 1;
    input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
}


void
on_menubar_chapter_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf    = GetIntf( GTK_WIDGET(menuitem), "intf_window" );
    input_area_t *  p_area    = p_intf->p_input->stream.p_selected_area;
    gint            i_chapter = (gint)user_data;

    p_area->i_part = i_chapter;

    p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );
    input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
}

gboolean
on_intf_window_destroy                 (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget),  "intf_window" );
    /* is there an output thread ? */
    if(p_main->b_video == 1)
    {
        gtk_widget_hide(widget);
    } else {
        p_intf->b_die = 1;
        gtk_widget_destroy(widget);
    }
  return TRUE;
}


void
on_main_window_toggle                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem),  "intf_popup" );
    
    if( GTK_WIDGET_VISIBLE(p_intf->p_sys->p_window) ) {
        gtk_widget_hide( p_intf->p_sys->p_window);

    } else {
        gtk_widget_show( p_intf->p_sys->p_window );
    }
}

