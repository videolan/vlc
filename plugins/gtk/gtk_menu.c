/*****************************************************************************
 * gtk_menu.c : functions to handle menu items.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_menu.c,v 1.15 2001/11/28 15:08:05 massiot Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#define gtk 12
#define gnome 42
#if ( MODULE_NAME == gtk )
#   include <gtk/gtk.h>
#elif ( MODULE_NAME == gnome )
#   include <gnome.h>
#endif
#undef gtk
#undef gnome

#include <string.h>

#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"

#include "video.h"
#include "video_output.h"
#include "audio_output.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_playlist.h"
#include "intf_gtk.h"

#include "main.h"

#include "modules_export.h"

/*
 * Local Prototypes
 */
static gint GtkLanguageMenus( gpointer , GtkWidget *, es_descriptor_t *, gint,
                        void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) );

void GtkMenubarAudioToggle   ( GtkCheckMenuItem *, gpointer );
void GtkPopupAudioToggle     ( GtkCheckMenuItem *, gpointer );
void GtkMenubarSubtitleToggle( GtkCheckMenuItem *, gpointer );
void GtkPopupSubtitleToggle  ( GtkCheckMenuItem *, gpointer );
static gint GtkTitleMenu( gpointer, GtkWidget *, 
                    void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) );
static gint GtkRadioMenu( intf_thread_t *, GtkWidget *, GSList *,
                          char *, int, int,
                   void( *pf_toggle )( GtkCheckMenuItem *, gpointer ) );
void GtkMenubarAngleToggle( GtkCheckMenuItem *, gpointer );
void GtkPopupAngleToggle( GtkCheckMenuItem *, gpointer );

gint GtkSetupMenus( intf_thread_t * p_intf );

/****************************************************************************
 * Gtk*Toggle: callbacks to toggle the value of a checkmenuitem
 ****************************************************************************
 * We need separate functions for menubar and popup here since we can't use
 * user_data to transmit intf_* and we need to refresh the other menu.
 ****************************************************************************/

#define GTKLANGTOGGLE( intf, window, menu, type, callback, b_update )   \
    intf_thread_t *         p_intf;                                     \
    GtkWidget *             p_menu;                                     \
    es_descriptor_t *       p_es;                                       \
                                                                        \
    p_intf = GetIntf( GTK_WIDGET(menuitem), (intf) );                   \
                                                                        \
    if( !p_intf->p_sys->b_update )                                      \
    {                                                                   \
        p_menu = GTK_WIDGET( gtk_object_get_data(                       \
                   GTK_OBJECT( p_intf->p_sys->window ), (menu) ) );     \
        p_es = (es_descriptor_t*)user_data;                             \
                                                                        \
        input_ToggleES( p_intf->p_input, p_es, menuitem->active );      \
                                                                        \
        p_intf->p_sys->b_update = menuitem->active;                     \
                                                                        \
        if( p_intf->p_sys->b_update )                                   \
        {                                                               \
            GtkLanguageMenus( p_intf, p_menu, p_es, type, callback );   \
        }                                                               \
                                                                        \
        p_intf->p_sys->b_update = 0;                                    \
    }

/*
 * Audio
 */ 

void GtkMenubarAudioToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKLANGTOGGLE( "intf_window", p_popup, "popup_audio", AUDIO_ES,
                   GtkPopupAudioToggle, b_audio_update );
}

void GtkPopupAudioToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKLANGTOGGLE( "intf_popup", p_window, "menubar_audio", AUDIO_ES,
                   GtkMenubarAudioToggle, b_audio_update );
}

/* 
 * Subtitles
 */ 

void GtkMenubarSubtitleToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKLANGTOGGLE( "intf_window", p_popup, "popup_subpictures", SPU_ES,
                   GtkPopupSubtitleToggle, b_spu_update );
}

void GtkPopupSubtitleToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKLANGTOGGLE( "intf_popup", p_window, "menubar_subpictures", SPU_ES,
                   GtkMenubarSubtitleToggle, b_spu_update );
}

#undef GTKLANGTOGGLE

/*
 * Navigation
 */

