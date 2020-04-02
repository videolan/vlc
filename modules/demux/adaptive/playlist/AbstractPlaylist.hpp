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
                virtual bool                    isLowLatency() const;
                void                            setType(const std::string &);
                void                            setMinBuffering( vlc_tick_t );
                void                            setMaxBuffering( vlc_tick_t );
                vlc_tick_t                      getMinBuffering() const;
                vlc_tick_t                      getMaxBuffering() const;
                virtual void                    debug() = 0;

                void    addPeriod               (BasePeriod *period);
                void    addBaseUrl              (const std::string &);
                void    setPlaylistUrl          (const std::string &);
                void    setAvailabilityTimeOffset(vlc_tick_t);
                void    setAvailabilityTimeComplete(bool);
                vlc_tick_t getAvailabilityTimeOffset() const;
                bool    getAvailabilityTimeComplete() const;

                virtual Url         getUrlSegment() const; /* impl */
                vlc_object_t *      getVLCObject()  const;

                virtual const std::vector<BasePeriod *>& getPeriods();
                virtual BasePeriod*                      getFirstPeriod();
                virtual BasePeriod*                      getNextPeriod(BasePeriod *period);

                bool                needsUpdates() const;
                void                updateWith(AbstractPlaylist *);

                Property<vlc_tick_t>                   duration;
                Property<time_t>                    playbackStart;
                Property<vlc_tick_t>                   availabilityEndTime;
                Property<vlc_tick_t>                   availabilityStartTime;
                Property<vlc_tick_t>                   minUpdatePeriod;
                Property<vlc_tick_t>                   maxSegmentDuration;
                Property<vlc_tick_t>                   timeShiftBufferDepth;
                Property<vlc_tick_t>                   suggestedPresentationDelay;

            protected:
                vlc_object_t                       *p_object;
                std::vector<BasePeriod *>           periods;
                std::vector<std::string>            baseUrls;
                std::string                         playlistUrl;
                std::string                         type;
                vlc_tick_t                          minBufferTime;
                vlc_tick_t                          maxBufferTime;
                bool                                b_needsUpdates;

             private:
                Undef<bool>                         availabilityTimeComplete;
                Undef<vlc_tick_t>                   availabilityTimeOffset;
        };
    }
}
#endif /* ABSTRACTPLAYLIST_HPP_ */
