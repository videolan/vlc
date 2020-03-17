/*
 * Properties.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC Authors
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
#ifndef PROPERTIES_HPP
#define PROPERTIES_HPP

template <typename T> class Property
{
    public:
        Property() {}

        T & Set(const T &i)
        {
            return value = i;
        }

        T const & Get() const
        {
            return value;
        }

    private:
        T value;
};

template <typename T> class Undef
{
    public:
        Undef() { undef = true; }

        bool isSet() const
        {
            return !undef;
        }

        void operator =(const T &v) { val = v; undef = false; }
        const T& value() const { return val; }

    private:
        bool undef;
        T val;
};

#endif // PROPERTIES_HPP
