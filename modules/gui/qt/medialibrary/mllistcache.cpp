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

const MLListCache::ItemType* MLListCache::get(size_t index) const
{
    assert(m_total_count >= 0 && index < static_cast<size_t>(m_total_count));
    if (index < m_offset || index >= m_offset + m_list.size())
        return nullptr;

    return &m_list[index - m_offset];
}

const MLListCache::ItemType* MLListCache::find(const std::function<bool (const MLListCache::ItemType&)> &&f, int *index) const
{
    if (m_total_count <= 0)
        return nullptr;

    for (auto iter = std::begin(m_list); iter != std::end(m_list); ++iter)
    {
        if (f(*iter))
        {
            if (index)
                *index = m_offset + std::distance(std::begin(m_list), iter);

            return &(*iter);
        }
    }

    return nullptr;
}

ssize_t MLListCache::count() const
{
    return m_total_count;
}

void MLListCache::initCount()
{
    assert(!m_countRequested);
    asyncCount();
}

void MLListCache::refer(size_t index)
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

void MLListCache::asyncCount()
{
    assert(!m_countTask);

    m_countRequested = true;
    struct Ctx {
        ssize_t totalCount;
    };
    m_medialib->runOnMLThread<Ctx>(this,
        //ML thread
        [loader = m_loader](vlc_medialibrary_t* ml, Ctx& ctx)
        {
            ctx.totalCount = loader->count(ml);
        },
        //UI thread
        [this](quint64, Ctx& ctx){
            m_total_count = ctx.totalCount;
            m_countTask = 0;
            emit localSizeChanged(m_total_count);
        }
    );
}

void MLListCache::asyncLoad(size_t offset, size_t count)
{
    if (m_loadTask)
        m_medialib->cancelMLTask(this, m_loadTask);

    m_lastRangeRequested = { offset, count };
    struct Ctx {
        std::vector<ItemType> list;
    };
    m_loadTask = m_medialib->runOnMLThread<Ctx>(this,
        //ML thread
        [loader = m_loader, offset, count]
        (vlc_medialibrary_t* ml, Ctx& ctx)
        {
            ctx.list = loader->load(ml, offset, count);
        },
        //UI thread
        [this, offset](quint64, Ctx& ctx)
        {
            m_loadTask = 0;

            m_offset = offset;
            m_list = std::move(ctx.list);
            if (m_list.size())
                emit localDataChanged(offset, m_list.size());
        }
    );
}
