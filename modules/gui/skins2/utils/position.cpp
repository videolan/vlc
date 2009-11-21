/*****************************************************************************
 * position.cpp
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

#include "position.hpp"


const string VarBox::m_type = "box";


SkinsRect::SkinsRect( int left, int top, int right, int bottom ):
    m_left( left ), m_top( top ), m_right( right ), m_bottom( bottom )
{
}


Position::Position( int left, int top, int right, int bottom,
                    const GenericRect &rRect,
                    Ref_t refLeftTop, Ref_t refRightBottom,
                    bool xKeepRatio, bool yKeepRatio ):
    m_left( left ), m_top( top ), m_right( right ), m_bottom( bottom ),
    m_rRect( rRect ), m_refLeftTop( refLeftTop ),
    m_refRighBottom( refRightBottom ), m_xKeepRatio( xKeepRatio ),
    m_yKeepRatio( yKeepRatio )
{
    // Here is how the resizing algorithm works:
    //
    //  - if we "keep the ratio" (xkeepratio="true" in the XML), the relative
    //    position of the control in the parent box (i.e. the given rRect) is
    //    saved, and will be kept constant. The size of the control will not
    //    be changed, only its position may vary. To do that, we consider the
    //    part of the box to the left of the control (for an horizontal
    //    resizing) and the part of the box to the right of the control,
    //    and we make sure that the ratio between their widths is constant.
    //
    //  - if we don't keep the ratio, the resizing algorithm is completely
    //    different. We consider that the top left hand corner of the control
    //    ("lefttop" attribute in the XML) is linked to one of the 4 corners
    //    of the parent box ("lefttop", "leftbottom", "righttop" and
    //    "rightbottom" values for the attribute). Same thing for the bottom
    //    right hand corner ("rightbottom" attribute). When resizing occurs,
    //    the linked corners will move together, and this will drive the
    //    moving/resizing of the control.

    // Initialize the horizontal ratio
    if( m_xKeepRatio )
    {
        // First compute the width of the box minus the width of the control
        int freeSpace = m_rRect.getWidth() - (m_right - m_left);
        // Instead of computing left/right, we compute left/(left+right),
        // which is more convenient in my opinion.
        if( freeSpace != 0 )
        {
            m_xRatio = (double)m_left / (double)freeSpace;
        }
        else
        {
            // If the control has the same size as the box, we can't compute
            // the ratio in the same way (otherwise we would divide by zero).
            // So we consider that the intent was to keep the control centered
            // (if you are unhappy with this, go and fix your skin :))
            m_xRatio = 0.5;
        }
    }

    // Initial the vertical ratio
    if( m_yKeepRatio )
    {
        // First compute the height of the box minus the height of the control
        int freeSpace = m_rRect.getHeight() - (m_bottom - m_top);
        // Instead of computing top/bottom, we compute top/(top+bottom),
        // which is more convenient in my opinion.
        if( freeSpace != 0 )
        {
            m_yRatio = (double)m_top / (double)freeSpace;
        }
        else
        {
            // If the control has the same size as the box, we can't compute
            // the ratio in the same way (otherwise we would divide by zero).
            // So we consider that the intent was to keep the control centered
            // (if you are unhappy with this, go and fix your skin :))
            m_yRatio = 0.5;
        }
    }

}


int Position::getLeft() const
{
    if( m_xKeepRatio )
    {
        // Ratio mode
        // First compute the width of the box minus the width of the control
        int freeSpace = m_rRect.getWidth() - (m_right - m_left);
        return m_rRect.getLeft() + (int)(m_xRatio * freeSpace);
    }
    else
    {
        switch( m_refLeftTop )
        {
        case kLeftTop:
        case kLeftBottom:
            return m_rRect.getLeft() + m_left;
            break;
        case kRightTop:
        case kRightBottom:
            return m_rRect.getLeft() + m_rRect.getWidth() + m_left - 1;
            break;
        }
        // Avoid a warning
        return 0;
    }
}


int Position::getTop() const
{
    if( m_yKeepRatio )
    {
        // Ratio mode
        // First compute the height of the box minus the height of the control
        int freeSpace = m_rRect.getHeight() - (m_bottom - m_top);
        return m_rRect.getTop() + (int)(m_yRatio * freeSpace);
    }
    else
    {
        switch( m_refLeftTop )
        {
            case kLeftTop:
            case kRightTop:
                return m_rRect.getTop() + m_top;
                break;
            case kRightBottom:
            case kLeftBottom:
                return m_rRect.getTop() + m_rRect.getHeight() + m_top - 1;
                break;
        }
        // Avoid a warning
        return 0;
    }
}


int Position::getRight() const
{
    if( m_xKeepRatio )
    {
        // Ratio mode
        // The width of the control being constant, we can use the result of
        // getLeft() (this will avoid rounding issues).
        return getLeft() + m_right - m_left;
    }
    else
    {
        switch( m_refRighBottom )
        {
            case kLeftTop:
            case kLeftBottom:
                return m_rRect.getLeft() + m_right;
                break;
            case kRightTop:
            case kRightBottom:
                return m_rRect.getLeft() + m_rRect.getWidth() + m_right - 1;
                break;
        }
        // Avoid a warning
        return 0;
    }
}


int Position::getBottom() const
{
    if( m_yKeepRatio )
    {
        // Ratio mode
        // The height of the control being constant, we can use the result of
        // getTop() (this will avoid rounding issues).
        return getTop() + m_bottom - m_top;
    }
    else
    {
        switch( m_refRighBottom )
        {
            case kLeftTop:
            case kRightTop:
                return m_rRect.getTop() + m_bottom;
                break;
            case kLeftBottom:
            case kRightBottom:
                return m_rRect.getTop() + m_rRect.getHeight() + m_bottom - 1;
                break;
        }
        // Avoid a warning
        return 0;
    }
}


int Position::getWidth() const
{
    return getRight() - getLeft() + 1;
}


int Position::getHeight() const
{
    return getBottom() - getTop() + 1;
}


VarBox::VarBox( intf_thread_t *pIntf, int width, int height ):
    Variable( pIntf ), m_width( width ), m_height( height )
{
}


int VarBox::getWidth() const
{
    return m_width;
}


int VarBox::getHeight() const
{
    return m_height;
}


void VarBox::setSize( int width, int height )
{
    m_width = width;
    m_height = height;
    notify();
}

