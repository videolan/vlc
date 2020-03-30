/*
 * BufferingLogic.hpp
 *****************************************************************************
 * Copyright (C) 2014 - 2020 VideoLabs, VideoLAN and VLC authors
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
#ifndef BUFFERINGLOGIC_HPP
#define BUFFERINGLOGIC_HPP

#include <vector>
#include <vlc_common.h>
#include "../tools/Properties.hpp"

namespace adaptive
{
    namespace playlist
    {
        class BaseRepresentation;
        class AbstractPlaylist;
    }

    namespace logic
    {
        using namespace playlist;

        class AbstractBufferingLogic
        {
            public:
                AbstractBufferingLogic();
                virtual ~AbstractBufferingLogic() {}

                virtual uint64_t getStartSegmentNumber(BaseRepresentation *) const = 0;
                virtual vlc_tick_t getMinBuffering(const AbstractPlaylist *) const = 0;
                virtual vlc_tick_t getMaxBuffering(const AbstractPlaylist *) const = 0;
                virtual vlc_tick_t getLiveDelay(const AbstractPlaylist *) const = 0;
                void setUserMinBuffering(vlc_tick_t);
                void setUserMaxBuffering(vlc_tick_t);
                void setUserLiveDelay(vlc_tick_t);
                void setLowDelay(bool);
                static const vlc_tick_t BUFFERING_LOWEST_LIMIT;
                static const vlc_tick_t DEFAULT_MIN_BUFFERING;
                static const vlc_tick_t DEFAULT_MAX_BUFFERING;
                static const vlc_tick_t DEFAULT_LIVE_BUFFERING;

            protected:
                vlc_tick_t userMinBuffering;
                vlc_tick_t userMaxBuffering;
                vlc_tick_t userLiveDelay;
                Undef<bool> userLowLatency;
        };

        class DefaultBufferingLogic : public AbstractBufferingLogic
        {
            public:
                DefaultBufferingLogic();
                virtual ~DefaultBufferingLogic() {}
                virtual uint64_t getStartSegmentNumber(BaseRepresentation *) const; /* impl */
                virtual vlc_tick_t getMinBuffering(const AbstractPlaylist *) const; /* impl */
                virtual vlc_tick_t getMaxBuffering(const AbstractPlaylist *) const; /* impl */
                virtual vlc_tick_t getLiveDelay(const AbstractPlaylist *) const; /* impl */

            protected:
                vlc_tick_t getBufferingOffset(const AbstractPlaylist *) const;
                uint64_t getLiveStartSegmentNumber(BaseRepresentation *) const;
                bool isLowLatency(const AbstractPlaylist *) const;
        };
    }
}

#endif
