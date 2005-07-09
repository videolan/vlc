/*****************************************************************************
 * menu.c : functions to handle menu items.
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Stéphane Borel <stef@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/intf.h>
#include <vlc/aout.h>
#include <vlc/vout.h>

#ifdef MODULE_NAME_IS_gnome
#   include <gnome.h>
#else
#   include <gtk/gtk.h>
#endif

#include <string.h>

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"

#include "playlist.h"
#include "common.h"

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
                          char *, int, int, int,
                   void( *pf_toggle )( GtkCheckMenuItem *, gpointer ) );

static void GtkMenubarDeinterlaceToggle( GtkCheckMenuItem * menuitem, gpointer user_data );
static void GtkPopupDeinterlaceToggle( GtkCheckMenuItem * menuitem, gpointer user_data );
static gint GtkDeinterlaceMenus( gpointer          p_data,
                                 GtkWidget *       p_root,
                                 void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) );

gint GtkSetupMenus( intf_thread_t * p_intf );

/****************************************************************************
 * Gtk*Toggle: callbacks to toggle the value of a checkmenuitem
 ****************************************************************************
 * We need separate functions for menubar and popup here since we can't use
 * user_data to transmit intf_* and we need to refresh the other menu.
 ****************************************************************************/

#define GTKLANGTOGGLE( window, menu, type, var_name, callback, b_update )\
    intf_thread_t *         p_intf;                                     \
    GtkWidget *             p_menu;                                     \
    es_descriptor_t *       p_es;                                       \
                                                                        \
    p_intf = GtkGetIntf( menuitem );                                    \
                                                                        \
    if( !p_intf->p_sys->b_update )                                      \
    {                                                                   \
        p_menu = GTK_WIDGET( gtk_object_get_data(                       \
                   GTK_OBJECT( p_intf->p_sys->window ), (menu) ) );     \
        p_es = (es_descriptor_t*)user_data;                             \
        if( p_es && menuitem->active )                                  \
            var_SetInteger( p_intf->p_sys->p_input, var_name, p_es->i_id ); \
        else                                                            \
            var_SetInteger( p_intf->p_sys->p_input, var_name, -1 );     \
                                                                        \
        p_intf->p_sys->b_update = menuitem->active;                     \
                                                                        \
        if( p_intf->p_sys->b_update )                                   \
        {                                                               \
            GtkLanguageMenus( p_intf, p_menu, p_es, type, callback );   \
        }                                                               \
                                                                        \
        p_intf->p_sys->b_update = VLC_FALSE;                            \
    }

/*
 * Audio
 */

void GtkMenubarAudioToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKLANGTOGGLE( p_popup, "popup_language", AUDIO_ES, "audio-es",
                   GtkPopupAudioToggle, b_audio_update );
}

void GtkPopupAudioToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKLANGTOGGLE( p_window, "menubar_audio", AUDIO_ES, "audio-es",
                   GtkMenubarAudioToggle, b_audio_update );
}

/*
 * Subtitles
 */

void GtkMenubarSubtitleToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKLANGTOGGLE( p_popup, "popup_subpictures", SPU_ES, "spu-es",
                   GtkPopupSubtitleToggle, b_spu_update );
}

void GtkPopupSubtitleToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKLANGTOGGLE( p_window, "menubar_subpictures", SPU_ES, "spu-es",
                   GtkMenubarSubtitleToggle, b_spu_update );
}

#undef GTKLANGTOGGLE

/*
 * Navigation
 */

void GtkPopupNavigationToggle( GtkCheckMenuItem * menuitem,
                               gpointer user_data )
{
    intf_thread_t * p_intf = GtkGetIntf( menuitem );

    if( menuitem->active &&
        !p_intf->p_sys->b_title_update &&
        !p_intf->p_sys->b_chapter_update )
    {
        input_area_t   *p_area;

        guint i_title   = DATA2TITLE( user_data );
        guint i_chapter = DATA2CHAPTER( user_data );

        /* FIXME use "navigation" variable */
        var_SetInteger( p_intf->p_sys->p_input, "title", i_title );
        var_SetInteger( p_intf->p_sys->p_input, "chapter", i_chapter );

        p_intf->p_sys->b_title_update = VLC_TRUE;
        p_intf->p_sys->b_chapter_update = VLC_TRUE;

        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        GtkSetupMenus( p_intf );
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}

/*
 * Program
 */
#define GTKPROGRAMTOGGLE( )                                                 \
    intf_thread_t * p_intf = GtkGetIntf( menuitem );                        \
                                                                            \
    if( menuitem->active && !p_intf->p_sys->b_program_update )              \
    {                                                                       \
        int i_program_id = (ptrdiff_t)user_data;                            \
                                                                            \
        var_SetInteger( p_intf->p_sys->p_input, "program", i_program_id );  \
                                                                            \
        p_intf->p_sys->b_program_update = VLC_TRUE;                         \
                                                                            \
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );      \
        GtkSetupMenus( p_intf );                                            \
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );    \
                                                                            \
        p_intf->p_sys->b_program_update = VLC_FALSE;                        \
                                                                            \
        var_SetInteger( p_intf->p_sys->p_input, "state", PLAYING_S );       \
    }

void GtkMenubarProgramToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKPROGRAMTOGGLE( );
}

void GtkPopupProgramToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GTKPROGRAMTOGGLE( );
}

/*
 * Title
 */

void GtkMenubarTitleToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    intf_thread_t * p_intf = GtkGetIntf( menuitem );

    if( menuitem->active && !p_intf->p_sys->b_title_update )
    {
        guint i_title = (ptrdiff_t)user_data;

        var_SetInteger( p_intf->p_sys->p_input, "title", i_title );

        p_intf->p_sys->b_title_update = VLC_TRUE;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        GtkSetupMenus( p_intf );
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}

/*
 * Chapter
 */

void GtkMenubarChapterToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    intf_thread_t * p_intf;
    input_area_t *  p_area;
    guint           i_chapter;
    GtkWidget *     p_popup_menu;

    p_intf    = GtkGetIntf( menuitem );
    p_area    = p_intf->p_sys->p_input->stream.p_selected_area;
    i_chapter = (ptrdiff_t)user_data;

    if( menuitem->active && !p_intf->p_sys->b_chapter_update )
    {
        var_SetInteger( p_intf->p_sys->p_input, "chapter", i_chapter );

        p_intf->p_sys->b_chapter_update = VLC_TRUE;
        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                             p_intf->p_sys->p_popup ), "popup_navigation" ) );

        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        GtkTitleMenu( p_intf, p_popup_menu, GtkPopupNavigationToggle );
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

        p_intf->p_sys->b_chapter_update = VLC_FALSE;
    }
}


static void GtkPopupObjectToggle( GtkCheckMenuItem * menuitem,
    gpointer user_data, int i_object_type, char *psz_variable )
{
    intf_thread_t   *p_intf = GtkGetIntf( menuitem );
    GtkLabel        *p_label;

    p_label = GTK_LABEL( ( GTK_BIN( menuitem )->child ) );

    if( menuitem->active && !p_intf->p_sys->b_aout_update &&
        !p_intf->p_sys->b_vout_update )
    {
        vlc_object_t * p_obj;

        p_obj = (vlc_object_t *)vlc_object_find( p_intf, i_object_type,
                                                  FIND_ANYWHERE );
        if( p_obj )
        {
            vlc_value_t val;

            if( user_data )
            {
                val = (vlc_value_t)user_data;
            }
            else
            {
                gtk_label_get( p_label, &val.psz_string );
            }

            if( var_Set( p_obj, psz_variable, val ) < 0 )
            {
                msg_Warn( p_obj, "cannot set variable (%s)", val.psz_string );
            }
            vlc_object_release( p_obj );
        }
    }
}
static void GtkPopupAoutChannelsToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GtkPopupObjectToggle( menuitem, user_data, VLC_OBJECT_AOUT, "audio-channels" );
}

static void GtkPopupAoutDeviceToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GtkPopupObjectToggle( menuitem, user_data, VLC_OBJECT_AOUT, "audio-device" );
}


static void GtkPopupVoutDeviceToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    GtkPopupObjectToggle( menuitem, user_data, VLC_OBJECT_VOUT, "video-device" );
}


static void GtkDeinterlaceUpdate( intf_thread_t *p_intf, char *psz_mode )
{
    char *psz_filter;
    unsigned int  i;

    psz_filter = config_GetPsz( p_intf, "vout-filter" );

    if( !strcmp( psz_mode, "None" ) )
    {
        config_PutPsz( p_intf, "vout-filter", "" );
    }
    else
    {
        if( !psz_filter || !*psz_filter )
        {
            config_PutPsz( p_intf, "vout-filter", "deinterlace" );
        }
        else
        {
            if( strstr( psz_filter, "deinterlace" ) == NULL )
            {
                psz_filter = realloc( psz_filter, strlen( psz_filter ) + 20 );
                strcat( psz_filter, ",deinterlace" );
            }
            config_PutPsz( p_intf, "vout-filter", psz_filter );
        }
    }

    if( psz_filter )
        free( psz_filter );

    /* now restart all video stream */
    if( p_intf->p_sys->p_input )
    {
        vout_thread_t *p_vout;
        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );

        /* Warn the vout we are about to change the filter chain */
        p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                  FIND_ANYWHERE );
        if( p_vout )
        {
            p_vout->b_filter_change = VLC_TRUE;
            vlc_object_release( p_vout );
        }

