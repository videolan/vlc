/*****************************************************************************
 * cmd_on_top.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "cmd_on_top.hpp"
#include "../src/theme.hpp"
#include "../src/window_manager.hpp"


void CmdOnTop::execute()
{
    getIntf()->p_sys->p_theme->getWindowManager().toggleOnTop();
}

void CmdSetOnTop::execute()
{
    getIntf()->p_sys->p_theme->getWindowManager().setOnTop( m_ontop );
}
