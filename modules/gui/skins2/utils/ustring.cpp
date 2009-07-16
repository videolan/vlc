/*****************************************************************************
 * ustring.cpp
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <sstream>
#include "ustring.hpp"


const uint32_t UString::npos = 0xffffffff;


UString::UString( const UString &rOther ): SkinObject( rOther.getIntf() )
{
    m_length = rOther.m_length;
    m_pString = new uint32_t[m_length + 1];
    memcpy( m_pString, rOther.m_pString, 4 * m_length + 4);
}


UString::UString( intf_thread_t *pIntf, const char *pString ):
    SkinObject( pIntf )
{
    // First we compute the length of the string
    const char *pCur = pString;
    for( m_length = 0; pCur && *pCur; m_length++ )
    {
        if( (*pCur & 0xfc) == 0xfc )
        {
            pCur += 6;
        }
        else if ( (*pCur & 0xf8 ) == 0xf8 )
        {
            pCur += 5;
        }
        else if ( (*pCur & 0xf0 ) == 0xf0 )
        {
            pCur += 4;
        }
        else if ( (*pCur & 0xe0 ) == 0xe0 )
        {
            pCur += 3;
        }
        else if ( (*pCur & 0xc0 ) == 0xc0 )
        {
            pCur += 2;
        }
        else
        {
            pCur++;
        }
    }
    if( !pCur || *pCur )
    {
        msg_Err( pIntf, "invalid UTF8 string: %s", pString );
        m_length = 0;
        m_pString = NULL;
        return;
    }

    m_pString = new uint32_t[m_length + 1];

    // Convert the UTF8 string into UNICODE
    pCur = pString;
    uint32_t aChar = 0;  // current unicode character
    int remaining = 0;   // remaining bytes
    for( uint32_t i = 0; i <= m_length; i++ )
    {
        if( (*pCur & 0xfc) == 0xfc )
        {
            aChar = *pCur & 1;
            remaining = 5;
        }
        else if ( (*pCur & 0xf8 ) == 0xf8 )
        {
            aChar = *pCur & 3;
            remaining = 4;
        }
        else if ( (*pCur & 0xf0 ) == 0xf0 )
        {
            aChar = *pCur & 7;
            remaining = 3;
        }
        else if ( (*pCur & 0xe0 ) == 0xe0 )
        {
            aChar = *pCur & 15;
            remaining = 2;
        }
        else if ( (*pCur & 0xc0 ) == 0xc0 )
        {
            aChar = *pCur & 31;
            remaining = 1;
        }
        else
        {
            aChar = *pCur;
            remaining = 0;
        }
        while( remaining )
        {
            pCur++;
            remaining--;
            aChar = ( aChar << 6 ) | ( *pCur & 0x3f );
        }
        m_pString[i] = aChar;
        pCur++;
    }
    m_pString[m_length] = 0;
}


UString::~UString()
{
    delete[] m_pString;
}


bool UString::operator ==( const UString &rOther ) const
{
    if( size() != rOther.size() )
    {
        return false;
    }

    for( uint32_t i = 0; i < size(); i++ )
    {
        if( m_pString[i] != rOther.m_pString[i] )
        {
            return false;
        }
    }

    return true;
}


bool UString::operator !=( const UString &rOther ) const
{
    return !(*this == rOther);
}


bool UString::operator <( const UString &rOther ) const
{
    const uint32_t *pOther = rOther.u_str();
    uint32_t i;
    for( i = 0; i < __MIN(m_length, rOther.length()); i++ )
    {
        if( m_pString[i] < pOther[i] )
        {
            return true;
        }
        else if( m_pString[i] > pOther[i] )
        {
            return false;
        }
    }
    return( m_pString[i] < pOther[i] );
}


bool UString::operator <=( const UString &rOther ) const
{
    return !( rOther < *this );
}


bool UString::operator >( const UString &rOther ) const
{
    return ( rOther < *this );
}


bool UString::operator >=( const UString &rOther ) const
{
    return !( *this < rOther );
}


UString& UString::operator =( const UString &rOther )
{
    if( this == &rOther )
        return *this;

    m_length = rOther.m_length;
    delete[] m_pString;
    m_pString = new uint32_t[size() + 1];
    for( uint32_t i = 0; i <= size(); i++ )
    {
        m_pString[i] = rOther.m_pString[i];
    }

    return *this;
}


UString& UString::operator +=( const UString &rOther )
{
    if( this == &rOther )
        return *this;

    int tempLength = this->length() + rOther.length();
    uint32_t *pTempString = new uint32_t[tempLength + 1];
    // Copy the first string
    memcpy( pTempString, this->m_pString, sizeof(uint32_t) * this->size() );
    // Append the second string
//     memcpy( pTempString + 4 * size(), rOther.m_pString,
//             4 * rOther.size() );
    for( uint32_t i = 0; i < rOther.size(); i++ )
    {
        pTempString[this->size() + i] = rOther.m_pString[i];
    }
    pTempString[tempLength] = 0;

    // Set the string internally
    delete[] m_pString;
    m_pString = pTempString;
    m_length = tempLength;

    return *this;
}


const UString UString::operator +( const UString &rOther ) const
{
    UString result( *this );
    result += rOther;

    return result;
}


const UString UString::operator +( const char *pString ) const
{
    UString temp( getIntf(), pString );
    return (*this + temp );
}


uint32_t UString::find( const UString &str, uint32_t position ) const
{
    uint32_t pos;
    for( pos = position; pos + str.size() <= size(); pos++ )
    {
        bool match = true;
        for( uint32_t i = 0; i < str.size(); i++ )
        {
            if( m_pString[pos + i] != str.m_pString[i] )
            {
                match = false;
                break;
            }
        }

        // Found
        if( match )
        {
            return pos;
        }
    }

    // Not found
    return npos;
}


uint32_t UString::find( const char *pString, uint32_t position ) const
{
    UString tmp( getIntf(), pString );
    return find( tmp, position );
}


void UString::replace( uint32_t position, uint32_t n1, const UString &str )
{
    UString start( substr( 0, position ) );
    UString end( substr( position + n1 ) );
    *this = start + str + end;
}


void UString::replace( uint32_t position, uint32_t n1, const char *pString )
{
    replace( position, n1, UString( getIntf(), pString ) );
}


UString UString::substr( uint32_t position, uint32_t n) const
{
    UString tmp( getIntf(), "" );
    if( position > size() )
    {
        msg_Err( getIntf(), "invalid position in UString::substr()" );
        return tmp;
    }
    tmp.m_length = (n < size() - position) ? n : size() - position;
    delete[] tmp.m_pString;
    tmp.m_pString = new uint32_t[tmp.size() + 1];
    for( uint32_t i = 0; i < tmp.size(); i++ )
    {
        tmp.m_pString[i] = m_pString[position + i];
    }

    return tmp;
}


UString UString::fromInt( intf_thread_t *pIntf, int number)
{
    stringstream ss;
    ss << number;
    return UString( pIntf, ss.str().c_str() );
}

