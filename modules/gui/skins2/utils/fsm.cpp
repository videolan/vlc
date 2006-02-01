/*****************************************************************************
 * fsm.cpp
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

#include "fsm.hpp"
#include "../commands/cmd_generic.hpp"


void FSM::addState( const string &state )
{
    m_states.insert( state );
}


void FSM::addTransition( const string &state1, const string &event,
                         const string &state2, CmdGeneric *pCmd )
{
    // Check that we already know the states
    if( m_states.find( state1 ) == m_states.end() ||
        m_states.find( state2 ) == m_states.end() )
    {
        msg_Warn( getIntf(),
                  "FSM: ignoring transition between invalid states" );
        return;
    }

    Key_t key( state1, event );
    Data_t data( state2, pCmd );

    // Check that the transition doesn't already exist
    if( m_transitions.find( key ) != m_transitions.end() )
    {
        msg_Warn( getIntf(), "FSM: transition already exists" );
        return;
    }

    m_transitions[key] = data;
}


void FSM::setState( const string &state )
{
    if( m_states.find( state ) == m_states.end() )
    {
        msg_Warn( getIntf(), "FSM: trying to set an invalid state" );
        return;
    }
    m_currentState = state;
}


void FSM::handleTransition( const string &event )
{
    string tmpEvent = event;
    Key_t key( m_currentState, event );
    map<Key_t, Data_t>::const_iterator it;

    // Find a transition
    it = m_transitions.find( key );

    // While the matching fails, try to match a more generic transition
    // For example, if "key:up:F" isn't a transition, "key:up" or "key" may be
    while( it == m_transitions.end() &&
           tmpEvent.rfind( ":", tmpEvent.size() ) != string::npos )
    {
        // Cut the last part
        tmpEvent = tmpEvent.substr( 0, tmpEvent.rfind( ":", tmpEvent.size() ) );

        key.second = tmpEvent;
        it = m_transitions.find( key );
    }

    // No transition was found
    if( it == m_transitions.end() )
    {
        return;
    }

    // Change state
    m_currentState = (*it).second.first;

    // Call the callback, if any
    CmdGeneric *pCmd = (*it).second.second;
    if( pCmd != NULL )
    {
        pCmd->execute();
    }
}
