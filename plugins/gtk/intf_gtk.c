/*****************************************************************************
 * intf_gtk.c: Gtk+ interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_gtk.c,v 1.20 2001/05/19 00:39:30 stef Exp $
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

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <gtk/gtk.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_msg.h"
#include "intf_playlist.h"

#include "video.h"
#include "video_output.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_menu.h"
#include "intf_gtk.h"

#include "main.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe      ( probedata_t *p_data );
static int  intf_Open       ( intf_thread_t *p_intf );
static void intf_Close      ( intf_thread_t *p_intf );
static void intf_Run        ( intf_thread_t *p_intf );

static gint GtkManage       ( gpointer p_data );
static gint GtkModeManage   ( intf_thread_t * p_intf );
static void GtkDisplayDate  ( GtkAdjustment *p_adj );

/*****************************************************************************
 * g_atexit: kludge to avoid the Gtk+ thread to segfault at exit
 *****************************************************************************
 * gtk_init() makes several calls to g_atexit() which calls atexit() to
 * register tidying callbacks to be called at program exit. Since the Gtk+
 * plugin is likely to be unloaded at program exit, we have to export this
 * symbol to intercept the g_atexit() calls. Talk about crude hack.
 *****************************************************************************/
void g_atexit( GVoidFunc func )
{
    intf_thread_t *p_intf = p_main->p_intf;

    if( p_intf->p_sys->pf_gdk_callback == NULL )
    {
        p_intf->p_sys->pf_gdk_callback = func;
    }
    else if( p_intf->p_sys->pf_gtk_callback == NULL )
    {
        p_intf->p_sys->pf_gtk_callback = func;
    }
    /* else nothing, but we could do something here */
    return;
}

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( intf_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = intf_Probe;
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Probe: probe the interface and return a score
 *****************************************************************************
 * This function tries to initialize Gtk+ and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int intf_Probe( probedata_t *p_data )
{
    if( TestMethod( INTF_METHOD_VAR, "gtk" ) )
    {
        return( 999 );
    }

    if( TestProgram( "gvlc" ) )
    {
        return( 190 );
    }

    return( 90 );
}

/*****************************************************************************
 * intf_Open: initialize and create window
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM));
        return( 1 );
    }

    /* Initialize Gtk+ thread */
    p_intf->p_sys->b_popup_changed = 0;
    p_intf->p_sys->b_window_changed = 0;
    p_intf->p_sys->b_playlist_changed = 0;

    p_intf->p_sys->b_slider_free = 1;

    p_intf->p_sys->pf_gtk_callback = NULL;
    p_intf->p_sys->pf_gdk_callback = NULL;

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface window
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: Gtk+ thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * gtk_main() from within it without annoying the rest of the program.
 * XXX: the approach may look kludgy, and probably is, but I could not find
 * a better way to dynamically load a Gtk+ interface at runtime.
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    /* gtk_init needs to know the command line. We don't care, so we
     * give it an empty one */
    char  *p_args[] = { "" };
    char **pp_args  = p_args;
    int    i_args   = 1;

    /* The data types we are allowed to receive */
    static GtkTargetEntry target_table[] =
    {
        { "text/uri-list", 0, DROP_ACCEPT_TEXT_URI_LIST },
        { "text/plain", 0, DROP_ACCEPT_TEXT_PLAIN }
    };

    /* intf_Manage callback timeout */
    int i_timeout;

    /* Initialize Gtk+ */
    gtk_init( &i_args, &pp_args );

    /* Create some useful widgets that will certainly be used */
    p_intf->p_sys->p_window = create_intf_window( );
    p_intf->p_sys->p_popup = create_intf_popup( );
    p_intf->p_sys->p_playlist = create_intf_playlist();
    
    /* Set the title of the main window */
    gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_window),
                          VOUT_TITLE " (Gtk+ interface)");

    /* Accept file drops on the main window */
    gtk_drag_dest_set( GTK_WIDGET( p_intf->p_sys->p_window ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       1, GDK_ACTION_COPY );

    /* Accept file drops on the playlist window */
    gtk_drag_dest_set( GTK_WIDGET( lookup_widget( p_intf->p_sys->p_playlist,
                                   "playlist_clist") ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       1, GDK_ACTION_COPY );

    /* Get the interface labels */
    p_intf->p_sys->p_slider_frame = GTK_FRAME( gtk_object_get_data(
        GTK_OBJECT(p_intf->p_sys->p_window ), "slider_frame" ) ); 

#define P_LABEL( name ) GTK_LABEL( gtk_object_get_data( \
                         GTK_OBJECT( p_intf->p_sys->p_window ), name ) )
    p_intf->p_sys->p_label_title = P_LABEL( "title_label" );
    p_intf->p_sys->p_label_chapter = P_LABEL( "chapter_label" );
#undef P_LABEL

    /* Connect the date display to the slider */
