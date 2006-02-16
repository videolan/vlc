/*****************************************************************************
 * playlist_manager.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Clément Stenac <zorglub@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include "playlist_manager.hpp"
#include "interface.hpp"

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
#include "vlc_meta.h"

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
    TreeCtrl_Event,

    UpdateItem_Event,
    AppendItem_Event,
    RemoveItem_Event,
};

DEFINE_LOCAL_EVENT_TYPE( wxEVT_PLAYLIST );

BEGIN_EVENT_TABLE(PlaylistManager, wxPanel)
    /* Tree control events */
    EVT_TREE_ITEM_ACTIVATED( TreeCtrl_Event, PlaylistManager::OnActivateItem )

    /* Custom events */
    EVT_COMMAND(-1, wxEVT_PLAYLIST, PlaylistManager::OnPlaylistEvent)
END_EVENT_TABLE()

/*****************************************************************************
 * PlaylistItem class
 ****************************************************************************/
class PlaylistItem : public wxTreeItemData
{
public:
    PlaylistItem( playlist_item_t *p_item ) : i_id(p_item->input.i_id) {}
    int i_id;
};

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
PlaylistManager::PlaylistManager( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxPanel( p_parent, -1, wxDefaultPosition, wxSize(0,0) )
{
    /* Initializations */
    p_intf = _p_intf;
    b_need_update = VLC_FALSE;
    i_items_to_append = 0;
    i_cached_item_id = -1;
    i_update_counter = 0;

    p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    var_Create( p_intf, "random", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_intf, "loop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_intf, "repeat", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );;

    /* Create the tree */
    treectrl = new wxTreeCtrl( this, TreeCtrl_Event,
                               wxDefaultPosition, wxDefaultSize,
                               wxTR_HIDE_ROOT | wxTR_LINES_AT_ROOT|
                               wxTR_NO_LINES |
                               wxTR_HAS_BUTTONS | wxTR_TWIST_BUTTONS |
                               wxTR_MULTIPLE | wxTR_EXTENDED );

    /* Add everything to the panel */
    sizer = new wxBoxSizer( wxHORIZONTAL );
    SetSizer( sizer );
    sizer->Add( treectrl, 1, wxEXPAND );
    sizer->Layout();
    sizer->Fit( this );

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

    /* Reduce font size */
    wxFont font = treectrl->GetFont(); font.SetPointSize(9);
    treectrl->SetFont( font );

#if wxUSE_DRAG_AND_DROP
    /* Associate drop targets with the playlist */
    SetDropTarget( new DragAndDrop( p_intf, VLC_TRUE ) );
#endif

    /* Update the playlist */
    Rebuild( VLC_TRUE );

    /*
     * We want to be notified of playlist changes
     */

    /* Some global changes happened -> Rebuild all */
    var_AddCallback( p_playlist, "intf-change", PlaylistChanged, this );

    /* We went to the next item */
    var_AddCallback( p_playlist, "playlist-current", PlaylistNext, this );

    /* One item has been updated */
    var_AddCallback( p_playlist, "item-change", ItemChanged, this );

    var_AddCallback( p_playlist, "item-append", ItemAppended, this );
    var_AddCallback( p_playlist, "item-deleted", ItemDeleted, this );
}

PlaylistManager::~PlaylistManager()
{
    if( p_playlist == NULL ) return;

    var_DelCallback( p_playlist, "item-change", ItemChanged, this );
    var_DelCallback( p_playlist, "playlist-current", PlaylistNext, this );
    var_DelCallback( p_playlist, "intf-change", PlaylistChanged, this );
    var_DelCallback( p_playlist, "item-append", ItemAppended, this );
    var_DelCallback( p_playlist, "item-deleted", ItemDeleted, this );
    vlc_object_release( p_playlist );
}

/*****************************************************************************
 * PlaylistChanged: callback triggered by the intf-change playlist variable
 *  We don't rebuild the playlist directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param )
{
    PlaylistManager *p_playlist = (PlaylistManager *)param;
    p_playlist->b_need_update = VLC_TRUE;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Next: callback triggered by the playlist-current playlist variable
 *****************************************************************************/
static int PlaylistNext( vlc_object_t *p_this, const char *psz_variable,
                         vlc_value_t oval, vlc_value_t nval, void *param )
{
    PlaylistManager *p_playlist = (PlaylistManager *)param;

    wxCommandEvent event( wxEVT_PLAYLIST, UpdateItem_Event );
    event.SetInt( oval.i_int );
    p_playlist->AddPendingEvent( event );
    event.SetInt( nval.i_int );
    p_playlist->AddPendingEvent( event );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Update functions
 *****************************************************************************/
void PlaylistManager::CreateNode( playlist_item_t *p_node, wxTreeItemId parent)
{
    wxTreeItemId node =
        treectrl->AppendItem( parent, wxL2U( p_node->input.psz_name ), -1, -1,
                              new PlaylistItem( p_node ) );
    treectrl->SetItemImage( node, p_node->input.i_type );

    UpdateNodeChildren( p_node, node );
}

void PlaylistManager::UpdateNode( playlist_item_t *p_node, wxTreeItemId node )
{
    wxTreeItemIdValue cookie;
    wxTreeItemId child;

    for( int i = 0; i < p_node->i_children ; i++ )
    {
        if( !i ) child = treectrl->GetFirstChild( node, cookie);
        else child = treectrl->GetNextChild( node, cookie );

        if( !child.IsOk() )
        {
            /* Not enough children */
            CreateNode( p_node->pp_children[i], node );
            /* Keep the tree pointer up to date */
            child = treectrl->GetNextChild( node, cookie );
        }
    }

    treectrl->SetItemImage( node, p_node->input.i_type );

}

void PlaylistManager::UpdateNodeChildren( playlist_item_t *p_node,
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

            UpdateTreeItem( item );
        }
        else
        {
            CreateNode( p_node->pp_children[i], node );
        }
    }
}

void PlaylistManager::UpdateTreeItem( wxTreeItemId item )
{
    if( ! item.IsOk() ) return;

    wxTreeItemData *p_data = treectrl->GetItemData( item );
    if( !p_data ) return;

    LockPlaylist( p_intf->p_sys, p_playlist );
    playlist_item_t *p_item =
        playlist_ItemGetById( p_playlist, ((PlaylistItem *)p_data)->i_id );
    if( !p_item )
    {
        UnlockPlaylist( p_intf->p_sys, p_playlist );
        return;
    }

    wxString msg;
    wxString duration = wxU( "" );
    char *psz_author =
        vlc_input_item_GetInfo( &p_item->input,
                                _(VLC_META_INFO_CAT), _(VLC_META_ARTIST) );
    if( !psz_author )
    {
        UnlockPlaylist( p_intf->p_sys, p_playlist );
        return;
    }

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
        msg = wxString( wxU( p_item->input.psz_name ) ) + duration;
    }
    else
    {
        msg = wxString(wxU( psz_author )) + wxT(" - ") +
                    wxString(wxU(p_item->input.psz_name)) + duration;
    }
    free( psz_author );
    treectrl->SetItemText( item , msg );
    treectrl->SetItemImage( item, p_item->input.i_type );

    if( p_playlist->status.p_item == p_item )
    {
        treectrl->SetItemBold( item, true );
        while( treectrl->GetItemParent( item ).IsOk() )
        {
            item = treectrl->GetItemParent( item );
            treectrl->Expand( item );
        }
    }
    else
    {
        treectrl->SetItemBold( item, false );
    }
    UnlockPlaylist( p_intf->p_sys, p_playlist );
}

