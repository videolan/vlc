/*****************************************************************************
 * gnome.c : Gnome plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: gnome.c,v 1.28 2002/07/02 19:16:47 sam Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <gnome.h>

#include "gnome_callbacks.h"
#include "gnome_interface.h"
#include "gnome_support.h"
#include "gtk_display.h"
#include "gtk_common.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void intf_getfunctions( function_list_t * p_function_list );
static int  intf_Open        ( intf_thread_t *p_intf );
static void intf_Close       ( intf_thread_t *p_intf );
static void intf_Run         ( intf_thread_t *p_intf );

static gint GnomeManage      ( gpointer p_data );

/*****************************************************************************
 * Local variables (mutex-protected).
 *****************************************************************************/
static void ** pp_global_data = NULL;

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
#define TOOLTIPS_TEXT N_("show tooltips")
#define TOOLTIPS_LONGTEXT N_("Show tooltips for configuration options.")

#define TOOLBAR_TEXT N_("show text on toolbar buttons")
#define TOOLBAR_LONGTEXT N_("Show the text below icons on the toolbar.")

#define PREFS_MAXH_TEXT N_("maximum height for the configuration windows")
#define PREFS_MAXH_LONGTEXT N_( \
    "You can set the maximum height that the configuration windows in the " \
    "preferences menu will occupy.")

MODULE_CONFIG_START
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_BOOL   ( "gnome-tooltips", 1, GtkHideTooltips, TOOLTIPS_TEXT,
             TOOLTIPS_LONGTEXT )
ADD_BOOL   ( "gnome-toolbartext", 1, GtkHideToolbarText, TOOLBAR_TEXT,
             TOOLBAR_LONGTEXT )
ADD_INTEGER( "gnome-prefs-maxh", 480, NULL, PREFS_MAXH_TEXT,
             PREFS_MAXH_LONGTEXT )
MODULE_CONFIG_STOP

MODULE_INIT_START
    pp_global_data = p_module->p_vlc->pp_global_data;
    SET_DESCRIPTION( _("Gnome interface module") )
#ifndef WIN32
    if( getenv( "DISPLAY" ) == NULL )
    {
        ADD_CAPABILITY( INTF, 15 )
    }
    else
