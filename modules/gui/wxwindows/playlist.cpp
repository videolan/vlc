/*****************************************************************************
 * playlist.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
 * $Id$
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/OR MODIFy
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

#include "bitmaps/shuffle.xpm"
#include "bitmaps/repeat.xpm"
#include "bitmaps/loop.xpm"

#include "bitmaps/type_unknown.xpm"
#include "bitmaps/type_net.xpm"
#include "bitmaps/type_card.xpm"
#include "bitmaps/type_disc.xpm"
#include "bitmaps/type_directory.xpm"
#include "bitmaps/type_playlist.xpm"

#include <wx/dynarray.h>

#define HELP_SHUFFLE N_( "Shuffle" )
#define HELP_LOOP N_( "Loop" )
#define HELP_REPEAT N_( "Repeat" )

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
    Randomize_Event,

    EnableSelection_Event,
    DisableSelection_Event,

    InvertSelection_Event,
    DeleteSelection_Event,
    Random_Event,
    Loop_Event,
    Repeat_Event,
    SelectAll_Event,

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
    TreeCtrl_Event,

    Browse_Event,  /* For export playlist */

    /* custom events */
    UpdateItem_Event,

    FirstView_Event = wxID_HIGHEST + 1000,
};

DEFINE_LOCAL_EVENT_TYPE( wxEVT_PLAYLIST );

BEGIN_EVENT_TABLE(Playlist, wxFrame)
    EVT_SIZE(Playlist::OnSize)

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

    EVT_MENU(Randomize_Event, Playlist::OnSort)

    EVT_MENU(EnableSelection_Event, Playlist::OnEnableSelection)
    EVT_MENU(DisableSelection_Event, Playlist::OnDisableSelection)
    EVT_MENU(InvertSelection_Event, Playlist::OnInvertSelection)
    EVT_MENU(DeleteSelection_Event, Playlist::OnDeleteSelection)
    EVT_MENU(SelectAll_Event, Playlist::OnSelectAll)
    EVT_MENU(Infos_Event, Playlist::OnInfos)

    EVT_MENU_OPEN( Playlist::OnMenuOpen )
    EVT_MENU( -1, Playlist::OnMenuEvent )

    EVT_TOOL(Random_Event, Playlist::OnRandom)
    EVT_TOOL(Repeat_Event, Playlist::OnRepeat)
    EVT_TOOL(Loop_Event, Playlist::OnLoop)

    /* Popup events */
    EVT_MENU( PopupPlay_Event, Playlist::OnPopupPlay)
    EVT_MENU( PopupDel_Event, Playlist::OnPopupDel)
    EVT_MENU( PopupEna_Event, Playlist::OnPopupEna)
    EVT_MENU( PopupInfo_Event, Playlist::OnPopupInfo)

    /* Tree control events */
    EVT_TREE_ITEM_ACTIVATED( TreeCtrl_Event, Playlist::OnActivateItem )

    EVT_CONTEXT_MENU( Playlist::OnPopup )

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

/*****************************************************************************
 * PlaylistItem class
 ****************************************************************************/
