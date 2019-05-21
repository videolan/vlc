/*
 * RateBasedAdaptationLogic.h
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

#ifndef RATEBASEDADAPTATIONLOGIC_H_
#define RATEBASEDADAPTATIONLOGIC_H_

#include "AbstractAdaptationLogic.h"
#include "../tools/MovingAverage.hpp"

namespace adaptive
{
    namespace logic
    {

        class RateBasedAdaptationLogic : public AbstractAdaptationLogic
        {
            public:
                RateBasedAdaptationLogic            (vlc_object_t *);
                virtual ~RateBasedAdaptationLogic   ();

                BaseRepresentation *getNextRepresentation(BaseAdaptationSet *, BaseRepresentation *);
                virtual void updateDownloadRate(const ID &, size_t, vlc_tick_t); /* reimpl */
                virtual void trackerEvent(const SegmentTrackerEvent &); /* reimpl */

            private:
                size_t                  bpsAvg;
                size_t                  currentBps;
                size_t                  usedBps;

                MovingAverage<size_t>   average;

                size_t                  dlsize;
                vlc_tick_t              dllength;

                mutable vlc_mutex_t     lock;
        };

        class FixedRateAdaptationLogic : public AbstractAdaptationLogic
        {
            public:
                FixedRateAdaptationLogic(vlc_object_t *, size_t);

                BaseRepresentation *getNextRepresentation(BaseAdaptationSet *, BaseRepresentation *);

            private:
                size_t                  currentBps;
        };
    }
}

#endif /* RATEBASEDADAPTATIONLOGIC_H_ */
