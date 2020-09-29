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

#include "sortfilterproxymodel.hpp"

#include <QItemSelection>

#include <cassert>

SortFilterProxyModel::SortFilterProxyModel( QObject *parent )
    : QSortFilterProxyModel( parent )
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);

    connect( this, &QAbstractListModel::rowsInserted, this, &SortFilterProxyModel::countChanged );
    connect( this, &QAbstractListModel::rowsRemoved, this, &SortFilterProxyModel::countChanged );
    connect( this, &QAbstractItemModel::modelReset, this, &SortFilterProxyModel::countChanged );
    connect( this, &QAbstractItemModel::layoutChanged, this, &SortFilterProxyModel::countChanged );

    connect( this, &QAbstractProxyModel::sourceModelChanged, this, &SortFilterProxyModel::updateFilterRole );
}

QString SortFilterProxyModel::searchPattern() const
{
    return filterRegExp().pattern();
}

void SortFilterProxyModel::setSearchPattern( const QString &searchPattern )
{
    setFilterRegExp(searchPattern);
}

QByteArray SortFilterProxyModel::searchRole() const
{
    return m_searchRole;
}

void SortFilterProxyModel::setSearchRole( const QByteArray &searchRole )
{
    m_searchRole = searchRole;
    emit searchRoleChanged();
    updateFilterRole();
}

int SortFilterProxyModel::count() const
{
    return rowCount();
}

QMap<QString, QVariant> SortFilterProxyModel::getDataAt( int idx )
{
    QMap<QString, QVariant> dataDict;
    QHash<int,QByteArray> roles = roleNames();
    for( const auto role: roles.keys() ) {
        dataDict[roles[role]] = data( index( idx, 0 ), role );
    }
    return dataDict;
}

QModelIndexList SortFilterProxyModel::mapIndexesToSource( const QModelIndexList &indexes )
{
    QModelIndexList sourceIndexes;
    sourceIndexes.reserve( indexes.size() );
    for( const auto &proxyIndex : indexes ) {
        sourceIndexes.push_back( mapToSource(proxyIndex) );
    }
    return sourceIndexes;
}

int SortFilterProxyModel::mapIndexToSource(int idx)
{
    return mapToSource( index( idx, 0 ) ).row();
}

void SortFilterProxyModel::updateFilterRole()
{
    setFilterRole( roleNames().key( m_searchRole ) );
}
