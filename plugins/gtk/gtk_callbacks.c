/*****************************************************************************
 * gtk_callbacks.c : Callbacks for the Gtk+ plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_callbacks.c,v 1.49 2002/07/15 20:09:31 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Stéphane Borel <stef@via.ecp.fr>
 *          Julien BLACHE <jb@technologeek.org>
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

#include <gtk/gtk.h>

#include <string.h>

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_common.h"

#include "netutils.h"

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
 * Callbacks
 *****************************************************************************/

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

void GtkClose( GtkMenuItem     *menuitem,
               gpointer         user_data )
{
    intf_thread_t *p_intf = GtkGetIntf( menuitem );
    p_intf->b_die = VLC_TRUE;
}

gboolean GtkWindowDelete( GtkWidget       *widget,
                          GdkEvent        *event,
                          gpointer         user_data )
{
    GtkExit( GTK_WIDGET( widget ), user_data );

    return TRUE;
}


gboolean GtkWindowToggle( GtkWidget       *widget,
                          gpointer         user_data )
{
    intf_thread_t *p_intf = GtkGetIntf( widget );
    
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

gboolean GtkFullscreen( GtkWidget       *widget,
                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );
    vout_thread_t *p_vout;

    p_vout = vlc_object_find( p_intf->p_sys->p_input,
                              VLC_OBJECT_VOUT, FIND_CHILD );
    if( p_vout == NULL )
    {
        return FALSE;
    }

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    vlc_object_release( p_vout );
    return TRUE;
}

void GtkWindowDrag( GtkWidget       *widget,
                    GdkDragContext  *drag_context,
                    gint             x,
                    gint             y,
                    GtkSelectionData *data,
                    guint            info,
                    guint            time,
                    gpointer         user_data)
{
    intf_thread_t * p_intf = GtkGetIntf( widget );
    GtkDropDataReceived( p_intf, data, info, PLAYLIST_END );
}


/****************************************************************************
 * Slider management
 ****************************************************************************/

gboolean GtkSliderRelease( GtkWidget       *widget,
                           GdkEventButton  *event,
                           gpointer         user_data )
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 1;
    vlc_mutex_unlock( &p_intf->change_lock );

    return FALSE;
}


gboolean GtkSliderPress( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->b_slider_free = 0;
    vlc_mutex_unlock( &p_intf->change_lock );

    return FALSE;
}


/****************************************************************************
 * DVD specific items
 ****************************************************************************/

void GtkTitlePrev( GtkButton * button, gpointer user_data )
{
    intf_thread_t *  p_intf;
    input_area_t *   p_area;
    int              i_id;

    p_intf = GtkGetIntf( button );

    i_id = p_intf->p_sys->p_input->stream.p_selected_area->i_id - 1;

    /* Disallow area 0 since it is used for video_ts.vob */
    if( i_id > 0 )
    {
        p_area = p_intf->p_sys->p_input->stream.pp_areas[i_id];
        input_ChangeArea( p_intf->p_sys->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        GtkSetupMenus( p_intf );
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}


void GtkTitleNext( GtkButton * button, gpointer user_data )
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;
    int             i_id;

    p_intf = GtkGetIntf( button );
    i_id = p_intf->p_sys->p_input->stream.p_selected_area->i_id + 1;

    if( i_id < p_intf->p_sys->p_input->stream.i_area_nb )
    {
        p_area = p_intf->p_sys->p_input->stream.pp_areas[i_id];   
        input_ChangeArea( p_intf->p_sys->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_title_update = 1;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        GtkSetupMenus( p_intf );
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}


void GtkChapterPrev( GtkButton * button, gpointer user_data )
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;

    p_intf = GtkGetIntf( button );
    p_area = p_intf->p_sys->p_input->stream.p_selected_area;

    if( p_area->i_part > 0 )
    {
        p_area->i_part--;
        input_ChangeArea( p_intf->p_sys->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        GtkSetupMenus( p_intf );
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}


void GtkChapterNext( GtkButton * button, gpointer user_data )
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;

    p_intf = GtkGetIntf( button );
    p_area = p_intf->p_sys->p_input->stream.p_selected_area;

    if( p_area->i_part < p_area->i_part_nb )
    {
        p_area->i_part++;
        input_ChangeArea( p_intf->p_sys->p_input, (input_area_t*)p_area );

        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );

        p_intf->p_sys->b_chapter_update = 1;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        GtkSetupMenus( p_intf );
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}

/****************************************************************************
 * Network specific items
 ****************************************************************************/
void GtkNetworkJoin( GtkEditable * editable, gpointer user_data )
{
    int     i_channel;

    i_channel = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( editable ) );
//    msg_Dbg( "intf info: joining channel %d", i_channel );

//    network_ChannelJoin( i_channel );
}

void GtkChannelGo( GtkButton * button, gpointer user_data )
{
    GtkWidget *     window;
    GtkWidget *     spin;
    int             i_channel;

    intf_thread_t *p_intf = GtkGetIntf( button );

    window = gtk_widget_get_toplevel( GTK_WIDGET (button) );
    spin = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( window ),
                       "network_channel_spinbutton" ) );

    i_channel = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( spin ) );
    msg_Dbg( p_intf, "joining channel %d", i_channel );

    vlc_mutex_lock( &p_intf->change_lock );
    network_ChannelJoin( p_intf, i_channel );
    vlc_mutex_unlock( &p_intf->change_lock );

