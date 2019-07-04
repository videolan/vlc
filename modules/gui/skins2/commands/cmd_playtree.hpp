/*****************************************************************************
 * cmd_playtree.hpp
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

#ifndef CMD_PLAYTREE_HPP
#define CMD_PLAYTREE_HPP

#include "cmd_generic.hpp"
#include "../utils/var_tree.hpp"

// TODO : implement branch specific stuff

/// Command to delete the selected items from a tree
class CmdPlaytreeDel: public CmdGeneric
{
public:
    CmdPlaytreeDel( intf_thread_t *pIntf, VarTree &rTree )
                  : CmdGeneric( pIntf ), m_rTree( rTree ) { }
    virtual ~CmdPlaytreeDel() { }
    virtual void execute();
    virtual std::string getType() const { return "playtree del"; }

private:
    /// Tree
    VarTree &m_rTree;
};

/// Command to reset the playtree
DEFINE_COMMAND( PlaytreeReset, "playtree reset" )

/// Command to sort the playtree
DEFINE_COMMAND( PlaytreeSort, "playtree sort" )

#endif
