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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "SegmentInfoCommon.h"

namespace adaptative
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
                void                    addSegment(ISegment *seg);
                void                    mergeWith(SegmentList *);
                void                    pruneBySegmentNumber(uint64_t);
                bool                    getSegmentNumberByTime(mtime_t, uint64_t *) const;
                mtime_t                 getPlaybackTimeBySegmentNumber(uint64_t);
                std::size_t             getOffset() const;

            private:
                std::vector<ISegment *>  segments;
                std::size_t pruned;
        };
    }
}

#endif /* SEGMENTLIST_H_ */
