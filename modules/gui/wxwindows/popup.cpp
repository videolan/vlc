/*****************************************************************************
 * popup.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: popup.cpp,v 1.3 2003/01/26 10:36:10 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

#include <wx/wxprec.h>
#include <wx/wx.h>
#include <wx/listctrl.h>

#include <vlc/intf.h>

#include "wxwindows.h"

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    /* menu items */
    Close_Event = wxID_HIGHEST + 1000,
    MenuDummy_Event,
    MenuLast_Event,
};

BEGIN_EVENT_TABLE(PopupMenu, wxMenu)
    /* Menu events */
    EVT_MENU(Close_Event, PopupMenu::OnClose)
    EVT_MENU(MenuDummy_Event, PopupMenu::OnEntrySelected)

END_EVENT_TABLE()

BEGIN_EVENT_TABLE(PopupEvtHandler, wxEvtHandler)
    EVT_MENU(-1, PopupEvtHandler::OnMenuEvent)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
PopupMenu::PopupMenu( intf_thread_t *_p_intf, Interface *_p_main_interface ):
    wxMenu( )
{
    vlc_object_t *p_object;

    /* Initializations */
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;
    i_item_id = MenuLast_Event;

    /* Audio menu */
    Append( MenuDummy_Event, _("Audio menu") );
    AppendSeparator();
    p_object = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );
    if( p_object == NULL ) return;

    CreateMenuEntry( "audio-device", p_object );
    CreateMenuEntry( "audio-channels", p_object );

    vlc_object_release( p_object );

    /* Video menu */
    AppendSeparator();
    Append( MenuDummy_Event, _("Video menu") );
    AppendSeparator();
    p_object = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                FIND_ANYWHERE );
    if( p_object == NULL ) return;

    CreateMenuEntry( "fullscreen", p_object );

    vlc_object_release( p_object );

    /* Input menu */
    AppendSeparator();
    Append( MenuDummy_Event, _("Input menu") );
    AppendSeparator();
    p_object = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                FIND_ANYWHERE );
    if( p_object == NULL ) return;

    CreateMenuEntry( "title", p_object );
    CreateMenuEntry( "chapter", p_object );

    vlc_object_release( p_object );

    /* Misc stuff */
    AppendSeparator();
    Append( Close_Event, _("&Close") );

    /* Intercept all menu events in our custom event handler */
    p_main_interface->p_popup_menu = this;
    p_main_interface->PushEventHandler(
        new PopupEvtHandler( p_intf, p_main_interface ) );

    wxPoint mousepos = wxGetMousePosition();
    p_main_interface->PopupMenu( this,
                                 p_main_interface->ScreenToClient(mousepos).x,
                                 p_main_interface->ScreenToClient(mousepos).y
                                 );
}

PopupMenu::~PopupMenu()
{
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void PopupMenu::OnClose( wxCommandEvent& WXUNUSED(event) )
{
    p_intf->b_die = VLC_TRUE;
}

void PopupMenu::OnEntrySelected( wxCommandEvent& WXUNUSED(event) )
{
}

void PopupMenu::CreateMenuEntry( char *psz_var, vlc_object_t *p_object )
{
    vlc_value_t val, val1;
    int i_type;

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_var );

    if( i_type & VLC_VAR_HASCHOICE )
    {
        Append( MenuDummy_Event, psz_var,
                CreateSubMenu( psz_var, p_object ),
                "YEAAAARRRGGGHHH HEEELLPPPPPP" );
        return;
    }

    if( var_Get( p_object, psz_var, &val ) < 0 )
    {
        return;
    }

    wxMenuItemExt *menuitem;

    switch( i_type )
    {
    case VLC_VAR_VOID:
        menuitem = new wxMenuItemExt( this, i_item_id++, psz_var,
                                      "", wxITEM_NORMAL, strdup(psz_var),
                                      p_object->i_object_id, val );
        Append( menuitem );
        break;

    case VLC_VAR_BOOL:
        val1.b_bool = !val.b_bool;
        menuitem = new wxMenuItemExt( this, i_item_id++, psz_var,
                                      "", wxITEM_CHECK, strdup(psz_var),
                                      p_object->i_object_id, val1 );
        Append( menuitem );
        Check( i_item_id - 1, val.b_bool ? TRUE : FALSE );
        break;

    case VLC_VAR_STRING:
        break;

    default:
        break;
    }

}

wxMenu *PopupMenu::CreateSubMenu( char *psz_var, vlc_object_t *p_object )
{
    wxMenu *menu = new wxMenu;
    vlc_value_t val;
    char *psz_value;
    int i_type, i;

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_var );

    switch( i_type )
    {
    case VLC_VAR_VOID:
    case VLC_VAR_STRING:
        break;

    default:
        break;
    }

    if( var_Get( p_object, psz_var, &val ) < 0 )
    {
        return NULL;
    }
    psz_value = val.psz_string;

    if( var_Change( p_object, psz_var, VLC_VAR_GETLIST, &val ) < 0 )
    {
        return NULL;
    }

    for( i = 0; i < val.p_list->i_count; i++ )
    {
        vlc_value_t another_val;
        wxMenuItemExt *menuitem;

        switch( i_type & VLC_VAR_TYPE )
        {
        case VLC_VAR_STRING:
          another_val.psz_string =
              strdup(val.p_list->p_values[i].psz_string);
          menuitem =
              new wxMenuItemExt( this, i_item_id++, another_val.psz_string,
                                 "", wxITEM_RADIO, strdup(psz_var),
                                 p_object->i_object_id,
                                 another_val );

          menu->Append( menuitem );

          if( !strcmp( psz_value, val.p_list->p_values[i].psz_string ) )
              menu->Check( i_item_id - 1, TRUE );
          break;

        case VLC_VAR_INTEGER:
          menuitem =
              new wxMenuItemExt( this, i_item_id++,
                                 wxString::Format(_("%d"),
                                 val.p_list->p_values[i].i_int),
                                 "", wxITEM_RADIO, strdup(psz_var),
                                 p_object->i_object_id,
                                 val.p_list->p_values[i] );

          menu->Append( menuitem );
          break;

        default:
          break;
        }
    }

    var_Change( p_object, psz_var, VLC_VAR_FREELIST, &val );

    return menu;
}

/*****************************************************************************
 * A small helper class which intercepts all popup menu events
 *****************************************************************************/
PopupEvtHandler::PopupEvtHandler( intf_thread_t *_p_intf,
                                  Interface *_p_main_interface )
{
    /* Initializations */
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;
}

PopupEvtHandler::~PopupEvtHandler()
{
}

void PopupEvtHandler::OnMenuEvent( wxCommandEvent& event )
{
    wxMenuItemExt *p_menuitem = (wxMenuItemExt *)
        p_main_interface->p_popup_menu->FindItem( event.GetId() );

    if( p_menuitem )
    {
        vlc_object_t *p_object;

        p_object = (vlc_object_t *)vlc_object_get( p_intf,
                                                   p_menuitem->i_object_id );
        if( p_object == NULL ) return;

        var_Set( p_object, p_menuitem->psz_var, p_menuitem->val );

        vlc_object_release( p_object );
    }
    else
        event.Skip();
}
