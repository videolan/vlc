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

#include "mlnetworkmodel.hpp"

#include "mlhelper.hpp"

namespace {

enum Role {
    NETWORK_NAME = Qt::UserRole + 1,
    NETWORK_MRL,
    NETWORK_INDEXED,
    NETWORK_CANINDEX,
    NETWORK_TYPE,
    NETWORK_PROTOCOL,
    NETWORK_TREE,
    NETWORK_SOURCE,
};

}

MLNetworkModel::MLNetworkModel( QObject* parent )
    : QAbstractListModel( parent )
    , m_ml( nullptr )
{
}

QVariant MLNetworkModel::data( const QModelIndex& index, int role ) const
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
        case NETWORK_INDEXED:
            return item.indexed;
        case NETWORK_CANINDEX:
            return item.canBeIndexed;
        case NETWORK_TYPE:
            return item.type;
        case NETWORK_PROTOCOL:
            return item.protocol;
        case NETWORK_TREE:
            return QVariant::fromValue( item.tree );
        case NETWORK_SOURCE:
            return item.mediaSource->description;
        default:
            return {};
    }
}

QHash<int, QByteArray> MLNetworkModel::roleNames() const
{
    return {
        { NETWORK_NAME, "name" },
        { NETWORK_MRL, "mrl" },
        { NETWORK_INDEXED, "indexed" },
        { NETWORK_CANINDEX, "can_index" },
        { NETWORK_TYPE, "type" },
        { NETWORK_PROTOCOL, "protocol" },
        { NETWORK_TREE, "tree" },
        { NETWORK_SOURCE, "source" },
    };
}

int MLNetworkModel::rowCount(const QModelIndex& parent) const
{
    if ( parent.isValid() )
        return 0;
    assert( m_items.size() < INT32_MAX );
    return static_cast<int>( m_items.size() );
}

Qt::ItemFlags MLNetworkModel::flags( const QModelIndex& idx ) const
{
    return QAbstractListModel::flags( idx ) | Qt::ItemIsEditable;
}

bool MLNetworkModel::setData( const QModelIndex& idx, const QVariant& value, int role )
{
    if (!m_ml)
        return false;

    if ( role != NETWORK_INDEXED )
        return false;
    auto enabled = value.toBool();
    assert( m_items[idx.row()].indexed != enabled );
    int res;
    if ( enabled )
        res = vlc_ml_add_folder( m_ml, qtu( m_items[idx.row()].mainMrl.toString( QUrl::None ) ) );
    else
        res = vlc_ml_remove_folder( m_ml, qtu( m_items[idx.row()].mainMrl.toString( QUrl::None ) ) );
    m_items[idx.row()].indexed = enabled;
    emit dataChanged(idx, idx, { NETWORK_INDEXED });
    return res == VLC_SUCCESS;
}

void MLNetworkModel::setContext(QmlMainContext* ctx, NetworkTreeItem parentTree )
{
    assert(!m_ctx);
    if (ctx) {
        m_ctx = ctx;
        m_ml = vlc_ml_instance_get( m_ctx->getIntf() );
        m_treeItem = parentTree;
        initializeMediaSources();
    }
}

bool MLNetworkModel::initializeMediaSources()
{
    auto libvlc = vlc_object_instance(m_ctx->getIntf());

    // When listing all found devices, we have no specified media and no parent,
    // but we can't go up a level in this case.
    // Otherwise, we can have a parent (when listing a subdirectory of a device)
    // or simply a media (that represents the device root folder)
    if ( m_treeItem.media != nullptr )
    {
        Item item;

        item.name = QString::fromUtf8(u8"⮤"); //arrow up ^_
        if ( m_treeItem.parent != nullptr )
        {
            QUrl parentMrl = QUrl{ m_treeItem.parent->psz_uri };
            item.mainMrl = parentMrl;
            item.mrls = {parentMrl};
            item.protocol = parentMrl.scheme();
        }
        item.indexed = false;
        item.type = TYPE_DIR;
        item.canBeIndexed = false;
        item.tree.source = m_treeItem.source;
        item.tree.media = m_treeItem.parent;

        beginInsertRows( {}, 0, 0 );
        m_items.push_back(item);
        endInsertRows();
    }

    if ( m_treeItem.media == nullptr )
    {
        // If there's no target media, we're handling device discovery
        auto provider = vlc_media_source_provider_Get( libvlc );

        using SourceMetaPtr = std::unique_ptr<vlc_media_source_meta_list_t,
                                              decltype( &vlc_media_source_meta_list_Delete )>;
        SourceMetaPtr providerList( vlc_media_source_provider_List( provider, SD_CAT_LAN ),
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
            std::unique_ptr<SourceListener> l{ new SourceListener(
                            MediaSourcePtr{ mediaSource, false }, this ) };
            if ( l->listener == nullptr )
                return false;
            m_listeners.push_back( std::move( l ) );
        }
        return m_listeners.empty() == false;
    }
    // Otherwise, we're listing content from a device or folder
    auto tree = m_treeItem.source->tree;
    std::unique_ptr<SourceListener> l{ new SourceListener( m_treeItem.source, this ) };
    if ( l->listener == nullptr )
        return false;
    vlc_media_tree_Preparse( tree, libvlc, m_treeItem.media );
    m_listeners.push_back( std::move( l ) );

    return true;
}

