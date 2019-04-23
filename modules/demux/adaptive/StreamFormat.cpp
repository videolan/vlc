/*
 * StreamFormat.cpp
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "StreamFormat.hpp"

#include <vlc_common.h>
#include <algorithm>

using namespace adaptive;

StreamFormat::operator unsigned() const
{
    return formatid;
}

std::string StreamFormat::str() const
{
    switch(formatid)
    {
        case MPEG2TS:
            return "TS";
        case MP4:
            return "MP4";
        case WEBVTT:
            return "WebVTT";
        case TTML:
            return "Timed Text";
        case PACKEDAAC:
            return "Packed AAC";
        case WEBM:
            return "WebM";
        case UNSUPPORTED:
            return "Unsupported";
        default:
        case UNKNOWN:
            return "Unknown";
    }
}

StreamFormat::StreamFormat( unsigned formatid_ )
{
    formatid = formatid_;
}

StreamFormat::StreamFormat( const std::string &mimetype )
{
    std::string mime = mimetype;
    std::transform(mime.begin(), mime.end(), mime.begin(), ::tolower);
    std::string::size_type pos = mime.find("/");
    formatid = UNKNOWN;
    if(pos != std::string::npos)
    {
        std::string tail = mime.substr(pos + 1);
        if(tail == "mp4")
            formatid = StreamFormat::MP4;
        else if (tail == "mp2t")
            formatid = StreamFormat::MPEG2TS;
        else if (tail == "vtt")
            formatid = StreamFormat::WEBVTT;
        else if (tail == "ttml+xml")
            formatid = StreamFormat::TTML;
        else if (tail == "webm")
            formatid = StreamFormat::WEBM;
    }
}

StreamFormat::StreamFormat(const void *data_, size_t sz)
{
    const uint8_t *data = reinterpret_cast<const uint8_t *>(data_);
    formatid = UNKNOWN;
    const char moov[] = "ftypmoovmoof";
    if(sz > 188 && data[0] == 0x47 && data[188] == 0x47)
        formatid = StreamFormat::MPEG2TS;
    else if(sz > 4 && (!memcmp(&moov, data, 4) ||
                       !memcmp(&moov[4], data, 4) ||
                       !memcmp(&moov[8], data, 4)))
        formatid = StreamFormat::MP4;
    else if(sz > 7 && !memcmp("WEBVTT", data, 6) &&
            std::isspace(static_cast<unsigned char>(data[7])))
        formatid = StreamFormat::WEBVTT;
    else if(sz > 4 && !memcmp(".Eß£", data, 4))
        formatid = StreamFormat::WEBM;
}

StreamFormat::~StreamFormat()
{

}

bool StreamFormat::operator ==(const StreamFormat &other) const
{
    return formatid == other.formatid;
}

bool StreamFormat::operator !=(const StreamFormat &other) const
{
    return formatid != other.formatid;
}
