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

#include "devicesourceprovider.hpp"
#include "networkmediamodel.hpp"

#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"

#include "util/shared_input_item.hpp"
#include "vlcmediasourcewrapper.hpp"

#include "util/locallistbasemodel.hpp"

#include "vlcmediasourcewrapper.hpp"

namespace
{

//represents an entry of the model
struct NetworkDeviceItem : public NetworkBaseItem
{
    NetworkDeviceItem(
        const SharedInputItem& item,
        const SharedMediaSourceModel& source
        )
    {
        name = qfu(item->psz_name);
        mainMrl = QUrl::fromEncoded(item->psz_uri);
        protocol = mainMrl.scheme();
        type = static_cast<NetworkDeviceModel::ItemType>(item->i_type);

        id = qHash(item);
        mrls.push_back(Source{mainMrl, item, source});

        char* artworkUrl = input_item_GetArtworkURL(item.get());
        if (artworkUrl)
        {
            artwork = QString::fromUtf8(artworkUrl);
            free(artworkUrl);
        }
    }

    QString sourceDesciption() const {
        if (mrls.empty())
            return {};
        return mrls.front().source->getDescription();
    }

    NetworkTreeItem makeTreeItem() const
    {
        if (unlikely(mrls.empty()))
            return {};
        const Source& src = mrls.front();
        return NetworkTreeItem(src.source, src.item);
    }

    SharedInputItem getInputItem() const
    {
        if (unlikely(mrls.empty()))
            return {};
        const Source& src = mrls.front();
        return src.item;
    }

    void addSource(const SharedInputItem& item,
                   const SharedMediaSourceModel& source)
    {
        mrls.emplace_back(Source{
            QUrl::fromEncoded(item->psz_uri),
            item,
            source
        });
    }

    /**
     * @brief addRemoveSource
     * @param item
     * @param source
     * @return false when no sources left
     */
    bool removeSource(const SharedInputItem& item,
                   const SharedMediaSourceModel&)
    {
        mrls.erase(std::remove_if(
            mrls.begin(), mrls.end(),
            [item](const Source& source){
                return source.item == item;
            })
        );

        if (mrls.empty())
        {
            mainMrl = QUrl{};
            return false;
        }
        else
        {
            mainMrl = mrls.front().mrl;
            return true;
        }
    }


    struct Source {
        QUrl mrl;
        SharedInputItem item;
        SharedMediaSourceModel source;
    };

    size_t id;
    std::vector<Source> mrls;
};

using NetworkDeviceItemPtr =std::shared_ptr<NetworkDeviceItem>;


static inline bool operator == (const NetworkDeviceItemPtr& a, const NetworkDeviceItemPtr& b) noexcept
{
    return a->id == b->id
           && QString::compare(a->name, b->name, Qt::CaseInsensitive) == 0
           && QString::compare(a->protocol, b->protocol, Qt::CaseInsensitive) == 0;
}

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

    ~NetworkDeviceModelPrivate()
    {
        //Explicilty disconnect signals here:
        // - destructor will release m_sourcesProvider
        // - its shared MediaSourceModel will be released
        // - they will notify `mediaRemoved`, but the slot expects the shared MediaSourceModel to be valid
        // at this point we don't care about model updates
        for (auto& conn : m_sourceUpdateConnections)
            QObject::disconnect(conn);
    }

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

        if (m_qmlInitializing || !m_ctx || m_sdSource == NetworkDeviceModel::CAT_UNDEFINED || m_sourceName.isEmpty())
            return false;

        m_items.clear();

        if (m_sourcesProvider)
            m_sourcesProvider.reset();

        m_name = QString {};
        emit q->nameChanged();

        m_sourcesProvider = std::make_unique<DeviceSourceProvider>( m_sdSource, m_sourceName, m_ctx );

        QObject::connect(m_sourcesProvider.get(), &DeviceSourceProvider::failed, q,
                [this]()
        {
            m_items.clear();

            m_revision += 1;
            invalidateCache();
        });

        QObject::connect(m_sourcesProvider.get(), &DeviceSourceProvider::nameUpdated, q,
                [this, q](QString name)
        {
            m_name = name;
            emit q->nameChanged();
        });

        for (auto conn : m_sourceUpdateConnections)
            QObject::disconnect(conn);
        m_sourceUpdateConnections.clear();

        //itemsUpdated is called only once after init.
        QObject::connect(m_sourcesProvider.get(), &DeviceSourceProvider::itemsUpdated, q,
                [this]()
        {
            for (const auto& source: m_sourcesProvider->getMediaSources())
            {
                //fill initial values
                for (const SharedInputItem& media : source->getMedias())
                    onMediaAdded(source, media);

                //don't store a strong reference in the lambda or this may leak the source
                //as the connection is destroyed when the source is destroyed but the
                //connection holds a reference
                QWeakPointer<MediaSourceModel> weakSource{source};

                auto conn = QObject::connect(
                    source.get(), &MediaSourceModel::mediaAdded,
                    q_ptr, [this, weakSource](SharedInputItem media) {
                        //source is the signal emitter, it should always exist
                        auto strongSource  = weakSource.toStrongRef();
                        assert(strongSource);
                        onMediaAdded(strongSource, media);
                    });
                m_sourceUpdateConnections.push_back(conn);

                conn = QObject::connect(
                    source.get(), &MediaSourceModel::mediaRemoved,
                    q_ptr, [this, weakSource](SharedInputItem media) {
                        auto strongSource  = weakSource.toStrongRef();
                        assert(strongSource);
                        onMediaRemoved(strongSource, media);
                    });

                m_sourceUpdateConnections.push_back(conn);
            }

            m_revision += 1;
            invalidateCache();
        });

