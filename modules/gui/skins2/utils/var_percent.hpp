/*****************************************************************************
 * var_percent.hpp
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

#ifndef VAR_PERCENT_HPP
#define VAR_PERCENT_HPP

#include "variable.hpp"
#include "observer.hpp"


/// Percentage variable
class VarPercent: public Variable, public Subject<VarPercent>
{
    public:
        VarPercent( intf_thread_t *pIntf );
        virtual ~VarPercent() {}

        /// Get the variable type
        virtual const string &getType() const { return m_type; }

        /// Set the internal value
        virtual void set( float percentage );
        virtual float get() const { return m_value; }

    private:
        /// Variable type
        static const string m_type;
        /// Percent value
        float m_value;
};

#endif