class PlaylistItem : public wxTreeItemData
{
public:
    PlaylistItem( playlist_item_t *_p_item ) : wxTreeItemData()
    {
        p_item = _p_item;
    }
protected:
    playlist_item_t *p_item;
friend class Playlist;
};

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Playlist::Playlist( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxFrame( p_parent, -1, wxU(_("Playlist")), wxDefaultPosition,
             wxSize(345,400), wxDEFAULT_FRAME_STYLE )
{
    vlc_value_t val;

    /* Initializations */
    p_intf = _p_intf;
    i_update_counter = 0;
    i_sort_mode = MODE_NONE;
    b_need_update = VLC_FALSE;
    SetIcon( *p_intf->p_sys->p_icon );

    p_view_menu = NULL;

    i_current_view = VIEW_SIMPLE;

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
    sort_menu->Append( Randomize_Event, wxU(_("&Shuffle Playlist")) );

    /* Create our "Selection" menu */
    wxMenu *selection_menu = new wxMenu;
    selection_menu->Append( EnableSelection_Event, wxU(_("&Enable")) );
    selection_menu->Append( DisableSelection_Event, wxU(_("&Disable")) );
    selection_menu->AppendSeparator();
    selection_menu->Append( InvertSelection_Event, wxU(_("&Invert")) );
    selection_menu->Append( DeleteSelection_Event, wxU(_("D&elete")) );
    selection_menu->Append( SelectAll_Event, wxU(_("&Select All")) );

    /* Create our "View" menu */
    ViewMenu();

    /* Append the freshly created menus to the menu bar */
    wxMenuBar *menubar = new wxMenuBar( wxMB_DOCKABLE );
    menubar->Append( manage_menu, wxU(_("&Manage")) );
    menubar->Append( sort_menu, wxU(_("S&ort")) );
    menubar->Append( selection_menu, wxU(_("&Selection")) );
    menubar->Append( p_view_menu, wxU(_("&View items") ) );

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

    /* Create the toolbar */
    wxToolBar *toolbar =
        CreateToolBar( wxTB_HORIZONTAL | wxTB_FLAT | wxTB_DOCKABLE );

    /* Create the random tool */
    toolbar->AddTool( Random_Event, wxT(""), wxBitmap(shuffle_on_xpm),
                       wxBitmap(shuffle_on_xpm), wxITEM_CHECK,
                       wxU(_(HELP_SHUFFLE) ) );
    var_Get( p_intf, "random", &val );
    toolbar->ToggleTool( Random_Event, val.b_bool );

    /* Create the Loop tool */
    toolbar->AddTool( Loop_Event, wxT(""), wxBitmap( loop_xpm),
                      wxBitmap( loop_xpm), wxITEM_CHECK,
                      wxU(_(HELP_LOOP )  ) );
    var_Get( p_intf, "loop", &val );
    toolbar->ToggleTool( Loop_Event, val.b_bool );

    /* Create the Repeat one checkbox */
    toolbar->AddTool( Repeat_Event, wxT(""), wxBitmap( repeat_xpm),
                      wxBitmap( repeat_xpm), wxITEM_CHECK,
                      wxU(_(HELP_REPEAT )  ) );
    var_Get( p_intf, "repeat", &val );
    toolbar->ToggleTool( Repeat_Event, val.b_bool ) ;

    /* Create the Search Textbox */
    search_text = new wxTextCtrl( toolbar, SearchText_Event, wxT(""),
                                  wxDefaultPosition, wxSize(100, -1),
                                  wxTE_PROCESS_ENTER);

    /* Create the search button */
    search_button = new wxButton( toolbar , Search_Event, wxU(_("Search")) );

    toolbar->AddControl( new wxControl( toolbar, -1, wxDefaultPosition,
                         wxSize(16, 16), wxBORDER_NONE ) );
    toolbar->AddControl( search_text );
    toolbar->AddControl( new wxControl( toolbar, -1, wxDefaultPosition,
                         wxSize(5, 5), wxBORDER_NONE ) );
    toolbar->AddControl( search_button );
    search_button->SetDefault();
    toolbar->Realize();

    /* Create the tree */
    treectrl = new wxTreeCtrl( playlist_panel, TreeCtrl_Event,
                               wxDefaultPosition, wxDefaultSize,
                               wxTR_HIDE_ROOT | wxTR_LINES_AT_ROOT|
                               wxTR_NO_LINES |
                               wxTR_HAS_BUTTONS | wxTR_TWIST_BUTTONS |
                               wxTR_MULTIPLE | wxTR_EXTENDED );

    /* Create image list */

    wxImageList *p_images = new wxImageList( 16 , 16, TRUE);

    wxIcon icons[10];
    icons[ ITEM_TYPE_UNKNOWN ] = wxIcon( type_unknown_xpm );
    icons[ ITEM_TYPE_DISC ] = wxIcon( type_disc_xpm );
    icons[ ITEM_TYPE_DIRECTORY ] = wxIcon( type_directory_xpm );
    icons[ ITEM_TYPE_PLAYLIST ] = wxIcon( type_playlist_xpm );
    icons[ ITEM_TYPE_NET ] = wxIcon( type_net_xpm );
    icons[ ITEM_TYPE_CARD ] = wxIcon( type_card_xpm );

    for( int i = 0; i< WXSIZEOF( icons ) ; i++ )
    {
       p_images->Add( wxBitmap( wxBitmap(icons[i]).ConvertToImage().Rescale(16,16) ) );
    }

    treectrl->AssignImageList( p_images );

    treectrl->AddRoot( wxU(_("root" )), -1, -1, NULL );

    /* Reduce font size */
    wxFont font= treectrl->GetFont();
    font.SetPointSize(8);
    treectrl->SetFont( font );

    /* Create the Up-Down buttons */
#if 0
    wxButton *up_button =
        new wxButton( playlist_panel, Up_Event, wxU(_("Up") ) );
    wxButton *down_button =
        new wxButton( playlist_panel, Down_Event, wxU(_("Down") ) );

    wxBoxSizer *updown_sizer = new wxBoxSizer( wxHORIZONTAL );
    updown_sizer->Layout();
    /* The top and bottom sizers */
    wxBoxSizer *bottom_sizer = new wxBoxSizer( wxHORIZONTAL );
    bottom_sizer->Add( up_button, 0, wxALIGN_LEFT | wxRIGHT, 3);
    bottom_sizer->Add( down_button, 0, wxALIGN_LEFT | wxLEFT, 3);
    bottom_sizer->Layout();
#endif
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( treectrl, 1, wxEXPAND | wxALL, 5 );
#if 0
    panel_sizer->Add( bottom_sizer, 0, wxALL, 5);
#endif
    panel_sizer->Layout();

    playlist_panel->SetSizerAndFit( panel_sizer );

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

void Playlist::OnSize( wxSizeEvent& event)
{
#if 0
    wxSize size = GetClientSize();
    if( listview )
        listview->SetColumnWidth( 0, size.x - listview->GetColumnWidth(1)
                        - 15 /* margins */ );
#endif
    event.Skip();
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

    var_DelCallback( p_playlist, "item-change", ItemChanged, this );
    var_DelCallback( p_playlist, "playlist-current", PlaylistNext, this );
    var_DelCallback( p_playlist, "intf-change", PlaylistChanged, this );
    vlc_object_release( p_playlist );
}

/**********************************************************************
 * Update one playlist item
 **********************************************************************/
void Playlist::UpdateNode( playlist_t *p_playlist, playlist_item_t *p_node,
                           wxTreeItemId node )
{
    long cookie;
    wxTreeItemId child;
    for( int i = 0; i< p_node->i_children ; i++ )
    {
        if( i == 0 )
        {
            child = treectrl->GetFirstChild( node, cookie);
        }
        else
        {
            child = treectrl->GetNextChild( node, cookie );
        }

        if( !child.IsOk() )
        {
            /* Not enough children */
            CreateNode( p_playlist, p_node->pp_children[i], node );
            /* Keep the tree pointer up to date */
            child = treectrl->GetNextChild( node, cookie );
        }
        else
        {
        }
    }

}
/* Creates the node p_node as last child of parent */
void Playlist::CreateNode( playlist_t *p_playlist, playlist_item_t *p_node,
                           wxTreeItemId parent )
{
    long cookie;
    wxTreeItemId node =
        treectrl->AppendItem( parent, wxL2U( p_node->input.psz_name ),
                              -1,-1, new PlaylistItem( p_node ) );
    treectrl->SetItemImage( node, p_node->input.i_type );

    for( int i = 0; i< p_node->i_children ; i++ )
    {
        /* Append the item */
        if( p_node->pp_children[i]->i_children == -1 )
        {
            wxTreeItemId item =
                treectrl->AppendItem( node,
                    wxL2U( p_node->pp_children[i]->input.psz_name ), -1,-1,
                           new PlaylistItem( p_node->pp_children[i]) );

            treectrl->SetItemImage( item,
                                    p_node->pp_children[i]->input.i_type );
        }
        else
        {
            CreateNode( p_playlist, p_node->pp_children[i],
                        node );
        }
    }
}

wxTreeItemId Playlist::FindItem( wxTreeItemId root, playlist_item_t *p_item )
{
    long cookie;
    PlaylistItem *p_wxcurrent;
    wxTreeItemId search;
    wxTreeItemId item = treectrl->GetFirstChild( root, cookie );
    wxTreeItemId child;

    while( item.IsOk() )
    {
        p_wxcurrent = (PlaylistItem *)treectrl->GetItemData( item );
        if( p_wxcurrent->p_item == p_item )
        {
            return item;
        }
        if( treectrl->ItemHasChildren( item ) )
        {
            wxTreeItemId search = FindItem( item, p_item );
            if( search.IsOk() )
            {
                return search;
            }
        }
        item = treectrl->GetNextChild( root, cookie);
    }
    /* Not found */
    wxTreeItemId dummy;
    return dummy;
}

/*wxTreeItemId Playlist::FindItemByName( wxTreeItemId root, wxString search_string, wxTreeItemId current )
{
    long cookie;
    PlaylistItem *p_wxcurrent;
    wxTreeItemId search;
    wxTreeItemId item = treectrl->GetFirstChild( root, cookie );
    wxTreeItemId child;

    while( item.IsOk() )
    {
        if( treectrl->GetItemText( item).Lower().Contains(
                                                 search_string.Lower() ) )
        {
            return item;
        if( treectrl->ItemHasChildren( item ) )
        {
            wxTreeItemId search = FindItem( item, p_item );
            if( search.IsOk() )
            {
                return search;
            }
        }
        item = treectrl->GetNextChild( root, cookie);
    }
  */  /* Not found */
    /*wxTreeItemId dummy;
    return dummy;
}
*/



void Playlist::SetCurrentItem( wxTreeItemId item )
{
    if( item.IsOk() )
    {
        treectrl->SetItemBold( item, true );
        treectrl->EnsureVisible( item );
    }
}

void Playlist::UpdateItem( int i )
{
    if( i < 0 ) return; /* Sanity check */
    playlist_item_t *p_item;

    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    p_item = playlist_ItemGetById( p_playlist, i );

    wxTreeItemId item = FindItem( treectrl->GetRootItem(), p_item);

    UpdateTreeItem( p_playlist, item );

    vlc_object_release(p_playlist);

}

void Playlist::UpdateTreeItem( playlist_t *p_playlist ,wxTreeItemId item )
{
    playlist_item_t *p_item;

   if( !item.IsOk() )
   {
        return;
   }

    p_item  =  ((PlaylistItem *)treectrl->GetItemData( item ))->p_item;

    if( !p_item )
    {
        return;
    }

    wxString msg;
    char *psz_author = playlist_ItemGetInfo( p_item, _("Meta-information"),
                                                         _("Artist"));
    char psz_duration[MSTRTIME_MAX_SIZE];
    mtime_t dur = p_item->input.i_duration;
    if( dur != -1 )
        secstotimestr( psz_duration, dur/1000000 );
    else
        memcpy( psz_duration, "-:--:--", sizeof("-:--:--") );

    if( !strcmp( psz_author, "" ) )
    {
        msg.Printf( wxString( wxL2U( p_item->input.psz_name ) ) + wxU( " ( ") +
                    wxString(wxL2U(psz_duration ) ) + wxU( ")") );
    }
    else
    {
        msg.Printf( wxString(wxU( psz_author )) + wxT(" - ") +
                    wxString(wxL2U(p_item->input.psz_name)) + wxU( " ( ") +
                    wxString(wxL2U(psz_duration ) ) + wxU( ")") );
    }
    treectrl->SetItemText( item , msg );

    if( p_playlist->status.p_item == p_item )
    {
        SetCurrentItem( item );
    }
    else
    {
        treectrl->SetItemBold( item, false );
    }
#if 0
    if( p_item->b_enabled == VLC_FALSE )
    {
        wxListItem listitem;
        listitem.m_itemId = i;
        listitem.SetTextColour( *wxLIGHT_GREY);
        listview->SetItem(listitem);
    }
#endif
}

/**********************************************************************
 * Rebuild the playlist
 **********************************************************************/
void Playlist::Rebuild()
{
    playlist_view_t *p_view;
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* ...and rebuild it */
    vlc_mutex_lock( &p_playlist->object_lock );

    p_view = playlist_ViewFind( p_playlist, i_current_view ); /* FIXME */

    /* HACK we should really get new*/
    msg_Dbg( p_intf, "rebuilding tree" );
    treectrl->DeleteAllItems();
    treectrl->AddRoot( wxU(_("root" )), -1, -1,
                         new PlaylistItem( p_view->p_root) );

    wxTreeItemId root = treectrl->GetRootItem();
    UpdateNode( p_playlist, p_view->p_root, root );

    wxTreeItemId item;
    if( p_playlist->status.p_item != NULL )
    {
        item = FindItem( root, p_playlist->status.p_item );
    }
    else if( p_playlist->status.p_node != NULL )
    {
        item = FindItem( root, p_playlist->status.p_node );
    }
    else
    {
        item = root;
    }

    SetCurrentItem( item );

    vlc_mutex_unlock( &p_playlist->object_lock );

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
#if 0
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
#endif
    vlc_object_release( p_playlist );
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void Playlist::DeleteItem( int item_id )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Delete( p_playlist, item_id );

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

}

void Playlist::OnAddMRL( wxCommandEvent& WXUNUSED(event) )
{
    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE, 0, 0 );

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
#if 0
    /* We use the first selected item, so find it */
    long i_item = listview->GetNextItem( -1, wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED);
    if( i_item > 0 && i_item < p_playlist->i_size )
    {
        playlist_Move( p_playlist, i_item, i_item - 1 );
        listview->Focus( i_item - 1 );
    }
#endif
    vlc_object_release( p_playlist );
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
#if 0
    /* We use the first selected item, so find it */
    long i_item = listview->GetNextItem( -1, wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED );
    if( i_item >= 0 && i_item < p_playlist->i_size - 1 )
    {
        playlist_Move( p_playlist, i_item, i_item + 2 );
        listview->Focus( i_item + 1 );
    }
#endif
    vlc_object_release( p_playlist );
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
           playlist_SortTitle( p_playlist, ORDER_NORMAL );
           break;
        case RSortTitle_Event:
           playlist_SortTitle( p_playlist, ORDER_REVERSE );
           break;
        case SortAuthor_Event:
           playlist_SortAuthor(p_playlist, ORDER_NORMAL );
           break;
        case RSortAuthor_Event:
           playlist_SortAuthor( p_playlist, ORDER_REVERSE );
           break;
        case Randomize_Event:
           playlist_Sort( p_playlist, SORT_RANDOM, ORDER_NORMAL );
           break;
    }
    vlc_object_release( p_playlist );

    Rebuild();
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
}

