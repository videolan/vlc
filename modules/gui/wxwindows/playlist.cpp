/*****************************************************************************
 * playlist.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2005 VideoLAN
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
#include "bitmaps/type_afile.xpm"
#include "bitmaps/type_vfile.xpm"
#include "bitmaps/type_net.xpm"
#include "bitmaps/type_card.xpm"
#include "bitmaps/type_disc.xpm"
#include "bitmaps/type_cdda.xpm"
#include "bitmaps/type_directory.xpm"
#include "bitmaps/type_playlist.xpm"
#include "bitmaps/type_node.xpm"

#include <wx/dynarray.h>
#include <wx/imaglist.h>

#define HELP_SHUFFLE N_( "Shuffle" )
#define HELP_LOOP N_( "Repeat All" )
#define HELP_REPEAT N_( "Repeat One" )

/* Callback prototype */
static int PlaylistChanged( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void * );
static int PlaylistNext( vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void * );
static int ItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int ItemAppended( vlc_object_t *p_this, const char *psz_variable,
                      vlc_value_t oval, vlc_value_t nval, void *param );
static int ItemDeleted( vlc_object_t *p_this, const char *psz_variable,
                      vlc_value_t oval, vlc_value_t nval, void *param );

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    /* menu items */
    AddFile_Event = 1,
    AddDir_Event,
    AddMRL_Event,
    Close_Event,
    Open_Event,
    Save_Event,

    SortTitle_Event,
    RSortTitle_Event,
    Randomize_Event,

    InvertSelection_Event,
    DeleteSelection_Event,
    Random_Event,
    Loop_Event,
    Repeat_Event,
    SelectAll_Event,

    PopupPlay_Event,
    PopupPlayThis_Event,
    PopupPreparse_Event,
    PopupSort_Event,
    PopupDel_Event,
    PopupInfo_Event,

    SearchText_Event,
    Search_Event,

    /* controls */
    TreeCtrl_Event,

    Browse_Event,  /* For export playlist */

    /* custom events */
    UpdateItem_Event,
    AppendItem_Event,
    RemoveItem_Event,

    MenuDummy_Event = wxID_HIGHEST + 999,

    FirstView_Event = wxID_HIGHEST + 1000,
    LastView_Event = wxID_HIGHEST + 1100,

    FirstSD_Event = wxID_HIGHEST + 2000,
    LastSD_Event = wxID_HIGHEST + 2100,
};

DEFINE_LOCAL_EVENT_TYPE( wxEVT_PLAYLIST );

BEGIN_EVENT_TABLE(Playlist, wxFrame)
    EVT_SIZE(Playlist::OnSize)

    /* Menu events */
    EVT_MENU(AddFile_Event, Playlist::OnAddFile)
    EVT_MENU(AddDir_Event, Playlist::OnAddDir)
    EVT_MENU(AddMRL_Event, Playlist::OnAddMRL)
    EVT_MENU(Close_Event, Playlist::OnClose)
    EVT_MENU(Open_Event, Playlist::OnOpen)
    EVT_MENU(Save_Event, Playlist::OnSave)

    EVT_MENU(SortTitle_Event, Playlist::OnSort)
    EVT_MENU(RSortTitle_Event, Playlist::OnSort)

    EVT_MENU(Randomize_Event, Playlist::OnSort)

    EVT_MENU(InvertSelection_Event, Playlist::OnInvertSelection)
    EVT_MENU(DeleteSelection_Event, Playlist::OnDeleteSelection)
    EVT_MENU(SelectAll_Event, Playlist::OnSelectAll)

    EVT_MENU_OPEN( Playlist::OnMenuOpen )
    EVT_MENU( -1, Playlist::OnMenuEvent )

    EVT_TOOL(Random_Event, Playlist::OnRandom)
    EVT_TOOL(Repeat_Event, Playlist::OnRepeat)
    EVT_TOOL(Loop_Event, Playlist::OnLoop)

    /* Popup events */
    EVT_MENU( PopupPlay_Event, Playlist::OnPopupPlay)
    EVT_MENU( PopupPlayThis_Event, Playlist::OnPopupPlay)
    EVT_MENU( PopupPreparse_Event, Playlist::OnPopupPreparse)
    EVT_MENU( PopupSort_Event, Playlist::OnPopupSort)
    EVT_MENU( PopupDel_Event, Playlist::OnPopupDel)
    EVT_MENU( PopupInfo_Event, Playlist::OnPopupInfo)

    /* Tree control events */
    EVT_TREE_ITEM_ACTIVATED( TreeCtrl_Event, Playlist::OnActivateItem )

    EVT_CONTEXT_MENU( Playlist::OnPopup )

    /* Button events */
    EVT_BUTTON( Search_Event, Playlist::OnSearch)
    EVT_BUTTON( Save_Event, Playlist::OnSave)

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
        i_id = p_item->input.i_id;
    }
