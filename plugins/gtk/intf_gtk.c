/*****************************************************************************
 * intf_gtk.c: Gtk+ interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_gtk.c,v 1.9 2001/03/09 19:38:47 octplane Exp $
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

#include "intf_msg.h"
#include "interface.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_sys.h"

#include "main.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe      ( probedata_t *p_data );
static int  intf_Open       ( intf_thread_t *p_intf );
static void intf_Close      ( intf_thread_t *p_intf );
static void intf_Run        ( intf_thread_t *p_intf );

static gint GtkManage       ( gpointer p_data );
static gint GtkLanguageMenus( gpointer, GtkWidget *, es_descriptor_t *, gint,
                              void (*pf_activate)(GtkMenuItem *, gpointer) );
static gint GtkChapterMenu  ( gpointer, GtkWidget *,
                              void (*pf_activate)(GtkMenuItem *, gpointer) );
static gint GtkTitleMenu    ( gpointer, GtkWidget *, 
                              void (*pf_activate)(GtkMenuItem *, gpointer) );
void GtkPlayListManage( gpointer p_data );


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

    p_intf->p_sys->b_menus_update = 1;
    p_intf->p_sys->b_scale_isfree = 1;


    p_intf->p_sys->i_playing = -1;

    p_intf->p_sys->pf_gtk_callback = NULL;
    p_intf->p_sys->pf_gdk_callback = NULL;

    /* Initialize lock */
    vlc_mutex_init( &p_intf->p_sys->change_lock );

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface window
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy lock */
    vlc_mutex_destroy( &p_intf->p_sys->change_lock );

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
    char *p_args[] = { "" };
    char **pp_args = p_args;
    int i_args = 1;
    GtkWidget * temp;

    /* The data types we are allowed to receive */
    static GtkTargetEntry target_table[] =
    {
        { "text/uri-list", 0, DROP_ACCEPT_TEXT_URI_LIST },
        { "text/plain", 0, DROP_ACCEPT_TEXT_PLAIN }
    };

    /* Initialize Gtk+ */
    gtk_init( &i_args, &pp_args );

    /* Create some useful widgets that will certainly be used */
    p_intf->p_sys->p_window = create_intf_window( );
    p_intf->p_sys->p_popup = create_intf_popup( );
    p_intf->p_sys->p_playlist = create_intf_playlist( );

    
    /* Set the title of the main window */
    gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_window),
                          VOUT_TITLE " (Gtk+ interface)");

    /* Accept file drops on the main window */
    gtk_drag_dest_set( GTK_WIDGET( p_intf->p_sys->p_window ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       1, GDK_ACTION_COPY );

    /* Accept file drops on the playlist window */
    temp = lookup_widget(p_intf->p_sys->p_playlist, "playlist_clist"); 
    
    
    gtk_drag_dest_set( GTK_WIDGET( temp ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       1, GDK_ACTION_COPY );

    /* We don't create these ones yet because we perhaps won't need them */
    p_intf->p_sys->p_about = NULL;
    p_intf->p_sys->p_modules = NULL;
    p_intf->p_sys->p_fileopen = NULL;
    p_intf->p_sys->p_disc = NULL;

    /* Store p_intf to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_popup),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_playlist),
                         "p_intf", p_intf );

 

    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );


    /* Sleep to avoid using all CPU - since some interfaces needs to access
     * keyboard events, a 100ms delay is a good compromise */
    p_intf->p_sys->i_timeout = gtk_timeout_add( INTF_IDLE_SLEEP / 1000,
                                                GtkManage, p_intf );
 

    /* Enter Gtk mode */
    gtk_main();

    /* launch stored callbacks */
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
    intf_thread_t *p_intf = (void *)p_data;

    GtkPlayListManage( p_data ); 

    vlc_mutex_lock( &p_intf->p_sys->change_lock );

    
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

    /* Update language/chapter menus after user request */
    if( p_intf->p_input != NULL && p_intf->p_sys->p_window != NULL &&
        p_intf->p_sys->b_menus_update )
    {
        es_descriptor_t *   p_audio_es;
        es_descriptor_t *   p_spu_es;
        GtkWidget *         p_menubar_menu;
        GtkWidget *         p_popup_menu;
        gint                i;

        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                     p_intf->p_sys->p_window ), "menubar_title" ) );

        GtkTitleMenu( p_intf, p_menubar_menu, on_menubar_title_activate );

        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                     p_intf->p_sys->p_window ), "menubar_chapter" ) );

        GtkChapterMenu( p_intf, p_menubar_menu, on_menubar_chapter_activate );

        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                     p_intf->p_sys->p_popup ), "popup_navigation" ) );

        GtkTitleMenu( p_intf, p_popup_menu, on_popup_navigation_activate );
    
        /* look for selected ES */
        p_audio_es = NULL;
        p_spu_es = NULL;

        for( i = 0 ; i < p_intf->p_input->stream.i_selected_es_number ; i++ )
        {
            if( p_intf->p_input->stream.pp_es[i]->b_audio )
            {
                p_audio_es = p_intf->p_input->stream.pp_es[i];
            }
    
            if( p_intf->p_input->stream.pp_es[i]->b_spu )
            {
                p_spu_es = p_intf->p_input->stream.pp_es[i];
            }
        }

        /* audio menus */

        /* find audio root menu */
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                             p_intf->p_sys->p_window ), "menubar_audio" ) );

        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                     p_intf->p_sys->p_popup ), "popup_audio" ) );

        GtkLanguageMenus( p_intf, p_menubar_menu, p_audio_es, 1,
                          on_menubar_audio_activate );
        GtkLanguageMenus( p_intf, p_popup_menu, p_audio_es, 1,
                          on_popup_audio_activate );

        /* sub picture menus */

        /* find spu root menu */
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                          p_intf->p_sys->p_window ), "menubar_subpictures" ) );

        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                     p_intf->p_sys->p_popup ), "popup_subpictures" ) );

        GtkLanguageMenus( p_intf, p_menubar_menu, p_spu_es, 2,
                          on_menubar_subpictures_activate  );
        GtkLanguageMenus( p_intf, p_popup_menu, p_spu_es, 2,
                          on_popup_subpictures_activate );

        /* everything is ready */
        p_intf->p_sys->b_menus_update = 0;
    }

    /* Manage the slider */
    if( p_intf->p_input != NULL && p_intf->p_sys->p_window != NULL
         && p_intf->p_sys->b_scale_isfree )
    {
        GtkWidget *p_scale;
        GtkAdjustment *p_adj;
   
        p_scale = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                                  p_intf->p_sys->p_window ), "hscale" ) );
        p_adj = gtk_range_get_adjustment ( GTK_RANGE( p_scale ) );

        /* Update the value */
        p_adj->value = ( 100. *
                         p_intf->p_input->stream.p_selected_area->i_tell ) /
                         p_intf->p_input->stream.p_selected_area->i_size;

        /* Gtv does it this way. Why not. */
        gtk_range_set_adjustment ( GTK_RANGE( p_scale ), p_adj );
        gtk_range_slider_update ( GTK_RANGE( p_scale ) );
        gtk_range_clear_background ( GTK_RANGE( p_scale ) );
        gtk_range_draw_background ( GTK_RANGE( p_scale ) );
    }


    /* Manage core vlc functions through the callback */
    p_intf->pf_manage( p_intf );

    if( p_intf->b_die )
    {
        /* Make sure we won't be called again */
        gtk_timeout_remove( p_intf->p_sys->i_timeout );

        vlc_mutex_unlock( &p_intf->p_sys->change_lock );

        /* Prepare to die, young Skywalker */
        gtk_main_quit();
        return( FALSE );
    }

    vlc_mutex_unlock( &p_intf->p_sys->change_lock );

    return( TRUE );
}

