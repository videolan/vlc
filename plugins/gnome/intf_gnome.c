/*****************************************************************************
 * intf_gnome.c: Gnome interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_gnome.c,v 1.26 2001/04/08 07:24:47 stef Exp $
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

#define MODULE_NAME gnome
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <gnome.h>

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
#include "intf_playlist.h"

#include "gnome_callbacks.h"
#include "gnome_interface.h"
#include "gnome_support.h"
#include "intf_gnome.h"

#include "main.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe     ( probedata_t *p_data );
static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

static gint GnomeManage    ( gpointer p_data );
static gint GnomeLanguageMenus( gpointer, GtkWidget *, es_descriptor_t *, gint,
                              void (*pf_toggle)(GtkCheckMenuItem *, gpointer) );
static gint GnomeChapterMenu  ( gpointer, GtkWidget *,
                              void (*pf_toggle)(GtkCheckMenuItem *, gpointer) );
static gint GnomeTitleMenu    ( gpointer, GtkWidget *, 
                              void (*pf_toggle)(GtkCheckMenuItem *, gpointer) );
static gint GnomeSetupMenu    ( intf_thread_t * p_intf );
static void GnomeDisplayDate  ( GtkAdjustment *p_adj );
static gint GnomeDVDModeManage( intf_thread_t * p_intf );
static gint GnomeFileModeManage( intf_thread_t * p_intf );
static gint GnomeNetworkModeManage( intf_thread_t * p_intf );

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
 * This function tries to initialize Gnome and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int intf_Probe( probedata_t *p_data )
{
    if( TestMethod( INTF_METHOD_VAR, "gnome" ) )
    {
        return( 999 );
    }

    return( 100 );
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

    /* Initialize Gnome thread */
    p_intf->p_sys->b_popup_changed = 0;
    p_intf->p_sys->b_window_changed = 0;
    p_intf->p_sys->b_playlist_changed = 0;
    p_intf->p_sys->b_menus_update = 1;

    p_intf->p_sys->b_slider_free = 1;

    p_intf->p_sys->b_mode_changed = 1;
    p_intf->p_sys->i_intf_mode = FILE_MODE;

    p_intf->p_sys->i_part = 0;

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

    /* The data types we are allowed to receive */
    static GtkTargetEntry target_table[] =
    {
        { "text/uri-list", 0, DROP_ACCEPT_TEXT_URI_LIST },
        { "text/plain",    0, DROP_ACCEPT_TEXT_PLAIN }
    };

    /* intf_Manage callback timeout */
    int i_timeout;

    /* Initialize Gnome */
    gnome_init( p_main->psz_arg0, VERSION, i_args, p_args );

    /* Create some useful widgets that will certainly be used */
    p_intf->p_sys->p_window = create_intf_window( );
    p_intf->p_sys->p_popup = create_intf_popup( );
    p_intf->p_sys->p_disc = create_intf_disc( );
    p_intf->p_sys->p_network = create_intf_network( );

    /* Set the title of the main window */
    gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_window),
                          VOUT_TITLE " (Gnome interface)");

    /* Accept file drops on the main window */
    gtk_drag_dest_set( GTK_WIDGET( p_intf->p_sys->p_window ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       1, GDK_ACTION_COPY );

    /* Get the interface labels */
    #define P_LABEL( name ) GTK_LABEL( gtk_object_get_data( \
                         GTK_OBJECT( p_intf->p_sys->p_window ), name ) )
    p_intf->p_sys->p_label_date = P_LABEL( "label_date" );
    p_intf->p_sys->p_label_status = P_LABEL( "label_status" );
    p_intf->p_sys->p_label_title = P_LABEL( "label_title" );
    p_intf->p_sys->p_label_chapter = P_LABEL( "label_chapter" );
    #undef P_LABEL

    /* Connect the date display to the slider */
    #define P_SLIDER GTK_RANGE( gtk_object_get_data( \
                         GTK_OBJECT( p_intf->p_sys->p_window ), "slider" ) )
    p_intf->p_sys->p_adj = gtk_range_get_adjustment( P_SLIDER );

    gtk_signal_connect ( GTK_OBJECT( p_intf->p_sys->p_adj ), "value_changed",
                         GTK_SIGNAL_FUNC( GnomeDisplayDate ), NULL );
    p_intf->p_sys->f_adj_oldvalue = 0;
    #undef P_SLIDER

    /* We don't create these ones yet because we perhaps won't need them */
    p_intf->p_sys->p_about = NULL;
    p_intf->p_sys->p_playlist = NULL;
    p_intf->p_sys->p_modules = NULL;
    p_intf->p_sys->p_fileopen = NULL;

    /* Store p_intf to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_popup),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_disc),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_network),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_adj),
                         "p_intf", p_intf );

    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );

    /* Sleep to avoid using all CPU - since some interfaces needs to access
     * keyboard events, a 100ms delay is a good compromise */
    i_timeout = gtk_timeout_add( INTF_IDLE_SLEEP / 1000, GnomeManage, p_intf );

    /* Enter gnome mode */
    gtk_main();

    /* Remove the timeout */
    gtk_timeout_remove( i_timeout );

    /* Get rid of stored callbacks so we can unload the plugin */
    if( p_intf->p_sys->pf_gtk_callback != NULL )
    {
        p_intf->p_sys->pf_gtk_callback( );
        p_intf->p_sys->pf_gtk_callback = NULL;

    }

    if( p_intf->p_sys->pf_gdk_callback != NULL )
    {
        p_intf->p_sys->pf_gdk_callback( );
        p_intf->p_sys->pf_gdk_callback = NULL;
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

    vlc_mutex_lock( &p_intf->change_lock );

    /* If the "display popup" flag has changed */
    if( p_intf->b_menu_change )
    {
        gnome_popup_menu_do_popup( p_intf->p_sys->p_popup,
                                   NULL, NULL, NULL, NULL );
        p_intf->b_menu_change = 0;
    }

    if( p_intf->p_sys->b_mode_changed )
    {
        /* Sets the interface mode according to playlist item */
        if( p_main->p_playlist->p_item != NULL )
        {
            if( !strncmp( p_main->p_playlist->p_item->psz_name, "dvd:", 4 ) )
            {
                p_intf->p_sys->i_intf_mode = DVD_MODE;
            }
            else if( !strncmp(
                        p_main->p_playlist->p_item->psz_name, "ts:", 4 ) )
            {
                p_intf->p_sys->i_intf_mode = NET_MODE;
            }
        }

        switch( p_intf->p_sys->i_intf_mode )
        {
            case DVD_MODE:
                GnomeDVDModeManage( p_intf );
                break;
            case NET_MODE:
                GnomeNetworkModeManage( p_intf );
                break;
            case FILE_MODE:
            default:
                GnomeFileModeManage( p_intf );
                break;
        }

        p_intf->p_sys->b_mode_changed = 0;
    }


    if( p_intf->p_input != NULL )
    {
        float           newvalue;
        char            psz_title[3];
        char            psz_chapter[3];

#define p_area p_intf->p_input->stream.p_selected_area
        /* Update language/chapter menus after user request */
        if( ( p_intf->p_sys->b_menus_update ) ||
            ( p_intf->p_sys->i_part != p_area->i_part ) )
        {
            p_intf->p_sys->b_menus_update = 1;
            GnomeSetupMenu( p_intf );
            p_intf->p_sys->b_menus_update = 0;

            snprintf( psz_title, 3, "%02d", p_area->i_id );
            gtk_label_set_text( p_intf->p_sys->p_label_title, psz_title );

            p_intf->p_sys->i_part = p_area->i_part;
            snprintf( psz_chapter, 3, "%02d", p_area->i_part );
            gtk_label_set_text( p_intf->p_sys->p_label_chapter, psz_chapter );
        }

        /* Manage the slider */
        newvalue = p_intf->p_sys->p_adj->value;

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

            input_Seek( p_intf->p_input, i_seek );

            /* Update the old value */
            p_intf->p_sys->f_adj_oldvalue = newvalue;
        }
#undef p_area
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
 * GnomeLanguageMenus: update interactive menus of the interface
 *****************************************************************************
 * Sets up menus with information from input:
 *  -languages
 *  -sub-pictures
 * Warning: since this function is designed to be called by management
 * function, the interface lock has to be taken
 *****************************************************************************/
static gint GnomeLanguageMenus( gpointer          p_data,
                                GtkWidget *       p_root,
                                es_descriptor_t * p_es,
                                gint              i_cat,
                          void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    GtkWidget *         p_menu;
    GtkWidget *         p_separator;
    GtkWidget *         p_item;
    GtkWidget *         p_item_active;
    GSList *            p_group;
    char *              psz_name;
    gint                i_item;
    gint                i;

    

    /* cast */
    p_intf = (intf_thread_t *)p_data;

    /* removes previous menu */
    gtk_menu_item_remove_submenu( GTK_MENU_ITEM( p_root ) );
    gtk_widget_set_sensitive( p_root, FALSE );

    p_group = NULL;

    /* menu container */
    p_menu = gtk_menu_new();

    /* special case for "off" item */
    psz_name = "Off";

    p_item = gtk_radio_menu_item_new_with_label( p_group, psz_name );
    p_group = gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_item ) );

    gtk_widget_show( p_item );

    /* signal hanling for off */
    gtk_signal_connect( GTK_OBJECT( p_item ), "toggled",
                        GTK_SIGNAL_FUNC ( pf_toggle ), NULL );

    gtk_menu_append( GTK_MENU( p_menu ), p_item );

    p_separator = gtk_menu_item_new();
    gtk_widget_set_sensitive( p_separator, FALSE );
    gtk_widget_show( p_separator );
    gtk_menu_append( GTK_MENU( p_menu ), p_separator );

    vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
    p_item_active = NULL;
    i_item = 0;

    /* create a set of language buttons and append them to the container */
    for( i = 0 ; i < p_intf->p_input->stream.i_es_number ; i++ )
    {
        if( p_intf->p_input->stream.pp_es[i]->i_cat == i_cat )
        {
            i_item++;
            psz_name = p_intf->p_input->stream.pp_es[i]->psz_desc;
            if( psz_name[0] == '\0' )
            {
                sprintf( psz_name, "Language %d", i_item );
            }

            p_item = gtk_radio_menu_item_new_with_label( p_group, psz_name );
            p_group =
                gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_item ) );

            if( p_es == p_intf->p_input->stream.pp_es[i] )
            {
                /* don't lose p_item when we append into menu */
                p_item_active = p_item;
            }

            gtk_widget_show( p_item );

            /* setup signal hanling */
            gtk_signal_connect( GTK_OBJECT( p_item ), "toggled",
                            GTK_SIGNAL_FUNC( pf_toggle ),
                            (gpointer)( p_intf->p_input->stream.pp_es[i] ) );

            gtk_menu_append( GTK_MENU( p_menu ), p_item );
        }
    }

    vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );

    /* link the new menu to the menubar item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_root ), p_menu );

    /* acitvation will call signals so we can only do it
     * when submenu is attached to menu - to get intf_window */
    if( p_item_active != NULL )
    {
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( p_item_active ),
                                        TRUE );
    }

    /* be sure that menu is sensitive if non empty */
    if( i_item > 0 )
    {
        gtk_widget_set_sensitive( p_root, TRUE );
    }

    return TRUE;
}

