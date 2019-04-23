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

#include "recent_media_model.hpp"
#include <cassert>

namespace {
    enum Roles
    {
        MRLRole = Qt::UserRole
    };
}

VLCRecentMediaModel::VLCRecentMediaModel(intf_thread_t *p_intf,QObject *parent)
    : QAbstractListModel(parent)
{
    assert(p_intf);
    rmrl = RecentsMRL::getInstance(p_intf);

    connect(rmrl, SIGNAL(saved()), this, SLOT(update()));
    connect(this, SIGNAL(limitChanged()), this, SLOT(update()));

    update();
}

int VLCRecentMediaModel::rowCount(QModelIndex const & ) const
{
    return items.count();
}

QVariant VLCRecentMediaModel::data(QModelIndex const &index, const int role) const
{
    if (!index.isValid())
        return {};
    switch (role)
    {
        case MRLRole :
            return QVariant::fromValue(items[index.row()]);
        default :
            return {};
    }
}

QHash<int, QByteArray> VLCRecentMediaModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames.insert(MRLRole, "mrl");
    return roleNames;
}

void VLCRecentMediaModel::clear()
{
    if (!items.isEmpty())
    {
        rmrl->clear();
        update();
    }
}

void VLCRecentMediaModel::update()
{
    beginResetModel();
    items = rmrl->recentList().mid(0,i_limit);
    endResetModel();
}

QStringList VLCRecentMediaModel::getItems()
{
    return items;
}

int VLCRecentMediaModel::getLimit() const
{
    return i_limit; 
}

void VLCRecentMediaModel::setLimit(int l)
{
    i_limit = l;
    update();
    emit limitChanged();
}