protected:
    playlist_item_t *p_item;
    int i_id;
friend class Playlist;
};

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Playlist::Playlist( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxFrame( p_parent, -1, wxU(_("Playlist")), wxDefaultPosition,
             wxSize(500,300), wxDEFAULT_FRAME_STYLE )
{
    vlc_value_t val;

    /* Initializations */
    p_intf = _p_intf;
    i_update_counter = 0;
    i_sort_mode = MODE_NONE;
    b_need_update = VLC_FALSE;
    SetIcon( *p_intf->p_sys->p_icon );

    p_view_menu = NULL;
    p_sd_menu = SDMenu();

    i_current_view = VIEW_SIMPLE;
    b_changed_view = VLC_FALSE;

    i_title_sorted = 0;
    i_group_sorted = 0;
    i_duration_sorted = 0;

    var_Create( p_intf, "random", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_intf, "loop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_intf, "repeat", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );;

    /* Create our "Manage" menu */
    wxMenu *manage_menu = new wxMenu;
    manage_menu->Append( AddFile_Event, wxU(_("&Simple Add File...")) );
    manage_menu->Append( AddDir_Event, wxU(_("Add &Directory...")) );
    manage_menu->Append( AddMRL_Event, wxU(_("&Add MRL...")) );
    manage_menu->AppendSeparator();
    manage_menu->Append( MenuDummy_Event, wxU(_("Services discovery")),
                         p_sd_menu );
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
    sort_menu->Append( Randomize_Event, wxU(_("&Shuffle Playlist")) );

    /* Create our "Selection" menu */
    wxMenu *selection_menu = new wxMenu;
    selection_menu->Append( InvertSelection_Event, wxU(_("&Invert")) );
    selection_menu->Append( DeleteSelection_Event, wxU(_("D&elete")) );
    selection_menu->Append( SelectAll_Event, wxU(_("&Select All")) );

    /* Create our "View" menu */
    ViewMenu();

    /* Append the freshly created menus to the menu bar */
    wxMenuBar *menubar = new wxMenuBar();
    menubar->Append( manage_menu, wxU(_("&Manage")) );
    menubar->Append( sort_menu, wxU(_("S&ort")) );
    menubar->Append( selection_menu, wxU(_("&Selection")) );
    menubar->Append( p_view_menu, wxU(_("&View items") ) );

    /* Attach the menu bar to the frame */
    SetMenuBar( menubar );

    /* Create the popup menu */
    node_popup = new wxMenu;
    node_popup->Append( PopupPlay_Event, wxU(_("Play")) );
    node_popup->Append( PopupPlayThis_Event, wxU(_("Play this branch")) );
    node_popup->Append( PopupPreparse_Event, wxU(_("Preparse")) );
    node_popup->Append( PopupSort_Event, wxU(_("Sort this branch")) );
    node_popup->Append( PopupDel_Event, wxU(_("Delete")) );
    node_popup->Append( PopupInfo_Event, wxU(_("Info")) );

    item_popup = new wxMenu;
    item_popup->Append( PopupPlay_Event, wxU(_("Play")) );
    item_popup->Append( PopupPreparse_Event, wxU(_("Preparse")) );
    item_popup->Append( PopupDel_Event, wxU(_("Delete")) );
    item_popup->Append( PopupInfo_Event, wxU(_("Info")) );

    /* Create a panel to put everything in */
    wxPanel *playlist_panel = new wxPanel( this, -1 );
    playlist_panel->SetAutoLayout( TRUE );

    /* Create the toolbar */
    wxToolBar *toolbar =
        CreateToolBar( wxTB_HORIZONTAL | wxTB_FLAT );

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
    wxImageList *p_images = new wxImageList( 16 , 16, TRUE );

    /* FIXME: absolutely needs to be in the right order FIXME */
    p_images->Add( wxIcon( type_unknown_xpm ) );
    p_images->Add( wxIcon( type_afile_xpm ) );
    p_images->Add( wxIcon( type_vfile_xpm ) );
    p_images->Add( wxIcon( type_directory_xpm ) );
    p_images->Add( wxIcon( type_disc_xpm ) );
    p_images->Add( wxIcon( type_cdda_xpm ) );
    p_images->Add( wxIcon( type_card_xpm ) );
    p_images->Add( wxIcon( type_net_xpm ) );
    p_images->Add( wxIcon( type_playlist_xpm ) );
    p_images->Add( wxIcon( type_node_xpm ) );
    treectrl->AssignImageList( p_images );

    treectrl->AddRoot( wxU(_("root" )), -1, -1, NULL );

    /* Reduce font size */
    wxFont font= treectrl->GetFont();
    font.SetPointSize(9);
    treectrl->SetFont( font );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    panel_sizer->Add( treectrl, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Layout();

    playlist_panel->SetSizerAndFit( panel_sizer );

    int pi_widths[1] =  { -1 };
    statusbar = CreateStatusBar( 1 );
    statusbar->SetStatusWidths( 1, pi_widths );

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

    p_saved_item = NULL;


    /* We want to be noticed of playlist changes */

    /* Some global changes happened -> Rebuild all */
    var_AddCallback( p_playlist, "intf-change", PlaylistChanged, this );

    /* We went to the next item */
    var_AddCallback( p_playlist, "playlist-current", PlaylistNext, this );

    /* One item has been updated */
    var_AddCallback( p_playlist, "item-change", ItemChanged, this );

    var_AddCallback( p_playlist, "item-append", ItemAppended, this );
    var_AddCallback( p_playlist, "item-deleted", ItemDeleted, this );

    /* Update the playlist */
    Rebuild( VLC_TRUE );

    vlc_object_release( p_playlist );
}

Playlist::~Playlist()
{
    if( pp_sds != NULL )
        free( pp_sds );

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
    var_DelCallback( p_playlist, "item-append", ItemAppended, this );
    var_DelCallback( p_playlist, "item-deleted", ItemDeleted, this );
    vlc_object_release( p_playlist );
}

/**********************************************************************
 * Update functions
 **********************************************************************/

/* Update a node */
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
    treectrl->SetItemImage( node, p_node->input.i_type );

}

/* Creates the node p_node as last child of parent */
void Playlist::CreateNode( playlist_t *p_playlist, playlist_item_t *p_node,
                           wxTreeItemId parent )
{
    wxTreeItemId node =
        treectrl->AppendItem( parent, wxL2U( p_node->input.psz_name ),
                              -1,-1, new PlaylistItem( p_node ) );
    treectrl->SetItemImage( node, p_node->input.i_type );

    UpdateNodeChildren( p_playlist, p_node, node );
}

/* Update all children (recursively) of this node */
void Playlist::UpdateNodeChildren( playlist_t *p_playlist,
                                   playlist_item_t *p_node,
                                   wxTreeItemId node )
{

    for( int i = 0; i< p_node->i_children ; i++ )
    {
        /* Append the item */
        if( p_node->pp_children[i]->i_children == -1 )
        {
            wxTreeItemId item =
                treectrl->AppendItem( node,
                    wxL2U( p_node->pp_children[i]->input.psz_name ), -1,-1,
                           new PlaylistItem( p_node->pp_children[i]) );

            UpdateTreeItem( p_playlist, item );
        }
        else
        {
            CreateNode( p_playlist, p_node->pp_children[i],
                        node );
        }
    }
}

/* Update an item in the tree */
void Playlist::UpdateTreeItem( playlist_t *p_playlist, wxTreeItemId item )
{
    if( ! item.IsOk() ) return;

    wxTreeItemData *p_data = treectrl->GetItemData( item );
    if( !p_data ) return;

    playlist_item_t *p_item = ((PlaylistItem *)p_data)->p_item;
    if( !p_item ) return;

    wxString msg;
    wxString duration = wxU( "" );
    char *psz_author = vlc_input_item_GetInfo( &p_item->input,
                                                     _("Meta-information"),
                                                     _("Artist"));
    if( psz_author == NULL )
        return;
    char psz_duration[MSTRTIME_MAX_SIZE];
    mtime_t dur = p_item->input.i_duration;

    if( dur != -1 )
    {
        secstotimestr( psz_duration, dur/1000000 );
        duration.Append( wxU( " ( " ) +  wxString( wxU( psz_duration ) ) +
                         wxU( " )" ) );
    }

    if( !strcmp( psz_author, "" ) || p_item->input.b_fixed_name == VLC_TRUE )
    {
        msg.Printf( wxString( wxU( p_item->input.psz_name ) ) + duration );
    }
    else
    {
        msg.Printf( wxString(wxU( psz_author )) + wxT(" - ") +
                    wxString(wxU(p_item->input.psz_name)) + duration );
    }
    free( psz_author );
    treectrl->SetItemText( item , msg );
    treectrl->SetItemImage( item, p_item->input.i_type );

    if( p_playlist->status.p_item == p_item )
    {
        treectrl->SetItemBold( item, true );
        treectrl->EnsureVisible( item );
    }
    else
    {
        treectrl->SetItemBold( item, false );
    }
}

/* Process a AppendItem request */
void Playlist::AppendItem( wxCommandEvent& event )
{
    playlist_add_t *p_add = (playlist_add_t *)event.GetClientData();

    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    wxTreeItemId item,node;
    if( p_playlist == NULL )
    {
        event.Skip();
        return;
    }

    if( p_add->i_view != i_current_view )
    {
        goto update;
    }

    node = FindItem( treectrl->GetRootItem(), p_add->p_node );
    if( !node.IsOk() )
    {
        goto update;
    }

    item = treectrl->AppendItem( node,
                                 wxL2U( p_add->p_item->input.psz_name ), -1,-1,
                                 new PlaylistItem( p_add->p_item ) );
    treectrl->SetItemImage( item, p_add->p_item->input.i_type );

    if( item.IsOk() && p_add->p_item->i_children == -1 )
    {
        UpdateTreeItem( p_playlist, item );
    }

update:
    int i_count = CountItems( treectrl->GetRootItem());
    if( i_count != p_playlist->i_size )
    {
        statusbar->SetStatusText( wxString::Format( wxU(_(
                                  "%i items in playlist (%i not shown)")),
                                  p_playlist->i_size,
                                  p_playlist->i_size - i_count ) );
        if( !b_changed_view )
        {
            i_current_view = VIEW_CATEGORY;
            b_changed_view = VLC_TRUE;
            b_need_update = VLC_TRUE;
        }
    }
    else
    {
        statusbar->SetStatusText( wxString::Format( wxU(_(
                                  "%i items in playlist")),
                                  p_playlist->i_size ), 0 );
    }

    vlc_object_release( p_playlist );
    return;
}

/* Process a updateitem request */
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

    p_item = playlist_LockItemGetById( p_playlist, i );

    wxTreeItemId item = FindItem( treectrl->GetRootItem(), p_item);

    if( item.IsOk() )
    {
        UpdateTreeItem( p_playlist, item );
    }

    vlc_object_release(p_playlist);
}

void Playlist::RemoveItem( int i )
{
    if( i <= 0 ) return; /* Sanity check */

    wxTreeItemId item = FindItem( treectrl->GetRootItem(), i );

    if( item.IsOk() )
    {
        treectrl->Delete( item );
    }
}


/**********************************************************************
 * Search functions (internal)
 **********************************************************************/

/* Find a wxItem from a playlist_item */
wxTreeItemId Playlist::FindItem( wxTreeItemId root, playlist_item_t *p_item )
{
    long cookie;
    PlaylistItem *p_wxcurrent;
    wxTreeItemId search;
    wxTreeItemId item = treectrl->GetFirstChild( root, cookie );
    wxTreeItemId child;

    if( p_item == p_saved_item && saved_tree_item.IsOk() )
    {
        return saved_tree_item;
    }

    p_wxcurrent = (PlaylistItem *)treectrl->GetItemData( root );

    if( !p_item || !p_wxcurrent )
    {
        wxTreeItemId dummy;
        return dummy;
    }

    if( p_wxcurrent->p_item == p_item )
    {
        return root;
    }

    while( item.IsOk() )
    {
        p_wxcurrent = (PlaylistItem *)treectrl->GetItemData( item );
        if( p_wxcurrent->p_item == p_item )
        {
            saved_tree_item = item;
            p_saved_item = p_item;
            return item;
        }
        if( treectrl->ItemHasChildren( item ) )
        {
            wxTreeItemId search = FindItem( item, p_item );
            if( search.IsOk() )
            {
                saved_tree_item = search;
                p_saved_item = p_item;
                return search;
            }
        }
        item = treectrl->GetNextChild( root, cookie );
    }
    /* Not found */
    wxTreeItemId dummy;
    return dummy;
}
/* Find a wxItem from a playlist id */
wxTreeItemId Playlist::FindItem( wxTreeItemId root, int i_id )
{
    long cookie;
    PlaylistItem *p_wxcurrent;
    wxTreeItemId search;
    wxTreeItemId item = treectrl->GetFirstChild( root, cookie );
    wxTreeItemId child;

    p_wxcurrent = (PlaylistItem *)treectrl->GetItemData( root );

    if( i_id < 0 )
    {
        wxTreeItemId dummy;
        return dummy;
    }

    if( !p_wxcurrent )
    {
        wxTreeItemId dummy;
        return dummy;
    }        

    if( p_wxcurrent->i_id == i_id )
    {
        return root;
    }

    while( item.IsOk() )
    {
        p_wxcurrent = (PlaylistItem *)treectrl->GetItemData( item );
        if( p_wxcurrent->i_id == i_id )
        {
            return item;
        }
        if( treectrl->ItemHasChildren( item ) )
        {
            wxTreeItemId search = FindItem( item, i_id );
            if( search.IsOk() )
            {
                return search;
            }
        }
        item = treectrl->GetNextChild( root, cookie );
    }
    /* Not found */
    wxTreeItemId dummy;
    return dummy;
}

int Playlist::CountItems( wxTreeItemId root )
{
    long cookie;
    int count = 0;
    wxTreeItemId item = treectrl->GetFirstChild( root, cookie );

    while( item.IsOk() )
    {
        if( treectrl->ItemHasChildren( item ) )
        {
            count += CountItems( item );
        }
        else if( ( (PlaylistItem *)treectrl->GetItemData( item ) )->
                            p_item->i_children == -1 )
            count++;
        item = treectrl->GetNextChild( root, cookie );
    }
    return count;
}

/* Find a wxItem from a name (from current) */
wxTreeItemId Playlist::FindItemByName( wxTreeItemId root, wxString search_string, wxTreeItemId current, vlc_bool_t *pb_current_found )
{
    long cookie;
    wxTreeItemId search;
    wxTreeItemId item = treectrl->GetFirstChild( root, cookie );
    wxTreeItemId child;

    while( item.IsOk() )
    {
        if( treectrl->GetItemText( item).Lower().Contains(
                                                 search_string.Lower() ) )
        {
            if( !current.IsOk() || *pb_current_found == VLC_TRUE )
            {
                return item;
            }
            else if( current.IsOk() && item == current )
            {
                *pb_current_found = VLC_TRUE;
            }
        }
        if( treectrl->ItemHasChildren( item ) )
        {
            wxTreeItemId search = FindItemByName( item, search_string, current,
                                                  pb_current_found );
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

/**********************************************************************
 * Rebuild the playlist
 **********************************************************************/
void Playlist::Rebuild( vlc_bool_t b_root )
{
    playlist_view_t *p_view;
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* We can remove the callbacks before locking, anyway, we won't
     * miss anything */
    if( b_root )
    {
        var_DelCallback( p_playlist, "item-change", ItemChanged, this );
        var_DelCallback( p_playlist, "playlist-current", PlaylistNext, this );
        var_DelCallback( p_playlist, "intf-change", PlaylistChanged, this );
        var_DelCallback( p_playlist, "item-append", ItemAppended, this );
        var_DelCallback( p_playlist, "item-deleted", ItemDeleted, this );

        /* ...and rebuild it */
        LockPlaylist( p_intf->p_sys, p_playlist );
    }

    p_view = playlist_ViewFind( p_playlist, i_current_view ); /* FIXME */

    /* HACK we should really get new*/
    treectrl->DeleteAllItems();
    treectrl->AddRoot( wxU(_("root" )), -1, -1,
                         new PlaylistItem( p_view->p_root) );

    wxTreeItemId root = treectrl->GetRootItem();
    UpdateNode( p_playlist, p_view->p_root, root );

    int i_count = CountItems( treectrl->GetRootItem() );

    if( i_count < p_playlist->i_size && !b_changed_view )
    {
        i_current_view = VIEW_CATEGORY;
        b_changed_view = VLC_TRUE;
        Rebuild( VLC_FALSE );
    }
    else if( i_count != p_playlist->i_size )
    {
        statusbar->SetStatusText( wxString::Format( wxU(_(
                                  "%i items in playlist (%i not shown)")),
                                  p_playlist->i_size,
                                  p_playlist->i_size - i_count ) );
    }
    else
    {
        statusbar->SetStatusText( wxString::Format( wxU(_(
                                  "%i items in playlist")),
                                  p_playlist->i_size ), 0 );
    }

    if( b_root )
    {
        /* Put callbacks back online */
        var_AddCallback( p_playlist, "intf-change", PlaylistChanged, this );
        var_AddCallback( p_playlist, "playlist-current", PlaylistNext, this );
        var_AddCallback( p_playlist, "item-change", ItemChanged, this );
        var_AddCallback( p_playlist, "item-append", ItemAppended, this );
        var_AddCallback( p_playlist, "item-deleted", ItemDeleted, this );

        UnlockPlaylist( p_intf->p_sys, p_playlist );
    }
    vlc_object_release( p_playlist );
}



void Playlist::ShowPlaylist( bool show )
{
    if( show ) Rebuild( VLC_TRUE );
    Show( show );
}

/* This function is called on a regular basis */
void Playlist::UpdatePlaylist()
{
    i_update_counter++;

    /* If the playlist isn't show there's no need to update it */
    if( !IsShown() ) return;

    if( this->b_need_update )
    {
        this->b_need_update = VLC_FALSE;
        Rebuild( VLC_TRUE );
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

    playlist_LockDelete( p_playlist, item_id );

    vlc_object_release( p_playlist );
}

void Playlist::DeleteNode( playlist_item_t *p_item )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_NodeDelete( p_playlist, p_item, VLC_TRUE , VLC_FALSE );

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

void Playlist::OnAddDir( wxCommandEvent& WXUNUSED(event) )
{
    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_DIRECTORY, 0, 0 );

}

void Playlist::OnAddMRL( wxCommandEvent& WXUNUSED(event) )
{
    p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_FILE, 0, 0 );

}

/********************************************************************
 * Sorting functions
 ********************************************************************/
void Playlist::OnSort( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    PlaylistItem *p_wxitem;
    p_wxitem = (PlaylistItem *)treectrl->GetItemData( treectrl->GetRootItem() );

    if( p_playlist == NULL )
    {
        return;
    }
    LockPlaylist( p_intf->p_sys, p_playlist );
    switch( event.GetId() )
    {
        case SortTitle_Event:
            playlist_RecursiveNodeSort( p_playlist, p_wxitem->p_item,
                                        SORT_TITLE_NODES_FIRST, ORDER_NORMAL );
            break;
        case RSortTitle_Event:
            playlist_RecursiveNodeSort( p_playlist, p_wxitem->p_item,
                                        SORT_TITLE_NODES_FIRST, ORDER_REVERSE );
    }
    UnlockPlaylist( p_intf->p_sys, p_playlist );

    vlc_object_release( p_playlist );
    Rebuild( VLC_TRUE );
}

/**********************************************************************
 * Search functions (user)
 **********************************************************************/
void Playlist::OnSearchTextChange( wxCommandEvent& WXUNUSED(event) )
{
   search_button->SetDefault();
}

void Playlist::OnSearch( wxCommandEvent& WXUNUSED(event) )
{
    wxString search_string = search_text->GetValue();

    vlc_bool_t pb_found = VLC_FALSE;

    wxTreeItemId found =
     FindItemByName( treectrl->GetRootItem(), search_string,
                     search_current, &pb_found );

    if( found.IsOk() )
    {
        search_current = found;
        treectrl->SelectItem( found, true );
    }
    else
    {
        wxTreeItemId dummy;
        search_current = dummy;
        found =  FindItemByName( treectrl->GetRootItem(), search_string,
                                 search_current, &pb_found );
        if( found.IsOk() )
        {
            search_current = found;
            treectrl->SelectItem( found, true );
        }
    }
}

/**********************************************************************
 * Selection functions
 **********************************************************************/
void Playlist::OnInvertSelection( wxCommandEvent& WXUNUSED(event) )
{
}

void Playlist::OnDeleteSelection( wxCommandEvent& WXUNUSED(event) )
{
    Rebuild( VLC_TRUE );
}

void Playlist::OnSelectAll( wxCommandEvent& WXUNUSED(event) )
{
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

/********************************************************************
 * Event
 ********************************************************************/
void Playlist::OnActivateItem( wxTreeEvent& event )
{
    playlist_item_t *p_item,*p_node;
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                    VLC_OBJECT_PLAYLIST,FIND_ANYWHERE );

    PlaylistItem *p_wxitem = (PlaylistItem *)treectrl->GetItemData(
                                                            event.GetItem() );
    wxTreeItemId parent = treectrl->GetItemParent( event.GetItem() );

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
        if( p_wxitem->p_item->i_children > 0 &&
            p_wxitem->p_item->pp_children[0]->i_children == -1)
        {
            p_item = p_wxitem->p_item->pp_children[0];
        }
        else
        {
            p_item = NULL;
        }
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

void Playlist::OnEnDis( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
    msg_Warn( p_intf, "not implemented" );
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
        vlc_object_release( p_playlist );
        return;
    }
    else if( event.GetId() < LastView_Event )
    {

        int i_new_view = event.GetId() - FirstView_Event;

        playlist_view_t *p_view = playlist_ViewFind( p_playlist, i_new_view );

        if( p_view != NULL )
        {
            b_changed_view = VLC_TRUE;
            i_current_view = i_new_view;
            playlist_ViewUpdate( p_playlist, i_new_view );
            Rebuild( VLC_TRUE );
            vlc_object_release( p_playlist );
            return;
        }
        else if( i_new_view >= VIEW_FIRST_SORTED &&
                 i_new_view <= VIEW_LAST_SORTED )
        {
            b_changed_view = VLC_TRUE;
            playlist_ViewInsert( p_playlist, i_new_view, "View" );
            playlist_ViewUpdate( p_playlist, i_new_view );

            i_current_view = i_new_view;

            Rebuild( VLC_TRUE );
        }
    }
    else if( event.GetId() >= FirstSD_Event && event.GetId() < LastSD_Event )
    {
        if( !playlist_IsServicesDiscoveryLoaded( p_playlist,
                                pp_sds[event.GetId() - FirstSD_Event] ) )
        {
            playlist_ServicesDiscoveryAdd( p_playlist,
                            pp_sds[event.GetId() - FirstSD_Event] );
        }
        else
        {
            wxMutexGuiLeave();
            playlist_ServicesDiscoveryRemove( p_playlist,
                            pp_sds[event.GetId() - FirstSD_Event] );
            wxMutexGuiEnter();
        }
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
                           wxU(_("Normal") ) );
/*    p_view_menu->Append( FirstView_Event + VIEW_SIMPLE,
                           wxU(_("Manually added") ) );
    p_view_menu->Append( FirstView_Event + VIEW_ALL,
                           wxU(_("All items, unsorted") ) ); */
    p_view_menu->Append( FirstView_Event + VIEW_S_AUTHOR,
                           wxU(_("Sorted by artist") ) );

    vlc_object_release( p_playlist);

    return p_view_menu;
}

wxMenu *Playlist::SDMenu()
{

    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                              VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist )
    {
        return NULL;
    }
    p_sd_menu = new wxMenu;

    vlc_list_t *p_list = vlc_list_find( p_playlist, VLC_OBJECT_MODULE,
                                        FIND_ANYWHERE );

    int i_number = 0;
    for( int i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        module_t * p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_capability, "services_discovery" ) )
            i_number++;
    }
    if( i_number ) pp_sds = (char **)calloc( i_number, sizeof(void *) );

    i_number = 0;
    for( int i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        module_t * p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_capability, "services_discovery" ) )
        {
            p_sd_menu->AppendCheckItem( FirstSD_Event + i_number ,
                wxU( p_parser->psz_longname ? p_parser->psz_longname :
                     (p_parser->psz_shortname ?
                      p_parser->psz_shortname : p_parser->psz_object_name) ) );

            if( playlist_IsServicesDiscoveryLoaded( p_playlist,
                                    p_parser->psz_object_name ) )
            {
                p_sd_menu->Check( FirstSD_Event + i_number, TRUE );
            }

            pp_sds[i_number++] = p_parser->psz_object_name;
        }
    }
    vlc_list_release( p_list );
    vlc_object_release( p_playlist );
    return p_sd_menu;
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
        if( p_popup_item->i_children == -1 )
            Playlist::PopupMenu( item_popup,
                                 ScreenToClient( wxGetMousePosition() ) );
        else
            Playlist::PopupMenu( node_popup,
                                 ScreenToClient( wxGetMousePosition() ) );
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
        if( p_popup_item->i_children > -1 )
        {
            if( event.GetId() == PopupPlay_Event &&
                p_popup_item->i_children > 0 )
            {
                playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                                  i_current_view, p_popup_item,
                                  p_popup_item->pp_children[0] );
            }
            else
            {
                playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                                  i_current_view, p_popup_item, NULL );
            }
        }
        else
        {
            if( event.GetId() == PopupPlay_Event )
            {
                playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                                  i_current_view, p_popup_parent,
                                  p_popup_item );
            }
        }
    }
    vlc_object_release( p_playlist );
}

