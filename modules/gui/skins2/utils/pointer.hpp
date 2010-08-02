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


class rect
{
public:
    rect( int v_x = 0, int v_y = 0, int v_width = 0, int v_height = 0 )
        : x( v_x ), y( v_y ), width( v_width ), height( v_height ) { }
    ~rect() { }
    int x;
    int y;
    int width;
    int height;

    // rect2 fully included in rect1
    static bool isIncluded( const rect& rect2, const rect& rect1 )
    {
        int x1 = rect1.x;
        int y1 = rect1.y;
        int w1 = rect1.width;
        int h1 = rect1.height;

        int x2 = rect2.x;
        int y2 = rect2.y;
        int w2 = rect2.width;
        int h2 = rect2.height;

        return     x2 >= x1 && x2 + w2 <= x1 + w1
               &&  y2 >= y1 && y2 + h2 <= y1 + h1;
    }

    static bool areDisjunct( const rect& rect2, const rect& rect1 )
    {
        int x1 = rect1.x;
        int y1 = rect1.y;
        int w1 = rect1.width;
        int h1 = rect1.height;

        int x2 = rect2.x;
        int y2 = rect2.y;
        int w2 = rect2.width;
        int h2 = rect2.height;

        return    y2 + h2 -1 < y1  // rect2 above rect1
               || y2 > y1 + h1 - 1  // rect2 under rect1
               || x2 > x1 + w1 -1  // rect2 right of rect1
               || x2 + w2 - 1 < x1; // rect2 left of rect1
    }

    static bool intersect( const rect& rect1, const rect& rect2, rect* pRect )
    {
        int x1 = rect1.x;
        int y1 = rect1.y;
        int w1 = rect1.width;
        int h1 = rect1.height;

        int x2 = rect2.x;
        int y2 = rect2.y;
        int w2 = rect2.width;
        int h2 = rect2.height;

        if( areDisjunct( rect1, rect2 ) )
            return false;
        else
        {
            int left = max( x1, x2 );
            int right = min( x1 + w1 - 1, x2 + w2 - 1 );
            int top = max( y1, y2 );
            int bottom = min( y1 + h1 - 1, y2 + h2 -1 );
            pRect->x = left;
            pRect->y = top;
            pRect->width = right - left + 1;
            pRect->height = bottom - top + 1;

            return pRect->width > 0 && pRect->height > 0;
        }
    }

    static bool join( const rect& rect1, const rect& rect2, rect* pRect )
    {
        int x1 = rect1.x;
        int y1 = rect1.y;
        int w1 = rect1.width;
        int h1 = rect1.height;

        int x2 = rect2.x;
        int y2 = rect2.y;
        int w2 = rect2.width;
        int h2 = rect2.height;

        int left = min( x1, x2 );
        int right = max( x1 + w1 - 1, x2 + w2 - 1 );
        int top = min( y1, y2 );
        int bottom = max( y1 + h1 - 1, y2 + h2 -1 );
        pRect->x = left;
        pRect->y = top;
        pRect->width = right - left + 1;
        pRect->height = bottom - top + 1;

        return pRect->width > 0 && pRect->height > 0;
    }
    static int min( int x, int y ) { return x < y ? x : y; }
    static int max( int x, int y ) { return x < y ? y : x; }

};


#endif
