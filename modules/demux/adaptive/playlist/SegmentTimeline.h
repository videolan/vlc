/*****************************************************************************
 * SegmentTimeline.cpp: Implement the SegmentTimeline tag.
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#ifndef SEGMENTTIMELINE_H
#define SEGMENTTIMELINE_H

#include "SegmentInfoCommon.h"
#include <vlc_common.h>
#include <list>

namespace adaptive
{
    namespace playlist
    {
        class SegmentTimeline : public TimescaleAble
        {
            class Element;

            public:
                SegmentTimeline(TimescaleAble *);
                SegmentTimeline(uint64_t);
                virtual ~SegmentTimeline();
                void addElement(uint64_t, stime_t d, uint64_t r = 0, stime_t t = 0);
                uint64_t getElementNumberByScaledPlaybackTime(stime_t) const;
                bool    getScaledPlaybackTimeDurationBySegmentNumber(uint64_t, stime_t *, stime_t *) const;
                stime_t getScaledPlaybackTimeByElementNumber(uint64_t) const;
                stime_t getMinAheadScaledTime(uint64_t) const;
                stime_t getTotalLength() const;
                uint64_t maxElementNumber() const;
                uint64_t minElementNumber() const;
                void pruneByPlaybackTime(vlc_tick_t);
                size_t pruneBySequenceNumber(uint64_t);
                void updateWith(SegmentTimeline &);
                void debug(vlc_object_t *, int = 0) const;

            private:
                std::list<Element *> elements;
                stime_t totalLength;

                class Element
                {
                    public:
                        Element(uint64_t, stime_t, uint64_t, stime_t);
                        void debug(vlc_object_t *, int = 0) const;
                        bool contains(stime_t) const;
                        stime_t  t;
                        stime_t  d;
                        uint64_t r;
                        uint64_t number;
                };
        };
    }
}

#endif // SEGMENTTIMELINE_H
