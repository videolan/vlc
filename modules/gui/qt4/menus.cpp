/*****************************************************************************
 * menus.cpp : Qt menus
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id: menus.cpp 15858 2006-06-09 21:35:13Z gbazin $
 *
 * Authors: Cl√ment Stenac <zorglub@videolan.org>
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

#include "menus.hpp"
#include "dialogs_provider.hpp"
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSignalMapper>

enum
{
    ITEM_NORMAL,
    ITEM_CHECK,
    ITEM_RADIO
};

static QActionGroup *currentGroup;

/*****************************************************************************
 * Static menu helpers
 * These create already mapped and connected menus
 *****************************************************************************/

#define STATIC_ADD( text, help, icon, slot ) { QAction *action = menu->addAction( text, THEDP, SLOT( slot ) ); }

QMenu *QVLCMenu::FileMenu()
{
    QMenu *menu = new QMenu();

    STATIC_ADD( _("Quick &Open File...") , "", NULL, simpleOpenDialog() );
    STATIC_ADD( _("&Advanced Open..." ), "", NULL, openDialog() );
    menu->addSeparator();
    STATIC_ADD( _("Streaming..."), "", NULL, streamingDialog() );
    menu->addSeparator();
    STATIC_ADD( _("&Quit") , "", NULL, quit() );

    return menu;
}

#if 0
QMenu *OpenStreamMenu( intf_thread_t *p_intf )
{
    QMenu *menu = new QMenu;
    menu->Append( OpenFileSimple_Event, wxU(_("Quick &Open File...")) );
    menu->Append( OpenFile_Event, wxU(_("Open &File...")) );
    menu->Append( OpenDirectory_Event, wxU(_("Open D&irectory...")) );
    menu->Append( OpenDisc_Event, wxU(_("Open &Disc...")) );
    menu->Append( OpenNet_Event, wxU(_("Open &Network Stream...")) );
    menu->Append( OpenCapture_Event, wxU(_("Open &Capture Device...")) );
    return menu;
}

wxMenu *MiscMenu( intf_thread_t *p_intf )
{
    wxMenu *menu = new wxMenu;
    menu->Append( MediaInfo_Event, wxU(_("Media &Info...")) );
    menu->Append( Messages_Event, wxU(_("&Messages...")) );
    menu->Append( Preferences_Event, wxU(_("&Preferences...")) );
    return menu;
}

#endif
/*****************************************************************************
 * Builders for the dynamic menus
 *****************************************************************************/
#define PUSH_VAR( var ) rs_varnames.push_back( var ); \
                        ri_objects.push_back( p_object->i_object_id )

static int InputAutoMenuBuilder( vlc_object_t *p_object,
                                 vector<int> &ri_objects,
                                 vector<const char *> &rs_varnames )
{
    PUSH_VAR( "bookmark");
    PUSH_VAR( "title" );
    PUSH_VAR ("chapter" );
    PUSH_VAR( "program" );
    PUSH_VAR( "navigation" );
    PUSH_VAR( "dvd_menus" );
    return VLC_SUCCESS;
}

static int VideoAutoMenuBuilder( vlc_object_t *p_object, 
                                 vector<int> &ri_objects,
                                 vector<const char *> &rs_varnames )
{
    PUSH_VAR( "fullscreen" );
    PUSH_VAR( "zoom" );
    PUSH_VAR( "deinterlace" );
    PUSH_VAR( "aspect-ratio" );
    PUSH_VAR( "crop" );
    PUSH_VAR( "video-on-top" );
    PUSH_VAR( "directx-wallpaper" );
    PUSH_VAR( "video-snapshot" );

    vlc_object_t *p_dec_obj = (vlc_object_t *)vlc_object_find( p_object,
                                                 VLC_OBJECT_DECODER,
                                                 FIND_PARENT );
    if( p_dec_obj != NULL )
    {
        PUSH_VAR( "ffmpeg-pp-q" );
        vlc_object_release( p_dec_obj );
    }
    return VLC_SUCCESS;
}

