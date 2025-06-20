/*****************************************************************************
 * SegmentTemplate.cpp: Implement the UrlTemplate element.
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

#ifndef SEGMENTTEMPLATE_H
#define SEGMENTTEMPLATE_H

#include "Segment.h"
#include "SegmentBaseType.hpp"

namespace adaptive
{
    namespace playlist
    {
        class ICanonicalUrl;
        class SegmentTemplateInit;
        class SegmentInformation;
        class SegmentTemplate;

        class SegmentTemplateSegment : public Segment
        {
            public:
                SegmentTemplateSegment( ICanonicalUrl * = nullptr );
                virtual ~SegmentTemplateSegment();
                void setSourceUrl( const std::string &url ) override;
                void setParentTemplate( SegmentTemplate * );

            protected:
                const SegmentTemplate *templ;
        };

        class SegmentTemplate : public AbstractMultipleSegmentBaseType
        {
            public:
                SegmentTemplate( SegmentTemplateSegment *, SegmentInformation * = nullptr );
                virtual ~SegmentTemplate();
                void setSourceUrl( const std::string &url );
                uint64_t getLiveTemplateNumber(vlc_tick_t, bool = true) const;
                void pruneByPlaybackTime(vlc_tick_t);
                size_t pruneBySequenceNumber(uint64_t);

                vlc_tick_t getMinAheadTime(uint64_t curnum) const override;
                Segment * getMediaSegment(uint64_t number) const override;
                Segment * getNextMediaSegment(uint64_t, uint64_t *, bool *) const override;
                uint64_t getStartSegmentNumber() const override;

                bool getSegmentNumberByTime(vlc_tick_t time, uint64_t *ret) const override;
                bool getPlaybackTimeDurationBySegmentNumber(uint64_t number,
                                            vlc_tick_t *time, vlc_tick_t *duration) const override;

                void debug(vlc_object_t *, int = 0) const override;

            protected:
                bool getScaledPlaybackTimeDurationBySegmentNumber(uint64_t, stime_t *,
                                                                  stime_t *, Timescale *) const;
                void setVirtualSegmentTime(uint64_t pos,
                                           SegmentTemplateSegment *virtualsegment) const;
                SegmentInformation *parentSegmentInformation;
                SegmentTemplateSegment *virtualsegment;
        };

        class SegmentTemplateInit : public InitSegment
        {
            public:
                SegmentTemplateInit( SegmentTemplate *, ICanonicalUrl * = nullptr );
                virtual ~SegmentTemplateInit();
                void setSourceUrl( const std::string &url ) override;

            protected:
                const SegmentTemplate *templ;
        };
    }
}
#endif // SEGMENTTEMPLATE_H