void MLNetworkModel::onItemCleared( MediaSourcePtr mediaSource, input_item_node_t* node )
{
    if ( m_treeItem.media != nullptr )
    {
        input_item_node_t *res;
        input_item_node_t *parent;
        if ( vlc_media_tree_Find( m_treeItem.source->tree, m_treeItem.media,
                                          &res, &parent ) == false )
            return;
        refreshMediaList( std::move( mediaSource ), res->pp_children, res->i_children, true );
    }
    else
        refreshDeviceList( std::move( mediaSource), node->pp_children, node->i_children, true );
}

void MLNetworkModel::onItemAdded( MediaSourcePtr mediaSource, input_item_node_t* parent,
                                  input_item_node_t *const children[],
                                  size_t count )
{
    if ( m_treeItem.media == nullptr )
        refreshDeviceList( std::move( mediaSource ), children, count, false );
    else if ( parent->p_item == m_treeItem.media )
        refreshMediaList( std::move( mediaSource ), children, count, false );
}

void MLNetworkModel::onItemRemoved( MediaSourcePtr,
                                    input_item_node_t *const children[],
                                    size_t count )
{
    for ( auto i = 0u; i < count; ++i )
    {
        input_item_t* p_item = children[i]->p_item;
        input_item_Hold( p_item );
        callAsync([this, p_item]() {
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

void MLNetworkModel::refreshMediaList( MediaSourcePtr mediaSource,
                                       input_item_node_t* const children[], size_t count,
                                       bool clear )
{
    auto items = new std::vector<Item>;
    for ( auto i = 0u; i < count; ++i )
    {
        auto it = children[i]->p_item;
        Item item;
        item.name = it->psz_name;
        item.protocol = "";
        item.indexed = false;
        item.type = (it->i_type == ITEM_TYPE_DIRECTORY || it->i_type == ITEM_TYPE_NODE) ?
                TYPE_DIR : TYPE_FILE;
        item.mainMrl = item.type == TYPE_DIR ?
                    QUrl::fromEncoded(QByteArray(it->psz_uri).append('/')) :
                    QUrl::fromEncoded(it->psz_uri);

        item.canBeIndexed = canBeIndexed( item.mainMrl );
        item.mediaSource = mediaSource;

        if ( item.canBeIndexed == true )
        {
            if ( vlc_ml_is_indexed( m_ml, qtu( item.mainMrl.toString( QUrl::None ) ),
                                    &item.indexed ) != VLC_SUCCESS )
                item.indexed = false;
        }
        item.tree = NetworkTreeItem{ mediaSource, it, m_treeItem.media };
        items->push_back( std::move( item ) );
    }
    callAsync([this, clear, items]() {
        std::unique_ptr<std::vector<Item>> itemsPtr{ items };
        if ( clear == true )
        {
            beginResetModel();
            // Keep the 'go to parent' item
            m_items.erase( begin( m_items ) + 1, end( m_items ) );
        }
        else
            beginInsertRows( {}, m_items.size(), m_items.size() + items->size() - 1 );
        std::move( begin( *items ), end( *items ), std::back_inserter( m_items ) );
        if ( clear == true )
            endResetModel();
        else
            endInsertRows();
    });
}

void MLNetworkModel::refreshDeviceList( MediaSourcePtr mediaSource, input_item_node_t* const children[], size_t count, bool clear )
{
    if ( clear == true )
    {
        callAsync([this, mediaSource]() {
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
        item.indexed = false;
        item.canBeIndexed = canBeIndexed( item.mainMrl );
        item.type = TYPE_SHARE;
        item.protocol = item.mainMrl.scheme();
        item.tree = NetworkTreeItem{ mediaSource,
                                     children[i]->p_item,
                                     nullptr };
        item.mediaSource = mediaSource;

        callAsync([this, item]() mutable {
            auto it = std::find_if( begin( m_items ), end( m_items ), [&item](const Item& i) {
                return QString::compare(item.name , i.name, Qt::CaseInsensitive ) == 0 &&
                        item.mainMrl.scheme() == i.mainMrl.scheme();
            });
            if ( it != end( m_items ) )
            {
                (*it).mrls.push_back( item.mainMrl );
                filterMainMrl( ( *it ), std::distance( begin( m_items ), it ) );
                return;
            }
            if ( item.canBeIndexed == true )
            {
                if ( vlc_ml_is_indexed( m_ml, qtu( item.mainMrl.toString( QUrl::None ) ),
                                        &item.indexed ) != VLC_SUCCESS )
                    item.indexed = false;
            }
            beginInsertRows( {}, m_items.size(), m_items.size() );
            m_items.push_back( std::move( item ) );
            endInsertRows();
        });
    }
}

MLNetworkModel::SourceListener::SourceListener(MediaSourcePtr s, MLNetworkModel* m)
    : source( s )
    , listener( nullptr, [s]( vlc_media_tree_listener_id* l ) {
            vlc_media_tree_RemoveListener( s->tree, l );
        } )
    , model( m )
{
    static const vlc_media_tree_callbacks cbs {
        &SourceListener::onItemCleared,
        &SourceListener::onItemAdded,
        &SourceListener::onItemRemoved
    };
    auto l = vlc_media_tree_AddListener( s->tree, &cbs, this, true );
    if ( l == nullptr )
        return;
    listener.reset( l );
}

void MLNetworkModel::SourceListener::onItemCleared( vlc_media_tree_t*, input_item_node_t* node,
                                                    void* userdata)
{
    auto* self = static_cast<SourceListener*>( userdata );
    self->model->onItemCleared( self->source, node );
}

void MLNetworkModel::SourceListener::onItemAdded( vlc_media_tree_t *, input_item_node_t * parent,
                                  input_item_node_t *const children[], size_t count,
                                  void *userdata )
{
    auto* self = static_cast<SourceListener*>( userdata );
    auto source = self->source;
    auto model = self->model;
    model->onItemAdded( source, parent, children, count );
}

void MLNetworkModel::SourceListener::onItemRemoved( vlc_media_tree_t *, input_item_node_t *,
                                    input_item_node_t *const children[], size_t count,
                                    void *userdata )
{
    auto* self = static_cast<SourceListener*>( userdata );
    self->model->onItemRemoved( self->source, children, count );
}

bool MLNetworkModel::canBeIndexed(const QUrl& url)
{
    return url.scheme() == "smb" || url.scheme() == "ftp";
}

void MLNetworkModel::filterMainMrl( MLNetworkModel::Item& item , size_t itemIndex )
{
    assert( item.mrls.empty() == false );
    if ( item.mrls.size() == 1 )
        return;

    //maybe we should rather use QHostAddress, but this adds a dependency uppon QNetwork that we don't require at the moment
    //https://stackoverflow.com/a/17871737/148173
    QRegExp ipv4("((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])");
    QRegExp ipv6("(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9]))");

    // We're looking for the mrl which is a (netbios) name, not an IP
    for ( const auto& mrl : item.mrls )
    {
        if (mrl.isEmpty() == true || mrl.scheme() == "")
            continue;

        QString host = mrl.host();
        if (ipv4.exactMatch(host) || ipv6.exactMatch(host))
            continue;

        item.mainMrl = mrl;
        item.canBeIndexed = canBeIndexed( mrl  );
        auto idx = index( static_cast<int>( itemIndex ), 0 );
        emit dataChanged( idx, idx, { NETWORK_MRL, NETWORK_CANINDEX } );
        return;
    }
    // If we can't get a cannonical name, don't attempt to index this as we
    // would fail to get a unique associated device in the medialibrary
    item.canBeIndexed = false;
    auto idx = index( static_cast<int>( itemIndex ), 0 );
    emit dataChanged( idx, idx, { NETWORK_CANINDEX } );
}