static int AudioAutoMenuBuilder( vlc_object_t *p_object,
                                 vector<int> &ri_objects,
                                 vector<const char *> &rs_varnames )
{
    PUSH_VAR( "audio-device" );
    PUSH_VAR( "audio-channels" );
    PUSH_VAR( "visual" );
    PUSH_VAR( "equalizer" );
    return VLC_SUCCESS;
}

static int IntfAutoMenuBuilder( intf_thread_t *p_intf, vector<int> &ri_objects,
                             vector<const char *> &rs_varnames, bool is_popup )
{
    /* vlc_object_find is needed because of the dialogs provider case */
    vlc_object_t *p_object;
    p_object = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_INTF,
                                                FIND_PARENT );
    if( p_object != NULL )
    {
        if( is_popup )
        {
            PUSH_VAR( "intf-switch" );
        }
        else
        {
            PUSH_VAR( "intf-switch" );
        }
        PUSH_VAR( "intf-add" );
        PUSH_VAR( "intf-skins" );
        vlc_object_release( p_object );
    }
    return VLC_SUCCESS;
}

#undef PUSH_VAR
/*****************************************************************************
 * Popup menus
 *****************************************************************************/

#define PUSH_VAR( var ) as_varnames.push_back( var ); \
                        ai_objects.push_back( p_object->i_object_id )

#define PUSH_SEPARATOR if( ai_objects.size() != i_last_separator ) { \
                            ai_objects.push_back( 0 ); \
                            as_varnames.push_back( "" ); \
                            i_last_separator = ai_objects.size(); }

#define POPUP_BOILERPLATE \
    unsigned int i_last_separator = 0; \
    vector<int> ai_objects; \
    vector<const char *> as_varnames; \
    playlist_t *p_playlist = (playlist_t *) vlc_object_find( p_intf, \
                                          VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );\
    if( !p_playlist ) \
        return; \
    input_thread_t *p_input = p_playlist->p_input

#define CREATE_POPUP    \
    QMenu *popupmenu = new QMenu(); \
    QVLCMenu::Populate( popupmenu, as_varnames, ai_objects ); \
    p_intf->p_sys->p_popup_menu = &popupmenu; \
    p_intf->p_sys->p_popup_menu = NULL; \
    i_last_separator = 0 // stop compiler warning 
///    p_parent->PopupMenu( &popupmenu, pos.x, pos.y ); \ /// TODO

#define POPUP_STATIC_ENTRIES \
    if( p_input != NULL ) \
    { \
        vlc_value_t val; \
        popupmenu.InsertSeparator( 0 ); \
        popupmenu.Insert( 0, Stop_Event, wxU(_("Stop")) ); \
        popupmenu.Insert( 0, Previous_Event, wxU(_("Previous")) ); \
        popupmenu.Insert( 0, Next_Event, wxU(_("Next")) ); \
        var_Get( p_input, "state", &val ); \
        if( val.i_int == PAUSE_S ) \
            popupmenu.Insert( 0, Play_Event, wxU(_("Play")) ); \
        else \
            popupmenu.Insert( 0, Pause_Event, wxU(_("Pause")) ); \
         \
        vlc_object_release( p_input ); \
    } \
    else \
    { \
        if( p_playlist && p_playlist->i_size ) \
        { \
            popupmenu.InsertSeparator( 0 ); \
            popupmenu.Insert( 0, Play_Event, wxU(_("Play")) ); \
        } \
        if( p_playlist ) vlc_object_release( p_playlist ); \
    } \
    \
    popupmenu.Append( MenuDummy_Event, wxU(_("Miscellaneous")), \
                      MiscMenu( p_intf ), wxT("") )

