/*****************************************************************************
 * bezier.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: bezier.hpp,v 1.1 2004/01/03 23:31:34 asmax Exp $
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

#ifndef BEZIER_HPP
#define BEZIER_HPP

#include "../src/skin_common.hpp"
#include <vector>
using namespace std;

#define MAX_BEZIER_POINT 1023


/// Class for Bezier curves
class Bezier: public SkinObject
{
    public:
        /// Values to indicate which coordinate(s) must be checked to consider
        /// that two points are distinct
        typedef enum
        {
            kCoordsBoth,    // x or y must be different (default)
            kCoordsX,       // only x is different
            kCoordsY        // only y is different
        } Flag_t;

        Bezier( intf_thread_t *p_intf,
                const vector<double> &pAbscissas,
                const vector<double> &pOrdinates,
                Flag_t flag = kCoordsBoth );
        ~Bezier() {}

        /// Return the percentage (between 0 and 1) of the curve point nearest
        /// from (x, y)

        double getNearestPercent( int x, int y ) const;

        /// Return the distance of (x, y) to the curve
        double getMinDist( int x, int y ) const;

        /// Get the coordinates of the point at t precent of
        /// the curve (t must be between 0 and 1)
        void getPoint( double t, int &x, int &y ) const;

        /// Get the width (maximum abscissa) of the curve
        int getWidth() const;

        /// Get the height (maximum ordinate) of the curve
        int getHeight() const;

    private:
        /// Number of control points
        int m_nbCtrlPt;
        /// vectors containing the coordinates of the control points
        vector<double> m_ptx;
        vector<double> m_pty;
        /// Vector containing precalculated factoriels
        vector<double> m_ft;

        /// Number of points (=pixels) used by the curve
        int m_nbPoints;
        /// Vectors with the coordinates of the different points of the curve
        vector<int> m_leftVect;
        vector<int> m_topVect;

        /// Return the index of the curve point that is the nearest from (x, y)
        int findNearestPoint( int x, int y ) const;
        /// Helper function to compute a coefficient of the curve
        inline double computeCoeff( int i, int n, double t ) const;
        /// x^n
        inline double power( double x, int n ) const;
};


#endif
