/*****************************************************************************
 * playlist.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "dialogs/playlist.hpp"
#include "dialogs/iteminfo.hpp"

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

#include <vlc_meta.h>
#include "charset.h"

#define HELP_SHUFFLE N_( "Shuffle" )
#define HELP_LOOP N_( "Repeat All" )
#define HELP_REPEAT N_( "Repeat One" )

namespace wxvlc {
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

    DeleteSelection_Event,
    Random_Event,
    Loop_Event,
    Repeat_Event,

    PopupPlay_Event,
    PopupPlayThis_Event,
    PopupPreparse_Event,
    PopupSort_Event,
    PopupDel_Event,
    PopupInfo_Event,
    PopupAddNode_Event,

    SearchText_Event,
    Search_Event,

    /* controls */
    Source_Event,
    TreeCtrl_Event,

    Browse_Event,  /* For export playlist */

    /* custom events */
    UpdateItem_Event,
    AppendItem_Event,
    RemoveItem_Event,

    MenuDummy_Event = wxID_HIGHEST + 999,

    FirstView_Event = wxID_HIGHEST + 1000,
    CategoryView_Event, OneLevelView_Event,
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
    EVT_MENU(Close_Event, Playlist::OnMenuClose)
    EVT_MENU(Open_Event, Playlist::OnOpen)
    EVT_MENU(Save_Event, Playlist::OnSave)

    EVT_MENU(SortTitle_Event, Playlist::OnSort)
    EVT_MENU(RSortTitle_Event, Playlist::OnSort)

    EVT_MENU(Randomize_Event, Playlist::OnSort)

    EVT_MENU(DeleteSelection_Event, Playlist::OnDeleteSelection)

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
    EVT_MENU( PopupAddNode_Event, Playlist::OnPopupAddNode)

    /* Source selector */
    EVT_LIST_ITEM_SELECTED( Source_Event, Playlist::OnSourceSelected )
    /* Tree control events */
    EVT_TREE_ITEM_ACTIVATED( TreeCtrl_Event, Playlist::OnActivateItem )
    EVT_TREE_KEY_DOWN( -1, Playlist::OnKeyDown )
    EVT_TREE_BEGIN_DRAG( TreeCtrl_Event, Playlist::OnDragItemBegin )
    EVT_TREE_END_DRAG( TreeCtrl_Event, Playlist::OnDragItemEnd )

    EVT_CONTEXT_MENU( Playlist::OnPopup )

    /* Button events */
    EVT_BUTTON( Search_Event, Playlist::OnSearch)
    EVT_BUTTON( Save_Event, Playlist::OnSave)

    /*EVT_TEXT( SearchText_Event, Playlist::OnSearchTextChange )*/
    EVT_TEXT_ENTER( SearchText_Event, Playlist::OnSearch )

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
    PlaylistItem( playlist_item_t *p_item ) : wxTreeItemData()
    {
        i_id = p_item->i_id;
        i_input_id = p_item->p_input->i_id;
    }
protected:
    int i_input_id;
    int i_id;
friend class Playlist;
friend class PlaylistFileDropTarget;
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
    pp_sds = NULL;
    i_update_counter = 0;
    i_sort_mode = MODE_NONE;
    b_need_update = VLC_FALSE;
    i_items_to_append = 0;
    p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    SetIcon( *p_intf->p_sys->p_icon );

    p_view_menu = NULL;
    p_sd_menu = SDMenu();

//    i_current_view = VIEW_ONELEVEL;
    p_current_viewroot = p_playlist->p_root_onelevel;
    p_current_treeroot = p_playlist->p_local_onelevel;

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
    manage_menu->Append( AddMRL_Event, wxU(_("&Add URL...")) );
    manage_menu->AppendSeparator();
    manage_menu->Append( MenuDummy_Event, wxU(_("Services Discovery")),
                         p_sd_menu );
    manage_menu->AppendSeparator();
    manage_menu->Append( Open_Event, wxU(_("&Open Playlist...")) );
    manage_menu->Append( Save_Event, wxU(_("&Save Playlist...")) );
    manage_menu->AppendSeparator();
    manage_menu->Append( Close_Event, wxU(_("&Close")) );

    /* Create our "Sort" menu */
    wxMenu *sort_menu = new wxMenu;
    sort_menu->Append( SortTitle_Event, wxU(_("Sort by &Title")) );
    sort_menu->Append( RSortTitle_Event, wxU(_("&Reverse Sort by Title")) );
    sort_menu->AppendSeparator();
    sort_menu->Append( Randomize_Event, wxU(_("&Shuffle")) );

    /* Create our "Selection" menu */
    wxMenu *selection_menu = new wxMenu;
    selection_menu->Append( DeleteSelection_Event, wxU(_("D&elete")) );

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
    node_popup->Append( PopupPlayThis_Event, wxU(_("Play this Branch")) );
    node_popup->Append( PopupPreparse_Event, wxU(_("Preparse")) );
    node_popup->Append( PopupSort_Event, wxU(_("Sort this Branch")) );
    node_popup->Append( PopupDel_Event, wxU(_("Delete")) );
    node_popup->Append( PopupInfo_Event, wxU(_("Info")) );
    node_popup->Append( PopupAddNode_Event, wxU(_("Add Node")) );

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

    /* Create teh source selector */
    source_sel = new wxListCtrl( playlist_panel, Source_Event,
                                wxDefaultPosition, wxDefaultSize,
                                wxLC_AUTOARRANGE|wxLC_SINGLE_SEL );

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

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxHORIZONTAL );
    panel_sizer->Add( source_sel, 0, wxALL | wxEXPAND, 5 );
    panel_sizer->Add( treectrl, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Layout();

    playlist_panel->SetSizerAndFit( panel_sizer );

    int pi_widths[1] =  { -1 };
    statusbar = CreateStatusBar( 1 );
    statusbar->SetStatusWidths( 1, pi_widths );

#if wxUSE_DRAG_AND_DROP
    /* Associate drop targets with the playlist */
    SetDropTarget( new PlaylistFileDropTarget( this ) );
    menubar->SetDropTarget( new PlaylistFileDropTarget( this ) );
    toolbar->SetDropTarget( new PlaylistFileDropTarget( this ) );
#endif

    i_saved_id = -1;
    i_saved_input_id = -1;

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

}

