/*****************************************************************************
 * cmd_change_skin.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef CMD_CHANGE_SKIN_HPP
#define CMD_CHANGE_SKIN_HPP

#include "cmd_generic.hpp"


/// "Change Skin" command
class CmdChangeSkin: public CmdGeneric
{
public:
    CmdChangeSkin( intf_thread_t *pIntf, const string &rFile ):
        CmdGeneric( pIntf ), m_file( rFile ) { }
    virtual ~CmdChangeSkin() { }
    virtual void execute();
    virtual string getType() const { return "change skin"; }

private:
    /// Skin file to load
    string m_file;
};

#endif