#define P_SLIDER GTK_RANGE( gtk_object_get_data( \
                         GTK_OBJECT( p_intf->p_sys->p_window ), "slider" ) )
    p_intf->p_sys->p_adj = gtk_range_get_adjustment( P_SLIDER );

    gtk_signal_connect ( GTK_OBJECT( p_intf->p_sys->p_adj ), "value_changed",
                         GTK_SIGNAL_FUNC( GtkDisplayDate ), NULL );
    p_intf->p_sys->f_adj_oldvalue = 0;
#undef P_SLIDER

    /* We don't create these ones yet because we perhaps won't need them */
    p_intf->p_sys->p_about = NULL;
    p_intf->p_sys->p_modules = NULL;
    p_intf->p_sys->p_fileopen = NULL;
    p_intf->p_sys->p_disc = NULL;
    p_intf->p_sys->p_network = NULL;
    p_intf->p_sys->p_preferences = NULL;
    p_intf->p_sys->p_jump = NULL;

    /* Store p_intf to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_popup),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_playlist ),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_adj),
                         "p_intf", p_intf );

    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );

    /* Sleep to avoid using all CPU - since some interfaces needs to access
     * keyboard events, a 100ms delay is a good compromise */
    i_timeout = gtk_timeout_add( INTF_IDLE_SLEEP / 1000, GtkManage, p_intf );

    /* Enter Gtk mode */
    gtk_main();

    /* Remove the timeout */
    gtk_timeout_remove( i_timeout );

    /* Launch stored callbacks */
    if( p_intf->p_sys->pf_gtk_callback != NULL )
    {
        p_intf->p_sys->pf_gtk_callback();

        if( p_intf->p_sys->pf_gdk_callback != NULL )
        {
            p_intf->p_sys->pf_gdk_callback();
        }
    }
}

/* following functions are local */

/*****************************************************************************
 * GtkManage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/

static gint GtkManage( gpointer p_data )
{
#define p_intf ((intf_thread_t *)p_data)

    vlc_mutex_lock( &p_intf->change_lock );
    
    /* If the "display popup" flag has changed */
    if( p_intf->b_menu_change )
    {
        if( !GTK_IS_WIDGET( p_intf->p_sys->p_popup ) )
        {
            p_intf->p_sys->p_popup = create_intf_popup();
            gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_popup ),
                                 "p_popup", p_intf );
        }
        gtk_menu_popup( GTK_MENU( p_intf->p_sys->p_popup ),
                        NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME );
        p_intf->b_menu_change = 0;
    }

    /* update the playlist */
//    GtkPlayListManage( p_data );

    if( p_intf->p_input != NULL && !p_intf->b_die )
    {
        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );

        /* New input or stream map change */
        if( p_intf->p_input->stream.b_changed )
        {
            GtkModeManage( p_intf );
        }

        /* Manage the slider */
        if( p_intf->p_input->stream.b_seekable )
        {
            float newvalue = p_intf->p_sys->p_adj->value;
    
#define p_area p_intf->p_input->stream.p_selected_area
            /* If the user hasn't touched the slider since the last time,
             * then the input can safely change it */
            if( newvalue == p_intf->p_sys->f_adj_oldvalue )
            {
                /* Update the value */
                p_intf->p_sys->p_adj->value = p_intf->p_sys->f_adj_oldvalue =
                    ( 100. * p_area->i_tell ) / p_area->i_size;
    
                gtk_signal_emit_by_name( GTK_OBJECT( p_intf->p_sys->p_adj ),
                                         "value_changed" );
            }
            /* Otherwise, send message to the input if the user has
             * finished dragging the slider */
            else if( p_intf->p_sys->b_slider_free )
            {
                off_t i_seek = ( newvalue * p_area->i_size ) / 100;

                /* release the lock to be able to seek */
                vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );
                input_Seek( p_intf->p_input, i_seek );
                vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
    
                /* Update the old value */
                p_intf->p_sys->f_adj_oldvalue = newvalue;
            }
#undef p_area
        }
        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );

        if( p_intf->p_sys->i_part !=
            p_intf->p_input->stream.p_selected_area->i_part )
        {
            p_intf->p_sys->b_chapter_update = 1;
            GtkSetupMenus( p_intf );
        }

    }
    else if( !p_intf->b_die )
    {
        GtkModeManage( p_intf );
    }

    /* Manage core vlc functions through the callback */
    p_intf->pf_manage( p_intf );

    if( p_intf->b_die )
    {
        vlc_mutex_unlock( &p_intf->change_lock );

        /* Prepare to die, young Skywalker */
        gtk_main_quit();

        /* Just in case */
        return( FALSE );
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return( TRUE );

#undef p_intf
}

/*****************************************************************************
 * GtkDisplayDate: display stream date
 *****************************************************************************
 * This function displays the current date related to the position in
 * the stream. It is called whenever the slider changes its value.
 * The lock has to be taken before you call the function.
 *****************************************************************************/
