/*****************************************************************************
 * bezier.h: Functios to handle Bezier curves
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: bezier.h,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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


#ifndef VLC_SKIN_BEZIER
#define VLC_SKIN_BEZIER

//---------------------------------------------------------------------------
#define MAX_BEZIER_POINT    1023
#define BEZIER_PTS_ALL      0
#define BEZIER_PTS_Y        1
#define BEZIER_PTS_X        2



//---------------------------------------------------------------------------
class Bezier
{
    private:
        int maxpt;
        double *ptx;
        double *pty;
        double *ft;
        double melange( int i, int n, double t );
        double bezier_pty( double t );
        double bezier_ptx( double t );

        // Different points
        int Flag;
        int Max;
        int *Left;
        int *Top;

        // x^n
        double Power( double x, int n );

    public:
        // Constructor
        Bezier( double *x, double *y, int n, int flag = BEZIER_PTS_ALL );

        // Destructor
        ~Bezier();

        void GetPoint( double i, int &x, int &y );
        void GetDifferentPoints( int *x, int *y, int OffX = 0, int OffY = 0 );
        int  GetNumOfDifferentPoints();

};

//---------------------------------------------------------------------------
#endif
