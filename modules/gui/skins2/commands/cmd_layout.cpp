/*****************************************************************************
 * cmd_layout.cpp
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

#include "cmd_layout.hpp"
#include "../src/top_window.hpp"
#include "../src/generic_layout.hpp"
#include "../src/theme.hpp"


CmdLayout::CmdLayout( intf_thread_t *pIntf, TopWindow &rWindow,
                      GenericLayout &rLayout )
    : CmdGeneric( pIntf ), m_rWindow( rWindow ), m_rLayout( rLayout ) { }


void CmdLayout::execute()
{
    Theme *p_theme = getIntf()->p_sys->p_theme;
    if( p_theme )
        p_theme->getWindowManager().setActiveLayout( m_rWindow, m_rLayout );
}
