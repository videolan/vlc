/*****************************************************************************
 * playlist.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
 * $Id$
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
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "wxwindows.h"

/* Callback prototype */
static int PlaylistChanged( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void * );
static int PlaylistNext( vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void * );
static int ItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );

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

    SortTitle_Event,
    RSortTitle_Event,
    SortAuthor_Event,
    RSortAuthor_Event,
    SortGroup_Event,
    RSortGroup_Event,
    Randomize_Event,

    EnableSelection_Event,
    DisableSelection_Event,

    InvertSelection_Event,
    DeleteSelection_Event,
    Random_Event,
    Loop_Event,
    Repeat_Event,
    SelectAll_Event,

    EnableGroup_Event,
    DisableGroup_Event,

    Up_Event,
    Down_Event,
    Infos_Event,

    PopupPlay_Event,
    PopupDel_Event,
    PopupEna_Event,
    PopupInfo_Event,

    SearchText_Event,
    Search_Event,

    /* controls */
    ListView_Event,

    Browse_Event,  /* For export playlist */

    /* custom events */
    UpdateItem_Event
};

DEFINE_LOCAL_EVENT_TYPE( wxEVT_PLAYLIST );

BEGIN_EVENT_TABLE(Playlist, wxFrame)
    /* Menu events */
    EVT_MENU(AddFile_Event, Playlist::OnAddFile)
    EVT_MENU(AddMRL_Event, Playlist::OnAddMRL)
    EVT_MENU(Close_Event, Playlist::OnClose)
    EVT_MENU(Open_Event, Playlist::OnOpen)
    EVT_MENU(Save_Event, Playlist::OnSave)

    EVT_MENU(SortTitle_Event, Playlist::OnSort)
    EVT_MENU(RSortTitle_Event, Playlist::OnSort)
    EVT_MENU(SortAuthor_Event, Playlist::OnSort)
    EVT_MENU(RSortAuthor_Event, Playlist::OnSort)
    EVT_MENU(SortGroup_Event, Playlist::OnSort)
    EVT_MENU(RSortGroup_Event, Playlist::OnSort)

    EVT_MENU(Randomize_Event, Playlist::OnSort)

    EVT_MENU(EnableSelection_Event, Playlist::OnEnableSelection)
    EVT_MENU(DisableSelection_Event, Playlist::OnDisableSelection)
    EVT_MENU(InvertSelection_Event, Playlist::OnInvertSelection)
    EVT_MENU(DeleteSelection_Event, Playlist::OnDeleteSelection)
    EVT_MENU(SelectAll_Event, Playlist::OnSelectAll)
    EVT_MENU(Infos_Event, Playlist::OnInfos)
    EVT_CHECKBOX(Random_Event, Playlist::OnRandom)
    EVT_CHECKBOX(Repeat_Event, Playlist::OnRepeat)
    EVT_CHECKBOX(Loop_Event, Playlist::OnLoop)

    EVT_MENU(EnableGroup_Event, Playlist::OnEnDis)
    EVT_MENU(DisableGroup_Event, Playlist::OnEnDis)

    /* Listview events */
    EVT_LIST_ITEM_ACTIVATED(ListView_Event, Playlist::OnActivateItem)
    EVT_LIST_COL_CLICK(ListView_Event, Playlist::OnColSelect)
    EVT_LIST_KEY_DOWN(ListView_Event, Playlist::OnKeyDown)
    EVT_LIST_ITEM_RIGHT_CLICK(ListView_Event, Playlist::OnPopup)

    /* Popup events */
    EVT_MENU( PopupPlay_Event, Playlist::OnPopupPlay)
    EVT_MENU( PopupDel_Event, Playlist::OnPopupDel)
    EVT_MENU( PopupEna_Event, Playlist::OnPopupEna)
    EVT_MENU( PopupInfo_Event, Playlist::OnPopupInfo)


    /* Button events */
    EVT_BUTTON( Search_Event, Playlist::OnSearch)
    EVT_BUTTON( Save_Event, Playlist::OnSave)
    EVT_BUTTON( Infos_Event, Playlist::OnInfos)

    EVT_BUTTON( Up_Event, Playlist::OnUp)
    EVT_BUTTON( Down_Event, Playlist::OnDown)

    EVT_TEXT(SearchText_Event, Playlist::OnSearchTextChange)

    /* Custom events */
    EVT_COMMAND(-1, wxEVT_PLAYLIST, Playlist::OnPlaylistEvent)

    /* Special events : we don't want to destroy the window when the user
     * clicks on (X) */
    EVT_CLOSE(Playlist::OnClose)