/*****************************************************************************
 * GnomeChapterMenu: generate chapter menu for current title
 *****************************************************************************/
static gint GnomeChapterMenu( gpointer p_data, GtkWidget * p_chapter,
                        void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    char                psz_name[10];
    GtkWidget *         p_chapter_menu;
    GtkWidget *         p_chapter_submenu;
    GtkWidget *         p_menu_item;
    GtkWidget *         p_item;
    GtkWidget *         p_item_selected;
    GSList *            p_chapter_group;
    gint                i_title;
    gint                i_chapter;
    gint                i_nb;

    /* cast */
    p_intf = (intf_thread_t*)p_data;

    /* removes previous menu */
    gtk_menu_item_remove_submenu( GTK_MENU_ITEM( p_chapter ) );
    gtk_widget_set_sensitive( p_chapter, FALSE );

    p_chapter_submenu = NULL;
    p_chapter_group = NULL;
    p_item_selected = NULL;
    p_menu_item = NULL;

    i_title = p_intf->p_input->stream.p_selected_area->i_id;
    p_chapter_menu = gtk_menu_new();
    i_nb = p_intf->p_input->stream.pp_areas[i_title]->i_part_nb;

    for( i_chapter = 0 ; i_chapter < i_nb ; i_chapter++ )
    {
        /* we group chapters in packets of ten for small screens */
        if( ( i_chapter % 10 == 0 ) && ( i_nb > 20 ) )
        {
            if( i_chapter != 0 )
            {
                gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_menu_item ),
                                           p_chapter_submenu );
                gtk_menu_append( GTK_MENU( p_chapter_menu ), p_menu_item );
            }

            sprintf( psz_name, "%d - %d", i_chapter + 1, i_chapter + 10);
            p_menu_item = gtk_menu_item_new_with_label( psz_name );
            gtk_widget_show( p_menu_item );
            p_chapter_submenu = gtk_menu_new();
        }

        sprintf( psz_name, "Chapter %d", i_chapter + 1 );

        p_item = gtk_radio_menu_item_new_with_label( p_chapter_group,
                                                     psz_name );
        p_chapter_group =
            gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_item ) );

        if( p_intf->p_input->stream.pp_areas[i_title]->i_part
                     == i_chapter + 1 )
        {
            p_item_selected = p_item;
        }
        
        gtk_widget_show( p_item );

        /* setup signal hanling */
        gtk_signal_connect( GTK_OBJECT( p_item ),
                        "toggled",
                        GTK_SIGNAL_FUNC( pf_toggle ),
                        (gpointer)(i_chapter + 1) );

        if( i_nb > 20 )
        {
            gtk_menu_append( GTK_MENU( p_chapter_submenu ), p_item );
        }
        else
        {
            gtk_menu_append( GTK_MENU( p_chapter_menu ), p_item );
        }
    }

    if( i_nb > 20 )
    {
        gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_menu_item ),
                                   p_chapter_submenu );
        gtk_menu_append( GTK_MENU( p_chapter_menu ), p_menu_item );
    }

    /* link the new menu to the title menu item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_chapter ),
                               p_chapter_menu );

    /* toggle currently selected chapter */
    if( p_item_selected != NULL )
    {
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( p_item_selected ),
                                        TRUE );
    }

    /* be sure that chapter menu is sensitive, if there are several items */
    if( p_intf->p_input->stream.pp_areas[i_title]->i_part_nb > 1 )
    {
        gtk_widget_set_sensitive( p_chapter, TRUE );
    }

    return TRUE;
}