void Playlist::OnPopupPreparse( wxMenuEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
    Preparse( p_playlist );
    vlc_object_release( p_playlist );
}

void Playlist::Preparse( playlist_t *p_playlist )
{
    if( p_popup_item != NULL )
    {
        if( p_popup_item->i_children == -1 )
        {
            playlist_PreparseEnqueue( p_playlist, &p_popup_item->input );
        }
        else
        {
            int i = 0;
            playlist_item_t *p_parent = p_popup_item;
            for( i = 0; i< p_parent->i_children ; i++ )
            {
                wxMenuEvent dummy;
                i_popup_item = FindItem( treectrl->GetRootItem(),
                                         p_parent->pp_children[i] );
                p_popup_item = p_parent->pp_children[i];
                Preparse( p_playlist );
            }
        }
    }
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
        DeleteNode( p_wxitem->p_item );
    }
}

void Playlist::OnPopupSort( wxMenuEvent& event )
{
    PlaylistItem *p_wxitem;

    p_wxitem = (PlaylistItem *)treectrl->GetItemData( i_popup_item );

    if( p_wxitem->p_item->i_children >= 0 )
    {
        playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

        if( p_playlist )
        {
            LockPlaylist( p_intf->p_sys, p_playlist );
            playlist_RecursiveNodeSort( p_playlist, p_wxitem->p_item,
                                        SORT_TITLE_NODES_FIRST, ORDER_NORMAL );
            UnlockPlaylist( p_intf->p_sys, p_playlist );

            treectrl->DeleteChildren( i_popup_item );
            UpdateNodeChildren( p_playlist, p_wxitem->p_item, i_popup_item );

            vlc_object_release( p_playlist );
        }
    }
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
        case AppendItem_Event:
            AppendItem( event );
            break;
        case RemoveItem_Event:
            RemoveItem( event.GetInt() );
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
static int ItemDeleted( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    Playlist *p_playlist_dialog = (Playlist *)param;

    wxCommandEvent event( wxEVT_PLAYLIST, RemoveItem_Event );
    event.SetInt( new_val.i_int );
    p_playlist_dialog->AddPendingEvent( event );

    return 0;
}

static int ItemAppended( vlc_object_t *p_this, const char *psz_variable,
                         vlc_value_t oval, vlc_value_t nval, void *param )
{
    Playlist *p_playlist_dialog = (Playlist *)param;

    playlist_add_t *p_add = (playlist_add_t *)malloc(sizeof( playlist_add_t));

    memcpy( p_add, nval.p_address, sizeof( playlist_add_t ) );

    wxCommandEvent event( wxEVT_PLAYLIST, AppendItem_Event );
    event.SetClientData( (void *)p_add );
    p_playlist_dialog->AddPendingEvent( event );

    return VLC_SUCCESS;
}