void GtkPopupNavigationToggle( GtkCheckMenuItem * menuitem,
                               gpointer user_data )
{
    intf_thread_t * p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_popup" );

    if( menuitem->active &&
        !p_intf->p_sys->b_title_update &&
        !p_intf->p_sys->b_chapter_update )
    {
        input_area_t   *p_area;

        gint i_title   = DATA2TITLE( user_data );
        gint i_chapter = DATA2CHAPTER( user_data );

        p_area = p_intf->p_input->stream.p_selected_area;

        if( p_area != p_intf->p_input->stream.pp_areas[i_title] )
        {
            p_area = p_intf->p_input->stream.pp_areas[i_title];
            p_intf->p_sys->b_title_update = 1;
        }

        p_area->i_part = i_chapter;

        input_ChangeArea( p_intf->p_input, (input_area_t*)p_area );

        p_intf->p_sys->b_chapter_update = 1;
        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
        GtkSetupMenus( p_intf );
        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
    }
}

/*
 * Title
 */

void GtkMenubarTitleToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    intf_thread_t * p_intf = GetIntf( GTK_WIDGET(menuitem), "intf_window" );

    if( menuitem->active && !p_intf->p_sys->b_title_update )
    {
        gint i_title = (gint)((long)user_data);
        input_ChangeArea( p_intf->p_input,
                          p_intf->p_input->stream.pp_areas[i_title] );

        p_intf->p_sys->b_title_update = 1;
        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
        GtkSetupMenus( p_intf );
        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );
        p_intf->p_sys->b_title_update = 0;

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );

    }
}

/*
 * Chapter
 */

void GtkMenubarChapterToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;
    gint            i_chapter;
    char            psz_chapter[5];
    GtkWidget *     p_popup_menu;

    p_intf    = GetIntf( GTK_WIDGET(menuitem), "intf_window" );
    p_area    = p_intf->p_input->stream.p_selected_area;
    i_chapter = (gint)((long)user_data);

    if( menuitem->active && !p_intf->p_sys->b_chapter_update )
    {
        p_area->i_part = i_chapter;
        input_ChangeArea( p_intf->p_input, (input_area_t*)p_area );

        snprintf( psz_chapter, 4, "%02d", p_area->i_part );
        psz_chapter[ 4 ] = '\0';
        gtk_label_set_text( p_intf->p_sys->p_label_chapter, psz_chapter );

        p_intf->p_sys->b_chapter_update = 1;
        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                             p_intf->p_sys->p_popup ), "popup_navigation" ) );

        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
        GtkTitleMenu( p_intf, p_popup_menu, GtkPopupNavigationToggle );
        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );

        p_intf->p_sys->b_chapter_update = 0;

        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
    }
}


/*
 * Angle
 */

#define GTKANGLETOGGLE( intf, window, menu, callback )                      \
    intf_thread_t * p_intf;                                                 \
    GtkWidget *     p_menu;                                                 \
    input_area_t *  p_area;                                                 \
                                                                            \
    p_intf    = GetIntf( GTK_WIDGET(menuitem), (intf) );                    \
                                                                            \
    if( menuitem->active && !p_intf->p_sys->b_angle_update )                \
    {                                                                       \
        p_menu    = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(            \
                                p_intf->p_sys->window ), (menu) ) );        \
        p_area    = p_intf->p_input->stream.p_selected_area;                \
        p_area->i_angle = (gint)((long)user_data);                          \
                                                                            \
        input_ChangeArea( p_intf->p_input, (input_area_t*)p_area );         \
                                                                            \
        p_intf->p_sys->b_angle_update = 1;                                  \
        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );             \
        GtkRadioMenu( p_intf, p_menu, NULL, "Angle",                        \
                      p_area->i_angle_nb, p_area->i_angle, (callback) );    \
        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );           \
        p_intf->p_sys->b_angle_update = 0;                                  \
    }

void GtkMenubarAngleToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKANGLETOGGLE( "intf_window", p_popup, "popup_angle",
                    GtkPopupAngleToggle );
}

void GtkPopupAngleToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKANGLETOGGLE( "intf_popup", p_window, "menubar_angle",
                    GtkMenubarAngleToggle );
}

#undef GTKANGLETOGGLE

/****************************************************************************
 * Functions to generate menus
 ****************************************************************************/

/*****************************************************************************
 * GtkRadioMenu: update interactive menus of the interface
 *****************************************************************************
 * Sets up menus with information from input
 * Warning: since this function is designed to be called by management
 * function, the interface lock has to be taken
 *****************************************************************************/
