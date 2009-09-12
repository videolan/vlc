/*****************************************************************************
 * anchor.hpp
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

#ifndef ANCHOR_HPP
#define ANCHOR_HPP

#include "skin_common.hpp"
#include "generic_layout.hpp"
#include "../utils/bezier.hpp"
#include "../utils/position.hpp"


/// Class for the windows anchors
class Anchor: public SkinObject
{
public:
    Anchor( intf_thread_t *pIntf, const Position &rPosition, int range,
            int priority, const Bezier &rCurve, GenericLayout &rLayout )
          : SkinObject( pIntf ), m_position( rPosition ), m_rCurve( rCurve ),
            m_range( range ), m_priority( priority ), m_rLayout( rLayout ) { }
    virtual ~Anchor() { }

    /**
     * Return true if the given anchor is hanged by this one
     * Two conditions are required:
     *  - the other anchor must be in position of anchoring (as defined
     *    by canHang())
     *  - the priority of the other anchor must be lower than this one's
     */
    bool isHanging( const Anchor &rOther ) const;

    /**
     * Return true if the other anchor, moved by the (xOffset, yOffset)
     * vector, is "hangable" by this one (i.e. if it is in its range of
     * action), else return false.
     * When the function returns true, the xOffset and yOffset parameters
     * are modified to indicate the position that the other anchor would
     * take if hanged by this one (this position is calculated to minimize
     * the difference with the old xOffset and yOffset, so that the window
     * doesn't "jump" in a strange way).
     */
    bool canHang( const Anchor &rOther, int &xOffset, int &yOffset ) const;

    // Indicate whether this anchor is reduced to a single point
    bool isPoint() const { return m_rCurve.getNbCtrlPoints() == 1; }

    // Getters
    const Position & getPosition() const { return m_position; }

    int getXPosAbs() const
    {
        return (m_position.getLeft() + m_rLayout.getLeft());
    }

    int getYPosAbs() const
    {
        return (m_position.getTop() + m_rLayout.getTop());
    }

private:
    /// Position in the layout
    Position m_position;

    /// Curve of the anchor
    const Bezier &m_rCurve;

    /// Range of action
    int m_range;

    /// Priority
    int m_priority;

    /// Parent layout
    GenericLayout &m_rLayout;
};


#endif
