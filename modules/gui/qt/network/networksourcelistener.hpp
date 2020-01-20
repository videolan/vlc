/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef MLNETWORKSOURCELISTENER_HPP
#define MLNETWORKSOURCELISTENER_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_media_library.h>
#include <vlc_media_source.h>
#include <vlc_threads.h>
#include <vlc_cxx_helpers.hpp>

#include <util/qml_main_context.hpp>

#include <memory>
#include <functional>

class NetworkSourceListener
{
public:
    using MediaSourcePtr = vlc_shared_data_ptr_type(vlc_media_source_t,
                                    vlc_media_source_Hold, vlc_media_source_Release);

    using ListenerPtr = std::unique_ptr<vlc_media_tree_listener_id,
                                        std::function<void(vlc_media_tree_listener_id*)>>;

    class SourceListenerCb
    {
    public:
        virtual ~SourceListenerCb();

        virtual void onItemCleared( MediaSourcePtr mediaSource, input_item_node_t* node ) = 0;
        virtual void onItemAdded( MediaSourcePtr mediaSource, input_item_node_t* parent, input_item_node_t *const children[], size_t count ) = 0;
        virtual void onItemRemoved( MediaSourcePtr mediaSource, input_item_node_t* node, input_item_node_t *const children[], size_t count ) = 0;
        virtual void onItemPreparseEnded( MediaSourcePtr mediaSource, input_item_node_t* node, enum input_item_preparse_status status ) = 0;
    };

public:
    NetworkSourceListener( MediaSourcePtr s, SourceListenerCb* m );
    NetworkSourceListener();

    NetworkSourceListener( NetworkSourceListener&& ) = default;
    NetworkSourceListener& operator=( NetworkSourceListener&& ) = default;

    NetworkSourceListener( const NetworkSourceListener& ) = delete;
    NetworkSourceListener& operator=( const NetworkSourceListener& ) = delete;

    static void onItemCleared( vlc_media_tree_t* tree, input_item_node_t* node,
                               void* userdata );
    static void onItemAdded( vlc_media_tree_t *tree, input_item_node_t *node,
                             input_item_node_t *const children[], size_t count,
                             void *userdata );
    static void onItemRemoved( vlc_media_tree_t *tree, input_item_node_t *node,
                               input_item_node_t *const children[], size_t count,
                               void *userdata );

    static void onItemPreparseEnded( vlc_media_tree_t *tree, input_item_node_t *node,
                                     enum input_item_preparse_status status,
                                     void *userdata );

    MediaSourcePtr source;
    ListenerPtr listener = nullptr;
    SourceListenerCb *cb = nullptr;
};
#endif // MLNETWORKSOURCELISTENER_HPP
