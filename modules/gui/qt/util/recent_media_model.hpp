/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#ifndef VLC_RECENT_MEDIA_MODEL_HPP
#define VLC_RECENT_MEDIA_MODEL_HPP

#ifdef HAVE_CONFIG_H

# include "config.h"

#endif

#include "qt.hpp"
#include <QAbstractListModel>

#include <QObject>
#include <QStringList>
#include "util/recents.hpp"

class VLCRecentMediaModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int limit READ getLimit WRITE setLimit NOTIFY limitChanged)

public:
    VLCRecentMediaModel(intf_thread_t *p_intf,QObject * parent = nullptr);

    Q_INVOKABLE void clear();

    Q_INVOKABLE int rowCount(QModelIndex const &parent = {}) const  override;

    QVariant data(QModelIndex const &index, const int role = Qt::DisplayRole) const  override;

    QHash<int, QByteArray> roleNames() const override;

    QStringList items;

    void setLimit(int l);
    int getLimit() const;

private:
    RecentsMRL *rmrl;
    int i_limit = 10;

signals:
    void limitChanged();

public slots:
    void update();
    QStringList getItems();

};

#endif // VLC_RECENT_MEDIA_MODEL_HPP
