/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#ifndef LOCAL_CACHE_LOADER_HPP
#define LOCAL_CACHE_LOADER_HPP

#include <vector>
#include <unordered_set>
#include <algorithm>

#include <QObject>
#include "listcache.hpp"

class LocalListCacheLoaderObj: public QObject
{
    Q_OBJECT

protected:
    inline size_t runTaskAsync(std::function<void(size_t taskId)> task)
    {
        size_t taskId = ++m_taskCounter;
        m_validTasks.insert(taskId);

        QMetaObject::invokeMethod(
            this, [this, taskId, task]() {
                auto taskIter = m_validTasks.find(taskId);
                if (taskIter == m_validTasks.cend())
                    return;
                m_validTasks.erase(taskIter);

                task(taskId);
            }, Qt::QueuedConnection);

        return taskId;
    }

    size_t m_taskCounter = 0;
    std::unordered_set<size_t> m_validTasks = {};
};

/**
 * @brief The LocalListCacheLoader class is designed to act like a ListCache loader for data that
 * are stored in memory. it provide, sorting, filtering and update events through the diffutil mechanism
 *
 * ItemType will be copied locally so it must be easilly copyable (sharedptr or primitive types)
 *
 * model must implement ModelSource interface to retreive data, and data revision
 *
 * data is sorted and filtered on the UI thread
 */
template<typename T>
class LocalListCacheLoader: public LocalListCacheLoaderObj, public ListCacheLoader<T>
{
public:
    using ItemType = T;
    using ItemList = std::vector<ItemType>;
    using ItemCompare =  std::function<bool(const ItemType&, const ItemType&)>;

    class ModelSource
    {
    public:
        virtual ~ModelSource()
        {
            // At this point, pure virtual methods are not available.
            m_isDying = true;
        }

        //the revision must be incremented each time the model changes
        virtual size_t getModelRevision() const = 0;

        //return the data matching the pattern
        virtual std::vector<ItemType> getModelData(const QString& pattern) const = 0;

        bool isDying() const { return m_isDying; }

    private:
        bool m_isDying = false;
    };

public:
    LocalListCacheLoader(
        const ModelSource* source,
        const QString& pattern,
        ItemCompare compare)
        : LocalListCacheLoaderObj()
        , m_source(source)
        , m_compare(compare)
        , m_pattern(pattern)
    {
    }

    void cancelTask(size_t taskId) override
    {
        m_validTasks.erase(taskId);
    }

    size_t countTask(std::function<void(size_t taskId, size_t count)> cb) override
    {
        return runTaskAsync([this, cb](size_t taskId) {
            if (!updateData())
                return;
            cb(taskId, m_items.size());
        });
    }

    size_t loadTask(
        size_t offset, size_t limit,
        std::function<void(size_t taskId, std::vector<ItemType>& data)> cb) override
    {
         return runTaskAsync([this, offset, limit, cb](size_t taskId) {
            if (!updateData())
                return;
            std::vector<ItemType> data;
            if (offset < m_items.size())
            {
                if (limit == 0 || offset + limit > m_items.size())
                {
                    std::copy(m_items.cbegin() + offset, m_items.cend(), std::back_inserter(data));
                }
                else
                {
                    data.reserve(limit);
                    std::copy_n(m_items.cbegin() + offset, limit, std::back_inserter(data));
                }
            }
            cb(taskId, data);
        });
    }

    size_t countAndLoadTask(
        size_t offset, size_t limit,
        std::function<void(size_t taskId, size_t count, std::vector<ItemType>& data)> cb) override
    {
        return runTaskAsync([this, offset, limit, cb](size_t taskId) {
            if (!updateData())
                return;
            std::vector<ItemType> data;
            if (offset < m_items.size())
            {
                if (limit == 0 || offset + limit > m_items.size())
                {
                    std::copy(m_items.cbegin() + offset, m_items.cend(), std::back_inserter(data));
                }
                else
                {
                    data.reserve(limit);
                    std::copy_n(m_items.cbegin() + offset, limit, std::back_inserter(data));
                }
            }
            cb(taskId, m_items.size(), data);
        });
    }

    bool updateData()
    {
        if (m_source->isDying())
            return false;

        if (m_revision == m_source->getModelRevision())
            return true;

        m_items = m_source->getModelData(m_pattern);
        std::sort(m_items.begin(), m_items.end(), m_compare);

        m_revision = m_source->getModelRevision();

        return true;
    }

    const ModelSource* m_source;
    ItemCompare m_compare;
    QString m_pattern;
    ItemList m_items;
    size_t m_revision = 0;
};

#endif