void GtkDisplayDate( GtkAdjustment *p_adj )
{
    intf_thread_t *p_intf;
   
    p_intf = gtk_object_get_data( GTK_OBJECT( p_adj ), "p_intf" );

    if( p_intf->p_input != NULL )
    {
#define p_area p_intf->p_input->stream.p_selected_area
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];

        gtk_frame_set_label( GTK_FRAME( p_intf->p_sys->p_slider_frame ),
                            input_OffsetToTime( p_intf->p_input, psz_time,
                                   ( p_area->i_size * p_adj->value ) / 100 ) );
#undef p_area
     }
}


/*****************************************************************************
 * GtkModeManage: actualise the aspect of the interface whenever the input
 *                changes.
 *****************************************************************************
 * The lock has to be taken before you call the function.
 *****************************************************************************/
static gint GtkModeManage( intf_thread_t * p_intf )
{
    GtkWidget *     p_dvd_box;
    GtkWidget *     p_file_box;
    GtkWidget *     p_network_box;
    GtkWidget *     p_slider;
    GtkWidget *     p_label;
    boolean_t       b_control;

#define GETWIDGET( ptr, name ) GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( \
                           p_intf->p_sys->ptr ) , ( name ) ) )
    /* hide all boxes except default file box */
    p_file_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "file_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_file_box ) );

    p_network_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "network_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_network_box ) );

    p_dvd_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "dvd_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_dvd_box ) );

    /* hide slider */
    p_slider = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                           p_intf->p_sys->p_window ), "slider_frame" ) );
    gtk_widget_hide( GTK_WIDGET( p_slider ) );

    /* controls unavailable */
    b_control = 0;

    /* show the box related to current input mode */
    if( p_intf->p_input != NULL )
    {
        switch( p_intf->p_input->stream.i_method & 0xf0 )
        {
            case INPUT_METHOD_FILE:
                gtk_widget_show( GTK_WIDGET( p_file_box ) );
                p_label = gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_window ),
                            "label_status" );
                gtk_label_set_text( GTK_LABEL( p_label ),
                                    p_intf->p_input->p_source );
                break;
            case INPUT_METHOD_DISC:
                gtk_widget_show( GTK_WIDGET( p_dvd_box ) );
                break;
            case INPUT_METHOD_NETWORK:
                gtk_widget_show( GTK_WIDGET( p_network_box ) );
                p_label = gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_window ),
                            "network_address_label" );
                gtk_label_set_text( GTK_LABEL( p_label ),
                                    p_intf->p_input->p_source );
                break;
            default:
                intf_ErrMsg( "intf error: can't determine input method" );
                break;
        }
    
        /* slider for seekable streams */
        if( p_intf->p_input->stream.b_seekable )
        {
            gtk_widget_show( GTK_WIDGET( p_slider ) );
        }
    
        /* control buttons for free pace streams */
        b_control = p_intf->p_input->stream.b_pace_control;

        /* get ready for menu regeneration */
        p_intf->p_sys->b_title_update = 1;
        p_intf->p_sys->b_chapter_update = 1;
        p_intf->p_sys->b_angle_update = 1;
        p_intf->p_sys->b_audio_update = 1;
        p_intf->p_sys->b_spu_update = 1;
        p_intf->p_sys->i_part = 0;
    
        p_intf->p_input->stream.b_changed = 0;
        intf_WarnMsg( 3, 
                      "intf info: menus refreshed as stream has changed" );
    }
    else
    {
        /* default mode */
        p_label = gtk_object_get_data( GTK_OBJECT( p_intf->p_sys->p_window ),
                        "label_status" );
        gtk_label_set_text( GTK_LABEL( p_label ), "" );
        gtk_widget_show( GTK_WIDGET( p_file_box ) );

        /* unsensitize menus */
        gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_title"), FALSE );
        gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_chapter"),
                                  FALSE );
        gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_angle"), FALSE );
        gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_audio"), FALSE );
        gtk_widget_set_sensitive( GETWIDGET(p_window,"menubar_subpictures"),
                                  FALSE );
        gtk_widget_set_sensitive( GETWIDGET(p_popup,"popup_navigation"),
                                  FALSE );
        gtk_widget_set_sensitive( GETWIDGET(p_popup,"popup_angle"), FALSE );
        gtk_widget_set_sensitive( GETWIDGET(p_popup,"popup_audio"), FALSE );
        gtk_widget_set_sensitive( GETWIDGET(p_popup,"popup_subpictures"),
                                  FALSE );
    }

    /* set control items */
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_back"), FALSE );
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_stop"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_pause"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_slow"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_fast"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_popup, "popup_back"), FALSE );
    gtk_widget_set_sensitive( GETWIDGET(p_popup, "popup_stop"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_popup, "popup_pause"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_popup, "popup_slow"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_popup, "popup_fast"), b_control );

#undef GETWIDGET
    return TRUE;
}
