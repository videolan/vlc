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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VAR_PERCENT_HPP
#define VAR_PERCENT_HPP

#include "variable.hpp"
#include "observer.hpp"

class VarPercent;


/// Percentage variable
class VarPercent: public Variable, public Subject<VarPercent>
{
public:
    VarPercent( intf_thread_t *pIntf ) :
        Variable( pIntf ), m_value( 0 ), m_step( .05f ) {}
    virtual ~VarPercent() { }

    /// Get the variable type
    virtual const string &getType() const { return m_type; }

    /// Set the internal value
    virtual void set( float percentage );
    virtual float get() const { return m_value; }

    /// Get the variable preferred step
    virtual float getStep() const { return m_step; }
    virtual void setStep( float val ) { m_step = val; }

    /// Increment or decrement variable
    void increment( int num ) { return set( m_value + num * m_step ); }

private:
    /// Variable type
    static const string m_type;
    /// Percent value
    float m_value;
    /// preferred step (for scrolling)
    float m_step;
};

#endif