void PlaylistManager::AppendItem( wxCommandEvent& event )
{
    playlist_add_t *p_add = (playlist_add_t *)event.GetClientData();
    playlist_item_t *p_item = NULL;
    wxTreeItemId item, node;

    i_items_to_append--;

    /* No need to do anything if the playlist is going to be rebuilt */
    if( b_need_update ) return;

    //if( p_add->i_view != i_current_view ) goto update;

    node = FindItem( treectrl->GetRootItem(), p_add->i_node );
    if( !node.IsOk() ) goto update;

    p_item = playlist_ItemGetById( p_playlist, p_add->i_item );
    if( !p_item ) goto update;

    item = FindItem( treectrl->GetRootItem(), p_add->i_item );
    if( item.IsOk() ) goto update;

    item = treectrl->AppendItem( node, wxL2U( p_item->input.psz_name ), -1,-1,
                                 new PlaylistItem( p_item ) );
    treectrl->SetItemImage( item, p_item->input.i_type );

    if( item.IsOk() && p_item->i_children == -1 ) UpdateTreeItem( item );

update:
    return;
}

void PlaylistManager::UpdateItem( int i )
{
    if( i < 0 ) return; /* Sanity check */

    wxTreeItemId item = FindItem( treectrl->GetRootItem(), i );
    if( item.IsOk() ) UpdateTreeItem( item );
}

void PlaylistManager::RemoveItem( int i )
{
    if( i <= 0 ) return; /* Sanity check */

    wxTreeItemId item = FindItem( treectrl->GetRootItem(), i );
    if( item.IsOk() )
    {
        treectrl->Delete( item );

        /* Invalidate cache */
        i_cached_item_id = -1;
    }
}

/* This function is called on a regular basis */
void PlaylistManager::Update()
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

/**********************************************************************
 * Rebuild the playlist
 **********************************************************************/
