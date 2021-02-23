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

#ifndef PROXY_COLUMN_MODEL
#define PROXY_COLUMN_MODEL

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qt.hpp"
#include <QAbstractItemModel>
#include <type_traits>


template <typename Model>
class ProxyColumnModel : public Model
{
public:
    ProxyColumnModel(const int extraColumns, const QHash<int, QString> &sectionDisplayData, QObject *parent = nullptr)
        : Model {parent}
        , m_extraColumns {extraColumns}
        , m_sectionDisplayData {sectionDisplayData}
    {
    }

    int columnCount(const QModelIndex &) const override
    {
        return 1 + m_extraColumns;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role == Qt::DisplayRole && m_sectionDisplayData.contains(section))
            return m_sectionDisplayData.value(section);

        return Model::headerData(section, orientation, role);
    }

    QVariant data(const QModelIndex &index, const int role) const override
    {
        if (index.column() != 0)
            return {};

        return Model::data(index, role);
    }

private:
    const int m_extraColumns;
    const QHash<int, QString> m_sectionDisplayData;

    static_assert(std::is_base_of<QAbstractListModel, Model>::value, "must be a subclass of list model");
};

#endif // PROXY_COLUMN_MODEL
