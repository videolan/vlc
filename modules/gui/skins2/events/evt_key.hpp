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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef EVT_KEY_HPP
#define EVT_KEY_HPP

#include "evt_input.hpp"


/// Class for keyboard events
class EvtKey: public EvtInput
{
    public:
        typedef enum
        {
            kDown,
            kUp
        } ActionType_t;

        EvtKey( intf_thread_t *pIntf, int key, ActionType_t action,
                int mod = kModNone ):
            EvtInput( pIntf, mod ), m_key( key ), m_action( action ) {}
        virtual ~EvtKey() {}

        /// Return the type of event
        virtual const string getAsString() const;

        int getKey() const { return m_key; }

    private:
        /// The concerned key, stored according to the '#define's in vlc_keys.h
        /// but without the modifiers (which are stored in EvtInput)
        int m_key;
        /// Type of action
        ActionType_t m_action;
};


#endif