void PlaylistManager::Rebuild( vlc_bool_t b_root )
{
    playlist_view_t *p_view;

    i_items_to_append = 0;
    i_cached_item_id = -1;

    p_view = playlist_ViewFind( p_playlist, VIEW_CATEGORY );

    treectrl->DeleteAllItems();
    treectrl->AddRoot( wxU(_("root" )), -1, -1,
                       new PlaylistItem( p_view->p_root ) );

    wxTreeItemId root = treectrl->GetRootItem();
    UpdateNode( p_view->p_root, root );
}

/**********************************************************************
 * Search functions (internal)
 **********************************************************************/

/* Find a wxItem from a playlist id */
wxTreeItemId PlaylistManager::FindItem( wxTreeItemId root, int i_id )
{
    wxTreeItemIdValue cookie;
    PlaylistItem *p_wxcurrent;
    wxTreeItemId dummy, search, item, child;

    if( i_id < 0 ) return dummy;
    if( i_cached_item_id == i_id ) return cached_item;

    p_wxcurrent = (PlaylistItem *)treectrl->GetItemData( root );
    if( !p_wxcurrent ) return dummy;

    if( p_wxcurrent->i_id == i_id )
    {
        i_cached_item_id = i_id;
        cached_item = root;
        return root;
    }

    item = treectrl->GetFirstChild( root, cookie );
    while( item.IsOk() )
    {
        p_wxcurrent = (PlaylistItem *)treectrl->GetItemData( item );
        if( !p_wxcurrent )
        {
            item = treectrl->GetNextChild( root, cookie );
            continue;
        }

        if( p_wxcurrent->i_id == i_id )
        {
            i_cached_item_id = i_id;
            cached_item = item;
            return item;
        }

        if( treectrl->ItemHasChildren( item ) )
        {
            wxTreeItemId search = FindItem( item, i_id );
            if( search.IsOk() ) return search;
        }

        item = treectrl->GetNextChild( root, cookie );
    }

    return dummy;
}

/********************************************************************
 * Events
 ********************************************************************/
void PlaylistManager::OnActivateItem( wxTreeEvent& event )
{
    playlist_item_t *p_item, *p_node;
    wxTreeItemId parent = treectrl->GetItemParent( event.GetItem() );
    PlaylistItem *p_wxitem = (PlaylistItem *)
        treectrl->GetItemData( event.GetItem() );

    if( !p_wxitem || !parent.IsOk() ) return;

    PlaylistItem *p_wxparent = (PlaylistItem *)treectrl->GetItemData( parent );
    if( !p_wxparent ) return;

    LockPlaylist( p_intf->p_sys, p_playlist );
    p_item = playlist_ItemGetById( p_playlist, p_wxitem->i_id );
    p_node = playlist_ItemGetById( p_playlist, p_wxparent->i_id );
    if( !p_item || p_item->i_children >= 0 )
    {
        p_node = p_item;
        p_item = NULL;
    }

    playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, VIEW_CATEGORY,
                      p_node, p_item );
    UnlockPlaylist( p_intf->p_sys, p_playlist );
}

void PlaylistManager::OnPlaylistEvent( wxCommandEvent& event )
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
 * ItemChanged: callback triggered by the item-change playlist variable
 *****************************************************************************/
static int ItemChanged( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    PlaylistManager *p_playlist = (PlaylistManager *)param;

    wxCommandEvent event( wxEVT_PLAYLIST, UpdateItem_Event );
    event.SetInt( new_val.i_int );
    p_playlist->AddPendingEvent( event );

    return VLC_SUCCESS;
}
static int ItemDeleted( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    PlaylistManager *p_playlist = (PlaylistManager *)param;

    wxCommandEvent event( wxEVT_PLAYLIST, RemoveItem_Event );
    event.SetInt( new_val.i_int );
    p_playlist->AddPendingEvent( event );

    return VLC_SUCCESS;
}

static int ItemAppended( vlc_object_t *p_this, const char *psz_variable,
                         vlc_value_t oval, vlc_value_t nval, void *param )
{
    PlaylistManager *p_playlist = (PlaylistManager *)param;

    playlist_add_t *p_add = (playlist_add_t *)malloc(sizeof( playlist_add_t));
    memcpy( p_add, nval.p_address, sizeof( playlist_add_t ) );

    if( p_playlist->i_items_to_append++ > 50 )
    {
        /* Too many items waiting to be added, it will be quicker to rebuild
         * the whole playlist */
        p_playlist->b_need_update = VLC_TRUE;
        return VLC_SUCCESS;
    }

    wxCommandEvent event( wxEVT_PLAYLIST, AppendItem_Event );
    event.SetClientData( (void *)p_add );
    p_playlist->AddPendingEvent( event );

    return VLC_SUCCESS;
}
}
