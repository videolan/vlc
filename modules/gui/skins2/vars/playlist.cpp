/*****************************************************************************
 * playlist.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: playlist.cpp,v 1.5 2004/01/05 20:02:21 gbazin Exp $
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

#include <vlc/vlc.h>

#if defined(HAVE_ICONV)
#include <iconv.h>
#include "charset.h"
#endif

#include "playlist.hpp"
#include "../utils/ustring.hpp"


Playlist::Playlist( intf_thread_t *pIntf ): VarList( pIntf )
{
    // Get the playlist VLC object
    m_pPlaylist = pIntf->p_sys->p_playlist;

#ifdef HAVE_ICONV
    // Try to guess the current charset
    char *pCharset = (char*)malloc( 100 );
    vlc_current_charset( &pCharset );
    iconvHandle = iconv_open( "UTF-8", pCharset );
    msg_Dbg( pIntf, "Using character encoding: %s", pCharset );
    free( pCharset );

    if( iconvHandle == (iconv_t)-1 )
    {
        msg_Warn( pIntf, "Unable to do requested conversion" );
    }
#else
    msg_Dbg( p_dec, "No iconv support available" );
#endif

    buildList();
}


Playlist::~Playlist()
{
#ifdef HAVE_ICONV
    if( iconvHandle != (iconv_t)-1 )
    {
        iconv_close( iconvHandle );
    }
#endif
}


void Playlist::add( const UStringPtr &rcString )
{
    VarList::add( rcString );
    //XXX
}


void Playlist::delSelected()
{
    // Remove the items from the VLC playlist
    int index = 0;
    ConstIterator it;
    for( it = begin(); it != end(); it++ )
    {
        if( (*it).m_selected )
        {
            playlist_Delete( m_pPlaylist, index );
        }
        else
        {
            index++;
        }
    }

    notify();
}


void Playlist::action( Elem_t *pItem )
{
    // Find the index of the item
    int index = 0;
    ConstIterator it;
    for( it = begin(); it != end(); it++ )
    {
        if( &*it == pItem ) break;
        index++;
    }
    // Item found ?
    if( index < size() )
    {
        playlist_Goto( m_pPlaylist, index );
    }
}


void Playlist::onChange()
{
    buildList();
    notify();
}


void Playlist::buildList()
{
    clear();

    vlc_mutex_lock( &m_pPlaylist->object_lock );
    for( int i = 0; i < m_pPlaylist->i_size; i++ )
    {
        // Get the name of the playlist item
        UString *pName = convertName( m_pPlaylist->pp_items[i]->psz_name );
        // Is it the played stream ?
        bool playing = (i == m_pPlaylist->i_index );
        // Add the item in the list
        m_list.push_back( Elem_t( UStringPtr( pName ), false, playing ) );
    }
    vlc_mutex_unlock( &m_pPlaylist->object_lock );
}


UString *Playlist::convertName( const char *pName )
{
#ifdef HAVE_ICONV
    if( iconvHandle == (iconv_t)-1 )
    {
        return new UString( getIntf(), pName );
    }

    char *pNewName, *pBufferOut;
    const char *pBufferIn;
    size_t ret, inbytesLeft, outbytesLeft;

    // Try to convert the playlist item into UTF8
    pNewName = (char*)malloc( 6 * strlen( pName ) );
    pBufferOut = pNewName;
    pBufferIn = pName;
    inbytesLeft = strlen( pName );
    outbytesLeft = 6 * inbytesLeft;
    // ICONV_CONST is defined in config.h
    ret = iconv( iconvHandle, (ICONV_CONST char **)&pBufferIn, &inbytesLeft,
                 &pBufferOut, &outbytesLeft );
    *pBufferOut = '\0';

    if( inbytesLeft )
    {
        msg_Warn( getIntf(), "Failed to convert the playlist item into UTF8" );
        free( pNewName );
        return new UString( getIntf(), pName );
    }
    else
    {
        UString *pString = new UString( getIntf(), pNewName );
        free( pNewName );
        return pString;
    }
#else
    return new UString( getIntf(), pName );
#endif
}

