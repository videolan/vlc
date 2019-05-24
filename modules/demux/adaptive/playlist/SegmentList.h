/*
 * SegmentList.h
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Jan 27, 2012
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

#ifndef SEGMENTLIST_H_
#define SEGMENTLIST_H_

#include "SegmentInfoCommon.h"

namespace adaptive
{
    namespace playlist
    {
        class SegmentInformation;
        class Segment;

        class SegmentList : public SegmentInfoCommon,
                            public TimescaleAble
        {
            public:
                SegmentList             ( SegmentInformation * = NULL );
                virtual ~SegmentList    ();

                const std::vector<ISegment *>&   getSegments() const;
                ISegment *              getSegmentByNumber(uint64_t);
                void                    addSegment(ISegment *seg);
                void                    updateWith(SegmentList *, bool = false);
                void                    pruneBySegmentNumber(uint64_t);
                void                    pruneByPlaybackTime(vlc_tick_t);
                bool                    getSegmentNumberByScaledTime(stime_t, uint64_t *) const;
                bool                    getPlaybackTimeDurationBySegmentNumber(uint64_t, vlc_tick_t *, vlc_tick_t *) const;
                stime_t                 getTotalLength() const;

            private:
                std::vector<ISegment *>  segments;
                stime_t totalLength;
        };
    }
}

#endif /* SEGMENTLIST_H_ */