END_EVENT_TABLE()


/* Event Table for the Newgroup class */
BEGIN_EVENT_TABLE(NewGroup, wxDialog)
    EVT_BUTTON( wxID_OK, NewGroup::OnOk)
    EVT_BUTTON( wxID_CANCEL, NewGroup::OnCancel)
END_EVENT_TABLE()


/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Playlist::Playlist( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxFrame( p_parent, -1, wxU(_("Playlist")), wxDefaultPosition,
             wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    vlc_value_t val;

    /* Initializations */
    iteminfo_dialog = NULL;
    p_intf = _p_intf;
    i_update_counter = 0;
    i_sort_mode = MODE_NONE;
    b_need_update = VLC_FALSE;
    SetIcon( *p_intf->p_sys->p_icon );

    i_title_sorted = 0;
    i_author_sorted = 0;
    i_group_sorted = 0;
    i_duration_sorted = 0;

    var_Create( p_intf, "random", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_intf, "loop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_intf, "repeat", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );;

    /* Create our "Manage" menu */
    wxMenu *manage_menu = new wxMenu;
    manage_menu->Append( AddFile_Event, wxU(_("&Simple Add...")) );
    manage_menu->Append( AddMRL_Event, wxU(_("&Add MRL...")) );
    manage_menu->AppendSeparator();
    manage_menu->Append( Open_Event, wxU(_("&Open Playlist...")) );
    manage_menu->Append( Save_Event, wxU(_("&Save Playlist...")) );
    manage_menu->AppendSeparator();
    manage_menu->Append( Close_Event, wxU(_("&Close")) );

    /* Create our "Sort" menu */
    wxMenu *sort_menu = new wxMenu;
    sort_menu->Append( SortTitle_Event, wxU(_("Sort by &title")) );
    sort_menu->Append( RSortTitle_Event, wxU(_("&Reverse sort by title")) );
    sort_menu->AppendSeparator();
    sort_menu->Append( SortAuthor_Event, wxU(_("Sort by &author")) );
    sort_menu->Append( RSortAuthor_Event, wxU(_("Reverse sort by author")) );
    sort_menu->AppendSeparator();
    sort_menu->Append( SortGroup_Event, wxU(_("Sort by &group")) );
    sort_menu->Append( RSortGroup_Event, wxU(_("Reverse sort by group")) );
    sort_menu->AppendSeparator();
    sort_menu->Append( Randomize_Event, wxU(_("&Shuffle Playlist")) );

    /* Create our "Selection" menu */
    wxMenu *selection_menu = new wxMenu;
    selection_menu->Append( EnableSelection_Event, wxU(_("&Enable")) );
    selection_menu->Append( DisableSelection_Event, wxU(_("&Disable")) );
    selection_menu->AppendSeparator();
    selection_menu->Append( InvertSelection_Event, wxU(_("&Invert")) );
    selection_menu->Append( DeleteSelection_Event, wxU(_("D&elete")) );
    selection_menu->Append( SelectAll_Event, wxU(_("&Select All")) );

    /* Create our "Group" menu */
    wxMenu *group_menu = new wxMenu;
    group_menu->Append( EnableGroup_Event, wxU(_("&Enable all group items")) );
    group_menu->Append( DisableGroup_Event,
                        wxU(_("&Disable all group items")) );

    /* Append the freshly created menus to the menu bar */
    wxMenuBar *menubar = new wxMenuBar( wxMB_DOCKABLE );
    menubar->Append( manage_menu, wxU(_("&Manage")) );
    menubar->Append( sort_menu, wxU(_("S&ort")) );
    menubar->Append( selection_menu, wxU(_("&Selection")) );
    menubar->Append( group_menu, wxU(_("&Groups")) );

    /* Attach the menu bar to the frame */
    SetMenuBar( menubar );

    /* Create the popup menu */
    popup_menu = new wxMenu;
    popup_menu->Append( PopupPlay_Event, wxU(_("Play")) );
    popup_menu->Append( PopupDel_Event, wxU(_("Delete")) );
    popup_menu->Append( PopupEna_Event, wxU(_("Enable/Disable")) );
    popup_menu->Append( PopupInfo_Event, wxU(_("Info")) );

    /* Create a panel to put everything in */
    wxPanel *playlist_panel = new wxPanel( this, -1 );
    playlist_panel->SetAutoLayout( TRUE );

    /* Create the Random checkbox */
    wxCheckBox *random_checkbox =
        new wxCheckBox( playlist_panel, Random_Event, wxU(_("Random")) );
    var_Get( p_intf, "random", &val );
    vlc_bool_t b_random = val.b_bool;
    random_checkbox->SetValue( b_random == VLC_FALSE ? 0 : 1 );

    /* Create the Loop Checkbox */
    wxCheckBox *loop_checkbox =
        new wxCheckBox( playlist_panel, Loop_Event, wxU(_("Repeat All")) );
    var_Get( p_intf, "loop", &val );
    int b_loop = val.b_bool ;
    loop_checkbox->SetValue( b_loop );

    /* Create the Repeat one checkbox */
    wxCheckBox *repeat_checkbox =
        new wxCheckBox( playlist_panel, Repeat_Event, wxU(_("Repeat One")) );
    var_Get( p_intf, "repeat", &val );
    int b_repeat = val.b_bool ;
    repeat_checkbox->SetValue( b_repeat );

    /* Create the Search Textbox */
    search_text =
        new wxTextCtrl( playlist_panel, SearchText_Event, wxT(""),
                        wxDefaultPosition, wxSize(140, -1),
                        wxTE_PROCESS_ENTER);

    /* Create the search button */
    search_button =
        new wxButton( playlist_panel, Search_Event, wxU(_("Search")) );


    /* Create the listview */
    /* FIXME: the given size is arbitrary, and prevents us from resizing
     * the window to smaller dimensions. But the sizers don't seem to adjust
     * themselves to the size of a listview, and with a wxDefaultSize the
     * playlist window is ridiculously small */
    listview = new wxListView( playlist_panel, ListView_Event,
                               wxDefaultPosition, wxSize( 500, 300 ),
                               wxLC_REPORT | wxSUNKEN_BORDER );
    listview->InsertColumn( 0, wxU(_("Name")) );
    listview->InsertColumn( 1, wxU(_("Author")) );
    listview->InsertColumn( 2, wxU(_("Duration")) );
    listview->InsertColumn( 3, wxU(_("Group")) );
    listview->SetColumnWidth( 0, 270 );
    listview->SetColumnWidth( 1, 150 );
    listview->SetColumnWidth( 2, 80 );

    /* Create the Up-Down buttons */
    wxButton *up_button =
        new wxButton( playlist_panel, Up_Event, wxU(_("Up") ) );

    wxButton *down_button =
        new wxButton( playlist_panel, Down_Event, wxU(_("Down") ) );

    /* Create the iteminfo button */
    wxButton *iteminfo_button =
        new wxButton( playlist_panel, Infos_Event, wxU(_("Item info") ) );

    /* Place everything in sizers */
    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    button_sizer->Add( iteminfo_button, 0, wxALIGN_CENTER|wxLEFT , 5);
    button_sizer->Layout();

    wxBoxSizer *updown_sizer = new wxBoxSizer( wxHORIZONTAL );
    updown_sizer->Add( up_button, 0, wxALIGN_LEFT|wxRIGHT, 3);
    updown_sizer->Add( down_button, 0, wxALIGN_LEFT|wxLEFT, 3);
    updown_sizer->Layout();

    wxBoxSizer *checkbox_sizer = new wxBoxSizer( wxHORIZONTAL );
    checkbox_sizer->Add( random_checkbox, 0,
                         wxEXPAND | wxALIGN_RIGHT | wxALL, 5);
    checkbox_sizer->Add( loop_checkbox, 0,
                         wxEXPAND | wxALIGN_RIGHT | wxALL, 5);
    checkbox_sizer->Add( repeat_checkbox, 0,
                         wxEXPAND | wxALIGN_RIGHT | wxALL, 5);
    checkbox_sizer->Layout();

    wxBoxSizer *search_sizer = new wxBoxSizer( wxHORIZONTAL );
    search_sizer->Add( search_text, 0, wxRIGHT|wxALIGN_CENTER, 3);
    search_sizer->Add( search_button, 0, wxLEFT|wxALIGN_CENTER, 3);
    search_sizer->Layout();

    /* The top and bottom sizers */
    wxBoxSizer *top_sizer = new wxBoxSizer( wxHORIZONTAL );
    top_sizer->Add( checkbox_sizer, 1, wxLEFT|wxRIGHT|wxALIGN_LEFT, 4 );
    top_sizer->Add( search_sizer, 1, wxLEFT|wxRIGHT|wxALIGN_RIGHT, 4 );
    top_sizer->Layout();

    wxBoxSizer *bottom_sizer = new wxBoxSizer( wxHORIZONTAL );
    bottom_sizer->Add( updown_sizer, 0,
                       wxEXPAND |wxRIGHT | wxLEFT | wxALIGN_LEFT, 4 );
    bottom_sizer->Add( button_sizer , 0,
                       wxEXPAND|wxLEFT | wxRIGHT | wxALIGN_RIGHT, 4 );
    bottom_sizer->Layout();

    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( top_sizer, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( listview, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( bottom_sizer, 0 , wxEXPAND | wxALL, 5);
    panel_sizer->Layout();

    playlist_panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( playlist_panel, 1, wxGROW, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );

#if wxUSE_DRAG_AND_DROP
    /* Associate drop targets with the playlist */
    SetDropTarget( new DragAndDrop( p_intf, VLC_TRUE ) );
#endif

    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* We want to be noticed of playlist changes */

    /* Some global changes happened -> Rebuild all */
    var_AddCallback( p_playlist, "intf-change", PlaylistChanged, this );

    /* We went to the next item */
    var_AddCallback( p_playlist, "playlist-current", PlaylistNext, this );

    /* One item has been updated */
    var_AddCallback( p_playlist, "item-change", ItemChanged, this );


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

    var_DelCallback( p_playlist, "item-change", ItemChanged, this );
    var_DelCallback( p_playlist, "playlist-current", PlaylistNext, this );
    var_DelCallback( p_playlist, "intf-change", PlaylistChanged, this );
    vlc_object_release( p_playlist );
}

/**********************************************************************
 * Update one playlist item
 **********************************************************************/
void Playlist::UpdateItem( int i )
{
    if( i < 0 ) return; /* Sanity check */

    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    playlist_item_t *p_item = playlist_ItemGetByPos( p_playlist, i );

    if( !p_item )
    {
        vlc_object_release(p_playlist);
        return;
    }

    listview->SetItem( i, 0, wxL2U(p_item->input.psz_name) );
    listview->SetItem( i, 1, wxU( playlist_ItemGetInfo( p_item,
                                       _("General") , _("Author") ) ) );
    char *psz_group = playlist_FindGroup(p_playlist,
                                         p_item->i_group);
    listview->SetItem( i, 3,
             wxL2U( psz_group ? psz_group : _("Normal") ) );

    if( p_item->b_enabled == VLC_FALSE )
    {
        wxListItem listitem;
        listitem.m_itemId = i;
        listitem.SetTextColour( *wxLIGHT_GREY);
        listview->SetItem(listitem);
    }

    char psz_duration[MSTRTIME_MAX_SIZE];
    mtime_t dur = p_item->input.i_duration;
    if( dur != -1 ) secstotimestr( psz_duration, dur/1000000 );
    else memcpy( psz_duration , "-:--:--", sizeof("-:--:--") );
    listview->SetItem( i, 2, wxU(psz_duration) );

    /* Change the colour for the currenty played stream */
    wxListItem listitem;
    listitem.m_itemId = i;
    if( i == p_playlist->i_index )
    {
        listitem.SetTextColour( *wxRED );
    }
    else
    {
        listitem.SetTextColour( *wxBLACK );
    }
    listview->SetItem( listitem );

    vlc_object_release(p_playlist);
}

/**********************************************************************
 * Rebuild the playlist
 **********************************************************************/
void Playlist::Rebuild()
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    int i_focused = listview->GetFocusedItem();

    /* Clear the list... */
    listview->DeleteAllItems();

    /* ...and rebuild it */
    vlc_mutex_lock( &p_playlist->object_lock );
    for( int i = 0; i < p_playlist->i_size; i++ )
    {
        wxString filename = wxL2U(p_playlist->pp_items[i]->input.psz_name);
        listview->InsertItem( i, filename );
        UpdateItem( i );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    if( i_focused >= 0 && i_focused < p_playlist->i_size )
    {
        listview->Focus( i_focused );
        listview->Select( i_focused );
    }
    else if( p_playlist->i_index >= 0 )
    {
        listview->Focus( p_playlist->i_index );
    }

    vlc_object_release( p_playlist );
}

void Playlist::ShowPlaylist( bool show )
{
    if( show ) Rebuild();
    Show( show );
}

void Playlist::UpdatePlaylist()
{
    i_update_counter++;

    /* If the playlist isn't show there's no need to update it */
    if( !IsShown() ) return;

    if( this->b_need_update )
    {
        this->b_need_update = VLC_FALSE;
        Rebuild();
    }

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
    int i_playlist_index = p_playlist->i_index;
    if( p_intf->p_sys->i_playing != i_playlist_index )
    {
        wxListItem listitem;
        listitem.m_itemId = i_playlist_index;
        listitem.SetTextColour( *wxRED );
        listview->SetItem( listitem );

        if( p_intf->p_sys->i_playing != -1 )
        {
            listitem.m_itemId = p_intf->p_sys->i_playing;
            listitem.SetTextColour( *wxBLACK );
            listview->SetItem( listitem );
        }
        p_intf->p_sys->i_playing = i_playlist_index;
    }

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
    struct {
        char *psz_desc;
        char *psz_filter;
        char *psz_module;
    } formats[] = {{ _("M3U file"), "*.m3u", "export-m3u" },
                   { _("PLS file"), "*.pls", "export-pls" }};
    wxString filter = wxT("");

    playlist_t * p_playlist =
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );

    if( ! p_playlist )
    {
        return;
    }
    if( p_playlist->i_size == 0 )
    {
        wxMessageBox( wxU(_("Playlist is empty") ), wxU(_("Can't save")),
                      wxICON_WARNING | wxOK, this );
        vlc_object_release( p_playlist );
        return;
    }

    for( unsigned int i = 0; i < sizeof(formats)/sizeof(formats[0]); i++)
    {
        filter.Append( wxU(formats[i].psz_desc) );
        filter.Append( wxT("|") );
        filter.Append( wxU(formats[i].psz_filter) );
        filter.Append( wxT("|") );
    }
    wxFileDialog dialog( this, wxU(_("Save playlist")),
                         wxT(""), wxT(""), filter, wxSAVE );

    if( dialog.ShowModal() == wxID_OK )
    {
        if( dialog.GetPath().mb_str() )
        {
            playlist_Export( p_playlist, dialog.GetPath().mb_str(),
                             formats[dialog.GetFilterIndex()].psz_module );
        }
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

    wxFileDialog dialog( this, wxU(_("Open playlist")), wxT(""), wxT(""),
        wxT("All playlists|*.pls;*.m3u;*.asx;*.b4s|M3U files|*.m3u"), wxOPEN );

    if( dialog.ShowModal() == wxID_OK )
    {
        playlist_Import( p_playlist, dialog.GetPath().mb_str() );
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

/********************************************************************
 * Move functions
 ********************************************************************/
void Playlist::OnUp( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* We use the first selected item, so find it */
    long i_item = listview->GetNextItem( -1, wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED);
    if( i_item > 0 && i_item < p_playlist->i_size )
    {
        playlist_Move( p_playlist, i_item, i_item - 1 );
        listview->Focus( i_item - 1 );
    }
    vlc_object_release( p_playlist );
    return;
}

void Playlist::OnDown( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* We use the first selected item, so find it */
    long i_item = listview->GetNextItem( -1, wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED );
    if( i_item >= 0 && i_item < p_playlist->i_size - 1 )
    {
        playlist_Move( p_playlist , i_item, i_item + 2 );
        listview->Focus( i_item + 1 );
    }
    vlc_object_release( p_playlist );
    return;
}

/********************************************************************
 * Sorting functions
 ********************************************************************/
void Playlist::OnSort( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
    switch( event.GetId() )
    {
        case SortTitle_Event:
           playlist_SortTitle( p_playlist , ORDER_NORMAL );
           break;
        case RSortTitle_Event:
           playlist_SortTitle( p_playlist , ORDER_REVERSE );
           break;
        case SortAuthor_Event:
           playlist_SortAuthor(p_playlist , ORDER_NORMAL );
           break;
        case RSortAuthor_Event:
           playlist_SortAuthor( p_playlist , ORDER_REVERSE );
           break;
        case SortGroup_Event:
           playlist_SortGroup( p_playlist , ORDER_NORMAL );
           break;
        case RSortGroup_Event:
           playlist_SortGroup( p_playlist , ORDER_REVERSE );
           break;
        case Randomize_Event:
           playlist_Sort( p_playlist , SORT_RANDOM, ORDER_NORMAL );
           break;
    }
    vlc_object_release( p_playlist );

    Rebuild();

    return;
}

void Playlist::OnColSelect( wxListEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
    switch( event.GetColumn() )
    {
        case 0:
            if( i_title_sorted != 1 )
            {
                playlist_SortTitle( p_playlist, ORDER_NORMAL );
                i_title_sorted = 1;
            }
            else
            {
                playlist_SortTitle( p_playlist, ORDER_REVERSE );
                i_title_sorted = -1;
            }
            break;
        case 1:
            if( i_author_sorted != 1 )
            {
                playlist_SortAuthor( p_playlist, ORDER_NORMAL );
                i_author_sorted = 1;
            }
            else
            {
                playlist_SortAuthor( p_playlist, ORDER_REVERSE );
                i_author_sorted = -1;
            }
            break;
        case 2:
            if( i_duration_sorted != 1 )
            {
                playlist_Sort( p_playlist, SORT_DURATION, ORDER_NORMAL );
                i_duration_sorted = 1;
            }
            else
            {
                playlist_Sort( p_playlist, SORT_DURATION, ORDER_REVERSE );
                i_duration_sorted = -1;
            }
            break;
        case 3:
            if( i_group_sorted != 1 )
            {
                playlist_SortGroup( p_playlist, ORDER_NORMAL );
                i_group_sorted = 1;
            }
            else
            {
                playlist_SortGroup( p_playlist, ORDER_REVERSE );
                i_group_sorted = -1;
            }
            break;
        default:
            break;
    }
    vlc_object_release( p_playlist );

    Rebuild();

    return;
}

/**********************************************************************
 * Search functions
 **********************************************************************/
void Playlist::OnSearchTextChange( wxCommandEvent& WXUNUSED(event) )
{
   search_button->SetDefault();
}

void Playlist::OnSearch( wxCommandEvent& WXUNUSED(event) )
{
    wxString search_string = search_text->GetValue();

    bool b_ok = false;
    int i_current;
    int i_first = 0 ;
    int i_item = -1;

    for( i_current = 0; i_current < listview->GetItemCount(); i_current++ )
    {
        if( listview->GetItemState( i_current, wxLIST_STATE_SELECTED ) ==
              wxLIST_STATE_SELECTED )
        {
            i_first = i_current;
            break;
        }
    }

    if( i_first == listview->GetItemCount() )
    {
        i_first = -1;
    }

    for( i_current = i_first + 1; i_current < listview->GetItemCount();
         i_current++ )
    {
        wxListItem listitem;
        listitem.SetId( i_current );
        listview->GetItem( listitem );
        if( listitem.m_text.Lower().Contains( search_string.Lower() ) )
        {
            i_item = i_current;
            b_ok = true;
            break;
        }
        listitem.SetColumn( 1 );
        listview->GetItem( listitem );
        if( listitem.m_text.Lower().Contains( search_string.Lower() ) )
        {
            i_item = i_current;
            b_ok = true;
            break;
        }
    }
    if( !b_ok )
    {
        for( i_current = -1 ; i_current < i_first - 1;
             i_current++ )
        {
            wxListItem listitem;
            listitem.SetId( i_current );
            listview->GetItem( listitem );
            if( listitem.m_text.Lower().Contains( search_string.Lower() ) )
            {
                i_item = i_current;
                b_ok = true;
                break;
            }
            listitem.SetColumn( 1 );
            listview->GetItem( listitem );
            if( listitem.m_text.Lower().Contains( search_string.Lower() ) )
            {
                i_item = i_current;
                b_ok = true;
                break;
            }
        }
    }

    if( i_item < 0 || i_item >= listview->GetItemCount() ) return;

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

/**********************************************************************
 * Selection functions
 **********************************************************************/
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
            /*XXX*/
            playlist_Enable( p_playlist, item );
            UpdateItem( item );
        }
    }
    vlc_object_release( p_playlist);
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
            /*XXX*/
            playlist_Disable( p_playlist, item );
            UpdateItem( item );
        }
    }
    vlc_object_release( p_playlist);
}

void Playlist::OnSelectAll( wxCommandEvent& WXUNUSED(event) )
{
    for( long item = 0; item < listview->GetItemCount(); item++ )
    {
        listview->Select( item, TRUE );
    }
}

/**********************************************************************
 * Playlist mode functions
 **********************************************************************/
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

void Playlist::ShowInfos( int i_item )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
   
    if( i_item == -1 )
    {
        return;
    }
 
    if( iteminfo_dialog == NULL )
    {
        vlc_mutex_lock( &p_playlist->object_lock);
        playlist_item_t *p_item = playlist_ItemGetByPos( p_playlist, i_item );
        vlc_mutex_unlock( &p_playlist->object_lock );

        if( p_item )
        {
            iteminfo_dialog = new ItemInfoDialog(
                              p_intf, p_item , this );
            if( iteminfo_dialog->ShowModal()  == wxID_OK )
                UpdateItem( i_item );
            delete iteminfo_dialog;
            iteminfo_dialog = NULL;
        }
    }
    vlc_object_release( p_playlist );
}

void Playlist::OnInfos( wxCommandEvent& WXUNUSED(event) )
{
    /* We use the first selected item, so find it */
    long i_item = listview->GetNextItem( -1 , wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED );
    ShowInfos( i_item );
}

void Playlist::OnEnDis( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    long i_item = listview->GetNextItem( i_item, wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED );

    if( i_item >= 0 && i_item < p_playlist->i_size )
    {
       switch( event.GetId() )
       {
           case EnableGroup_Event:
               /*XXX*/
               playlist_EnableGroup( p_playlist ,
                                  p_playlist->pp_items[i_item]->i_group );
               break;
           case DisableGroup_Event:
               playlist_DisableGroup( p_playlist ,
                                  p_playlist->pp_items[i_item]->i_group );
               break;
       }
       Rebuild();
    }

    vlc_object_release( p_playlist );
}

/*****************************************************************************
 * Popup management functions
 *****************************************************************************/
void Playlist::OnPopup( wxListEvent& event )
{
    i_popup_item = event.GetIndex();
    Playlist::PopupMenu( popup_menu , ScreenToClient( wxGetMousePosition() ) );
}

void Playlist::OnPopupPlay( wxMenuEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
    if( i_popup_item != -1 )
    {
        playlist_Goto( p_playlist, i_popup_item );
    }
    vlc_object_release( p_playlist );
}

void Playlist::OnPopupDel( wxMenuEvent& event )
{
    DeleteItem( i_popup_item );
}

void Playlist::OnPopupEna( wxMenuEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    if( p_playlist->pp_items[i_popup_item]->b_enabled )
        //playlist_IsEnabled( p_playlist, i_popup_item ) )
    {
        playlist_Disable( p_playlist, i_popup_item );
    }
    else
    {
        playlist_Enable( p_playlist, i_popup_item );
    }
    vlc_object_release( p_playlist);
    UpdateItem( i_popup_item );
}

void Playlist::OnPopupInfo( wxMenuEvent& event )
{
    ShowInfos( i_popup_item );
}

/*****************************************************************************
 * Custom events management
 *****************************************************************************/
void Playlist::OnPlaylistEvent( wxCommandEvent& event )
{
    switch( event.GetId() )
    {
    case UpdateItem_Event:
        UpdateItem( event.GetInt() );
        break;
    }
}

/*****************************************************************************
 * PlaylistChanged: callback triggered by the intf-change playlist variable
 *  We don't rebuild the playlist directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param )
{
    Playlist *p_playlist_dialog = (Playlist *)param;
    p_playlist_dialog->b_need_update = VLC_TRUE;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Next: callback triggered by the playlist-current playlist variable
 *****************************************************************************/
static int PlaylistNext( vlc_object_t *p_this, const char *psz_variable,
                         vlc_value_t oval, vlc_value_t nval, void *param )
{
    Playlist *p_playlist_dialog = (Playlist *)param;

    wxCommandEvent event( wxEVT_PLAYLIST, UpdateItem_Event );
    event.SetInt( oval.i_int );
    p_playlist_dialog->AddPendingEvent( event );
    event.SetInt( nval.i_int );
    p_playlist_dialog->AddPendingEvent( event );

    return 0;
}

/*****************************************************************************
 * ItemChanged: callback triggered by the item-change playlist variable
 *****************************************************************************/
static int ItemChanged( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    Playlist *p_playlist_dialog = (Playlist *)param;

    wxCommandEvent event( wxEVT_PLAYLIST, UpdateItem_Event );
    event.SetInt( new_val.i_int );
    p_playlist_dialog->AddPendingEvent( event );

    return 0;
}

/***************************************************************************
 * NewGroup Class
 ***************************************************************************/
NewGroup::NewGroup( intf_thread_t *_p_intf, wxWindow *_p_parent ):
    wxDialog( _p_parent, -1, wxU(_("New Group")), wxDefaultPosition,
             wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    psz_name = NULL;
    SetIcon( *p_intf->p_sys->p_icon );

    /* Create a panel to put everything in*/
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );

    wxStaticText *group_label =
            new wxStaticText( panel , -1,
                wxU(_("Enter a name for the new group:")));

    groupname = new wxTextCtrl(panel, -1, wxU(""),wxDefaultPosition,
                               wxSize(100,27),wxTE_PROCESS_ENTER);

    wxButton *ok_button = new wxButton(panel, wxID_OK, wxU(_("OK")) );
    ok_button->SetDefault();
    wxButton *cancel_button = new wxButton( panel, wxID_CANCEL,
                                            wxU(_("Cancel")) );

    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );

    button_sizer->Add( ok_button, 0, wxALL, 5 );
    button_sizer->Add( cancel_button, 0, wxALL, 5 );
    button_sizer->Layout();

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( group_label, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( groupname, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( button_sizer, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Layout();

    panel->SetSizerAndFit( panel_sizer );

    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    main_sizer->Add( panel, 1, wxEXPAND, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );
}

NewGroup::~NewGroup()
{
}

void NewGroup::OnOk( wxCommandEvent& event )
{
    psz_name = strdup( groupname->GetLineText(0).mb_str() );

    playlist_t * p_playlist =
          (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );

    if( p_playlist )
    {
        if( !playlist_CreateGroup( p_playlist, psz_name ) )
        {
            psz_name = NULL;
        }
        vlc_object_release( p_playlist );
    }

    EndModal( wxID_OK );
}

void NewGroup::OnCancel( wxCommandEvent& WXUNUSED(event) )
{
    EndModal( wxID_CANCEL );
}
