/*****************************************************************************
 * bezier.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: bezier.cpp,v 1.2 2004/01/11 17:12:17 asmax Exp $
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

#include "bezier.hpp"
#include <math.h>


Bezier::Bezier( intf_thread_t *p_intf, const vector<float> &rAbscissas,
                const vector<float> &rOrdinates, Flag_t flag )
    : SkinObject( p_intf )
{
    // We expect rAbscissas and rOrdinates to have the same size, of course
    m_nbCtrlPt = rAbscissas.size();

    // Copy the control points coordinates
    m_ptx.assign( rAbscissas.begin(), rAbscissas.end() );
    m_pty.assign( rOrdinates.begin(), rOrdinates.end() );

    // Precalculate the factoriels
    m_ft.push_back( 1 );
    for( int i = 1; i < m_nbCtrlPt; i++ )
    {
        m_ft.push_back( i * m_ft[i - 1] );
    }

    // Initialization
    int cx, cy, oldx, oldy;
    m_leftVect.reserve( MAX_BEZIER_POINT + 1 );
    m_topVect.reserve( MAX_BEZIER_POINT + 1 );

    // Calculate the first point
    getPoint( 0, oldx, oldy );
    m_leftVect[0] = oldx;
    m_topVect[0]  = oldy;

    // Compute the number of different points
    float percentage;
    for( float j = 1; j <= MAX_BEZIER_POINT; j++ )
    {
        percentage = j / MAX_BEZIER_POINT;
        getPoint( percentage, cx, cy );
        if( ( flag == kCoordsBoth && ( cx != oldx || cy != oldy ) ) ||
            ( flag == kCoordsX && cx != oldx ) ||
            ( flag == kCoordsY && cy != oldy ) )
        {
            m_leftVect.push_back( cx );
            m_topVect.push_back( cy );
            oldx = cx;
            oldy = cy;
        }
    }
    m_nbPoints = m_leftVect.size();
}


float Bezier::getNearestPercent( int x, int y ) const
{
    int nearest = findNearestPoint( x, y );
    return (float)nearest / (float)(m_nbPoints - 1);
}


float Bezier::getMinDist( int x, int y ) const
{
    // XXX: duplicate code with findNearestPoint
    int minDist = (m_leftVect[0] - x) * (m_leftVect[0] - x) +
                  (m_topVect[0] - y) * (m_topVect[0] - y);

    int dist;
    for( int i = 1; i < m_nbPoints; i++ )
    {
        dist = (m_leftVect[i] - x) * (m_leftVect[i] - x) +
               (m_topVect[i] - y) * (m_topVect[i] - y);
        if( dist < minDist )
        {
            minDist = dist;
        }
    }
    return sqrt( minDist );
}


void Bezier::getPoint( float t, int &x, int &y ) const
{
    // See http://astronomy.swin.edu.au/~pbourke/curves/bezier/ for a simple
    // explanation of the algorithm
    float xPos = 0;
    float yPos = 0;
    float coeff;
    for( int i = 0; i < m_nbCtrlPt; i++ )
    {
        coeff = computeCoeff( i, m_nbCtrlPt - 1, t );
        xPos += m_ptx[i] * coeff;
        yPos += m_pty[i] * coeff;
    }

    // float cast to avoid strange truncatures
    // XXX: not very nice...
    x = (int)(float)xPos;
    y = (int)(float)yPos;
}


int Bezier::getWidth() const
{
    int width = 0;
    for( int i = 0; i < m_nbPoints; i++ )
    {
        if( m_leftVect[i] > width )
        {
            width = m_leftVect[i];
        }
    }
    return width;
}


int Bezier::getHeight() const
{
    int height = 0;
    for( int i = 0; i < m_nbPoints; i++ )
    {
        if( m_topVect[i] > height )
        {
            height = m_topVect[i];
        }
    }
    return height;
}


int Bezier::findNearestPoint( int x, int y ) const
{
    // XXX: duplicate code with getMinDist
    // The distance to the first point is taken as the reference
    int refPoint = 0;
    int minDist = (m_leftVect[0] - x) * (m_leftVect[0] - x) +
                  (m_topVect[0] - y) * (m_topVect[0] - y);

    int dist;
    for( int i = 1; i < m_nbPoints; i++ )
    {
        dist = (m_leftVect[i] - x) * (m_leftVect[i] - x) +
               (m_topVect[i] - y) * (m_topVect[i] - y);
        if( dist < minDist )
        {
            minDist = dist;
            refPoint = i;
        }
    }

    return refPoint;
}


inline float Bezier::computeCoeff( int i, int n, float t ) const
{
    return (power( t, i ) * power( 1 - t, (n - i) ) *
        (m_ft[n] / m_ft[i] / m_ft[n - i]));
}


inline float Bezier::power( float x, int n ) const
{
    if( n > 0 )
        return x * power( x, n - 1);
    else
        return 1;
}

