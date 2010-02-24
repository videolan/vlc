/*****************************************************************************
 * maemo_menus.c : menus creation for the maemo plugin
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc_common.h>
#include <vlc_interface.h>

#include <gtk/gtk.h>

#include "maemo.h"
#include "maemo_callbacks.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static GtkMenu *Populate( intf_thread_t *p_intf, GtkMenu *menu,
                          const char **varnames, vlc_object_t **objects,
                          unsigned int elements );

/*****************************************************************************
 * Utility functions
 *****************************************************************************/
static input_thread_t *get_input(intf_thread_t *p_intf)
{
    return p_intf->p_sys->p_input;
}

static vlc_object_t *get_vout(intf_thread_t *p_intf)
{
    return (vlc_object_t *)(p_intf->p_sys->p_input ?
                            input_GetVout( p_intf->p_sys->p_input ) : 0);
}

static vlc_object_t *get_aout(intf_thread_t *p_intf)
{
    return (vlc_object_t *)(p_intf->p_sys->p_input ?
                            input_GetAout( p_intf->p_sys->p_input ) : 0);
}

static gint quit_event( GtkWidget *widget, gpointer data )
{
    intf_thread_t *p_intf = (intf_thread_t *)data;
    (void)widget;
    libvlc_Quit( p_intf->p_libvlc );
    return TRUE;
}

/*****************************************************************************
 * Definitions of variables for the dynamic menus
 *****************************************************************************/
#define SIZE_LIST 20

#define PUSH_VAR( var ) \
    ppsz_varnames[index] = var; \
    p_objects[index] = VLC_OBJECT(p_object); \
    if(index < SIZE_LIST - 1) index++; \
    ppsz_varnames[index] = 0; p_objects[index] = 0;

#define PUSH_INPUTVAR( var ) \
    ppsz_varnames[index] = var; \
    p_objects[index] = VLC_OBJECT(p_input); \
    if(index < SIZE_LIST - 1) index++; \
    ppsz_varnames[index] = 0; p_objects[index] = 0;

#define ADD_MENU_ITEM( label, callback ) \
    item = gtk_menu_item_new_with_label( label ); \
    gtk_menu_append( main_menu, item ); \
    g_signal_connect( GTK_OBJECT( item ), "activate", G_CALLBACK( callback ), \
                      p_intf );

#define ADD_SEPARATOR() \
    item = gtk_separator_menu_item_new(); \
    gtk_menu_append( main_menu, item );



static GtkMenu *create_video_menu( intf_thread_t *p_intf )
{
    vlc_object_t *p_object = get_vout(p_intf);
    input_thread_t *p_input = get_input(p_intf);;

    vlc_object_t *p_objects[SIZE_LIST];
    const char *ppsz_varnames[SIZE_LIST];
    int index = 0;

    PUSH_INPUTVAR( "video-es" );
    PUSH_INPUTVAR( "spu-es" );
    PUSH_VAR( "fullscreen" );
    PUSH_VAR( "video-wallpaper" );
    PUSH_VAR( "video-snapshot" );
    PUSH_VAR( "zoom" );
    PUSH_VAR( "autoscale" );
    PUSH_VAR( "aspect-ratio" );
    PUSH_VAR( "crop" );
    PUSH_VAR( "deinterlace" );
    PUSH_VAR( "deinterlace-mode" );
    PUSH_VAR( "postprocess" );

    GtkWidget *menu = gtk_menu_new();
    return Populate( p_intf, GTK_MENU(menu), ppsz_varnames, p_objects, index );
}

static GtkMenu *create_audio_menu( intf_thread_t *p_intf )
{
    vlc_object_t *p_object = get_aout(p_intf);
    input_thread_t *p_input = get_input(p_intf);;

    vlc_object_t *p_objects[SIZE_LIST];
    const char *ppsz_varnames[SIZE_LIST];
    int index = 0;

    PUSH_INPUTVAR( "audio-es" );
    PUSH_VAR( "audio-channels" );
    PUSH_VAR( "audio-device" );
    PUSH_VAR( "visual" );

    GtkWidget *menu = gtk_menu_new();
    return Populate( p_intf, GTK_MENU(menu), ppsz_varnames, p_objects, index );
}

static GtkMenu *create_input_menu( intf_thread_t *p_intf )
{
    input_thread_t *p_object = get_input(p_intf);

    vlc_object_t *p_objects[SIZE_LIST];
    const char *ppsz_varnames[SIZE_LIST];
    int index = 0;

    PUSH_VAR( "bookmark" );
    PUSH_VAR( "title" );
    PUSH_VAR( "chapter" );
    PUSH_VAR( "navigation" );
    PUSH_VAR( "program" );

    GtkWidget *menu = gtk_menu_new();
    return Populate( p_intf, GTK_MENU(menu), ppsz_varnames, p_objects, index );
}

