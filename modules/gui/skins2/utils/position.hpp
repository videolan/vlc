/*****************************************************************************
 * position.hpp
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef POSITION_HPP
#define POSITION_HPP


/// Interface for rectangular objects
class Box
{
    public:
        /// Get the size of the box
        virtual int getWidth() const = 0;
        virtual int getHeight() const = 0;
};


/// Characterization of a rectangle
class Rect: public Box
{
    public:
        Rect( int left, int top, int right, int bottom );

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
class Position
{
    public:
        /// Type for reference edge/corner
        typedef enum
        {
            /// Coordinates are relative to the upper left corner
            kLeftTop,
            /// Coordinates are relative to the upper right corner
            kRightTop,
            /// Coordinates are relative to the lower left corner
            kLeftBottom,
            /// Coordinates are relative to the lower right corner
            kRightBottom
        } Ref_t;

        /// Create a new position relative to the given box
        Position( int left, int top, int right, int bottom, const Box &rBox,
                  Ref_t refLeftTop = kLeftTop,
                  Ref_t refRightBottom = kLeftTop );

        ~Position() {}

        /// Get the position relative to the left top corner of the box
        int getLeft() const;
        int getTop() const;
        int getRight() const;
        int getBottom() const;
        /// Get the size of the rectangle
        int getWidth() const;
        int getHeight() const;

    private:
        /// Position and reference edge/corner
        int m_left;
        int m_top;
        int m_right;
        int m_bottom;
        const Box &m_rBox;
        Ref_t m_refLeftTop;
        Ref_t m_refRighBottom;
};


#endif
