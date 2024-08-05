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
#include "networkbasemodel.hpp"

QVariant NetworkBaseModel::basedata(const NetworkBaseItem& item, int role) const
{
    switch (role)
    {
    case NETWORK_BASE_NAME:
        return item.name;
    case NETWORK_BASE_MRL:
        return item.mainMrl;
    case NETWORK_BASE_TYPE:
        return item.type;
    case NETWORK_BASE_PROTOCOL:
        return item.protocol;
    case NETWORK_BASE_ARTWORK:
        return item.artwork;
    case NETWORK_BASE_ARTWORK_FALLBACK:
        return artworkFallbackForType(item.type);
    default:
        return {};
    };
}

QString NetworkBaseModel::artworkFallbackForType(const ItemType type) const
{
    switch (type) {
    case TYPE_DISC:
        return "qrc:///sd/disc.svg";
    case TYPE_CARD:
        return "qrc:///sd/capture-card.svg";
    case TYPE_STREAM:
        return "qrc:///sd/stream.svg";
    case TYPE_PLAYLIST:
        return "qrc:///sd/playlist.svg";
    case TYPE_FILE:
        return "qrc:///sd/file.svg";
    default:
        return "qrc:///sd/directory.svg";
    }
}

QHash<int, QByteArray> NetworkBaseModel::roleNames() const
{
    return {
        { NETWORK_BASE_NAME, "name" },
        { NETWORK_BASE_MRL, "mrl" },
        { NETWORK_BASE_TYPE, "type" },
        { NETWORK_BASE_PROTOCOL, "protocol" },
        { NETWORK_BASE_ARTWORK, "artwork" },
        { NETWORK_BASE_ARTWORK_FALLBACK, "artworkFallback" },
    };
}
