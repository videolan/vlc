/*****************************************************************************
 * cmd_vars.cpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#include "cmd_vars.hpp"
#include "../src/vlcproc.hpp"
#include "../utils/var_text.hpp"
#include "../vars/equalizer.hpp"
#include "../vars/playtree.hpp"


void CmdItemUpdate::execute()
{
    VlcProc::instance( getIntf() )->getPlaytreeVar().onUpdateItem( m_pos );
}

bool CmdItemUpdate::checkRemove( CmdGeneric *pQueuedCommand ) const
{
    // We don't use RTTI - Use C-style cast
    CmdItemUpdate *pUpdateCommand = (CmdItemUpdate *)(pQueuedCommand);
    return m_pos== pUpdateCommand->m_pos;
}

void CmdItemPlaying::execute()
{
    VlcProc::instance( getIntf() )->getPlaytreeVar().onUpdatePlaying( m_pos );
}

void CmdPlaytreeAppend::execute()
{
    VlcProc::instance( getIntf() )->getPlaytreeVar().onAppend( m_pos );
}

void CmdPlaytreeDelete::execute()
{
    VlcProc::instance( getIntf() )->getPlaytreeVar().onDelete( m_pos );
}

void CmdSetText::execute()
{
    m_rText.set( m_value );
}


void CmdSetEqBands::execute()
{
    m_rEqBands.set( m_value );
}


void CmdSetEqPreamp::execute()
{
    m_rPreamp.set( m_value, false );
}
