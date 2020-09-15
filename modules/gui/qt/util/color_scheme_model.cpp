/*****************************************************************************
 * Copyright (C) 2020 the VideoLAN team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "color_scheme_model.hpp"

ColorSchemeModel::ColorSchemeModel(QObject* parent)
    : QStringListModel(parent)
{
}

void ColorSchemeModel::setAvailableColorSchemes(const QStringList& colorSchemeList)
{
    setStringList(colorSchemeList);

    //indexOf return -1 on "not found", wich will generate an invalid index
    int position = colorSchemeList.indexOf(m_current);

    QPersistentModelIndex oldIndex = m_checkedItem;
    m_checkedItem = index(position);
    if (oldIndex.isValid())
        emit dataChanged(oldIndex, oldIndex);
    if (m_checkedItem.isValid())
        emit dataChanged(m_checkedItem, m_checkedItem);
    else
    {
        m_current = QString{};
        emit currentChanged(m_current);
    }
}

Qt::ItemFlags ColorSchemeModel::flags (const QModelIndex & index) const
{
    Qt::ItemFlags defaultFlags = QStringListModel::flags(index);
    if (index.isValid()){
        return defaultFlags | Qt::ItemIsUserCheckable;
    }
    return defaultFlags;
}


bool ColorSchemeModel::setData(const QModelIndex &index,
                                const QVariant &value, int role)
{
    if(!index.isValid())
        return false;

    if (role != Qt::CheckStateRole)
        return QStringListModel::setData(index, value, role);

    if (value.type() != QVariant::Bool || value.toBool() == false)
        return false;

    if (index != m_checkedItem) {
        QPersistentModelIndex oldIndex = m_checkedItem;
        m_checkedItem = index;
        if (oldIndex.isValid())
            emit dataChanged(oldIndex, oldIndex);
        emit dataChanged(m_checkedItem, m_checkedItem);
        m_current = data(index, Qt::DisplayRole).toString();
        emit currentChanged(m_current);
    }

    return true;
}

void ColorSchemeModel::setCurrent(const QString& current)
{
    //indexOf return -1 on "not found", wich will generate an invalid index
    int position = stringList().indexOf(current);

    QPersistentModelIndex oldIndex = m_checkedItem;
    m_checkedItem = index(position);
    if (oldIndex.isValid())
        emit dataChanged(oldIndex, oldIndex);
    if (m_checkedItem.isValid())
        emit dataChanged(m_checkedItem, m_checkedItem);

    m_current = current;
    emit currentChanged(m_current);
}

QVariant ColorSchemeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if(role == Qt::CheckStateRole)
        return m_checkedItem == index ? Qt::Checked : Qt::Unchecked;

    return QStringListModel::data(index, role);
}