#endif
    {
        ADD_CAPABILITY( INTF, 100 )
    }
    ADD_PROGRAM( "gnome-vlc" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    intf_getfunctions( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * g_atexit: kludge to avoid the Gnome thread to segfault at exit
 *****************************************************************************
 * gtk_init() makes several calls to g_atexit() which calls atexit() to
 * register tidying callbacks to be called at program exit. Since the Gnome
 * plugin is likely to be unloaded at program exit, we have to export this
 * symbol to intercept the g_atexit() calls. Talk about crude hack.
 *****************************************************************************/
void g_atexit( GVoidFunc func )
{
    intf_thread_t *p_intf;

    int i_dummy;

    if( pp_global_data == NULL )
    {
        atexit( func );
        return;
    }

    p_intf = (intf_thread_t *)*pp_global_data;
    if( p_intf == NULL )
    {
        return;
    }

    for( i_dummy = 0;
         i_dummy < MAX_ATEXIT && p_intf->p_sys->pf_callback[i_dummy] != NULL;
         i_dummy++ )
    {
        ;
    }

    if( i_dummy >= MAX_ATEXIT - 1 )
    {
        msg_Err( p_intf, "too many atexit() callbacks to register" );
        return;
    }

    p_intf->p_sys->pf_callback[i_dummy]     = func;
    p_intf->p_sys->pf_callback[i_dummy + 1] = NULL;
}

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void intf_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
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
        msg_Err( p_intf, "out of memory" );
        return( 1 );
    }

    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    /* Initialize Gnome thread */
    p_intf->p_sys->b_playing = 0;
    p_intf->p_sys->b_popup_changed = 0;
    p_intf->p_sys->b_window_changed = 0;
    p_intf->p_sys->b_playlist_changed = 0;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->i_playing = -1;
    p_intf->p_sys->b_slider_free = 1;

    p_intf->p_sys->pf_callback[0] = NULL;

    p_intf->p_sys->i_part = 0;

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface window
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: Gnome thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * gtk_main() from within it without annoying the rest of the program.
 * XXX: the approach may look kludgy, and probably is, but I could not find
 * a better way to dynamically load a Gnome interface at runtime.
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    /* gnome_init needs to know the command line. We don't care, so we
     * give it an empty one */
    char *p_args[] = { "" };
    int   i_args   = 1;
    int   i_dummy;

    /* The data types we are allowed to receive */
    static GtkTargetEntry target_table[] =
    {
        { "STRING", 0, DROP_ACCEPT_STRING },
        { "text/uri-list", 0, DROP_ACCEPT_TEXT_URI_LIST },
        { "text/plain",    0, DROP_ACCEPT_TEXT_PLAIN }
    };

    /* Initialize Gnome */

    /* gnome_init will register stuff with g_atexit, so we need to take
     * the global lock if we want to be able to intercept the calls */
    vlc_mutex_lock( p_intf->p_vlc->p_global_lock );
    *p_intf->p_vlc->pp_global_data = p_intf;
    gnome_init( p_intf->p_vlc->psz_object_name, VERSION, i_args, p_args );
    vlc_mutex_unlock( p_intf->p_vlc->p_global_lock );

    /* Create some useful widgets that will certainly be used */
    p_intf->p_sys->p_window = create_intf_window( );
    p_intf->p_sys->p_popup = create_intf_popup( );
    p_intf->p_sys->p_playwin = create_intf_playlist();
    p_intf->p_sys->p_messages = create_intf_messages();

    /* Set the title of the main window */
    gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_window),
                          VOUT_TITLE " (Gnome interface)");

    /* Accept file drops on the main window */
    gtk_drag_dest_set( GTK_WIDGET( p_intf->p_sys->p_window ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       1, GDK_ACTION_COPY );
    /* Accept file drops on the playlist window */
    gtk_drag_dest_set( GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_playwin ), "playlist_clist") ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       1, GDK_ACTION_COPY );

    /* Get the slider object */
    p_intf->p_sys->p_slider_frame = gtk_object_get_data(
                      GTK_OBJECT( p_intf->p_sys->p_window ), "slider_frame" );

    /* Configure the log window */
    p_intf->p_sys->p_messages_text = GTK_TEXT( gtk_object_get_data(
        GTK_OBJECT(p_intf->p_sys->p_messages ), "messages_textbox" ) );
    gtk_text_set_line_wrap( p_intf->p_sys->p_messages_text, TRUE);
    gtk_text_set_word_wrap( p_intf->p_sys->p_messages_text, FALSE);

    /* Get the interface labels */
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

    p_intf->p_sys->p_tooltips = gtk_tooltips_new();

    /* We don't create these ones yet because we perhaps won't need them */
    p_intf->p_sys->p_about = NULL;
    p_intf->p_sys->p_modules = NULL;
    p_intf->p_sys->p_fileopen = NULL;
    p_intf->p_sys->p_disc = NULL;
    p_intf->p_sys->p_network = NULL;
    p_intf->p_sys->p_sat = NULL;
    p_intf->p_sys->p_jump = NULL;

    /* Hide tooltips if the option is set */
    if( !config_GetInt( p_intf, "gnome-tooltips" ) )
    {
        gtk_tooltips_disable( p_intf->p_sys->p_tooltips );
    }

    /* Hide toolbar text of the option is set */
    if( !config_GetInt( p_intf, "gnome-toolbartext" ) )
    {
        gtk_toolbar_set_style(
            GTK_TOOLBAR(lookup_widget( p_intf->p_sys->p_window, "toolbar" )),
            GTK_TOOLBAR_ICONS );
    }

    /* Store p_intf to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_popup),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_playwin ),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_messages ),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_adj),
                         "p_intf", p_intf );

    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );

    /* Sleep to avoid using all CPU - since some interfaces needs to access
     * keyboard events, a 100ms delay is a good compromise */
    i_dummy = gtk_timeout_add( INTF_IDLE_SLEEP / 1000, GnomeManage, p_intf );

    /* Enter gnome mode */
    gtk_main();

    /* Remove the timeout */
    gtk_timeout_remove( i_dummy );

    /* Destroy the Tooltips structure */
    gtk_object_destroy( GTK_OBJECT(p_intf->p_sys->p_tooltips) );

    /* Get rid of stored callbacks so we can unload the plugin */
    for( i_dummy = 0;
         i_dummy < MAX_ATEXIT && p_intf->p_sys->pf_callback[i_dummy] != NULL;
         i_dummy++ )
    {
        p_intf->p_sys->pf_callback[i_dummy]();
    }
}

