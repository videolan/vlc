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

#include "control_list_model.hpp"

ControlListModel::ControlListModel(QObject *parent) : QAbstractListModel(parent)
{
    connect(this, &QAbstractListModel::rowsInserted, this, &ControlListModel::countChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &ControlListModel::countChanged);
    connect(this, &QAbstractListModel::modelReset, this, &ControlListModel::countChanged);
}

int ControlListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_controls.size();
}

QVariant ControlListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const ControlType control = m_controls.at(index.row());

    switch (role) {
    case ID_ROLE:
        return QVariant(control);
    }
    return QVariant();
}

bool ControlListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    ControlType control = m_controls.at(index.row());

    switch (role) {
    case ID_ROLE:
        if (value.canConvert<int>())
            control = static_cast<ControlType>(value.toInt());
        else
            return false;
        break;
    }

    if (setButtonAt(index.row(), control)) {
        emit dataChanged(index, index, { role });
        return true;
    }
    return false;
}

Qt::ItemFlags ControlListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return (Qt::ItemIsEditable | Qt::ItemNeverHasChildren);
}

QHash<int, QByteArray> ControlListModel::roleNames() const
{
    return {
        {
            ID_ROLE, "id"
        }
    };
}

QVector<int> ControlListModel::getControls() const
{
    QVector<int> list;

    for (auto i : m_controls)
    {
        list.append(static_cast<int>(i));
    }

    return list;
}

void ControlListModel::setControls(const QVector<int> &list)
{
    beginResetModel();

    m_controls.resize(list.size());

    for (int i = 0; i < list.size(); ++i)
    {
        m_controls[i] = static_cast<ControlType>(list.at(i));
    }

    endResetModel();
}

bool ControlListModel::setButtonAt(int index, const ControlType &button)
{
    if(index < 0 || index >= m_controls.size())
        return false;

    const ControlType oldControl = m_controls.at(index);

    if (button == oldControl)
        return false;

    m_controls[index] = button;
    return true;
}

void ControlListModel::insert(int index, QVariantMap bdata)
{
    beginInsertRows(QModelIndex(), index, index);
    m_controls.insert(index, static_cast<ControlType>(bdata.value("id").toInt()));
    endInsertRows();
}
void ControlListModel::move(int src, int dest)
{
    if(src == dest)
        return;

    beginMoveRows(QModelIndex(), src, src, QModelIndex(), dest + (src < dest ? 1 : 0));
    m_controls.move(src, dest);
    endMoveRows();
}

void ControlListModel::remove(int index)
{
    beginRemoveRows(QModelIndex(), index, index);
    m_controls.remove(index);
    endRemoveRows();
}

void ControlListModel::clear()
{
    beginResetModel();
    m_controls.clear();
    endResetModel();
}
