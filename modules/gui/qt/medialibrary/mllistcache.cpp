/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#include "mllistcache.hpp"
#include <vlc_diffutil.h>

namespace {

//callbacks for the diff algorithm to access the data

uint32_t cacheDataLength(const void* data)
{
    auto cache = static_cast<const MLListCache::CacheData*>(data);
    assert(cache);
    return std::min(cache->loadedCount, cache->queryCount);
}

bool cacheDataCompare(const void* dataOld, uint32_t oldIndex, const void* dataNew,  uint32_t newIndex)
{
    auto cacheOld = static_cast<const MLListCache::CacheData*>(dataOld);
    auto cacheNew = static_cast<const MLListCache::CacheData*>(dataNew);
    assert(cacheOld);
    assert(cacheNew);
    assert(oldIndex < cacheOld->list.size());
    assert(newIndex < cacheNew->list.size());
    return cacheNew->list[newIndex]->getId() == cacheOld->list[oldIndex]->getId();
}

}

MLListCache::MLListCache(MediaLib* medialib, std::unique_ptr<ListCacheLoader<MLListCache::ItemType>>&& loader, bool useMove, size_t limit, size_t offset, size_t chunkSize)
    : m_medialib(medialib)
    , m_useMove(useMove)
    , m_loader(loader.release())
    , m_chunkSize(chunkSize)
    , m_limit(limit)
    , m_offset(offset)
{
    assert(medialib);
}

size_t MLListCache::fixupIndexForMove(size_t index) const
{
    //theses elements have already been moved
    for (const PartialIndexRedirect& hole : m_partialIndexRedirect)
    {
        if (hole.op == PartialIndexRedirect::Operation::DEL)
        {
            if (hole.index <= index)
                index += hole.count;
            else
                break;
        }
        else
        {
            if (hole.index <= index)
            {
                if (index <= hole.index + hole.count - 1)
                    return hole.val.add.x + (index - hole.index);
                else
                    index -= hole.count;
            }
            else
                break;
        }
    }
    return index;
}

const MLListCache::ItemType* MLListCache::get(size_t index) const
{
    //the view may access the model while we're updating it
    //everything before m_partialIndex is updated in the new model,
    //everything after m_partialIndex is still valid in the old model
    if (unlikely(m_oldData))
    {
        if (m_cachedData)
        {
            if (index >= m_partialLoadedCount)
                return nullptr;
            else if (index >= m_partialIndex)
            {
                if (m_useMove)
                {
                    index = fixupIndexForMove(index);
                }
                return &m_oldData->list.at(index + (m_partialX - m_partialIndex));
            }
            else
                return &m_cachedData->list.at(index);
        }
        else
        {
            if (index >= m_oldData->loadedCount)
                return nullptr;

            return &m_oldData->list.at(index);
        }
    }

    if (!m_cachedData)
        return nullptr;

    if (index + 1 > m_cachedData->loadedCount)
        return nullptr;

    return &m_cachedData->list.at(index);
}

const MLListCache::ItemType* MLListCache::find(const std::function<bool (const MLListCache::ItemType&)> &&f, int *index) const
{

    if (!m_cachedData || m_cachedData->queryCount == 0)
        return nullptr;

    auto it = std::find_if(m_cachedData->list.cbegin(), m_cachedData->list.cend(), f);
    if (it == m_cachedData->list.cend())
        return nullptr;

    if (index)
        *index = std::distance(m_cachedData->list.cbegin(), it);

    return &(*it);
}

int MLListCache::updateItem(std::unique_ptr<MLItem>&& newItem)
{
    //we can't update an item locally while the model has pending updates
    //no worry, we'll receive the update once the actual model notifies us
    if (m_oldData)
        return -1;

    //we can't update an item before we have any cache
    if (unlikely(!m_cachedData))
        return -1;

    MLItemId mlid = newItem->getId();
    //this may be inneficient to look at every items, maybe we can have a hashmap to access the items by id
    auto it = std::find_if(m_cachedData->list.begin(), m_cachedData->list.end(), [mlid](const ItemType& item) {
        return (item->getId() == mlid);
    });
    //item not found
    if (it == m_cachedData->list.end())
        return -1;

    int pos = std::distance(m_cachedData->list.begin(), it);
    *it = std::move(newItem);
    emit localDataChanged(pos, pos);
    return pos;
}

