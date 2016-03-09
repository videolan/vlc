/*
 * Conversions.hpp
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
#ifndef CONVERSIONS_HPP
#define CONVERSIONS_HPP

#include <vlc_common.h>
#include <string>
#include <sstream>

class IsoTime
{
    public:
        IsoTime(const std::string&);
        operator time_t() const;

    private:
        time_t time;
};

class UTCTime
{
    public:
        UTCTime(const std::string&);
        time_t  time() const;
        mtime_t mtime() const;

    private:
        mtime_t t;
};

template<typename T> class Integer
{
    public:
        Integer(const std::string &str)
        {
            try
            {
                std::istringstream in(str);
                in.imbue(std::locale("C"));
                in >> value;
            } catch (int) {
                value = 0;
            }
        }

        operator T() const
        {
            return value;
        }

    private:
        T value;
};

#endif // CONVERSIONS_HPP