void QVLCMenu::VideoPopupMenu( intf_thread_t *p_intf, const QPoint &pos )
{
    POPUP_BOILERPLATE;
    if( p_input )
    {
        vlc_object_yield( p_input );
        as_varnames.push_back( "video-es" );
        ai_objects.push_back( p_input->i_object_id );
        as_varnames.push_back( "spu-es" );
        ai_objects.push_back( p_input->i_object_id );
        vlc_object_t *p_vout = (vlc_object_t *)vlc_object_find( p_input,
                                                VLC_OBJECT_VOUT, FIND_CHILD );
        if( p_vout )
        {
            VideoAutoMenuBuilder( p_vout, ai_objects, as_varnames );
            vlc_object_release( p_vout );
        }
        vlc_object_release( p_input );
    }
    vlc_object_release( p_playlist );
    //CREATE_POPUP;
}

void QVLCMenu::AudioPopupMenu( intf_thread_t *p_intf, const QPoint &pos )
{
    POPUP_BOILERPLATE;
    if( p_input )
    {
        vlc_object_yield( p_input );
        as_varnames.push_back( "audio-es" );
        ai_objects.push_back( p_input->i_object_id );
        vlc_object_t *p_aout = (vlc_object_t *)vlc_object_find( p_input,
                                             VLC_OBJECT_AOUT, FIND_ANYWHERE );
        if( p_aout )
        {
            AudioAutoMenuBuilder( p_aout, ai_objects, as_varnames );
            vlc_object_release( p_aout );
        }
        vlc_object_release( p_input );
    }
    vlc_object_release( p_playlist );
    //CREATE_POPUP;
}

#if 0
/* Navigation stuff, and general */
static void MiscPopupMenu( intf_thread_t *p_intf, const QPoint &pos )
{
    POPUP_BOILERPLATE;
    if( p_input )
    {
        vlc_object_yield( p_input );
        as_varnames.push_back( "audio-es" );
        InputAutoMenuBuilder( VLC_OBJECT(p_input), ai_objects, as_varnames );
        PUSH_SEPARATOR;
    }
    IntfAutoMenuBuilder( p_intf, ai_objects, as_varnames, true );

    Menu popupmenu( p_intf, PopupMenu_Events );
    popupmenu.Populate( as_varnames, ai_objects );

    POPUP_STATIC_ENTRIES;
    popupmenu.Append( MenuDummy_Event, wxU(_("Open")),
                      OpenStreamMenu( p_intf ), wxT("") );

    p_intf->p_sys->p_popup_menu = &popupmenu;
    p_parent->PopupMenu( &popupmenu, pos.x, pos.y );
    p_intf->p_sys->p_popup_menu = NULL;
    vlc_object_release( p_playlist );
}

void PopupMenu( intf_thread_t *p_intf, wxWindow *p_parent,
                const wxPoint& pos )
{
    POPUP_BOILERPLATE;
    if( p_input )
    {
        vlc_object_yield( p_input );
        InputAutoMenuBuilder( VLC_OBJECT(p_input), ai_objects, as_varnames );

        /* Video menu */
        PUSH_SEPARATOR;
        as_varnames.push_back( "video-es" );
        ai_objects.push_back( p_input->i_object_id );
        as_varnames.push_back( "spu-es" );
        ai_objects.push_back( p_input->i_object_id );
        vlc_object_t *p_vout = (vlc_object_t *)vlc_object_find( p_input,
                                                VLC_OBJECT_VOUT, FIND_CHILD );
        if( p_vout )
        {
            VideoAutoMenuBuilder( p_vout, ai_objects, as_varnames );
            vlc_object_release( p_vout );
        }
        /* Audio menu */
        PUSH_SEPARATOR
        as_varnames.push_back( "audio-es" );
        ai_objects.push_back( p_input->i_object_id );
        vlc_object_t *p_aout = (vlc_object_t *)vlc_object_find( p_input,
                                             VLC_OBJECT_AOUT, FIND_ANYWHERE );
        if( p_aout )
        {
            AudioAutoMenuBuilder( p_aout, ai_objects, as_varnames );
            vlc_object_release( p_aout );
        }
    }

    /* Interface menu */
    PUSH_SEPARATOR
    IntfAutoMenuBuilder( p_intf, ai_objects, as_varnames, true );

    /* Build menu */
    Menu popupmenu( p_intf, PopupMenu_Events );
    popupmenu.Populate( as_varnames, ai_objects );
    POPUP_STATIC_ENTRIES;

        popupmenu.Append( MenuDummy_Event, wxU(_("Open")),
                          OpenStreamMenu( p_intf ), wxT("") );
    p_intf->p_sys->p_popup_menu = &popupmenu;
    p_parent->PopupMenu( &popupmenu, pos.x, pos.y );
    p_intf->p_sys->p_popup_menu = NULL;
    vlc_object_release( p_playlist );
}
#endif