//    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
}


/****************************************************************************
 * About box
 ****************************************************************************/

gboolean GtkAboutShow( GtkWidget       *widget,
                       gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

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
    intf_thread_t *p_intf = GtkGetIntf( button );

    gtk_widget_hide( p_intf->p_sys->p_about );
}


/****************************************************************************
 * Jump box
 ****************************************************************************/

gboolean GtkJumpShow( GtkWidget       *widget,
                      gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

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


void GtkJumpOk( GtkButton       *button,
                gpointer         user_data)
{
    intf_thread_t * p_intf = GtkGetIntf( button );
    int i_hours, i_minutes, i_seconds;

    if( p_intf->p_sys->p_input == NULL )
    {
        return;
    }

#define GET_VALUE( name )                                                   \
    gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( gtk_object_get_data( \
        GTK_OBJECT( p_intf->p_sys->p_jump ), name ) ) )
    i_hours   = GET_VALUE( "jump_hour_spinbutton" );
    i_minutes = GET_VALUE( "jump_minute_spinbutton" );
    i_seconds = GET_VALUE( "jump_second_spinbutton" );
#undef GET_VALUE

    input_Seek( p_intf->p_sys->p_input,
                i_seconds + 60 * i_minutes + 3600 * i_hours,
                INPUT_SEEK_SECONDS | INPUT_SEEK_SET );

    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


void GtkJumpCancel( GtkButton       *button,
                    gpointer         user_data)
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

/****************************************************************************
 * Callbacks for disc ejection
 ****************************************************************************/
gboolean GtkDiscEject ( GtkWidget *widget, gpointer user_data )
{
    char *psz_device = NULL;
    char *psz_parser;
    char *psz_current;

    intf_thread_t *p_intf = GtkGetIntf( widget );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return FALSE;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_playlist->i_index < 0 )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        return FALSE;
    }

    psz_current = p_playlist->pp_items[ p_playlist->i_index ]->psz_name;

    /*
     * Get the active input
     * Determine whether we can eject a media, ie it's a VCD or DVD
     * If it's neither a VCD nor a DVD, then return
     */

    /*
     * Don't really know if I must lock the stuff here, we're using it read-only
     */

    if( psz_current != NULL )
    {
        if( !strncmp(psz_current, "dvd:", 4) )
        {
            switch( psz_current[4] )
            {
            case '\0':
            case '@':
                psz_device = config_GetPsz( p_intf, "dvd" );
                break;
            default:
                /* Omit the first 4 characters */
                psz_device = strdup( psz_current + 4 );
                break;
            }
        }
        else if( !strncmp(psz_current, "vcd:", 4) )
        {
            switch( psz_current[4] )
            {
            case '\0':
            case '@':
                psz_device = config_GetPsz( p_intf, "vcd" );
                break;
            default:
                /* Omit the first 4 characters */
                psz_device = strdup( psz_current + 4 );
                break;
            }
        }
        else
        {
            psz_device = strdup( psz_current );
        }
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );

    if( psz_device == NULL )
    {
        return TRUE;
    }

    /* Remove what we have after @ */
    psz_parser = psz_device;
    for( psz_parser = psz_device ; *psz_parser ; psz_parser++ )
    {
        if( *psz_parser == '@' )
        {
            *psz_parser = '\0';
            break;
        }
    }

    /* If there's a stream playing, we aren't allowed to eject ! */
    if( p_intf->p_sys->p_input == NULL )
    {
        msg_Dbg( p_intf, "ejecting %s", psz_device );

        intf_Eject( p_intf, psz_device );
    }

    free(psz_device);

    return TRUE;
}

/****************************************************************************
 * Messages window
 ****************************************************************************/

gboolean GtkMessagesShow( GtkWidget       *widget,
                          gpointer         user_data)
{
    static GdkColor black = { 0, 0x0000, 0x0000, 0x0000 };
    static GdkColormap *colormap;
    intf_thread_t *p_intf = GtkGetIntf( widget );

    gtk_widget_show( p_intf->p_sys->p_messages );
    colormap = gdk_colormap_get_system ();
    gdk_color_alloc( colormap, &black );
    gdk_window_set_background( p_intf->p_sys->p_messages_text->text_area,
                               &black );

    gdk_window_raise( p_intf->p_sys->p_messages->window );

    gtk_text_set_point( p_intf->p_sys->p_messages_text,
                    gtk_text_get_length( p_intf->p_sys->p_messages_text ) );

    return TRUE;
}


void
GtkMessagesOk                          (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );
    gtk_widget_hide( p_intf->p_sys->p_messages );
}


gboolean
GtkMessagesDelete                      (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( widget );
    gtk_widget_hide( p_intf->p_sys->p_messages );
    return TRUE;
}


void
GtkOpenNotebookChanged                 (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        gint             page_num,
                                        gpointer         user_data)
{
    GtkOpenChanged( GTK_WIDGET( notebook ), user_data );
}

