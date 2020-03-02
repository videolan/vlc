/*****************************************************************************
 * devicelister.h: Media library network file system
 *****************************************************************************
 * Copyright (C) 2018 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "devicelister.h"

#include <vlc_services_discovery.h>

namespace vlc
{
namespace medialibrary
{

DeviceLister::DeviceLister( vlc_object_t* parent )
    : m_parent( parent )
{
}

void DeviceLister::refresh()
{
    /*
     * This can be left empty since we always propagate devices changes through
     * the media source/media tree callbacks
     */
}

bool DeviceLister::start( ml::IDeviceListerCb* cb )
{
    vlc::threads::mutex_locker lock( m_mutex );

    auto provider = vlc_media_source_provider_Get( vlc_object_instance( m_parent ) );

    using SourceMetaPtr = std::unique_ptr<vlc_media_source_meta_list_t,
                                          decltype( &vlc_media_source_meta_list_Delete )>;

    SourceMetaPtr providerList{
        vlc_media_source_provider_List( provider, SD_CAT_LAN ),
        &vlc_media_source_meta_list_Delete
    };

    m_cb = cb;

    auto nbProviders = vlc_media_source_meta_list_Count( providerList.get() );

    for ( auto i = 0u; i < nbProviders; ++i )
    {
        auto meta = vlc_media_source_meta_list_Get( providerList.get(), i );
        auto mediaSource = vlc_media_source_provider_GetMediaSource( provider,
                                                                     meta->name );
        if ( mediaSource == nullptr )
            continue;
        MediaSource s{ mediaSource };

        static const vlc_media_tree_callbacks cbs {
            &onChildrenReset, &onChildrenAdded, &onChildrenRemoved, nullptr
        };

        s.l = vlc_media_tree_AddListener( s.s->tree, &cbs, this, true );
        m_mediaSources.push_back( std::move( s ) );
    }
    return m_mediaSources.empty() == false;
}

void DeviceLister::stop()
{
    vlc::threads::mutex_locker lock( m_mutex );

    m_mediaSources.clear();
    m_cb = nullptr;
}

void DeviceLister::onChildrenReset( vlc_media_tree_t* tree, input_item_node_t* node,
                                    void* data )
{
    auto self = static_cast<DeviceLister*>( data );
    self->onChildrenReset( tree, node );
}

void DeviceLister::onChildrenAdded( vlc_media_tree_t* tree, input_item_node_t* node,
                                    input_item_node_t* const children[], size_t count,
                                    void* data )
{
    auto self = static_cast<DeviceLister*>( data );
    self->onChildrenAdded( tree, node, children, count );
}

void DeviceLister::onChildrenRemoved( vlc_media_tree_t* tree, input_item_node_t* node,
                                      input_item_node_t* const children[], size_t count,
                                      void* data )
{
    auto self = static_cast<DeviceLister*>( data );
    self->onChildrenRemoved( tree, node, children, count );
}

void DeviceLister::onChildrenReset( vlc_media_tree_t* tree, input_item_node_t* )
{
    for ( auto i = 0; i < tree->root.i_children; ++i )
    {
        auto c = tree->root.pp_children[i];
        m_cb->onDeviceMounted( c->p_item->psz_name, c->p_item->psz_uri, true );
    }
}

void DeviceLister::onChildrenAdded( vlc_media_tree_t*, input_item_node_t*,
                                    input_item_node_t* const children[], size_t count )
{
    for ( auto i = 0u; i < count; ++i )
    {
        auto c = children[i];
        m_cb->onDeviceMounted( c->p_item->psz_name, c->p_item->psz_uri, true );
    }
}

void DeviceLister::onChildrenRemoved(vlc_media_tree_t*, input_item_node_t*,
                                     input_item_node_t* const children[], size_t count )
{
    for ( auto i = 0u; i < count; ++i )
    {
        auto c = children[i];
        m_cb->onDeviceUnmounted( c->p_item->psz_name, c->p_item->psz_uri );
    }
}

DeviceLister::MediaSource::MediaSource( vlc_media_source_t *ms )
    : s( ms )
    , l( nullptr )
{
}

DeviceLister::MediaSource::~MediaSource()
{
    if ( l != nullptr )
        vlc_media_tree_RemoveListener( s->tree, l );
    if ( s != nullptr )
        vlc_media_source_Release( s );
}

DeviceLister::MediaSource::MediaSource( DeviceLister::MediaSource&& rhs ) noexcept
    : s( rhs.s )
    , l( rhs.l )
{
    rhs.s = nullptr;
    rhs.l = nullptr;
}

DeviceLister::MediaSource&
DeviceLister::MediaSource::operator=( DeviceLister::MediaSource&& rhs ) noexcept
{
    s = rhs.s;
    l = rhs.l;
    rhs.s = nullptr;
    rhs.l = nullptr;
    return *this;
}

}
}
