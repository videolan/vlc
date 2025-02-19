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

#ifndef BASEMODEL_P_HPP
#define BASEMODEL_P_HPP

#include <memory>
#include "listcacheloader.hpp"
#include "listcache.hpp"
#include "base_model.hpp"

class BaseModelPrivate {
    Q_DECLARE_PUBLIC(BaseModel)
public:
    BaseModelPrivate(BaseModel * pub)
        : q_ptr(pub)
    {
    }
    virtual ~BaseModelPrivate() = default;
    virtual void validateCache() const = 0;
    virtual void resetCache() = 0;
    virtual void invalidateCache() = 0;
    virtual bool loading() const = 0;

    virtual unsigned int getLoadedCount() const = 0;
    virtual unsigned int getCount() const = 0;
    virtual unsigned int getMaximumCount() const = 0;

    virtual bool initializeModel() = 0;

protected:
    virtual bool cachable() const
    {
        return !m_qmlInitializing;
    }

    //accessing procted functions from BaseModel must be done here
    //has friendship isn't inherited
    template<typename T>
    void validateCacheImpl(T* cache) const
    {
        Q_Q(const BaseModel);

        QObject::connect(cache, &T::localSizeChanged,
                q, &BaseModel::onLocalSizeChanged);

        QObject::connect(cache, &T::localDataChanged,
                q, &BaseModel::onCacheDataChanged);

        QObject::connect(cache, &T::beginInsertRows,
                q, &BaseModel::onCacheBeginInsertRows);
        QObject::connect(cache, &T::endInsertRows,
                q, &BaseModel::endInsertRows);

        QObject::connect(cache, &T::beginRemoveRows,
                q, &BaseModel::onCacheBeginRemoveRows);
        QObject::connect(cache, &T::endRemoveRows,
                q, &BaseModel::endRemoveRows);

        QObject::connect(cache, &T::endMoveRows,
                q, &BaseModel::endMoveRows);
        QObject::connect(cache, &T::beginMoveRows,
                q, &BaseModel::onCacheBeginMoveRows);

        cache->initCount();
    }

    template<typename T>
    void resetCacheImpl(T& cache)
    {
        Q_Q(BaseModel);
        emit q->beginResetModel();
        // 'abandon' existing cache and queue it for deletion
        if (cache)
        {
            cache->disconnect(q);
            cache->deleteLater();
            cache.release();
        }
        emit q->endResetModel();
        validateCache();
    }

    template <typename T>
    void invalidateCacheImpl(T& cache)
    {
        Q_Q(BaseModel);
        if (cache)
        {
            cache->invalidate();
            emit q->loadingChanged();
        }
        else
            validateCache();
    }

    BaseModel* q_ptr = nullptr;

    QString m_searchPattern = {};
    Qt::SortOrder m_sortOrder = Qt::SortOrder::AscendingOrder;
    QString m_sortCriteria = {};
    unsigned int m_limit = 0;
    unsigned int m_offset = 0;
    bool m_qmlInitializing = false;
};

template<typename T>
class BaseModelPrivateT : public BaseModelPrivate
{
public:
    using BaseModelPrivate::BaseModelPrivate;
    virtual ~BaseModelPrivateT() = default;

public:
    void validateCache() const override;
    void resetCache() override;
    void invalidateCache() override;

    bool loading() const override;

    const T* item(int signedidx) const;

    unsigned int getCount() const override;
    unsigned int getLoadedCount() const override;
    unsigned int getMaximumCount() const override;

    virtual std::unique_ptr<ListCacheLoader<T>> createLoader() const = 0;

protected:
    mutable std::unique_ptr<ListCache<T>> m_cache;
};


template<typename T>
unsigned BaseModelPrivateT<T>::getCount() const
{
    if (!m_cache)
        return 0;

    ssize_t queryCount = m_cache->queryCount();
    if (queryCount == ListCache<T>::COUNT_UNINITIALIZED)
        return 0;

    return static_cast<unsigned>(queryCount);
}

template<typename T>
unsigned BaseModelPrivateT<T>::getLoadedCount() const
{
    if (!m_cache)
        return 0;

    ssize_t loadedCount = m_cache->loadedCount();
    if (loadedCount == ListCache<T>::COUNT_UNINITIALIZED)
        return 0;

    return static_cast<unsigned>(loadedCount);
}

template<typename T>
unsigned BaseModelPrivateT<T>::getMaximumCount() const
{
    if (!m_cache)
        return 0;

    ssize_t maximumCount = m_cache->maximumCount();
    if (maximumCount == ListCache<T>::COUNT_UNINITIALIZED)
        return 0;
    return static_cast<unsigned>(maximumCount);
}

template<typename T>
const T* BaseModelPrivateT<T>::item(int signedidx) const
{
    if (!m_cache)
        return nullptr;

    ssize_t count = m_cache->queryCount();

    if (count == 0 || signedidx < 0 || signedidx >= count)
        return nullptr;

    unsigned int idx = static_cast<unsigned int>(signedidx);

    m_cache->refer(idx);

    return m_cache->get(idx);
}

template<typename T>
void BaseModelPrivateT<T>::validateCache() const
{
    if (m_cache)
        return;

    if (!cachable())
        return;

    auto loader = createLoader();
    m_cache = std::make_unique<ListCache<T>>(std::move(loader), false, m_limit, m_offset);
    validateCacheImpl(m_cache.get());
}


template<typename T>
void BaseModelPrivateT<T>::resetCache()
{
    resetCacheImpl(m_cache);
}

template<typename T>
void BaseModelPrivateT<T>::invalidateCache()
{
    invalidateCacheImpl(m_cache);
}

template<typename T>
bool BaseModelPrivateT<T>::loading() const
{
    bool loading = !m_cache || (m_cache->queryCount() == ListCache<T>::COUNT_UNINITIALIZED);
    return loading;
}

#endif // BASEMODEL_P_HPP