#if 0
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

    if( i_item < 0 || i_item >= listview->GetItemCount() )
    {
        return;
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
#endif

/**********************************************************************
 * Selection functions
 **********************************************************************/
void Playlist::OnInvertSelection( wxCommandEvent& WXUNUSED(event) )
{
}

void Playlist::OnDeleteSelection( wxCommandEvent& WXUNUSED(event) )
{
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
#if 0
    for( long item = listview->GetItemCount() - 1; item >= 0; item-- )
    {
        if( listview->IsSelected( item ) )
        {
            /*XXX*/
            playlist_Enable( p_playlist, item );
            UpdateItem( item );
        }
    }
#endif
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
#if 0
    for( long item = listview->GetItemCount() - 1; item >= 0; item-- )
    {
        if( listview->IsSelected( item ) )
        {
            /*XXX*/
            playlist_Disable( p_playlist, item );
            UpdateItem( item );
        }
    }
#endif
    vlc_object_release( p_playlist);
}

void Playlist::OnSelectAll( wxCommandEvent& WXUNUSED(event) )
{
#if 0
    for( long item = 0; item < listview->GetItemCount(); item++ )
    {
        listview->Select( item, TRUE );
    }
#endif
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
    var_Set( p_playlist, "random", val);
    vlc_object_release( p_playlist );
}

void Playlist::OnLoop( wxCommandEvent& event )
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
    var_Set( p_playlist, "loop", val);
    vlc_object_release( p_playlist );
}