/*****************************************************************************
 * GnomeTitleMenu: sets menus for titles and chapters selection
 *****************************************************************************
 * Generates two types of menus:
 *  -simple list of titles
 *  -cascaded lists of chapters for each title
 *****************************************************************************/
static gint GnomeTitleMenu( gpointer       p_data,
                            GtkWidget *    p_navigation, 
                            void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    char                psz_name[10];
    GtkWidget *         p_title_menu;
    GtkWidget *         p_title_submenu;
    GtkWidget *         p_title_item;
    GtkWidget *         p_item_active;
    GtkWidget *         p_chapter_menu;
    GtkWidget *         p_chapter_submenu;
    GtkWidget *         p_title_menu_item;
    GtkWidget *         p_chapter_menu_item;
    GtkWidget *         p_item;
    GSList *            p_title_group;
    GSList *            p_chapter_group;
    gint                i_title;
    gint                i_chapter;
    gint                i_title_nb;
    gint                i_chapter_nb;

    /* cast */
    p_intf = (intf_thread_t*)p_data;

    p_title_menu = gtk_menu_new();
    p_title_group = NULL;
    p_title_submenu = NULL;
    p_title_menu_item = NULL;
    p_chapter_group = NULL;
    p_chapter_submenu = NULL;
    p_chapter_menu_item = NULL;
    p_item_active = NULL;
    i_title_nb = p_intf->p_input->stream.i_area_nb;

    /* loop on titles */
    for( i_title = 1 ; i_title < i_title_nb ; i_title++ )
    {
        /* we group titles in packets of ten for small screens */
        if( ( i_title % 10 == 1 ) && ( i_title_nb > 20 ) )
        {
            if( i_title != 1 )
            {
                gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_title_menu_item ),
                                           p_title_submenu );
                gtk_menu_append( GTK_MENU( p_title_menu ), p_title_menu_item );
            }

            sprintf( psz_name, "%d - %d", i_title, i_title + 9 );
            p_title_menu_item = gtk_menu_item_new_with_label( psz_name );
            gtk_widget_show( p_title_menu_item );
            p_title_submenu = gtk_menu_new();
        }

        sprintf( psz_name, "Title %d", i_title );

        if( pf_toggle == on_menubar_title_toggle )
        {
            p_title_item = gtk_radio_menu_item_new_with_label( p_title_group,
                                                           psz_name );
            p_title_group =
              gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_title_item ) );

            if( p_intf->p_input->stream.pp_areas[i_title] ==
                         p_intf->p_input->stream.p_selected_area )
            {
                p_item_active = p_title_item;
            }

            /* setup signal hanling */
            gtk_signal_connect( GTK_OBJECT( p_title_item ),
                     "toggled",
                     GTK_SIGNAL_FUNC( pf_toggle ),
                     (gpointer)(p_intf->p_input->stream.pp_areas[i_title]) );

            if( p_intf->p_input->stream.i_area_nb > 1 )
            {
                /* be sure that menu is sensitive */
                gtk_widget_set_sensitive( p_navigation, TRUE );
            }
        }
        else
        {
    
            p_title_item = gtk_menu_item_new_with_label( psz_name );
            p_chapter_menu = gtk_menu_new();
            i_chapter_nb =
                    p_intf->p_input->stream.pp_areas[i_title]->i_part_nb;
    
            for( i_chapter = 0 ; i_chapter < i_chapter_nb ; i_chapter++ )
            {
                /* we group chapters in packets of ten for small screens */
                if( ( i_chapter % 10 == 0 ) && ( i_chapter_nb > 20 ) )
                {
                    if( i_chapter != 0 )
                    {
                        gtk_menu_item_set_submenu(
                                    GTK_MENU_ITEM( p_chapter_menu_item ),
                                    p_chapter_submenu );
                        gtk_menu_append( GTK_MENU( p_chapter_menu ),
                                         p_chapter_menu_item );
                    }

                    sprintf( psz_name, "%d - %d", i_chapter + 1,
                                                  i_chapter + 10);
                    p_chapter_menu_item =
                            gtk_menu_item_new_with_label( psz_name );
                    gtk_widget_show( p_chapter_menu_item );
                    p_chapter_submenu = gtk_menu_new();
                }

                sprintf( psz_name, "Chapter %d", i_chapter + 1 );
    
                p_item = gtk_radio_menu_item_new_with_label(
                                                p_chapter_group, psz_name );
                p_chapter_group = gtk_radio_menu_item_group(
                                                GTK_RADIO_MENU_ITEM( p_item ) );
                gtk_widget_show( p_item );

#define p_area p_intf->p_input->stream.pp_areas[i_title]
                if( ( p_area == p_intf->p_input->stream.p_selected_area ) &&
                    ( p_area->i_part == i_chapter + 1 ) )
                {
                    p_item_active = p_item;
                }
#undef p_area

                /* setup signal hanling */
                gtk_signal_connect( GTK_OBJECT( p_item ),
                           "toggled",
                           GTK_SIGNAL_FUNC( pf_toggle ),
                           (gpointer)( ( i_title * 100 ) + ( i_chapter + 1) ) );

                if( i_chapter_nb > 20 )
                {
                    gtk_menu_append( GTK_MENU( p_chapter_submenu ), p_item );
                }
                else
                {
                    gtk_menu_append( GTK_MENU( p_chapter_menu ), p_item );
                }
            }

            if( i_chapter_nb > 20 )
            {
                gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_chapter_menu_item ),
                                           p_chapter_submenu );
                gtk_menu_append( GTK_MENU( p_chapter_menu ),
                                 p_chapter_menu_item );
            }

            /* link the new menu to the title menu item */
            gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_title_item ),
                                       p_chapter_menu );

            if( p_intf->p_input->stream.pp_areas[i_title]->i_part_nb > 1 )
            {
                /* be sure that menu is sensitive */
                gtk_widget_set_sensitive( p_navigation, TRUE );
            }
        }
        gtk_widget_show( p_title_item );

        if( i_title_nb > 20 )
        {
            gtk_menu_append( GTK_MENU( p_title_submenu ), p_title_item );
        }
        else
        {
            gtk_menu_append( GTK_MENU( p_title_menu ), p_title_item );
        }
    }

    if( i_title_nb > 20 )
    {
        gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_title_menu_item ),
                                   p_title_submenu );
        gtk_menu_append( GTK_MENU( p_title_menu ), p_title_menu_item );
    }

    /* be sure that menu is sensitive */
    gtk_widget_set_sensitive( p_title_menu, TRUE );

    /* link the new menu to the menubar item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_navigation ), p_title_menu );

    if( p_item_active != NULL )
    {
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( p_item_active ),
                                        TRUE );
    }


    return TRUE;
}

/*****************************************************************************
 * GnomeSetupMenu: function that generates title/chapter/audio/subpic
 * menus with help from preceding functions
 *****************************************************************************/
