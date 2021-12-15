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

#ifndef LISTCACHE_HPP
#define LISTCACHE_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc_common.h"
#include <cassert>
#include <memory>
#include <vector>
#include <QtGlobal>
#include <QObject>
#include <QSharedPointer>
#include "util/listcacheloader.hpp"

#include "medialib.hpp"

/**
 * `MLListCache` represents a cache for a (constant) list of items.
 *
 * The caller must provide a `ListCacheLoader<T>`, defining the following
 * methods:
 *  - `count()` returns the number of items in the list;
 *  - `load(index, count)` returning the items for the requested interval.
 *
 * These functions are assumed to be long-running, so they executed from a
 * separate thread, not to block the UI thread.
 *
 * The precise cache strategy is unspecified (it may change in the future), but
 * the general principle is to keep locally only a part of the whole data.
 *
 * The list of items it represents is assumed constant:
 *  1. the list size will never change once initialized,
 *  2. the items retrieved by several calls at a specific location should be
 *     "the same".
 *
 * Note that (2.) might not always be respected in practice, for example if the
 * data is retrieved from a database where content changes between calls. The
 * cache does not really care, the data will just be inconsistent for the user.
 *
 * All its public methods must be called from the UI thread.
 */

struct MLRange
{
    size_t offset = 0;
    size_t count = 0;

    MLRange() = default;

    MLRange(size_t offset, size_t count)
        : offset(offset)
        , count(count)
    {
    }

    bool isEmpty() {
        return count == 0;
    }

    bool contains(size_t index) {
        return index >= offset && index < offset + count;
    }
};

class MLListCache : public QObject
{
    Q_OBJECT

public:
    typedef std::unique_ptr<MLItem> ItemType;
public:
    static constexpr ssize_t COUNT_UNINITIALIZED = -1;

    MLListCache(MediaLib* medialib, ListCacheLoader<ItemType> *loader,
              size_t chunkSize = 100)
        : m_medialib(medialib)
        , m_loader(loader)
        , m_chunkSize(chunkSize) {}

    /**
     * Return the item at specified index
     *
     * This returns the local item (`nullptr` if not present), and does not
     * retrieve anything from the loader.
     */
    const ItemType *get(size_t index) const;

    /**
     * Return the first item in the cache for which the functor f returns true
     *
     * This returns the local item (`nullptr` if not present), and does not
     * retrieve anything from the loader.
     */
    const ItemType *find(const std::function<bool (const ItemType&)> &&f, int *index = nullptr) const;


    /**
     * Return the number of items or `COUNT_UNINITIALIZED`
     *
     * This returns the local count, and does not retrieve anything from the
     * loader.
     */
    ssize_t count() const;

    /**
     * Init the list size
     *
     * This method must be called at most once (the list size is not expected
     * to change).
     */
    void initCount();

    /**
     * Request to load data so that the item as index will become available
     */
    void refer(size_t index);

signals:
    /* useful for signaling QAbstractItemModel::modelAboutToBeReset() */
    void localSizeAboutToBeChanged(size_t size);

    void localSizeChanged(size_t size);
    void localDataChanged(size_t index, size_t count);

private:
    void asyncLoad(size_t offset, size_t count);
    void asyncCount();

    MediaLib* m_medialib = nullptr;

    /* Ownershipshared between this cache and the runnable spawned to execute
     * loader callbacks */
    QSharedPointer<ListCacheLoader<ItemType>> m_loader;
    size_t m_chunkSize;

    std::vector<ItemType> m_list;
    ssize_t m_total_count = COUNT_UNINITIALIZED;
    size_t m_offset = 0;

    bool m_countRequested = false;
    MLRange m_lastRangeRequested;

    uint64_t m_loadTask = 0;
    uint64_t m_countTask = 0;
};

#endif