#define ES p_intf->p_sys->p_input->stream.pp_es[i]
        /* create a set of language buttons and append them to the container */
        for( i = 0 ; i < p_intf->p_sys->p_input->stream.i_es_number ; i++ )
        {
            if( ( ES->i_cat == VIDEO_ES ) &&
                    ES->p_dec != NULL )
            {
                input_UnselectES( p_intf->p_sys->p_input, ES );
                input_SelectES( p_intf->p_sys->p_input, ES );
            }
#undef ES
        }
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }

    if( strcmp( psz_mode, "None" ) )
    {
        vout_thread_t *p_vout;
        p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                  FIND_ANYWHERE );
        if( p_vout )
        {
            vlc_value_t val;

            val.psz_string = psz_mode;
            if( var_Set( p_vout, "deinterlace-mode", val ) != VLC_SUCCESS )
                config_PutPsz( p_intf, "deinterlace-mode", psz_mode );

            vlc_object_release( p_vout );
        }
        else
            config_PutPsz( p_intf, "deinterlace-mode", psz_mode );

    }
}

static void GtkMenubarDeinterlaceToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    intf_thread_t   *p_intf = GtkGetIntf( menuitem );
    GtkLabel        *p_label;
    char            *psz_mode;
    GtkWidget       *p_popup_menu;

    p_label = GTK_LABEL( ( GTK_BIN( menuitem )->child ) );

    if( !p_intf->p_sys->b_deinterlace_update && menuitem->active )
    {
        gtk_label_get( p_label, &psz_mode );
        GtkDeinterlaceUpdate( p_intf, psz_mode );

        p_intf->p_sys->b_deinterlace_update = VLC_TRUE;

        p_popup_menu   = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                                     p_intf->p_sys->p_popup ), "popup_deinterlace" ) );

        GtkDeinterlaceMenus( p_intf, p_popup_menu, GtkPopupDeinterlaceToggle );

        p_intf->p_sys->b_deinterlace_update = VLC_FALSE;

    }
}

static void GtkPopupDeinterlaceToggle( GtkCheckMenuItem * menuitem, gpointer user_data )
{
    intf_thread_t   *p_intf = GtkGetIntf( menuitem );
    GtkLabel        *p_label;
    char            *psz_mode;
    GtkWidget       *p_menubar_menu;

    p_label = GTK_LABEL( ( GTK_BIN( menuitem )->child ) );

    if( !p_intf->p_sys->b_deinterlace_update && menuitem->active )
    {
        gtk_label_get( p_label, &psz_mode );
        GtkDeinterlaceUpdate( p_intf, psz_mode );

        p_intf->p_sys->b_deinterlace_update = VLC_TRUE;

        p_menubar_menu   = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                                     p_intf->p_sys->p_window ), "menubar_deinterlace" ) );

        GtkDeinterlaceMenus( p_intf, p_menubar_menu, GtkMenubarDeinterlaceToggle );

        p_intf->p_sys->b_deinterlace_update = VLC_FALSE;
    }
}

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
                            int i_start, int i_end, int i_selected,
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
    gtk_object_set_data( GTK_OBJECT( p_menu ), "p_intf", p_intf );

    for( i_item = i_start ; i_item <= i_end ; i_item++ )
    {
        /* we group chapters in packets of ten for small screens */
        if( ( i_item % 10 == i_start ) && ( i_end > i_start + 20 ) )
        {
            if( i_item != i_start )
            {
                gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_item_group ),
                                           p_submenu );
                gtk_menu_append( GTK_MENU( p_menu ), p_item_group );
            }

            snprintf( psz_name, GTK_MENU_LABEL_SIZE,
                      "%ss %d to %d", psz_item_name, i_item, i_item + 9 );
            psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
            p_item_group = gtk_menu_item_new_with_label( psz_name );
            gtk_widget_show( p_item_group );
            p_submenu = gtk_menu_new();
            gtk_object_set_data( GTK_OBJECT( p_submenu ), "p_intf", p_intf );
        }

        snprintf( psz_name, GTK_MENU_LABEL_SIZE, "%s %d",
                  psz_item_name, i_item );
        psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';

        p_item = gtk_radio_menu_item_new_with_label( p_group, psz_name );
        p_group = gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_item ) );

        if( i_selected == i_item )
        {
            p_item_selected = p_item;
        }

        gtk_widget_show( p_item );

        /* setup signal hanling */
        gtk_signal_connect( GTK_OBJECT( p_item ),
                            "toggled",
                            GTK_SIGNAL_FUNC( pf_toggle ),
                            (gpointer)((long)(i_item)) );

        if( i_end > i_start + 20 )
        {
            gtk_menu_append( GTK_MENU( p_submenu ), p_item );
        }
        else
        {
            gtk_menu_append( GTK_MENU( p_menu ), p_item );
        }
    }

    if( i_end > i_start + 20 )
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
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( p_item_selected ),
                                        TRUE );
    }

    /* be sure that menu is sensitive, if there are several items */
    if( i_end > i_start )
    {
        gtk_widget_set_sensitive( p_root, TRUE );
    }

    return TRUE;
}

/*****************************************************************************
 * GtkProgramMenu: update the programs menu of the interface
 *****************************************************************************
 * Builds the program menu according to what have been found in the PAT
 * by the input. Usefull for multi-programs streams such as DVB ones.
 *****************************************************************************/