int MLListCache::deleteItem(const MLItemId& mlid)
{
    //we can't update an item locally while the model has pending updates
    //no worry, we'll receive the update once the actual model notifies us
    if (m_oldData)
        return -1;

    //we can't remove an item before we have any cache
    if (unlikely(!m_cachedData))
        return -1;

    auto it = std::find_if(m_cachedData->list.begin(), m_cachedData->list.end(), [mlid](const ItemType& item) {
        return (item->getId() == mlid);
    });

    //item not found
    if (it == m_cachedData->list.end())
        return -1;

    int pos = std::distance(m_cachedData->list.begin(), it);

    emit beginRemoveRows(pos, pos);
    m_cachedData->list.erase(it, it+1);
    size_t delta = m_cachedData->loadedCount - m_cachedData->list.size();
    m_cachedData->loadedCount -= delta;
    m_cachedData->maximumCount -= delta;
    m_cachedData->queryCount -= delta;
    emit endRemoveRows();
    emit localSizeChanged(m_cachedData->queryCount, m_cachedData->maximumCount);

    return pos;
}

void MLListCache::moveRange(int first, int last, int to)
{
    assert(first <= last);
    if (first <= to && to <= last)
        return;

    if (unlikely(!m_cachedData))
        return;

    emit beginMoveRows(first, last, to);
    auto it = m_cachedData->list.begin();
    //build a temporary list with the items in order
    std::vector<ItemType> tmpList;
    if (to < first)
    {
        std::move(it, it+to, std::back_inserter(tmpList));
        std::move(it+first, it+(last+1), std::back_inserter(tmpList));
        std::move(it+to, it+first, std::back_inserter(tmpList));
        std::move(it+(last+1), m_cachedData->list.end(), std::back_inserter(tmpList));
    }
    else //last < to
    {
        std::move(it, it+first, std::back_inserter(tmpList));
        std::move(it+last+1, it+to, std::back_inserter(tmpList));
        std::move(it+first, it+(last+1), std::back_inserter(tmpList));
        std::move(it+to, m_cachedData->list.end(), std::back_inserter(tmpList));
    }

    m_cachedData->list = std::move(tmpList);
    emit endMoveRows();
}

void MLListCache::deleteRange(int first, int last)
{
    if (unlikely(!m_cachedData))
        return;
    assert(first <= last);
    assert(first >= 0);
    assert(static_cast<size_t>(last) < m_cachedData->queryCount);
    emit beginRemoveRows(first, last);
    if (static_cast<size_t>(first) < m_cachedData->loadedCount)
    {
        auto itFirst = std::next(m_cachedData->list.begin(), first);

        auto itLast = m_cachedData->list.begin();
        if (static_cast<size_t>(last) < m_cachedData->loadedCount)
            itLast = std::next(itLast, last + 1);
        else
            itLast = m_cachedData->list.end();

        m_cachedData->list.erase(itFirst, itLast);
        m_cachedData->loadedCount = m_cachedData->list.size();
    }
    //else data are not loaded, just mark them as deleted

    int rangeSize =  (last + 1) - first;
    m_cachedData->queryCount -= rangeSize;
    m_cachedData->maximumCount -= rangeSize;
    emit endRemoveRows();
    emit localSizeChanged(m_cachedData->queryCount, m_cachedData->maximumCount);
}

ssize_t MLListCache::queryCount() const
{
    if (m_cachedData)
        return m_cachedData->queryCount;
    else if (m_oldData)
        return m_oldData->queryCount;
    else
        return COUNT_UNINITIALIZED;
}


ssize_t MLListCache::maximumCount() const
{
    if (m_cachedData)
        return m_cachedData->maximumCount;
    else if (m_oldData)
        return m_oldData->maximumCount;
    else
        return COUNT_UNINITIALIZED;
}

void MLListCache::initCount()
{
    assert(!m_cachedData);
    asyncCountAndLoad();
}

void MLListCache::refer(size_t index)
{
    //m_maxReferedIndex is in terms of number of item, not the index
    index++;

    if (!m_cachedData)
        return;

    if (index > m_cachedData->queryCount)
        return;

   /* index is already in the list */
    if (index <= m_cachedData->loadedCount)
        return;

    if (index > m_maxReferedIndex)
    {
        m_maxReferedIndex = index;
        if (!m_appendTask && !m_countTask)
        {
            if (m_cachedData)
                asyncFetchMore();
            else
                asyncCountAndLoad();
        }
    }
}

