/*****************************************************************************
 * playlist.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: playlist.cpp,v 1.4 2002/11/23 18:42:59 gbazin Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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
    AddUrl_Event = 1,
    AddDirectory_Event,
    Close_Event,

    InvertSelection_Event,
    DeleteSelection_Event,
    SelectAll_Event,

    /* controls */
    ListView_Event
};

BEGIN_EVENT_TABLE(Playlist, wxFrame)
    /* Menu events */
    EVT_MENU(AddUrl_Event, Playlist::OnAddUrl)
    EVT_MENU(AddDirectory_Event, Playlist::OnAddDirectory)
    EVT_MENU(Close_Event, Playlist::OnClose)
    EVT_MENU(InvertSelection_Event, Playlist::OnInvertSelection)
    EVT_MENU(DeleteSelection_Event, Playlist::OnDeleteSelection)
    EVT_MENU(SelectAll_Event, Playlist::OnSelectAll)

    /* Listview events */
    EVT_LIST_ITEM_ACTIVATED(ListView_Event, Playlist::OnActivateItem)
    EVT_LIST_KEY_DOWN(ListView_Event, Playlist::OnKeyDown)

    /* Button events */
    EVT_BUTTON( wxID_OK, Playlist::OnClose)

    /* Special events : we don't want to destroy the window when the user
     * clicks on (X) */
    EVT_CLOSE(Playlist::OnClose)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Playlist::Playlist( intf_thread_t *_p_intf, Interface *_p_main_interface ):
    wxFrame( _p_main_interface, -1, "Playlist", wxDefaultPosition,
             wxSize::wxSize( 400, 500 ), wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;

    /* Create our "Manage" menu */
    wxMenu *manage_menu = new wxMenu;
    manage_menu->Append( AddUrl_Event, _("Add &Url...") );
    manage_menu->Append( AddDirectory_Event, _("Add &Directory...") );
    manage_menu->AppendSeparator();
    manage_menu->Append( Close_Event, _("&Close") );

    /* Create our "Selection" menu */
    wxMenu *selection_menu = new wxMenu;
    selection_menu->Append( InvertSelection_Event, _("&Invert") );
    selection_menu->Append( DeleteSelection_Event, _("&Delete") );
    selection_menu->Append( SelectAll_Event, _("&Select All") );

    /* Append the freshly created menus to the menu bar */
    wxMenuBar *menubar = new wxMenuBar( wxMB_DOCKABLE );
    menubar->Append( manage_menu, _("&Manage") );
    menubar->Append( selection_menu, _("&Selection") );

    /* Attach the menu bar to the frame */
    SetMenuBar( menubar );

    /* Create a panel to put everything in */
    wxPanel *playlist_panel = new wxPanel( this, -1 );
    playlist_panel->SetAutoLayout( TRUE );

    /* Create the listview */
    /* FIXME: the given size is arbitrary, and prevents us from resizing
     * the window to smaller dimensions. But the sizers don't seem to adjust
     * themselves to the size of a listview, and with a wxDefaultSize the
     * playlist window is ridiculously small */
    listview = new wxListView( playlist_panel, ListView_Event,
                               wxDefaultPosition,
                               wxSize( 350, 300 ), wxLC_REPORT );
    listview->InsertColumn( 0, _("Url") );
    listview->InsertColumn( 1, _("Duration") );
    listview->SetColumnWidth( 0, 250 );
    listview->SetColumnWidth( 1, 100 );

    /* Create the OK button */
    ok_button = new wxButton( playlist_panel, wxID_OK, _("OK") );
    ok_button->SetDefault();

    /* Place everything in sizers */
    wxBoxSizer *ok_button_sizer = new wxBoxSizer( wxHORIZONTAL );
    ok_button_sizer->Add( ok_button, 0, wxALL, 5 );
    ok_button_sizer->Layout();
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( listview, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( ok_button_sizer, 0, wxALIGN_CENTRE );
    panel_sizer->Layout();
    playlist_panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( playlist_panel, 1, wxGROW, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );

    /* Associate drop targets with the playlist */
    SetDropTarget( new DragAndDrop( p_intf ) );

    /* Update the playlist */
    Rebuild();

}

Playlist::~Playlist()
{
}

void Playlist::Rebuild()
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* Clear the list... */
    listview->DeleteAllItems();

    /* ...and rebuild it */
    vlc_mutex_lock( &p_playlist->object_lock );
    for( int i = 0; i < p_playlist->i_size; i++ )
    {
        wxString filename = p_playlist->pp_items[i]->psz_name;
        listview->InsertItem( i, filename );
        /* FIXME: we should try to find the actual duration... */
        listview->SetItem( i, 1, _("no info") );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    /* Change the colour for the currenty played stream */
    wxListItem listitem;
    listitem.m_itemId = p_playlist->i_index;
    listitem.SetTextColour( *wxRED );
    listview->SetItem( listitem );

    vlc_object_release( p_playlist );
}

/* Update the colour of items */
void Playlist::Manage()
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_intf->p_sys->i_playing != p_playlist->i_index )
    {
        wxListItem listitem;
        listitem.m_itemId = p_playlist->i_index;
        listitem.SetTextColour( *wxRED );
        listview->SetItem( listitem );

        if( p_intf->p_sys->i_playing != -1 )
        {
            listitem.m_itemId = p_intf->p_sys->i_playing;
            listitem.SetTextColour( *wxBLACK );
            listview->SetItem( listitem );
        }
        p_intf->p_sys->i_playing = p_playlist->i_index;
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    vlc_object_release( p_playlist );
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void Playlist::DeleteItem( int item )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Delete( p_playlist, item );
    listview->DeleteItem( item );

    vlc_object_release( p_playlist );
}

void Playlist::OnClose( wxCommandEvent& WXUNUSED(event) )
{
    Hide();
}

void Playlist::OnAddUrl( wxCommandEvent& WXUNUSED(event) )
{
    /* TODO */
}

void Playlist::OnAddDirectory( wxCommandEvent& WXUNUSED(event) )
{
    /* TODO */
}

void Playlist::OnInvertSelection( wxCommandEvent& WXUNUSED(event) )
{
    for( long item = 0; item < listview->GetItemCount(); item++ )
    {
        listview->Select( item, ! listview->IsSelected( item ) );
    }
}

void Playlist::OnDeleteSelection( wxCommandEvent& WXUNUSED(event) )
{
    /* Delete from the end to the beginning, to avoid a shift of indices */
    for( long item = listview->GetItemCount() - 1; item >= 0; item-- )
    {
        if( listview->IsSelected( item ) )
        {
            DeleteItem( item );
        }
    }

    Rebuild();
}

void Playlist::OnSelectAll( wxCommandEvent& WXUNUSED(event) )
{
    for( long item = 0; item < listview->GetItemCount(); item++ )
    {
        listview->Select( item, TRUE );
    }
}

void Playlist::OnActivateItem( wxListEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Goto( p_playlist, event.GetIndex() );

    vlc_object_release( p_playlist );
}

void Playlist::OnKeyDown( wxListEvent& event )
{
    long keycode = event.GetKeyCode();
    /* Delete selected items */
    if( keycode == WXK_BACK || keycode == WXK_DELETE )
    {
        /* We send a dummy event */
        OnDeleteSelection( event );
    }
}

