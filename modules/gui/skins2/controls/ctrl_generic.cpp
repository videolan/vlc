/*****************************************************************************
 * ctrl_generic.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: ctrl_generic.cpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#include "ctrl_generic.hpp"
#include "../src/generic_layout.hpp"
#include "../src/generic_window.hpp"
#include "../src/os_graphics.hpp"
#include "../utils/position.hpp"


CtrlGeneric::CtrlGeneric( intf_thread_t *pIntf, const UString &rHelp ):
    SkinObject( pIntf ), m_pLayout( NULL ), m_pPosition( NULL ),
    m_help( rHelp )
{
}


CtrlGeneric::~CtrlGeneric()
{
    if( m_pPosition )
    {
        delete m_pPosition;
    }
}


void CtrlGeneric::setLayout( GenericLayout *pLayout,
                             const Position &rPosition )
{
    m_pLayout = pLayout;
    if( m_pPosition )
    {
        delete m_pPosition;
    }
    m_pPosition = new Position( rPosition );
    onPositionChange();
}


void CtrlGeneric::notifyLayout() const
{
    // Notify the layout
    if( m_pLayout )
    {
        m_pLayout->onControlUpdate( *this );
    }
}


void CtrlGeneric::captureMouse() const
{
    // Tell the layout we want to capture the mouse
    if( m_pLayout )
    {
        m_pLayout->onControlCapture( *this );
    }
}


void CtrlGeneric::releaseMouse() const
{
    // Tell the layout we want to release the mouse
    if( m_pLayout )
    {
        m_pLayout->onControlRelease( *this );
    }
}


void CtrlGeneric::notifyTooltipChange() const
{
    GenericWindow *pWin = getWindow();
    if( pWin )
    {
        // Notify the window
        pWin->onTooltipChange( *this );
    }
}


GenericWindow *CtrlGeneric::getWindow() const
{
    if( m_pLayout )
    {
        return m_pLayout->getWindow();
    }
    return NULL;
}