static gint GtkRadioMenu( intf_thread_t * p_intf,
                            GtkWidget * p_root, GSList * p_menu_group,
                            char * psz_item_name,
                            int i_nb, int i_selected,
                     void( *pf_toggle )( GtkCheckMenuItem *, gpointer ) )
{
    char                psz_name[ GTK_MENU_LABEL_SIZE ];
    GtkWidget *         p_menu;
    GtkWidget *         p_submenu;
    GtkWidget *         p_item_group;
    GtkWidget *         p_item;
    GtkWidget *         p_item_selected;
    GSList *            p_group;
    gint                i_item;

    /* temporary hack to avoid blank menu when an open menu is removed */
    if( GTK_MENU_ITEM(p_root)->submenu != NULL )
    {
        gtk_menu_popdown( GTK_MENU( GTK_MENU_ITEM(p_root)->submenu ) );
    }
    /* removes previous menu */
    gtk_menu_item_remove_submenu( GTK_MENU_ITEM( p_root ) );
    gtk_widget_set_sensitive( p_root, FALSE );

    p_item_group = NULL;
    p_submenu = NULL;
    p_item_selected = NULL;
    p_group = p_menu_group;

    p_menu = gtk_menu_new();

    for( i_item = 0 ; i_item < i_nb ; i_item++ )
    {
        /* we group chapters in packets of ten for small screens */
        if( ( i_item % 10 == 0 ) && ( i_nb > 20 ) )
        {
            if( i_item != 0 )
            {
                gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_item_group ),
                                           p_submenu );
                gtk_menu_append( GTK_MENU( p_menu ), p_item_group );
            }

            snprintf( psz_name, GTK_MENU_LABEL_SIZE,
                      "%ss %d to %d", psz_item_name, i_item + 1, i_item + 10);
            psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
            p_item_group = gtk_menu_item_new_with_label( psz_name );
            gtk_widget_show( p_item_group );
            p_submenu = gtk_menu_new();
        }

        snprintf( psz_name, GTK_MENU_LABEL_SIZE, "%s %d",
                  psz_item_name, i_item + 1 );
        psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';

        p_item = gtk_radio_menu_item_new_with_label( p_group, psz_name );
        p_group = gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_item ) );

        if( i_selected == i_item + 1 )
        {
            p_item_selected = p_item;
        }
        
        gtk_widget_show( p_item );

        /* setup signal hanling */
        gtk_signal_connect( GTK_OBJECT( p_item ),
                            "toggled",
                            GTK_SIGNAL_FUNC( pf_toggle ),
                            (gpointer)((long)(i_item + 1)) );

        if( i_nb > 20 )
        {
            gtk_menu_append( GTK_MENU( p_submenu ), p_item );
        }
        else
        {
            gtk_menu_append( GTK_MENU( p_menu ), p_item );
        }
    }

    if( i_nb > 20 )
    {
        gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_item_group ), p_submenu );
        gtk_menu_append( GTK_MENU( p_menu ), p_item_group );
    }

    /* link the new menu to the title menu item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_root ), p_menu );

    /* toggle currently selected chapter
     * We have to release the lock since input_ToggleES needs it */
    if( p_item_selected != NULL )
    {
        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( p_item_selected ),
                                        TRUE );
        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
    }

    /* be sure that menu is sensitive, if there are several items */
    if( i_nb > 1 )
    {
        gtk_widget_set_sensitive( p_root, TRUE );
    }

    return TRUE;
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
                                gint              i_cat,
                          void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    GtkWidget *         p_menu;
    GtkWidget *         p_separator;
    GtkWidget *         p_item;
    GtkWidget *         p_item_active;
    GSList *            p_group;
    char                psz_name[ GTK_MENU_LABEL_SIZE ];
    gint                i_item;
    gint                i;

    

    /* cast */
    p_intf = (intf_thread_t *)p_data;

    /* temporary hack to avoid blank menu when an open menu is removed */
    if( GTK_MENU_ITEM(p_root)->submenu != NULL )
    {
        gtk_menu_popdown( GTK_MENU( GTK_MENU_ITEM(p_root)->submenu ) );
    }
    /* removes previous menu */
    gtk_menu_item_remove_submenu( GTK_MENU_ITEM( p_root ) );
    gtk_widget_set_sensitive( p_root, FALSE );

    p_group = NULL;

    /* menu container */
    p_menu = gtk_menu_new();

    /* special case for "off" item */
    snprintf( psz_name, GTK_MENU_LABEL_SIZE, "None" );
    psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';

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

    p_item_active = NULL;
    i_item = 0;

    vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );

    /* create a set of language buttons and append them to the container */
    for( i = 0 ; i < p_intf->p_input->stream.i_es_number ; i++ )
    {
        if( p_intf->p_input->stream.pp_es[i]->i_cat == i_cat )
        {
            i_item++;
            strcpy( psz_name, p_intf->p_input->stream.pp_es[i]->psz_desc );
            if( psz_name[0] == '\0' )
            {
                snprintf( psz_name, GTK_MENU_LABEL_SIZE,
                          "Language %d", i_item );
                psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
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
     * when submenu is attached to menu - to get intf_window 
     * We have to release the lock since input_ToggleES needs it */
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
 * GtkTitleMenu: sets menus for titles and chapters selection
 *****************************************************************************
 * Generates two types of menus:
 *  -simple list of titles
 *  -cascaded lists of chapters for each title
 *****************************************************************************/
static gint GtkTitleMenu( gpointer       p_data,
                            GtkWidget *    p_navigation, 
                            void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    char                psz_name[ GTK_MENU_LABEL_SIZE ];
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

    /* temporary hack to avoid blank menu when an open menu is removed */
    if( GTK_MENU_ITEM(p_navigation)->submenu != NULL )
    {
        gtk_menu_popdown( GTK_MENU( GTK_MENU_ITEM(p_navigation)->submenu ) );
    }
    /* removes previous menu */
    gtk_menu_item_remove_submenu( GTK_MENU_ITEM( p_navigation ) );
    gtk_widget_set_sensitive( p_navigation, FALSE );

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

            snprintf( psz_name, GTK_MENU_LABEL_SIZE,
                      "%d - %d", i_title, i_title + 9 );
            psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
            p_title_menu_item = gtk_menu_item_new_with_label( psz_name );
            gtk_widget_show( p_title_menu_item );
            p_title_submenu = gtk_menu_new();
        }

        snprintf( psz_name, GTK_MENU_LABEL_SIZE, "Title %d (%d)", i_title,
                  p_intf->p_input->stream.pp_areas[i_title]->i_part_nb );
        psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
#if 0
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
#endif
        {
            p_title_item = gtk_menu_item_new_with_label( psz_name );

#if 1    
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

                    snprintf( psz_name, GTK_MENU_LABEL_SIZE,
                              "%d - %d", i_chapter + 1, i_chapter + 10 );
                    psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
                    p_chapter_menu_item =
                            gtk_menu_item_new_with_label( psz_name );
                    gtk_widget_show( p_chapter_menu_item );
                    p_chapter_submenu = gtk_menu_new();
                }

                snprintf( psz_name, GTK_MENU_LABEL_SIZE,
                          "Chapter %d", i_chapter + 1 );
                psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
    
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
                           (gpointer)POS2DATA( i_title, i_chapter + 1) );

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
#else
        GtkRadioMenu( p_intf, p_title_item, p_chapter_group, "Chapter",
                        p_intf->p_input->stream.pp_areas[i_title]->i_part_nb,
                        i_title * 100,
                        p_intf->p_input->stream.p_selected_area->i_part +
                            p_intf->p_input->stream.p_selected_area->i_id *100,
                        pf_toggle );

#endif
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

    /* Default selected chapter
     * We have to release the lock since input_ToggleES needs it */
    if( p_item_active != NULL )
    {
        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( p_item_active ),
                                        TRUE );
        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
    }
#if 0
    if( p_intf->p_input->stream.i_area_nb > 1 )
    {
        /* be sure that menu is sensitive */
        gtk_widget_set_sensitive( p_navigation, TRUE );
    }
#endif

    return TRUE;
}

