/*****************************************************************************
 * gtk_main.c : Gtk+ wrapper for gtk_main
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: gtk_main.c,v 1.1 2002/08/20 18:08:51 sam Exp $
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

#ifdef HAVE_GNOME_H
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

    /* XXX: Ugly kludge, see g_atexit */
    void ( *pf_callback[MAX_ATEXIT] ) ( void );

} gtk_main_t;

/*****************************************************************************
 * Local variables (mutex-protected).
 *****************************************************************************/
static void **       pp_global_data = NULL;
static int           i_refcount = 0;
static gtk_main_t *  p_gtk_main = NULL;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    pp_global_data = p_module->p_vlc->pp_global_data;
    set_description( _("Gtk+ helper module") );
    set_capability( "gtk_main", 100 );
    add_shortcut( "gtk" );
#ifdef HAVE_GNOME_H
    add_shortcut( "gnome" );
#endif
    set_callbacks( Open, Close );
vlc_module_end();

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
    gtk_main_t *p_this;

    int i_dummy;

    if( pp_global_data == NULL )
    {
        atexit( func );
        return;
    }

    p_this = (gtk_main_t *)*pp_global_data;
    if( p_this == NULL )
    {
        /* Looks like this atexit() call wasn't for us. */
        return;
    }

    for( i_dummy = 0;
         i_dummy < MAX_ATEXIT && p_this->pf_callback[i_dummy] != NULL;
         i_dummy++ )
    {
        ;
    }

    if( i_dummy >= MAX_ATEXIT - 1 )
    {
        msg_Err( p_this, "too many atexit() callbacks to register" );
        return;
    }

    p_this->pf_callback[i_dummy]     = func;
    p_this->pf_callback[i_dummy + 1] = NULL;
}

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    /* Initialize Gtk+ */

    vlc_mutex_lock( p_this->p_vlc->p_global_lock );

    if( i_refcount > 0 )
    {
        i_refcount++;
        vlc_mutex_unlock( p_this->p_vlc->p_global_lock );

        return VLC_SUCCESS;
    }

    p_gtk_main = vlc_object_create( p_this, sizeof(gtk_main_t) );
    p_gtk_main->pf_callback[0] = NULL;

    /* Only initialize gthreads if it's the first time we do it */
    if( !g_thread_supported() )
    {
        g_thread_init( NULL );
    }

    /* Launch the gtk_main() thread. It will not return until it has
     * called gdk_threads_enter(), which ensures us thread safety. */
    if( vlc_thread_create( p_gtk_main, "gtk_main", GtkMain, VLC_TRUE ) )
    {
        vlc_object_destroy( p_gtk_main );
        i_refcount--;
        vlc_mutex_unlock( p_this->p_vlc->p_global_lock );
        return VLC_ETHREAD;
    }

    i_refcount++;
    vlc_mutex_unlock( p_this->p_vlc->p_global_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    int i_dummy;

    vlc_mutex_lock( p_this->p_vlc->p_global_lock );

    i_refcount--;

    if( i_refcount > 0 )
    {
        vlc_mutex_unlock( p_this->p_vlc->p_global_lock );
        return;
    }

    gtk_main_quit();
    vlc_thread_join( p_gtk_main );

    /* Launch stored callbacks */
    for( i_dummy = 0;
         i_dummy < MAX_ATEXIT && p_gtk_main->pf_callback[i_dummy] != NULL;
         i_dummy++ )
    {
        p_gtk_main->pf_callback[i_dummy]();
    }

    vlc_object_destroy( p_gtk_main );
    p_gtk_main = NULL;

    vlc_mutex_unlock( p_this->p_vlc->p_global_lock );
}

static gint foo(gpointer foo)
{
    return TRUE;
}

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
    static char **pp_args  = p_args;
    static int    i_args   = 1;

    /* gtk_init will register stuff with g_atexit, so we need to have
     * the global lock if we want to be able to intercept the calls */
    *p_this->p_vlc->pp_global_data = p_gtk_main;

    /* FIXME: deprecated ? */
    /* gdk_threads_init(); */

#ifdef HAVE_GNOME_H
    gnome_init( p_this->p_vlc->psz_object_name, VERSION, i_args, p_args );
#else
    gtk_set_locale();
    gtk_init( &i_args, &pp_args );
#endif

    gdk_threads_enter();

    vlc_thread_ready( p_this );

    /* If we don't add this simple timeout, gtk_main remains stuck ... */
    gtk_timeout_add( INTF_IDLE_SLEEP / 1000, foo, p_this );

    /* Enter Gtk mode */
    gtk_main();

    gdk_threads_leave();
}

