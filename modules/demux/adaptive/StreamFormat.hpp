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
            enum class Type
            {
                Unsupported,
                MPEG2TS,
                MP4,
                WebM,
                Ogg,
                WebVTT,
                TTML,
                PackedAAC,
                PackedMP3,
                PackedAC3,
                Unknown,
            };
            static const unsigned PEEK_SIZE   = 4096;

            StreamFormat( Type = Type::Unsupported );
            explicit StreamFormat( const std::string &mime );
            StreamFormat( const void *, size_t );
            ~StreamFormat();
            operator Type() const;
            std::string str() const;
            bool operator==(Type) const;
            bool operator!=(Type) const;
            bool operator==(const StreamFormat &) const;
            bool operator!=(const StreamFormat &) const;

        private:
            Type type;
    };

}

#endif // STREAMFORMAT_HPP
