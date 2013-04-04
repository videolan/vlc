/*****************************************************************************
 * pointer.hpp
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

#ifndef POINTER_HPP
#define POINTER_HPP


/// Reference couting pointer
template <class T> class CountedPtr
{
public:
    typedef T *pointer;
    typedef T &reference;

    explicit CountedPtr( pointer pPtr = 0 ): m_pCounter( 0 )
    {
        if( pPtr ) m_pCounter = new Counter( pPtr );
    }

    ~CountedPtr() { release(); }

    CountedPtr( const CountedPtr &rPtr ) { acquire( rPtr.m_pCounter ); }

    CountedPtr &operator=( const CountedPtr &rPtr )
    {
        if( this != &rPtr )
        {
            release();
            acquire( rPtr.m_pCounter );
        }
        return *this;
    }

    // XXX Somebody explain why operator* and operator-> don't use get()
    reference operator*() const { return *m_pCounter->m_pPtr; }
    pointer   operator->() const { return m_pCounter->m_pPtr; }

    pointer get() const { return m_pCounter ? m_pCounter->m_pPtr : 0; }

    bool unique() const
    {
        return ( m_pCounter ? m_pCounter->m_count == 1 : true );
    }

private:
    struct Counter
    {
        Counter( pointer pPtr = 0, unsigned int c = 1 )
               : m_pPtr( pPtr ), m_count( c ) { }
        pointer m_pPtr;
        unsigned int m_count;
    } *m_pCounter;

    void acquire( Counter* pCount )
    {
        m_pCounter = pCount;
        if( pCount ) ++pCount->m_count;
    }

    void release()
    {
        if( m_pCounter )
        {
            if( --m_pCounter->m_count == 0 )
            {
                delete m_pCounter->m_pPtr;
                delete m_pCounter;
            }
            m_pCounter = 0;
        }
    }
};

#endif
