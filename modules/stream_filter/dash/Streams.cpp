/*
 * Streams.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN authors
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
#include "Streams.hpp"

using namespace dash::Streams;

Stream::Stream(const std::string &mime)
{
    type = mimeToType(mime);
}

Stream::Stream(const Type type)
{
    this->type = type;
}

Type Stream::mimeToType(const std::string &mime)
{
    Type mimetype;
    if (mime == "video/mp4")
        mimetype = Streams::VIDEO;
    else if (mime == "audio/mp4")
        mimetype = Streams::AUDIO;
    else if (mime == "application/mp4")
        mimetype = Streams::APPLICATION;
    else /* unknown of unsupported */
        mimetype = Streams::UNKNOWN;
    return mimetype;
}

bool Stream::operator ==(const Stream &stream) const
{
    return stream.type == type;
}