/*****************************************************************************
 * GtkMenuRadioItem: give a menu item adapted to language/title selection,
 * ie the menu item is a radio button.
 *****************************************************************************/
static GtkWidget * GtkMenuRadioItem( GtkWidget * p_menu,
                                     GSList **   p_button_group,
                                     gint        b_active,
                                     char *      psz_name )
{
    GtkWidget *     p_item;

#if 0
    GtkWidget *     p_button;

    /* create button */
    p_button =
        gtk_radio_button_new_with_label( *p_button_group, psz_name );

    /* add button to group */
    *p_button_group =
        gtk_radio_button_group( GTK_RADIO_BUTTON( p_button ) );

    /* prepare button for display */
    gtk_widget_show( p_button );

    /* create menu item to store button */
    p_item = gtk_menu_item_new();

    /* put button inside item */
    gtk_container_add( GTK_CONTAINER( p_item ), p_button );

    /* add item to menu */
    gtk_menu_append( GTK_MENU( p_menu ), p_item );

          gtk_signal_connect( GTK_OBJECT( p_item ), "activate",
               GTK_SIGNAL_FUNC( on_audio_toggle ),
               NULL );


    /* prepare item for display */
    gtk_widget_show( p_item );

    /* is it the selected item ? */
    if( b_active )
    {
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( p_button ), TRUE );
    }
