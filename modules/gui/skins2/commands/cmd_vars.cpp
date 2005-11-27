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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
    rVar.onUpdate( m_id );
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