static gint GtkProgramMenu( gpointer            p_data,
                            GtkWidget *         p_root,
                            pgrm_descriptor_t * p_pgrm,
                      void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    GtkWidget *         p_menu;
    GtkWidget *         p_item;
    GtkWidget *         p_item_active;
    GSList *            p_group;
    char                psz_name[ GTK_MENU_LABEL_SIZE ];
    guint               i;

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
    gtk_object_set_data( GTK_OBJECT( p_menu ), "p_intf", p_intf );

    p_item_active = NULL;

    /* create a set of program buttons and append them to the container */
    for( i = 0 ; i < p_intf->p_sys->p_input->stream.i_pgrm_number ; i++ )
    {
        snprintf( psz_name, GTK_MENU_LABEL_SIZE, "id %d",
            p_intf->p_sys->p_input->stream.pp_programs[i]->i_number );
        psz_name[GTK_MENU_LABEL_SIZE-1] = '\0';

        p_item = gtk_radio_menu_item_new_with_label( p_group, psz_name );
        p_group =
            gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_item ) );

        if( p_pgrm == p_intf->p_sys->p_input->stream.pp_programs[i] )
        {
            /* don't lose p_item when we append into menu */
            p_item_active = p_item;
        }

        gtk_widget_show( p_item );

        /* setup signal hanling */
        gtk_signal_connect( GTK_OBJECT( p_item ), "toggled",
                        GTK_SIGNAL_FUNC( pf_toggle ),
                        (gpointer)(ptrdiff_t)( p_intf->p_sys->p_input->
                        stream.pp_programs[i]->i_number ) );

        gtk_menu_append( GTK_MENU( p_menu ), p_item );
    }

    /* link the new menu to the menubar item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_root ), p_menu );

    /* activation will call signals so we can only do it
     * when submenu is attached to menu - to get intf_window
     * We have to release the lock since input_ToggleES needs it */
    if( p_item_active != NULL )
    {
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( p_item_active ),
                                        TRUE );
    }

    /* be sure that menu is sensitive if more than 1 program */
    if( p_intf->p_sys->p_input->stream.i_pgrm_number > 1 )
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
    guint               i_item;
    guint               i;

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
    gtk_object_set_data( GTK_OBJECT( p_menu ), "p_intf", p_intf );

    /* special case for "off" item */
    snprintf( psz_name, GTK_MENU_LABEL_SIZE, _("None") );
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

    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );

#define ES p_intf->p_sys->p_input->stream.pp_es[i]
    /* create a set of language buttons and append them to the container */
    for( i = 0 ; i < p_intf->p_sys->p_input->stream.i_es_number ; i++ )
    {
        if( ( ES->i_cat == i_cat ) &&
            ( !ES->p_pgrm ||
              ES->p_pgrm ==
                 p_intf->p_sys->p_input->stream.p_selected_program ) )
        {
            i_item++;
            if( !p_intf->p_sys->p_input->stream.pp_es[i]->psz_desc ||
                !*p_intf->p_sys->p_input->stream.pp_es[i]->psz_desc )
            {
                snprintf( psz_name, GTK_MENU_LABEL_SIZE,
                          "Language %d", i_item );
                psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
            }
            else
            {
                strcpy( psz_name,
                        p_intf->p_sys->p_input->stream.pp_es[i]->psz_desc );
            }

            p_item = gtk_radio_menu_item_new_with_label( p_group, psz_name );
            p_group =
                gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_item ) );

            if( p_es == p_intf->p_sys->p_input->stream.pp_es[i] )
            {
                /* don't lose p_item when we append into menu */
                p_item_active = p_item;
            }

            gtk_widget_show( p_item );

            /* setup signal hanling */
            gtk_signal_connect( GTK_OBJECT( p_item ), "toggled",
                            GTK_SIGNAL_FUNC( pf_toggle ),
                            (gpointer)( p_intf->p_sys->p_input->stream.pp_es[i] ) );

            gtk_menu_append( GTK_MENU( p_menu ), p_item );
        }
    }

    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

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
    guint               i_title;
    guint               i_chapter;
    guint               i_title_nb;
    guint               i_chapter_nb;

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
    i_title_nb = p_intf->p_sys->p_input->stream.i_area_nb - 1;

    gtk_object_set_data( GTK_OBJECT( p_title_menu ), "p_intf", p_intf );

    /* loop on titles */
    for( i_title = 1 ; i_title <= i_title_nb ; i_title++ )
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
            gtk_object_set_data( GTK_OBJECT( p_title_submenu ),
                                 "p_intf", p_intf );
        }

        snprintf( psz_name, GTK_MENU_LABEL_SIZE, _("Title %d (%d)"), i_title,
                  p_intf->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb - 1);
        psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
