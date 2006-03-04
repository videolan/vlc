/*****************************************************************************
 * playlist.hpp: Header for the playlist
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _WXVLC_PLAYLIST_H_
#define _WXVLC_PLAYLIST_H_

#include "wxwidgets.hpp"

#include <wx/treectrl.h>

#define MODE_NONE 0
#define MODE_GROUP 1
#define MODE_AUTHOR 2
#define MODE_TITLE 3

#define OPEN_NORMAL 0
#define OPEN_STREAM 1

namespace wxvlc
{
class ItemInfoDialog;
class NewGroup;
class ExportPlaylist;

/* Playlist */
class Playlist: public wxFrame
{
public:
    /* Constructor */
    Playlist( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~Playlist();

    void UpdatePlaylist();
    void ShowPlaylist( bool show );
    void UpdateItem( int );
    void AppendItem( wxCommandEvent& );

    bool b_need_update;
    int  i_items_to_append;

private:
    void RemoveItem( int );
    void DeleteTreeItem( wxTreeItemId );
    void DeleteItem( int item );
    void DeleteNode( playlist_item_t *node );

    void RecursiveDeleteSelection( wxTreeItemId );

    /* Event handlers (these functions should _not_ be virtual) */

    /* Menu Handlers */
    void OnAddFile( wxCommandEvent& event );
    void OnAddDir( wxCommandEvent& event );
    void OnAddMRL( wxCommandEvent& event );
    void OnMenuClose( wxCommandEvent& event );
    void OnClose( wxCloseEvent& WXUNUSED(event) );

    void OnDeleteSelection( wxCommandEvent& event );

    void OnOpen( wxCommandEvent& event );
    void OnSave( wxCommandEvent& event );

    /* Search (user) */
    void OnSearch( wxCommandEvent& event );
    /*void OnSearchTextChange( wxCommandEvent& event );*/
    wxTextCtrl *search_text;
    wxButton *search_button;
    wxTreeItemId search_current;

    void OnEnDis( wxCommandEvent& event );

    /* Sort */
    int i_sort_mode;
    void OnSort( wxCommandEvent& event );
    int i_title_sorted;
    int i_group_sorted;
    int i_duration_sorted;

    /* Dynamic menus */
    void OnMenuEvent( wxCommandEvent& event );
    void OnMenuOpen( wxMenuEvent& event );
    wxMenu *p_view_menu;
    wxMenu *p_sd_menu;
    wxMenu *ViewMenu();
    wxMenu *SDMenu();

    void OnUp( wxCommandEvent& event);
    void OnDown( wxCommandEvent& event);

    void OnRandom( wxCommandEvent& event );
    void OnRepeat( wxCommandEvent& event );
    void OnLoop ( wxCommandEvent& event );

    void OnActivateItem( wxTreeEvent& event );
    void OnKeyDown( wxTreeEvent& event );
    void OnNewGroup( wxCommandEvent& event );

    void OnDragItemBegin( wxTreeEvent& event );
    void OnDragItemEnd( wxTreeEvent& event );
    wxTreeItemId draged_tree_item;

    /* Popup  */
    wxMenu *item_popup;
    wxMenu *node_popup;
    wxTreeItemId i_wx_popup_item;
    int i_popup_item;
    int i_popup_parent;
    void OnPopup( wxContextMenuEvent& event );
    void OnPopupPlay( wxCommandEvent& event );
    void OnPopupPreparse( wxCommandEvent& event );
    void OnPopupSort( wxCommandEvent& event );
    void OnPopupDel( wxCommandEvent& event );
    void OnPopupEna( wxCommandEvent& event );
    void OnPopupInfo( wxCommandEvent& event );
    void Rebuild( vlc_bool_t );

    void Preparse();

    /* Update */
    void UpdateNode( playlist_item_t*, wxTreeItemId );
    void UpdateNodeChildren( playlist_item_t*, wxTreeItemId );
    void CreateNode( playlist_item_t*, wxTreeItemId );
    void UpdateTreeItem( wxTreeItemId );

    /* Search (internal) */
    int CountItems( wxTreeItemId);
    wxTreeItemId FindItem( wxTreeItemId, int );
    wxTreeItemId FindItemByName( wxTreeItemId, wxString,
                                 wxTreeItemId, vlc_bool_t *);

    wxTreeItemId saved_tree_item;
    int i_saved_id;

    playlist_t *p_playlist;


    /* Custom events */
    void OnPlaylistEvent( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();


    /* Global widgets */
    wxStatusBar *statusbar;
    ItemInfoDialog *iteminfo_dialog;

    int i_update_counter;

    intf_thread_t *p_intf;
    wxTreeCtrl *treectrl;
    int i_current_view;
    vlc_bool_t b_changed_view;
    char **pp_sds;


};

} // end of wxvlc namespace

#endif
