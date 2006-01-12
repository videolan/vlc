/*****************************************************************************
 * playlist_manager.hpp: Header for the playlist manager
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifndef _WXVLC_PLAYLIST_MANAGER_H_
#define _WXVLC_PLAYLIST_MANAGER_H_

#include "wxwidgets.hpp"
#include <wx/treectrl.h>

namespace wxvlc
{
/* PlaylistManager */
class PlaylistManager: public wxPanel
{
public:
    /* Constructor */
    PlaylistManager( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~PlaylistManager();

    void Update();

    bool b_need_update;
    int  i_items_to_append;

private:
    DECLARE_EVENT_TABLE();

    /* Update */
    void Rebuild( vlc_bool_t );
    void CreateNode( playlist_item_t*, wxTreeItemId );
    void UpdateNode( playlist_item_t*, wxTreeItemId );
    void UpdateNodeChildren( playlist_item_t*, wxTreeItemId );
    void UpdateTreeItem( wxTreeItemId );

    void UpdateItem( int );
    void AppendItem( wxCommandEvent& );
    void RemoveItem( int );

    wxTreeItemId FindItem( wxTreeItemId, int );

    /* Events */
    void OnActivateItem( wxTreeEvent& event );
    /* Custom events */
    void OnPlaylistEvent( wxCommandEvent& event );

    /* Simple cache for FindItem() */
    int i_cached_item_id;
    wxTreeItemId cached_item;

    intf_thread_t *p_intf;
    playlist_t *p_playlist;
    wxTreeCtrl *treectrl;
    wxSizer *sizer;

    int i_update_counter;
};

} // end of wxvlc namespace

#endif
