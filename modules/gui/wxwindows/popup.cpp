/*****************************************************************************
 * popup.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: popup.cpp,v 1.1 2002/12/13 01:50:32 gbazin Exp $
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
    Close_Event = 1,
    MenuEntry_Event,
    MenuDummy_Event
};

BEGIN_EVENT_TABLE(PopupMenu, wxMenu)
    /* Menu events */
    EVT_MENU(Close_Event, PopupMenu::OnClose)
    EVT_MENU(MenuEntry_Event, PopupMenu::OnEntrySelected)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
PopupMenu::PopupMenu( intf_thread_t *_p_intf, Interface *_p_main_interface ):
    wxMenu( "VideoLan" )
{
    /* Initializations */
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;

    Append(MenuEntry_Event, "Dummy1");
    Append(MenuEntry_Event, "Dummy2");
    Append(MenuDummy_Event, "&Dummy sub menu",
           CreateDummyMenu(), "Dummy sub menu help");
    Append(MenuEntry_Event, "Dummy3", "", TRUE);
    AppendSeparator();
    Append( Close_Event, _("&Close") );

    wxPoint mousepos = wxGetMousePosition();
    p_main_interface->PopupMenu( this, mousepos.x, mousepos.y );
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

wxMenu *PopupMenu::CreateDummyMenu()
{
    wxMenu *menu = new wxMenu;
    menu->Append(MenuEntry_Event, "Sub Dummy1");
    menu->AppendSeparator();
    menu->Append(MenuEntry_Event, "Sub Dummy2", "", TRUE);

    return menu;
}