/*****************************************************************************
 * Auto menus
 *****************************************************************************/
QMenu *QVLCMenu::AudioMenu( intf_thread_t *p_intf, QMenu * current )
{
    vector<int> ai_objects;
    vector<const char *> as_varnames;

    vlc_object_t *p_object = (vlc_object_t *)vlc_object_find( p_intf,
                                        VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( p_object != NULL )
    {
        PUSH_VAR( "audio-es" );
        vlc_object_release( p_object );
    }

    p_object = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );
    if( p_object )
    {
        AudioAutoMenuBuilder( p_object, ai_objects, as_varnames );
        vlc_object_release( p_object );
    }
    return Populate( p_intf, current, as_varnames, ai_objects );
}


QMenu *QVLCMenu::VideoMenu( intf_thread_t *p_intf, QMenu *current )
{
    vlc_object_t *p_object;
    vector<int> ai_objects;
    vector<const char *> as_varnames;

    p_object = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                FIND_ANYWHERE );
    if( p_object != NULL )
    {
        PUSH_VAR( "video-es" );
        PUSH_VAR( "spu-es" );
        vlc_object_release( p_object );
    }

    p_object = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                FIND_ANYWHERE );
    if( p_object != NULL )
    {
        VideoAutoMenuBuilder( p_object, ai_objects, as_varnames );
        vlc_object_release( p_object );
    }
    return Populate( p_intf, current, as_varnames, ai_objects );
}

QMenu *QVLCMenu::NavigMenu( intf_thread_t *p_intf, QMenu *current )
{
    vlc_object_t *p_object;
    vector<int> ai_objects;
    vector<const char *> as_varnames;

    p_object = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                FIND_ANYWHERE );
    if( p_object != NULL )
    {
        InputAutoMenuBuilder( p_object, ai_objects, as_varnames );
        PUSH_VAR( "prev-title"); PUSH_VAR ( "next-title" );
        PUSH_VAR( "prev-chapter"); PUSH_VAR( "next-chapter" );
        vlc_object_release( p_object );
    }
    return Populate( p_intf, current, as_varnames, ai_objects );
}
#if 0
wxMenu *SettingsMenu( intf_thread_t *_p_intf, wxWindow *p_parent,
                      wxMenu *p_menu )
{
    vlc_object_t *p_object;
    vector<int> ai_objects;
    vector<const char *> as_varnames;

    p_object = (vlc_object_t *)vlc_object_find( _p_intf, VLC_OBJECT_INTF,
                                                FIND_PARENT );
    if( p_object != NULL )
    {
        PUSH_VAR( "intf-switch" );
        PUSH_VAR( "intf-add" );
        vlc_object_release( p_object );
    }

    /* Build menu */
    Menu *p_vlc_menu = (Menu *)p_menu;
    if( !p_vlc_menu )
        p_vlc_menu = new Menu( _p_intf, SettingsMenu_Events );
    else
        p_vlc_menu->Clear();

    p_vlc_menu->Populate( as_varnames, ai_objects );

    return p_vlc_menu;
}
#endif

