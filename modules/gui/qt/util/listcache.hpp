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
#include <memory>
#include <vector>
#include <QtGlobal>
#include <QObject>

/**
 * `ListCache<T>` represents a cache for a (constant) list of items.
 *
 * The caller must provide a `ListCacheLoader<T>`, defining the following
 * methods:
 *  - `count()` returns the number of items in the list;
 *  - `load(index, count)` returning the items for the requested interval.
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
 * cache does not really care, the data will just be inconcistent for the user.
 *
 * All its public methods must be called from the UI thread.
 */

template <typename T>
struct ListCacheLoader
{
    virtual ~ListCacheLoader() = default;
    virtual size_t count() const = 0;
    virtual std::vector<T> load(size_t index, size_t count) const = 0;
};

/* Non-template class for signals */
class BaseListCache : public QObject
{
    Q_OBJECT

signals:
    void localDataChanged(size_t index, size_t count);
};

template <typename T>
class ListCache : public BaseListCache
{
public:
    static constexpr ssize_t COUNT_UNINITIALIZED = -1;

    ListCache(std::unique_ptr<ListCacheLoader<T>> loader, size_t chunkSize = 100)
        : m_loader(std::move(loader))
        , m_chunkSize(chunkSize) {}

    /**
     * Return the item at specified index
     *
     * This returns the local item (`nullptr` if not present), and does not
     * retrieve anything from the loader.
     */
    const T *get(size_t index) const;


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

private:
    std::unique_ptr<ListCacheLoader<T>> m_loader;
    size_t m_chunkSize;

    std::vector<T> m_list;
    ssize_t m_total_count = COUNT_UNINITIALIZED;
    size_t m_offset = 0;
};

template <typename T>
const T *ListCache<T>::get(size_t index) const
{
    assert(m_total_count >= 0 && index < static_cast<size_t>(m_total_count));
    if (index < m_offset || index >= m_offset + m_list.size())
        return nullptr;

    return &m_list[index - m_offset];
}

template <typename T>
ssize_t ListCache<T>::count() const
{
    return m_total_count;
}

template <typename T>
void ListCache<T>::initCount()
{
    assert(m_total_count == COUNT_UNINITIALIZED);
    m_total_count = static_cast<ssize_t>(m_loader->count());
}

template <typename T>
void ListCache<T>::refer(size_t index)
{
    if (m_total_count == -1 || index >= static_cast<size_t>(m_total_count))
    {
        /*
         * The request is incompatible with the total count of the list.
         *
         * Either the count is not retrieved yet, or the content has changed in
         * the loader source.
         */
        return;
    }

    /* index outside the known portion of the list */
    if (index < m_offset || index >= m_offset + m_list.size())
    {
        /* FIXME bad heuristic if the interval of visible items crosses a cache
         * page boundary */
        m_offset = index - index % m_chunkSize;
        size_t count = qMin(m_total_count - m_offset, m_chunkSize);
        m_list = m_loader->load(m_offset, count);
        if (m_list.size())
            emit localDataChanged(m_offset, m_list.size());
    }
}

#endif
