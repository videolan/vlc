/*
 * SegmentBase.h
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

#ifndef SEGMENTBASE_H_
#define SEGMENTBASE_H_

#include "Segment.h"
#include "SegmentBaseType.hpp"
#include "../tools/Properties.hpp"

namespace adaptive
{
    namespace playlist
    {
        class SegmentInformation;

        /* SegmentBase can contain only one segment */
        class SegmentBase : public Segment,
                            public AbstractSegmentBaseType
        {
            public:
                SegmentBase             (SegmentInformation * = nullptr);
                virtual ~SegmentBase    ();

                virtual vlc_tick_t getMinAheadTime(uint64_t curnum) const; /* impl */
                virtual Segment *getMediaSegment(uint64_t number) const; /* impl */
                virtual Segment *getNextMediaSegment(uint64_t, uint64_t *, bool *) const; /* impl */
                virtual uint64_t getStartSegmentNumber() const; /* impl */

                virtual bool getSegmentNumberByTime(vlc_tick_t time, uint64_t *ret) const; /* impl */
                virtual bool getPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                            vlc_tick_t *time, vlc_tick_t *duration) const; /* impl */

                virtual void debug(vlc_object_t *,int = 0) const; /* reimpl */

            protected:
                SegmentInformation *parent;
        };
    }
}

#endif /* SEGMENTBASE_H_ */