QMenu * QVLCMenu::Populate( intf_thread_t *p_intf, QMenu *current,
                            vector< const char *> & ras_varnames,
                            vector<int> & rai_objects )
{
    QMenu *menu = current;
    if( !menu )
        menu = new QMenu();
    else
        menu->clear();

    currentGroup = NULL;

    vlc_object_t *p_object;
    vlc_bool_t b_section_empty = VLC_FALSE;
    int i;

#define APPEND_EMPTY { QAction *action = menu->addAction( _("Empty" ) ); \
                       action->setEnabled( false ); }

    for( i = 0; i < (int)rai_objects.size() ; i++ )
    {
        if( !ras_varnames[i] || !*ras_varnames[i] )
        {
            if( b_section_empty )
                APPEND_EMPTY;
            menu->addSeparator();
            b_section_empty = VLC_TRUE;
            continue;
        }

        if( rai_objects[i] == 0  )
        {
            /// \bug What is this ?
            // Append( menu, ras_varnames[i], NULL );
            b_section_empty = VLC_FALSE;
            continue;
        }

        p_object = (vlc_object_t *)vlc_object_get( p_intf,
                                                   rai_objects[i] );
        if( p_object == NULL ) continue;

        b_section_empty = VLC_FALSE;
        CreateItem( menu, ras_varnames[i], p_object );
        vlc_object_release( p_object );
    }

    /* Special case for empty menus */
    if( menu->actions().size() == 0 || b_section_empty )
        APPEND_EMPTY

    return menu;
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/

#define FREE(x) if(x) { free(x);x=NULL;}

static bool IsMenuEmpty( const char *psz_var, vlc_object_t *p_object,
                         bool b_root = TRUE )
{
    vlc_value_t val, val_list;
    int i_type, i_result, i;

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_var );

    /* Check if we want to display the variable */
    if( !(i_type & VLC_VAR_HASCHOICE) ) return FALSE;

    var_Change( p_object, psz_var, VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int == 0 ) return TRUE;

    if( (i_type & VLC_VAR_TYPE) != VLC_VAR_VARIABLE )
    {
        /* Very evil hack ! intf-switch can have only one value */ 
        if( !strcmp( psz_var, "intf-switch" ) ) return FALSE;
        if( val.i_int == 1 && b_root ) return TRUE;
        else return FALSE;
    }

    /* Check children variables in case of VLC_VAR_VARIABLE */
    if( var_Change( p_object, psz_var, VLC_VAR_GETLIST, &val_list, NULL ) < 0 )
    {
        return TRUE;
    }

    for( i = 0, i_result = TRUE; i < val_list.p_list->i_count; i++ )
    {
        if( !IsMenuEmpty( val_list.p_list->p_values[i].psz_string,
                          p_object, FALSE ) )
        {
            i_result = FALSE;
            break;
        }
    }

    /* clean up everything */
    var_Change( p_object, psz_var, VLC_VAR_FREELIST, &val_list, NULL );

    return i_result;
}

void QVLCMenu::CreateItem( QMenu *menu, const char *psz_var,
                           vlc_object_t *p_object )
{
    QAction *action;
    vlc_value_t val, text;
    int i_type;

    /* Check the type of the object variable */
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
    if( IsMenuEmpty( psz_var, p_object ) )  return;

    /* Get the descriptive name of the variable */
    var_Change( p_object, psz_var, VLC_VAR_GETTEXT, &text, NULL );

    if( i_type & VLC_VAR_HASCHOICE )
    {
        /* Append choices menu */
        QMenu *submenu = CreateChoicesMenu( psz_var, p_object, true );
        submenu->setTitle( text.psz_string ? text.psz_string : psz_var );
        menu->addMenu( submenu );
        FREE( text.psz_string );
        return;
    }

#define TEXT_OR_VAR text.psz_string ? text.psz_string : psz_var

    switch( i_type & VLC_VAR_TYPE )
    {
    case VLC_VAR_VOID:
        var_Get( p_object, psz_var, &val );
        CreateAndConnect( menu, psz_var, TEXT_OR_VAR, "", ITEM_NORMAL,
                          p_object->i_object_id, val, i_type );
        break;

    case VLC_VAR_BOOL:
        var_Get( p_object, psz_var, &val );
        val.b_bool = !val.b_bool;
        CreateAndConnect( menu, psz_var, TEXT_OR_VAR, "", ITEM_CHECK,
                          p_object->i_object_id, val, i_type, val.b_bool );
        break;
    }
    FREE( text.psz_string );
}


