/*****************************************************************************
 * gnome_callbacks.c : Callbacks for the Gnome plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gnome_callbacks.c,v 1.24 2001/04/22 00:08:26 stef Exp $
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

#define MODULE_NAME gnome
#include "modules_inner.h"

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
#include "intf_playlist.h"
#include "intf_msg.h"

#include "gnome_callbacks.h"
#include "gnome_interface.h"
#include "gnome_support.h"
#include "intf_gnome.h"

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
 * Interface callbacks
 *****************************************************************************
 * The following callbacks are related to the main interface window.
 *****************************************************************************/
void
on_intf_window_destroy                 (GtkObject       *object,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(object), "intf_window" );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->b_die = 1;
    vlc_mutex_unlock( &p_intf->change_lock );
}


gboolean
on_slider_button_press_event           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), "intf_window" );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 0;
    vlc_mutex_unlock( &p_intf->change_lock );

    return FALSE;
}


gboolean
on_slider_button_release_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), "intf_window" );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 1;
    vlc_mutex_unlock( &p_intf->change_lock );

    return FALSE;
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
    char *psz_text = data->data;
    int i_len      = strlen( psz_text );

    switch( info )
    {
    case DROP_ACCEPT_TEXT_PLAIN: /* FIXME: handle multiple files */

        if( i_len < 1 )
        {
            return;
        }

        /* get rid of ' ' at the end */
        *( psz_text + i_len - 1 ) = 0;

        intf_WarnMsg( 1, "intf: dropped text/uri-list data `%s'", psz_text );
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, psz_text );

        break;

    case DROP_ACCEPT_TEXT_URI_LIST: /* FIXME: handle multiple files */

        if( i_len < 2 )
        {
            return;
        }

        /* get rid of \r\n at the end */
        *( psz_text + i_len - 2 ) = 0;

        intf_WarnMsg( 1, "intf: dropped text/uri-list data `%s'", psz_text );
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, psz_text );
        break;

    default:

        intf_ErrMsg( "intf error: unknown dropped type");
        break;
    }
}


void
on_button_title_prev_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;
    int             i_id;

    p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );
    i_id = p_intf->p_input->stream.p_selected_area->i_id - 1;

    if( i_id >= 0 )
    {
        p_area = p_intf->p_input->stream.pp_areas[i_id];
        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
    }
}


void
on_button_title_next_clicked           (GtkButton       *button,
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
on_button_chapter_prev_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;

    p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );
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
on_button_chapter_next_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;

    p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );
    p_area = p_intf->p_input->stream.p_selected_area;

    if( p_area->i_part < p_area->i_part_nb )
    {
        p_area->i_part++;
        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
    }
}


/*****************************************************************************
 * Menubar callbacks
 *****************************************************************************
 * The following callbacks are related to the menubar of the main
 * interface window.
 *****************************************************************************/
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
on_menubar_disc_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    gtk_widget_show( p_intf->p_sys->p_disc );
    gdk_window_raise( p_intf->p_sys->p_disc->window );
}


void
on_menubar_network_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    gtk_widget_show( p_intf->p_sys->p_network );
    gdk_window_raise( p_intf->p_sys->p_network->window );
}


void
on_menubar_exit_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->b_die = 1;
    vlc_mutex_unlock( &p_intf->change_lock );
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
    gdk_window_raise( p_intf->p_sys->p_playlist->window );
}


void
on_menubar_audio_toggle                (GtkCheckMenuItem     *menuitem,
                                        gpointer              user_data)
{
    intf_thread_t *         p_intf;
    es_descriptor_t *       p_es;

    p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    if( !p_intf->p_sys->b_audio_update )
    {
        p_es = (es_descriptor_t*)user_data;

        input_ToggleES( p_intf->p_input, p_es, menuitem->active );

        p_intf->p_sys->b_audio_update = menuitem->active;
    }
}


void
on_menubar_subtitle_toggle             (GtkCheckMenuItem     *menuitem,
                                        gpointer              user_data)
{
    intf_thread_t *         p_intf;
    es_descriptor_t *       p_es;

    p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    if( !p_intf->p_sys->b_spu_update )
    {
        p_es = (es_descriptor_t*)user_data;

        input_ToggleES( p_intf->p_input, p_es, menuitem->active );

        p_intf->p_sys->b_spu_update = menuitem->active;
    }
}


