/*****************************************************************************
 * cmd_notify_playlist.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: cmd_notify_playlist.hpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#ifndef CMD_NOTIFY_PLAYLIST_HPP
#define CMD_NOTIFY_PLAYLIST_HPP

#include "cmd_generic.hpp"


/// Command to notify the playlist of a change
class CmdNotifyPlaylist: public CmdGeneric
{
    public:
        CmdNotifyPlaylist( intf_thread_t *pIntf ): CmdGeneric( pIntf ) {}
        virtual ~CmdNotifyPlaylist() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "notify playlist"; }
};

#endif