#if 0
        if( pf_toggle == on_menubar_title_toggle )
        {
            p_title_item = gtk_radio_menu_item_new_with_label( p_title_group,
                                                           psz_name );
            p_title_group =
              gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_title_item ) );

            if( p_intf->p_sys->p_input->stream.pp_areas[i_title] ==
                         p_intf->p_sys->p_input->stream.p_selected_area )
            {
                p_item_active = p_title_item;
            }

            /* setup signal hanling */
            gtk_signal_connect( GTK_OBJECT( p_title_item ),
                     "toggled",
                     GTK_SIGNAL_FUNC( pf_toggle ),
                     (gpointer)(p_intf->p_sys->p_input->stream.pp_areas[i_title]) );

            if( p_intf->p_sys->p_input->stream.i_area_nb > 1 )
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
            gtk_object_set_data( GTK_OBJECT( p_chapter_menu ),
                                 "p_intf", p_intf );
            i_chapter_nb =
               p_intf->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb - 1;

            for( i_chapter = 1 ; i_chapter <= i_chapter_nb ; i_chapter++ )
            {
                /* we group chapters in packets of ten for small screens */
                if( ( i_chapter % 10 == 1 ) && ( i_chapter_nb > 20 ) )
                {
                    if( i_chapter != 1 )
                    {
                        gtk_menu_item_set_submenu(
                                    GTK_MENU_ITEM( p_chapter_menu_item ),
                                    p_chapter_submenu );
                        gtk_menu_append( GTK_MENU( p_chapter_menu ),
                                         p_chapter_menu_item );
                    }

                    snprintf( psz_name, GTK_MENU_LABEL_SIZE,
                              "%d - %d", i_chapter, i_chapter + 9 );
                    psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';
                    p_chapter_menu_item =
                            gtk_menu_item_new_with_label( psz_name );
                    gtk_widget_show( p_chapter_menu_item );
                    p_chapter_submenu = gtk_menu_new();
                    gtk_object_set_data( GTK_OBJECT( p_chapter_submenu ),
                                         "p_intf", p_intf );
                }

                snprintf( psz_name, GTK_MENU_LABEL_SIZE,
                          _("Chapter %d"), i_chapter );
                psz_name[ GTK_MENU_LABEL_SIZE - 1 ] = '\0';

                p_item = gtk_radio_menu_item_new_with_label(
                                                p_chapter_group, psz_name );
                p_chapter_group = gtk_radio_menu_item_group(
                                                GTK_RADIO_MENU_ITEM( p_item ) );
                gtk_widget_show( p_item );

#define p_area p_intf->p_sys->p_input->stream.pp_areas[i_title]
                if( ( p_area ==
                        p_intf->p_sys->p_input->stream.p_selected_area ) &&
                    ( p_area->i_part == i_chapter ) )
                {
                    p_item_active = p_item;
                }
#undef p_area

                /* setup signal hanling */
                gtk_signal_connect( GTK_OBJECT( p_item ),
                           "toggled",
                           GTK_SIGNAL_FUNC( pf_toggle ),
                           (gpointer)POS2DATA( i_title, i_chapter ) );

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

            if( p_intf->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb > 1 )
            {
                /* be sure that menu is sensitive */
                gtk_widget_set_sensitive( p_navigation, TRUE );
            }
#else
            GtkRadioMenu( p_intf, p_title_item, p_chapter_group, _("Chapter"),
                p_intf->p_sys->p_input->stream.pp_areas[i_title]->i_part_nb - 1,
                1, i_title * 100,
                p_intf->p_sys->p_input->stream.p_selected_area->i_part +
                p_intf->p_sys->p_input->stream.p_selected_area->i_id *100,
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
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( p_item_active ),
                                        TRUE );
    }
#if 0
    if( p_intf->p_sys->p_input->stream.i_area_nb > 1 )
    {
        /* be sure that menu is sensitive */
        gtk_widget_set_sensitive( p_navigation, TRUE );
    }
#endif

    return TRUE;
}

/*****************************************************************************
 * GtkSetupVarMenu :
 *****************************************************************************
 *
 *****************************************************************************/