static void toplevel_menu_callback(GtkMenuItem *menuitem, gpointer data)
{
    intf_thread_t *p_intf = (intf_thread_t *)data;
    GtkMenu *(*pf_menu)(intf_thread_t *) = 0;
    GtkMenu *menu;

    if(menuitem == p_intf->p_sys->menu_input) pf_menu = create_input_menu;
    else if(menuitem == p_intf->p_sys->menu_audio) pf_menu = create_audio_menu;
    else if(menuitem == p_intf->p_sys->menu_video) pf_menu = create_video_menu;
    else return;

    menu = GTK_MENU(gtk_menu_item_get_submenu(menuitem));
    if(menu) gtk_object_destroy( GTK_OBJECT(menu) );
    menu = pf_menu(p_intf);
    gtk_menu_item_set_submenu(menuitem, GTK_WIDGET(menu));
    gtk_widget_show_all( GTK_WIDGET(menuitem) );
}

GtkWidget *create_menu( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    GtkWidget *main_menu, *item;

    /* Creating the main menu */
    main_menu = gtk_menu_new();

    /* Filling the menu */
    ADD_MENU_ITEM( "Open", open_cb );
    ADD_MENU_ITEM( "Open Address", open_address_cb );
    ADD_MENU_ITEM( "Open Webcam", open_webcam_cb );
    ADD_SEPARATOR();

    item = gtk_menu_item_new_with_label ("Playback");
    p_sys->menu_input = GTK_MENU_ITEM(item);
    gtk_menu_bar_append(main_menu, item);
    g_signal_connect( GTK_OBJECT(item), "activate",
                      G_CALLBACK( toplevel_menu_callback ), p_intf );

    item = gtk_menu_item_new_with_label ("Audio");
    p_sys->menu_audio = GTK_MENU_ITEM(item);
    gtk_menu_bar_append(main_menu, item);
    g_signal_connect( GTK_OBJECT(item), "activate",
                      G_CALLBACK( toplevel_menu_callback ), p_intf );

    item = gtk_menu_item_new_with_label ("Video");
    p_sys->menu_video = GTK_MENU_ITEM(item);
    gtk_menu_bar_append(main_menu, item);
    g_signal_connect( GTK_OBJECT(item), "activate",
                      G_CALLBACK( toplevel_menu_callback ), p_intf );

    toplevel_menu_callback(p_sys->menu_input, p_intf);
    toplevel_menu_callback(p_sys->menu_video, p_intf);
    toplevel_menu_callback(p_sys->menu_audio, p_intf);

    ADD_SEPARATOR();
    ADD_MENU_ITEM( "Exit", quit_event );

    gtk_widget_show_all( main_menu );
    return main_menu;
}

/*************************************************************************
 * Builders for automenus
 *************************************************************************/
enum
{
    ITEM_NORMAL,
    ITEM_CHECK,
    ITEM_RADIO
};

typedef struct VlcMenuItemClass
{
  GtkRadioMenuItemClass menuitemclass;

} VlcMenuItemClass;

typedef struct VlcMenuItem
{
  GtkRadioMenuItem menuitem;

  vlc_object_t *p_obj;
  int i_type;
  vlc_value_t val;
  const char *psz_var;

} VlcMenuItem;

static void vlc_menu_item_destroy (GtkObject *object)
{
  VlcMenuItem *menuitem = (VlcMenuItem *)object;

  if(menuitem->i_type == VLC_VAR_STRING && menuitem->val.psz_string)
      free(menuitem->val.psz_string);
  gtk_object_destroy( object );
}

static void vlc_menu_item_class_init (VlcMenuItemClass *klass)
{
    GtkObjectClass *object_class = (GtkObjectClass*)klass;
    object_class->destroy = vlc_menu_item_destroy;
}

static void vlc_menu_item_init (VlcMenuItem *menuitem){(void)menuitem;}

static GtkType vlc_menu_item_get_type (void)
{
    static GtkType vlc_menu_item_type = 0;

    if (!vlc_menu_item_type)
    {
        static const GtkTypeInfo vlc_menu_item_info =
        {
            (char *)"VlcMenuItem",
            sizeof (VlcMenuItem),
            sizeof (VlcMenuItemClass),
            (GtkClassInitFunc) vlc_menu_item_class_init,
            (GtkObjectInitFunc) vlc_menu_item_init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL,
        };

        vlc_menu_item_type = gtk_type_unique (GTK_TYPE_MENU_ITEM, &vlc_menu_item_info);
    }

    return vlc_menu_item_type;
}