/* following functions are local */

/*****************************************************************************
 * GnomeManage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
static gint GnomeManage( gpointer p_data )
{
#define p_intf ((intf_thread_t *)p_data)
    int i_start, i_stop;

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

        gnome_popup_menu_do_popup( p_intf->p_sys->p_popup,
                                   NULL, NULL, NULL, NULL );
        p_intf->b_menu_change = 0;
    }

    /* Update the log window */
    vlc_mutex_lock( p_intf->p_sys->p_sub->p_lock );
    i_stop = *p_intf->p_sys->p_sub->pi_stop;
    vlc_mutex_unlock( p_intf->p_sys->p_sub->p_lock );

    if( p_intf->p_sys->p_sub->i_start != i_stop )
    {
        static GdkColor white  = { 0, 0xffff, 0xffff, 0xffff };
        static GdkColor gray   = { 0, 0xaaaa, 0xaaaa, 0xaaaa };
        static GdkColor yellow = { 0, 0xffff, 0xffff, 0x6666 };
        static GdkColor red    = { 0, 0xffff, 0x6666, 0x6666 };

        static const char * ppsz_type[4] = { ": ", " error: ", " warning: ",
                                             " debug: " };
        static GdkColor *   pp_color[4] = { &white, &red, &yellow, &gray };

        for( i_start = p_intf->p_sys->p_sub->i_start;
             i_start != i_stop;
             i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
            /* Append all messages to log window */
            gtk_text_insert( p_intf->p_sys->p_messages_text, NULL, &gray,
             NULL, p_intf->p_sys->p_sub->p_msg[i_start].psz_module, -1 );

            gtk_text_insert( p_intf->p_sys->p_messages_text, NULL, &gray,
                NULL, ppsz_type[p_intf->p_sys->p_sub->p_msg[i_start].i_type],
                -1 );

            gtk_text_insert( p_intf->p_sys->p_messages_text, NULL,
                pp_color[p_intf->p_sys->p_sub->p_msg[i_start].i_type], NULL,
                p_intf->p_sys->p_sub->p_msg[i_start].psz_msg, -1 );

            gtk_text_insert( p_intf->p_sys->p_messages_text, NULL, &gray,
                NULL, "\n", -1 );
        }

        vlc_mutex_lock( p_intf->p_sys->p_sub->p_lock );
        p_intf->p_sys->p_sub->i_start = i_start;
        vlc_mutex_unlock( p_intf->p_sys->p_sub->p_lock );

        gtk_text_set_point( p_intf->p_sys->p_messages_text,
                    gtk_text_get_length( p_intf->p_sys->p_messages_text ) );
    }

    /* Update the playlist */
    GtkPlayListManage( p_intf );

    /* Update the input */
    if( p_intf->p_sys->p_input != NULL )
    {
        if( p_intf->p_sys->p_input->b_dead )
        {
            vlc_object_release( p_intf->p_sys->p_input );
            p_intf->p_sys->p_input = NULL;
        }
    }
    
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                          FIND_ANYWHERE );
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
                GtkModeManage( p_intf );
                GtkSetupMenus( p_intf );
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

                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                    input_Seek( p_input, i_seek, INPUT_SEEK_SET );
                    vlc_mutex_lock( &p_input->stream.stream_lock );

                    /* Update the old value */
                    p_intf->p_sys->f_adj_oldvalue = newvalue;
                }
#undef p_area
            }

            if( p_intf->p_sys->i_part !=
                p_input->stream.p_selected_area->i_part )
            {
                p_intf->p_sys->b_chapter_update = 1;
                GtkSetupMenus( p_intf );
            }
        }

        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else if( p_intf->p_sys->b_playing && !p_intf->p_vlc->b_die )
    {
        GtkModeManage( p_intf );
        p_intf->p_sys->b_playing = 0;
    }

    if( p_intf->p_vlc->b_die )
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

