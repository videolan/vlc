/*
 * IsoffMainManager.h
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Jan 27, 2010
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

#ifndef ISOFFMAINMANAGER_H_
#define ISOFFMAINMANAGER_H_

#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "mpd/MPD.h"
#include "mpd/Period.h"
#include "mpd/AdaptationSet.h"
#include "mpd/Representation.h"
#include "mpd/SegmentInfo.h"
#include "mpd/Segment.h"
#include "mpd/IMPDManager.h"

namespace dash
{
    namespace mpd
    {
        class IsoffMainManager : public IMPDManager
        {
            public:
                IsoffMainManager            (MPD *mpd);
                virtual ~IsoffMainManager   ();

                const std::vector<Period *>&    getPeriods              () const;
                Period*                         getFirstPeriod          ();
                Period*                         getNextPeriod           (Period *period);
                Representation*                 getBestRepresentation   (Period *period);
                std::vector<Segment *>          getSegments             (const Representation *rep);
                Representation*                 getRepresentation       (Period *period, uint64_t bitrate) const;
                const MPD*                      getMPD                  () const;
                Representation*                 getRepresentation       (Period *period, uint64_t bitrate,
                                                                         int width, int height) const;

            private:
                MPD *mpd;
        };
    }
}

#endif /* ISOFFMAINMANAGER_H_ */
