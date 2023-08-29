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

#include "maininterface/mainctx.hpp"

#include "networkdevicemodel.hpp"
#include "networkmediamodel.hpp"
#include "mediatreelistener.hpp"

#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"

#include "util/shared_input_item.hpp"
#include "util/base_model_p.hpp"
#include "util/locallistcacheloader.hpp"

namespace
{

//represents an entry of the model
struct NetworkDeviceItem
{
    NetworkDeviceItem(const SharedInputItem& item, const NetworkDeviceModel::MediaSourcePtr& mediaSource)
        : name(qfu(item->psz_name))
        , mainMrl(QUrl::fromEncoded(item->psz_uri))
        , protocol(mainMrl.scheme())
        , type( static_cast<NetworkDeviceModel::ItemType>(item->i_type))
        , mediaSource(mediaSource)
        , inputItem(item)
    {
        id = qHash(name) ^ qHash(protocol);
        mrls.push_back(std::make_pair(mainMrl, mediaSource));

        char* artworkUrl = input_item_GetArtworkURL(inputItem.get());
        if (artworkUrl)
        {
            artwork = QString::fromUtf8(artworkUrl);
            free(artworkUrl);
        }
    }

    uint id;
    QString name;
    QUrl mainMrl;
    std::vector<std::pair<QUrl, NetworkDeviceModel::MediaSourcePtr>> mrls;
    QString protocol;
    NetworkDeviceModel::ItemType type;
    NetworkDeviceModel::MediaSourcePtr mediaSource;
    SharedInputItem inputItem;
    QString artwork;
};

using NetworkDeviceItemPtr =  std::shared_ptr<NetworkDeviceItem>;
using NetworkDeviceModelLoader = LocalListCacheLoader<NetworkDeviceItemPtr>;

//hash and compare function for std::unordered_set
struct NetworkDeviceItemHash
{
    std::size_t operator()(const NetworkDeviceItemPtr& s) const noexcept
    {
        return s->id;
    }
};

struct NetworkDeviceItemEqual
{
    bool operator()(const NetworkDeviceItemPtr& a, const NetworkDeviceItemPtr& b) const noexcept
    {
        return a->id == b->id
            && QString::compare(a->name, b->name, Qt::CaseInsensitive) == 0
            && QString::compare(a->protocol, b->protocol, Qt::CaseInsensitive) == 0;
    }
};

using NetworkDeviceItemSet = std::unordered_set<NetworkDeviceItemPtr, NetworkDeviceItemHash, NetworkDeviceItemEqual>;

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

//handle discovery events from the media source provider
struct NetworkDeviceModel::ListenerCb : public MediaTreeListener::MediaTreeListenerCb {
    ListenerCb(NetworkDeviceModel* model, MediaSourcePtr mediaSource)
        : model(model)
        , mediaSource(std::move(mediaSource))
    {}

    void onItemCleared( MediaTreePtr tree, input_item_node_t* node ) override;
    void onItemAdded( MediaTreePtr tree, input_item_node_t* parent, input_item_node_t *const children[], size_t count ) override;
    void onItemRemoved( MediaTreePtr tree, input_item_node_t * node, input_item_node_t *const children[], size_t count ) override;
    inline void onItemPreparseEnded( MediaTreePtr, input_item_node_t *, enum input_item_preparse_status ) override {}

