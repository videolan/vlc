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

#ifndef NETWORKBASEMODEL_HPP
#define NETWORKBASEMODEL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "util/base_model.hpp"
#include <vlc_media_source.h>

#include <QString>
#include <QUrl>

struct NetworkBaseItem;
class NetworkBaseModel: public BaseModel
{
    Q_OBJECT

public:
    enum Role {
        NETWORK_BASE_NAME = Qt::UserRole + 1,
        NETWORK_BASE_MRL,
        NETWORK_BASE_TYPE,
        NETWORK_BASE_PROTOCOL,
        NETWORK_BASE_ARTWORK,
        NETWORK_BASE_ARTWORK_FALLBACK,
        NETWORK_BASE_MAX
    };

    enum ItemType{
        // qt version of input_item_type_e
        TYPE_UNKNOWN = ITEM_TYPE_UNKNOWN,
        TYPE_FILE,
        TYPE_DIRECTORY,
        TYPE_DISC,
        TYPE_CARD,
        TYPE_STREAM,
        TYPE_PLAYLIST,
        TYPE_NODE,
    };
    Q_ENUM( ItemType )

    using BaseModel::BaseModel;

    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString artworkFallbackForType(const ItemType type) const;

protected:
    QVariant basedata(const NetworkBaseItem& item, int role) const;
};

struct NetworkBaseItem
{
    QString name;
    QUrl mainMrl;
    QString protocol;
    NetworkBaseModel::ItemType type;
    QString artwork;
};

#endif /* NETWORKBASEMODEL_HPP */
