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

extern "C"
{
    #include "../../meta_engine/ID3Tag.h"
}

#include <algorithm>

using namespace adaptive;

StreamFormat::operator StreamFormat::Type() const
{
    return type;
}

std::string StreamFormat::str() const
{
    switch(type)
    {
        case Type::MPEG2TS:
            return "TS";
        case Type::MP4:
            return "MP4";
        case Type::WebVTT:
            return "WebVTT";
        case Type::TTML:
            return "Timed Text";
        case Type::PackedAAC:
            return "Packed AAC";
        case Type::PackedMP3:
            return "Packed MP3";
        case Type::PackedAC3:
            return "Packed AC-3";
        case Type::WebM:
            return "WebM";
        case Type::Ogg:
            return "Ogg";
        case Type::Unsupported:
            return "Unsupported";
        default:
        case Type::Unknown:
            return "Unknown";
    }
}

StreamFormat::StreamFormat( Type type_ )
{
    type = type_;
}

StreamFormat::StreamFormat( const std::string &mimetype )
{
    std::string mime = mimetype;
    std::transform(mime.begin(), mime.end(), mime.begin(), ::tolower);
    std::string::size_type pos = mime.find("/");
    type = Type::Unknown;
    if(pos != std::string::npos)
    {
        std::string tail = mime.substr(pos + 1);
        if(tail == "mp4")
            type = StreamFormat::Type::MP4;
        else if(tail == "aac")
            type = StreamFormat::Type::PackedAAC;
        else if(tail == "mpeg" || tail == "mp3")
            type = StreamFormat::Type::PackedMP3;
        else if(tail == "ac3")
            type = StreamFormat::Type::PackedAC3;
        else if (tail == "mp2t")
            type = StreamFormat::Type::MPEG2TS;
        else if (tail == "vtt")
            type = StreamFormat::Type::WebVTT;
        else if (tail == "ttml+xml")
            type = StreamFormat::Type::TTML;
        else if (tail == "webm")
            type = StreamFormat::Type::WebM;
    }
}

static int ID3Callback(uint32_t, const uint8_t *, size_t, void *)
{
    return VLC_EGENERIC;
}

StreamFormat::StreamFormat(const void *data_, size_t sz)
{
    const uint8_t *data = reinterpret_cast<const uint8_t *>(data_);
    type = Type::Unknown;
    const char moov[] = "ftypmoovmoof";

    if(sz > 188 && data[0] == 0x47 && data[188] == 0x47)
        type = StreamFormat::Type::MPEG2TS;
    else if(sz > 8 && (!memcmp(&moov,    &data[4], 4) ||
                       !memcmp(&moov[4], &data[4], 4) ||
                       !memcmp(&moov[8], &data[4], 4)))
        type = StreamFormat::Type::MP4;
    else if(sz > 7 && !memcmp("WEBVTT", data, 6) &&
            std::isspace(static_cast<unsigned char>(data[7])))
        type = StreamFormat::Type::WebVTT;
    else if(sz > 4 && !memcmp("\x1A\x45\xDF\xA3", data, 4))
        type = StreamFormat::Type::WebM;
    else if(sz > 4 && !memcmp("OggS", data, 4))
        type = StreamFormat::Type::Ogg;
    else /* Check Packet Audio formats */
    {
        /* It MUST have ID3 header, but HLS spec is an oxymoron */
        while(sz > 10 && ID3TAG_IsTag(data, false))
        {
            size_t tagsize = ID3TAG_Parse(data, sz, ID3Callback, this);
            if(tagsize >= sz || tagsize == 0)
                return; /* not enough peek */
            data += tagsize;
            sz -= tagsize;
        }
        /* Skipped ID3 if any */
        if(sz > 3 && (!memcmp("\xFF\xF1", data, 2) ||
                      !memcmp("\xFF\xF9", data, 2)))
        {
            type = StreamFormat::Type::PackedAAC;
        }
        else if(sz > 4 && data[0] == 0xFF && (data[1] & 0xE6) > 0xE0)
        {
            type = StreamFormat::Type::PackedMP3;
        }
        else if(sz > 4 && data[0] == 0x0b && data[1] == 0x77)
        {
            type = StreamFormat::Type::PackedAC3;
        }
    }
}

StreamFormat::~StreamFormat()
{

}

bool StreamFormat::operator ==(Type t) const
{
    return type == t;
}

bool StreamFormat::operator !=(Type t) const
{
    return type != t;
}

bool StreamFormat::operator ==(const StreamFormat &other) const
{
    return type == other.type;
}

bool StreamFormat::operator !=(const StreamFormat &other) const
{
    return type != other.type;
}
