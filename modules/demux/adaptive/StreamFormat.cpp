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
