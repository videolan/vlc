/*****************************************************************************
 * SegmentInfoCommon.h: Implement the common part for both SegmentInfoDefault
 *                      and SegmentInfo
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

#ifndef SEGMENTINFOCOMMON_H
#define SEGMENTINFOCOMMON_H

#include <string>
#include <list>
#include "Inheritables.hpp"
#include "Templates.hpp"
#include "Segment.h"
#include "ICanonicalUrl.hpp"
#include "../tools/Properties.hpp"

namespace adaptive
{
    namespace playlist
    {
        class Segment;

        class SegmentInfoCommon : public ICanonicalUrl,
                                  public Initializable<Segment>,
                                  public Indexable<Segment>
        {
            public:
                SegmentInfoCommon( ICanonicalUrl *parent = NULL );
                virtual ~SegmentInfoCommon();
                int                     getStartIndex() const;
                void                    setStartIndex( int startIndex );
                void                    appendBaseURL( const std::string& url );
                virtual Url             getUrlSegment() const; /* impl */
                Property<stime_t>      duration;
                static bool             getSegmentNumberByScaledTime(const std::vector<ISegment *> &,
                                                                     stime_t, uint64_t *);

            private:
                int                     startIndex;
                std::list<std::string>  baseURLs;
        };
    }
}

#endif // SEGMENTINFOCOMMON_H
