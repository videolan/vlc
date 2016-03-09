/*
 * BytesRange.hpp
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
#ifndef BYTESRANGE_HPP
#define BYTESRANGE_HPP

#include <vlc_common.h>

namespace adaptive
{
    namespace http
    {
        class BytesRange
        {
            public:
                BytesRange();
                BytesRange(size_t start, size_t end);
                bool isValid() const;
                size_t getStartByte() const;
                size_t getEndByte() const;

            private:
                size_t bytesStart;
                size_t bytesEnd;
        };
    }
}

#endif // BYTESRANGE_HPP