QMenu *QVLCMenu::CreateChoicesMenu( const char *psz_var, 
                                    vlc_object_t *p_object, bool b_root )
{
    vlc_value_t val, val_list, text_list;
    int i_type, i;

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_var );

    /* Make sure we want to display the variable */
    if( IsMenuEmpty( psz_var, p_object, b_root ) ) return NULL;

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
        return NULL;
    }

    if( var_Change( p_object, psz_var, VLC_VAR_GETLIST,
                    &val_list, &text_list ) < 0 )
    {
        return NULL;
    }
#define NORMAL_OR_RADIO i_type & VLC_VAR_ISCOMMAND ? ITEM_NORMAL: ITEM_RADIO
#define NOTCOMMAND !(i_type & VLC_VAR_ISCOMMAND) 
#define CURVAL val_list.p_list->p_values[i]
#define CURTEXT text_list.p_list->p_values[i].psz_string

    QMenu *submenu = new QMenu();
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t another_val;
        QString menutext;
        QMenu *subsubmenu;

        switch( i_type & VLC_VAR_TYPE )
        {
        case VLC_VAR_VARIABLE:
            subsubmenu = CreateChoicesMenu( CURVAL.psz_string,
                                                   p_object, false );
            subsubmenu->setTitle( CURTEXT ? CURTEXT : CURVAL.psz_string );
            submenu->addMenu( subsubmenu );
            break;

        case VLC_VAR_STRING:
          var_Get( p_object, psz_var, &val );
          another_val.psz_string = strdup( CURVAL.psz_string );

          menutext = CURTEXT ? CURTEXT : another_val.psz_string;
          CreateAndConnect( submenu, psz_var, menutext, "", NORMAL_OR_RADIO,
                            p_object->i_object_id, another_val, i_type, 
                            NOTCOMMAND && val.psz_string &&
                            !strcmp( val.psz_string, CURVAL.psz_string ) );

          if( val.psz_string ) free( val.psz_string );
          break;

        case VLC_VAR_INTEGER:
          var_Get( p_object, psz_var, &val );
          if( CURTEXT ) menutext = CURTEXT; 
          else menutext.sprintf( "%d", CURVAL.i_int);
          CreateAndConnect( submenu, psz_var, menutext, "", NORMAL_OR_RADIO,
                            p_object->i_object_id, CURVAL, i_type,
                            NOTCOMMAND && CURVAL.i_int == val.i_int );
          break;

        case VLC_VAR_FLOAT:
          var_Get( p_object, psz_var, &val );
          if( CURTEXT ) menutext = CURTEXT ;
          else menutext.sprintf( "%.2f", CURVAL.f_float );
          CreateAndConnect( submenu, psz_var, menutext, "", NORMAL_OR_RADIO,
                            p_object->i_object_id, CURVAL, i_type,
                            NOTCOMMAND && CURVAL.f_float == val.f_float );
          break;

        default:
          break;
        }
    }

    /* clean up everything */
    var_Change( p_object, psz_var, VLC_VAR_FREELIST, &val_list, &text_list );

#undef NORMAL_OR_RADIO
#undef NOTCOMMAND
#undef CURVAL
#undef CURTEXT
    return submenu;
}

