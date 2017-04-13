/*
 * NearOptimalAdaptationLogic.hpp
 *****************************************************************************
 * Copyright (C) 2017 - VideoLAN Authors
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
#ifndef NEAROPTIMALADAPTATIONLOGIC_HPP
#define NEAROPTIMALADAPTATIONLOGIC_HPP

#include "AbstractAdaptationLogic.h"
#include "Representationselectors.hpp"
#include "../tools/MovingAverage.hpp"
#include <map>

namespace adaptive
{
    namespace logic
    {
        class NearOptimalContext
        {
            friend class NearOptimalAdaptationLogic;

            public:
                NearOptimalContext();

            private:
                mtime_t buffering_min;
                mtime_t buffering_level;
                mtime_t buffering_target;
                unsigned last_download_rate;
                MovingAverage<unsigned> average;
        };

        class NearOptimalAdaptationLogic : public AbstractAdaptationLogic
        {
            public:
                NearOptimalAdaptationLogic(vlc_object_t *);
                virtual ~NearOptimalAdaptationLogic();

                virtual BaseRepresentation* getNextRepresentation(BaseAdaptationSet *, BaseRepresentation *);
                virtual void                updateDownloadRate     (const ID &, size_t, mtime_t); /* reimpl */
                virtual void                trackerEvent           (const SegmentTrackerEvent &); /* reimpl */

            private:
                BaseRepresentation *        getNextQualityIndex( BaseAdaptationSet *, RepresentationSelector &,
                                                                 float gammaP, mtime_t VD,
                                                                 mtime_t Q /*current buffer level*/);
                float                       getUtility(const BaseRepresentation *);
                unsigned                    getAvailableBw(unsigned, const BaseRepresentation *) const;
                unsigned                    getMaxCurrentBw() const;
                std::map<adaptive::ID, NearOptimalContext> streams;
                std::map<uint64_t, float>   utilities;
                unsigned                    currentBps;
                unsigned                    usedBps;
                vlc_object_t *              p_obj;
                vlc_mutex_t                 lock;
        };
    }
}

#endif // NEAROPTIMALADAPTATIONLOGIC_HPP
