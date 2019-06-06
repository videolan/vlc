/*****************************************************************************
 * var_string.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "var_string.hpp"


const std::string VarString::m_type = "string";


void VarString::set( std::string str )
{
    // If the value has changed, notify the observers
    if( m_value != str )
    {
        m_value = str;
        notify( NULL );
    }
}