/*****************************************************************************
 * GtkSetupMenus: function that generates title/chapter/audio/subpic
 * menus with help from preceding functions
 *****************************************************************************/
gint GtkSetupMenus( intf_thread_t * p_intf )
{
    es_descriptor_t *   p_audio_es;
    es_descriptor_t *   p_spu_es;
    GtkWidget *         p_menubar_menu;
    GtkWidget *         p_popup_menu;
    gint                i;

    p_intf->p_sys->b_chapter_update |= p_intf->p_sys->b_title_update;
    p_intf->p_sys->b_angle_update |= p_intf->p_sys->b_title_update;
    p_intf->p_sys->b_audio_update |= p_intf->p_sys->b_title_update;
    p_intf->p_sys->b_spu_update |= p_intf->p_sys->b_title_update;

//    vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );

    if( p_intf->p_sys->b_title_update )
    { 
        char            psz_title[5];

        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                            p_intf->p_sys->p_window ), "menubar_title" ) );
        GtkRadioMenu( p_intf, p_menubar_menu, NULL, "Title",
                      p_intf->p_input->stream.i_area_nb - 1,
                      p_intf->p_input->stream.p_selected_area->i_id,
                      GtkMenubarTitleToggle );

        snprintf( psz_title, 4, "%d",
                  p_intf->p_input->stream.p_selected_area->i_id );
        psz_title[ 4 ] = '\0';
        gtk_label_set_text( p_intf->p_sys->p_label_title, psz_title );

        p_intf->p_sys->b_title_update = 0;
    }

    if( p_intf->p_sys->b_chapter_update )
    {
        char            psz_chapter[5];

        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                             p_intf->p_sys->p_popup ), "popup_navigation" ) );
        GtkTitleMenu( p_intf, p_popup_menu, GtkPopupNavigationToggle );