void Playlist::OnRepeat( wxCommandEvent& event )
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
    var_Set( p_playlist, "repeat", val);
    vlc_object_release( p_playlist );
}



void Playlist::OnActivateItem( wxTreeEvent& event )
{
    playlist_item_t *p_item,*p_node;
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
		                 VLC_OBJECT_PLAYLIST,FIND_ANYWHERE );
    PlaylistItem *p_wxitem = (PlaylistItem *)treectrl->GetItemData( 
		                                   event.GetItem() );
    wxTreeItemId parent = treectrl->GetItemParent( event.GetItem() );
    if( parent.IsOk() )
    {
        fprintf(stderr,"Ca gère\n" );
    }
    else
    {
        fprintf(stderr,"Ca craint\n" );
    }
    PlaylistItem *p_wxparent = (PlaylistItem *)treectrl->GetItemData( parent );

    if( p_playlist == NULL )
    {
        return;
    }

    if( p_wxitem->p_item->i_children == -1 )
    {
        p_node = p_wxparent->p_item;
        p_item = p_wxitem->p_item;
    }
    else
    {
        p_node = p_wxitem->p_item;
        p_item = NULL;
    }

    playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, i_current_view,
                      p_node, p_item );

    vlc_object_release( p_playlist );
}

void Playlist::OnKeyDown( wxTreeEvent& event )
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
}