void
on_menubar_title_toggle                (GtkCheckMenuItem     *menuitem,
                                        gpointer              user_data)
{
    intf_thread_t * p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    if( menuitem->active && !p_intf->p_sys->b_title_update )
    {
        p_intf->p_input->pf_set_area( p_intf->p_input,
                                     (input_area_t*)user_data );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
    }
}


void
on_menubar_chapter_toggle              (GtkCheckMenuItem     *menuitem,
                                        gpointer              user_data)
{
    intf_thread_t * p_intf    = GetIntf( GTK_WIDGET(menuitem), "intf_window" );
    input_area_t *  p_area    = p_intf->p_input->stream.p_selected_area;
    gint            i_chapter = (gint)user_data;
    char            psz_chapter[3];

    if( menuitem->active && !p_intf->p_sys->b_chapter_update )
    {
        p_area->i_part = i_chapter;
        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        snprintf( psz_chapter, 3, "%02d", p_area->i_part );
        gtk_label_set_text( p_intf->p_sys->p_label_chapter, psz_chapter );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
    }

}


void
on_menubar_angle_toggle                (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data)
{
    intf_thread_t * p_intf    = GetIntf( GTK_WIDGET(menuitem), "intf_window" );
    input_area_t *  p_area    = p_intf->p_input->stream.p_selected_area;
    gint            i_angle   = (gint)user_data;

    if( menuitem->active && !p_intf->p_sys->b_angle_update )
    {
        p_area->i_angle = i_angle;
        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        p_intf->p_sys->b_angle_update = 1;
    }
}


void
on_menubar_modules_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_modules ) )
    {
        p_intf->p_sys->p_modules = create_intf_modules();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_modules ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_modules );
    gdk_window_raise( p_intf->p_sys->p_modules->window );
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


/*****************************************************************************
 * Toolbar callbacks
 *****************************************************************************
 * The following callbacks are related to the toolbar of the main
 * interface window.
 *****************************************************************************/
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
on_toolbar_disc_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    gtk_widget_show( p_intf->p_sys->p_disc );
    gdk_window_raise( p_intf->p_sys->p_disc->window );
}


void
on_toolbar_network_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    gtk_widget_show( p_intf->p_sys->p_network );
    gdk_window_raise( p_intf->p_sys->p_network->window );
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
    gdk_window_raise( p_intf->p_sys->p_playlist->window );
}


void
on_toolbar_prev_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_window" );

    if( p_intf->p_input != NULL )
    {
        /* FIXME: temporary hack */
        intf_PlaylistPrev( p_main->p_playlist );
        intf_PlaylistPrev( p_main->p_playlist );
        p_intf->p_input->b_eof = 1;
    }

    p_intf->p_sys->b_mode_changed = 1;
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

    p_intf->p_sys->b_mode_changed = 1;
}


/*****************************************************************************
 * Popup callbacks
 *****************************************************************************
 * The following callbacks are related to the popup menu. The popup
 * menu is activated when right-clicking on the video output window.
 *****************************************************************************/
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
on_popup_audio_toggle                  (GtkCheckMenuItem     *menuitem,
                                        gpointer              user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );
    es_descriptor_t *       p_es;

    p_es = (es_descriptor_t*)user_data;

    if( !p_intf->p_sys->b_audio_update )
    {
        input_ToggleES( p_intf->p_input, p_es, menuitem->active );

        p_intf->p_sys->b_audio_update = menuitem->active;
    }
}


void
on_popup_subtitle_toggle            (GtkCheckMenuItem     *menuitem,
                                     gpointer              user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );
    es_descriptor_t *       p_es;

    p_es = (es_descriptor_t*)user_data;

    if( !p_intf->p_sys->b_spu_update )
    {
        input_ToggleES( p_intf->p_input, p_es, menuitem->active );

        p_intf->p_sys->b_spu_update = menuitem->active;
    }
}


void
on_popup_navigation_toggle             (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data)
{
    intf_thread_t * p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( menuitem->active &&
        !p_intf->p_sys->b_title_update &&
        !p_intf->p_sys->b_chapter_update )
    {
        input_area_t *  p_area;
        gint            i_title;
        gint            i_chapter;

        i_title   = (gint)(user_data) / 100;
        i_chapter = (gint)(user_data) - ( 100 * i_title );
        p_area = p_intf->p_input->stream.p_selected_area;


        if( p_area != p_intf->p_input->stream.pp_areas[i_title] )
        {
            p_area = p_intf->p_input->stream.pp_areas[i_title];
            p_intf->p_sys->b_title_update = 1;
        }

        p_area->i_part = i_chapter;
        p_intf->p_sys->b_chapter_update = 1;

        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
    }
}