static GtkType vlc_check_menu_item_get_type (void)
{
    static GtkType vlc_check_menu_item_type = 0;

    if (!vlc_check_menu_item_type)
    {
        static const GtkTypeInfo vlc_check_menu_item_info =
        {
            (char *)"VlcCheckMenuItem",
            sizeof (VlcMenuItem),
            sizeof (VlcMenuItemClass),
            (GtkClassInitFunc) vlc_menu_item_class_init,
            (GtkObjectInitFunc) vlc_menu_item_init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL,
        };

        vlc_check_menu_item_type = gtk_type_unique (GTK_TYPE_CHECK_MENU_ITEM, &vlc_check_menu_item_info);
    }

    return vlc_check_menu_item_type;
}

static GtkType vlc_radio_menu_item_get_type (void)
{
    static GtkType vlc_radio_menu_item_type = 0;

    if (!vlc_radio_menu_item_type)
    {
        static const GtkTypeInfo vlc_radio_menu_item_info =
        {
            (char *)"VlcRadioMenuItem",
            sizeof (VlcMenuItem),
            sizeof (VlcMenuItemClass),
            (GtkClassInitFunc) vlc_menu_item_class_init,
            (GtkObjectInitFunc) vlc_menu_item_init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL,
        };

        vlc_radio_menu_item_type = gtk_type_unique (GTK_TYPE_RADIO_MENU_ITEM, &vlc_radio_menu_item_info);
    }

    return vlc_radio_menu_item_type;
}

static GtkWidget *vlc_menu_item_new (vlc_object_t *p_obj, int i_type,
                                     vlc_value_t val, const char *var)
{
    VlcMenuItem *item;

    switch(i_type)
    {
    case ITEM_CHECK:
      item = (VlcMenuItem *)gtk_type_new (vlc_check_menu_item_get_type ());
      break;
    case ITEM_RADIO:
      item = (VlcMenuItem *)gtk_type_new (vlc_radio_menu_item_get_type ());
      break;
    default:
      item = (VlcMenuItem *)gtk_type_new (vlc_menu_item_get_type ());
      break;
    }
    item->p_obj = p_obj;
    item->i_type = i_type;
    item->val = val;
    item->psz_var = var;
    return GTK_WIDGET (item);
}

/****/

static int CreateChoicesMenu( intf_thread_t *p_intf, GtkMenu *submenu, const char *psz_var,
                              vlc_object_t *p_object, bool b_root );

static void menu_callback(GtkMenuItem *menuitem, gpointer user_data)
{
    VlcMenuItem *item = (VlcMenuItem *)menuitem;
    vlc_object_t *p_object = item->p_obj;
    (void)user_data;
    if( p_object == NULL ) return;
    var_Set( p_object, item->psz_var, item->val );
}

static void CreateAndConnect( intf_thread_t *p_intf,
        GtkMenu *menu, const char *psz_var,
        const char *text, const char *help,
        int i_item_type, vlc_object_t *p_obj,
        vlc_value_t val, int i_val_type,
        bool checked )
{
    GtkMenuItem *menu_item =
        (GtkMenuItem *)vlc_menu_item_new (p_obj, i_item_type, val, psz_var );

    (void)help; (void)i_val_type;

#if GTK_CHECK_VERSION(2,16,0)
    gtk_menu_item_set_label (menu_item, text ? text : psz_var);
#else
    GtkWidget *accel_label = gtk_accel_label_new(text ? text : psz_var);
    gtk_misc_set_alignment(GTK_MISC (accel_label), 0.0, 0.5);
    gtk_container_add (GTK_CONTAINER (menu_item), accel_label);
    gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), GTK_WIDGET(menu_item));
    gtk_widget_show (accel_label);
#endif /* GTK_CHECK_VERSION(2,16,0) */

    gtk_menu_append( GTK_WIDGET(menu), GTK_WIDGET(menu_item) );

    if( i_item_type == ITEM_CHECK || i_item_type == ITEM_RADIO )
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), checked);

    g_signal_connect( GTK_OBJECT(menu_item), "activate", G_CALLBACK( menu_callback ),
                      p_intf );
}

