/*****************************************************************************
 * bezier.cpp: Functions to handle Bezier curves
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: bezier.cpp,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

//--- SKIN ------------------------------------------------------------------
#include "bezier.h"



//---------------------------------------------------------------------------
// Inline methods : supposed to accelerate the code
//---------------------------------------------------------------------------
inline double Bezier::melange( int i, int n, double t )
{
    return Power( t, i ) * Power( 1 - t, (n - i) ) * ft[n] / ft[i] / ft[n - i];
}
//---------------------------------------------------------------------------
inline double Bezier::bezier_pty( double t )
{
    double res = 0;
    for( int i = 0; i <= maxpt; i++ )
    {
        res += pty[i] * melange( i, maxpt, t );
    }
    return res;
}
//---------------------------------------------------------------------------
inline double Bezier::bezier_ptx( double t )
{
    double res = 0;
    for( int i = 0; i <= maxpt; i++ )
    {
        res += ptx[i] * melange( i, maxpt, t );
    }
    return res;
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// BEZIER
// The bezier class generate bezier curves
//---------------------------------------------------------------------------
Bezier::Bezier( double *x, double *y, int n, int flag )
{
    int i;

    // x and y pointer are arrays of the coordinates of the points
    // n is the number of points

    // Allocation of ressources for arrays of points
    ptx = new double[n];
    pty = new double[n];

    // ft is factoriels
    // Here we create an array an precalculate them
    ft  = new double[n];
    ft[0] = 1;
    for( i = 0; i < n; i++ )
    {
        ptx[i] = x[i];    // assign values of coordinates
        pty[i] = y[i];
        if( i > 0 )
            ft[i] = i * ft[i - 1];
    }
    maxpt = n - 1;

    // FLAG values :
    //   - BEZIER_PTS_ALL : when x and y are differents
    //   - BEZIER_PTS_Y   : when only y is different
    //   - BEZIER_PTS_X   : when only x is different

    // Initialization
    Flag         = flag;
    Max          = 0;                 // Init number of pixels
    double Range = MAX_BEZIER_POINT;  // max number of pixel
    int last_i   = 0;
    int cx, cy, oldx, oldy;
    Left = new int[MAX_BEZIER_POINT + 1];
    Top  = new int[MAX_BEZIER_POINT + 1];

    // Calculate first point
    double per = 0;
    double j;
    GetPoint( per, oldx, oldy );
    Left[0] = oldx;
    Top[0]  = oldy;

    // Search for number of different points
    for( j = 1; j <= Range; j++ )
    {
        per = j / Range;
        GetPoint( per, cx, cy );
        if( ( Flag == BEZIER_PTS_ALL && ( cy != oldy || cx != oldx ) ) ||
            ( Flag == BEZIER_PTS_Y   && cy != oldy ) ||
            ( Flag == BEZIER_PTS_X   && cx != oldx ) )
        {
            Max++;
            Left[Max] = cx;
            Top[Max]  = cy;
            oldx = cx;
            oldy = cy;

            // Accelerator
            if( i - last_i > 2 )
            {
                i += i - last_i - 1;
            }
            last_i = i;
        }
    }
}
//---------------------------------------------------------------------------
Bezier::~Bezier()
{
    delete[] Left;
    delete[] Top;
    delete[] ptx;
    delete[] pty;
    delete[] ft;
}
//---------------------------------------------------------------------------
void Bezier::GetPoint( double i, int &x, int &y )
{
    // Get the coordinates of the point at i precent of
    // the curve (i must be between 0 and 1)
    x = (int)(float)bezier_ptx( i );
    y = (int)(float)bezier_pty( i );
}
//---------------------------------------------------------------------------
int Bezier::GetNumOfDifferentPoints()
{
    return Max;
}
//---------------------------------------------------------------------------
void Bezier::GetDifferentPoints( int *x, int *y, int OffX, int OffY )
{
    for( int i = 0; i <= Max; i++ )
    {
        x[i] = Left[i] + OffX;
        y[i] = Top[i]  + OffY;
    }
}
//---------------------------------------------------------------------------
double Bezier::Power( double x, int n )
{
    if( n > 0 )
        return x * Power( x, n - 1);
    else
        return 1;
}
//---------------------------------------------------------------------------

