/*****************************************************************************
 * playlist.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: playlist.cpp,v 1.13 2003/07/17 17:30:40 gbazin Exp $
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

/* Callback prototype */
int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param );

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    /* menu items */
    AddFile_Event = 1,
    AddMRL_Event,
    Close_Event,
    Open_Event,
    Save_Event,

    InvertSelection_Event,
    DeleteSelection_Event,
    SelectAll_Event,

    /* controls */
    ListView_Event
};

BEGIN_EVENT_TABLE(Playlist, wxFrame)
    /* Menu events */
    EVT_MENU(AddFile_Event, Playlist::OnAddFile)
    EVT_MENU(AddMRL_Event, Playlist::OnAddMRL)
    EVT_MENU(Close_Event, Playlist::OnClose)
    EVT_MENU(Open_Event, Playlist::OnOpen)
    EVT_MENU(Save_Event, Playlist::OnSave)
    EVT_MENU(InvertSelection_Event, Playlist::OnInvertSelection)
    EVT_MENU(DeleteSelection_Event, Playlist::OnDeleteSelection)
    EVT_MENU(SelectAll_Event, Playlist::OnSelectAll)

    /* Listview events */
    EVT_LIST_ITEM_ACTIVATED(ListView_Event, Playlist::OnActivateItem)
    EVT_LIST_KEY_DOWN(ListView_Event, Playlist::OnKeyDown)

    /* Button events */
    EVT_BUTTON( Close_Event, Playlist::OnClose)
    EVT_BUTTON( Save_Event, Playlist::OnSave)

    /* Special events : we don't want to destroy the window when the user
     * clicks on (X) */
    EVT_CLOSE(Playlist::OnClose)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Playlist::Playlist( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxFrame( p_parent, -1, wxU(_("Playlist")), wxDefaultPosition,
             wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    i_update_counter = 0;
    b_need_update = VLC_FALSE;
    vlc_mutex_init( p_intf, &lock );
    SetIcon( *p_intf->p_sys->p_icon );

    /* Create our "Manage" menu */
    wxMenu *manage_menu = new wxMenu;
    manage_menu->Append( AddFile_Event, wxU(_("&Simple Add...")) );
    manage_menu->Append( AddMRL_Event, wxU(_("&Add MRL...")) );
    manage_menu->Append( Open_Event, wxU(_("&Open Playlist...")) );
    manage_menu->Append( Save_Event, wxU(_("&Save Playlist...")) );
    manage_menu->AppendSeparator();
    manage_menu->Append( Close_Event, wxU(_("&Close")) );

    /* Create our "Selection" menu */
    wxMenu *selection_menu = new wxMenu;
    selection_menu->Append( InvertSelection_Event, wxU(_("&Invert")) );
    selection_menu->Append( DeleteSelection_Event, wxU(_("&Delete")) );
    selection_menu->Append( SelectAll_Event, wxU(_("&Select All")) );

    /* Append the freshly created menus to the menu bar */
    wxMenuBar *menubar = new wxMenuBar( wxMB_DOCKABLE );
    menubar->Append( manage_menu, wxU(_("&Manage")) );
    menubar->Append( selection_menu, wxU(_("&Selection")) );

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
                               wxDefaultPosition, wxSize( 355, 300 ),
                               wxLC_REPORT | wxSUNKEN_BORDER );
    listview->InsertColumn( 0, wxU(_("Url")) );
    listview->InsertColumn( 1, wxU(_("Duration")) );
    listview->SetColumnWidth( 0, 250 );
    listview->SetColumnWidth( 1, 100 );

    /* Create the Close button */
    wxButton *close_button = new wxButton( playlist_panel, Close_Event,
                                           wxU(_("Close")) );
    close_button->SetDefault();

    /* Place everything in sizers */
    wxBoxSizer *close_button_sizer = new wxBoxSizer( wxHORIZONTAL );
    close_button_sizer->Add( close_button, 0, wxALL, 5 );
    close_button_sizer->Layout();
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( listview, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( close_button_sizer, 0, wxALIGN_CENTRE );
    panel_sizer->Layout();
    playlist_panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( playlist_panel, 1, wxGROW, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );

#if !defined(__WXX11__)
    /* Associate drop targets with the playlist */
    SetDropTarget( new DragAndDrop( p_intf ) );
#endif

    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* We want to be noticed of playlit changes */
    var_AddCallback( p_playlist, "intf-change", PlaylistChanged, this );
    vlc_object_release( p_playlist );

    /* Update the playlist */
    Rebuild();
}

Playlist::~Playlist()
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    var_DelCallback( p_playlist, "intf-change", PlaylistChanged, this );
    vlc_object_release( p_playlist );
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
        wxString filename = wxU(p_playlist->pp_items[i]->psz_name);
        listview->InsertItem( i, filename );
        /* FIXME: we should try to find the actual duration... */
        listview->SetItem( i, 1, wxU(_("no info")) );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    /* Change the colour for the currenty played stream */
    wxListItem listitem;
    listitem.m_itemId = p_playlist->i_index;
    listitem.SetTextColour( *wxRED );
    listview->SetItem( listitem );

    vlc_object_release( p_playlist );
}

void Playlist::ShowPlaylist( bool show )
{
    if( show ) Rebuild();
    Show( show );
}

void Playlist::UpdatePlaylist()
{
    vlc_bool_t b_need_update = VLC_FALSE;
    i_update_counter++;

    /* If the playlist isn't show there's no need to update it */
    if( !IsShown() ) return;

    vlc_mutex_lock( &lock );
    if( this->b_need_update )
    {
        b_need_update = VLC_TRUE;
        this->b_need_update = VLC_FALSE;
    }
    vlc_mutex_unlock( &lock );

    if( b_need_update ) Rebuild();

    /* Updating the playing status every 0.5s is enough */
    if( i_update_counter % 5 ) return;

    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* Update the colour of items */
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

void Playlist::OnSave( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    wxFileDialog dialog( this, wxU(_("Save playlist")),
                         wxT(""), wxT(""), wxT("*"), wxSAVE );

    if( dialog.ShowModal() == wxID_OK )
    {
        playlist_SaveFile( p_playlist, dialog.GetPath().mb_str() );
    }

    vlc_object_release( p_playlist );
}

void Playlist::OnOpen( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    wxFileDialog dialog( this, wxU(_("Open playlist")),
                         wxT(""), wxT(""), wxT("*"), wxOPEN );

    if( dialog.ShowModal() == wxID_OK )
    {
        playlist_LoadFile( p_playlist, dialog.GetPath().mb_str() );
    }

    vlc_object_release( p_playlist );
}

void Playlist::OnAddFile( wxCommandEvent& WXUNUSED(event) )
{
    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE_SIMPLE, 0 );

#if 0
    Rebuild();
#endif
}

void Playlist::OnAddMRL( wxCommandEvent& WXUNUSED(event) )
{
    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE, 0 );

#if 0
    Rebuild();
#endif
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

/*****************************************************************************
 * PlaylistChanged: callback triggered by the intf-change playlist variable
 *  We don't rebuild the playlist directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    Playlist *p_playlist_dialog = (Playlist *)param;
    vlc_mutex_lock( &p_playlist_dialog->lock );
    p_playlist_dialog->b_need_update = VLC_TRUE;
    vlc_mutex_unlock( &p_playlist_dialog->lock );
    return VLC_SUCCESS;
}
