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

#include <unordered_set>
#include <QTimer>

#include "maininterface/mainctx.hpp"
#include "util/workerthreadset.hpp"

#include "devicesourceprovider.hpp"
#include "networkmediamodel.hpp"

#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"

#include "util/shared_input_item.hpp"
#include "util/locallistbasemodel.hpp"

namespace
{

bool itemMatchPattern(const NetworkDeviceItemPtr& a, const QString& pattern)
{
    return a->name.contains(pattern, Qt::CaseInsensitive);
}

bool ascendingMrl(const NetworkDeviceItemPtr& a, const NetworkDeviceItemPtr& b)
{
    return (QString::compare(a->mainMrl.toString(),
                             b->mainMrl.toString(), Qt::CaseInsensitive) <= 0);
}

bool ascendingName(const NetworkDeviceItemPtr& a, const NetworkDeviceItemPtr& b)
{
    int result = QString::compare(a->name, b->name, Qt::CaseInsensitive);

    if (result != 0)
        return (result <= 0);

    return ascendingMrl(a, b);
}

bool descendingMrl(const NetworkDeviceItemPtr& a, const NetworkDeviceItemPtr& b)
{
    return (QString::compare(a->mainMrl.toString(),
                             b->mainMrl.toString(), Qt::CaseInsensitive) >= 0);
}

bool descendingName(const NetworkDeviceItemPtr& a, const NetworkDeviceItemPtr& b)
{
    int result = QString::compare(a->name, b->name, Qt::CaseInsensitive);

    if (result != 0)
        return (result >= 0);

    return descendingMrl(a, b);
}

}

// ListCache specialisation

template<>
bool ListCache<NetworkDeviceItemPtr>::compareItems(const NetworkDeviceItemPtr& a, const NetworkDeviceItemPtr& b)
{
    //just compare the pointers here
    return a == b;
}

// NetworkDeviceModelPrivate

using NetworkDeviceModelLoader = LocalListCacheLoader<NetworkDeviceItemPtr>;

class NetworkDeviceModelPrivate
    : public LocalListBaseModelPrivate<NetworkDeviceItemPtr>
{
    Q_DECLARE_PUBLIC(NetworkDeviceModel)
public:
    NetworkDeviceModelPrivate(NetworkDeviceModel * pub)
        : LocalListBaseModelPrivate<NetworkDeviceItemPtr>(pub)
    {}

    NetworkDeviceModelLoader::ItemCompare getSortFunction() const override
    {
        if (m_sortCriteria == "mrl")
        {
            if (m_sortOrder == Qt::AscendingOrder)
                return ascendingMrl;
            else
                return descendingMrl;
        }
        else
        {
            if (m_sortOrder == Qt::AscendingOrder)
                return ascendingName;
            else
                return descendingName;
        }
    }

    bool initializeModel() override
    {
        Q_Q(NetworkDeviceModel);

        if (m_qmlInitializing || !q->m_ctx || q->m_sdSource == NetworkDeviceModel::CAT_UNDEFINED || q->m_sourceName.isEmpty())
            return false;

        m_items.clear();

        if (m_sources)
        {
            q->disconnect( m_sources );
            m_sources->deleteLater();
            m_sources = nullptr;
        }

        q->m_name = QString {};
        emit q->nameChanged();

        m_sources = new DeviceSourceProvider( q->m_sdSource, q->m_sourceName );
        q->m_ctx->workersThreads()->assignToWorkerThread( m_sources );

        // make sure we're not releasing resources on main thread
        // by clearing copies of model before source provider
        QObject::connect(q, &QObject::destroyed, m_sources, [sources = m_sources]()
        {
            sources->deleteLater();
        });

        QObject::connect(m_sources, &DeviceSourceProvider::failed, q,
                [this]()
        {
            m_items.clear();

            m_revision += 1;
            invalidateCache();
        });

        QObject::connect(m_sources, &DeviceSourceProvider::nameUpdated, q,
                [q](QString name)
        {
            q->m_name = name;
            emit q->nameChanged();
        });

        QObject::connect(m_sources, &DeviceSourceProvider::itemsUpdated, q,
                [this](NetworkDeviceItemSet items)
        {
            m_items = items;

            m_revision += 1;
            invalidateCache();
        });

        QMetaObject::invokeMethod(m_sources,
                                  [sources = this->m_sources, intf = q->m_ctx->getIntf()]()
        {
            sources->init( intf );
        });

        //service discovery don't notify preparse end
        m_loading = false;
        emit q->loadingChanged();

        return true;
    }

    const NetworkDeviceItem* getItemForRow(int row) const
    {
        const NetworkDeviceItemPtr* ref = item(row);
        if (ref)
            return ref->get();
        return nullptr;
    }

public: //LocalListCacheLoader::ModelSource
    std::vector<NetworkDeviceItemPtr> getModelData(const QString& pattern) const override
    {
        std::vector<NetworkDeviceItemPtr> items;
        if (pattern.isEmpty())
        {
            std::copy(
                m_items.cbegin(), m_items.cend(),
                std::back_inserter(items));
        }
        else
        {
            std::copy_if(
                m_items.cbegin(), m_items.cend(),
                std::back_inserter(items),
                [&pattern](const NetworkDeviceItemPtr& item){
                    return itemMatchPattern(item, pattern);
                });
        }
        return items;
    }

public:
    NetworkDeviceItemSet m_items;
    QPointer<DeviceSourceProvider> m_sources {};
};

NetworkDeviceModel::NetworkDeviceModel( QObject* parent )
    : NetworkDeviceModel(new NetworkDeviceModelPrivate(this), parent)
{
}

