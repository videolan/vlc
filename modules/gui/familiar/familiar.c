/*****************************************************************************
 * familiar.c : familiar plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: familiar.c,v 1.15 2002/12/15 22:45:35 jpsaman Exp $
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

void GtkAutoPlayFile( vlc_object_t * );

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
    p_intf->p_sys->b_filelist_update = 0;
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
#ifdef HAVE_GPE_INIT_H
    char  *p_args[] = { "" };
    char **pp_args  = p_args;
    int    i_args   = 1;

    /* Initialize GPE interface */
    if (gpe_application_init(&i_args, &pp_args) == FALSE)
        exit (1);
#else
# ifdef NEED_GTK_MAIN
    /* Initialize Gtk+ */
    gtk_set_locale ();
    gdk_threads_enter();
# else
    /* gtk_init needs to know the command line. We don't care, so we
     * give it an empty one */
    char  *p_args[] = { "" };
    char **pp_args  = p_args;
    int    i_args   = 1;
    int    i_dummy;

    gtk_set_locale ();
    gtk_init( &i_args, &pp_args );
# endif
#endif
    /* Create some useful widgets that will certainly be used */

// FIXME: magic path
    add_pixmap_directory("share");
    add_pixmap_directory("/usr/share/videolan");

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
//    gtk_widget_hide( GTK_WIDGET(p_intf->p_sys->p_notebook) );

    p_intf->p_sys->p_progess = GTK_PROGRESS_BAR( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_window ), "progress" ) );
    gtk_widget_hide( GTK_WIDGET(p_intf->p_sys->p_progess) );

    p_intf->p_sys->p_clist = GTK_CLIST( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_window ), "clistmedia" ) );
    gtk_clist_set_column_visibility (GTK_CLIST (p_intf->p_sys->p_clist), 2, FALSE);
    gtk_clist_set_column_visibility (GTK_CLIST (p_intf->p_sys->p_clist), 3, FALSE);
    gtk_clist_set_column_visibility (GTK_CLIST (p_intf->p_sys->p_clist), 4, FALSE);
    gtk_clist_column_titles_show (GTK_CLIST (p_intf->p_sys->p_clist));

    /* Store p_intf to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         "p_intf", p_intf );
    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );
    ReadDirectory(p_intf->p_sys->p_clist, "/mnt");

#ifdef NEED_GTK_MAIN
    /* Sleep to avoid using all CPU - since some interfaces need to
     * access keyboard events, a 100ms delay is a good compromise */
    while( !p_intf->b_die )
    {
        gdk_threads_leave();
        msleep( INTF_IDLE_SLEEP );
        gdk_threads_enter();
    }
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
    vlc_list_t list = vlc_list_find( p_this, VLC_OBJECT_INTF,
                                                FIND_ANYWHERE );

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

