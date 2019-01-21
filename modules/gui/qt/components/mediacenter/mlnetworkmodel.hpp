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

#ifndef MLNETWORKMODEL_HPP
#define MLNETWORKMODEL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QAbstractListModel>

#include <vlc_media_library.h>
#include <vlc_media_source.h>
#include <vlc_threads.h>
#include <vlc_cxx_helpers.hpp>

#include <components/qml_main_context.hpp>

#include <memory>

using MediaSourcePtr = vlc_shared_data_ptr_type(vlc_media_source_t,
                                vlc_media_source_Hold, vlc_media_source_Release);

class NetworkTreeItem
{
    Q_GADGET
public:
    NetworkTreeItem() : source(nullptr), media(nullptr), parent(nullptr) {}
    NetworkTreeItem( MediaSourcePtr s, input_item_t* m, input_item_t* p )
        : source( std::move( s ) )
        , media( m )
        , parent( p )
    {
    }
    NetworkTreeItem( const NetworkTreeItem& ) = default;
    NetworkTreeItem& operator=( const NetworkTreeItem& ) = default;
    NetworkTreeItem( NetworkTreeItem&& ) = default;
    NetworkTreeItem& operator=( NetworkTreeItem&& ) = default;

    MediaSourcePtr source;
    input_item_t *media;
    input_item_t *parent;
};

class MLNetworkModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemType
    {
        TYPE_SHARE,
        TYPE_DIR,
        TYPE_FILE,
    };
    Q_ENUM( ItemType );

    explicit MLNetworkModel(QObject* parent = nullptr);
    MLNetworkModel( QmlMainContext* ctx, QString parentMrl, QObject* parent = nullptr );

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex& parent) const override;

    Qt::ItemFlags flags( const QModelIndex& idx ) const override;
    bool setData( const QModelIndex& idx,const QVariant& value, int role ) override;

    Q_INVOKABLE void setContext(QmlMainContext* ctx, NetworkTreeItem parentMrl);

private:
    struct Item
    {
        QString name;
        QUrl mainMrl;
        std::vector<QUrl> mrls;
        QString protocol;
        bool indexed;
        ItemType type;
        bool canBeIndexed;
        NetworkTreeItem tree;
        MediaSourcePtr mediaSource;
    };

    ///call function @a fun on object thread
    template <typename Fun>
    void callAsync(Fun&& fun)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        QMetaObject::invokeMethod(this, std::forward<Fun>(fun), Qt::QueuedConnection, nullptr);
#else
        QObject src;
        QObject::connect(&src, &QObject::destroyed, q, std::forward<Fun>(fun), Qt::QueuedConnection);
#endif
    }

    bool initializeMediaSources();
    void onItemCleared( MediaSourcePtr mediaSource, input_item_node_t* node );
    void onItemAdded( MediaSourcePtr mediaSource, input_item_node_t* parent, input_item_node_t *const children[], size_t count );
    void onItemRemoved( MediaSourcePtr mediaSource, input_item_node_t *const children[], size_t count );

    void refreshMediaList(MediaSourcePtr s, input_item_node_t* const children[], size_t count , bool clear);
    void refreshDeviceList(MediaSourcePtr mediaSource, input_item_node_t* const children[], size_t count , bool clear);

    static bool canBeIndexed(const QUrl& url );
    void filterMainMrl( Item& item, size_t itemIndex );

private:
    struct SourceListener
    {
        using ListenerPtr = std::unique_ptr<vlc_media_tree_listener_id,
                                            std::function<void(vlc_media_tree_listener_id*)>>;

        SourceListener( MediaSourcePtr s, MLNetworkModel* m );
        SourceListener() : source( nullptr ), listener( nullptr ), model( nullptr ){}

        SourceListener( SourceListener&& ) = default;
        SourceListener& operator=( SourceListener&& ) = default;

        SourceListener( const SourceListener& ) = delete;
        SourceListener& operator=( const SourceListener& ) = delete;

        static void onItemCleared( vlc_media_tree_t* tree, input_item_node_t* node,
                                   void* userdata );
        static void onItemAdded( vlc_media_tree_t *tree, input_item_node_t *node,
                                 input_item_node_t *const children[], size_t count,
                                 void *userdata );
        static void onItemRemoved( vlc_media_tree_t *tree, input_item_node_t *node,
                                   input_item_node_t *const children[], size_t count,
                                   void *userdata );

        MediaSourcePtr source;
        ListenerPtr listener;
        MLNetworkModel *model;
    };

    std::vector<Item> m_items;
    QmlMainContext* m_ctx = nullptr;
    vlc_medialibrary_t* m_ml;
    NetworkTreeItem m_treeItem;
    std::vector<std::unique_ptr<SourceListener>> m_listeners;
};

#endif // MLNETWORKMODEL_HPP
