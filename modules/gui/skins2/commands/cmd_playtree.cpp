/*****************************************************************************
 * cmd_playtree.cpp
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
    /// \todo Choose the correct view
    const struct vlc_playlist_sort_criterion option = {
        .key = VLC_PLAYLIST_SORT_KEY_TITLE,
        .order = VLC_PLAYLIST_SORT_ORDER_ASCENDING };
    vlc_playlist_Lock( getPL() );
    vlc_playlist_Sort( getPL(), &option, 1 );
    vlc_playlist_Unlock( getPL() );
}

void CmdPlaytreeReset::execute()
{
    VlcProc::instance( getIntf() )->getPlaytreeVar().onChange();
}
