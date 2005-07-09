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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "evt_input.hpp"

const int EvtInput::kModNone  = 0;
const int EvtInput::kModAlt   = 1;
const int EvtInput::kModCtrl  = 2;
const int EvtInput::kModShift = 4;


EvtInput::EvtInput( intf_thread_t *pIntf, int mod ):
    EvtGeneric( pIntf), m_mod( mod )
{
}


void EvtInput::addModifier( string &rEvtString ) const
{
    if( m_mod == kModNone )
    {
        rEvtString += ":none";
    }
    else
    {
        string modList = ":";
        if( m_mod & kModAlt )
        {
            modList += "alt,";
        }
        if( m_mod & kModCtrl )
        {
            modList += "ctrl,";
        }
        if( m_mod & kModShift )
        {
            modList += "shift,";
        }
        // Remove the last ','
        modList = modList.substr( 0, modList.size() - 1 );
        rEvtString += modList;
    }
}
