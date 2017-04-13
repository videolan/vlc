/*
 * AbstractAdaptationLogic.h
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

#ifndef ABSTRACTADAPTATIONLOGIC_H_
#define ABSTRACTADAPTATIONLOGIC_H_

#include "IDownloadRateObserver.h"
#include "../SegmentTracker.hpp"

namespace adaptive
{
    namespace playlist
    {
        class BaseRepresentation;
        class BaseAdaptationSet;
    }

    namespace logic
    {
        using namespace playlist;

        class AbstractAdaptationLogic : public IDownloadRateObserver,
                                        public SegmentTrackerListenerInterface
        {
            public:
                AbstractAdaptationLogic             ();
                virtual ~AbstractAdaptationLogic    ();

                virtual BaseRepresentation* getNextRepresentation(BaseAdaptationSet *, BaseRepresentation *) = 0;
                virtual void                updateDownloadRate     (const ID &, size_t, mtime_t);
                virtual void                trackerEvent           (const SegmentTrackerEvent &) {}
                void                        setMaxDeviceResolution (int, int);

                enum LogicType
                {
                    Default = 0,
                    AlwaysBest,
                    AlwaysLowest,
                    RateBased,
                    FixedRate,
                    Predictive,
                    NearOptimal,
                };

            protected:
                int maxwidth;
                int maxheight;
        };
    }
}

#endif /* ABSTRACTADAPTATIONLOGIC_H_ */
