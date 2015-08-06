/*
 * HLSStreamFormat.hpp
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
#ifndef HLSSTREAMFORMAT_HPP
#define HLSSTREAMFORMAT_HPP

#include "../adaptative/StreamFormat.hpp"
#include <string>

namespace hls
{
    using namespace adaptative;

    class HLSStreamFormat : public StreamFormat
    {
        public:
            static const unsigned UNKNOWN   = StreamFormat::UNSUPPORTED + 1; /* will probe */
            static const unsigned MPEG2TS   = StreamFormat::UNSUPPORTED + 2;
            static const unsigned PACKEDAAC = StreamFormat::UNSUPPORTED + 3;

            static StreamFormat mimeToFormat(const std::string &mime)
            {
                std::string::size_type pos = mime.find("/");
                if(pos != std::string::npos)
                {
                    std::string tail = mime.substr(pos + 1);
                    if(tail == "aac")
                        return StreamFormat(HLSStreamFormat::PACKEDAAC);
                    else if (tail == "mp2t")
                        return StreamFormat(HLSStreamFormat::MPEG2TS);
                }
                return StreamFormat();
            }
    };

}

#endif // HLSSTREAMFORMAT_HPP
