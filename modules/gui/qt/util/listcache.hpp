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
#include "listcacheloader.hpp"

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

class ListCacheBase : public QObject
{
    Q_OBJECT
signals:
    void localSizeChanged(size_t querySize, size_t maximumSize);

    void localDataChanged(int sourceFirst, int sourceLast);

    void beginInsertRows(int sourceFirst, int sourceLast);
    void endInsertRows();

    void beginRemoveRows(int sourceFirst, int sourceLast);
    void endRemoveRows();

    void beginMoveRows(int sourceFirst, int sourceLast, int destination);
    void endMoveRows();
};

/**
 * `ListCache` represents a cache for a (constant) list of items.
 *
 * The caller must provide a `ListCacheLoader<T>` to load and count data
 *
 * The cache will load data by chunk.
 * When data is invalidated, it will use a differentiation algorithm to provide
 * update events on data that changes
 *
 * All its public methods must be called from the UI thread.
 */
template<typename T>
class ListCache : public ListCacheBase
{
public:
    using ItemType = T;

    struct CacheData {
        explicit CacheData(std::vector<ItemType>&& list_,
                           size_t queryCount_,
                           size_t maximumCount_)
            : list(std::move(list_))
            , queryCount(queryCount_)
            , maximumCount(maximumCount_)
        {
            loadedCount = list.size();
        }

        std::vector<ItemType> list;
        //How many items are does the query returns min(maximumCount - offset, limit)
        size_t queryCount = 0;
        //how many items in the table
        size_t maximumCount = 0;
        //how many items are loaded (list.size)
        size_t loadedCount = 0;
    };

public:
    static constexpr ssize_t COUNT_UNINITIALIZED = -1;

    ListCache(std::unique_ptr<ListCacheLoader<ItemType>>&& loader,
        bool useMove, size_t limit, size_t offset, size_t chunkSize = 100);

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
    int updateItem(ItemType&& newItem);

    /**
     * insert an item in the cache and notify changes
     */
    void insertItem(ItemType&& item, int position);

    /**
     * @brief insertItemList insert items in the cache at position p position and
     * notify changes
     * @param first iterator to the first element to insert (list.begin)
     * @param last iterator past the last element to insert (list.end())
     * @param position insertion position 0 is equivalent to push_front
     * @note items are "moved" into the cache
     */
    template<typename IterType>
    void insertItemList(IterType first, IterType last, int position);

    /**
     * Removes from the cache list given its item id
     *
     * it returns the index row when the id is found and removed, -1 otherwise
     */
    int deleteItem(const std::function<bool (const ItemType&)> &&f);


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
     * @return the number of items or `COUNT_UNINITIALIZED`
     *
     * This returns the local count, and does not retrieve anything from the
     * loader.
     */
    ssize_t queryCount() const;

    /**
     * @return the total number of elements in the list, without taking @ref m_limit in account.
     * COUNT_UNINITIALIZED is returned if the list isn't initialized
     *
     * This may be usefull to know whether there are additionnal elements past the limit
     */
    ssize_t maximumCount() const;

    /**
     * @return underlying list size
     * COUNT_UNINITIALIZED is returned if the list isn't initialized
     *
     * This may be usefull to know loaded size
     */
    ssize_t loadedCount() const;

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

    /**
     * Request to fetch more data
     */
    void fetchMore();

    /*
     * reload
     */
    void invalidate();



private:
    static uint32_t cacheDataLength(const void* data);
    static bool cacheDataCompare(const void* dataOld, uint32_t oldIndex, const void* dataNew,  uint32_t newIndex);
    //this function must be specialized
    static bool compareItems(const ItemType& a, const ItemType& b);

    void asyncFetchMore();
    void asyncCountAndLoad();
    void partialUpdate();
    size_t fixupIndexForMove(size_t index) const;

    bool m_useMove = false;

    /* Ownershipshared between this cache and the runnable spawned to execute
     * loader callbacks */
    QSharedPointer<ListCacheLoader<ItemType>> m_loader;
    size_t m_chunkSize;

    //0 limit means no limit
    size_t m_limit = 0;
    size_t m_offset = 0;

    //highest index requested by the view (1 based, 0 is nothing referenced)
    size_t m_maxReferedIndex = 0;

    bool m_needReload = false;

    uint64_t m_appendTask = 0;
    uint64_t m_countTask = 0;

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

#include "listcache.hxx"

#endif