static gint GtkSetupVarMenu( intf_thread_t * p_intf,
                             vlc_object_t * p_object,
                             GtkWidget *p_root,
                             char * psz_variable,
                             void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) )
{
    vlc_value_t         val, text, val_list, text_list;
    GtkWidget *         p_menu;
    GSList *            p_group = NULL;
    GtkWidget *         p_item;
    GtkWidget *         p_item_active = NULL;

    int                 i_item, i_type;

     /* temporary hack to avoid blank menu when an open menu is removed */
    if( GTK_MENU_ITEM(p_root)->submenu != NULL )
    {
        gtk_menu_popdown( GTK_MENU( GTK_MENU_ITEM(p_root)->submenu ) );
    }
    /* removes previous menu */
    gtk_menu_item_remove_submenu( GTK_MENU_ITEM( p_root ) );
    gtk_widget_set_sensitive( p_root, FALSE );

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_variable );

    /* Make sure we want to display the variable */
    if( i_type & VLC_VAR_HASCHOICE )
    {
        var_Change( p_object, psz_variable, VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int == 0 ) return FALSE;
    }

    /* Get the descriptive name of the variable */
    var_Change( p_object, psz_variable, VLC_VAR_GETTEXT, &text, NULL );

    /* get the current value */
    if( var_Get( p_object, psz_variable, &val ) < 0 )
    {
        return FALSE;
    }

    if( var_Change( p_object, psz_variable, VLC_VAR_GETLIST,
                    &val_list, &text_list ) < 0 )
    {
        if( i_type == VLC_VAR_STRING ) free( val.psz_string );
        return FALSE;
    }

    /* menu container */
    p_menu = gtk_menu_new();
    gtk_object_set_data( GTK_OBJECT( p_menu ), "p_intf", p_intf );

    for( i_item = 0; i_item < val_list.p_list->i_count; i_item++ )
    {
        switch( i_type & VLC_VAR_TYPE )
        {
        case VLC_VAR_STRING:
            p_item = gtk_radio_menu_item_new_with_label( p_group,
                     text_list.p_list->p_values[i_item].psz_string ?
                     text_list.p_list->p_values[i_item].psz_string :
                     val_list.p_list->p_values[i_item].psz_string );

            /* signal hanling for off */
            gtk_signal_connect( GTK_OBJECT( p_item ), "toggled",
                GTK_SIGNAL_FUNC ( pf_toggle ),
                /* FIXME memory leak */
                strdup(val_list.p_list->p_values[i_item].psz_string) );

            if( !strcmp( val.psz_string,
                         val_list.p_list->p_values[i_item].psz_string ) )
            {
                p_item_active = p_item;
            }
            break;
        case VLC_VAR_INTEGER:
            p_item = gtk_radio_menu_item_new_with_label( p_group,
                     text_list.p_list->p_values[i_item].psz_string ?
                     text_list.p_list->p_values[i_item].psz_string :
                     NULL /* FIXME */ );

            /* signal hanling for off */
            gtk_signal_connect( GTK_OBJECT( p_item ), "toggled",
                GTK_SIGNAL_FUNC ( pf_toggle ),
                (gpointer)val_list.p_list->p_values[i_item].i_int );

            if( val.i_int == val_list.p_list->p_values[i_item].i_int )
            {
                p_item_active = p_item;
            }
            break;
        default:
            /* FIXME */
            return FALSE;
        }

        p_group = gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_item ) );

        gtk_widget_show( p_item );

        gtk_menu_append( GTK_MENU( p_menu ), p_item );
    }

    /* link the new menu to the menubar item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_root ), p_menu );

    if( p_item_active )
    {
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(p_item_active),
                                        TRUE );
    }

    if( val_list.p_list->i_count > 0 )
    {
        gtk_widget_set_sensitive( p_root, TRUE );
    }

    /* clean up everything */
    if( i_type == VLC_VAR_STRING ) free( val.psz_string );
    var_Change( p_object, psz_variable, VLC_VAR_FREELIST,
                &val_list, &text_list );

    return TRUE;
}

/*****************************************************************************
 * GtkDeinterlaceMenus: update interactive menus of the interface
 *****************************************************************************
 *****************************************************************************/
static gint GtkDeinterlaceMenus( gpointer          p_data,
                                 GtkWidget *       p_root,
                                 void(*pf_toggle )( GtkCheckMenuItem *, gpointer ) )
{
    intf_thread_t *     p_intf;
    GtkWidget *         p_menu;
    GtkWidget *         p_separator;
    GtkWidget *         p_item;
    GtkWidget *         p_item_active;
    GSList *            p_group;
    guint               i_item;
    guint               i;
    char                *ppsz_deinterlace_mode[] = { "discard", "blend", "mean", "bob", "linear", NULL };
    char                *psz_deinterlace_option;
    char                *psz_filter;

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
    gtk_object_set_data( GTK_OBJECT( p_menu ), "p_intf", p_intf );

    /* special case for "off" item */
    p_item = gtk_radio_menu_item_new_with_label( p_group, "None" );
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


    /* search actual deinterlace mode */
    psz_filter = config_GetPsz( p_intf, "filter" );
    psz_deinterlace_option = strdup( "None" );

    if( psz_filter && *psz_filter )
    {
       if( strstr ( psz_filter, "deinterlace" ) )
       {
            vlc_value_t val;
            vout_thread_t *p_vout;

            p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                      FIND_ANYWHERE );
            if( p_vout &&
                var_Get( p_vout, "deinterlace-mode", &val ) == VLC_SUCCESS )
            {
                if( val.psz_string && *val.psz_string )
                {
                    free( psz_deinterlace_option );
                    psz_deinterlace_option = val.psz_string;
                }
                else if( val.psz_string ) free( val.psz_string );
            }

            if( p_vout ) vlc_object_release( p_vout );
       }
    }
    if( psz_filter )
        free( psz_filter );

    p_item_active = NULL;
    i_item = 0;

    /* create a set of deinteralce buttons and append them to the container */
    for( i = 0; ppsz_deinterlace_mode[i] != NULL; i++ )
    {
        i_item++;

        p_item = gtk_radio_menu_item_new_with_label( p_group, ppsz_deinterlace_mode[i] );
        p_group = gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM( p_item ) );
        gtk_widget_show( p_item );

        if( !strcmp( ppsz_deinterlace_mode[i], psz_deinterlace_option ) )
        {
            p_item_active = p_item;
        }
        /* setup signal hanling */
        gtk_signal_connect( GTK_OBJECT( p_item ), "toggled",
                            GTK_SIGNAL_FUNC( pf_toggle ),
                            NULL );

        gtk_menu_append( GTK_MENU( p_menu ), p_item );

    }

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
 * GtkSetupMenus: function that generates title/chapter/audio/subpic
 * menus with help from preceding functions
 *****************************************************************************
 * Function called with the lock on stream
 *****************************************************************************/