void
on_popup_angle_toggle                  (GtkCheckMenuItem     *menuitem,
                                        gpointer             user_data)
{
    intf_thread_t * p_intf    = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );
    input_area_t *  p_area    = p_intf->p_input->stream.p_selected_area;
    gint            i_angle   = (gint)user_data;

    if( menuitem->active && !p_intf->p_sys->b_angle_update )
    {
        p_area->i_angle = i_angle;
        p_intf->p_input->pf_set_area( p_intf->p_input, (input_area_t*)p_area );

        p_intf->p_sys->b_angle_update = 1;
    }
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
on_popup_disc_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    gtk_widget_show( p_intf->p_sys->p_disc );
    gdk_window_raise( p_intf->p_sys->p_disc->window );
}


void
on_popup_network_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    gtk_widget_show( p_intf->p_sys->p_network );
    gdk_window_raise( p_intf->p_sys->p_network->window );
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
on_popup_exit_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->b_die = 1;
    vlc_mutex_unlock( &p_intf->change_lock );
}


/*****************************************************************************
 * Fileopen callbacks
 *****************************************************************************
 * The following callbacks are related to the file requester.
 *****************************************************************************/
void
on_intf_fileopen_destroy               (GtkObject       *object,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(object), "intf_fileopen" );

    p_intf->p_sys->p_fileopen = NULL;
}


void
on_fileopen_ok_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_fileopen" );

    GtkWidget *filesel;
    gchar *filename;

    filesel = gtk_widget_get_toplevel (GTK_WIDGET (button));
    gtk_widget_hide (filesel);
    filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel));

    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, (char*)filename );

    /* Select added item and switch to file interface */
    if( p_intf->p_input != NULL )
        p_intf->p_input->b_eof = 1;

//    p_intf->p_sys->b_mode_changed = 1;
}


void
on_fileopen_cancel_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


/*****************************************************************************
 * Playlist callbacks
 *****************************************************************************
 * The following callbacks are related to the playlist.
 *****************************************************************************/
void
on_intf_playlist_destroy               (GtkObject       *object,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(object), "intf_playlist" );

    p_intf->p_sys->p_playlist = NULL;
}


void
on_playlist_ok_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_playlist" );

    gtk_widget_hide( p_intf->p_sys->p_playlist );
}


void
on_playlist_close_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}



/*****************************************************************************
 * Module manager callbacks
 *****************************************************************************
 * The following callbacks are related to the module manager.
 *****************************************************************************/
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
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


/*****************************************************************************
 * Open disc callbacks
 *****************************************************************************
 * The following callbacks are related to the disc manager.
 *****************************************************************************/
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
        p_intf->p_sys->i_intf_mode = DVD_MODE;
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

    /* Select added item and switch to DVD interface */
    intf_PlaylistJumpto( p_main->p_playlist, p_main->p_playlist->i_size-2 );
    if( p_intf->p_input != NULL )
        p_intf->p_input->b_eof = 1;
//    p_intf->p_sys->b_mode_changed = 1;

    gtk_widget_hide( p_intf->p_sys->p_disc );
}


void
on_disc_cancel_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


/*****************************************************************************
 * Network stream callbacks
 *****************************************************************************
 * The following callbacks are related to the network stream manager.
 *****************************************************************************/
void
on_network_ok_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_network" );
    char *psz_source, *psz_server, *psz_protocol;
    unsigned int i_port;

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

    /* Allocate room for "protocol://server:port" */
    psz_source = malloc( strlen( psz_protocol ) + strlen( psz_server ) + 10 );
    if( psz_source == NULL )
    {
        return;
    }
   
    /* Build source name and add it to playlist */
    sprintf( psz_source, "%s://%s:%i", psz_protocol, psz_server, i_port );
    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, psz_source );
    free( psz_source );

    /* Select added item and switch to network interface */
    intf_PlaylistJumpto( p_main->p_playlist, p_main->p_playlist->i_size-2 );
    if( p_intf->p_input != NULL )
        p_intf->p_input->b_eof = 1;
//    p_intf->p_sys->b_mode_changed = 1;

    gtk_widget_hide( p_intf->p_sys->p_network );
}


void
on_network_cancel_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

