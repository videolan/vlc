/*****************************************************************************
 * position.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#ifndef POSITION_HPP
#define POSITION_HPP

#include "variable.hpp"
#include "observer.hpp"
#include "pointer.hpp"


/// Interface for rectangular objects
class Box
{
public:
    virtual ~Box() { }

    /// Get the size of the box
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};


/// Interface for rectangular objects with a position
class GenericRect: public Box
{
public:
    virtual int getLeft() const = 0;
    virtual int getTop() const = 0;
};


/// Characterization of a rectangle
class SkinsRect: public GenericRect
{
public:
    SkinsRect( int left, int top, int right, int bottom );

    virtual int getLeft() const { return m_left; }
    virtual int getTop() const { return m_top; }
    virtual int getRight() const { return m_right; }
    virtual int getBottom() const { return m_bottom; }
    virtual int getWidth() const { return m_right - m_left; }
    virtual int getHeight() const { return m_bottom - m_top; }

private:
    int m_left;
    int m_top;
    int m_right;
    int m_bottom;
};


/// Relative position of a rectangle in a box
/**
 * Note: Even if the object is tied to its direct container rectangle, the
 * coordinates returned by getLeft(), getTop(), getRight() and getBottom()
 * are not relative to the direct container (which is usually a panel or
 * the layout) but to the root container (i.e. the layout).
 */
class Position: public GenericRect
{
public:
    /// Type for reference edge/corner
    enum Ref_t
    {
        /// Coordinates are relative to the upper left corner
        kLeftTop,
        /// Coordinates are relative to the upper right corner
        kRightTop,
        /// Coordinates are relative to the lower left corner
        kLeftBottom,
        /// Coordinates are relative to the lower right corner
        kRightBottom
    };

    /// Create a new position relative to the given box
    Position( int left, int top, int right, int bottom,
              const GenericRect &rRect,
              Ref_t refLeftTop, Ref_t refRightBottom,
              bool xKeepRatio, bool yKeepRatio );

    ~Position() { }

    /// Get the position relative to the left top corner of the box
    virtual int getLeft() const;
    virtual int getTop() const;
    int getRight() const;
    int getBottom() const;
    /// Get the size of the rectangle
    virtual int getWidth() const;
    virtual int getHeight() const;
    /// Get the reference corners
    Ref_t getRefLeftTop() const { return m_refLeftTop; }
    Ref_t getRefRightBottom() const { return m_refRighBottom; }

private:
    /// Position and reference edge/corner
    int m_left;
    int m_top;
    int m_right;
    int m_bottom;
    const GenericRect &m_rRect;
    Ref_t m_refLeftTop;
    Ref_t m_refRighBottom;
    /// "Keep ratio" mode
    bool m_xKeepRatio;
    bool m_yKeepRatio;
    /// Initial width ratio (usually between 0 and 1)
    double m_xRatio;
    /// Initial height ratio (usually between 0 and 1)
    double m_yRatio;
};

typedef CountedPtr<Position> PositionPtr;


/// Variable implementing the Box interface
class VarBox: public Variable, public Box, public Subject<VarBox>
{
public:
    VarBox( intf_thread_t *pIntf, int width = 0, int height = 0 );

    virtual ~VarBox() { }

    /// Get the variable type
    virtual const std::string &getType() const { return m_type; }

    /// Get the size of the box
    virtual int getWidth() const;
    virtual int getHeight() const;

    /// Change the size of the box
    void setSize( int width, int height );

private:
    /// Variable type
    static const std::string m_type;
    /// Size
    int m_width, m_height;
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

    bool operator==( const rect& other ) const
    {
        return x == other.x &&
               y == other.y &&
               width == other.width &&
               height == other.height;
    }
};

#endif
