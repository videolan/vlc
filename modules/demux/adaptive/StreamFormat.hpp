/*
 * StreamFormat.hpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN and VLC authors
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
#ifndef STREAMFORMAT_HPP
#define STREAMFORMAT_HPP

#include <string>

namespace adaptive
{

    class StreamFormat
    {
        public:
            static const unsigned UNSUPPORTED = 0;
            static const unsigned MPEG2TS     = 1;
            static const unsigned MP4         = 2;
            static const unsigned WEBVTT      = 3;
            static const unsigned TTML        = 4;
            static const unsigned PACKEDAAC   = 5;
            static const unsigned UNKNOWN     = 0xFF; /* will probe */

            StreamFormat( unsigned = UNSUPPORTED );
            explicit StreamFormat( const std::string &mime );
            ~StreamFormat();
            operator unsigned() const;
            std::string str() const;
            bool operator==(const StreamFormat &) const;
            bool operator!=(const StreamFormat &) const;

        private:
            unsigned formatid;
    };

}

#endif // STREAMFORMAT_HPP