static bool IsMenuEmpty( const char *psz_var,
                         vlc_object_t *p_object,
                         bool b_root )
{
    vlc_value_t val, val_list;
    int i_type, i_result, i;

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_var );

    /* Check if we want to display the variable */
    if( !( i_type & VLC_VAR_HASCHOICE ) ) return false;

    var_Change( p_object, psz_var, VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int == 0 ) return true;

    if( ( i_type & VLC_VAR_TYPE ) != VLC_VAR_VARIABLE )
    {
        if( val.i_int == 1 && b_root ) return true;
        else return false;
    }

    /* Check children variables in case of VLC_VAR_VARIABLE */
    if( var_Change( p_object, psz_var, VLC_VAR_GETLIST, &val_list, NULL ) < 0 )
    {
        return true;
    }

    for( i = 0, i_result = true; i < val_list.p_list->i_count; i++ )
    {
        if( !IsMenuEmpty( val_list.p_list->p_values[i].psz_string,
                    p_object, false ) )
        {
            i_result = false;
            break;
        }
    }

    /* clean up everything */
    var_FreeList( &val_list, NULL );

    return i_result;
}

static void UpdateItem( intf_thread_t *p_intf, GtkMenu *menu,
                 const char *psz_var, vlc_object_t *p_object, bool b_submenu )
{
    vlc_value_t val, text;
    int i_type;

    /* Check the type of the object variable */
    /* This HACK is needed so we have a radio button for audio and video tracks
       instread of a checkbox */
    if( !strcmp( psz_var, "audio-es" )
     || !strcmp( psz_var, "video-es" ) )
        i_type = VLC_VAR_INTEGER | VLC_VAR_HASCHOICE;
    else
        i_type = var_Type( p_object, psz_var );

    switch( i_type & VLC_VAR_TYPE )
    {
        case VLC_VAR_VOID:
        case VLC_VAR_BOOL:
        case VLC_VAR_VARIABLE:
        case VLC_VAR_STRING:
        case VLC_VAR_INTEGER:
        case VLC_VAR_FLOAT:
            break;
        default:
            /* Variable doesn't exist or isn't handled */
            return;
    }

    /* Make sure we want to display the variable */
    if( !g_list_length(GTK_MENU_SHELL(menu)->children) && IsMenuEmpty( psz_var, p_object, true ) )
    {
        return;
    }

    /* Get the descriptive name of the variable */
    int i_ret = var_Change( p_object, psz_var, VLC_VAR_GETTEXT, &text, NULL );
    if( i_ret != VLC_SUCCESS )
    {
        text.psz_string = NULL;
    }

    /* Some specific stuff */
    bool forceDisabled = false;
    if( !strcmp( psz_var, "spu-es" ) )
    {
        vlc_object_t *p_vout = get_vout(p_intf);
        forceDisabled = ( p_vout == NULL );
        if( p_vout ) vlc_object_release( p_vout );
    }

    if( i_type & VLC_VAR_HASCHOICE )
    {
        /* Append choices menu */
        if( b_submenu )
        {
            GtkWidget *item =
                gtk_menu_item_new_with_label( text.psz_string ? text.psz_string : psz_var );
            GtkWidget *submenu = gtk_menu_new( );
            gtk_menu_append( menu, item );
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
            if( CreateChoicesMenu( p_intf, GTK_MENU(submenu), psz_var, p_object, true ) )
                gtk_widget_set_sensitive(item, false);

            if( forceDisabled )
                gtk_widget_set_sensitive(item, false);
        }
        else
        {

            if( CreateChoicesMenu( p_intf, menu, psz_var, p_object, true ) )
                gtk_widget_set_sensitive(menu, false);
        }
        FREENULL( text.psz_string );
        return;
    }

    switch( i_type & VLC_VAR_TYPE )
    {
        case VLC_VAR_VOID:
            var_Get( p_object, psz_var, &val );
            CreateAndConnect( p_intf, menu, psz_var, text.psz_string, "",
                              ITEM_NORMAL, p_object, val, i_type, false );
            break;

        case VLC_VAR_BOOL:
            var_Get( p_object, psz_var, &val );
            val.b_bool = !val.b_bool;
            CreateAndConnect( p_intf, menu, psz_var, text.psz_string, "",
                              ITEM_CHECK, p_object, val, i_type, !val.b_bool );
            break;
    }
    FREENULL( text.psz_string );
}

/** HACK for the navigation submenu:
 * "title %2i" variables take the value 0 if not set
 */
static bool CheckTitle( vlc_object_t *p_object, const char *psz_var )
{
    int i_title = 0;
    if( sscanf( psz_var, "title %2i", &i_title ) <= 0 )
        return true;

    int i_current_title = var_GetInteger( p_object, "title" );
    return ( i_title == i_current_title );
}


