/*****************************************************************************
 * ustring.hpp
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

#ifndef USTRING_HPP
#define USTRING_HPP

#include "../src/skin_common.hpp"
#include "pointer.hpp"


// Class for UNICODE strings handling
class UString: public SkinObject
{
public:
    static const uint32_t npos;

    /// Copy constructor
    UString( const UString &rOther );

    /// Create a new unicode string from an UTF8 string
    UString( intf_thread_t *pIntf, const char *pString );

    ~UString();

    /// Get the unicode string
    const uint32_t *u_str() const { return m_pString; }

    /// Get the length of the string
    uint32_t length() const { return m_length; }
    uint32_t size() const { return m_length; }

    /// Comparison
    bool operator ==( const UString &rOther ) const;
    bool operator !=( const UString &rOther ) const;
    bool operator <( const UString &rOther ) const;
    bool operator <=( const UString &rOther ) const;
    bool operator >( const UString &rOther ) const;
    bool operator >=( const UString &rOther ) const;
    /// Assignment
    UString& operator =( const UString &rOther );
    /// Concatenation with assignment
    UString& operator +=( const UString &rOther );
    /// Concatenation
    const UString operator +( const UString &rOther ) const;
    const UString operator +( const char *pString ) const;


    /// Search for the first occurance of the substring specified by str
    /// in this string, starting at position. If found, it returns the
    /// index of the first character of the matching substring. If not
    /// found, it returns npos
    uint32_t find( const UString &str, uint32_t position = 0 ) const;
    uint32_t find( const char *pString, uint32_t position = 0 ) const;

    /// Insert elements of str in place of n1 elements in this string,
    /// starting at position position
    void replace( uint32_t position, uint32_t n1, const UString &str );
    void replace( uint32_t position, uint32_t n1, const char *pString );

    /// Returns a string composed of copies of the lesser of n and size()
    /// characters in this string starting at index position
    UString substr( uint32_t position = 0, uint32_t n = npos) const;

    /// Build a string from an integer
    static UString fromInt(intf_thread_t *pIntf, int number);

private:
    /// Unicode string
    uint32_t *m_pString;
    /// String length
    uint32_t m_length;
};


typedef CountedPtr<UString> UStringPtr;

#endif
