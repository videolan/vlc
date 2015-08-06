/*
 * AbstractPlaylist.hpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 * Copyright (C) 2015 VideoLAN and VLC authors
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
#ifndef ABSTRACTPLAYLIST_HPP_
#define ABSTRACTPLAYLIST_HPP_

#include <vector>
#include <string>

#include "ICanonicalUrl.hpp"
#include "../tools/Properties.hpp"

namespace adaptative
{
    namespace playlist
    {
        class BasePeriod;

        class AbstractPlaylist : public ICanonicalUrl
        {
            public:
                AbstractPlaylist(stream_t *);
                virtual ~AbstractPlaylist();

                virtual bool                    isLive() const = 0;
                void                            setType(const std::string &);
                virtual void                    debug() = 0;

                void    addPeriod               (BasePeriod *period);
                void    addBaseUrl              (const std::string &);

                virtual Url         getUrlSegment() const; /* impl */
                vlc_object_t *      getVLCObject()  const;

                virtual const std::vector<BasePeriod *>& getPeriods();
                virtual BasePeriod*                      getFirstPeriod();
                virtual BasePeriod*                      getNextPeriod(BasePeriod *period);

                void                mergeWith(AbstractPlaylist *, mtime_t = 0);
                void                pruneBySegmentNumber(uint64_t);
                void                getTimeLinesBoundaries(mtime_t *, mtime_t *) const;
                void                getPlaylistDurationsRange(mtime_t *, mtime_t *) const;

                Property<mtime_t>                   duration;
                Property<time_t>                    playbackStart;
                Property<time_t>                    availabilityEndTime;
                Property<time_t>                    availabilityStartTime;
                Property<time_t>                    minUpdatePeriod;
                Property<mtime_t>                   maxSegmentDuration;
                Property<time_t>                    minBufferTime;
                Property<time_t>                    timeShiftBufferDepth;

            protected:
                stream_t                           *stream;
                std::vector<BasePeriod *>           periods;
                std::vector<std::string>            baseUrls;
                std::string                         type;
        };
    }
}
#endif /* ABSTRACTPLAYLIST_HPP_ */