void Playlist::OnInfos( wxCommandEvent& WXUNUSED(event) )
{
    /* We use the first selected item, so find it */
#if 0
    long i_item = listview->GetNextItem( -1, wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED );
    ShowInfos( i_item );
#endif
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
#if 0
    long i_item = listview->GetNextItem( -1, wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED );

    if( i_item >= 0 && i_item < p_playlist->i_size )
    {
       Rebuild();
    }
#endif
    vlc_object_release( p_playlist );
}

/**********************************************************************
 * Menu
 **********************************************************************/

void Playlist::OnMenuOpen( wxMenuEvent& event)
{
#if defined( __WXMSW__ )
#   define GetEventObject GetMenu
#endif

    if( event.GetEventObject() == p_view_menu )
    {
        p_view_menu = ViewMenu();
    }
#if defined( __WXMSW__ )
#   undef GetEventObject
#endif
}

void Playlist::OnMenuEvent( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    if( event.GetId() < FirstView_Event )
    {
        event.Skip();
        return;
    }

    int i_new_view = event.GetId() - FirstView_Event;

    playlist_view_t *p_view = playlist_ViewFind( p_playlist, i_new_view );

    if( p_view != NULL )
    {
        i_current_view = i_new_view;
        playlist_ViewUpdate( p_playlist, i_new_view );
        Rebuild();
        vlc_object_release( p_playlist );
        return;
    }
    else if( i_new_view >= VIEW_FIRST_SORTED && i_new_view <= VIEW_LAST_SORTED )
    {
        playlist_ViewInsert( p_playlist, i_new_view, "View" );
        playlist_ViewUpdate( p_playlist, i_new_view );

        i_current_view = i_new_view;

        Rebuild();
    }

    vlc_object_release( p_playlist );
}

