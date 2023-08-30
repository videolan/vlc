/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#ifndef STANDARDPATHMODEL_HPP
#define STANDARDPATHMODEL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// VLC includes
#include "util/base_model.hpp"

class StandardPathModelPrivate;
class StandardPathModel : public BaseModel
{
    Q_OBJECT

public: // Enums
    // NOTE: Roles should be aligned with the NetworkDeviceModel.
    enum Role
    {
        PATH_NAME = Qt::UserRole + 1,
        PATH_MRL,
        PATH_TYPE,
        PATH_PROTOCOL,
        PATH_SOURCE,
        PATH_TREE,
        PATH_ARTWORK
    };

public:
    StandardPathModel(QObject * parent = nullptr);

public: // QAbstractItemModel implementation
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

    Q_DECLARE_PRIVATE(StandardPathModel)
};

#endif // STANDARDPATHMODEL_HPP
