/*****************************************************************************
 * playtree.hpp
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef PLAYTREE_HPP
#define PLAYTREE_HPP

#include "../utils/var_tree.hpp"

/// Variable for VLC playlist (new tree format)
class Playtree: public VarTree
{
    public:
        Playtree( intf_thread_t *pIntf );
        virtual ~Playtree();

        /// Remove the selected elements from the list
        virtual void delSelected();

        /// Execute the action associated to this item
        virtual void action( VarTree *pItem );

        /// Function called to notify playlist changes
        void onChange();

        /// Function called to notify playlist item update
        void onUpdateItem( int id );

        /// Function called to notify playlist item append
        void onAppend( playlist_add_t * );

        /// Function called to notify playlist item delete
        void onDelete( int );

        /// Items waiting to be appended
        int i_items_to_append;

    private:
        /// VLC playlist object
        playlist_t *m_pPlaylist;
        /// Iconv handle
        vlc_iconv_t iconvHandle;

        /// Build the list from the VLC playlist
        void buildTree();

        /// Update Node's children
        void buildNode( playlist_item_t *p_node, VarTree &m_pNode );
};

#endif