Playlist::~Playlist()
{
    if( pp_sds != NULL ) free( pp_sds );

    if( p_playlist == NULL ) return;

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
void Playlist::UpdateNode( playlist_item_t *p_node, wxTreeItemId node )
{
    wxTreeItemIdValue cookie;
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
            CreateNode( p_node->pp_children[i], node );
            /* Keep the tree pointer up to date */
            child = treectrl->GetNextChild( node, cookie );
        }
    }
    treectrl->SetItemImage( node, p_node->p_input->i_type );

}

/* Creates the node p_node as last child of parent */
void Playlist::CreateNode( playlist_item_t *p_node, wxTreeItemId parent )
{
    wxTreeItemId node =
        treectrl->AppendItem( parent, wxL2U( p_node->p_input->psz_name ),
                              -1,-1, new PlaylistItem( p_node ) );
    treectrl->SetItemImage( node, p_node->p_input->i_type );

    UpdateNodeChildren( p_node, node );
}

/* Update all children (recursively) of this node */
void Playlist::UpdateNodeChildren( playlist_item_t *p_node,
                                   wxTreeItemId node )
{
    for( int i = 0; i< p_node->i_children ; i++ )
    {
        /* Append the item */
        if( p_node->pp_children[i]->i_children == -1 )
        {
            if( !(p_node->pp_children[i]->i_flags & PLAYLIST_DBL_FLAG) )
            {
                wxTreeItemId item =
                    treectrl->AppendItem( node,
                    wxL2U( p_node->pp_children[i]->p_input->psz_name ), -1,-1,
                           new PlaylistItem( p_node->pp_children[i]) );

                UpdateTreeItem( item );
            }
        }
        else
        {
            CreateNode( p_node->pp_children[i], node );
        }
    }
}

/* Update an item in the tree */
void Playlist::UpdateTreeItem( wxTreeItemId item )
{
    LockPlaylist( p_intf->p_sys, p_playlist );
    if( ! item.IsOk() ) return;

    wxTreeItemData *p_data = treectrl->GetItemData( item );
    if( !p_data ) return;

    playlist_item_t *p_item = playlist_ItemGetById( p_playlist,
                                          ((PlaylistItem *)p_data)->i_id );
    if( !p_item )
    {
        UnlockPlaylist( p_intf->p_sys, p_playlist );
        return;
    }

    wxString msg;
    wxString duration = wxU( "" );

    char *psz_author;
    if( p_item->p_input->p_meta )
    {
        psz_author= p_item->p_input->p_meta->psz_artist ?
                        strdup( p_item->p_input->p_meta->psz_artist ) :
                        strdup("");
    }
    else
        psz_author = strdup( "" );

    char psz_duration[MSTRTIME_MAX_SIZE];
    mtime_t dur = p_item->p_input->i_duration;

    if( dur != -1 )
    {
        secstotimestr( psz_duration, dur/1000000 );
        duration.Append( wxU( " ( " ) +  wxString( wxU( psz_duration ) ) +
                         wxU( " )" ) );
    }

    if( !strcmp( psz_author, "" ) || p_item->p_input->b_fixed_name == VLC_TRUE )
    {
        msg = wxString( wxU( p_item->p_input->psz_name ) ) + duration;
    }
    else
    {
        msg = wxString(wxU( psz_author )) + wxT(" - ") +
              wxString(wxU(p_item->p_input->psz_name)) + duration;
    }
    free( psz_author );
    treectrl->SetItemText( item , msg );
    treectrl->SetItemImage( item, p_item->p_input->i_type );

    if( p_playlist->status.p_item == p_item )
    {
        treectrl->SetItemBold( item, true );
        while( treectrl->GetItemParent( item ).IsOk() )
        {
            item = treectrl->GetItemParent( item );
            if( ! (item == treectrl->GetRootItem() &&
                treectrl->HasFlag( wxTR_HIDE_ROOT ) ) )
                treectrl->Expand( item );
        }
    }
    else
    {
        treectrl->SetItemBold( item, false );
    }
    UnlockPlaylist( p_intf->p_sys, p_playlist );
}