wxMenu * Playlist::ViewMenu()
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return NULL;
    }

    if( !p_view_menu )
    {
        p_view_menu = new wxMenu;
    }
    else
    {
        wxMenuItemList::Node *node = p_view_menu->GetMenuItems().GetFirst();
        for( ; node; )
        {
            wxMenuItem *item = node->GetData();
            node = node->GetNext();
            p_view_menu->Delete( item );
        }
    }

    /* FIXME : have a list of "should have" views */
    p_view_menu->Append( FirstView_Event + VIEW_CATEGORY,
                           wxU(_("By category") ) );
    p_view_menu->Append( FirstView_Event + VIEW_SIMPLE,
                           wxU(_("Manually added") ) );
    p_view_menu->Append( FirstView_Event + VIEW_ALL,
                           wxU(_("All items, unsorted") ) );
    p_view_menu->Append( FirstView_Event + VIEW_S_AUTHOR,
                           wxU(_("Sorted by author") ) );
#if 0
    for( int i = 0; i< p_playlist->i_views; i++ )
    {
        p_view_menu->Append( FirstView_Event + p_playlist->pp_views[i]->i_id,
                             wxU( p_playlist->pp_views[i]->psz_name ) );
    }
#endif

    vlc_object_release( p_playlist);

    return p_view_menu;
}


