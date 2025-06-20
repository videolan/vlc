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

template <typename T> class Ratio
{
    public:
        Ratio() { m_den = m_num = 0; }
        Ratio(T n, T d) { m_den = d, m_num = n; }
        bool isValid() const { return m_num && m_den; }
        T num() const { return m_num; }
        T den() const { return m_den; }
        T width() const { return m_num; }
        T height() const { return m_den; }

    protected:
        T m_num;
        T m_den;
};

using AspectRatio = Ratio<unsigned int>;
using Rate = Ratio<unsigned int>;

#endif // PROPERTIES_HPP