/* Process a AppendIt em request */
void Playlist::AppendItem( wxCommandEvent& event )
{
    playlist_add_t *p_add = (playlist_add_t *)event.GetClientData();
    playlist_item_t *p_item = NULL;
    wxTreeItemId item, node;

    i_items_to_append--;

    /* No need to do anything if the playlist is going to be rebuilt */
    if( b_need_update ) return;

    node = FindItem( treectrl->GetRootItem(), p_add->i_node );
    if( !node.IsOk() ) goto update;

    p_item = playlist_ItemGetById( p_playlist, p_add->i_item );
    if( !p_item ) goto update;
    if( (p_item->i_flags & PLAYLIST_DBL_FLAG ) ) goto update;

    item = FindItem( treectrl->GetRootItem(), p_add->i_item );
    if( item.IsOk() ) goto update;

    item = treectrl->AppendItem( node,
                                 wxL2U( p_item->p_input->psz_name ), -1,-1,
                                 new PlaylistItem( p_item ) );
    treectrl->SetItemImage( item, p_item->p_input->i_type );

    if( item.IsOk() && p_item->i_children == -1 )
    {
        UpdateTreeItem( item );
    }

update:
    int i_count = CountItems( treectrl->GetRootItem());
    statusbar->SetStatusText( wxString::Format( wxU(_(
                                  "%i items in playlist" ) ), i_count ) );
    return;
}

/* Process a updateitem request */
void Playlist::UpdateItem( int i )
{
    if( i < 0 ) return; /* Sanity check */
    wxTreeItemId item = FindItemByInput( treectrl->GetRootItem(), i );
    if( item.IsOk() )
    {
        UpdateTreeItem( item );
    }
}

void Playlist::RemoveItem( int i )
{
    if( i <= 0 ) return; /* Sanity check */
    if( i == i_saved_id ) i_saved_id = -1;

    /* Hack: always invalidate input item cache */
    i_saved_input_id = -1;

    /// \todo Check if it is in the source selector */

    wxTreeItemId item = FindItem( treectrl->GetRootItem(), i );

    if( item.IsOk() )
    {
        treectrl->Delete( item );
    }
}


/**********************************************************************
 * Search functions (internal)
 **********************************************************************/

/* Find a wxItem from a playlist id */
wxTreeItemId Playlist::FindItem( wxTreeItemId root, int i_id )
{
    return FindItemInner( root, i_id, false );
}

wxTreeItemId Playlist::FindItemByInput( wxTreeItemId root, int i_input_id )
{
    return FindItemInner( root, i_input_id, true );
}

wxTreeItemId Playlist::FindItemInner( wxTreeItemId root, int i_id, bool b_byinput )
{
    wxTreeItemIdValue cookie;
    PlaylistItem *p_wxcurrent;
    wxTreeItemId search;
    wxTreeItemId item = treectrl->GetFirstChild( root, cookie );
    wxTreeItemId child;

    p_wxcurrent = (PlaylistItem *)treectrl->GetItemData( root );

    if( i_id < 0 )
    {
        wxTreeItemId dummy; dummy.Unset(); return dummy;
    }
    if( b_byinput && i_saved_input_id == i_id )
        return saved_input_tree_item;
    if( !b_byinput && i_saved_id == i_id)
        return saved_tree_item;

    if( !p_wxcurrent )
    {
        wxTreeItemId dummy; dummy.Unset(); return dummy;
    }

    if( !b_byinput && p_wxcurrent->i_id == i_id  )
    {
        i_saved_id = i_id;
        saved_tree_item = root;
        return root;
    }
    if( b_byinput && p_wxcurrent->i_input_id == i_id )
    {
        i_saved_input_id = i_id;
        saved_input_tree_item = root;
        return root;
    }

    while( item.IsOk() )
    {
        p_wxcurrent = (PlaylistItem *)treectrl->GetItemData( item );
        if( !b_byinput && p_wxcurrent->i_id == i_id )
        {
            i_saved_id = i_id;
            saved_tree_item = item;
            return item;
        }
        else if( b_byinput && p_wxcurrent->i_input_id == i_id )
        {
            i_saved_input_id = i_id;
            saved_input_tree_item = item;
            return item;
        }
        if( treectrl->ItemHasChildren( item ) )
        {
            wxTreeItemId search = FindItemInner( item, i_id, b_byinput );
            if( search.IsOk() )
            {
                if( !b_byinput )
                {
                    i_saved_id = i_id;
                    saved_tree_item = search;
                    return search;
                }
                else
                {
                    i_saved_input_id = i_id;
                    saved_input_tree_item = search;
                    return search;

                }
            }
        }
        item = treectrl->GetNextChild( root, cookie );
    }
    /* Not found */
    wxTreeItemId dummy; dummy.Unset(); return dummy;
}

