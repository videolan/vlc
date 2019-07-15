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
#ifndef MANIFESTPARSER_HPP
#define MANIFESTPARSER_HPP

#include "../../adaptive/playlist/SegmentInfoCommon.h"

#include <cstdlib>
#include <sstream>

#include <vlc_common.h>

namespace adaptive
{
    namespace playlist
    {
        class SegmentInformation;
        class MediaSegmentTemplate;
        class BasePeriod;
        class BaseAdaptationSet;
    }
    namespace xml
    {
        class Node;
    }
}

namespace smooth
{
    namespace playlist
    {
        using namespace adaptive::playlist;
        using namespace adaptive;

        class Manifest;

        class ManifestParser
        {
            public:
                ManifestParser             (xml::Node *, vlc_object_t *,
                                            stream_t *, const std::string &);
                virtual ~ManifestParser    ();

                Manifest * parse();

            private:
                xml::Node       *root;
                vlc_object_t    *p_object;
                stream_t        *p_stream;
                std::string      playlisturl;
        };
    }
}


#endif // MANIFESTPARSER_HPP
