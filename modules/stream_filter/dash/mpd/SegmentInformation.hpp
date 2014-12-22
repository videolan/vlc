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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ICanonicalUrl.hpp"
#include "Properties.hpp"
#include <vlc_common.h>
#include <vector>

namespace dash
{
    namespace mpd
    {
        class ISegment;
        class SegmentBase;
        class SegmentList;
        class SegmentTemplate;

        /* common segment elements for period/adaptset/rep 5.3.9.1,
         * with properties inheritance */
        class SegmentInformation : public ICanonicalUrl
        {
            friend class IsoffMainParser;

            public:
                SegmentInformation( SegmentInformation * = 0 );
                explicit SegmentInformation( ICanonicalUrl * );
                virtual ~SegmentInformation();
                std::vector<ISegment *> getSegments() const;
                bool canBitswitch() const;
                uint64_t getTimescale() const;
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

            private:
                void setSegmentList(SegmentList *);
                void setSegmentBase(SegmentBase *);
                void setSegmentTemplate(SegmentTemplate *, SegmentInfoType);
                void setBitstreamSwitching(bool);

                SegmentBase *     inheritSegmentBase() const;
                SegmentList *     inheritSegmentList() const;
                SegmentTemplate * inheritSegmentTemplate(SegmentInfoType) const;

                SegmentInformation *parent;
                SegmentBase     *segmentBase;
                SegmentList     *segmentList;
                SegmentTemplate *segmentTemplate[InfoTypeCount];

                enum BitswitchPolicy
                {
                    BITSWITCH_INHERIT,
                    BITSWITCH_YES,
                    BITSWITCH_NO
                } bitswitch_policy;

                Property<uint64_t> timescale;
        };
    }
}

#endif // SEGMENTINFORMATION_HPP