int Playlist::CountItems( wxTreeItemId root )
{
    wxTreeItemIdValue cookie;
    int count = 0;
    wxTreeItemId item = treectrl->GetFirstChild( root, cookie );

    while( item.IsOk() )
    {
        if( treectrl->ItemHasChildren( item ) )
        {
            count += CountItems( item );
        }
        else
        {
            playlist_item_t *p_item;
            LockPlaylist( p_intf->p_sys, p_playlist );
            p_item = playlist_ItemGetById( p_playlist, ((PlaylistItem *)treectrl->GetItemData( item ))->i_id );
            if( p_item && p_item->i_children == -1 )
                count++;
            UnlockPlaylist( p_intf->p_sys, p_playlist );
        }
        item = treectrl->GetNextChild( root, cookie );
    }
    return count;
}

/* Find a wxItem from a name (from current) */
wxTreeItemId Playlist::FindItemByName( wxTreeItemId root, wxString search_string, wxTreeItemId current, vlc_bool_t *pb_current_found )
{
    wxTreeItemIdValue cookie;
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
    wxTreeItemId dummy; dummy.Unset();
    return dummy;
}

/**********************************************************************
 * Rebuild the playlist
 **********************************************************************/
void Playlist::Rebuild( vlc_bool_t b_root )
{
    i_items_to_append = 0;

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
    /* Invalidate cache */
    i_saved_id = -1;
    i_saved_input_id = -1;

    /* Rebuild the list */
    source_sel->ClearAll();
    for( int i = 0 ; i< p_current_viewroot->i_children ; i++ )
    {
        source_sel->InsertItem( i,
               wxL2U( p_current_viewroot->pp_children[i]->p_input->psz_name) );
        source_sel->SetItemData( i,
                        p_current_viewroot->pp_children[i]->i_id );
        if( p_current_viewroot->pp_children[i] == p_current_treeroot )
            source_sel->Select( i );
    }

    /* HACK we should really get new*/
    treectrl->DeleteAllItems();
    treectrl->AddRoot( wxU(_("root" )), -1, -1,
                         new PlaylistItem( p_current_treeroot ) );

    wxTreeItemId root = treectrl->GetRootItem();
    //UpdateNode( p_current_treeroot, root );
    //CreateNode( p_current_treeroot, root );
    UpdateNodeChildren( p_current_treeroot, root );

    int i_count = CountItems( treectrl->GetRootItem() );

    statusbar->SetStatusText( wxString::Format( wxU(_(
                              "%i items in playlist")), i_count ), 0 );

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
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void Playlist::DeleteTreeItem( wxTreeItemId item )
{
   PlaylistItem *p_wxitem;
   playlist_item_t *p_item;
   p_wxitem = (PlaylistItem *)treectrl->GetItemData( item );

   LockPlaylist( p_intf->p_sys, p_playlist );
   p_item = playlist_ItemGetById( p_playlist, p_wxitem->i_id );

   if( !p_item )
   {
       UnlockPlaylist( p_intf->p_sys, p_playlist );
       return;
   }

   if( p_item->i_children == -1 ) DeleteItem( p_item->p_input->i_id );
   else DeleteNode( p_item );

   RemoveItem( p_item->i_id );
   UnlockPlaylist( p_intf->p_sys, p_playlist );
}

void Playlist::DeleteItem( int item_id )
{
    playlist_DeleteAllFromInput( p_playlist, item_id );
}

void Playlist::DeleteNode( playlist_item_t *p_item )
{
    playlist_NodeDelete( p_playlist, p_item, VLC_TRUE , VLC_FALSE );
}

void Playlist::OnMenuClose( wxCommandEvent& event )
{
    wxCloseEvent cevent;
    OnClose(cevent);
}

void Playlist::OnClose( wxCloseEvent& WXUNUSED(event) )
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
                   { _("XSPF playlist"), "*.xspf", "export-xspf"}
    };

    wxString filter = wxT("");

    if( p_playlist->i_size == 0 )
    {
        wxMessageBox( wxU(_("Playlist is empty") ), wxU(_("Can't save")),
                      wxICON_WARNING | wxOK, this );
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
              abort();
//            playlist_Export( p_playlist, dialog.GetPath().mb_str(),
//                             /* ROOT */
//                             formats[dialog.GetFilterIndex()].psz_module );
        }
    }

}

