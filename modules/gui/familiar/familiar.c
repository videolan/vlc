/*****************************************************************************
 * familiar.c : familiar plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: familiar.c,v 1.22 2003/01/03 20:55:00 jpsaman Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <gtk/gtk.h>

#ifdef HAVE_GPE_INIT_H
#include <gpe/init.h>
#endif

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "familiar.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Run          ( intf_thread_t * );

void GtkAutoPlayFile     ( vlc_object_t * );
static int Manage        ( intf_thread_t *p_intf );
void E_(GtkDisplayDate)  ( GtkAdjustment *p_adj );
gint E_(GtkModeManage)   ( intf_thread_t * p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define AUTOPLAYFILE_TEXT  N_("autoplay selected file")
#define AUTOPLAYFILE_LONGTEXT N_("automatically play a file when selected in the "\
        "file selection list")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_bool( "familiar-autoplayfile", 1, GtkAutoPlayFile, AUTOPLAYFILE_TEXT, AUTOPLAYFILE_LONGTEXT );
    set_description( _("Familiar Linux Gtk+ interface module") );
    set_capability( "interface", 70 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return VLC_ENOMEM;
    }

#ifdef NEED_GTK_MAIN
    msg_Dbg( p_intf, "Using gtk_main" );
    p_intf->p_sys->p_gtk_main = module_Need( p_this, "gtk_main", "gtk" );
    if( p_intf->p_sys->p_gtk_main == NULL )
    {
        free( p_intf->p_sys );
        return VLC_ENOMOD;
    }
#endif

    /* Initialize Gtk+ thread */
    p_intf->p_sys->p_input = NULL;

    p_intf->p_sys->b_autoplayfile = 1;
    p_intf->p_sys->b_playing = 0;
    p_intf->p_sys->b_slider_free = 1;

    p_intf->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

#ifdef NEED_GTK_MAIN
    msg_Dbg( p_intf, "Releasing gtk_main" );
    module_Unneed( p_intf, p_intf->p_sys->p_gtk_main );
#endif

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: Gtk+ thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * gtk_main() from within it without annoying the rest of the program.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
#ifndef NEED_GTK_MAIN 
    /* gtk_init needs to know the command line. We don't care, so we
     * give it an empty one */
    char  *p_args[] = { "" };
    char **pp_args  = p_args;
    int    i_args   = 1;
    int    i_dummy;
#endif

#ifdef HAVE_GPE_INIT_H
    /* Initialize GPE interface */
    msg_Dbg( p_intf, "Starting familiar GPE interface" );
    if (gpe_application_init(&i_args, &pp_args) == FALSE)
        exit (1);
#else
    gtk_set_locale ();
 #ifndef NEED_GTK_MAIN
    msg_Dbg( p_intf, "Starting familiar GTK+ interface" );
    gtk_init( &i_args, &pp_args );
 #else
    /* Initialize Gtk+ */
    msg_Dbg( p_intf, "Starting familiar GTK+ interface thread" );
    gdk_threads_enter();
 #endif
#endif

    /* Create some useful widgets that will certainly be used */
// FIXME: magic path
    add_pixmap_directory("share");
    add_pixmap_directory("/usr/share/vlc");

    p_intf->p_sys->p_window = create_familiar();
    if (p_intf->p_sys->p_window == NULL)
    {
        msg_Err( p_intf, "unable to create familiar interface" );
    }

    /* Set the title of the main window */
    gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_window),
                          VOUT_TITLE " (Familiar Linux interface)");

    p_intf->p_sys->p_notebook = GTK_NOTEBOOK( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_window ), "notebook" ) );

    /* Get the slider object */
    p_intf->p_sys->p_slider = GTK_HSCALE( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_window ), "slider" ) );
    p_intf->p_sys->p_slider_label = GTK_LABEL( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_window ), "slider_label" ) );

    /* Connect the date display to the slider */
