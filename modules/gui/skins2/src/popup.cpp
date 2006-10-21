/*****************************************************************************
 * popup.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "popup.hpp"
#include "os_factory.hpp"
#include "os_popup.hpp"
#include "window_manager.hpp"
#include "../commands/cmd_generic.hpp"
#include "../events/evt_menu.hpp"


Popup::Popup( intf_thread_t *pIntf, WindowManager &rWindowManager )
    : SkinObject( pIntf ), m_rWindowManager( rWindowManager )
{
    // Get the OSFactory
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );

    // Create an OSPopup to handle OS specific processing
    m_pOsPopup = pOsFactory->createOSPopup();
}


void Popup::show( int xPos, int yPos )
{
    // Notify that we are the active popup menu, so that the window which
    // receives our menu events knows whom to forward them
    m_rWindowManager.setActivePopup( *this );

    m_pOsPopup->show( xPos, yPos );
}


void Popup::hide()
{
    m_pOsPopup->hide();
}


void Popup::addItem( const string &rLabel, CmdGeneric &rCmd, int pos )
{
    m_pOsPopup->addItem( rLabel, pos );
    m_actions[pos] = &rCmd;
}


void Popup::addSeparator( int pos )
{
    m_pOsPopup->addSeparator( pos );
    m_actions[pos] = NULL;
}


void Popup::handleEvent( const EvtMenu &rEvent )
{
    unsigned int n = m_pOsPopup->getPosFromId( rEvent.getItemId() );
    if( (n < m_actions.size()) && m_actions[n] )
    {
        m_actions[n]->execute();
    }
    else
    {
        // Should never happen
        msg_Warn( getIntf(), "problem in the popup implementation" );
    }
}