        m_sourcesProvider->init();

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

    void onMediaAdded(const QSharedPointer<MediaSourceModel>& mediaSource, SharedInputItem media)
    {
        std::size_t hash = qHash(media);
        auto it = std::find_if(
            m_items.begin(), m_items.end(),
            [hash](const NetworkDeviceItemPtr& item) {
                return item->id == hash;
            });
        if (it != m_items.end())
        {
            (*it)->addSource(media, mediaSource);
        }
        else
        {
            m_items.push_back(std::make_unique<NetworkDeviceItem>(media, mediaSource));
            m_revision += 1;
            invalidateCache();
        }
    }

    void onMediaRemoved(const QSharedPointer<MediaSourceModel>& mediaSource, SharedInputItem media)
    {
        std::size_t hash = qHash(media);
        auto it = std::find_if(
            m_items.begin(), m_items.end(),
            [hash](const NetworkDeviceItemPtr& item) {
                return item->id == hash;
            });
        if (it == m_items.end())
            return;

        bool needDestruction = (*it)->removeSource(media, mediaSource);
        if (needDestruction)
        {
            m_items.erase(it);
        }

        m_revision += 1;
        invalidateCache();
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
    std::unique_ptr<DeviceSourceProvider> m_sourcesProvider;
    std::vector<NetworkDeviceItemPtr> m_items;

    MainCtx* m_ctx = nullptr;
    NetworkDeviceModel::SDCatType m_sdSource = NetworkDeviceModel::CAT_UNDEFINED;
    QString m_sourceName; // '*' -> all sources
    QString m_name; // source long name

    std::vector<QMetaObject::Connection> m_sourceUpdateConnections;
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
    if (!d->m_ctx)
        return {};

    const NetworkDeviceItem* item = d->getItemForRow(index.row());
    if (!item)
        return {};

    switch ( role )
    {
        case NETWORK_SOURCE:
            return item->sourceDesciption();
        case NETWORK_TREE:
            return QVariant::fromValue(item->makeTreeItem());
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
    if (d->m_ctx == ctx)
        return;
    d->m_ctx = ctx;
    d->initializeModel();
    emit ctxChanged();
}

MainCtx* NetworkDeviceModel::getCtx() const
{
    Q_D(const NetworkDeviceModel);
    return d->m_ctx;
}

NetworkDeviceModel::SDCatType NetworkDeviceModel::getSdSource() const {
    Q_D(const NetworkDeviceModel);
    return d->m_sdSource;
}
QString NetworkDeviceModel::getName() const {
    Q_D(const NetworkDeviceModel);
    return d->m_name;

}
QString NetworkDeviceModel::getSourceName() const {
    Q_D(const NetworkDeviceModel);
    return d->m_sourceName;
}

void NetworkDeviceModel::setSdSource(SDCatType s)
{
    Q_D(NetworkDeviceModel);
    if (d->m_sdSource == s)
        return;
    d->m_sdSource = s;
    d->initializeModel();
    emit sdSourceChanged();
}

void NetworkDeviceModel::setSourceName(const QString& sourceName)
{
    Q_D(NetworkDeviceModel);
    if (d->m_sourceName == sourceName)
        return;
    d->m_sourceName = sourceName;
    d->initializeModel();
    emit sourceNameChanged();
}

bool NetworkDeviceModel::insertIntoPlaylist(const QModelIndexList &itemIdList, ssize_t playlistIndex)
{
    Q_D(NetworkDeviceModel);
    if (!(d->m_ctx && d->m_sdSource != CAT_MYCOMPUTER))
        return false;
    QVector<vlc::playlist::Media> medias;
    medias.reserve( itemIdList.size() );
    for ( const QModelIndex &id : itemIdList )
    {
        const NetworkDeviceItem* item = d->getItemForRow(id.row());
        if (!item)
            continue;
        medias.append( vlc::playlist::Media {item->getInputItem().get() } );
    }
    if (medias.isEmpty())
        return false;
    d->m_ctx->getIntf()->p_mainPlaylistController->insert(playlistIndex, medias, false);
    return true;
}

bool NetworkDeviceModel::addToPlaylist(int row)
{
    Q_D(NetworkDeviceModel);
    if (!(d->m_ctx && d->m_sdSource != CAT_MYCOMPUTER))
        return false;

    const NetworkDeviceItem* item = d->getItemForRow(row);
    if (!item)
        return false;

    vlc::playlist::Media media{ item->getInputItem().get() };
    d->m_ctx->getIntf()->p_mainPlaylistController->append( QVector<vlc::playlist::Media>{ media }, false);
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
    if (!(d->m_ctx && d->m_sdSource != CAT_MYCOMPUTER))
        return false;

    const NetworkDeviceItem* item = d->getItemForRow(row);
    if (!item)
        return false;

    vlc::playlist::Media media{ item->getInputItem().get() };
    d->m_ctx->getIntf()->p_mainPlaylistController->append( QVector<vlc::playlist::Media>{ media }, true);
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

        items.append(QVariant::fromValue(SharedInputItem(item->getInputItem().get(), true)));
    }

    return items;
}
