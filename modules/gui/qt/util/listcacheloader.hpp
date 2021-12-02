/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#ifndef LISTCACHELOADER_HPP
#define LISTCACHELOADER_HPP

struct vlc_medialibrary_t;

template <typename T>
struct ListCacheLoader
{
    virtual ~ListCacheLoader() = default;
    virtual size_t count(vlc_medialibrary_t* ml) const = 0;
    virtual std::vector<T> load(vlc_medialibrary_t* ml, size_t index, size_t count) const = 0;
};

#endif
