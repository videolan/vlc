/*
 * BytesRange.cpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN Authors
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

#include "BytesRange.hpp"

using namespace adaptive::http;

BytesRange::BytesRange()
{
    bytesStart = 2;
    bytesEnd = 1;
}

BytesRange::BytesRange(size_t start, size_t end)
{
    bytesStart = start;
    bytesEnd = end;
}

bool BytesRange::isValid() const
{
    if(bytesEnd < bytesStart)
        return bytesEnd == 0;
    return true;
}

size_t BytesRange::getStartByte() const
{
    return bytesStart;
}

size_t BytesRange::getEndByte() const
{
    return bytesEnd;
}
