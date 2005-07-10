/*****************************************************************************
 * gnome.c : Gnome plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000 the VideoLAN team
 * $Id$
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
#include "display.h"
#include "common.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static void Run          ( intf_thread_t * );
static void Manage       ( intf_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define TOOLTIPS_TEXT N_("Show tooltips")
#define TOOLTIPS_LONGTEXT N_("Show tooltips for configuration options.")

#define TOOLBAR_TEXT N_("Show text on toolbar buttons")
#define TOOLBAR_LONGTEXT N_("Show the text below icons on the toolbar.")

#define PREFS_MAXH_TEXT N_("Maximum height for the configuration windows")
#define PREFS_MAXH_LONGTEXT N_( \
    "You can set the maximum height that the configuration windows in the " \
    "preferences menu will occupy.")

#define PATH_TEXT N_("Interface default search path")
#define PATH_LONGTEXT N_( \
    "This option allows you to set the default path that the interface will " \
    "open when looking for a file.")

vlc_module_begin();
#ifdef WIN32
    int i = 90;
#else
    int i = getenv( "DISPLAY" ) == NULL ? 15 : 100;
#endif
    set_description( _("GNOME interface") );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );

    add_bool( "gnome-tooltips", 1, E_(GtkHideTooltips),
              TOOLTIPS_TEXT, TOOLTIPS_LONGTEXT, VLC_FALSE );
    add_bool( "gnome-toolbartext", 1, GtkHideToolbarText, TOOLBAR_TEXT,
              TOOLBAR_LONGTEXT, VLC_FALSE );
    add_integer( "gnome-prefs-maxh", 480, NULL,
                 PREFS_MAXH_TEXT, PREFS_MAXH_LONGTEXT, VLC_TRUE );
    add_directory( "gnome-search-path", NULL, NULL, PATH_TEXT,
                   PATH_LONGTEXT, VLC_TRUE );

    set_capability( "interface", i );
    set_callbacks( Open, Close );
    set_program( "gnome-vlc" );
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

    p_intf->p_sys->p_gtk_main =
        module_Need( p_this, "gui-helper", "gnome", VLC_TRUE );
    if( p_intf->p_sys->p_gtk_main == NULL )
    {
        free( p_intf->p_sys );
        return VLC_ENOMOD;
    }

    p_intf->pf_run = Run;

    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    /* Initialize Gnome thread */
    p_intf->p_sys->b_playing = VLC_FALSE;
    p_intf->p_sys->b_deinterlace_update = VLC_FALSE;

    p_intf->p_sys->b_aout_update = VLC_FALSE;
    p_intf->p_sys->b_vout_update = VLC_FALSE;

    p_intf->p_sys->b_popup_changed = VLC_FALSE;
    p_intf->p_sys->b_window_changed = VLC_FALSE;
    p_intf->p_sys->b_playlist_changed = VLC_FALSE;
    p_intf->p_sys->b_program_update = VLC_FALSE;
    p_intf->p_sys->b_title_update = VLC_FALSE;
    p_intf->p_sys->b_chapter_update = VLC_FALSE;
    p_intf->p_sys->b_spu_update = VLC_FALSE;
    p_intf->p_sys->b_audio_update = VLC_FALSE;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->i_playing = -1;
    p_intf->p_sys->b_slider_free = VLC_TRUE;

    p_intf->p_sys->i_part = 0;
    p_intf->p_sys->b_mute = VLC_FALSE;

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

    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    module_Unneed( p_intf, p_intf->p_sys->p_gtk_main );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: Gnome thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * gtk_main() from within it without annoying the rest of the program.
 * XXX: the approach may look kludgy, and probably is, but I could not find
 * a better way to dynamically load a Gnome interface at runtime.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    /* The data types we are allowed to receive */
    static GtkTargetEntry target_table[] =
    {
        { "STRING", 0, DROP_ACCEPT_STRING },
        { "text/uri-list", 0, DROP_ACCEPT_TEXT_URI_LIST },
        { "text/plain",    0, DROP_ACCEPT_TEXT_PLAIN }
    };
    char *psz_sout;
    GString *       p_target;

    gdk_threads_enter();

    /* Create some useful widgets that will certainly be used */
    p_intf->p_sys->p_window = create_intf_window( );
    p_intf->p_sys->p_popup = create_intf_popup( );
    p_intf->p_sys->p_playwin = create_intf_playlist();
    p_intf->p_sys->p_messages = create_intf_messages();
    p_intf->p_sys->p_tooltips = gtk_tooltips_new();
    p_intf->p_sys->p_sout = create_intf_sout();

    /* Set the title of the main window */
    gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_window),
                          VOUT_TITLE " (Gnome interface)");

    /* Accept file drops on the main window */
    gtk_drag_dest_set( GTK_WIDGET( p_intf->p_sys->p_window ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       DROP_ACCEPT_END, GDK_ACTION_COPY );
    /* Accept file drops on the playlist window */
    gtk_drag_dest_set( GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_playwin ), "playlist_clist") ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       DROP_ACCEPT_END, GDK_ACTION_COPY );

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
                         GTK_SIGNAL_FUNC( E_(GtkDisplayDate) ), NULL );
    p_intf->p_sys->f_adj_oldvalue = 0;
    #undef P_SLIDER

    /* We don't create these ones yet because we perhaps won't need them */
    p_intf->p_sys->p_about = NULL;
    p_intf->p_sys->p_modules = NULL;
    p_intf->p_sys->p_open = NULL;
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
    gtk_object_set_data( GTK_OBJECT( gtk_object_get_data(
                             GTK_OBJECT(p_intf->p_sys->p_popup),
                             "popup_audio" ) ), "p_intf", p_intf );
    gtk_object_set_data( GTK_OBJECT( gtk_object_get_data(
                             GTK_OBJECT(p_intf->p_sys->p_popup),
                             "popup_video" ) ), "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_playwin ),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_messages ),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_adj),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_sout ),
                                     "p_intf", p_intf );

    psz_sout = config_GetPsz( p_intf, "sout" );
    p_target = g_string_new( psz_sout ? psz_sout : "" );
    if( psz_sout ) free( psz_sout );

    gtk_entry_set_text( gtk_object_get_data( GTK_OBJECT( p_intf->p_sys->p_sout ), "sout_entry_target" ), p_target->str );
    g_string_free( p_target, TRUE );

    /* FIXME it's to be sure that only file entry is selected */
    gtk_toggle_button_set_active(  gtk_object_get_data( GTK_OBJECT( p_intf->p_sys->p_sout ),
                                   "sout_access_udp" ), TRUE );

    gtk_toggle_button_set_active(  gtk_object_get_data( GTK_OBJECT( p_intf->p_sys->p_sout ),
                                   "sout_access_file" ), TRUE );

    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );

    while( !p_intf->b_die )
    {
        Manage( p_intf );

        /* Sleep to avoid using all CPU - since some interfaces need to
         * access keyboard events, a 100ms delay is a good compromise */
        gdk_threads_leave();
        msleep( INTF_IDLE_SLEEP );
        gdk_threads_enter();
    }

    /* Destroy the Tooltips structure */
    gtk_object_destroy( GTK_OBJECT(p_intf->p_sys->p_tooltips) );
    gtk_object_destroy( GTK_OBJECT(p_intf->p_sys->p_messages) );
    gtk_object_destroy( GTK_OBJECT(p_intf->p_sys->p_playwin) );
    gtk_object_destroy( GTK_OBJECT(p_intf->p_sys->p_popup) );
    gtk_object_destroy( GTK_OBJECT(p_intf->p_sys->p_window) );

    gdk_threads_leave();
}

