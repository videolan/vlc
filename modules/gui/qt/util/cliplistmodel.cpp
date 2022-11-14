/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

#include "cliplistmodel.hpp"

// BaseClipListModel

/* explicit */ BaseClipListModel::BaseClipListModel(QObject * parent)
    : QAbstractListModel(parent) {}

// Interface

void BaseClipListModel::updateItems()
{
    int count = implicitCount();

    if (m_count == count) return;

    if (m_count < count)
        expandItems(count);
    else
        shrinkItems(count);
}

// QAbstractItemModel implementation

int BaseClipListModel::rowCount(const QModelIndex &) const /* override */
{
    return count();
}

// Properties

int BaseClipListModel::count() const
{
    return m_count;
}

int BaseClipListModel::maximumCount() const
{
    return m_maximumCount;
}

void BaseClipListModel::setMaximumCount(int count)
{
    if (m_maximumCount == count)
        return;

    m_maximumCount = count;

    count = implicitCount();

    if (m_count == count)
    {
        emit maximumCountChanged();

        return;
    }

    if (m_count < count)
        expandItems(count);
    else
        shrinkItems(count);

    emit maximumCountChanged();
}

QString BaseClipListModel::searchPattern() const
{
    return m_searchPattern;
}

void BaseClipListModel::setSearchPattern(const QString & pattern)
{
    if (m_searchPattern == pattern)
        return;

    m_searchPattern = pattern;

    emit searchPatternChanged();
}

QByteArray BaseClipListModel::searchRole() const
{
    return m_searchRole;
}

void BaseClipListModel::setSearchRole(const QByteArray & role)
{
    if (m_searchRole == role)
        return;

    m_searchRole = role;

    emit searchRoleChanged();
}

QString BaseClipListModel::sortCriteria() const
{
    return m_sortCriteria;
}

void BaseClipListModel::setSortCriteria(const QString & criteria)
{
    if (m_sortCriteria == criteria)
        return;

    m_sortCriteria = criteria;

    updateSort();

    emit sortCriteriaChanged();
}

Qt::SortOrder BaseClipListModel::sortOrder() const
{
    return m_sortOrder;
}

void BaseClipListModel::setSortOrder(Qt::SortOrder order)
{
    if (m_sortOrder == order)
        return;

    m_sortOrder = order;

    updateSort();

    emit sortOrderChanged();
}
