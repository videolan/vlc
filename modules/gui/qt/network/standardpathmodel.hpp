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
#include <vlc_media_source.h>
#include <vlc_cxx_helpers.hpp>
#include "networkdevicemodel.hpp"
#include "util/cliplistmodel.hpp"

// Qt includes
#include <QAbstractListModel>
#include <QStandardPaths>
#include <QUrl>

struct StandardPathItem;

class StandardPathModel : public ClipListModel<StandardPathItem>
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

public: // Declarations
    using InputItemPtr = vlc_shared_data_ptr_type(input_item_t,
                                                  input_item_Hold,
                                                  input_item_Release);

    using MediaTreePtr = vlc_shared_data_ptr_type(vlc_media_tree_t,
                                                  vlc_media_tree_Hold,
                                                  vlc_media_tree_Release);

public:
    StandardPathModel(QObject * parent = nullptr);

public: // QAbstractItemModel implementation
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

protected: // ClipListModel implementation
    void onUpdateSort(const QString & criteria, Qt::SortOrder order) override;

private: // Static functions
    static bool ascendingName(const StandardPathItem & a, const StandardPathItem & b);
    static bool ascendingMrl (const StandardPathItem & a, const StandardPathItem & b);

    static bool descendingName(const StandardPathItem & a, const StandardPathItem & b);
    static bool descendingMrl (const StandardPathItem & a, const StandardPathItem & b);

private: // Functions
    void addItem(const QString & path, const QString & name, const QUrl & artwork);
};

struct StandardPathItem
{
    QString name;
    QUrl    mrl;

    QString protocol;

    NetworkDeviceModel::ItemType type;

    StandardPathModel::InputItemPtr inputItem;
    StandardPathModel::MediaTreePtr tree;

    QUrl artwork;
};

#endif // STANDARDPATHMODEL_HPP
