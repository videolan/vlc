/*****************************************************************************
 * cmd_playlist.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: cmd_playlist.hpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#ifndef CMD_PLAYLIST_HPP
#define CMD_PLAYLIST_HPP

#include "cmd_generic.hpp"
#include "../utils/var_list.hpp"


/// Command to delete the selected items from a list
class CmdPlaylistDel: public CmdGeneric
{
    public:
        CmdPlaylistDel( intf_thread_t *pIntf, VarList &rList ):
            CmdGeneric( pIntf ), m_rList( rList ) {}
        virtual ~CmdPlaylistDel() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playlist del"; }

    private:
        /// List
        VarList &m_rList;
};


/// Command to sort the playlist
DEFINE_COMMAND( PlaylistSort, "playlist sort" )

/// Command to jump to the next item
DEFINE_COMMAND( PlaylistNext, "playlist next" )

/// Command to jump to the previous item
DEFINE_COMMAND( PlaylistPrevious, "playlist previous" )


#endif
