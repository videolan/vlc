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
#include "../tools/Properties.hpp"
#include "SegmentInfoCommon.h"

namespace adaptive
{
    namespace playlist
    {
        class ICanonicalUrl;
        class InitSegmentTemplate;
        class SegmentInformation;
        class SegmentTimeline;

        class BaseSegmentTemplate : public Segment
        {
            public:
                BaseSegmentTemplate( ICanonicalUrl * = NULL );
        };

        class MediaSegmentTemplate : public BaseSegmentTemplate,
                                     public Initializable<InitSegmentTemplate>,
                                     public TimescaleAble
        {
            public:
                MediaSegmentTemplate( SegmentInformation * = NULL );
                virtual ~MediaSegmentTemplate();
                virtual void setSourceUrl( const std::string &url ); /* reimpl */
                void mergeWith( MediaSegmentTemplate *, vlc_tick_t );
                virtual uint64_t getSequenceNumber() const; /* reimpl */
                uint64_t getCurrentLiveTemplateNumber() const;
                stime_t getMinAheadScaledTime(uint64_t) const;
                void pruneByPlaybackTime(vlc_tick_t);
                size_t pruneBySequenceNumber(uint64_t);
                virtual void debug(vlc_object_t *, int = 0) const; /* reimpl */
                Property<size_t>        startNumber;
                Property<SegmentTimeline *> segmentTimeline;

            protected:
                SegmentInformation *parentSegmentInformation;
        };

        class InitSegmentTemplate : public BaseSegmentTemplate
        {
            public:
                InitSegmentTemplate( ICanonicalUrl * = NULL );
        };
    }
}
#endif // SEGMENTTEMPLATE_H
