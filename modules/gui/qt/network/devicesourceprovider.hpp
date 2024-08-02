/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qt.hpp"

#include <QString>
#include <QSet>
#include <vector>

#include "networkdevicemodel.hpp"
#include "mediatreelistener.hpp"

//represents an entry of the model
struct NetworkDeviceItem : public NetworkBaseItem
{
    NetworkDeviceItem(const SharedInputItem& item, const NetworkDeviceModel::MediaSourcePtr& source)
    {
        name = qfu(item->psz_name);
        mainMrl = QUrl::fromEncoded(item->psz_uri);
        protocol = mainMrl.scheme();
        type = static_cast<NetworkDeviceModel::ItemType>(item->i_type);
        mediaSource = source;
        inputItem = item;

        id = qHash(name) ^ qHash(protocol);
        mrls.push_back(std::make_pair(mainMrl, source));

        char* artworkUrl = input_item_GetArtworkURL(inputItem.get());
        if (artworkUrl)
        {
            artwork = QString::fromUtf8(artworkUrl);
            free(artworkUrl);
        }
    }

    uint id;
    std::vector<std::pair<QUrl, NetworkDeviceModel::MediaSourcePtr>> mrls;
    NetworkDeviceModel::MediaSourcePtr mediaSource;
    SharedInputItem inputItem;
 };

using NetworkDeviceItemPtr =std::shared_ptr<NetworkDeviceItem>;

static inline bool operator == (const NetworkDeviceItemPtr& a, const NetworkDeviceItemPtr& b) noexcept
{
    return a->id == b->id
        && QString::compare(a->name, b->name, Qt::CaseInsensitive) == 0
        && QString::compare(a->protocol, b->protocol, Qt::CaseInsensitive) == 0;
}


static inline std::size_t qHash(const NetworkDeviceItemPtr& s, size_t = 0) noexcept
{
    return s->id;
}

using NetworkDeviceItemSet = QSet<NetworkDeviceItemPtr>;

class DeviceSourceProvider : public QObject
{
    Q_OBJECT

public:
    using MediaSourcePtr = NetworkDeviceModel::MediaSourcePtr;

    DeviceSourceProvider(NetworkDeviceModel::SDCatType sdSource,
                         const QString &sourceName,
                         QObject *parent = nullptr);

    void init(qt_intf_t *intf);

signals:
    void failed();
    void nameUpdated( QString name );
    void itemsUpdated( NetworkDeviceItemSet items );

private:
    struct ListenerCb;

    void addItems(const std::vector<SharedInputItem>& inputList,
                  const MediaSourcePtr& mediaSource,
                  bool clear);

    void removeItems(const std::vector<SharedInputItem>& inputList,
                     const MediaSourcePtr& mediaSource);

    NetworkDeviceModel::SDCatType m_sdSource;
    QString m_sourceName; // '*' -> all sources
    QString m_name; // source long name

    NetworkDeviceItemSet m_items;

    // destruction of listeners may cause destruction of source 'MediaSource'
    // maintain a seperate reference of MediaSources to fix cyclic free
    std::vector<MediaSourcePtr> m_mediaSources;

    std::vector<std::unique_ptr<MediaTreeListener>> m_listeners;
};
