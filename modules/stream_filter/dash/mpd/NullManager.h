/*
 * NullManager.h
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Apr 20, 2011
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef NULLMANAGER_H_
#define NULLMANAGER_H_

#include "mpd/IMPDManager.h"

#include "mpd/MPD.h"
#include "mpd/Period.h"
#include "mpd/Representation.h"
#include "mpd/ISegment.h"
#include "mpd/IMPDManager.h"

namespace dash
{
    namespace mpd
    {
        class NullManager : public IMPDManager
        {
            public:
                const std::vector<Period *>&   getPeriods              () const;
                Period*                 getFirstPeriod          ();
                Period*                 getNextPeriod           (Period *period);
                Representation*         getBestRepresentation   (Period *period);
                std::vector<ISegment *> getSegments             (Representation *rep);
                Representation*         getRepresentation       (Period *period, long bitrate);
                const MPD*              getMPD                  () const;
            private:
                std::vector<Period *>   periods;
                std::vector<ISegment *> segments;
        };
    }
}

#endif /* NULLMANAGER_H_ */
