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

#include <StreamFormat.hpp>

#include <vlc_common.h>
#include <list>

namespace adaptive
{
    namespace http
    {
        class HTTPConnectionManager;
    }

    namespace logic
    {
        class AbstractAdaptationLogic;
    }

    namespace playlist
    {
        class BaseAdaptationSet;
        class BaseRepresentation;
        class SegmentChunk;
    }

    using namespace playlist;
    using namespace logic;
    using namespace http;

    class SegmentTrackerEvent
    {
        public:
            SegmentTrackerEvent(SegmentChunk *);
            SegmentTrackerEvent(BaseRepresentation *, BaseRepresentation *);
            SegmentTrackerEvent(const StreamFormat *);
            enum
            {
                DISCONTINUITY,
                SWITCHING,
                FORMATCHANGE,
            } type;
            union
            {
               struct
               {
                    SegmentChunk *sc;
               } discontinuity;
               struct
               {
                    BaseRepresentation *prev;
                    BaseRepresentation *next;
               } switching;
               struct
               {
                    const StreamFormat *f;
               } format;
            } u;
    };

    class SegmentTrackerListenerInterface
    {
        public:
            virtual void trackerEvent(const SegmentTrackerEvent &) = 0;
    };

    class SegmentTracker
    {
        public:
            SegmentTracker(AbstractAdaptationLogic *, BaseAdaptationSet *);
            ~SegmentTracker();

            void setAdaptationLogic(AbstractAdaptationLogic *);
            StreamFormat getCurrentFormat() const;
            bool segmentsListReady() const;
            void reset();
            SegmentChunk* getNextChunk(bool, HTTPConnectionManager *);
            bool setPositionByTime(mtime_t, bool, bool);
            void setPositionByNumber(uint64_t, bool);
            mtime_t getPlaybackTime() const; /* Current segment start time if selected */
            mtime_t getMinAheadTime() const;
            void registerListener(SegmentTrackerListenerInterface *);
            void updateSelected();

        private:
            void notify(const SegmentTrackerEvent &);
            bool first;
            bool initializing;
            bool index_sent;
            bool init_sent;
            uint64_t next;
            uint64_t curNumber;
            StreamFormat format;
            AbstractAdaptationLogic *logic;
            BaseAdaptationSet *adaptationSet;
            BaseRepresentation *curRepresentation;
            std::list<SegmentTrackerListenerInterface *> listeners;
    };
}

#endif // SEGMENTTRACKER_HPP
