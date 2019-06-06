/*****************************************************************************
 * cmd_add_item.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef CMD_ADD_ITEM_HPP
#define CMD_ADD_ITEM_HPP

#include "cmd_generic.hpp"
#include <string>


/// "Add item" command
class CmdAddItem: public CmdGeneric
{
public:
    CmdAddItem( intf_thread_t *pIntf, const std::string &rName, bool playNow )
              : CmdGeneric( pIntf ), m_name( rName ), m_playNow( playNow ) { }
    virtual ~CmdAddItem() { }
    virtual void execute();
    virtual std::string getType() const { return "add item"; }

private:
    /// Name of the item to enqueue
    std::string m_name;
    /// Should we play the item immediately?
    bool m_playNow;
};

#endif
