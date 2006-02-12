/*****************************************************************************
 * cmd_vars.cpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "cmd_vars.hpp"
#include "../src/vlcproc.hpp"
#include "../utils/var_text.hpp"
#include "../vars/equalizer.hpp"
#include "../vars/playlist.hpp"
#include "../vars/playtree.hpp"


void CmdNotifyPlaylist::execute()
{
    // Notify the playlist variable
    Playlist &rVar = VlcProc::instance( getIntf() )->getPlaylistVar();
    rVar.onChange();
}

void CmdPlaytreeChanged::execute()
{
    // Notify  the playtree variable
    Playtree &rVar = VlcProc::instance( getIntf() )->getPlaytreeVar();
    rVar.onChange();
}


void CmdPlaytreeUpdate::execute()
{
    // Notify  the playtree variable
    Playtree &rVar = VlcProc::instance( getIntf() )->getPlaytreeVar();
    rVar.onUpdateItem( m_id );
}

bool CmdPlaytreeUpdate::checkRemove( CmdGeneric *pQueuedCommand ) const
{

    CmdPlaytreeUpdate *pUpdateCommand = (CmdPlaytreeUpdate *)(pQueuedCommand);
    //CmdPlaytreeUpdate *pUpdateCommand = dynamic_cast<CmdPlaytreeUpdate *>(pQueuedCommand);
    if( m_id == pUpdateCommand->m_id )
    {
        return true;
    }
    return false;
}


void CmdPlaytreeAppend::execute()
{
    // Notify  the playtree variable
    Playtree &rVar = VlcProc::instance( getIntf() )->getPlaytreeVar();
    rVar.onAppend( m_pAdd );
}

void CmdPlaytreeDelete::execute()
{
    // Notify  the playtree variable
    Playtree &rVar = VlcProc::instance( getIntf() )->getPlaytreeVar();
    rVar.onDelete( m_id );
}

void CmdSetText::execute()
{
    // Change the text variable
    m_rText.set( m_value );
}


void CmdSetEqBands::execute()
{
    // Change the equalizer bands
    m_rEqBands.set( m_value );
}


void CmdSetEqPreamp::execute()
{
    // Change the preamp variable
    m_rPreamp.set( m_value, false );
}
