/*****************************************************************************
 * playlist.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: playlist.cpp,v 1.22 2003/10/06 16:23:30 zorglub Exp $
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
#include <vlc/intf.h>

#include "wxwindows.h"
#include <wx/listctrl.h>

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
    Sort_Event,
    RSort_Event,
    Close_Event,
    Open_Event,
    Save_Event,

    EnableSelection_Event,
    DisableSelection_Event,

    InvertSelection_Event,
    DeleteSelection_Event,
    Random_Event,
    Loop_Event,
    Repeat_Event,
    SelectAll_Event,

    En_Dis_Event,

    Infos_Event,

    SearchText_Event,
    Search_Event,

    /* controls */
    ListView_Event
};

BEGIN_EVENT_TABLE(Playlist, wxFrame)
    /* Menu events */
    EVT_MENU(AddFile_Event, Playlist::OnAddFile)
    EVT_MENU(AddMRL_Event, Playlist::OnAddMRL)
    EVT_MENU(Sort_Event, Playlist::OnSort)
    EVT_MENU(RSort_Event, Playlist::OnRSort)
    EVT_MENU(Close_Event, Playlist::OnClose)
    EVT_MENU(Open_Event, Playlist::OnOpen)
    EVT_MENU(Save_Event, Playlist::OnSave)
    EVT_MENU(EnableSelection_Event, Playlist::OnEnableSelection)
    EVT_MENU(DisableSelection_Event, Playlist::OnDisableSelection)
    EVT_MENU(InvertSelection_Event, Playlist::OnInvertSelection)
    EVT_MENU(DeleteSelection_Event, Playlist::OnDeleteSelection)
    EVT_MENU(SelectAll_Event, Playlist::OnSelectAll)
    EVT_MENU(Infos_Event, Playlist::OnInfos)
    EVT_CHECKBOX(Random_Event, Playlist::OnRandom)
    EVT_CHECKBOX(Repeat_Event, Playlist::OnRepeat)
    EVT_CHECKBOX(Loop_Event, Playlist::OnLoop)

    /* Listview events */
    EVT_LIST_ITEM_ACTIVATED(ListView_Event, Playlist::OnActivateItem)
    EVT_LIST_KEY_DOWN(ListView_Event, Playlist::OnKeyDown)

    /* Button events */
    EVT_BUTTON( Search_Event, Playlist::OnSearch)
    EVT_BUTTON( En_Dis_Event, Playlist::OnEnDis)
    EVT_BUTTON( Save_Event, Playlist::OnSave)
    EVT_BUTTON( Infos_Event, Playlist::OnInfos)

    EVT_TEXT(SearchText_Event, Playlist::OnSearchTextChange)

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
    iteminfo_dialog = NULL;
    p_intf = _p_intf;
    vlc_value_t  val;
    i_update_counter = 0;
    b_need_update = VLC_FALSE;
    vlc_mutex_init( p_intf, &lock );
    SetIcon( *p_intf->p_sys->p_icon );

    var_Create( p_intf, "random", VLC_VAR_BOOL );
    var_Change( p_intf, "random", VLC_VAR_INHERITVALUE, & val, NULL );
    var_Create( p_intf, "loop", VLC_VAR_BOOL );
    var_Create( p_intf, "loop", VLC_VAR_BOOL );
    var_Change( p_intf, "repeat", VLC_VAR_INHERITVALUE, & val, NULL );
    var_Change( p_intf, "repeat", VLC_VAR_INHERITVALUE, & val, NULL );

    /* Create our "Manage" menu */
    wxMenu *manage_menu = new wxMenu;
    manage_menu->Append( AddFile_Event, wxU(_("&Simple Add...")) );
    manage_menu->Append( AddMRL_Event, wxU(_("&Add MRL...")) );
    manage_menu->Append( Sort_Event, wxU(_("&Sort")) );
    manage_menu->Append( RSort_Event, wxU(_("&Reverse Sort")) );
    manage_menu->Append( Open_Event, wxU(_("&Open Playlist...")) );
    manage_menu->Append( Save_Event, wxU(_("&Save Playlist...")) );
    manage_menu->AppendSeparator();
    manage_menu->Append( Close_Event, wxU(_("&Close")) );

    /* Create our "Selection" menu */
    wxMenu *selection_menu = new wxMenu;
    selection_menu->Append( EnableSelection_Event, wxU(_("&Enable")) );
    selection_menu->Append( DisableSelection_Event, wxU(_("&Disable")) );
    selection_menu->AppendSeparator();
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
                               wxDefaultPosition, wxSize( 305, 300 ),
                               wxLC_REPORT | wxSUNKEN_BORDER );
    listview->InsertColumn( 0, wxU(_("Url")) );
    #if 0
        listview->InsertColumn( 1, wxU(_("Duration")) );
    #endif
    listview->SetColumnWidth( 0, 300 );
    #if 0
        listview->SetColumnWidth( 1, 100 );
    #endif

    /* Create the Random checkbox */
    wxCheckBox *random_checkbox =
        new wxCheckBox( playlist_panel, Random_Event, wxU(_("Random")) );

    var_Get( p_intf, "random", &val);
    vlc_bool_t b_random = val.b_bool;
    random_checkbox->SetValue( b_random == VLC_FALSE ? 0 : 1);

    /* Create the Loop Checkbox */
    wxCheckBox *loop_checkbox =
        new wxCheckBox( playlist_panel, Loop_Event, wxU(_("Loop")) );

    var_Get( p_intf, "loop", &val );
    int b_loop = val.b_bool ;
    loop_checkbox->SetValue( b_loop );

    /* Create the Repeat one checkbox */
    wxCheckBox *repeat_checkbox =
        new wxCheckBox( playlist_panel, Repeat_Event, wxU(_("Repeat one")) );

    var_Get( p_intf, "repeat", &val );
    int b_repeat = val.b_bool ;
    repeat_checkbox->SetValue( b_repeat );

    /* Create the Search Textbox */
    search_text =
        new wxTextCtrl( playlist_panel, SearchText_Event, wxT(""),
                        wxDefaultPosition, wxSize( 140, -1),
                        wxTE_PROCESS_ENTER);

    /* Create the search button */
    search_button =
        new wxButton( playlist_panel, Search_Event, wxU(_("Search")) );

    wxButton *en_dis_button =
        new wxButton( playlist_panel, En_Dis_Event, wxU(_("Enable/Disable Group") ) );

    wxButton *iteminfo_button =
        new wxButton( playlist_panel, Infos_Event, wxU(_("Item Infos") ) );
    /* Place everything in sizers */
    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    button_sizer->Add( en_dis_button, 0, wxALIGN_CENTER|wxRIGHT, 5);
    button_sizer->Add( iteminfo_button, 0, wxALIGN_CENTER|wxLEFT , 5);
    button_sizer->Layout();


    wxBoxSizer *checkbox_sizer = new wxBoxSizer( wxHORIZONTAL );
    checkbox_sizer->Add( random_checkbox, 0,
                       wxEXPAND|wxALIGN_RIGHT, 5);
    checkbox_sizer->Add( loop_checkbox, 0,
                       wxEXPAND|wxALIGN_RIGHT, 5);
    checkbox_sizer->Add( repeat_checkbox, 0,
                       wxEXPAND|wxALIGN_RIGHT, 5);

    checkbox_sizer->Layout();

    wxBoxSizer *search_sizer = new wxBoxSizer( wxHORIZONTAL );
    search_sizer->Add( search_text, 0, wxALL|wxALIGN_CENTER, 5);
    search_sizer->Add( search_button, 0, wxALL|wxALIGN_CENTER, 5);

    search_sizer->Layout();

    wxBoxSizer *bottom_sizer = new wxBoxSizer( wxVERTICAL );
    bottom_sizer->Add( checkbox_sizer, 0, wxALL|wxALIGN_CENTER, 5 );
    bottom_sizer->Add( button_sizer , 0, wxALL|wxALIGN_CENTER, 5 );

    bottom_sizer->Layout();

    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( listview, 1, wxEXPAND | wxALL, 5 );

    panel_sizer->Add( search_sizer, 0, wxALIGN_CENTRE );
    panel_sizer->Add( bottom_sizer, 0 , wxALIGN_CENTRE);
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

    delete iteminfo_dialog;

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
        if( p_playlist->pp_items[i]->b_enabled == VLC_FALSE )
        {
            wxListItem listitem;
            listitem.m_itemId = i;
            listitem.SetTextColour( *wxLIGHT_GREY);
            listview->SetItem(listitem);
        }
        /* FIXME: we should try to find the actual duration... */
        /* While we don't use it, hide it, it's ugly */
        #if 0
            listview->SetItem( i, 1, wxU(_("no info")) );
        #endif
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    /* Change the colour for the currenty played stream */
    wxListItem listitem;
    listitem.m_itemId = p_playlist->i_index;
    listitem.SetTextColour( *wxRED );
    listview->SetItem( listitem );

    listview->Focus( p_playlist->i_index );

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
    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE_SIMPLE, 0, 0 );

