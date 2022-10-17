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

#ifndef SORT_FILTER_PROXY_MODEL
#define SORT_FILTER_PROXY_MODEL

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qt.hpp"
#include <QSortFilterProxyModel>

class SortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

    Q_PROPERTY( QByteArray searchRole READ searchRole WRITE setSearchRole NOTIFY searchRoleChanged  FINAL)
    Q_PROPERTY( QString searchPattern READ searchPattern WRITE setSearchPattern NOTIFY searchPatternChanged  FINAL)
    Q_PROPERTY( QString sortCriteria READ sortCriteria WRITE setSortCriteria NOTIFY sortCriteriaChanged  FINAL)
    Q_PROPERTY( Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged  FINAL)
    Q_PROPERTY( int count READ count NOTIFY countChanged  FINAL)

public:
    SortFilterProxyModel( QObject * parent = nullptr );

    QString searchPattern() const;
    void setSearchPattern( const QString &searchPattern );

    QByteArray searchRole() const;
    void setSearchRole( const QByteArray &searchRole );

    QString sortCriteria() const;
    void setSortCriteria( const QString &sortCriteria );

    Qt::SortOrder sortOrder() const;
    void setSortOrder(Qt::SortOrder sortOrder);

    int count() const;

    Q_INVOKABLE QMap<QString, QVariant> getDataAt( int idx );
    Q_INVOKABLE QModelIndexList mapIndexesToSource( const QModelIndexList &indexes );
    Q_INVOKABLE int mapIndexToSource( int idx );

signals:
    void searchPatternChanged();
    void searchRoleChanged();
    void countChanged();
    void sortCriteriaChanged();
    void sortOrderChanged();

private slots:
    void updateFilterRole();
    void updateSortRole();

private:
    QByteArray m_searchRole;
    QString m_sortCriteria;
};

#endif // SORT_FILTER_PROXY_MODEL