    NetworkDeviceModel *model;
    MediaSourcePtr mediaSource;
};

// ListCache specialisation

template<>
bool ListCache<NetworkDeviceItemPtr>::compareItems(const NetworkDeviceItemPtr& a, const NetworkDeviceItemPtr& b)
{
    //just compare the pointers here
    return a == b;
}

// NetworkDeviceModelPrivate

class NetworkDeviceModelPrivate
    : public BaseModelPrivateT<NetworkDeviceItemPtr>
    , public LocalListCacheLoader<NetworkDeviceItemPtr>::ModelSource
{
    Q_DECLARE_PUBLIC(NetworkDeviceModel)
public:
    NetworkDeviceModelPrivate(NetworkDeviceModel * pub)
        : BaseModelPrivateT<NetworkDeviceItemPtr>(pub)
        , m_items(0, NetworkDeviceItemHash{}, NetworkDeviceItemEqual{})
    {}

    NetworkDeviceModelLoader::ItemCompare getSortFunction() const
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

    std::unique_ptr<ListCacheLoader<NetworkDeviceItemPtr>> createLoader() const override
    {
        return std::make_unique<NetworkDeviceModelLoader>(
            this, m_searchPattern,
            getSortFunction());
    }

    bool initializeModel() override
    {
        Q_Q(NetworkDeviceModel);

        if (m_qmlInitializing || !q->m_ctx || q->m_sdSource == NetworkDeviceModel::CAT_UNDEFINED || q->m_sourceName.isEmpty())
            return false;

        auto libvlc = vlc_object_instance(q->m_ctx->getIntf());

        m_listeners.clear();
        m_items.clear();

        q->m_name = QString {};

        auto provider = vlc_media_source_provider_Get( libvlc );

        using SourceMetaPtr = std::unique_ptr<vlc_media_source_meta_list_t,
                                              decltype( &vlc_media_source_meta_list_Delete )>;

        SourceMetaPtr providerList( vlc_media_source_provider_List( provider, static_cast<services_discovery_category_e>(q->m_sdSource) ),
                                   &vlc_media_source_meta_list_Delete );
        if ( providerList == nullptr )
            return false;

        auto nbProviders = vlc_media_source_meta_list_Count( providerList.get() );

        for ( auto i = 0u; i < nbProviders; ++i )
        {
            auto meta = vlc_media_source_meta_list_Get( providerList.get(), i );
            const QString sourceName = qfu( meta->name );
            if ( q->m_sourceName != '*' && q->m_sourceName != sourceName )
                continue;

            q->m_name += q->m_name.isEmpty() ? qfu( meta->longname ) : ", " + qfu( meta->longname );
            emit q->nameChanged();

            auto mediaSource = vlc_media_source_provider_GetMediaSource( provider,
                                                                        meta->name );
            if ( mediaSource == nullptr )
                continue;
            std::unique_ptr<MediaTreeListener> l{ new MediaTreeListener(
                MediaTreePtr{ mediaSource->tree },
                std::make_unique<NetworkDeviceModel::ListenerCb>(q, MediaSourcePtr{ mediaSource }) ) };
            if ( l->listener == nullptr )
                return false;
            m_listeners.push_back( std::move( l ) );
        }
        return m_listeners.empty() == false;
    }

    const NetworkDeviceItem* getItemForRow(int row) const
    {
        const NetworkDeviceItemPtr* ref = item(row);
        if (ref)
            return ref->get();
        return nullptr;
    }

    void addItems(
        const std::vector<SharedInputItem>& inputList,
        const MediaSourcePtr& mediaSource,
        bool clear)
    {
        bool dataChanged = false;

        if (clear)
        {
            //std::remove_if doesn't work with unordered_set
            //due to iterators being const
            for (auto it = m_items.begin(); it != m_items.end(); )
            {
                it = std::find_if(
                    it, m_items.end(),
                    [&mediaSource](const NetworkDeviceItemPtr& item) {
                        return item->mediaSource == mediaSource;
                    });

                if (it != m_items.end())
                    it = m_items.erase(it);
            }
            dataChanged = true;
        }

        for (const SharedInputItem & inputItem : inputList)
        {
            auto newItem = std::make_shared<NetworkDeviceItem>(inputItem, mediaSource);
            auto it = m_items.find(newItem);
            if (it != m_items.end())
            {
                (*it)->mrls.push_back(std::make_pair(newItem->mainMrl, mediaSource));
            }
            else
            {
                m_items.emplace(std::move(newItem));
                dataChanged = true;
            }
        }

        if (dataChanged)
        {
            m_modelRevision += 1;
            invalidateCache();
        }
    }

    void removeItems(const std::vector<SharedInputItem>& inputList, const MediaSourcePtr& mediaSource)
    {
        bool dataChanged = false;
        for (const SharedInputItem& p_item : inputList)
        {
            auto items = m_items;

            auto oldItem = std::make_shared<NetworkDeviceItem>(p_item, mediaSource);
            NetworkDeviceItemSet::iterator it = m_items.find(oldItem);
            if (it != m_items.end())
            {
                bool found = false;

                const NetworkDeviceItemPtr& item = *it;
                if (item->mrls.size() > 1)
                {
                    auto mrlIt = std::find_if(
                        item->mrls.begin(), item->mrls.end(),
                        [&oldItem]( const std::pair<QUrl, MediaSourcePtr>& mrl ) {
                            return mrl.first.matches(oldItem->mainMrl, QUrl::StripTrailingSlash)
                                && mrl.second == oldItem->mediaSource;
                        });

                    if ( mrlIt != item->mrls.end() )
                    {
                        found = true;
                        item->mrls.erase( mrlIt );
                    }
                }

                if (!found)
                {
                    items.erase(it);
                    dataChanged = true;
                }
            }

        }
        if (dataChanged)
        {
            m_modelRevision += 1;
            invalidateCache();
        }
    }

public: //LocalListCacheLoader::ModelSource
    size_t getModelRevision() const override
    {
        return m_modelRevision;
    }

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
    size_t m_modelRevision = 0;
    NetworkDeviceItemSet m_items;
    std::vector<std::unique_ptr<MediaTreeListener>> m_listeners;
};

NetworkDeviceModel::NetworkDeviceModel( QObject* parent )
    : NetworkDeviceModel(new NetworkDeviceModelPrivate(this), parent)
{
}

NetworkDeviceModel::NetworkDeviceModel( NetworkDeviceModelPrivate* priv, QObject* parent)
    : BaseModel(priv, parent)
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
        case NETWORK_NAME:
            return item->name;
        case NETWORK_MRL:
            return item->mainMrl;
        case NETWORK_TYPE:
            return item->type;
        case NETWORK_PROTOCOL:
            return item->protocol;
        case NETWORK_SOURCE:
            return item->mediaSource->description;
        case NETWORK_TREE:
            return QVariant::fromValue( NetworkTreeItem(MediaTreePtr{ item->mediaSource->tree }, item->inputItem.get()) );
        case NETWORK_ARTWORK:
            return item->artwork;
        default:
            return {};
    }
}