#else
    p_item = gtk_menu_item_new_with_label( psz_name );
    gtk_menu_append( GTK_MENU( p_menu ), p_item );
    gtk_widget_show( p_item );
#endif

    return p_item;
}

/*****************************************************************************
 * GtkLanguageMenus: update interactive menus of the interface
 *****************************************************************************
 * Sets up menus with information from input:
 *  -languages
 *  -sub-pictures
 * Warning: since this function is designed to be called by management
 * function, the interface lock has to be taken
 *****************************************************************************/
static gint GtkLanguageMenus( gpointer          p_data,
                              GtkWidget *       p_root,
                              es_descriptor_t * p_es,
                              gint              i_type,
                        void(*pf_activate )( GtkMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    GtkWidget *         p_menu;
    GtkWidget *         p_separator;
    GtkWidget *         p_item;
    GSList *            p_button_group;
    char *              psz_name;
    gint                b_active;
    gint                b_audio;
    gint                b_spu;
    gint                i;

    

    /* cast */
    p_intf = (intf_thread_t *)p_data;

    vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );

    b_audio = ( i_type == 1 );
    p_button_group = NULL;

    /* menu container for audio */
    p_menu = gtk_menu_new();

    /* create a set of language buttons and append them to the container */
    b_active = ( p_es == NULL ) ? 1 : 0;
    psz_name = "Off";

    p_item = GtkMenuRadioItem( p_menu, &p_button_group, b_active, psz_name );

    /* setup signal hanling */
    gtk_signal_connect( GTK_OBJECT( p_item ), "activate",
            GTK_SIGNAL_FUNC ( pf_activate ), NULL );

    p_separator = gtk_menu_item_new();
    gtk_widget_show( p_separator );
    gtk_menu_append( GTK_MENU( p_menu ), p_separator );
    gtk_widget_set_sensitive( p_separator, FALSE );

    for( i = 0 ; i < p_intf->p_input->stream.i_es_number ; i++ )
    {

        b_audio = ( i_type == 1 ) && p_intf->p_input->stream.pp_es[i]->b_audio;
        b_spu   = ( i_type == 2 ) && p_intf->p_input->stream.pp_es[i]->b_spu;

        if( b_audio || b_spu )
        {
            b_active = ( p_es == p_intf->p_input->stream.pp_es[i] ) ? 1 : 0;
            psz_name = p_intf->p_input->stream.pp_es[i]->psz_desc;

            p_item = GtkMenuRadioItem( p_menu, &p_button_group,
                                       b_active, psz_name );

            /* setup signal hanling */
            gtk_signal_connect( GTK_OBJECT( p_item ), "activate",
               GTK_SIGNAL_FUNC( pf_activate ),
                 (gpointer)( p_intf->p_input->stream.pp_es[i] ) );

        }
    }

    /* link the new menu to the menubar item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_root ), p_menu );

    /* be sure that menu is sensitive */
    gtk_widget_set_sensitive( p_root, TRUE );

    vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );

    return TRUE;
}

/*****************************************************************************
 * GtkChapterMenu: generate chapter menu for current title
 *****************************************************************************/