static gint GnomeSetupMenu( intf_thread_t * p_intf )
{
    es_descriptor_t *   p_audio_es;
    es_descriptor_t *   p_spu_es;
    GtkWidget *         p_menubar_menu;
    GtkWidget *         p_popup_menu;
    gint                i;

    if( p_intf->p_input->stream.i_area_nb > 1 )
    {
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                            p_intf->p_sys->p_window ), "menubar_title" ) );
        GnomeTitleMenu( p_intf, p_menubar_menu, on_menubar_title_toggle );

        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                             p_intf->p_sys->p_popup ), "popup_navigation" ) );
        GnomeTitleMenu( p_intf, p_popup_menu, on_popup_navigation_toggle );
    
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                             p_intf->p_sys->p_window ), "menubar_chapter" ) );
        GnomeChapterMenu( p_intf, p_menubar_menu, on_menubar_chapter_toggle );
    }

    /* look for selected ES */
    p_audio_es = NULL;
    p_spu_es = NULL;

    for( i = 0 ; i < p_intf->p_input->stream.i_selected_es_number ; i++ )
    {
        if( p_intf->p_input->stream.pp_selected_es[i]->i_cat == AUDIO_ES )
        {
            p_audio_es = p_intf->p_input->stream.pp_selected_es[i];
        }

        if( p_intf->p_input->stream.pp_selected_es[i]->i_cat == SPU_ES )
        {
            p_spu_es = p_intf->p_input->stream.pp_selected_es[i];
        }
    }

    /* audio menus */

    /* find audio root menu */
    p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                         p_intf->p_sys->p_window ), "menubar_audio" ) );

    p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                 p_intf->p_sys->p_popup ), "popup_audio" ) );

    GnomeLanguageMenus( p_intf, p_menubar_menu, p_audio_es, AUDIO_ES,
                        on_menubar_audio_toggle );
    GnomeLanguageMenus( p_intf, p_popup_menu, p_audio_es, AUDIO_ES,
                        on_popup_audio_toggle );

    /* sub picture menus */

    /* find spu root menu */
    p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                      p_intf->p_sys->p_window ), "menubar_subtitle" ) );

    p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                 p_intf->p_sys->p_popup ), "popup_subtitle" ) );

    GnomeLanguageMenus( p_intf, p_menubar_menu, p_spu_es, SPU_ES,
                        on_menubar_subtitle_toggle  );
    GnomeLanguageMenus( p_intf, p_popup_menu, p_spu_es, SPU_ES,
                        on_popup_subtitle_toggle );

    return TRUE;
}