NetworkDeviceModel::NetworkDeviceModel( NetworkDeviceModelPrivate* priv, QObject* parent)
    : NetworkBaseModel(priv, parent)
{
}

QVariant NetworkDeviceModel::data( const QModelIndex& index, int role ) const
{
    Q_D(const NetworkDeviceModel);
    if (!m_ctx)
        return {};

    const NetworkDeviceItem* item = d->getItemForRow(index.row());
    if (!item)
        return {};

    switch ( role )
    {
        case NETWORK_SOURCE:
            return item->mediaSource->description;
        case NETWORK_TREE:
            return QVariant::fromValue( NetworkTreeItem(MediaTreePtr{ item->mediaSource->tree }, item->inputItem.get()) );
        default:
            return NetworkBaseModel::basedata(*item, role);
    }
}

QHash<int, QByteArray> NetworkDeviceModel::roleNames() const
{
    QHash<int, QByteArray> roles = NetworkBaseModel::roleNames();
    roles[NETWORK_SOURCE] = "source";
    roles[NETWORK_TREE] = "tree";
    return roles;
}

void NetworkDeviceModel::setCtx(MainCtx* ctx)
{
    Q_D(NetworkDeviceModel);
    if (m_ctx == ctx)
        return;
    m_ctx = ctx;
    d->initializeModel();
    emit ctxChanged();
}

void NetworkDeviceModel::setSdSource(SDCatType s)
{
    Q_D(NetworkDeviceModel);
    if (m_sdSource == s)
        return;
    m_sdSource = s;
    d->initializeModel();
    emit sdSourceChanged();
}

void NetworkDeviceModel::setSourceName(const QString& sourceName)
{
    Q_D(NetworkDeviceModel);
    if (m_sourceName == sourceName)
        return;
    m_sourceName = sourceName;
    d->initializeModel();
    emit sourceNameChanged();
}

bool NetworkDeviceModel::insertIntoPlaylist(const QModelIndexList &itemIdList, ssize_t playlistIndex)
{
    Q_D(NetworkDeviceModel);
    if (!(m_ctx && m_sdSource != CAT_MYCOMPUTER))
        return false;
    QVector<vlc::playlist::Media> medias;
    medias.reserve( itemIdList.size() );
    for ( const QModelIndex &id : itemIdList )
    {
        const NetworkDeviceItem* item = d->getItemForRow(id.row());
        if (!item)
            continue;
        medias.append( vlc::playlist::Media {item->inputItem.get()} );
    }
    if (medias.isEmpty())
        return false;
    m_ctx->getIntf()->p_mainPlaylistController->insert(playlistIndex, medias, false);
    return true;
}

bool NetworkDeviceModel::addToPlaylist(int row)
{
    Q_D(NetworkDeviceModel);
    if (!(m_ctx && m_sdSource != CAT_MYCOMPUTER))
        return false;

    const NetworkDeviceItem* item = d->getItemForRow(row);
    if (!item)
        return false;

    vlc::playlist::Media media{ item->inputItem.get() };
    m_ctx->getIntf()->p_mainPlaylistController->append( QVector<vlc::playlist::Media>{ media }, false);
    return true;
}

bool NetworkDeviceModel::addToPlaylist(const QVariantList &itemIdList)
{
    bool ret = false;
    for (const QVariant& varValue: itemIdList)
    {
        if (varValue.canConvert<int>())
        {
            auto index = varValue.value<int>();
            ret |= addToPlaylist(index);
        }
    }
    return ret;
}

bool NetworkDeviceModel::addToPlaylist(const QModelIndexList &itemIdList)
{
    bool ret = false;
    for (const QModelIndex& index: itemIdList)
    {
        if (!index.isValid())
            continue;
        ret |= addToPlaylist(index.row());
    }
    return ret;
}

bool NetworkDeviceModel::addAndPlay(int row)
{
    Q_D(NetworkDeviceModel);
    if (!(m_ctx && m_sdSource != CAT_MYCOMPUTER))
        return false;

    const NetworkDeviceItem* item = d->getItemForRow(row);
    if (!item)
        return false;

    vlc::playlist::Media media{ item->inputItem.get() };
    m_ctx->getIntf()->p_mainPlaylistController->append( QVector<vlc::playlist::Media>{ media }, true);
    return true;
}

bool NetworkDeviceModel::addAndPlay(const QVariantList& itemIdList)
{
    bool ret = false;
    for (const QVariant& varValue: itemIdList)
    {
        if (varValue.canConvert<int>())
        {
            auto row = varValue.value<int>();
            if (!ret)
                ret |= addAndPlay(row);
            else
                ret |= addToPlaylist(row);
        }
    }
    return ret;
}

bool NetworkDeviceModel::addAndPlay(const QModelIndexList& itemIdList)
{
    bool ret = false;
    for (const QModelIndex& index: itemIdList)
    {
        if (!index.isValid())
            continue;
        if (!ret)
            ret |= addAndPlay(index.row());
        else
            ret |= addToPlaylist(index.row());
    }
    return ret;
}

/* Q_INVOKABLE */
QVariantList NetworkDeviceModel::getItemsForIndexes(const QModelIndexList & indexes) const
{
    Q_D(const NetworkDeviceModel);
    QVariantList items;

    for (const QModelIndex & modelIndex : indexes)
    {
        const NetworkDeviceItem* item = d->getItemForRow(modelIndex.row());
        if (!item)
            continue;

        items.append(QVariant::fromValue(SharedInputItem(item->inputItem.get(), true)));
    }

    return items;
}