void QVLCMenu::CreateAndConnect( QMenu *menu, const char *psz_var,
                                 QString text, QString help,
                                 int i_item_type, int i_object_id, 
                                 vlc_value_t val, int i_val_type,
                                 bool checked )
{
    QAction *action = new QAction( text, menu );
    action->setText( text );
    action->setToolTip( help );

    if( i_item_type == ITEM_CHECK )
    {
        action->setCheckable( true );
        currentGroup = NULL;
    }
    else if( i_item_type == ITEM_RADIO )
    {
        action->setCheckable( true );
        if( !currentGroup )
            currentGroup = new QActionGroup(menu);
        currentGroup->addAction( action );
    }
    else
        currentGroup = NULL;
    if( checked ) action->setChecked( true );

    MenuItemData *itemData = new MenuItemData( i_object_id, i_val_type,
                                               val, psz_var );
    connect( action, SIGNAL(triggered()), THEDP->menusMapper, SLOT(map()) ); 
    THEDP->menusMapper->setMapping( action, itemData );
    
    menu->addAction( action );
}

void QVLCMenu::DoAction( intf_thread_t *p_intf, QObject *data )
{
    MenuItemData *itemData = qobject_cast<MenuItemData *>(data);
    
    vlc_object_t *p_object = (vlc_object_t *)vlc_object_get( p_intf,
                                           itemData->i_object_id );
    if( p_object == NULL ) return;

    var_Set( p_object, itemData->psz_var, itemData->val );
    vlc_object_release( p_object );
}


#if 0
void MenuEvtHandler::OnMenuEvent( wxCommandEvent& event )
{
    wxMenuItem *p_menuitem = NULL;
    int i_hotkey_event = p_intf->p_sys->i_first_hotkey_event;
    int i_hotkeys = p_intf->p_sys->i_hotkeys;

    if( event.GetId() >= Play_Event && event.GetId() <= Stop_Event )
    {
        input_thread_t *p_input;
        playlist_t * p_playlist =
            (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
        if( !p_playlist ) return;

        switch( event.GetId() )
        {
        case Play_Event:
        case Pause_Event:
            p_input =
                (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                   FIND_ANYWHERE );
            if( !p_input ) playlist_Play( p_playlist );
            else
            {
                vlc_value_t val;
                var_Get( p_input, "state", &val );
                if( val.i_int != PAUSE_S ) val.i_int = PAUSE_S;
                else val.i_int = PLAYING_S;
                var_Set( p_input, "state", val );
                vlc_object_release( p_input );
            }
            break;
        case Stop_Event:
            playlist_Stop( p_playlist );
            break;
        case Previous_Event:
            playlist_Prev( p_playlist );
            break;
        case Next_Event:
            playlist_Next( p_playlist );
            break;
        }

        vlc_object_release( p_playlist );
        return;
    }

    /* Check if this is an auto generated menu item */
    if( event.GetId() < FirstAutoGenerated_Event )
    {
        event.Skip();
        return;
    }

    /* Check if this is an hotkey event */
    if( event.GetId() >= i_hotkey_event &&
        event.GetId() < i_hotkey_event + i_hotkeys )
    {
        vlc_value_t val;

        val.i_int =
            p_intf->p_vlc->p_hotkeys[event.GetId() - i_hotkey_event].i_key;

        /* Get the key combination and send it to the hotkey handler */
        var_Set( p_intf->p_vlc, "key-pressed", val );
        return;
    }

    if( !p_main_interface ||
        (p_menuitem = p_main_interface->GetMenuBar()->FindItem(event.GetId()))
        == NULL )
    {
        if( p_intf->p_sys->p_popup_menu )
        {
            p_menuitem =
                p_intf->p_sys->p_popup_menu->FindItem( event.GetId() );
        }
    }

    if( p_menuitem )
    {
        wxMenuItemExt *p_menuitemext = (wxMenuItemExt *)p_menuitem;
        vlc_object_t *p_object;

        p_object = (vlc_object_t *)vlc_object_get( p_intf,
                                       p_menuitemext->i_object_id );
        if( p_object == NULL ) return;

        wxMutexGuiLeave(); // We don't want deadlocks
        var_Set( p_object, p_menuitemext->psz_var, p_menuitemext->val );
        //wxMutexGuiEnter();

        vlc_object_release( p_object );
    }
    else
        event.Skip();
}
#endif
