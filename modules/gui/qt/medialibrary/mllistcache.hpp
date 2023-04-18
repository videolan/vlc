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

#include <vlc_common.h>
#include <cassert>
#include <memory>
#include <vector>
#include <set>
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

    MLRange(const MLRange& other)
        : offset(other.offset)
        , count(other.count)
    {}

    MLRange& operator=(const MLRange& other)
    {
        offset = other.offset;
        count = other.count;
        return *this;
    }

    bool isEmpty() const {
        return count == 0;
    }

    bool contains(size_t index) const {
        return index >= offset && index < offset + count;
    }

    void reset()
    {
        offset = 0;
        count = 0;
    }

    ///returns the overlapping range of the current range and @a other
    MLRange overlap(const MLRange& other) const
    {
        if (isEmpty() || other.isEmpty())
            return MLRange{};
        if (contains(other.offset))
            return MLRange(other.offset, (offset + count) - other.offset);
        else if (other.contains(offset))
            return MLRange(offset, (other.offset + other.count) - offset);
        else
            return MLRange{};
    }
};

class MLListCache : public QObject
{
    Q_OBJECT

public:
    typedef std::unique_ptr<MLItem> ItemType;

public:
    static constexpr ssize_t COUNT_UNINITIALIZED = -1;

    MLListCache(MediaLib* medialib, std::unique_ptr<ListCacheLoader<ItemType>>&& loader,
        bool useMove, size_t chunkSize = 100);

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
     * replace item in the cache with the one provided. replacement is based on newItem MLItemId
     *
     * this returns the index of the replaced item, or -1 if the item is not in the cache
     */
    int updateItem(std::unique_ptr<MLItem>&& newItem);

    /**
     * Removes from the cache list given its item id
     *
     * it returns the index row when the id is found and removed, -1 otherwise
     */
    int deleteItem(const MLItemId& mlid);


    /**
     * move items in the cache contained between @a first
     * and @a last to the index @a to
     */
    void moveRange(int first, int last, int to);

    /**
     * remove items in the cache from the index @a first up to the
     * index @a last
     */
    void deleteRange(int first, int last);

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

    /*
     * reload
     */
    void invalidate();

signals:
    void localSizeChanged(size_t size);

    void localDataChanged(int sourceFirst, int sourceLast);

    void beginInsertRows(int sourceFirst, int sourceLast);
    void endInsertRows();

    void beginRemoveRows(int sourceFirst, int sourceLast);
    void endRemoveRows();

    void beginMoveRows(int sourceFirst, int sourceLast, int destination);
    void endMoveRows();

private:
    void asyncFetchMore();
    void asyncCountAndLoad();
    void partialUpdate();
    size_t fixupIndexForMove(size_t index) const;

    MediaLib* m_medialib = nullptr;

    bool m_useMove = false;

    /* Ownershipshared between this cache and the runnable spawned to execute
     * loader callbacks */
    QSharedPointer<ListCacheLoader<ItemType>> m_loader;
    size_t m_chunkSize;

    //highest index requested by the view (1 based, 0 is nothing referenced)
    size_t m_maxReferedIndex = 0;

    bool m_needReload = false;

    uint64_t m_appendTask = 0;
    uint64_t m_countTask = 0;

    struct CacheData {
        explicit CacheData(std::vector<ItemType>&& list_, size_t totalCount_)
            : list(std::move(list_))
            , totalCount(totalCount_)
        {
            loadedCount = list.size();
        }

        std::vector<ItemType> list;
        size_t totalCount = 0;
        size_t loadedCount = 0;
    };

    std::unique_ptr<CacheData> m_cachedData;
    std::unique_ptr<CacheData> m_oldData;

    MLRange m_rangeRequested;


    //access the list while it's being updated
    size_t m_partialIndex = 0;
    size_t m_partialX = 0;
    size_t m_partialLoadedCount = 0;

    //represent a redirection of items that are after m_partialIndex
    struct PartialIndexRedirect {
        enum class Operation
        {
            ADD,
            DEL
        };

        explicit PartialIndexRedirect(Operation op_, size_t index_, size_t count_, size_t x_ = 0)
            : op(op_)
            , index(index_)
            , count(count_)
        {
            if (op == Operation::ADD)
            {
                val.add.x = x_;
            }
        }
        Operation op;
        union {
            struct {
                size_t x;
            } add;
            struct {
            } del;
        } val;
        size_t index;
        size_t count;

        //for set ordering
        friend bool operator<(const PartialIndexRedirect& l, const PartialIndexRedirect& r)
        {
            return l.index < r.index;
        }

    };
    //store index redirection keeping index order
    std::set<PartialIndexRedirect> m_partialIndexRedirect;
};

#endif