#if 0
    Rebuild();
#endif
}

void Playlist::OnAddMRL( wxCommandEvent& WXUNUSED(event) )
{
    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE, 0, 0 );

#if 0
    Rebuild();
#endif
}

void Playlist::OnSort( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Sort( p_playlist , 0  );

    vlc_object_release( p_playlist );

    Rebuild();

    return;
}

void Playlist::OnRSort( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Sort( p_playlist , 1 );

    vlc_object_release( p_playlist );

    Rebuild();

    return;
}

void Playlist::OnSearchTextChange( wxCommandEvent& WXUNUSED(event) )
{
   search_button->SetDefault();
}

void Playlist::OnSearch( wxCommandEvent& WXUNUSED(event) )
{
    wxString search_string= search_text->GetValue();

    int i_current;
    int i_first = 0 ;
    int i_item = -1;

    for( i_current = 0 ; i_current <= listview->GetItemCount() ; i_current++ ) 
    {
        if( listview->GetItemState( i_current, wxLIST_STATE_SELECTED)
                   == wxLIST_STATE_SELECTED )
        {
            i_first = i_current;
            break;
        }
    }

    for ( i_current = i_first + 1; i_current <= listview->GetItemCount() ; 
          i_current++ )
    {
        wxListItem listitem;
        listitem.SetId( i_current );
        listview->GetItem( listitem );
        if( listitem.m_text.Lower().Contains( search_string.Lower() ) )
        {
            i_item = i_current;
            break;
        }
    }
    for( long item = 0; item < listview->GetItemCount(); item++ )
    {
        listview->Select( item, FALSE );
    }
 
    wxListItem listitem;
    listitem.SetId(i_item);
    listitem.m_state = wxLIST_STATE_SELECTED;
    listview->Select( i_item, TRUE );
    listview->Focus( i_item );

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

void Playlist::OnEnableSelection( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    for( long item = listview->GetItemCount() - 1; item >= 0; item-- )
    {
        if( listview->IsSelected( item ) )
        {
            playlist_Enable( p_playlist, item );
        }
    }
    vlc_object_release( p_playlist);
    Rebuild();
}

void Playlist::OnDisableSelection( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    for( long item = listview->GetItemCount() - 1; item >= 0; item-- )
    {
        if( listview->IsSelected( item ) )
        {
            playlist_Disable( p_playlist, item );
        }
    }
    vlc_object_release( p_playlist);
    Rebuild();
}

void Playlist::OnRandom( wxCommandEvent& event )
{
    vlc_value_t val;
    val.b_bool = event.IsChecked();
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
    var_Set( p_playlist , "random", val);
    vlc_object_release( p_playlist );
}
void Playlist::OnLoop ( wxCommandEvent& event )
{
    vlc_value_t val;
    val.b_bool = event.IsChecked();
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
    var_Set( p_playlist , "loop", val);
    vlc_object_release( p_playlist );
}

void Playlist::OnRepeat ( wxCommandEvent& event )
{
    vlc_value_t val;
    val.b_bool = event.IsChecked();
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
    var_Set( p_playlist , "repeat", val);
    vlc_object_release( p_playlist );
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

void Playlist::OnInfos( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    if( iteminfo_dialog == NULL )
    {
        /* We use the first selected item, so find it */
        long i_item = -1;
        i_item = listview->GetNextItem(i_item,
                         wxLIST_NEXT_ALL,
                         wxLIST_STATE_SELECTED);
        if( i_item >= 0 && i_item < p_playlist->i_size )
        {
            iteminfo_dialog = new ItemInfoDialog(
                              p_intf, p_playlist->pp_items[i_item], this );
            if( iteminfo_dialog->ShowModal()  == wxID_OK )
                Rebuild();
            delete iteminfo_dialog;
            iteminfo_dialog = NULL;
        }
    }
    vlc_object_release( p_playlist );
}


void Playlist::OnEnDis( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    long i_item = -1;
    i_item = listview->GetNextItem(i_item,
                       wxLIST_NEXT_ALL,
                       wxLIST_STATE_SELECTED);

    if( i_item >= 0 && i_item < p_playlist->i_size )
    {
       if( p_playlist->pp_items[i_item]->b_enabled == VLC_TRUE)
           playlist_DisableGroup( p_playlist ,
                                  p_playlist->pp_items[i_item]->i_group );
       else
           playlist_EnableGroup( p_playlist ,
                                  p_playlist->pp_items[i_item]->i_group );
       Rebuild();
    }

    vlc_object_release( p_playlist );
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