void Playlist::OnOpen( wxCommandEvent& WXUNUSED(event) )
{
    wxFileDialog dialog( this, wxU(_("Open playlist")), wxT(""), wxT(""),
        wxT("All playlists|*.pls;*.m3u;*.asx;*.b4s;*.xspf|XSPF playlist|*.xspf|M3U files|*.m3u"), wxOPEN );

    if( dialog.ShowModal() == wxID_OK )
    {
        playlist_Import( p_playlist, dialog.GetPath().mb_str() );
    }
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
    PlaylistItem *p_wxitem;
    p_wxitem = (PlaylistItem *)treectrl->GetItemData( treectrl->GetRootItem() );

    LockPlaylist( p_intf->p_sys, p_playlist );
    switch( event.GetId() )
    {
        case SortTitle_Event:
            playlist_RecursiveNodeSort( p_playlist,
                            playlist_ItemGetById( p_playlist, p_wxitem->i_id ),
                            SORT_TITLE_NODES_FIRST, ORDER_NORMAL );
            break;
        case RSortTitle_Event:
            playlist_RecursiveNodeSort( p_playlist,
                            playlist_ItemGetById( p_playlist, p_wxitem->i_id ),
                            SORT_TITLE_NODES_FIRST, ORDER_REVERSE );
    }
    UnlockPlaylist( p_intf->p_sys, p_playlist );

    Rebuild( VLC_TRUE );
}

/**********************************************************************
 * Search functions (user)
 **********************************************************************/
/*void Playlist::OnSearchTextChange( wxCommandEvent& WXUNUSED(event) )
{
   search_button->SetDefault();
}*/

void Playlist::OnSearch( wxCommandEvent& WXUNUSED(event) )
{
    wxString search_string = search_text->GetValue();
    PlaylistItem *p_wxroot;
    p_wxroot = (PlaylistItem *)treectrl->GetItemData( treectrl->GetRootItem() );
    playlist_item_t *p_root = playlist_ItemGetById( p_playlist, p_wxroot->i_id );

    assert( p_root );
    char *psz_name = wxFromLocale( search_string );
    playlist_LiveSearchUpdate( p_playlist, p_root, psz_name );
    Rebuild( VLC_TRUE );

    wxLocaleFree( psz_name );
}

/**********************************************************************
 * Selection functions
 **********************************************************************/
void Playlist::RecursiveDeleteSelection(  wxTreeItemId root )
{
    wxTreeItemIdValue cookie;
    wxTreeItemId child = treectrl->GetFirstChild( root, cookie );
    while( child.IsOk() )
    {
        if( treectrl->ItemHasChildren( child ) )
        {
            RecursiveDeleteSelection( child );
            if( treectrl->IsSelected(child ) ) DeleteTreeItem( child );
        }
        else if( treectrl->IsSelected( child ) )
            DeleteTreeItem( child );
        child = treectrl->GetNextChild( root, cookie );
    }
}

void Playlist::OnDeleteSelection( wxCommandEvent& WXUNUSED(event) )
{
    RecursiveDeleteSelection( treectrl->GetRootItem() );
}

/**********************************************************************
 * Playlist mode functions
 **********************************************************************/
void Playlist::OnRandom( wxCommandEvent& event )
{
    vlc_value_t val;
    val.b_bool = event.IsChecked();
    var_Set( p_playlist, "random", val);
}

void Playlist::OnLoop( wxCommandEvent& event )
{
    vlc_value_t val;
    val.b_bool = event.IsChecked();
    var_Set( p_playlist, "loop", val);
}

void Playlist::OnRepeat( wxCommandEvent& event )
{
    vlc_value_t val;
    val.b_bool = event.IsChecked();
    var_Set( p_playlist, "repeat", val);
}

/********************************************************************
 * Event
 ********************************************************************/
void Playlist::OnActivateItem( wxTreeEvent& event )
{
    playlist_item_t *p_item, *p_parent;

    PlaylistItem *p_wxitem = (PlaylistItem *)treectrl->GetItemData(
                                                            event.GetItem() );

    LockPlaylist( p_intf->p_sys, p_playlist );

    if( !( p_wxitem ) )
    {
        UnlockPlaylist( p_intf->p_sys, p_playlist );
        return;
    }
    p_item = playlist_ItemGetById( p_playlist, p_wxitem->i_id );

    p_parent = p_item;
    while( p_parent )
    {
        if( p_parent == p_current_treeroot )
            break;
        p_parent = p_parent->p_parent;
    }

    if( p_parent )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, 1242,
                          p_parent, p_item );
    }
    UnlockPlaylist( p_intf->p_sys, p_playlist );
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
    /* Work around wxWin32 bug */
    else if( keycode == WXK_RETURN )
    {
        wxArrayTreeItemIds items;
        if( treectrl->GetSelections( items ) > 0 )
        {
            wxTreeEvent event;
            event.SetItem( items.Item( 0 ) );
            OnActivateItem( event );
        }
    }
    else
    {
        event.Skip();
    }
}

void Playlist::OnDragItemBegin( wxTreeEvent& event )
{
    event.Allow();
    draged_tree_item = event.GetItem();
}

