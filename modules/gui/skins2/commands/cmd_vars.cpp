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
    if( !m_pItem )
        return;

    // update playtree
    playlist_t* pPlaylist = getIntf()->p_sys->p_playlist;
    playlist_Lock( pPlaylist );
    playlist_item_t* p_plItem = playlist_ItemGetByInput( pPlaylist, m_pItem );
    int id = p_plItem ? p_plItem->i_id : 0;
    playlist_Unlock( pPlaylist );

    if( id )
        VlcProc::instance( getIntf() )->getPlaytreeVar().onUpdateItem( id );

    // update current input if needed
    input_item_t* p_current = NULL;
    input_thread_t* pInput = getIntf()->p_sys->p_input;
    if( pInput )
        p_current = input_GetItem( pInput );

    if( p_current == m_pItem )
        VlcProc::instance( getIntf() )->update_current_input();
}

bool CmdItemUpdate::checkRemove( CmdGeneric *pQueuedCommand ) const
{
    // We don't use RTTI - Use C-style cast
    CmdItemUpdate *pUpdateCommand = (CmdItemUpdate *)(pQueuedCommand);
    return m_pItem == pUpdateCommand->m_pItem;
}


void CmdPlaytreeAppend::execute()
{
    VlcProc::instance( getIntf() )->getPlaytreeVar().onAppend( m_pAdd );
}

void CmdPlaytreeDelete::execute()
{
    VlcProc::instance( getIntf() )->getPlaytreeVar().onDelete( m_id );
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
