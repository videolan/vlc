/*****************************************************************************
 * cmd_playtree.cpp
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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

#include "cmd_playtree.hpp"
#include "../src/vlcproc.hpp"
#include "../utils/var_bool.hpp"

void CmdPlaytreeDel::execute()
{
    m_rTree.delSelected();
}

void CmdPlaytreeSort::execute()
{
    /// \todo Choose sort method/order - Need more commands
    playlist_t *p_playlist = getIntf()->p_sys->p_playlist;
    vlc_mutex_lock( &p_playlist->object_lock );
    playlist_view_t* p_view = playlist_ViewFind( p_playlist, p_playlist->status.i_view );
    playlist_RecursiveNodeSort( p_playlist, p_view->p_root , SORT_TITLE, ORDER_NORMAL );
    vlc_mutex_unlock( &p_playlist->object_lock );

    // Ask for rebuild
    Playtree &rVar = VlcProc::instance( getIntf() )->getPlaytreeVar();
    rVar.onChange();
}
