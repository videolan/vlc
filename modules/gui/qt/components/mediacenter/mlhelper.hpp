/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef MLHELPER_HPP
#define MLHELPER_HPP

#include <memory>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vlc_media_library.h"

template<typename T>
class MLDeleter
{
public:
    void operator() (T* obj) {
        vlc_ml_release(obj);
    }
};

template<typename T>
using ml_unique_ptr = std::unique_ptr<T, MLDeleter<T> >;

template<typename T>
class MLListRange
{
public:
    MLListRange( T* begin, T* end )
        : m_begin(begin)
        , m_end(end)
    {
    }

    T* begin() const
    {
        return m_begin;
    }

    T* end() const
    {
        return m_end;
    }

private:
    T* m_begin;
    T* m_end;
};

template<typename T, typename L>
MLListRange<T> ml_range_iterate(L& list)
{
    return MLListRange<T>{ list->p_items, list->p_items + list->i_nb_items };
}


#endif // MLHELPER_HPP
