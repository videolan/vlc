/*****************************************************************************
 * cmd_vars.hpp
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
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

class Stream;

/// Command to notify the playlist of a change
DEFINE_COMMAND( NotifyPlaylist, "notify playlist" )


/// Command to set a stream variable
class CmdSetStream: public CmdGeneric
{
    public:
        CmdSetStream( intf_thread_t *pIntf, Stream &rStream,
                      const UString &rName, bool updateVLC ):
            CmdGeneric( pIntf ), m_rStream( rStream ), m_name( rName ),
            m_updateVLC( updateVLC ) {}
        virtual ~CmdSetStream() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "set stream"; }

    private:
        /// Stream variable to set
        Stream &m_rStream;
        /// Value to set
        const UString m_name;
        bool m_updateVLC;
};


#endif
