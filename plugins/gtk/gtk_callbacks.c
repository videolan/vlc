/*****************************************************************************
 * gtk_callbacks.c : Callbacks for the Gtk+ plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_callbacks.c,v 1.17 2001/05/15 01:01:44 stef Exp $
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

#include "video.h"
#include "video_output.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "intf_gtk.h"

#include "main.h"

/*****************************************************************************
 * Callbacks
 *****************************************************************************/

/*
 * Main interface callbacks
 */

gboolean
GtkExit                                (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->b_die = 1;
    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}

gboolean
GtkWindowDelete                        (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    GtkExit( GTK_WIDGET( widget ), NULL, user_data );

    return TRUE;
}


gboolean
GtkWindowToggle                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );
    
    if( GTK_WIDGET_VISIBLE(p_intf->p_sys->p_window) )
    {
        gtk_widget_hide( p_intf->p_sys->p_window);

    } 
    else 
    {
        gtk_widget_show( p_intf->p_sys->p_window );
    }

    return TRUE;
}

gboolean
GtkFullscreen                          (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    if( p_vout_bank->i_count )
    {
        vlc_mutex_lock( &p_vout_bank->pp_vout[0]->change_lock );
        if( p_vout_bank->pp_vout[0]->b_fullscreen ==
                !GTK_CHECK_MENU_ITEM( widget )->active )
            p_vout_bank->pp_vout[0]->i_changes |= VOUT_FULLSCREEN_CHANGE;
        vlc_mutex_unlock( &p_vout_bank->pp_vout[0]->change_lock );
    }

    return TRUE;
}

void
GtkWindowDrag                          (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf =  GetIntf( GTK_WIDGET(widget), "intf_window" );
    int end = p_main->p_playlist->i_size;
    GtkDropDataReceived( p_intf, data, info, PLAYLIST_END);

    if( p_intf->p_input != NULL )
    {
       /* FIXME: temporary hack */
       p_intf->p_input->b_eof = 1;
    }
     
    intf_PlaylistJumpto( p_main->p_playlist, end-1 );
    p_main->p_playlist->b_stopped = 0;
}



gboolean
GtkSliderRelease                       (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), "intf_window" );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 1;
    vlc_mutex_unlock( &p_intf->change_lock );

    return FALSE;
}


gboolean
GtkSliderPress                         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), "intf_window" );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 0;
    vlc_mutex_unlock( &p_intf->change_lock );

    return FALSE;
}


void
GtkTitlePrev                           (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;
    int             i_id;

    p_intf = GetIntf( GTK_WIDGET(button), (char*)user_data );
    i_id = p_intf->p_input->stream.p_selected_area->i_id - 1;

    /* Disallow area 0 since it is used for video_ts.vob */
    if( i_id > 0 )
    {
        p_area = p_intf->p_input->stream.pp_areas[i_id];
        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
    }
}


void
GtkTitleNext                           (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;
    int             i_id;

    p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );
    i_id = p_intf->p_input->stream.p_selected_area->i_id + 1;

    if( i_id < p_intf->p_input->stream.i_area_nb )
    {
        p_area = p_intf->p_input->stream.pp_areas[i_id];   
        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
    }

}


void
GtkChapterPrev                         (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;

    p_intf = GetIntf( GTK_WIDGET(button), (char*)user_data );
    p_area = p_intf->p_input->stream.p_selected_area;

    if( p_area->i_part > 0 )
    {
        p_area->i_part--;
        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
    }
}


void
GtkChapterNext                         (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;

    p_intf = GetIntf( GTK_WIDGET(button), (char*)user_data );
    p_area = p_intf->p_input->stream.p_selected_area;

    if( p_area->i_part < p_area->i_part_nb )
    {
        p_area->i_part++;
        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
    }
}

/****************************************************************************
 * About box
 ****************************************************************************/

gboolean
GtkAboutShow                           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_about ) )
    {
        p_intf->p_sys->p_about = create_intf_about();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_about ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_about );
    gdk_window_raise( p_intf->p_sys->p_about->window );

    return TRUE;
}

void GtkAboutOk( GtkButton * button, gpointer user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), (char*)user_data );

    gtk_widget_hide( p_intf->p_sys->p_about );
}


/****************************************************************************
 * Jump box
 ****************************************************************************/

gboolean
GtkJumpShow                            (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_jump ) )
    {
        p_intf->p_sys->p_jump = create_intf_jump();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_jump ),
                             "p_intf", p_intf );
    }

    gtk_widget_show( p_intf->p_sys->p_jump );
    gdk_window_raise( p_intf->p_sys->p_jump->window );

    return FALSE;
}


void
GtkJumpOk                              (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf;
    off_t           i_seek;
    off_t           i_size;
    int             i_hours;
    int             i_minutes;
    int             i_seconds;

    p_intf = GetIntf( GTK_WIDGET( button ), (char*)user_data );

#define GET_VALUE( name )                                                   \
    gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( gtk_object_get_data( \
        GTK_OBJECT( p_intf->p_sys->p_jump ), name ) ) )

    i_hours   = GET_VALUE( "jump_hour_spinbutton" );
    i_minutes = GET_VALUE( "jump_minute_spinbutton" );
    i_seconds = GET_VALUE( "jump_second_spinbutton" );

#undef GET_VALUE

    i_seconds += 60 *i_minutes + 3600* i_hours;

    vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
    i_seek = i_seconds * 50 * p_intf->p_input->stream.i_mux_rate;
    i_size = p_intf->p_input->stream.p_selected_area->i_size;
    vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );

    if( i_seek < i_size )
    {
        input_Seek( p_intf->p_input, i_seek );
    }
    p_main->p_playlist->b_stopped = 0;
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


void
GtkJumpCancel                          (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


void
GtkFileOpenActivate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkFileOpenShow( GTK_WIDGET( menuitem ), NULL, user_data );
}


void
GtkDiscOpenActivate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkDiscOpenShow( GTK_WIDGET( menuitem ), NULL, user_data );
}


void
GtkNetworkOpenActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkNetworkOpenShow( GTK_WIDGET( menuitem ), NULL, user_data );

}


void
GtkExitActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkExit( GTK_WIDGET( menuitem ), NULL, user_data );
}


void
GtkWindowToggleActivate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkWindowToggle( GTK_WIDGET( menuitem ), NULL, user_data );
}


void
GtkFullscreenActivate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkFullscreen( GTK_WIDGET( menuitem ), NULL, user_data );
}


void
GtkPlaylistActivate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkPlaylistShow( GTK_WIDGET( menuitem ), NULL, user_data );
}


void
GtkModulesActivate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
GtkPreferencesActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkPreferencesShow( GTK_WIDGET( menuitem ), NULL, user_data );
}


void
GtkAboutActivate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkAboutShow( GTK_WIDGET( menuitem ), NULL, user_data );
}



void
GtkJumpActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkJumpShow( GTK_WIDGET( menuitem ), NULL, user_data );
}



