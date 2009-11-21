/*****************************************************************************
 * cmd_minimize.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Mohammed Adnène Trojette     <adn@via.ecp.fr>
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

#include "cmd_minimize.hpp"
#include "../src/window_manager.hpp"
#include "../src/os_factory.hpp"


void CmdMinimize::execute()
{
    OSFactory::instance( getIntf() )->minimize();
}


void CmdRestore::execute()
{
    OSFactory::instance( getIntf() )->restore();
}


CmdMaximize::CmdMaximize( intf_thread_t *pIntf, WindowManager &rWindowManager,
                          TopWindow &rWindow )
    : CmdGeneric( pIntf ), m_rWindowManager( rWindowManager ),
      m_rWindow( rWindow ) { }


void CmdMaximize::execute()
{
    // Simply delegate the job to the WindowManager
    m_rWindowManager.maximize( m_rWindow );
}


CmdUnmaximize::CmdUnmaximize( intf_thread_t *pIntf,
                              WindowManager &rWindowManager,
                              TopWindow &rWindow )
    : CmdGeneric( pIntf ), m_rWindowManager( rWindowManager ),
      m_rWindow( rWindow ) { }


void CmdUnmaximize::execute()
{
    // Simply delegate the job to the WindowManager
    m_rWindowManager.unmaximize( m_rWindow );
}


void CmdAddInTray::execute()
{
    OSFactory::instance( getIntf() )->addInTray();
}


void CmdRemoveFromTray::execute()
{
    OSFactory::instance( getIntf() )->removeFromTray();
}


void CmdAddInTaskBar::execute()
{
    OSFactory::instance( getIntf() )->addInTaskBar();
}


void CmdRemoveFromTaskBar::execute()
{
    OSFactory::instance( getIntf() )->removeFromTaskBar();
}

