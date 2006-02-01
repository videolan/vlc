/*****************************************************************************
 * cmd_resize.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "cmd_resize.hpp"
#include "../src/generic_layout.hpp"
#include "../src/vlcproc.hpp"


CmdResize::CmdResize( intf_thread_t *pIntf, GenericLayout &rLayout, int width,
                      int height ):
    CmdGeneric( pIntf ), m_rLayout( rLayout ), m_width( width ),
    m_height( height )
{
}


void CmdResize::execute()
{
    // Resize the layout
    m_rLayout.resize( m_width, m_height );
}


CmdResizeVout::CmdResizeVout( intf_thread_t *pIntf, void *pWindow, int width,
                              int height ):
    CmdGeneric( pIntf ), m_pWindow( pWindow ), m_width( width ),
    m_height( height )
{
}


void CmdResizeVout::execute()
{
    VarBox &rVoutSize = VlcProc::instance( getIntf() )->getVoutSizeVar();
    rVoutSize.setSize( m_width, m_height );
}