#if 0
        GtkRadioMenu( p_intf, p_menubar_menu, NULL, "Title",
                        p_intf->p_input->stream.i_area_nb - 1,
                        p_intf->p_input->stream.p_selected_area->i_id,
                        on_menubar_chapter_toggle );
#endif

        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                             p_intf->p_sys->p_window ), "menubar_chapter" ) );

        GtkRadioMenu( p_intf, p_menubar_menu, NULL, "Chapter",
                        p_intf->p_input->stream.p_selected_area->i_part_nb,
                        p_intf->p_input->stream.p_selected_area->i_part,
                        GtkMenubarChapterToggle );


        snprintf( psz_chapter, 4, "%d", 
                  p_intf->p_input->stream.p_selected_area->i_part );
        psz_chapter[ 4 ] = '\0';
        gtk_label_set_text( p_intf->p_sys->p_label_chapter, psz_chapter );

        p_intf->p_sys->i_part =
                p_intf->p_input->stream.p_selected_area->i_part;

        p_intf->p_sys->b_chapter_update = 0;
    }

    if( p_intf->p_sys->b_angle_update )
    {
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                             p_intf->p_sys->p_window ), "menubar_angle" ) );
        GtkRadioMenu( p_intf, p_menubar_menu, NULL, "Angle",
                        p_intf->p_input->stream.p_selected_area->i_angle_nb,
                        p_intf->p_input->stream.p_selected_area->i_angle,
                        GtkMenubarAngleToggle );

        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                             p_intf->p_sys->p_popup ), "popup_angle" ) );
        GtkRadioMenu( p_intf, p_popup_menu, NULL, "Angle",
                        p_intf->p_input->stream.p_selected_area->i_angle_nb,
                        p_intf->p_input->stream.p_selected_area->i_angle,
                        GtkPopupAngleToggle );

        p_intf->p_sys->b_angle_update = 0;
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

    vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );

    /* audio menus */
    if( p_intf->p_sys->b_audio_update )
    {
        /* find audio root menu */
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                             p_intf->p_sys->p_window ), "menubar_audio" ) );
    
        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                     p_intf->p_sys->p_popup ), "popup_audio" ) );
    
        p_intf->p_sys->b_audio_update = 1;
        GtkLanguageMenus( p_intf, p_menubar_menu, p_audio_es, AUDIO_ES,
                            GtkMenubarAudioToggle );
        p_intf->p_sys->b_audio_update = 1;
        GtkLanguageMenus( p_intf, p_popup_menu, p_audio_es, AUDIO_ES,
                            GtkPopupAudioToggle );
    
        p_intf->p_sys->b_audio_update = 0;
    }
    
    /* sub picture menus */
    if( p_intf->p_sys->b_spu_update )
    {
        /* find spu root menu */
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                          p_intf->p_sys->p_window ), "menubar_subpictures" ) );
    
        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT( 
                     p_intf->p_sys->p_popup ), "popup_subpictures" ) );
    
        p_intf->p_sys->b_spu_update = 1;
        GtkLanguageMenus( p_intf, p_menubar_menu, p_spu_es, SPU_ES,
                            GtkMenubarSubtitleToggle  );
        p_intf->p_sys->b_spu_update = 1;
        GtkLanguageMenus( p_intf, p_popup_menu, p_spu_es, SPU_ES,
                            GtkPopupSubtitleToggle );
    
        p_intf->p_sys->b_spu_update = 0;
    }

    vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );

    return TRUE;
}

