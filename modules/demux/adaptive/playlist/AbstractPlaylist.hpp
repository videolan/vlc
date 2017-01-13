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

namespace adaptive
{

    namespace playlist
    {
        class BasePeriod;

        class AbstractPlaylist : public ICanonicalUrl
        {
            public:
                AbstractPlaylist(vlc_object_t *);
                virtual ~AbstractPlaylist();

                virtual bool                    isLive() const = 0;
                void                            setType(const std::string &);
                void                            setMinBuffering( mtime_t );
                mtime_t                         getMinBuffering() const;
                mtime_t                         getMaxBuffering() const;
                virtual void                    debug() = 0;

                void    addPeriod               (BasePeriod *period);
                void    addBaseUrl              (const std::string &);
                void    setPlaylistUrl          (const std::string &);

                virtual Url         getUrlSegment() const; /* impl */
                vlc_object_t *      getVLCObject()  const;

                virtual const std::vector<BasePeriod *>& getPeriods();
                virtual BasePeriod*                      getFirstPeriod();
                virtual BasePeriod*                      getNextPeriod(BasePeriod *period);

                void                mergeWith(AbstractPlaylist *, mtime_t = 0);
                void                pruneByPlaybackTime(mtime_t);

                Property<mtime_t>                   duration;
                Property<time_t>                    playbackStart;
                Property<time_t>                    availabilityEndTime;
                Property<time_t>                    availabilityStartTime;
                Property<mtime_t>                   minUpdatePeriod;
                Property<mtime_t>                   maxSegmentDuration;
                Property<mtime_t>                   timeShiftBufferDepth;
                Property<mtime_t>                   suggestedPresentationDelay;

            protected:
                vlc_object_t                       *p_object;
                std::vector<BasePeriod *>           periods;
                std::vector<std::string>            baseUrls;
                std::string                         playlistUrl;
                std::string                         type;
                mtime_t                             minBufferTime;
        };
    }
}
#endif /* ABSTRACTPLAYLIST_HPP_ */
