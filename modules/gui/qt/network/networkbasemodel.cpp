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
    default:
        return {};
    };
}


QHash<int, QByteArray> NetworkBaseModel::roleNames() const
{
    return {
        { NETWORK_BASE_NAME, "name" },
        { NETWORK_BASE_MRL, "mrl" },
        { NETWORK_BASE_TYPE, "type" },
        { NETWORK_BASE_PROTOCOL, "protocol" },
        { NETWORK_BASE_ARTWORK, "artwork" },
    };
}
