/*****************************************************************************
 * anchor.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: anchor.cpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#include "anchor.hpp"


bool Anchor::isHanging( const Anchor &rOther ) const
{
    return (getXPosAbs() == rOther.getXPosAbs() &&
            getYPosAbs() == rOther.getYPosAbs() &&
            m_priority > rOther.m_priority );
}


bool Anchor::canHang( const Anchor &rOther, int &xOffset, int &yOffset ) const
{
    int xDist = getXPosAbs() - (rOther.getXPosAbs() + xOffset);
    int yDist = getYPosAbs() - (rOther.getYPosAbs() + yOffset);
    if( m_range > 0 && xDist*xDist + yDist*yDist <= m_range*m_range )
    {
        xOffset = getXPosAbs() - rOther.getXPosAbs();
        yOffset = getYPosAbs() - rOther.getYPosAbs();
        return true;
    }
    else
    {
        return false;
    }
}