void MLListCache::invalidate()
{
    if (m_cachedData)
    {
        if (m_cachedData && !m_oldData)
        {
            m_oldData = std::move(m_cachedData);
            m_partialX = 0;
        }
        else
            m_cachedData.reset();
    }

    if (m_appendTask)
    {
        m_medialib->cancelMLTask(this, m_appendTask);
        m_appendTask = 0;
    }

    if (m_countTask)
    {
        m_needReload = true;
    }
    else
    {
        asyncCountAndLoad();
    }
}

void MLListCache::partialUpdate()
{
    //compare the model the user have and the updated model
    //and notify for changes

    vlc_diffutil_callback_t diffOp = {
        cacheDataLength,
        cacheDataLength,
        cacheDataCompare
    };

    diffutil_snake_t* snake = vlc_diffutil_build_snake(&diffOp, m_oldData.get(), m_cachedData.get());
    int diffutilFlags = VLC_DIFFUTIL_RESULT_AGGREGATE;
    if (m_useMove)
        diffutilFlags |= VLC_DIFFUTIL_RESULT_MOVE;

    vlc_diffutil_changelist_t* changes = vlc_diffutil_build_change_list(
        snake, &diffOp, m_oldData.get(), m_cachedData.get(),
        diffutilFlags);

    m_partialIndex = 0;
    m_partialLoadedCount = m_oldData->loadedCount;
    size_t partialQueryCount = m_oldData->queryCount;
    size_t partialMaximumCount = m_oldData->maximumCount;
    for (size_t i = 0; i < changes->size; i++)
    {
        vlc_diffutil_change_t& op = changes->data[i];
        switch (op.type)
        {
        case VLC_DIFFUTIL_OP_IGNORE:
            break;
        case VLC_DIFFUTIL_OP_INSERT:
            m_partialX = op.op.insert.x;
            m_partialIndex = op.op.insert.index;
            emit beginInsertRows(op.op.insert.index, op.op.insert.index + op.count - 1);
            m_partialIndex += op.count;
            m_partialLoadedCount += op.count;
            partialQueryCount += op.count;
            partialMaximumCount += op.count;
            emit endInsertRows();
            emit localSizeChanged(partialQueryCount, partialMaximumCount);
            break;
        case VLC_DIFFUTIL_OP_REMOVE:
            m_partialX = op.op.remove.x;
            m_partialIndex = op.op.remove.index;
            emit beginRemoveRows(op.op.remove.index, op.op.remove.index + op.count - 1);
            m_partialLoadedCount -= op.count;
            m_partialX += op.count;
            partialQueryCount -= op.count;
            partialMaximumCount -= op.count;
            emit endRemoveRows();
            emit localSizeChanged(partialQueryCount, partialMaximumCount);
            break;
        case VLC_DIFFUTIL_OP_MOVE:
            m_partialX = op.op.move.x;
            if (op.op.move.from > op.op.move.to)
            {
                m_partialIndex = op.op.move.to;
                emit beginMoveRows(op.op.move.from, op.op.move.from + op.count - 1, op.op.move.to);
                m_partialIndexRedirect.insert(PartialIndexRedirect(PartialIndexRedirect::Operation::DEL, op.op.move.from, op.count));
                m_partialIndex += op.count;
            }
            else
            {
                m_partialIndex = op.op.move.from + op.count - 1;
                emit beginMoveRows(op.op.move.from, op.op.move.from + op.count - 1, op.op.move.to);
                m_partialIndexRedirect.insert(PartialIndexRedirect(PartialIndexRedirect::Operation::ADD, op.op.move.to, op.count, op.op.move.x));
                m_partialIndex = op.op.move.from + 1;
                m_partialX += op.count;
            }
            emit endMoveRows();
            break;
        }
    }
    vlc_diffutil_free_change_list(changes);
    vlc_diffutil_free_snake(snake);

    //ditch old model
    if (m_useMove)
        m_partialIndexRedirect.clear();
    m_oldData.reset();

    //if we have change outside our cache
    //just notify for addition/removal at a the end of the list
    if (partialQueryCount != m_cachedData->queryCount)
    {
        if (partialQueryCount > m_cachedData->queryCount)
        {
            emit beginRemoveRows(m_cachedData->queryCount - 1, partialQueryCount - 1);
            emit endRemoveRows();
            emit localSizeChanged(m_cachedData->queryCount, m_cachedData->maximumCount);
        }
        else
        {
            emit beginInsertRows(partialQueryCount - 1, m_cachedData->queryCount - 1);
            emit endInsertRows();
            emit localSizeChanged(m_cachedData->queryCount, m_cachedData->maximumCount);
        }
    }
}

