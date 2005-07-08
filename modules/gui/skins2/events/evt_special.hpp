/*****************************************************************************
 * evt_special.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale RÃ©seaux) and its contributors
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

#ifndef EVT_SPECIAL_HPP
#define EVT_SPECIAL_HPP

#include "evt_generic.hpp"


/// Class for non-genuine events
class EvtSpecial: public EvtGeneric
{
    public:
        typedef enum
        {
            kShow,
            kHide,
            kEnable,
            kDisable
        } ActionType_t;

        EvtSpecial( intf_thread_t *pIntf, ActionType_t action ):
            EvtGeneric( pIntf ), m_action( action ) {}
        virtual ~EvtSpecial() {}

        /// Return the type of event
        virtual const string getAsString() const;

    private:
        /// Type of action
        ActionType_t m_action;
};


#endif