static int CreateChoicesMenu( intf_thread_t *p_intf, GtkMenu *submenu, const char *psz_var,
                       vlc_object_t *p_object, bool b_root )
{
    vlc_value_t val, val_list, text_list;
    int i_type, i;

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_var );

    /* Make sure we want to display the variable */
    if( !g_list_length(GTK_MENU_SHELL(submenu)->children) &&
        IsMenuEmpty( psz_var, p_object, b_root ) )
        return VLC_EGENERIC;

    switch( i_type & VLC_VAR_TYPE )
    {
        case VLC_VAR_VOID:
        case VLC_VAR_BOOL:
        case VLC_VAR_VARIABLE:
        case VLC_VAR_STRING:
        case VLC_VAR_INTEGER:
        case VLC_VAR_FLOAT:
            break;
        default:
            /* Variable doesn't exist or isn't handled */
            return VLC_EGENERIC;
    }

    if( var_Change( p_object, psz_var, VLC_VAR_GETLIST,
                    &val_list, &text_list ) < 0 )
    {
        return VLC_EGENERIC;
    }

#define CURVAL val_list.p_list->p_values[i]
#define CURTEXT text_list.p_list->p_values[i].psz_string

    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t another_val;
        char string[16] = {0};
        char *menutext = string;

        switch( i_type & VLC_VAR_TYPE )
        {
            case VLC_VAR_VARIABLE:
              {
                GtkWidget *subsubmenu = gtk_menu_new();
                GtkWidget *submenuitem =
                    gtk_menu_item_new_with_label( CURTEXT ? CURTEXT : CURVAL.psz_string );
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenuitem), subsubmenu);
                gtk_menu_append( submenu, submenuitem );
                CreateChoicesMenu( p_intf, GTK_MENU(subsubmenu), CURVAL.psz_string, p_object, false );
                break;
              }

            case VLC_VAR_STRING:
                var_Get( p_object, psz_var, &val );
                another_val.psz_string = strdup( CURVAL.psz_string );
                menutext = CURTEXT ? CURTEXT : another_val.psz_string;
                CreateAndConnect( p_intf, submenu, psz_var, menutext, "",
                                  ITEM_RADIO, p_object, another_val, i_type,
                        val.psz_string && !strcmp( val.psz_string, CURVAL.psz_string ) );
                free( val.psz_string );
                break;

            case VLC_VAR_INTEGER:
                var_Get( p_object, psz_var, &val );
                if( CURTEXT ) menutext = CURTEXT;
                else snprintf( menutext, sizeof(string)-1, "%d", CURVAL.i_int );
                CreateAndConnect( p_intf, submenu, psz_var, menutext, "",
                                  ITEM_RADIO, p_object, CURVAL, i_type,
                        ( CURVAL.i_int == val.i_int )
                        && CheckTitle( p_object, psz_var ) );
                break;

            case VLC_VAR_FLOAT:
                var_Get( p_object, psz_var, &val );
                if( CURTEXT ) menutext = CURTEXT;
                else snprintf( menutext, sizeof(string)-1, "%.2f", CURVAL.f_float );
                CreateAndConnect( p_intf, submenu, psz_var, menutext, "",
                                  ITEM_RADIO, p_object, CURVAL, i_type,
                                  CURVAL.f_float == val.f_float );
                break;

            default:
                break;
        }
    }

    /* clean up everything */
    var_FreeList( &val_list, &text_list );

#undef CURVAL
#undef CURTEXT
    return !g_list_length(GTK_MENU_SHELL(submenu)->children) ? VLC_EGENERIC : VLC_SUCCESS;
}

static GtkMenu *Populate( intf_thread_t *p_intf, GtkMenu *menu,
                          const char **varnames, vlc_object_t **objects,
                          unsigned int elements)
{
    for( unsigned int i = 0; i < elements ; i++ )
    {
        if( (!varnames[i] || !*varnames[i]) &&
            g_list_length(GTK_MENU_SHELL(menu)->children) )
        {
            gtk_menu_append( menu, gtk_separator_menu_item_new() );
            continue;
        }

        if( objects[i] )
        {
            UpdateItem( p_intf, menu, varnames[i], objects[i], true );
        }
    }

    if(!g_list_length(GTK_MENU_SHELL(menu)->children))
    {
        GtkWidget *menuitem = gtk_menu_item_new_with_label( "Empty" );
        gtk_menu_append( menu, menuitem );
        gtk_widget_set_sensitive(menuitem, false);
    }

    return menu;
}
