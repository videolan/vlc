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

#include "base_model.hpp"
#include "base_model_p.hpp"

BaseModel::BaseModel(BaseModelPrivate* priv, QObject* parent)
    : QAbstractListModel(parent)
    , d_ptr(priv)
{
    connect(this, &BaseModel::offsetChanged,  this, &BaseModel::resetCache);
    connect(this, &BaseModel::limitChanged,  this, &BaseModel::resetCache);
    connect(this, &BaseModel::sortCriteriaChanged,  this, &BaseModel::resetCache);
    connect(this, &BaseModel::sortOrderChanged,  this, &BaseModel::resetCache);
    connect(this, &BaseModel::searchPatternChanged,  this, &BaseModel::resetCache);
}

BaseModel::~BaseModel()
{}

void BaseModel::classBegin()
{
    Q_D(BaseModel);
    d->m_qmlInitializing = true;
}

void BaseModel::componentComplete()
{
    Q_D(BaseModel);
    d->m_qmlInitializing = false;
    if (d->initializeModel())
        d->validateCache();
}

int BaseModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const BaseModel);
    if (parent.isValid())
        return 0;
    return d->getCount();
}


void BaseModel::onCacheDataChanged(int first, int last)
{
    emit dataChanged(index(first), index(last));
}

void BaseModel::onCacheBeginInsertRows(int first, int last)
{
    emit beginInsertRows({}, first, last);
}

void BaseModel::onCacheBeginRemoveRows(int first, int last)
{
    emit beginRemoveRows({}, first, last);
}

void BaseModel::onCacheBeginMoveRows(int first, int last, int destination)
{
    emit beginMoveRows({}, first, last, {}, destination);
}

unsigned int BaseModel::getLimit() const
{
    Q_D(const BaseModel);
    return d->m_limit;
}

void BaseModel::setLimit(unsigned int limit)
{
    Q_D(BaseModel);
    if (d->m_limit == limit)
        return;
    d->m_limit = limit;
    emit limitChanged();
}

unsigned int BaseModel::getOffset() const
{
    Q_D(const BaseModel);
    return d->m_offset;
}

void BaseModel::setOffset(unsigned int offset)
{
    Q_D(BaseModel);
    if (d->m_offset == offset)
        return;
    d->m_offset = offset;
    emit offsetChanged();
}

const QString& BaseModel::searchPattern() const
{
    Q_D(const BaseModel);
    return d->m_searchPattern;
}

void BaseModel::setSearchPattern( const QString& pattern )
{
    Q_D(BaseModel);
    QString patternToApply = pattern.length() == 0 ? QString{} : pattern;
    if (patternToApply == d->m_searchPattern)
        /* No changes */
        return;

    d->m_searchPattern = std::move(patternToApply);
    emit searchPatternChanged();
}

Qt::SortOrder BaseModel::getSortOrder() const
{
    Q_D(const BaseModel);
    return d->m_sortOrder;
}

void BaseModel::setSortOrder(Qt::SortOrder order)
{
    Q_D(BaseModel);
    if (d->m_sortOrder == order)
        return;
    d->m_sortOrder = order;
    emit sortOrderChanged();
}

const QString BaseModel::getSortCriteria() const
{
    Q_D(const BaseModel);
    return d->m_sortCriteria;
}

void BaseModel::setSortCriteria(const QString& criteria)
{
    Q_D(BaseModel);
    if (d->m_sortCriteria == criteria)
        return;
    d->m_sortCriteria = criteria;
    emit sortCriteriaChanged();
}

void BaseModel::unsetSortCriteria()
{
    setSortCriteria({});
}

void BaseModel::onResetRequested()
{
    Q_D(BaseModel);
    d->invalidateCache();
}

void BaseModel::onLocalSizeChanged(size_t queryCount, size_t maximumCount)
{
    emit countChanged(queryCount);
    emit maximumCountChanged(maximumCount);
    emit loadingChanged();
}

void BaseModel::validateCache() const
{
    Q_D(const BaseModel);
    d->validateCache();
}

void BaseModel::resetCache()
{
    Q_D(BaseModel);
    d->resetCache();
}

void BaseModel::invalidateCache()
{
    Q_D(BaseModel);
    d->invalidateCache();
}

unsigned int BaseModel::getCount() const
{
    Q_D(const BaseModel);
    return d->getCount();
}

unsigned int BaseModel::getLoadedCount() const
{
    Q_D(const BaseModel);
    return d->getLoadedCount();
}

unsigned int BaseModel::getMaximumCount() const
{
    Q_D(const BaseModel);
    return d->getMaximumCount();
}

bool BaseModel::loading() const
{
    Q_D(const BaseModel);
    return d->loading();
}

QMap<QString, QVariant> BaseModel::getDataAt(const QModelIndex & index) const
{
    QMap<QString, QVariant> dataDict;
    QHash<int, QByteArray> roles = roleNames();

    for (int role: roles.keys())
    {
        dataDict[roles[role]] = data(index, role);
    }
    return dataDict;
}

QMap<QString, QVariant> BaseModel::getDataAt(int idx) const
{
    return getDataAt(index(idx));
}


