/*
 * Representation.hpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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

#ifndef HLSREPRESENTATION_H_
#define HLSREPRESENTATION_H_

#include "../adaptative/playlist/BaseRepresentation.h"
#include "../adaptative/tools/Properties.hpp"

namespace hls
{
    namespace playlist
    {
        class M3U8;

        using namespace adaptative;
        using namespace adaptative::playlist;

        class Representation : public BaseRepresentation
        {
            friend class Parser;

            public:
                Representation( BaseAdaptationSet * );
                virtual ~Representation ();
                virtual StreamFormat getStreamFormat() const; /* reimpl */

                void localMergeWithPlaylist(M3U8 *, mtime_t);
                bool isLive() const;
                virtual void mergeWith(SegmentInformation *, mtime_t); /* reimpl */

            private:
                bool b_live;
                Property<std::string> playlistUrl;
                Property<std::string> audio;
                Property<std::string> video;
                Property<std::string> subtitles;
                Property<std::string> closedcaptions;

        };
    }
}

#endif /* HLSREPRESENTATION_H_ */