gint GtkSetupMenus( intf_thread_t * p_intf )
{
    es_descriptor_t *   p_audio_es;
    es_descriptor_t *   p_spu_es;
    GtkWidget *         p_menubar_menu;
    GtkWidget *         p_popup_menu;
    guint               i;

    p_intf->p_sys->b_chapter_update |= p_intf->p_sys->b_title_update;
    p_intf->p_sys->b_audio_update |= p_intf->p_sys->b_title_update |
                                     p_intf->p_sys->b_program_update;
    p_intf->p_sys->b_spu_update |= p_intf->p_sys->b_title_update |
                                   p_intf->p_sys->b_program_update;

    if( 1 )
    {
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                                     p_intf->p_sys->p_window ), "menubar_deinterlace" ) );
        p_popup_menu   = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                                     p_intf->p_sys->p_popup ), "popup_deinterlace" ) );

        p_intf->p_sys->b_deinterlace_update = VLC_TRUE;
        GtkDeinterlaceMenus( p_intf, p_menubar_menu, GtkMenubarDeinterlaceToggle );
        p_intf->p_sys->b_deinterlace_update = VLC_TRUE;
        GtkDeinterlaceMenus( p_intf, p_popup_menu, GtkPopupDeinterlaceToggle );

        p_intf->p_sys->b_deinterlace_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_program_update )
    {
        pgrm_descriptor_t * p_pgrm;

        if( p_intf->p_sys->p_input->stream.p_new_program )
        {
            p_pgrm = p_intf->p_sys->p_input->stream.p_new_program;
        }
        else
        {
            p_pgrm = p_intf->p_sys->p_input->stream.p_selected_program;
        }

        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_window ), "menubar_program" ) );
        GtkProgramMenu( p_intf, p_menubar_menu, p_pgrm,
                        GtkMenubarProgramToggle );

        p_intf->p_sys->b_program_update = VLC_TRUE;
        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_popup ), "popup_program" ) );
        GtkProgramMenu( p_intf, p_popup_menu, p_pgrm,
                        GtkPopupProgramToggle );

        p_intf->p_sys->b_program_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_title_update )
    {
        char            psz_title[5];

        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_window ), "menubar_title" ) );
        GtkRadioMenu( p_intf, p_menubar_menu, NULL, _("Title"), 1,
                      p_intf->p_sys->p_input->stream.i_area_nb - 1,
                      p_intf->p_sys->p_input->stream.p_selected_area->i_id,
                      GtkMenubarTitleToggle );

        snprintf( psz_title, 4, "%d",
                  p_intf->p_sys->p_input->stream.p_selected_area->i_id );
        psz_title[ 4 ] = '\0';
        gtk_label_set_text( p_intf->p_sys->p_label_title, psz_title );

        p_intf->p_sys->b_title_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_chapter_update )
    {
        char            psz_chapter[5];

        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                             p_intf->p_sys->p_popup ), "popup_navigation" ) );
        GtkTitleMenu( p_intf, p_popup_menu, GtkPopupNavigationToggle );
#if 0
        GtkRadioMenu( p_intf, p_menubar_menu, NULL, _("Title"), 1,
                        p_intf->p_sys->p_input->stream.i_area_nb - 1,
                        p_intf->p_sys->p_input->stream.p_selected_area->i_id,
                        on_menubar_chapter_toggle );
