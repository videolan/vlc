/*****************************************************************************
 * var_text.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: var_text.hpp,v 1.1 2004/01/03 23:31:34 asmax Exp $
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

#ifndef VAR_TEXT_HPP
#define VAR_TEXT_HPP

#include "variable.hpp"
#include "var_percent.hpp"
#include "observer.hpp"
#include "ustring.hpp"


/// String variable
class VarText: public Variable, public Subject<VarText>,
               public Observer<VarPercent>, public Observer< VarText >
{
    public:
        VarText( intf_thread_t *pIntf );
        virtual ~VarText();

        /// Set the internal value
        virtual void set( const UString &rText );
        virtual const UString get() const;

        /// Methods called when an observed variable is modified
        virtual void onUpdate( Subject<VarPercent> &rVariable );
        virtual void onUpdate( Subject<VarText> &rVariable );

    private:
        /// The text of the variable
        UString m_text;
        /// Actual text after having replaced the variables
        UString m_lastText;

        /// Get the raw text without replacing the $something's
        const UString &getRaw() const { return m_text; }
};

#endif
