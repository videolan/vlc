/*****************************************************************************
 * playlist.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifndef PLAYLIST_HPP
#define PLAYLIST_HPP

#include "../utils/var_list.hpp"

/// Variable for VLC playlist
class Playlist: public VarList
{
    public:
        Playlist( intf_thread_t *pIntf );
        virtual ~Playlist();

        /// Remove the selected elements from the list
        virtual void delSelected();

        /// Execute the action associated to this item
        virtual void action( Elem_t *pItem );

        /// Function called to notify playlist changes
        void onChange();

    private:
        /// VLC playlist object
        playlist_t *m_pPlaylist;
        /// Iconv handle
        vlc_iconv_t iconvHandle;

        /// Build the list from the VLC playlist
        void buildList();

        /// Convert a string to UTF8 from the current encoding
        UString *convertName( const char *pName );
};


#endif