void MLListCache::asyncCountAndLoad()
{
    if (m_countTask)
        m_medialib->cancelMLTask(this, m_countTask);

    size_t count = std::max(m_maxReferedIndex, m_chunkSize);

    struct Ctx {
        size_t maximumCount;
        std::vector<ItemType> list;
    };

    m_countTask = m_medialib->runOnMLThread<Ctx>(this,
        //ML thread
        [loader = m_loader, offset = m_offset, count = count](vlc_medialibrary_t* ml, Ctx& ctx)
        {
            ctx.list = loader->load(ml, offset, count);
            ctx.maximumCount = loader->count(ml);
        },
        //UI thread
        [this](quint64 taskId, Ctx& ctx)
        {
            if (m_countTask != taskId)
                return;

            //quite unlikley but model may change between count and load
            if (unlikely(ctx.list.size() > ctx.maximumCount))
            {
                ctx.maximumCount = ctx.list.size();
                m_needReload = true;
            }

            size_t queryCount = (m_limit > 0)
                ? std::min(m_limit, ctx.maximumCount)
                : ctx.maximumCount;
            //note: should we drop items past queryCount?
            m_cachedData = std::make_unique<CacheData>(std::move(ctx.list),
                                                       queryCount,
                                                       ctx.maximumCount);

            if (m_oldData)
            {
                partialUpdate();
            }
            else
            {
                if (m_cachedData->queryCount > 0)
                {
                    //no previous data, we insert everything
                    emit beginInsertRows(0, m_cachedData->queryCount - 1);
                    emit endInsertRows();
                    emit localSizeChanged(m_cachedData->queryCount, m_cachedData->maximumCount);
                }
                else
                    emit localSizeChanged(0, m_cachedData->maximumCount);
            }

            m_countTask = 0;
            if (m_needReload)
            {
                m_needReload  = false;
                m_oldData = std::move(m_cachedData);
                m_partialX = 0;
                asyncCountAndLoad();
            }
            else if (m_maxReferedIndex < m_cachedData->loadedCount)
            {
                m_maxReferedIndex = m_cachedData->loadedCount;
            }
            else if (m_maxReferedIndex > m_cachedData->loadedCount
                && m_maxReferedIndex <= m_cachedData->queryCount)
            {
                asyncFetchMore();
            }
        }
    );
}

void MLListCache::asyncFetchMore()
{
    if (m_maxReferedIndex <= m_cachedData->loadedCount)
        return;

    assert(m_cachedData);
    if (m_appendTask)
        m_medialib->cancelMLTask(this, m_appendTask);

    m_maxReferedIndex = std::min(m_cachedData->queryCount, m_maxReferedIndex);
    size_t count = ((m_maxReferedIndex - m_cachedData->loadedCount) / m_chunkSize + 1 ) * m_chunkSize;

    struct Ctx {
        std::vector<ItemType> list;
    };
    m_appendTask = m_medialib->runOnMLThread<Ctx>(this,
        //ML thread
        [loader = m_loader, offset = m_offset + m_cachedData->loadedCount, count]
        (vlc_medialibrary_t* ml, Ctx& ctx)
        {
            ctx.list = loader->load(ml, offset, count);
        },
        //UI thread
        [this](quint64 taskId, Ctx& ctx)
        {
            if (taskId != m_appendTask)
                return;

            assert(m_cachedData);

            int updatedCount = ctx.list.size();
            if (updatedCount >= 0)
            {
                int updatedOffset = m_cachedData->loadedCount;
                std::move(ctx.list.begin(), ctx.list.end(), std::back_inserter(m_cachedData->list));
                m_cachedData->loadedCount += updatedCount;
                emit localDataChanged(updatedOffset, updatedOffset + updatedCount - 1);
            }

            m_appendTask = 0;
            if (m_maxReferedIndex > m_cachedData->loadedCount)
            {
                asyncFetchMore();
            }
        }
    );
}