#endif

        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                             p_intf->p_sys->p_window ), "menubar_chapter" ) );

        GtkRadioMenu( p_intf, p_menubar_menu, NULL, _("Chapter"), 1,
                        p_intf->p_sys->p_input->stream.p_selected_area->i_part_nb - 1,
                        p_intf->p_sys->p_input->stream.p_selected_area->i_part,
                        GtkMenubarChapterToggle );


        snprintf( psz_chapter, 4, "%d",
                  p_intf->p_sys->p_input->stream.p_selected_area->i_part );
        psz_chapter[ 4 ] = '\0';
        gtk_label_set_text( p_intf->p_sys->p_label_chapter, psz_chapter );

        p_intf->p_sys->i_part =
                p_intf->p_sys->p_input->stream.p_selected_area->i_part;

        p_intf->p_sys->b_chapter_update = VLC_FALSE;
    }

    /* look for selected ES */
    p_audio_es = NULL;
    p_spu_es = NULL;

    for( i = 0 ; i < p_intf->p_sys->p_input->stream.i_selected_es_number ; i++ )
    {
        if( p_intf->p_sys->p_input->stream.pp_selected_es[i]->i_cat == AUDIO_ES )
        {
            p_audio_es = p_intf->p_sys->p_input->stream.pp_selected_es[i];
        }

        if( p_intf->p_sys->p_input->stream.pp_selected_es[i]->i_cat == SPU_ES )
        {
            p_spu_es = p_intf->p_sys->p_input->stream.pp_selected_es[i];
        }
    }

    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

    /* audio menus */
    if( p_intf->p_sys->b_audio_update )
    {
        /* find audio root menu */
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                             p_intf->p_sys->p_window ), "menubar_audio" ) );

        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                     p_intf->p_sys->p_popup ), "popup_language" ) );

        p_intf->p_sys->b_audio_update = VLC_TRUE;
        GtkLanguageMenus( p_intf, p_menubar_menu, p_audio_es, AUDIO_ES,
                            GtkMenubarAudioToggle );
        p_intf->p_sys->b_audio_update = VLC_TRUE;
        GtkLanguageMenus( p_intf, p_popup_menu, p_audio_es, AUDIO_ES,
                            GtkPopupAudioToggle );

        p_intf->p_sys->b_audio_update = VLC_FALSE;
    }

    /* sub picture menus */
    if( p_intf->p_sys->b_spu_update )
    {
        /* find spu root menu */
        p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                          p_intf->p_sys->p_window ), "menubar_subpictures" ) );

        p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                     p_intf->p_sys->p_popup ), "popup_subpictures" ) );

        p_intf->p_sys->b_spu_update = VLC_TRUE;
        GtkLanguageMenus( p_intf, p_menubar_menu, p_spu_es, SPU_ES,
                            GtkMenubarSubtitleToggle  );
        p_intf->p_sys->b_spu_update = VLC_TRUE;
        GtkLanguageMenus( p_intf, p_popup_menu, p_spu_es, SPU_ES,
                            GtkPopupSubtitleToggle );

        p_intf->p_sys->b_spu_update = VLC_FALSE;
    }
    /* create audio channels and device menu (in menubar _and_ popup */
    if( p_intf->p_sys->b_aout_update )
    {
        aout_instance_t *p_aout;

        p_aout = (aout_instance_t*)vlc_object_find( p_intf, VLC_OBJECT_AOUT, FIND_ANYWHERE );

        if( p_aout != NULL )
        {
            vlc_value_t val;
            val.b_bool = VLC_FALSE;

            var_Set( (vlc_object_t *)p_aout, "intf-change", val );

            /* audio-channels */
            p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                              p_intf->p_sys->p_window ), "menubar_audio_channels" ) );
            p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                         p_intf->p_sys->p_popup ), "popup_audio_channels" ) );
            GtkSetupVarMenu( p_intf, (vlc_object_t *)p_aout, p_popup_menu,
                              "audio-channels",  GtkPopupAoutChannelsToggle );
            GtkSetupVarMenu( p_intf, (vlc_object_t *)p_aout, p_menubar_menu,
                              "audio-channels",  GtkPopupAoutChannelsToggle );

            /* audio-device */
            p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                              p_intf->p_sys->p_window ), "menubar_audio_device" ) );
            p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                         p_intf->p_sys->p_popup ), "popup_audio_device" ) );
            GtkSetupVarMenu( p_intf, (vlc_object_t *)p_aout, p_popup_menu,
                              "audio-device",  GtkPopupAoutDeviceToggle );
            GtkSetupVarMenu( p_intf, (vlc_object_t *)p_aout, p_menubar_menu,
                              "audio-device",  GtkPopupAoutDeviceToggle );

            vlc_object_release( (vlc_object_t *)p_aout );
        }
        p_intf->p_sys->b_aout_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_vout_update )
    {
        vout_thread_t *p_vout;

        p_vout = (vout_thread_t*)vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );

        if( p_vout != NULL )
        {
            vlc_value_t val;
            val.b_bool = VLC_FALSE;

            var_Set( (vlc_object_t *)p_vout, "intf-change", val );

            /* video-device */
            p_menubar_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                              p_intf->p_sys->p_window ), "menubar_video_device" ) );
            p_popup_menu = GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                         p_intf->p_sys->p_popup ), "popup_video_device" ) );
            GtkSetupVarMenu( p_intf, (vlc_object_t *)p_vout, p_popup_menu,
                              "video-device",  GtkPopupVoutDeviceToggle );
            GtkSetupVarMenu( p_intf, (vlc_object_t *)p_vout, p_menubar_menu,
                              "video-device",  GtkPopupVoutDeviceToggle );


            vlc_object_release( (vlc_object_t *)p_vout );
        }
        p_intf->p_sys->b_vout_update = VLC_FALSE;
    }
    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );

    return TRUE;
}

