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

#include "networkdevicemodel.hpp"
#include "networkmediamodel.hpp"
#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"
#include "util/qmlinputitem.hpp"
#include <maininterface/mainctx.hpp>

NetworkDeviceModel::NetworkDeviceModel( QObject* parent )
    : ClipListModel(parent)
{
    m_comparator = ascendingName;
}

QVariant NetworkDeviceModel::data( const QModelIndex& index, int role ) const
{
    if (!m_ctx)
        return {};
    auto idx = index.row();
    if ( idx < 0 || idx >= count() )
        return {};
    const auto& item = m_items[idx];
    switch ( role )
    {
        case NETWORK_NAME:
            return item.name;
        case NETWORK_MRL:
            return item.mainMrl;
        case NETWORK_TYPE:
            return item.type;
        case NETWORK_PROTOCOL:
            return item.protocol;
        case NETWORK_SOURCE:
            return item.mediaSource->description;
        case NETWORK_TREE:
            return QVariant::fromValue( NetworkTreeItem(MediaTreePtr{ item.mediaSource->tree }, item.inputItem.get()) );
        case NETWORK_ARTWORK:
            return item.artworkUrl;
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
    if (ctx) {
        m_ctx = ctx;
    }
    if (m_ctx && m_sdSource != CAT_UNDEFINED && !m_sourceName.isEmpty()) {
        initializeMediaSources();
    }
    emit ctxChanged();
}

void NetworkDeviceModel::setSdSource(SDCatType s)
{
    m_sdSource = s;
    if (m_ctx && m_sdSource != CAT_UNDEFINED && !m_sourceName.isEmpty()) {
        initializeMediaSources();
    }
    emit sdSourceChanged();
}

void NetworkDeviceModel::setSourceName(const QString& sourceName)
{
    m_sourceName = sourceName;
    if (m_ctx && m_sdSource != CAT_UNDEFINED && !m_sourceName.isEmpty()) {
        initializeMediaSources();
    }
    emit sourceNameChanged();
}

bool NetworkDeviceModel::insertIntoPlaylist(const QModelIndexList &itemIdList, ssize_t playlistIndex)
{
    if (!(m_ctx && m_sdSource != CAT_MYCOMPUTER))
        return false;
    QVector<vlc::playlist::Media> medias;
    medias.reserve( itemIdList.size() );
    for ( const QModelIndex &id : itemIdList )
    {
        if ( !id.isValid() )
            continue;
        const int index = id.row();
        if ( index < 0 || index >= count() )
            continue;

        medias.append( vlc::playlist::Media {m_items[index].inputItem.get()} );
    }
    if (medias.isEmpty())
        return false;
    m_ctx->getMainPlaylistController()->insert(playlistIndex, medias, false);
    return true;
}

bool NetworkDeviceModel::addToPlaylist(int index)
{
    if (!(m_ctx && m_sdSource != CAT_MYCOMPUTER))
        return false;
    if (index < 0 || index >= count() )
        return false;
    auto item =  m_items[index];
    vlc::playlist::Media media{ item.inputItem.get() };
    m_ctx->getMainPlaylistController()->append( { media }, false);
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

bool NetworkDeviceModel::addAndPlay(int index)
{
    if (!(m_ctx && m_sdSource != CAT_MYCOMPUTER))
        return false;
    if (index < 0 || index >= count() )
        return false;
    auto item =  m_items[index];
    vlc::playlist::Media media{ item.inputItem.get() };
    m_ctx->getMainPlaylistController()->append( { media }, true);
    return true;
}

bool NetworkDeviceModel::addAndPlay(const QVariantList& itemIdList)
{
    bool ret = false;
    for (const QVariant& varValue: itemIdList)
    {
        if (varValue.canConvert<int>())
        {
            auto index = varValue.value<int>();
            if (!ret)
                ret |= addAndPlay(index);
            else
                ret |= addToPlaylist(index);
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

QMap<QString, QVariant> NetworkDeviceModel::getDataAt(int idx)
{
    QMap<QString, QVariant> dataDict;
    QHash<int,QByteArray> roles = roleNames();
    for (auto role: roles.keys()) {
        dataDict[roles[role]] = data(index(idx), role);
    }
    return dataDict;
}

/* Q_INVOKABLE */
QVariantList NetworkDeviceModel::getItemsForIndexes(const QModelIndexList & indexes) const
{
    QVariantList items;

    for (const QModelIndex & modelIndex : indexes)
    {
        int index = modelIndex.row();

        if (index < 0 || index >= count())
            continue;

        QmlInputItem input(m_items[index].inputItem.get(), true);

        items.append(QVariant::fromValue(input));
    }

    return items;
}

// Protected ClipListModel implementation

void NetworkDeviceModel::onUpdateSort(const QString & criteria, Qt::SortOrder order) /* override */
{
    if (criteria == "mrl")
    {
        if (order == Qt::AscendingOrder)
            m_comparator = ascendingMrl;
        else
            m_comparator = descendingMrl;
    }
    else
    {
        if (order == Qt::AscendingOrder)
            m_comparator = ascendingName;
        else
            m_comparator = descendingName;
    }
}

bool NetworkDeviceModel::initializeMediaSources()
{
    auto libvlc = vlc_object_instance(m_ctx->getIntf());

    m_listeners.clear();

    clearItems();

    m_name = QString {};

    auto provider = vlc_media_source_provider_Get( libvlc );

    using SourceMetaPtr = std::unique_ptr<vlc_media_source_meta_list_t,
                                          decltype( &vlc_media_source_meta_list_Delete )>;

    SourceMetaPtr providerList( vlc_media_source_provider_List( provider, static_cast<services_discovery_category_e>(m_sdSource) ),
                                &vlc_media_source_meta_list_Delete );
    if ( providerList == nullptr )
        return false;

    auto nbProviders = vlc_media_source_meta_list_Count( providerList.get() );

    for ( auto i = 0u; i < nbProviders; ++i )
    {
        auto meta = vlc_media_source_meta_list_Get( providerList.get(), i );
        const QString sourceName = qfu( meta->name );
        if ( m_sourceName != '*' && m_sourceName != sourceName )
            continue;

        m_name += m_name.isEmpty() ? qfu( meta->longname ) : ", " + qfu( meta->longname );
        emit nameChanged();

        auto mediaSource = vlc_media_source_provider_GetMediaSource( provider,
                                                                     meta->name );
        if ( mediaSource == nullptr )
            continue;
        std::unique_ptr<MediaTreeListener> l{ new MediaTreeListener(
                        MediaTreePtr{ mediaSource->tree },
                        std::make_unique<ListenerCb>(this, MediaSourcePtr{ mediaSource }) ) };
        if ( l->listener == nullptr )
            return false;
        m_listeners.push_back( std::move( l ) );
    }
    return m_listeners.empty() == false;
}


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

    std::vector<InputItemPtr> itemList;

    itemList.reserve( count );
    for ( auto i = 0u; i < count; ++i )
        itemList.emplace_back( children[i]->p_item );

    QMetaObject::invokeMethod(model, [model=model, itemList=std::move(itemList)]() {
        int implicitCount = model->implicitCount();

        for (auto p_item : itemList)
        {
            QUrl itemUri = QUrl::fromEncoded(p_item->psz_uri);
            auto it = std::find_if( begin( model->m_items ), end( model->m_items ),
                                   [p_item, itemUri](const NetworkDeviceItem & i) {
                return QString::compare( qfu(p_item->psz_name), i.name, Qt::CaseInsensitive ) == 0 &&
                    itemUri.scheme() == i.mainMrl.scheme();
            });
            if ( it == end( model->m_items ) )
                continue;

            auto mrlIt = std::find_if( begin( (*it).mrls ), end( (*it).mrls),
                                       [itemUri]( const QUrl& mrl ) {
                return mrl.matches(itemUri, QUrl::StripTrailingSlash);
            });

            if ( mrlIt == end( (*it).mrls ) )
                continue;
            (*it).mrls.erase( mrlIt );
            if ( (*it).mrls.empty() == false )
                continue;
            auto idx = std::distance( begin( model->m_items ), it );

            model->eraseItem(it, idx, implicitCount);
        }

       model->updateItems();
    }, Qt::QueuedConnection);
}

void NetworkDeviceModel::refreshDeviceList(MediaSourcePtr mediaSource,
                                           input_item_node_t * const children[], size_t count,
                                           bool clear)
{
    if (clear)
    {
        QMetaObject::invokeMethod(this, [this, mediaSource]()
        {
            int implicitCount = this->implicitCount();

            int index = 0;

            std::vector<NetworkDeviceItem>::iterator it = m_items.begin();

            while (it != m_items.end())
            {
                if (it->mediaSource != mediaSource)
                    continue;

                eraseItem(it, index, implicitCount);

                index++;
            }
        });
    }

    std::vector<InputItemPtr> itemList;

    itemList.reserve(count);

    for (size_t i = 0; i < count; i++)
    {
        input_item_t * item = children[i]->p_item;

        itemList.emplace_back(item);
    }

    QMetaObject::invokeMethod(this, [this, itemList = std::move(itemList), mediaSource]() mutable
    {
        addItems(itemList, mediaSource);
    }, Qt::QueuedConnection);
}

void NetworkDeviceModel::addItems(const std::vector<InputItemPtr> & inputList,
                                  const MediaSourcePtr & mediaSource)
{
    // NOTE: We need to check duplicates when we're not sorting by name. Otherwise it's handled via
    //       the 'name' sorting functions.
    bool checkDuplicate = (m_sortCriteria != "name");

    for (const InputItemPtr & inputItem : inputList)
    {
        NetworkDeviceItem item;

        item.name = qfu(inputItem->psz_name);

        item.mainMrl = QUrl::fromEncoded(inputItem->psz_uri);

        std::vector<NetworkDeviceItem>::iterator it;

        if (checkDuplicate)
        {
            it = std::find_if(begin(m_items), end(m_items), [item](const NetworkDeviceItem & i)
            {
                return matchItem(item, i);
            });

            // NOTE: We don't want the same name and scheme to appear twice in the list.
            if (it != end(m_items))
                continue;

            it = std::upper_bound(begin(m_items), end(m_items), item, m_comparator);

            if (it != end(m_items))
                continue;
        }
        else
        {
            it = std::upper_bound(begin(m_items), end(m_items), item, m_comparator);

            if (it != end(m_items) && matchItem(item, *it))
                continue;
        }

        item.mrls.push_back(item.mainMrl);

        item.protocol = item.mainMrl.scheme();

        item.type = static_cast<ItemType>(inputItem->i_type);

        item.mediaSource = mediaSource;

        item.inputItem = InputItemPtr(inputItem);

        char * artwork = input_item_GetArtworkURL(inputItem.get());

        if (artwork)
        {
            item.artworkUrl = QUrl::fromEncoded(artwork);

            free(artwork);
        }

        insertItem(it, std::move(item));
    }

    updateItems();
}

void NetworkDeviceModel::eraseItem(std::vector<NetworkDeviceItem>::iterator & it,
                                   int index, int count)
{
    if (index < count)
    {
        removeItem(it, index);
    }
    // NOTE: We don't want to notify the view if the item's position is beyond the
    //       maximumCount.
    else
        it = m_items.erase(it);
}

// Private static function

/* static */ bool NetworkDeviceModel::matchItem(const NetworkDeviceItem & a,
                                                const NetworkDeviceItem & b)
{
    return (QString::compare(a.name, b.name, Qt::CaseInsensitive) == 0
            &&
            QString::compare(a.mainMrl.scheme(), b.mainMrl.scheme(), Qt::CaseInsensitive) == 0);
}

/* static */ bool NetworkDeviceModel::ascendingName(const NetworkDeviceItem & a,
                                                    const NetworkDeviceItem & b)
{
    int result = QString::compare(a.name, b.name, Qt::CaseInsensitive);

    if (result != 0)
        return (result <= 0);

    return (QString::compare(a.mainMrl.scheme(), b.mainMrl.scheme(), Qt::CaseInsensitive) <= 0);
}

/* static */ bool NetworkDeviceModel::ascendingMrl(const NetworkDeviceItem & a,
                                                   const NetworkDeviceItem & b)
{
    return (QString::compare(a.mainMrl.toString(),
                             b.mainMrl.toString(), Qt::CaseInsensitive) <= 0);
}

/* static */ bool NetworkDeviceModel::descendingName(const NetworkDeviceItem & a,
                                                     const NetworkDeviceItem & b)
{
    int result = QString::compare(a.name, b.name, Qt::CaseInsensitive);

    if (result != 0)
        return (result >= 0);

    return (QString::compare(a.mainMrl.scheme(), b.mainMrl.scheme(), Qt::CaseInsensitive) >= 0);
}

/* static */ bool NetworkDeviceModel::descendingMrl(const NetworkDeviceItem & a,
                                                    const NetworkDeviceItem & b)
{
    return (QString::compare(a.mainMrl.toString(),
                             b.mainMrl.toString(), Qt::CaseInsensitive) >= 0);
}