/*****************************************************************************
 * GnomeDisplayDate: display stream date
 *****************************************************************************
 * This function displays the current date related to the position in
 * the stream. It is called whenever the slider changes its value.
 *****************************************************************************/
void GnomeDisplayDate( GtkAdjustment *p_adj )
{
    intf_thread_t *p_intf;
   
    p_intf = gtk_object_get_data( GTK_OBJECT( p_adj ), "p_intf" );

    if( p_intf->p_input != NULL )
    {
#define p_area p_intf->p_input->stream.p_selected_area
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];

        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );

        gtk_label_set_text( p_intf->p_sys->p_label_date,
                            input_OffsetToTime( p_intf->p_input, psz_time,
                                   ( p_area->i_size * p_adj->value ) / 100 ) );

        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );
#undef p_area
     }
}


/*****************************************************************************
 * GnomeDVDModeManage
 *****************************************************************************/
static gint GnomeDVDModeManage( intf_thread_t * p_intf )
{
    GtkWidget *     p_dvd_box;
    GtkWidget *     p_file_box;
    GtkWidget *     p_network_box;

    p_file_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "file_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_file_box ) );

    p_network_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "network_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_network_box ) );

    p_dvd_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "dvd_box" ) );
    gtk_widget_show( GTK_WIDGET( p_dvd_box ) );

    gtk_label_set_text( p_intf->p_sys->p_label_status,
                        "Status: playing DVD" );

    return TRUE;
}

