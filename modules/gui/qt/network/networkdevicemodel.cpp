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

namespace {

enum Role {
    NETWORK_NAME = Qt::UserRole + 1,
    NETWORK_MRL,
    NETWORK_TYPE,
    NETWORK_PROTOCOL,
    NETWORK_SOURCE,
    NETWORK_TREE,
    NETWORK_ARTWORK,
};

}

NetworkDeviceModel::NetworkDeviceModel( QObject* parent )
    : QAbstractListModel( parent )
    , m_ml( nullptr )
{
}

QVariant NetworkDeviceModel::data( const QModelIndex& index, int role ) const
{
    if (!m_ctx)
        return {};
    auto idx = index.row();
    if ( idx < 0 || (size_t)idx >= m_items.size() )
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
            return QVariant::fromValue( NetworkTreeItem(item.mediaSource, item.inputItem.get()) );
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


int NetworkDeviceModel::rowCount(const QModelIndex& parent) const
{
    if ( parent.isValid() )
        return 0;
    return getCount();
}


void NetworkDeviceModel::setCtx(QmlMainContext* ctx)
{
    if (ctx) {
        m_ctx = ctx;
        m_ml = vlc_ml_instance_get( m_ctx->getIntf() );
    }
    if (m_ctx && m_sdSource != CAT_UNDEFINED) {
        initializeMediaSources();
    }
    emit ctxChanged();
}

void NetworkDeviceModel::setSdSource(SDCatType s)
{
    m_sdSource = s;
    if (m_ctx && m_sdSource != CAT_UNDEFINED) {
        initializeMediaSources();
    }
    emit sdSourceChanged();
}

int NetworkDeviceModel::getCount() const
{
    assert( m_items.size() < INT32_MAX );
    return static_cast<int>( m_items.size() );
}


bool NetworkDeviceModel::addToPlaylist(int index)
{
    if (!(m_ctx && m_sdSource != CAT_MYCOMPUTER))
        return false;
    if (index < 0 || (size_t)index >= m_items.size() )
        return false;
    auto item =  m_items[index];
    vlc::playlist::Media media{ item.inputItem.get() };
    m_ctx->getIntf()->p_sys->p_mainPlaylistController->append( { media }, false);
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
    if (index < 0 || (size_t)index >= m_items.size() )
        return false;
    auto item =  m_items[index];
    vlc::playlist::Media media{ item.inputItem.get() };
    m_ctx->getIntf()->p_sys->p_mainPlaylistController->append( { media }, true);
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

bool NetworkDeviceModel::initializeMediaSources()
{
    auto libvlc = vlc_object_instance(m_ctx->getIntf());

    m_listeners.clear();
    if (!m_items.empty()) {
        beginResetModel();
        m_items.clear();
        endResetModel();
        emit countChanged();
    }

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
        auto mediaSource = vlc_media_source_provider_GetMediaSource( provider,
                                                                     meta->name );
        if ( mediaSource == nullptr )
            continue;
        std::unique_ptr<NetworkSourceListener> l{ new NetworkSourceListener(
                        MediaSourcePtr{ mediaSource, false }, this ) };
        if ( l->listener == nullptr )
            return false;
        m_listeners.push_back( std::move( l ) );
    }
    return m_listeners.empty() == false;
}


void NetworkDeviceModel::onItemCleared( MediaSourcePtr mediaSource, input_item_node_t* node )
{
    if (node != &mediaSource->tree->root)
        return;
    refreshDeviceList( std::move( mediaSource), node->pp_children, node->i_children, true );
}

void NetworkDeviceModel::onItemAdded( MediaSourcePtr mediaSource, input_item_node_t* parent,
                                  input_item_node_t *const children[],
                                  size_t count )
{
    if (parent != &mediaSource->tree->root)
        return;
    refreshDeviceList( std::move( mediaSource ), children, count, false );
}

void NetworkDeviceModel::onItemRemoved(MediaSourcePtr mediaSource, input_item_node_t* node,
                                    input_item_node_t *const children[],
                                    size_t count )
{
    if (node != &mediaSource->tree->root)
        return;

    std::vector<InputItemPtr> itemList;
    itemList.reserve( count );
    for ( auto i = 0u; i < count; ++i )
        itemList.emplace_back( children[i]->p_item );

    QMetaObject::invokeMethod(this, [this, itemList=std::move(itemList)]() {
        for (auto p_item : itemList)
        {
            QUrl itemUri = QUrl::fromEncoded(p_item->psz_uri);
            auto it = std::find_if( begin( m_items ), end( m_items ), [p_item, itemUri](const Item& i) {
                return QString::compare( qfu(p_item->psz_name), i.name, Qt::CaseInsensitive ) == 0 &&
                    itemUri.scheme() == i.mainMrl.scheme();
            });
            if ( it == end( m_items ) )
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
            auto idx = std::distance( begin( m_items ), it );
            beginRemoveRows({}, idx, idx );
            m_items.erase( it );
            endRemoveRows();
            emit countChanged();
        }
    }, Qt::QueuedConnection);
}

void NetworkDeviceModel::refreshDeviceList( MediaSourcePtr mediaSource, input_item_node_t* const children[], size_t count, bool clear )
{
    if ( clear == true )
    {
        QMetaObject::invokeMethod(this, [this, mediaSource]() {
            beginResetModel();
            m_items.erase(std::remove_if(m_items.begin(), m_items.end(), [mediaSource](const Item& value) {
                return value.mediaSource == mediaSource;
            }), m_items.end());
            endResetModel();
            emit countChanged();
        });
    }

    std::vector<InputItemPtr> itemList;
    itemList.reserve( count );
    for ( auto i = 0u; i < count; ++i )
        itemList.emplace_back( children[i]->p_item );

    QMetaObject::invokeMethod(this, [this, itemList=std::move(itemList), mediaSource=std::move(mediaSource) ]() mutable {
        for ( auto p_item : itemList )
        {
            Item item;
            item.mainMrl = QUrl::fromEncoded( p_item->psz_uri );
            item.name = qfu(p_item->psz_name);
            item.mrls.push_back( item.mainMrl );
            item.type = static_cast<ItemType>( p_item->i_type );
            item.protocol = item.mainMrl.scheme();
            item.mediaSource = mediaSource;
            item.inputItem = InputItemPtr(p_item);

            char* artwork = input_item_GetArtworkURL( p_item.get() );
            if (artwork)
            {
                item.artworkUrl = QUrl::fromEncoded(artwork);
                free(artwork);
            }

            auto it = std::upper_bound(begin( m_items ), end( m_items ), item, [](const Item& a, const Item& b) {
                int comp =  QString::compare(a.name , b.name, Qt::CaseInsensitive );
                if (comp == 0)
                    comp = QString::compare(a.mainMrl.scheme(), b.mainMrl.scheme());
                return comp <= 0;
            });

            if (it != end( m_items )
                && QString::compare(it->name , item.name, Qt::CaseInsensitive ) == 0
                && it->mainMrl.scheme() == item.mainMrl.scheme())
                continue;

            int pos = std::distance(begin(m_items), it);
            beginInsertRows( {}, pos, pos );
            m_items.insert( it, std::move( item ) );
            endInsertRows();
            emit countChanged();
        }
    }, Qt::QueuedConnection);
}