#define P_SLIDER GTK_RANGE( gtk_object_get_data( \
                         GTK_OBJECT( p_intf->p_sys->p_window ), "slider" ) )
    p_intf->p_sys->p_adj = gtk_range_get_adjustment( P_SLIDER );

    gtk_signal_connect ( GTK_OBJECT( p_intf->p_sys->p_adj ), "value_changed",
                         GTK_SIGNAL_FUNC( E_(GtkDisplayDate) ), NULL );
    p_intf->p_sys->f_adj_oldvalue = 0;
#undef P_SLIDER

    p_intf->p_sys->p_clist = GTK_CLIST( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_window ), "clistmedia" ) );
    gtk_clist_set_column_visibility (GTK_CLIST (p_intf->p_sys->p_clist), 2, FALSE);
    gtk_clist_set_column_visibility (GTK_CLIST (p_intf->p_sys->p_clist), 3, FALSE);
    gtk_clist_set_column_visibility (GTK_CLIST (p_intf->p_sys->p_clist), 4, FALSE);
    gtk_clist_column_titles_show (GTK_CLIST (p_intf->p_sys->p_clist));

    /* Store p_intf to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         "p_intf", p_intf );
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_adj),
                         "p_intf", p_intf );

    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );
    ReadDirectory(p_intf->p_sys->p_clist, "/mnt");

#ifdef NEED_GTK_MAIN
    msg_Dbg( p_intf, "Manage GTK keyboard events using threads" );
    while( !p_intf->b_die )
    {
        Manage( p_intf );

        /* Sleep to avoid using all CPU - since some interfaces need to
         * access keyboard events, a 100ms delay is a good compromise */
        gdk_threads_leave();
        msleep( INTF_IDLE_SLEEP );
        gdk_threads_enter();
    }
#else
    msg_Dbg( p_intf, "Manage GTK keyboard events using timeouts" );
    /* Sleep to avoid using all CPU - since some interfaces needs to access
     * keyboard events, a 100ms delay is a good compromise */
    i_dummy = gtk_timeout_add( INTF_IDLE_SLEEP / 1000, (GtkFunction)Manage, p_intf );

    /* Enter Gtk mode */
    gtk_main();
    /* Remove the timeout */
    gtk_timeout_remove( i_dummy );
#endif

    gtk_object_destroy( GTK_OBJECT(p_intf->p_sys->p_window) );
#ifdef NEED_GTK_MAIN
    gdk_threads_leave();
#else
    gtk_main_quit();
#endif
}

/*****************************************************************************
 * GtkAutoplayFile: Autoplay file depending on configuration settings
 *****************************************************************************/
void GtkAutoPlayFile( vlc_object_t *p_this )
{
    GtkWidget *cbautoplay;
    intf_thread_t *p_intf;
    int i_index;
    vlc_list_t list = vlc_list_find( p_this, VLC_OBJECT_INTF, FIND_ANYWHERE );

    for( i_index = 0; i_index < list.i_count; i_index++ )
    {
        p_intf = (intf_thread_t *)list.p_values[i_index].p_object ;

        if( strcmp( MODULE_STRING, p_intf->p_module->psz_object_name ) )
        {
            continue;
        }
        cbautoplay = GTK_WIDGET( gtk_object_get_data(
                            GTK_OBJECT( p_intf->p_sys->p_window ),
                            "cbautoplay" ) );

        if( !config_GetInt( p_this, "familiar-autoplayfile" ) )
        {
            p_intf->p_sys->b_autoplayfile = VLC_FALSE;
        }
        else
        {
            p_intf->p_sys->b_autoplayfile = VLC_TRUE;
        }
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( cbautoplay ),
                                      p_intf->p_sys->b_autoplayfile );
    }
    vlc_list_release( &list );
}

/* following functions are local */

