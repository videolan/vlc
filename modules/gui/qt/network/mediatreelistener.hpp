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

#include <memory>
#include <functional>

#include "vlcmediasourcewrapper.hpp"

class MediaTreeListener
{
public:
    using ListenerPtr = std::unique_ptr<vlc_media_tree_listener_id,
                                        std::function<void(vlc_media_tree_listener_id*)>>;

    class MediaTreeListenerCb
    {
    public:
        virtual ~MediaTreeListenerCb() = default;
        virtual void onItemCleared( MediaTreePtr tree, input_item_node_t* node ) = 0;
        virtual void onItemAdded( MediaTreePtr tree, input_item_node_t* parent, input_item_node_t *const children[], size_t count ) = 0;
        virtual void onItemRemoved( MediaTreePtr tree, input_item_node_t* node, input_item_node_t *const children[], size_t count ) = 0;
        virtual void onItemPreparseEnded( MediaTreePtr tree, input_item_node_t* node, int status ) = 0;
    };

public:
    MediaTreeListener( MediaTreePtr tree, std::unique_ptr<MediaTreeListenerCb> &&cb );

    MediaTreeListener( MediaTreeListener&& ) = default;
    MediaTreeListener& operator=( MediaTreeListener&& ) = default;

    MediaTreeListener( const MediaTreeListener& ) = delete;
    MediaTreeListener& operator=( const MediaTreeListener& ) = delete;

    MediaTreePtr tree;
    ListenerPtr listener = nullptr;
    std::unique_ptr<MediaTreeListenerCb> cb;
};
#endif // MLNETWORKSOURCELISTENER_HPP
