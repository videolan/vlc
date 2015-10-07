/*
 * SegmentTracker.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
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
#ifndef SEGMENTTRACKER_HPP
#define SEGMENTTRACKER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <list>

namespace adaptative
{
    namespace logic
    {
        class AbstractAdaptationLogic;
    }

    namespace playlist
    {
        class BaseAdaptationSet;
        class BaseRepresentation;
        class SegmentChunk;
        class ISegment;
    }

    using namespace playlist;
    using namespace logic;

    class SegmentTrackerListenerInterface
    {
        public:
            enum notifications
            {
                NOTIFICATION_DISCONTINUITY = 0
            };
            virtual void trackerNotification(notifications, ISegment *) = 0;
    };

    class SegmentTracker
    {
        public:
            SegmentTracker(AbstractAdaptationLogic *, BaseAdaptationSet *);
            ~SegmentTracker();

            void setAdaptationLogic(AbstractAdaptationLogic *);
            void resetCounter();
            SegmentChunk* getNextChunk(bool);
            bool setPositionByTime(mtime_t, bool, bool);
            void setPositionByNumber(uint64_t, bool);
            mtime_t getSegmentStart() const;
            void registerListener(SegmentTrackerListenerInterface *);
            void pruneFromCurrent();
            void updateSelected();

        private:
            void notify(SegmentTrackerListenerInterface::notifications, ISegment *);
            bool initializing;
            bool index_sent;
            bool init_sent;
            uint64_t count;
            AbstractAdaptationLogic *logic;
            BaseAdaptationSet *adaptationSet;
            BaseRepresentation *prevRepresentation;
            std::list<SegmentTrackerListenerInterface *> listeners;
    };
}

#endif // SEGMENTTRACKER_HPP