/*****************************************************************************
 * Manage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
static int Manage( intf_thread_t *p_intf )
{
    vlc_mutex_lock( &p_intf->change_lock );

    /* Update the input */
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                          FIND_ANYWHERE );
    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

    if( p_intf->p_sys->p_input )
    {
        input_thread_t *p_input = p_intf->p_sys->p_input;

        vlc_mutex_lock( &p_input->stream.stream_lock );
        if( !p_input->b_die )
        {
            /* New input or stream map change */
            if( p_input->stream.b_changed )
            {
                E_(GtkModeManage)( p_intf );
                p_intf->p_sys->b_playing = 1;
            }

            /* Manage the slider */
            if( p_input->stream.b_seekable && p_intf->p_sys->b_playing )
            {
                float newvalue = p_intf->p_sys->p_adj->value;

#define p_area p_input->stream.p_selected_area
                /* If the user hasn't touched the slider since the last time,
                 * then the input can safely change it */
                if( newvalue == p_intf->p_sys->f_adj_oldvalue )
                {
                    /* Update the value */
                    p_intf->p_sys->p_adj->value =
                    p_intf->p_sys->f_adj_oldvalue =
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
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                    input_Seek( p_input, i_seek, INPUT_SEEK_SET );
                    vlc_mutex_lock( &p_input->stream.stream_lock );

                    /* Update the old value */
                    p_intf->p_sys->f_adj_oldvalue = newvalue;
                }
#undef p_area
            }
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else if( p_intf->p_sys->b_playing && !p_intf->b_die )
    {
        E_(GtkModeManage)( p_intf );
        p_intf->p_sys->b_playing = 0;
    }

#ifndef NEED_GTK_MAIN
    if( p_intf->b_die )
    {
        vlc_mutex_unlock( &p_intf->change_lock );

        /* Prepare to die, young Skywalker */
        gtk_main_quit();

        return FALSE;
    }
#endif

    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}

/*****************************************************************************
 * GtkDisplayDate: display stream date
 *****************************************************************************
 * This function displays the current date related to the position in
 * the stream. It is called whenever the slider changes its value.
 * The lock has to be taken before you call the function.
 *****************************************************************************/
void E_(GtkDisplayDate)( GtkAdjustment *p_adj )
{
    intf_thread_t *p_intf;

    p_intf = gtk_object_get_data( GTK_OBJECT( p_adj ), "p_intf" );

    if( p_intf->p_sys->p_input )
    {
#define p_area p_intf->p_sys->p_input->stream.p_selected_area
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];

        gtk_label_set_text( GTK_LABEL( p_intf->p_sys->p_slider_label ),
                        input_OffsetToTime( p_intf->p_sys->p_input, psz_time,
                                   ( p_area->i_size * p_adj->value ) / 100 ) );
#undef p_area
     }
}

/*****************************************************************************
 * GtkModeManage: actualize the aspect of the interface whenever the input
 *                changes.
 *****************************************************************************
 * The lock has to be taken before you call the function.
 *****************************************************************************/
gint E_(GtkModeManage)( intf_thread_t * p_intf )
{
    GtkWidget *     p_slider;
    vlc_bool_t      b_control;

#define GETWIDGET( ptr, name ) GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( \
                           p_intf->p_sys->ptr ) , ( name ) ) )
    /* hide slider */
    p_slider = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                           p_intf->p_sys->p_window ), "slider" ) );
    gtk_widget_hide( GTK_WIDGET( p_slider ) );

    /* controls unavailable */
    b_control = 0;

    /* show the box related to current input mode */
    if( p_intf->p_sys->p_input )
    {
        /* initialize and show slider for seekable streams */
        if( p_intf->p_sys->p_input->stream.b_seekable )
        {
            p_intf->p_sys->p_adj->value = p_intf->p_sys->f_adj_oldvalue = 0;
            gtk_signal_emit_by_name( GTK_OBJECT( p_intf->p_sys->p_adj ),
                                     "value_changed" );
            gtk_widget_show( GTK_WIDGET( p_slider ) );
        }

        /* control buttons for free pace streams */
        b_control = p_intf->p_sys->p_input->stream.b_pace_control;

        p_intf->p_sys->p_input->stream.b_changed = 0;
        msg_Dbg( p_intf, "stream has changed, refreshing interface" );
    }

    /* set control items */
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_rewind"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_pause"), b_control );
    gtk_widget_set_sensitive( GETWIDGET(p_window, "toolbar_forward"), b_control );

#undef GETWIDGET
    return TRUE;
}

