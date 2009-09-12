/*****************************************************************************
 * var_text.hpp
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

#ifndef VAR_TEXT_HPP
#define VAR_TEXT_HPP

#include "variable.hpp"
#include "var_percent.hpp"
#include "observer.hpp"
#include "ustring.hpp"


/// String variable
class VarText: public Variable, public Subject<VarText>,
               public Observer<VarPercent>,
               public Observer<VarText>
{
public:
    // Set substVars to true to replace "$X" variables in the text
    VarText( intf_thread_t *pIntf, bool substVars = true );
    virtual ~VarText();

    /// Get the variable type
    virtual const string &getType() const { return m_type; }

    /// Set the internal value
    virtual void set( const UString &rText );
    virtual const UString get() const;

    /// Methods called when an observed variable is modified
    virtual void onUpdate( Subject<VarPercent> &rVariable, void* );
    virtual void onUpdate( Subject<VarText> &rVariable, void* );

private:
    /// Stop observing other variables
    void delObservers();

    /// Variable type
    static const string m_type;
    /// The text of the variable
    UString m_text;
    /// Actual text after having replaced the variables
    UString m_lastText;
    /// Flag to activate or not "$X" variables substitution
    bool m_substVars;
};

#endif
