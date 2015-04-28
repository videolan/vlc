/*
 * SegmentInformation.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN Authors
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
#ifndef SEGMENTINFORMATION_HPP
#define SEGMENTINFORMATION_HPP

#ifndef __STDC_CONSTANT_MACROS
  #define __STDC_CONSTANT_MACROS
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ICanonicalUrl.hpp"
#include "../tools/Properties.hpp"
#include "SegmentInfoCommon.h"
#include <vlc_common.h>
#include <vector>

namespace adaptative
{
    namespace playlist
    {
        class SegmentBase;
        class SegmentList;
        class SegmentTimeline;
        class SegmentTemplate;
        class AbstractPlaylist;
        class ISegment;

        /* common segment elements for period/adaptset/rep 5.3.9.1,
         * with properties inheritance */
        class SegmentInformation : public ICanonicalUrl,
                                   public TimescaleAble
        {
            public:
                SegmentInformation( SegmentInformation * = 0 );
                explicit SegmentInformation( AbstractPlaylist * );
                virtual ~SegmentInformation();
                bool canBitswitch() const;
                virtual mtime_t getPeriodStart() const;

                class SplitPoint
                {
                    public:
                        size_t offset;
                        mtime_t time;
                };
                void SplitUsingIndex(std::vector<SplitPoint>&);

                enum SegmentInfoType
                {
                    INFOTYPE_INIT = 0,
                    INFOTYPE_MEDIA,
                    INFOTYPE_INDEX
                };
                static const int InfoTypeCount = INFOTYPE_INDEX + 1;

                ISegment * getSegment(SegmentInfoType, uint64_t = 0) const;
                bool getSegmentNumberByTime(mtime_t, uint64_t *) const;
                mtime_t getPlaybackTimeBySegmentNumber(uint64_t) const;
                void collectTimelines(std::vector<SegmentTimeline *> *) const;

            protected:
                std::vector<ISegment *> getSegments() const;
                std::vector<ISegment *> getSegments(SegmentInfoType) const;
                std::vector<SegmentInformation *> childs;

            public:
                void setSegmentList(SegmentList *);
                void setSegmentBase(SegmentBase *);
                void setSegmentTemplate(MediaSegmentTemplate *);
                void setBitstreamSwitching(bool);
                virtual Url getUrlSegment() const; /* impl */
                Property<Url *> baseUrl;

            private:
                void init();
                SegmentBase *     inheritSegmentBase() const;
                SegmentList *     inheritSegmentList() const;
                MediaSegmentTemplate * inheritSegmentTemplate() const;

                SegmentInformation *parent;
                SegmentBase     *segmentBase;
                SegmentList     *segmentList;
                MediaSegmentTemplate *mediaSegmentTemplate;

                enum BitswitchPolicy
                {
                    BITSWITCH_INHERIT,
                    BITSWITCH_YES,
                    BITSWITCH_NO
                } bitswitch_policy;
        };
    }
}

#endif // SEGMENTINFORMATION_HPP
