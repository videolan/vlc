/*****************************************************************************
 * evt_input.hpp
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

#ifndef EVT_INPUT_HPP
#define EVT_INPUT_HPP

#include "evt_generic.hpp"


/// Base class for mouse and key events
class EvtInput: public EvtGeneric
{
    public:
        virtual ~EvtInput() {}

        /// Masks for modifier keys
        static const int kModNone;
        static const int kModAlt;
        static const int kModCtrl;
        static const int kModShift;

        /// Get the modifiers
        virtual int getMod() const { return m_mod; }

    protected:
        EvtInput( intf_thread_t *pIntf, int mod = kModNone );

        /// Add the modifier to the event string
        virtual void addModifier( string &rEvtString ) const;

    private:
        /// Modifiers (special key(s) pressed during the mouse event)
        int m_mod;
};

#endif