void Playlist::OnDragItemEnd( wxTreeEvent& event )
{
    wxTreeItemId dest_tree_item = event.GetItem();

    if( !dest_tree_item.IsOk() ) return;

    /* check that we're not trying to move a node into one of it's children */
    wxTreeItemId parent = dest_tree_item;
    while( parent != treectrl->GetRootItem() )
    {
        if( draged_tree_item == parent ) return;
        parent = treectrl->GetItemParent( parent );
    }

    LockPlaylist( p_intf->p_sys, p_playlist );

    PlaylistItem *p_wxdrageditem =
        (PlaylistItem *)treectrl->GetItemData( draged_tree_item );
    PlaylistItem *p_wxdestitem =
        (PlaylistItem *)treectrl->GetItemData( dest_tree_item );
    if( !p_wxdrageditem || !p_wxdestitem )
    {
        UnlockPlaylist( p_intf->p_sys, p_playlist );
        return;
    }

    playlist_item_t *p_drageditem =
        playlist_ItemGetById(p_playlist, p_wxdrageditem->i_id );
    playlist_item_t *p_destitem =
        playlist_ItemGetById(p_playlist, p_wxdestitem->i_id );
    if( !p_drageditem || !p_destitem )
    {
        UnlockPlaylist( p_intf->p_sys, p_playlist );
        return;
    }

    if( p_destitem->i_children == -1 )
    /* this is a leaf */
    {
        parent = treectrl->GetItemParent( dest_tree_item );
        PlaylistItem *p_parent =
            (PlaylistItem *)treectrl->GetItemData( parent );
        if( !p_parent )
        {
            UnlockPlaylist( p_intf->p_sys, p_playlist );
            return;
        }
        playlist_item_t *p_destitem2 =
            playlist_ItemGetById( p_playlist, p_parent->i_id );
        if( !p_destitem2 )
        {
            UnlockPlaylist( p_intf->p_sys, p_playlist );
            return;
        }
        int i;
        for( i = 0; i < p_destitem2->i_children; i++ )
        {
            if( p_destitem2->pp_children[i] == p_destitem ) break;
        }
        playlist_TreeMove( p_playlist, p_drageditem, p_destitem2, i );
    }
    else
    /* this is a node */
    {
        playlist_TreeMove( p_playlist, p_drageditem, p_destitem, 0 );
    }

    UnlockPlaylist( p_intf->p_sys, p_playlist );

    /* FIXME: having this Rebuild() is dirty */
    Rebuild( VLC_TRUE );
}

#if wxUSE_DRAG_AND_DROP
PlaylistFileDropTarget::PlaylistFileDropTarget( Playlist *p ):p( p ){}

/********************************************************************
 * File Drag And Drop handling
 ********************************************************************/
bool PlaylistFileDropTarget::OnDropFiles( wxCoord x, wxCoord y,
                               const wxArrayString& filenames )
{
    int i_pos = 0;
    playlist_item_t *p_dest;

    LockPlaylist( p->p_intf->p_sys, p->p_playlist );

    /* find the destination node and position in that node */
    const wxPoint pt( x, y );
    wxTreeItemId item = p->treectrl->HitTest( pt );

    if( !item.IsOk() )
    {
        /* We were droped below the last item so we append to the
         * general node */
        msg_Err( p->p_playlist, "USE OF P_GENERAL" );
        p_dest = p->p_playlist->p_local_category;
        i_pos = PLAYLIST_END;
    }
    else
    {
        PlaylistItem *p_plitem =
            (PlaylistItem *)p->treectrl->GetItemData( item );
        p_dest = playlist_ItemGetById( p->p_playlist, p_plitem->i_id );

        if( p_dest->i_children == -1 )
        {
            /* This is a leaf. Append right after it
             * We thus need to find the parrent node and the position of the
             * leaf in it's children list */
            wxTreeItemId parent = p->treectrl->GetItemParent( item );
            PlaylistItem *p_parent =
                (PlaylistItem *)p->treectrl->GetItemData( parent );
            if( !p_parent )
            {
                UnlockPlaylist( p->p_intf->p_sys, p->p_playlist );
                return FALSE;
            }
            playlist_item_t *p_node =
                playlist_ItemGetById( p->p_playlist, p_parent->i_id );
            if( !p_node )
            {
                UnlockPlaylist( p->p_intf->p_sys, p->p_playlist );
                return FALSE;
            }
            for( i_pos = 0; i_pos < p_node->i_children; i_pos++ )
            {
                if( p_node->pp_children[i_pos] == p_dest ) break;
            }
            p_dest = p_node;
        }
    }

    UnlockPlaylist( p->p_intf->p_sys, p->p_playlist );

    /* Put the items in the playlist node */
    for( size_t i = 0; i < filenames.GetCount(); i++ )
    {
        const char *psz_utf8 = wxDnDFromLocale( filenames[i] );
        input_item_t *p_input = input_ItemNew( p->p_playlist,
                                              psz_utf8, psz_utf8 );
        playlist_NodeAddInput( p->p_playlist, p_input,
                               p_dest, PLAYLIST_PREPARSE, i_pos );
        wxDnDLocaleFree( psz_utf8 );
    }

    /* FIXME: having this Rebuild() is dirty */
    p->Rebuild( VLC_TRUE );

    return TRUE;
}
#endif

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
    if( event.GetId() < FirstView_Event )
    {
        event.Skip();
        return;
    }
    else if( event.GetId() < LastView_Event )
    {
        if( event.GetId() == CategoryView_Event )
        {
            p_current_viewroot = p_playlist->p_root_category;
            if( p_current_treeroot == p_playlist->p_local_category ||
                p_current_treeroot == p_playlist->p_local_onelevel )
            {
                p_current_treeroot = p_playlist->p_local_category;
            }
            else if( p_current_treeroot == p_playlist->p_ml_category ||
                     p_current_treeroot == p_playlist->p_ml_onelevel )
            {
                p_current_treeroot = p_playlist->p_ml_category;
            }
        }
        else if( event.GetId() == OneLevelView_Event )
        {
            p_current_viewroot = p_playlist->p_root_onelevel;
            if( p_current_treeroot == p_playlist->p_local_category ||
                p_current_treeroot == p_playlist->p_local_onelevel )
            {
                p_current_treeroot = p_playlist->p_local_onelevel;
            }
            else if( p_current_treeroot == p_playlist->p_ml_category ||
                     p_current_treeroot == p_playlist->p_ml_onelevel )
            {
                p_current_treeroot = p_playlist->p_ml_onelevel;
            }
        }
        wxCommandEvent event;
        OnSearch( event );
        return;
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
            //wxMutexGuiLeave();
            playlist_ServicesDiscoveryRemove( p_playlist,
                            pp_sds[event.GetId() - FirstSD_Event] );
            //wxMutexGuiEnter();
        }
    }
}

