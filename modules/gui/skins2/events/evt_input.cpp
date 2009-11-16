/*****************************************************************************
 * evt_input.cpp
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

#include "evt_input.hpp"
#include "vlc_keys.h"

const int
    EvtInput::kModNone=0,
    EvtInput::kModAlt=KEY_MODIFIER_ALT,
    EvtInput::kModShift=KEY_MODIFIER_SHIFT,
    EvtInput::kModCtrl=KEY_MODIFIER_CTRL,
    EvtInput::kModMeta=KEY_MODIFIER_META,
    EvtInput::kModCmd=KEY_MODIFIER_COMMAND;

EvtInput::EvtInput( intf_thread_t *pIntf, int mod )
    : EvtGeneric( pIntf), m_mod( mod ) { }


void EvtInput::addModifier( string &rEvtString ) const
{
    if( m_mod == kModNone )
    {
        rEvtString += ":none";
    }
    else
    {
        string m = ":";
        if( m_mod & kModAlt )
            m += "alt,";
        if( m_mod & kModCtrl )
            m += "ctrl,";
        if( m_mod & kModShift )
            m += "shift,";
        if( m_mod & kModMeta )
            m += "meta,";
        if( m_mod & kModCmd )
            m += "cmd,";
        // Append the result except the last ','
        rEvtString.insert( rEvtString.end(), m.begin(), m.end()-1 );
    }
}
