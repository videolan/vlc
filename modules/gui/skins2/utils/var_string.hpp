/*****************************************************************************
 * var_string.hpp
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 *
 * Author: Erwan Tulou      <erwan10 aT videolan DoT org>
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

#ifndef VAR_STRING
#define VAR_STRING

#include "variable.hpp"
#include "observer.hpp"

class VarString;


/// String variable
class VarString: public Variable, public Subject<VarString>
{
public:
    VarString( intf_thread_t *pIntf ): Variable( pIntf ) { }
    virtual ~VarString() { }

    /// Get the variable type
    virtual const std::string &getType() const { return m_type; }

    /// Set the internal value
    virtual void set( std::string str );
    virtual std::string get() const { return m_value; }

private:
    /// Variable type
    static const std::string m_type;
    /// string value
    std::string m_value;
};

#endif