wxMenu * Playlist::ViewMenu()
{
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

    p_view_menu->Append( CategoryView_Event, wxU(_("Normal") ) );
    p_view_menu->Append( OneLevelView_Event, wxU(_("One level") ) );
    return p_view_menu;
}

wxMenu *Playlist::SDMenu()
{
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

            /* hack to handle submodules properly */
            int i = -1;
            while( p_parser->pp_shortcuts[++i] != NULL );
            i--;
            if( playlist_IsServicesDiscoveryLoaded( p_playlist,
                                    i>=0?p_parser->pp_shortcuts[i]
                                    :p_parser->psz_object_name ) )
            {
                p_sd_menu->Check( FirstSD_Event + i_number, TRUE );
            }

            pp_sds[i_number++] = i>=0?p_parser->pp_shortcuts[i]
                                 :p_parser->psz_object_name;
        }
    }
    vlc_list_release( p_list );
    return p_sd_menu;
}


/*****************************************************************************
 * Popup management functions
 *****************************************************************************/
void Playlist::OnPopup( wxContextMenuEvent& event )
{
    wxPoint pt = event.GetPosition();
    playlist_item_t *p_item;

    i_wx_popup_item = treectrl->HitTest( ScreenToClient( pt ) );
    if( i_wx_popup_item.IsOk() )
    {
        PlaylistItem *p_wxitem = (PlaylistItem *)treectrl->GetItemData(
                                                            i_wx_popup_item );
        PlaylistItem *p_wxparent= (PlaylistItem *)treectrl->GetItemData(
                                  treectrl->GetItemParent( i_wx_popup_item ) );
        i_popup_item = p_wxitem->i_id;
        i_popup_parent = p_wxparent->i_id;
        treectrl->SelectItem( i_wx_popup_item );

        LockPlaylist( p_intf->p_sys, p_playlist );
        p_item = playlist_ItemGetById( p_playlist, i_popup_item );

        if( !p_item )
        {
            UnlockPlaylist( p_intf->p_sys, p_playlist );
            return;
        }
        if( p_item->i_children == -1 )
        {
            UnlockPlaylist( p_intf->p_sys, p_playlist );
            Playlist::PopupMenu( item_popup,
                                 ScreenToClient( wxGetMousePosition() ) );
        }
        else
        {
            UnlockPlaylist( p_intf->p_sys, p_playlist );
            Playlist::PopupMenu( node_popup,
                                 ScreenToClient( wxGetMousePosition() ) );
        }
    }
}

void Playlist::OnPopupPlay( wxCommandEvent& event )
{
    playlist_item_t *p_popup_item, *p_popup_parent;
    LockPlaylist( p_intf->p_sys, p_playlist );
    p_popup_item = playlist_ItemGetById( p_playlist, i_popup_item );

    p_popup_parent = p_popup_item;
    while( p_popup_parent )
    {
        if( p_popup_parent == p_current_treeroot )
            break;
        p_popup_parent = p_popup_parent->p_parent;
    }

    if( p_popup_parent )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, 1242,
                          p_popup_parent, p_popup_item );
    }
    UnlockPlaylist( p_intf->p_sys, p_playlist );
}

