/*****************************************************************************
 * gnome2.c : GNOME 2 plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale Réseaux) and its contributors
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

#include "gnome2_interface.h"
#include "gnome2_support.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static void Run          ( intf_thread_t * );
static int  Manage       ( intf_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    int i = getenv( "DISPLAY" ) == NULL ? 15 : 95;
    set_description( _("Gtk2 interface") );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );
    set_capability( "interface", i );
    set_callbacks( Open, Close );
    set_program( "gvlc" );
vlc_module_end();

/*****************************************************************************
 * intf_sys_t
 *****************************************************************************/
struct intf_sys_t
{
    module_t *p_gui_helper;

    GtkWidget *p_app;
};

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

#ifdef NEED_GTK2_MAIN
    p_intf->p_sys->p_gui_helper =
        module_Need( p_this, "gui-helper", "gnome2", VLC_TRUE );
    if( p_intf->p_sys->p_gui_helper == NULL )
    {
        free( p_intf->p_sys );
        return VLC_ENOMOD;
    }
#endif

    p_intf->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

#ifdef NEED_GTK2_MAIN
    module_Unneed( p_intf, p_intf->p_sys->p_gui_helper );
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
#ifdef NEED_GTK2_MAIN
    gdk_threads_enter();
#else
    /* gnome_program_init needs to know the command line. We don't care, so
     * we give it an empty one */
    char  *p_args[] = { "", NULL };
    int    i_args   = 1;
    int    i_dummy;

    gtk_set_locale();
    gnome_program_init( PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                        i_args, p_args,
                        GNOME_PARAM_APP_DATADIR, "",//PACKAGE_DATA_DIR,
                        NULL );
#endif

    /* Create some useful widgets that will certainly be used */
    p_intf->p_sys->p_app = create_app1();

    /* Set the title of the main window */
    //gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_app),
    //                      VOUT_TITLE " (Gtk+ interface)" );

    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_app );

#ifdef NEED_GTK2_MAIN
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
    /* Sleep to avoid using all CPU - since some interfaces needs to access
     * keyboard events, a 100ms delay is a good compromise */
    i_dummy = gtk_timeout_add( INTF_IDLE_SLEEP / 1000, (GtkFunction)Manage,
                               p_intf );
    /* Enter Gtk mode */
    gtk_main();
    /* Remove the timeout */
    gtk_timeout_remove( i_dummy );
#endif

    gtk_object_destroy( GTK_OBJECT(p_intf->p_sys->p_app) );

#ifdef NEED_GTK2_MAIN
    gdk_threads_leave();
#endif
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
#ifndef NEED_GTK2_MAIN
    if( p_intf->b_die )
    {
        gtk_main_quit();

        return FALSE;
    }
#endif

    return TRUE;
}
