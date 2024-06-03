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

#include "devicesourceprovider.hpp"
#include "networkmediamodel.hpp"


//handle discovery events from the media source provider
struct DeviceSourceProvider::ListenerCb : public MediaTreeListener::MediaTreeListenerCb {
    ListenerCb(DeviceSourceProvider* provider, NetworkDeviceModel::MediaSourcePtr mediaSource)
        : provider(provider)
        , mediaSource(std::move(mediaSource))
    {}

    inline void onItemPreparseEnded( MediaTreePtr, input_item_node_t *, enum input_item_preparse_status ) override final {}

    void onItemCleared( MediaTreePtr tree, input_item_node_t* node ) override
    {
        if (node != &tree->root)
            return;

        refresh( node->pp_children, node->i_children, true);
    }

    void onItemAdded( MediaTreePtr tree, input_item_node_t* parent, input_item_node_t *const children[], size_t count ) override
    {
        if (parent != &tree->root)
            return;

        refresh( children, count, false );
    }

    void onItemRemoved( MediaTreePtr tree, input_item_node_t * node, input_item_node_t *const children[], size_t count ) override
    {
        if (node != &tree->root)
            return;

        std::vector<SharedInputItem> itemList;

        itemList.reserve( count );
        for ( auto i = 0u; i < count; ++i )
            itemList.emplace_back( children[i]->p_item );

        QMetaObject::invokeMethod(provider, [provider = this->provider,
                                  itemList = std::move(itemList),
                                  mediaSource = this->mediaSource]()
        {
            provider->removeItems(itemList, mediaSource);
        });
    }

    void refresh(input_item_node_t * const children[], size_t count,
                 bool clear)
    {
        std::vector<SharedInputItem> itemList;

        itemList.reserve(count);
        for (size_t i = 0; i < count; i++)
            itemList.emplace_back(children[i]->p_item);

        QMetaObject::invokeMethod(provider, [provider = this->provider,
                                  itemList = std::move(itemList),
                                  mediaSource = this->mediaSource, clear]()
        {
            provider->addItems(itemList, mediaSource, clear);
        });
    }

    DeviceSourceProvider *provider;
    MediaSourcePtr mediaSource;
};


DeviceSourceProvider::DeviceSourceProvider(NetworkDeviceModel::SDCatType sdSource
                                           , const QString &sourceName, QObject *parent)
    : QObject(parent)
    , m_sdSource {sdSource}
    , m_sourceName {sourceName}
{
}

void DeviceSourceProvider::init(qt_intf_t *intf)
{
    using SourceMetaPtr = std::unique_ptr<vlc_media_source_meta_list_t,
                                          decltype( &vlc_media_source_meta_list_Delete )>;

    auto libvlc = vlc_object_instance(intf);

    auto provider = vlc_media_source_provider_Get( libvlc );
    SourceMetaPtr providerList( vlc_media_source_provider_List(
                                    provider,
                                    static_cast<services_discovery_category_e>(m_sdSource) ),
                               &vlc_media_source_meta_list_Delete );

    if (!providerList)
    {
        emit failed();
        return;
    }

    size_t nbProviders = vlc_media_source_meta_list_Count( providerList.get() );
    for ( auto i = 0u; i < nbProviders; ++i )
    {
        auto meta = vlc_media_source_meta_list_Get( providerList.get(), i );
        const QString sourceName = qfu( meta->name );
        if ( m_sourceName != '*' && m_sourceName != sourceName )
            continue;

        m_name += m_name.isEmpty() ? qfu( meta->longname ) : ", " + qfu( meta->longname );

        MediaSourcePtr mediaSource(
                    vlc_media_source_provider_GetMediaSource(provider, meta->name)
                    , false );

        if ( mediaSource == nullptr )
            continue;

        std::unique_ptr<MediaTreeListener> l{ new MediaTreeListener(
            MediaTreePtr{ mediaSource->tree },
            std::make_unique<DeviceSourceProvider::ListenerCb>(this, mediaSource) ) };
        if ( l->listener == nullptr )
            break;

        m_mediaSources.push_back( std::move( mediaSource ) );
        m_listeners.push_back( std::move( l ) );
    }

    if ( !m_name.isEmpty() )
        emit nameUpdated( m_name );

    if ( !m_listeners.empty() )
        emit itemsUpdated( m_items );
    else
        emit failed();
}

void DeviceSourceProvider::addItems(const std::vector<SharedInputItem> &inputList,
                                    const MediaSourcePtr &mediaSource, const bool clear)
{
    bool dataChanged = false;

    if (clear)
    {
        const qsizetype removed = m_items.removeIf([&mediaSource](const NetworkDeviceItemPtr &item)
        {
            return item->mediaSource == mediaSource;
        });

        if (removed > 0)
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
            m_items.insert(std::move(newItem));
            dataChanged = true;
        }
    }

    if (dataChanged)
    {
        emit itemsUpdated(m_items);
    }
}

void DeviceSourceProvider::removeItems(const std::vector<SharedInputItem> &inputList,
                                       const MediaSourcePtr &mediaSource)
{
    bool dataChanged = false;
    for (const SharedInputItem& p_item : inputList)
    {
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
                m_items.erase(it);
                dataChanged = true;
            }
        }
    }

    if (dataChanged)
        emit itemsUpdated(m_items);
}
