/*****************************************************************************
 * evt_key.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef EVT_KEY_HPP
#define EVT_KEY_HPP

#include "evt_input.hpp"


/// Class for keyboard events
class EvtKey: public EvtInput
{
public:
    enum ActionType_t
    {
        kDown,
        kUp
    };

    EvtKey( intf_thread_t *I, int key, ActionType_t actn, int mod = kModNone )
          : EvtInput( I, mod ), m_key( key ), m_action( actn ) { }
    virtual ~EvtKey() { }
    virtual const string getAsString() const;

    int getKey() const { return m_key; }
    int getModKey() const { return m_key | getMod(); }

    ActionType_t getKeyState() const { return m_action; }

private:
    /// The concerned key, stored according to the '#define's in vlc_keys.h
    /// but without the modifiers (which are stored in EvtInput)
    int m_key;
    /// Type of action
    ActionType_t m_action;
};


#endif