/*****************************************************************************
 * Popup management functions
 *****************************************************************************/
void Playlist::OnPopup( wxContextMenuEvent& event )
{
    wxPoint pt = event.GetPosition();

    i_popup_item = treectrl->HitTest( ScreenToClient( pt ) );
    if( i_popup_item.IsOk() )
    {
        PlaylistItem *p_wxitem = (PlaylistItem *)treectrl->GetItemData(
                                                            i_popup_item );
        PlaylistItem *p_wxparent= (PlaylistItem *) treectrl->GetItemData(
                                   treectrl->GetItemParent( i_popup_item ) );
        p_popup_item = p_wxitem->p_item;
        p_popup_parent = p_wxparent->p_item;
        treectrl->SelectItem( i_popup_item );
        Playlist::PopupMenu( popup_menu,
                             ScreenToClient( wxGetMousePosition() ) );
    }
    else
    {
    }
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
    if( p_popup_item != NULL )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                          VIEW_SIMPLE, p_popup_parent, p_popup_item );
                                /*FIXME*/
    }
    vlc_object_release( p_playlist );
}

void Playlist::OnPopupDel( wxMenuEvent& event )
{
    PlaylistItem *p_wxitem;

    p_wxitem = (PlaylistItem *)treectrl->GetItemData( i_popup_item );

    if( p_wxitem->p_item->i_children == -1 )
    {
        DeleteItem( p_wxitem->p_item->input.i_id );
    }
    else
    {
        //DeleteNode( p_wxitem->p_item );
    }
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

    p_popup_item->b_enabled = VLC_TRUE - p_popup_item->b_enabled;

    vlc_object_release( p_playlist);
    UpdateItem( i_popup_item );
}

void Playlist::OnPopupInfo( wxMenuEvent& event )
{
    if( p_popup_item )
    {
        iteminfo_dialog = new ItemInfoDialog( p_intf, p_popup_item, this );
        if( iteminfo_dialog->ShowModal() == wxID_OK )
        {
            UpdateItem( i_popup_item );
        }
        delete iteminfo_dialog;
    }
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
