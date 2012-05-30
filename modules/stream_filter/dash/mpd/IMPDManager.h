/*
 * IMPDManager.h
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef IMPDMANAGER_H_
#define IMPDMANAGER_H_

#include "mpd/Period.h"
#include "mpd/Representation.h"

namespace dash
{
    namespace mpd
    {
        class MPD;

        enum Profile
        {
            UnknownProfile,
            Full2011,
            Basic,
            BasicCM,
            IsoffMain
        };
        class IMPDManager
        {
            public:
                virtual ~IMPDManager(){}

                virtual const std::vector<Period *>&    getPeriods              () const                            = 0;
                virtual Period*                         getFirstPeriod          ()                                  = 0;
                virtual Period*                         getNextPeriod           (Period *period)                    = 0;
                virtual Representation*                 getBestRepresentation   (Period *period)                    = 0;
                virtual std::vector<Segment *>          getSegments             (const Representation *rep)         = 0;
                virtual Representation*                 getRepresentation       (Period *period, uint64_t bitrate) const = 0;
                virtual const MPD*                      getMPD                  () const                            = 0;
                virtual Representation*                 getRepresentation       (Period *period, uint64_t bitrate,
                                                                                 int width, int height) const       = 0;
        };
    }
}
#endif /* IMPDMANAGER_H_ */