/*****************************************************************************
 * GnomeFileModeManage
 *****************************************************************************/
static gint GnomeFileModeManage( intf_thread_t * p_intf )
{
    GtkWidget *     p_dvd_box;
    GtkWidget *     p_file_box;
    GtkWidget *     p_network_box;

    p_network_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "network_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_network_box ) );

    p_dvd_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "dvd_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_dvd_box ) );

    p_file_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "file_box" ) );
    gtk_widget_show( GTK_WIDGET( p_file_box ) );

    gtk_label_set_text( p_intf->p_sys->p_label_status,
                        "Status: foo" );

    return TRUE;
}

/*****************************************************************************
 * GnomeNetworkModeManage
 *****************************************************************************/
static gint GnomeNetworkModeManage( intf_thread_t * p_intf )
{
    GtkWidget *     p_dvd_box;
    GtkWidget *     p_file_box;
    GtkWidget *     p_network_box;

    p_dvd_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "dvd_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_dvd_box ) );

    p_file_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "file_box" ) );
    gtk_widget_hide( GTK_WIDGET( p_file_box ) );

    p_network_box = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                 p_intf->p_sys->p_window ), "network_box" ) );
    gtk_widget_show( GTK_WIDGET( p_network_box ) );

    gtk_label_set_text( p_intf->p_sys->p_label_status,
                        "Status: waiting for stream" );

    return TRUE;
}