QHash<int, QByteArray> NetworkDeviceModel::roleNames() const
{
    return {
        { NETWORK_NAME, "name" },
        { NETWORK_MRL, "mrl" },
        { NETWORK_TYPE, "type" },
        { NETWORK_PROTOCOL, "protocol" },
        { NETWORK_SOURCE, "source" },
        { NETWORK_TREE, "tree" },
        { NETWORK_ARTWORK, "artwork" },
    };
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

// NetworkDeviceModel::ListenerCb implementation

void NetworkDeviceModel::ListenerCb::onItemCleared( MediaTreePtr tree, input_item_node_t* node )
{
    if (node != &tree->root)
        return;
    model->refreshDeviceList( mediaSource, node->pp_children, node->i_children, true );
}

void NetworkDeviceModel::ListenerCb::onItemAdded( MediaTreePtr tree, input_item_node_t* parent,
                                                  input_item_node_t *const children[],
                                                  size_t count )
{
    if (parent != &tree->root)
        return;
    model->refreshDeviceList( mediaSource, children, count, false );
}

void NetworkDeviceModel::ListenerCb::onItemRemoved( MediaTreePtr tree, input_item_node_t* node,
                                                    input_item_node_t *const children[],
                                                    size_t count )
{
    if (node != &tree->root)
        return;

    std::vector<SharedInputItem> itemList;

    itemList.reserve( count );
    for ( auto i = 0u; i < count; ++i )
        itemList.emplace_back( children[i]->p_item );

    QMetaObject::invokeMethod(model, [model=model, mediaSource=mediaSource, itemList=std::move(itemList)]() {
        model->d_func()->removeItems(itemList, mediaSource);
    }, Qt::QueuedConnection);
}

void NetworkDeviceModel::refreshDeviceList(MediaSourcePtr mediaSource,
                                           input_item_node_t * const children[], size_t count,
                                           bool clear)
{
    std::vector<SharedInputItem> itemList;

    itemList.reserve(count);
    for (size_t i = 0; i < count; i++)
        itemList.emplace_back(children[i]->p_item);

    QMetaObject::invokeMethod(this, [this, clear, itemList = std::move(itemList), mediaSource]() mutable
    {
        Q_D(NetworkDeviceModel);
        d->addItems(itemList, mediaSource, clear);
    }, Qt::QueuedConnection);
}