static gint GtkChapterMenu( gpointer p_data, GtkWidget * p_chapter,
                        void(*pf_activate )( GtkMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    char                psz_name[10];
    GtkWidget *         p_chapter_menu;
    GtkWidget *         p_item;
    GSList *            p_chapter_button_group;
    gint                i_title;
    gint                i_chapter;
    gint                b_active;

    /* cast */
    p_intf = (intf_thread_t*)p_data;

    i_title = p_intf->p_input->stream.p_selected_area->i_id;
    p_chapter_menu = gtk_menu_new();

    for( i_chapter = 0;
         i_chapter < p_intf->p_input->stream.pp_areas[i_title]->i_part_nb ;
         i_chapter++ )
    {
        b_active = ( p_intf->p_input->stream.pp_areas[i_title]->i_part
                     == i_chapter + 1 ) ? 1 : 0;
        
        sprintf( psz_name, "Chapter %d", i_chapter + 1 );

        p_item = GtkMenuRadioItem( p_chapter_menu, &p_chapter_button_group,
                                   b_active, psz_name );
        /* setup signal hanling */
        gtk_signal_connect( GTK_OBJECT( p_item ),
                        "activate",
                        GTK_SIGNAL_FUNC( pf_activate ),
                        (gpointer)(i_chapter + 1) );
    }

    /* link the new menu to the title menu item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_chapter ),
                               p_chapter_menu );

    /* be sure that chapter menu is sensitive */
    gtk_widget_set_sensitive( p_chapter, TRUE );

    return TRUE;
}

/*****************************************************************************
 * GtkTitleMenu: sets menus for titles and chapters selection
 *****************************************************************************
 * Generates two type of menus:
 *  -simple list of titles
 *  -cascaded lists of chapters for each title
 *****************************************************************************/
static gint GtkTitleMenu( gpointer       p_data,
                          GtkWidget *    p_navigation, 
                          void(*pf_activate )( GtkMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    char                psz_name[10];
    GtkWidget *         p_title_menu;
    GtkWidget *         p_title_item;
    GtkWidget *         p_chapter_menu;
    GtkWidget *         p_item;
    GSList *            p_title_button_group;
    GSList *            p_chapter_button_group;
    gint                i_title;
    gint                i_chapter;
    gint                b_active;

    /* cast */
    p_intf = (intf_thread_t*)p_data;

    p_title_menu = gtk_menu_new();
    p_title_button_group = NULL;
    p_chapter_button_group = NULL;

    /* loop on titles */
    for( i_title = 1 ;
         i_title < p_intf->p_input->stream.i_area_nb ;
         i_title++ )
    {
        b_active = ( p_intf->p_input->stream.pp_areas[i_title] ==
                     p_intf->p_input->stream.p_selected_area ) ? 1 : 0;
        sprintf( psz_name, "Title %d", i_title );

        p_title_item = GtkMenuRadioItem( p_title_menu, &p_title_button_group,
                                         b_active, psz_name );

        if( pf_activate == on_menubar_title_activate )
        {
            /* setup signal hanling */
            gtk_signal_connect( GTK_OBJECT( p_title_item ),
                     "activate",
                     GTK_SIGNAL_FUNC( pf_activate ),
                     (gpointer)(p_intf->p_input->stream.pp_areas[i_title]) );
        }
        else
        {
            p_chapter_menu = gtk_menu_new();
    
            for( i_chapter = 0;
                 i_chapter <
                        p_intf->p_input->stream.pp_areas[i_title]->i_part_nb ;
                 i_chapter++ )
            {
                b_active = ( p_intf->p_input->stream.pp_areas[i_title]->i_part
                             == i_chapter + 1 ) ? 1 : 0;
                
                sprintf( psz_name, "Chapter %d", i_chapter + 1 );
    
                p_item = GtkMenuRadioItem( p_chapter_menu,
                                           &p_chapter_button_group,
                                           b_active, psz_name );
    
                /* setup signal hanling */
                gtk_signal_connect( GTK_OBJECT( p_item ),
                           "activate",
                           GTK_SIGNAL_FUNC( pf_activate ),
                           (gpointer)( ( i_title * 100 ) + ( i_chapter + 1) ) );
        }

        /* link the new menu to the title menu item */
        gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_title_item ),
                                   p_chapter_menu );
        }

        /* be sure that chapter menu is sensitive */
        gtk_widget_set_sensitive( p_title_menu, TRUE );

    }

    /* link the new menu to the menubar audio item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_navigation ), p_title_menu );

    /* be sure that audio menu is sensitive */
    gtk_widget_set_sensitive( p_navigation, TRUE );


    return TRUE;
}
