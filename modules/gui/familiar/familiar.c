/*****************************************************************************
 * familiar.c : familiar plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: familiar.c,v 1.3 2002/08/14 21:50:01 jpsaman Exp $
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

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "familiar.h"

/*****************************************************************************
 * Local variables (mutex-protected).
 *****************************************************************************/
static void ** pp_global_data = NULL;

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
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );             

static void Run          ( intf_thread_t * );                  

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
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
        return( 1 );
    }

    /* Initialize Gtk+ thread */
    p_intf->p_sys->p_input = NULL;

    p_intf->pf_run = Run;

    return( 0 );
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

    /* Destroy structure */
    if (p_intf->p_sys) free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: Gtk+ thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * gtk_main() from within it without annoying the rest of the program.
 * XXX: the approach may look kludgy, and probably is, but I could not find
 * a better way to dynamically load a Gtk+ interface at runtime.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    /* gtk_init needs to know the command line. We don't care, so we
     * give it an empty one */
    char  *p_args[] = { "" };
    char **pp_args  = p_args;
    int    i_args   = 1;
    int    i_dummy  = 0;

    /* Initialize Gtk+ */
    gtk_set_locale ();

    /* gtk_init will register stuff with g_atexit, so we need to take
     * the global lock if we want to be able to intercept the calls */
    vlc_mutex_lock( p_intf->p_vlc->p_global_lock );
    *p_intf->p_vlc->pp_global_data = p_intf;
    gtk_init( &i_args, &pp_args );
    vlc_mutex_unlock( p_intf->p_vlc->p_global_lock );

    /* Create some useful widgets that will certainly be used */
// FIXME: magic path
    add_pixmap_directory("share");
    p_intf->p_sys->p_window = create_familiar();
    if (p_intf->p_sys->p_window == NULL)
    {
        msg_Err( p_intf, "unable to create familiar interface" );
    }

    /* Set the title of the main window */
    gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_window),
                          VOUT_TITLE " (Familiar Linux interface)");

    /* Get the slider object */
    p_intf->p_sys->p_notebook = GTK_NOTEBOOK( gtk_object_get_data(
        GTK_OBJECT( p_intf->p_sys->p_window ), "notebook" ) );
//    gtk_widget_hide( GTK_WIDGET(p_intf->p_sys->p_notebook) );

    /* Store p_intf to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         "p_intf", p_intf );
    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );

    /* Enter Gtk mode */
    gtk_main();

    /* Remove the timeout */
    gtk_timeout_remove( i_dummy );
}