void Playlist::OnPopupPreparse( wxCommandEvent& event )
{
    Preparse();
}

void Playlist::Preparse()
{
    playlist_item_t *p_popup_item;
    LockPlaylist( p_intf->p_sys, p_playlist );
    p_popup_item = playlist_ItemGetById( p_playlist, i_popup_item );

    if( p_popup_item != NULL )
    {
        if( p_popup_item->i_children == -1 )
        {
            playlist_PreparseEnqueue( p_playlist, p_popup_item->p_input );
        }
        else
        {
            int i = 0;
            playlist_item_t *p_parent = p_popup_item;
            for( i = 0; i< p_parent->i_children ; i++ )
            {
                wxMenuEvent dummy;
                i_wx_popup_item = FindItem( treectrl->GetRootItem(),
                                         p_parent->pp_children[i]->i_id );
                i_popup_item = p_parent->pp_children[i]->i_id;
                Preparse();
            }
        }
    }
    UnlockPlaylist( p_intf->p_sys, p_playlist );
}

void Playlist::OnPopupDel( wxCommandEvent& event )
{
    DeleteTreeItem( i_wx_popup_item );
}

void Playlist::OnPopupSort( wxCommandEvent& event )
{
    PlaylistItem *p_wxitem;
    playlist_item_t *p_item;

    p_wxitem = (PlaylistItem *)treectrl->GetItemData( i_wx_popup_item );
    LockPlaylist( p_intf->p_sys, p_playlist );

    p_item = playlist_ItemGetById( p_playlist, p_wxitem->i_id );
    if( p_item->i_children >= 0 )
    {
        playlist_RecursiveNodeSort( p_playlist, p_item,
                                    SORT_TITLE_NODES_FIRST, ORDER_NORMAL );

        treectrl->DeleteChildren( i_wx_popup_item );
        i_saved_id = -1; i_saved_input_id = -1;
        UpdateNodeChildren( p_item, i_wx_popup_item );

    }
    UnlockPlaylist( p_intf->p_sys, p_playlist );
}

void Playlist::OnPopupInfo( wxCommandEvent& event )
{
    LockPlaylist( p_intf->p_sys, p_playlist );
    playlist_item_t *p_popup_item = playlist_ItemGetById( p_playlist, i_popup_item );
    if( p_popup_item )
    {
        iteminfo_dialog = new ItemInfoDialog( p_intf, p_popup_item, this );
        if( iteminfo_dialog->ShowModal() == wxID_OK )
        {
            UpdateItem( i_wx_popup_item );
        }
        delete iteminfo_dialog;
    }
    UnlockPlaylist( p_intf->p_sys, p_playlist );
}

void Playlist::OnPopupAddNode( wxCommandEvent& event )
{
    wxTextEntryDialog text( NULL, wxU(_( "Please enter node name" )),
        wxU(_( "Add node" )), wxU(_( "New node" )) );
    if( text.ShowModal() != wxID_OK ) return;

    char *psz_name = wxFromLocale( text.GetValue() );

    LockPlaylist( p_intf->p_sys, p_playlist );

    PlaylistItem *p_wxitem;
    playlist_item_t *p_item;

    p_wxitem = (PlaylistItem *)treectrl->GetItemData( i_wx_popup_item );

    p_item = playlist_ItemGetById( p_playlist, p_wxitem->i_id );

    playlist_NodeCreate( p_playlist, psz_name, p_item );

    UnlockPlaylist( p_intf->p_sys, p_playlist );
    Rebuild( VLC_TRUE );

    wxLocaleFree( psz_name );
}

void Playlist::OnSourceSelected( wxListEvent &event )
{
   int i_id = event.GetData();

   if( p_current_treeroot && i_id != p_current_treeroot->i_id )
   {
       playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_id );
       if( p_item ) p_current_treeroot = p_item;
       Rebuild( VLC_TRUE );
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
            fprintf( stderr,"Update input item id %i\n", event.GetInt() );
            UpdateItem( event.GetInt() );
            break;
        case AppendItem_Event:
            fprintf( stderr,"Append item id %i\n",
                         ((playlist_add_t*)event.GetClientData())->i_item );
            AppendItem( event );
            break;
        case RemoveItem_Event:
            fprintf( stderr,"Remove item id %i\n", event.GetInt() );
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

    if( ++p_playlist_dialog->i_items_to_append >= 50 )
    {
        /* Too many items waiting to be added, it will be quicker to rebuild
         * the whole playlist */
        p_playlist_dialog->b_need_update = VLC_TRUE;
        return VLC_SUCCESS;
    }

    wxCommandEvent event( wxEVT_PLAYLIST, AppendItem_Event );
    event.SetClientData( (void *)p_add );
    p_playlist_dialog->AddPendingEvent( event );

    return VLC_SUCCESS;
}
}