/* following functions are local */

/*****************************************************************************
 * Manage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
static void Manage( intf_thread_t *p_intf )
{
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
                                   NULL, NULL, NULL, NULL, NULL );
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

        /* If the messages list becomes too big, just clean half of it. */
        if( gtk_text_get_length( p_intf->p_sys->p_messages_text ) >
            VLC_MSG_QSIZE * 1000 )
        {
            gtk_text_set_point( p_intf->p_sys->p_messages_text, 0 );
            gtk_text_forward_delete( p_intf->p_sys->p_messages_text,
                gtk_text_get_length( p_intf->p_sys->p_messages_text ) / 2 );
        }

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
        aout_instance_t *p_aout  = NULL;
        vout_thread_t   *p_vout  = NULL;
        vlc_bool_t      b_need_menus = VLC_FALSE;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        if( !p_input->b_die )
        {
            /* New input or stream map change */
            if( p_input->stream.b_changed )
            {
                E_(GtkModeManage)( p_intf );
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
                 * finished dragging the slider.
                 * Beware, the hack below is needed by the dvdplay plugin! */
                else if( p_intf->p_sys->b_slider_free
                /* hack -> */ && (p_intf->p_sys->f_adj_oldvalue < 100.) )
                {
                    if( newvalue >= 0. && newvalue < 100. )
                    {
                        double f_fpos = (double)newvalue / 100.0;

                        /* release the lock to be able to seek */
                        vlc_mutex_unlock( &p_input->stream.stream_lock );
                        var_SetFloat( p_input, "position", f_fpos );
                        vlc_mutex_lock( &p_input->stream.stream_lock );

                    }

                    /* Update the old value */
                    p_intf->p_sys->f_adj_oldvalue = newvalue;
                }
#undef p_area
            }

            if( p_intf->p_sys->i_part !=
                p_input->stream.p_selected_area->i_part )
            {
                p_intf->p_sys->b_chapter_update = 1;
                b_need_menus = VLC_TRUE;
            }

            /* Does the audio output require to update the menus ? */
            p_aout = (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                         FIND_ANYWHERE );
            if( p_aout != NULL )
            {
                vlc_value_t val;
                if( var_Get( (vlc_object_t *)p_aout, "intf-change", &val ) >= 0
                    && val.b_bool )
                {
                    p_intf->p_sys->b_aout_update = 1;
                    b_need_menus = 1;
                }

                vlc_object_release( (vlc_object_t *)p_aout );
            }


            /* Does the video output require to update the menus ? */
            p_vout = (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                       FIND_ANYWHERE );
            if( p_vout != NULL ) 
            {
                vlc_value_t val;
                if( var_Get( (vlc_object_t *)p_vout, "intf-change", &val ) >= 0
                    && val.b_bool )
                {
                    p_intf->p_sys->b_vout_update = 1;
                    b_need_menus = 1;
                }

                vlc_object_release( (vlc_object_t *)p_vout );
            }

            if( b_need_menus )
            {
                GtkSetupMenus( p_intf );
            }
        }

        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else if( p_intf->p_sys->b_playing && !p_intf->b_die )
    {
        E_(GtkModeManage)( p_intf );
        p_intf->p_sys->b_playing = 0;
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return;
}

