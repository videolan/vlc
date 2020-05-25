/*
 * SegmentInformation.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC Authors
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

#include "ICanonicalUrl.hpp"
#include "../tools/Properties.hpp"
#include "../encryption/CommonEncryption.hpp"
#include "SegmentInfoCommon.h"
#include <vlc_common.h>
#include <vector>

namespace adaptive
{
    namespace playlist
    {
        class SegmentBase;
        class SegmentList;
        class SegmentTimeline;
        class SegmentTemplate;
        class MediaSegmentTemplate;
        class AbstractPlaylist;
        class ISegment;

        /* common segment elements for period/adaptset/rep 5.3.9.1,
         * with properties inheritance */
        class SegmentInformation : public ICanonicalUrl,
                                   public TimescaleAble,
                                   public Unique
        {
            friend class MediaSegmentTemplate;
            public:
                SegmentInformation( SegmentInformation * = 0 );
                explicit SegmentInformation( AbstractPlaylist * );
                virtual ~SegmentInformation();

                virtual vlc_tick_t getPeriodStart() const;
                virtual vlc_tick_t getPeriodDuration() const;
                virtual AbstractPlaylist *getPlaylist() const;

                class SplitPoint
                {
                    public:
                        size_t offset;
                        vlc_tick_t time;
                        vlc_tick_t duration;
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
                ISegment * getNextSegment(SegmentInfoType, uint64_t, uint64_t *, bool *) const;
                bool getSegmentNumberByTime(vlc_tick_t, uint64_t *) const;
                bool getPlaybackTimeDurationBySegmentNumber(uint64_t, vlc_tick_t *, vlc_tick_t *) const;
                bool     getMediaPlaybackRange(vlc_tick_t *, vlc_tick_t *, vlc_tick_t *) const;
                virtual void updateWith(SegmentInformation *);
                virtual void mergeWithTimeline(SegmentTimeline *); /* ! don't use with global merge */
                virtual void pruneBySegmentNumber(uint64_t);
                virtual void pruneByPlaybackTime(vlc_tick_t);
                virtual uint64_t translateSegmentNumber(uint64_t, const SegmentInformation *) const;
                void setEncryption(const CommonEncryption &);
                const CommonEncryption & intheritEncryption() const;

            protected:
                std::size_t getAllSegments(std::vector<ISegment *> &) const;
                virtual std::size_t getSegments(SegmentInfoType, std::vector<ISegment *>&) const;
                std::vector<SegmentInformation *> childs;
                SegmentInformation * getChildByID( const ID & );
                SegmentInformation *parent;

            public:
                void updateSegmentList(SegmentList *, bool = false);
                void setSegmentBase(SegmentBase *);
                void setSegmentTemplate(MediaSegmentTemplate *);
                virtual Url getUrlSegment() const; /* impl */
                Property<Url *> baseUrl;
                void setAvailabilityTimeOffset(vlc_tick_t);
                void setAvailabilityTimeComplete(bool);
                SegmentBase *     inheritSegmentBase() const;
                SegmentList *     inheritSegmentList() const;
                MediaSegmentTemplate * inheritSegmentTemplate() const;
                vlc_tick_t        inheritAvailabilityTimeOffset() const;
                bool              inheritAvailabilityTimeComplete() const;

            private:
                void init();
                SegmentBase     *segmentBase;
                SegmentList     *segmentList;
                MediaSegmentTemplate *mediaSegmentTemplate;
                CommonEncryption commonEncryption;
                Undef<bool>      availabilityTimeComplete;
                Undef<vlc_tick_t>availabilityTimeOffset;
        };
    }
}

#endif // SEGMENTINFORMATION_HPP
