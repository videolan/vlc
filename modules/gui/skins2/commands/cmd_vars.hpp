/*****************************************************************************
 * cmd_vars.hpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#ifndef CMD_VARS_HPP
#define CMD_VARS_HPP

#include "cmd_generic.hpp"
#include "../utils/ustring.hpp"

class VarText;

/// Command to notify the playlist of a change
DEFINE_COMMAND( NotifyPlaylist, "notify playlist" )

/// Command to notify the playlist of a change
DEFINE_COMMAND( PlaytreeChanged, "playtree changed" )

/// Command to notify the playtree of an item update
class CmdPlaytreeUpdate: public CmdGeneric
{
    public:
        CmdPlaytreeUpdate( intf_thread_t *pIntf, int id ):
            CmdGeneric( pIntf ), m_id( id ) {}
        virtual ~CmdPlaytreeUpdate() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "playtree update"; }

    private:
        /// Playlist item ID
        int m_id;
};


/// Command to set a text variable
class CmdSetText: public CmdGeneric
{
    public:
        CmdSetText( intf_thread_t *pIntf, VarText &rText,
                    const UString &rValue ):
            CmdGeneric( pIntf ), m_rText( rText ), m_value( rValue ) {}
        virtual ~CmdSetText() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "set text"; }

    private:
        /// Text variable to set
        VarText &m_rText;
        /// Value to set
        const UString m_value;
};


#endif
