/*****************************************************************************
 * evt_key.cpp
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

#include "evt_key.hpp"
#include <vlc_keys.h>


const string EvtKey::getAsString() const
{
    string event = "key";

    // Add the action
    if( m_action == kDown )
        event += ":down";
    else if( m_action == kUp )
        event += ":up";
    else
        msg_Warn( getIntf(), "Unknown action type" );

    // Add the key
    char *keyName = vlc_keycode2str( m_key & ~KEY_MODIFIER, true );
    if( keyName )
    {
        event += (string)":" + keyName;
        free( keyName );
    }
    else
        msg_Warn( getIntf(), "Unknown key: %d", m_key );

    // Add the modifier
    addModifier( event );

    return event;
}
