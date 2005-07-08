/*****************************************************************************
 * cmd_layout.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale RÃ©seaux) and its contributors
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "cmd_layout.hpp"
#include "../src/top_window.hpp"
#include "../src/generic_layout.hpp"
#include "../src/theme.hpp"


CmdLayout::CmdLayout( intf_thread_t *pIntf, const string &windowId,
                      const string &layoutId ):
    CmdGeneric( pIntf ), m_windowId( windowId ), m_layoutId( layoutId )
{
}


void CmdLayout::execute()
{
    // Get the window and the layout
    if( !getIntf()->p_sys->p_theme )
    {
        return;
    }
    TopWindow *pWindow =
        getIntf()->p_sys->p_theme->getWindowById( m_windowId );
    GenericLayout *pLayout =
        getIntf()->p_sys->p_theme->getLayoutById( m_layoutId );
    if( !pWindow || !pLayout )
    {
        msg_Err( getIntf(), "Cannot change layout (%s, %s)",
                 m_windowId.c_str(), m_layoutId.c_str() );
        return;
    }

    // XXX TODO: check that the layout isn't a layout of another window

    getIntf()->p_sys->p_theme->getWindowManager().setActiveLayout( *pWindow,
                                                                   *pLayout );
}
