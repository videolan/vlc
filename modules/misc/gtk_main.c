/*****************************************************************************
 * gtk_main.c : Gtk+ wrapper for gtk_main
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: gtk_main.c,v 1.8 2002/10/04 13:13:54 sam Exp $
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
#include <vlc/vlc.h>

#include <stdlib.h>                                              /* atexit() */

#include <gtk/gtk.h>

#ifdef MODULE_NAME_IS_gnome_main
#   include <gnome.h>
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static void GtkMain ( vlc_object_t * );

/*****************************************************************************
 * The gtk_main_t object.
 *****************************************************************************/
#define MAX_ATEXIT 10

typedef struct gtk_main_t
{
    VLC_COMMON_MEMBERS

} gtk_main_t;

/*****************************************************************************
 * Local variables (mutex-protected).
 *****************************************************************************/
static int           i_refcount = 0;
static gtk_main_t *  p_gtk_main = NULL;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Gtk+ helper module") );
#ifdef MODULE_NAME_IS_gtk_main
    set_capability( "gtk_main", 90 );
#else
    set_capability( "gtk_main", 100 );
    add_shortcut( "gnome" );
#endif
    add_shortcut( "gtk" );
    set_callbacks( Open, Close );
    linked_with_a_crap_library_which_uses_atexit();
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    vlc_mutex_lock( &p_this->p_libvlc->global_lock );

    if( i_refcount > 0 )
    {
        i_refcount++;
        vlc_mutex_unlock( &p_this->p_libvlc->global_lock );

        return VLC_SUCCESS;
    }

    p_gtk_main = vlc_object_create( p_this, sizeof(gtk_main_t) );

    /* Only initialize gthreads if it's the first time we do it */
    if( !g_thread_supported() )
    {
        g_thread_init( NULL );
    }

    /* Launch the gtk_main() thread. It will not return until it has
     * called gdk_threads_enter(), which ensures us thread safety. */
    if( vlc_thread_create( p_gtk_main, "gtk_main", GtkMain,
                           VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        vlc_object_destroy( p_gtk_main );
        i_refcount--;
        vlc_mutex_unlock( &p_this->p_libvlc->global_lock );
        return VLC_ETHREAD;
    }

    i_refcount++;
    vlc_mutex_unlock( &p_this->p_libvlc->global_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    vlc_mutex_lock( &p_this->p_libvlc->global_lock );

    i_refcount--;

    if( i_refcount > 0 )
    {
        vlc_mutex_unlock( &p_this->p_libvlc->global_lock );
        return;
    }

    gtk_main_quit();
    vlc_thread_join( p_gtk_main );

    vlc_object_destroy( p_gtk_main );
    p_gtk_main = NULL;

    vlc_mutex_unlock( &p_this->p_libvlc->global_lock );
}

static gint foo( gpointer bar ) { return TRUE; }

/*****************************************************************************
 * GtkMain: Gtk+ thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * gtk_main() from within it without annoying the rest of the program.
 *****************************************************************************/
static void GtkMain( vlc_object_t *p_this )
{
    /* gtk_init needs to know the command line. We don't care, so we
     * give it an empty one */
    static char  *p_args[] = { "" };
#ifdef MODULE_NAME_IS_gtk_main
    static char **pp_args  = p_args;
#endif
    static int    i_args   = 1;

    /* FIXME: deprecated ? */
    /* gdk_threads_init(); */

#ifdef MODULE_NAME_IS_gnome_main
    gnome_init( p_this->p_vlc->psz_object_name, VERSION, i_args, p_args );
#else
    gtk_set_locale();
    gtk_init( &i_args, &pp_args );
#endif

    gdk_threads_enter();

    vlc_thread_ready( p_this );

    /* If we don't add this simple timeout, gtk_main remains stuck if
     * we try to close the window without having sent any gtk event. */
    gtk_timeout_add( INTF_IDLE_SLEEP / 1000, foo, p_this );

    /* Enter Gtk mode */
    gtk_main();

    gdk_threads_leave();
}

