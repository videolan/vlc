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

#include <functional>

/**
 * Provide operations for the list cache to load an count data
 *
 * These functions are assumed to be long-running, so they may be executed from a
 * separate thread, not to block the UI thread.
 *
 * The callbacks are expected to run on the UI thread
 */
template <typename T>
struct ListCacheLoader
{
    using ItemType = T;

    virtual ~ListCacheLoader() = default;

    virtual void cancelTask(size_t taskId) = 0;
    virtual size_t countTask(std::function<void(size_t taskId, size_t count)> cb) = 0;
    virtual size_t loadTask(
        size_t offset, size_t limit,
        std::function<void(size_t taskId, std::vector<T>& data)>) = 0;
    virtual size_t countAndLoadTask(
        size_t offset, size_t limit,
        std::function<void(size_t taskId, size_t count, std::vector<T>& data)> cb) = 0;
};

#endif
