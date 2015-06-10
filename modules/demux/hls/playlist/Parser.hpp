/*
 * Parser.hpp
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
#ifndef PARSER_HPP
#define PARSER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../adaptative/playlist/SegmentInfoCommon.h"

#include <cstdlib>
#include <sstream>

#include <vlc_common.h>

namespace adaptative
{
    namespace playlist
    {
        class SegmentInformation;
        class MediaSegmentTemplate;
        class BasePeriod;
        class BaseAdaptationSet;
    }
}

namespace hls
{
    namespace playlist
    {
        using namespace adaptative::playlist;

        class M3U8;
        class AttributesTag;
        class Tag;
        class Representation;

        class Parser
        {
            public:
                Parser             (stream_t *p_stream);
                virtual ~Parser    ();

                M3U8 *             parse  (const std::string &);

            private:
                void parseAdaptationSet(BasePeriod *, const AttributesTag *);
                void parseRepresentation(BaseAdaptationSet *, const AttributesTag *);
                void parseRepresentation(BaseAdaptationSet *, const AttributesTag *,
                                         const std::list<Tag *>&);
                void parseSegments(Representation *, const std::list<Tag *>&);
                std::list<Tag *> parseEntries(stream_t *);

                stream_t        *p_stream;
        };
    }
}

#endif // PARSER_HPP
