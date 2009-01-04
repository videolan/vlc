/*****************************************************************************
 * cmd_update_item.cpp
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>
#include "../src/os_factory.hpp"
#include "async_queue.hpp"
#include "cmd_vars.hpp"
#include "cmd_update_item.hpp"

void CmdUpdateItem::execute()
{
    playlist_t *pPlaylist = getIntf()->p_sys->p_playlist;
    if( pPlaylist == NULL )
        return;

	input_thread_t *p_input = playlist_CurrentInput( pPlaylist );
    if( !p_input )
        return;

	// Get playlist item information
	input_item_t *pItem = input_GetItem( p_input );

    char *pszName = input_item_GetName( pItem );
    char *pszUri = input_item_GetURI( pItem );

	string name = pszName;
	// XXX: This should be done in VLC core, not here...
	// Remove path information if any
	OSFactory *pFactory = OSFactory::instance( getIntf() );
	string::size_type pos = name.rfind( pFactory->getDirSeparator() );
	if( pos != string::npos )
	{
		name = name.substr( pos + 1, name.size() - pos + 1 );
	}
	UString srcName( getIntf(), name.c_str() );
	UString srcURI( getIntf(), pszUri );

    free( pszName );
    free( pszUri );

   // Create commands to update the stream variables
	CmdSetText *pCmd1 = new CmdSetText( getIntf(), m_rStreamName, srcName );
	CmdSetText *pCmd2 = new CmdSetText( getIntf(), m_rStreamURI, srcURI );
	// Push the commands in the asynchronous command queue
	AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
	pQueue->push( CmdGenericPtr( pCmd1 ), false );
	pQueue->push( CmdGenericPtr( pCmd2 ), false );
	vlc_object_release( p_input );
}

