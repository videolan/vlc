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
#include "asynctask.hpp"

/**
 * `ListCache<T>` represents a cache for a (constant) list of items.
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
    /* useful for signaling QAbstractItemModel::modelAboutToBeReset() */
    void localSizeAboutToBeChanged(size_t size);

    void localSizeChanged(size_t size);
    void localDataChanged(size_t index, size_t count);

protected slots:
    virtual void onLoadResult() = 0;
    virtual void onCountResult() = 0;
};

template <typename T>
class CountTask;

template <typename T>
class LoadTask;

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

template <typename T>
class ListCache : public BaseListCache
{
public:
    static constexpr ssize_t COUNT_UNINITIALIZED = -1;

    ListCache(QThreadPool &threadPool, ListCacheLoader<T> *loader,
              size_t chunkSize = 100)
        : m_threadPool(threadPool)
        , m_loader(loader)
        , m_chunkSize(chunkSize) {}
    ~ListCache();

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
    void asyncLoad(size_t offset, size_t count);
    void onLoadResult() override;

    void asyncCount();
    void onCountResult() override;

    QThreadPool &m_threadPool;
    /* Ownershipshared between this cache and the runnable spawned to execute
     * loader callbacks */
    QSharedPointer<ListCacheLoader<T>> m_loader;
    size_t m_chunkSize;

    std::vector<T> m_list;
    ssize_t m_total_count = COUNT_UNINITIALIZED;
    size_t m_offset = 0;

    bool m_countRequested = false;
    MLRange m_lastRangeRequested;

    LoadTask<T> *m_loadTask = nullptr;
    CountTask<T> *m_countTask = nullptr;
};

template <typename T>
ListCache<T>::~ListCache()
{
    if (m_countTask)
        m_countTask->abandon();
    if (m_loadTask)
        m_loadTask->abandon();
}

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
    assert(!m_countRequested);
    asyncCount();
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
    if (!m_lastRangeRequested.contains(index))
    {
        /* FIXME bad heuristic if the interval of visible items crosses a cache
         * page boundary */
        size_t offset = index - index % m_chunkSize;
        size_t count = qMin(m_total_count - offset, m_chunkSize);
        asyncLoad(offset, count);
    }
}

template <typename T>
class CountTask : public AsyncTask<size_t>
{
public:
    CountTask(QSharedPointer<ListCacheLoader<T>> loader) : m_loader(loader) {}

    size_t execute() override
    {
        return m_loader->count();
    }

private:
    QSharedPointer<ListCacheLoader<T>> m_loader;
};

template <typename T>
void ListCache<T>::asyncCount()
{
    assert(!m_countTask);

    m_countTask = new CountTask<T>(m_loader);
    connect(m_countTask, &BaseAsyncTask::result,
            this, &ListCache<T>::onCountResult);
    m_countRequested = true;
    m_countTask->start(m_threadPool);
}

template <typename T>
void ListCache<T>::onCountResult()
{
    CountTask<T> *task = static_cast<CountTask<T> *>(sender());
    assert(task == m_countTask);

    m_offset = 0;
    m_list.clear();
    m_total_count = static_cast<ssize_t>(task->takeResult());
    emit localSizeChanged(m_total_count);

    task->abandon();
    m_countTask = nullptr;
}

template <typename T>
class LoadTask : public AsyncTask<std::vector<T>>
{
public:
    LoadTask(QSharedPointer<ListCacheLoader<T>> loader, size_t offset,
             size_t count)
        : m_loader(loader)
        , m_offset(offset)
        , m_count(count)
    {
    }

    std::vector<T> execute() override
    {
        return m_loader->load(m_offset, m_count);
    }

private:
    QSharedPointer<ListCacheLoader<T>> m_loader;
    size_t m_offset;
    size_t m_count;

    friend class ListCache<T>;
};

template <typename T>
void ListCache<T>::asyncLoad(size_t offset, size_t count)
{
    if (m_loadTask)
        /* Cancel any current pending task */
        m_loadTask->abandon();

    m_loadTask = new LoadTask<T>(m_loader, offset, count);
    connect(m_loadTask, &BaseAsyncTask::result,
            this, &ListCache<T>::onLoadResult);
    m_lastRangeRequested = { offset, count };
    m_loadTask->start(m_threadPool);
}

template <typename T>
void ListCache<T>::onLoadResult()
{
    LoadTask<T> *task = static_cast<LoadTask<T> *>(sender());
    assert(task == m_loadTask);

    m_offset = task->m_offset;
    m_list = task->takeResult();
    if (m_list.size())
        emit localDataChanged(m_offset, m_list.size());

    task->abandon();
    m_loadTask = nullptr;
}

#endif
