/*****************************************************************************
 * cmd_muxer.hpp
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id$
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifndef CMD_MUXER_HPP
#define CMD_MUXER_HPP

#include "cmd_generic.hpp"
#include <list>


/// This command only contains other commands (composite pattern)
class CmdMuxer: public CmdGeneric
{
    public:
        CmdMuxer( intf_thread_t *pIntf, const list<CmdGeneric*> &rList ):
            CmdGeneric( pIntf ), m_list( rList ) {}
        virtual ~CmdMuxer() {}

        /// This method does the real job of the command
        virtual void execute();

        /// Return the type of the command
        virtual string getType() const { return "muxer"; }

    private:
        /// List of commands we will execute sequentially
        list<CmdGeneric*> m_list;
};

#endif
