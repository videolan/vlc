/*****************************************************************************
 * bezier.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "bezier.hpp"
#include <math.h>

// XXX should be in VLC core
#ifndef HAVE_LRINTF
#   ifdef HAVE_LRINT
#       define lrintf( x ) (int)rint( x )
#   elif defined _WIN32
        __inline long int lrintf( float x )
        {
            int i;
            _asm fld x __asm fistp i
            return i;
        }
#   endif
#endif

Bezier::Bezier( intf_thread_t *p_intf, const std::vector<float> &rAbscissas,
                const std::vector<float> &rOrdinates, Flag_t flag )
    : SkinObject( p_intf )
{
    // Copy the control points coordinates
    m_ptx.assign( rAbscissas.begin(), rAbscissas.end() );
    m_pty.assign( rOrdinates.begin(), rOrdinates.end() );

    // We expect m_ptx and m_pty to have the same size, of course
    m_nbCtrlPt = m_ptx.size();

    // Precalculate the factoriels
    m_ft.push_back( 1 );
    for( int i = 1; i < m_nbCtrlPt; i++ )
    {
        m_ft.push_back( i * m_ft[i - 1] );
    }

    // Calculate the first point
    int oldx, oldy;
    computePoint( 0, oldx, oldy );
    m_leftVect.push_back( oldx );
    m_topVect.push_back( oldy );
    m_percVect.push_back( 0 );

    // Calculate the other points
    float percentage;
    int cx, cy;
    for( float j = 1; j <= MAX_BEZIER_POINT; j++ )
    {
        percentage = j / MAX_BEZIER_POINT;
        computePoint( percentage, cx, cy );
        if( ( flag == kCoordsBoth && ( cx != oldx || cy != oldy ) ) ||
            ( flag == kCoordsX && cx != oldx ) ||
            ( flag == kCoordsY && cy != oldy ) )
        {
            m_percVect.push_back( percentage );
            m_leftVect.push_back( cx );
            m_topVect.push_back( cy );
            oldx = cx;
            oldy = cy;
        }
    }
    m_nbPoints = m_leftVect.size();

    // If we have only one control point, we duplicate it
    // This allows simplifying the algorithms used in the class
    if( m_nbPoints == 1 )
    {
        m_leftVect.push_back( m_leftVect[0] );
        m_topVect.push_back( m_topVect[0] );
        m_percVect.push_back( 1 );
        m_nbPoints = 2;
   }

    // Ensure that the percentage of the last point is always 1
    m_percVect[m_nbPoints - 1] = 1;
}


float Bezier::getNearestPercent( int x, int y ) const
{
    int nearest = findNearestPoint( x, y );
    return m_percVect[nearest];
}


float Bezier::getMinDist( int x, int y, float xScale, float yScale ) const
{
    int nearest = findNearestPoint( x, y );
    double xDist = xScale * (m_leftVect[nearest] - x);
    double yDist = yScale * (m_topVect[nearest] - y);
    return sqrt( xDist * xDist + yDist * yDist );
}


void Bezier::getPoint( float t, int &x, int &y ) const
{
    // Find the precalculated point whose percentage is nearest from t
    int refPoint = 0;
    float minDiff = fabs( m_percVect[0] - t );

    // The percentages are stored in increasing order, so we can stop the loop
    // as soon as 'diff' starts increasing
    float diff;
    while( refPoint < m_nbPoints &&
           (diff = fabs( m_percVect[refPoint] - t )) <= minDiff )
    {
        refPoint++;
        minDiff = diff;
    }

    // The searched point is then (refPoint - 1)
    // We know that refPoint > 0 because we looped at least once
    x = m_leftVect[refPoint - 1];
    y = m_topVect[refPoint - 1];
}


int Bezier::getWidth() const
{
    int width = 0;
    for( int i = 0; i < m_nbPoints; i++ )
    {
        if( m_leftVect[i] >= width )
        {
            width = m_leftVect[i] + 1;
        }
    }
    return width;
}


int Bezier::getHeight() const
{
    int height = 0;
    for( int i = 0; i < m_nbPoints; i++ )
    {
        if( m_topVect[i] >= height )
        {
            height = m_topVect[i] + 1;
        }
    }
    return height;
}


int Bezier::findNearestPoint( int x, int y ) const
{
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


inline float Bezier::power( float x, int n )
{
#if 0
    return n <= 0 ? 1 : x * power( x, n - 1 );
#else
    return powf( x, n );
#endif
}


inline float Bezier::computeCoeff( int i, int n, float t ) const
{
    return (power( t, i ) * power( 1 - t, (n - i) ) *
        (m_ft[n] / m_ft[i] / m_ft[n - i]));
}


void Bezier::computePoint( float t, int &x, int &y ) const
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

    x = lrintf(xPos);
    y = lrintf(yPos);
}

