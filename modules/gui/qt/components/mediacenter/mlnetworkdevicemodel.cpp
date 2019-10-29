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

#include "mlnetworkdevicemodel.hpp"
#include "mlnetworkmediamodel.hpp"

#include "components/playlist/media.hpp"
#include "components/playlist/playlist_controller.hpp"

namespace {

enum Role {
    NETWORK_NAME = Qt::UserRole + 1,
    NETWORK_MRL,
    NETWORK_TYPE,
    NETWORK_PROTOCOL,
    NETWORK_SOURCE,
    NETWORK_TREE,
};

}

MLNetworkDeviceModel::MLNetworkDeviceModel( QObject* parent )
    : QAbstractListModel( parent )
    , m_ml( nullptr )
{
}

QVariant MLNetworkDeviceModel::data( const QModelIndex& index, int role ) const
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
        default:
            return {};
    }
}

QHash<int, QByteArray> MLNetworkDeviceModel::roleNames() const
{
    return {
        { NETWORK_NAME, "name" },
        { NETWORK_MRL, "mrl" },
        { NETWORK_TYPE, "type" },
        { NETWORK_PROTOCOL, "protocol" },
        { NETWORK_SOURCE, "source" },
        { NETWORK_TREE, "tree" },
    };
}


int MLNetworkDeviceModel::rowCount(const QModelIndex& parent) const
{
    if ( parent.isValid() )
        return 0;
    assert( m_items.size() < INT32_MAX );
    return static_cast<int>( m_items.size() );
}


void MLNetworkDeviceModel::setCtx(QmlMainContext* ctx)
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

void MLNetworkDeviceModel::setSdSource(SDCatType s)
{
    m_sdSource = s;
    if (m_ctx && m_sdSource != CAT_UNDEFINED) {
        initializeMediaSources();
    }
    emit sdSourceChanged();
}


bool MLNetworkDeviceModel::addToPlaylist(int index)
{
    if (!(m_ctx && m_sdSource != CAT_MYCOMPUTER))
        return false;
    if (index < 0 || index >= m_items.size() )
        return false;
    auto item =  m_items[index];
    vlc::playlist::Media media{ item.inputItem.get() };
    return true;
}

bool MLNetworkDeviceModel::addToPlaylist(const QVariantList &itemIdList)
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

bool MLNetworkDeviceModel::addAndPlay(int index)
{
    if (!(m_ctx && m_sdSource != CAT_MYCOMPUTER))
        return false;
    if (index < 0 || index >= m_items.size() )
        return false;
    auto item =  m_items[index];
    vlc::playlist::Media media{ item.inputItem.get() };
    m_ctx->getIntf()->p_sys->p_mainPlaylistController->append( { media }, true);
    return true;
}

bool MLNetworkDeviceModel::addAndPlay(const QVariantList& itemIdList)
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

bool MLNetworkDeviceModel::initializeMediaSources()
{
    auto libvlc = vlc_object_instance(m_ctx->getIntf());

    m_listeners.clear();
    if (!m_items.empty()) {
        beginResetModel();
        m_items.clear();
        endResetModel();
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
        std::unique_ptr<MLNetworkSourceListener> l{ new MLNetworkSourceListener(
                        MediaSourcePtr{ mediaSource, false }, this ) };
        if ( l->listener == nullptr )
            return false;
        m_listeners.push_back( std::move( l ) );
    }
    return m_listeners.empty() == false;
}


void MLNetworkDeviceModel::onItemCleared( MediaSourcePtr mediaSource, input_item_node_t* node )
{
    refreshDeviceList( std::move( mediaSource), node->pp_children, node->i_children, true );
}

void MLNetworkDeviceModel::onItemAdded( MediaSourcePtr mediaSource, input_item_node_t* parent,
                                  input_item_node_t *const children[],
                                  size_t count )
{
    refreshDeviceList( std::move( mediaSource ), children, count, false );
}

void MLNetworkDeviceModel::onItemRemoved( MediaSourcePtr,
                                    input_item_node_t *const children[],
                                    size_t count )
{
    for ( auto i = 0u; i < count; ++i )
    {
        input_item_t* p_item = children[i]->p_item;
        input_item_Hold( p_item );
        QMetaObject::invokeMethod(this, [this, p_item]() {
            QUrl itemUri = QUrl::fromEncoded(p_item->psz_uri);
            auto it = std::find_if( begin( m_items ), end( m_items ), [p_item, itemUri](const Item& i) {
                return QString::compare( qfu(p_item->psz_name), i.name, Qt::CaseInsensitive ) == 0 &&
                    itemUri.scheme() == i.mainMrl.scheme();
            });
            if ( it == end( m_items ) )
            {
                input_item_Release( p_item );
                return;
            }
            auto mrlIt = std::find_if( begin( (*it).mrls ), end( (*it).mrls),
                                       [itemUri]( const QUrl& mrl ) {
                return mrl == itemUri;
            });
            input_item_Release( p_item );
            if ( mrlIt == end( (*it).mrls ) )
                return;
            (*it).mrls.erase( mrlIt );
            if ( (*it).mrls.empty() == false )
                return;
            auto idx = std::distance( begin( m_items ), it );
            beginRemoveRows({}, idx, idx );
            m_items.erase( it );
            endRemoveRows();
        });
    }
}

void MLNetworkDeviceModel::refreshDeviceList( MediaSourcePtr mediaSource, input_item_node_t* const children[], size_t count, bool clear )
{
    if ( clear == true )
    {
        QMetaObject::invokeMethod(this, [this, mediaSource]() {
            beginResetModel();
            m_items.erase(std::remove_if(m_items.begin(), m_items.end(), [mediaSource](const Item& value) {
                return value.mediaSource == mediaSource;
            }), m_items.end());
            endResetModel();
        });
    }
    for ( auto i = 0u; i < count; ++i )
    {
        Item item;
        item.mainMrl = QUrl::fromEncoded( QByteArray{ children[i]->p_item->psz_uri }.append( '/' ) );
        item.name = qfu(children[i]->p_item->psz_name);
        item.mrls.push_back( item.mainMrl );
        item.type = static_cast<ItemType>( children[i]->p_item->i_type );
        item.protocol = item.mainMrl.scheme();
        item.mediaSource = mediaSource;
        item.inputItem = InputItemPtr(children[i]->p_item);

        QMetaObject::invokeMethod(this, [this, item]() mutable {
            auto it = std::upper_bound(begin( m_items ), end( m_items ), item, [](const Item& a, const Item& b) {
                int comp =  QString::compare(a.name , b.name, Qt::CaseInsensitive );
                if (comp == 0)
                    comp = QString::compare(a.mainMrl.scheme(), b.mainMrl.scheme());
                return comp <= 0;
            });

            if (it != end( m_items )
                && QString::compare(it->name , item.name, Qt::CaseInsensitive ) == 0
                && it->mainMrl.scheme() == item.mainMrl.scheme())
                return;

            int pos = std::distance(begin(m_items), it);
            beginInsertRows( {}, pos, pos );
            m_items.insert( it, std::move( item ) );
            endInsertRows();
        });
    }
}
