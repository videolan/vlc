/*
 * SegmentBaseType.hpp
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs, VideoLAN and VLC Authors
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
#ifndef SEGMENTBASETYPE_H_
#define SEGMENTBASETYPE_H_

#include "Segment.h"
#include "Inheritables.hpp"
#include "Templates.hpp"
#include "../tools/Properties.hpp"

namespace adaptive
{
    namespace playlist
    {
        class SegmentInformation;
        class SegmentTimeline;

        class AbstractSegmentBaseType : public Initializable<InitSegment>,
                                        public Indexable<IndexSegment>,
                                        public TimescaleAble
        {
            public:
                AbstractSegmentBaseType( SegmentInformation * );
                virtual ~AbstractSegmentBaseType();

                virtual vlc_tick_t getMinAheadTime(uint64_t) const = 0;
                virtual Segment *getMediaSegment(uint64_t pos) const = 0;
                virtual InitSegment *getInitSegment() const;
                virtual IndexSegment *getIndexSegment() const;
                virtual Segment *getNextMediaSegment(uint64_t, uint64_t *, bool *) const = 0;
                virtual uint64_t getStartSegmentNumber() const = 0;

                virtual bool getSegmentNumberByTime(vlc_tick_t time, uint64_t *ret) const = 0;
                virtual bool getPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                                vlc_tick_t *time, vlc_tick_t *duration) const = 0;

                Timescale inheritTimescale() const; /* reimpl */

                virtual void debug(vlc_object_t *, int = 0) const;

                static Segment * findSegmentByScaledTime(const std::vector<Segment *> &,
                                                         stime_t);
                static uint64_t findSegmentNumberByScaledTime(const std::vector<Segment *> &,
                                                             stime_t);
                SegmentInformation * getParent() const;

            protected:
                SegmentInformation *parent;
        };

        class AbstractMultipleSegmentBaseType : public AbstractSegmentBaseType
        {
            public:
                AbstractMultipleSegmentBaseType( SegmentInformation * );
                virtual ~AbstractMultipleSegmentBaseType();

                void setSegmentTimeline( SegmentTimeline * );
                SegmentTimeline * inheritSegmentTimeline() const;
                SegmentTimeline * getSegmentTimeline() const;
                void setStartNumber( uint64_t );
                uint64_t inheritStartNumber() const;
                stime_t inheritDuration() const;
                virtual void updateWith(AbstractMultipleSegmentBaseType *, bool = false);
                Property<stime_t>       duration;

            protected:
                uint64_t startNumber;
                SegmentTimeline *segmentTimeline;

        };
    }
}

#endif /* SEGMENTBASETYPE_H_ */
